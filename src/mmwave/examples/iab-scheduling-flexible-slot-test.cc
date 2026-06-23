/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
*   Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
*   Copyright (c) 2015, NYU WIRELESS, Tandon School of Engineering, New York University
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License version 2 as
*   published by the Free Software Foundation;
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*   Author: Marco Miozzo <marco.miozzo@cttc.es>
*           Nicola Baldo  <nbaldo@cttc.es>
*
*   Modified by: Marco Mezzavilla < mezzavilla@nyu.edu>
*                         Sourjya Dutta <sdutta@nyu.edu>
*                         Russell Ford <russell.ford@nyu.edu>
*                         Menglei Zhang <menglei@nyu.edu>
*/

#include "ns3/epc-helper.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/command-line.h"
#include "ns3/mmwave-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/mmwave-iab-net-device.h"
#include <sstream>
#include <string>

using namespace ns3;
using namespace mmwave;

static bool m_isConnectionEstablished = false;
double simTimeSeconds = 3.0;
bool useIdealRrc = false;
bool enableIabNodeB = true;
uint8_t numSymResvForOddIabs = 0;
uint8_t numSymResvDl = 3;
uint8_t numSymResvSw = 1;

NS_LOG_COMPONENT_DEFINE ("IabSchedulingFlexibleSlotTest");

struct AppSetupParams
{
  uint16_t basePort;
  uint32_t sendSize;
  DataRate rate;
  Time IPI;
  Time startTime;
  Time endTime;
  Time simTime;
};

void
CheckSchedulingCallback(MmWaveEnbMac::MmWaveSchedTraceInfo schedParams)
{
  bool oddLayerNode = false;
  if (schedParams.m_cellId == 1) // IAB node in layer 1
  {
      oddLayerNode = true;
  }

  SlotAllocInfo allocInfo = schedParams.m_indParam.m_slotAllocInfo;

  for (auto iTti : allocInfo.m_ttiAllocInfo)
  {
      bool isCtrl = (iTti.m_ttiType == TtiAllocInfo::TddTtiType::CTRL);
      if (isCtrl)
      {
          continue;
      }
      bool isDl = false;
      if (iTti.m_tddMode == TtiAllocInfo::DL_slotAllocInfo)
      {
          isDl = true;
      }

      if (oddLayerNode)
      {
          if (isDl)
          {
              NS_ABORT_IF((iTti.m_dci.m_symStart <
                           numSymResvForOddIabs)); // can only allocate at symbols indexed
                                                   // [numSymResvForOddIabs + 1,
                                                   // numSymResvForOddIabs + numSymResvDl]
              NS_ABORT_IF((iTti.m_dci.m_symStart + iTti.m_dci.m_numSym - 1 >
                           numSymResvForOddIabs + numSymResvDl));
          }
          else
          {
              NS_ABORT_IF(
                  (iTti.m_dci.m_symStart <
                   numSymResvForOddIabs + numSymResvDl +
                       numSymResvSw)); // can only allocate at symbols indexed
                                       // [numSymResvForOddIabs + numSymResvDl + numSymResvSw, 12]
              NS_ABORT_IF((iTti.m_dci.m_symStart + iTti.m_dci.m_numSym - 1 > 12));
          }
      }
      else
      {
          if (isDl)
          {
              NS_ABORT_IF((iTti.m_dci.m_symStart > numSymResvDl)); // can only allocate at symbols
                                                                   //  [1, numSymResvDl]
              NS_ABORT_IF((iTti.m_dci.m_symStart + iTti.m_dci.m_numSym - 1 > numSymResvDl));
          }
          else
          {
              NS_ABORT_IF(
                  (iTti.m_dci.m_symStart <
                   numSymResvDl +
                       numSymResvSw)); // can only allocate at symbols
                                       // [numSymResvDl + numSymResvSw, numSymResvForOddIabs]
              NS_ABORT_IF((iTti.m_dci.m_symStart + iTti.m_dci.m_numSym - 1 > numSymResvForOddIabs));
          }
      }
  }
}

static void
ConnectionEstablishedCallback (uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  NS_LOG_FUNCTION (imsi << cellId);

  m_isConnectionEstablished = true;
}

static void
CheckConnectionEstablished ()
{
  NS_ABORT_MSG_IF (!m_isConnectionEstablished, "The UE could not connect to the Enb");
}

double
PacketByteSizeFromBitrateAndIpi (DataRate rate, Time IPI)
{
  double size = rate.GetBitRate () * IPI.GetSeconds ();
  return size / 8;
}

static void
RxSinkHeader (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &from,
              const Address &to, const SeqTsSizeHeader &header)
{
  *stream->GetStream () << "Rx " << Simulator::Now ().GetNanoSeconds () << " "
                        << packet->GetSize () << " "
                        << InetSocketAddress::ConvertFrom (to).GetPort () << " "
                        << InetSocketAddress::ConvertFrom (from).GetPort () << " "
                        << (Simulator::Now () - header.GetTs ()).GetNanoSeconds () << "\n";
}

static void
TxSinkHeader (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &from,
              const Address &to, const SeqTsSizeHeader &header)
{
  *stream->GetStream () << "Tx " << Simulator::Now ().GetNanoSeconds () << " "
                        << packet->GetSize () << " "
                        << InetSocketAddress::ConvertFrom (to).GetPort () << " "
                        << InetSocketAddress::ConvertFrom (from).GetPort () << " "
                        << (Simulator::Now () - header.GetTs ()).GetNanoSeconds () << "\n";
}

void
InstallUdpApplication (Ptr<OutputStreamWrapper> traceStream, Ptr<Node> srcNode,
                       Ipv4Address targetAddress, AppSetupParams params)
{
  ApplicationContainer app;
  UdpClientHelper client (targetAddress, params.basePort);
  client.SetAttribute ("Interval", TimeValue (params.IPI));
  client.SetAttribute ("PacketSize",
                       UintegerValue (PacketByteSizeFromBitrateAndIpi (params.rate, params.IPI)));
  client.SetAttribute ("MaxPackets", UintegerValue (std::numeric_limits<uint32_t>::max ()));
  app.Add (client.Install (srcNode));
  app.Get (0)->TraceConnectWithoutContext ("TxWithSeqTsSize",
                                           MakeBoundCallback (TxSinkHeader, traceStream));
  app.Start (params.startTime);
  app.Stop (params.endTime);
}

void
InstallUdpPacketSink (Ptr<OutputStreamWrapper> traceStream, Ipv4Address srcAddress,
                      Ptr<Node> targetNode, AppSetupParams params)
{
  ApplicationContainer app;
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory",
                                     InetSocketAddress (srcAddress, params.basePort));
  app.Add (packetSinkHelper.Install (targetNode));
  app.Get (0)->TraceConnectWithoutContext ("RxWithSeqTsSize",
                                           MakeBoundCallback (RxSinkHeader, traceStream));
  app.Start (Seconds (0));
  app.Stop (params.simTime);
}

int
main (int argc, char *argv[])
{
  // Command line arguments
  CommandLine cmd;
  cmd.AddValue ("useIdealRrc", "Whether to us ea simplified RRC model", useIdealRrc);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::MmWaveHelper::UseIdealRrc", BooleanValue (useIdealRrc));

  Config::SetDefault ("ns3::PacketSink::EnableSeqTsSizeHeader",BooleanValue (true));


  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::NumSymResvForDl",
                      UintegerValue (numSymResvDl));
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::NumSymReservedSw",
                      UintegerValue (numSymResvSw));

  Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper> ();
  Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
  mmwaveHelper->SetEpcHelper (epcHelper);
  mmwaveHelper->SetHarqEnabled (true);
  std::string dataSensorRate = "25Mbps";
  uint8_t numUe = 1;

  NodeContainer ueNodes;
  NodeContainer enbNode;

  ueNodes.Create (numUe);
  enbNode.Create (1);

  Ptr<Node> pgw = epcHelper->GetPgwNode ();

  // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (100.0)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
      ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  // Install Mobility Model
  MobilityHelper uemobility;
  Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator> ();
  Ptr<UniformRandomVariable> distRv = CreateObject<UniformRandomVariable> ();
  for (unsigned i = 0; i < numUe; i++)
    {
      double dist = distRv->GetValue (1, 15);
      uePositionAlloc->Add (Vector (dist, 0.0, 0.0));
    }
  uemobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  uemobility.SetPositionAllocator (uePositionAlloc);
  uemobility.Install (ueNodes);

  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (50.0, 0.0, 0.0));
  MobilityHelper embMobility;
  embMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  embMobility.SetPositionAllocator (enbPositionAlloc);
  embMobility.Install (enbNode);

  // Install Devices to the nodes
  NetDeviceContainer ueMmWaveDev = mmwaveHelper->InstallUeDevice (ueNodes.Get (0));
  NetDeviceContainer enbMmWaveDev = mmwaveHelper->InstallEnbDevice (enbNode);

  // Install the IP stack on the UEs
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (ueMmWaveDev);
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting =
          ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  mmwaveHelper->AttachToClosestEnb (ueMmWaveDev, enbMmWaveDev);

  AsciiTraceHelper asciiTraceHelper;

  // Base ports
  uint16_t dataSensorPort = 1235; // base port for the data stream

  // Install UL data
  Ptr<OutputStreamWrapper> dataSensorUl =
      asciiTraceHelper.CreateFileStream ("UL-app-trace.txt");
  *dataSensorUl->GetStream () << "rx_tx time packet_size port_to port_from delay\n";
  AppSetupParams dataSensorParams;
  dataSensorParams.basePort = dataSensorPort;
  dataSensorParams.startTime = MilliSeconds (100);
  dataSensorParams.endTime = Seconds (simTimeSeconds); // Some cooldown time
  dataSensorParams.IPI = MilliSeconds (20); // On average, a packet every 20 ms
  dataSensorParams.rate = DataRate (dataSensorRate);
  dataSensorParams.simTime = Seconds (simTimeSeconds);
  dataSensorParams.sendSize = 512;
  for (unsigned int i = 0; i < ueNodes.GetN (); i++)
    {
      InstallUdpApplication (dataSensorUl, ueNodes.Get (i), internetIpIfaces.GetAddress (1),
                             dataSensorParams);
      InstallUdpPacketSink (dataSensorUl, internetIpIfaces.GetAddress (1), remoteHost,
                            dataSensorParams);
      dataSensorParams.basePort++;
    }

  // Install DL data
  Ptr<OutputStreamWrapper> dataSensorDl =
      asciiTraceHelper.CreateFileStream ("DL-app-trace.txt");
  *dataSensorDl->GetStream () << "rx_tx time packet_size port_to port_from delay\n";
  AppSetupParams mrRemoteCommandsParams;
  InstallUdpApplication (dataSensorDl, remoteHost, ueIpIface.GetAddress (0),
                         dataSensorParams);
  InstallUdpPacketSink (dataSensorDl, ueIpIface.GetAddress (0), ueNodes.Get (0),
                        dataSensorParams);

  // Schedule check of UE connection to Enb
  auto ueMmwA = DynamicCast<MmWaveUeNetDevice> (ueMmWaveDev.Get (0));
  ueMmwA->GetRrc ()->TraceConnectWithoutContext (
      "ConnectionEstablished", MakeBoundCallback (&ConnectionEstablishedCallback));

  // Check scheduling decisions
  // eNB devices
  Config::ConnectWithoutContextFailSafe (
      "/NodeList/*/DeviceList/*/ComponentCarrierMap/*/MmWaveEnbMac/SchedulingTraceEnb",
      MakeBoundCallback (CheckSchedulingCallback));

  mmwaveHelper->EnableTraces ();

  Simulator::Schedule (Seconds (simTimeSeconds - 0.01), &CheckConnectionEstablished);

  Simulator::Stop (Seconds (simTimeSeconds));
  Simulator::Run ();

  Simulator::Destroy ();

  return 0;
}
