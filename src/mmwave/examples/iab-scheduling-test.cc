/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 University of Padova, Dep. of Information Engineering, SIGNET lab.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *   Authors: Matteo Pagin <paginmatte@dei.unipd.it>
 *            Alessandro Traspadini <traspadini@dei.unipd.it>
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
double simTimeSeconds = 1.0;
bool useIdealRrc = false;
bool enableIabNodeB = true;
uint8_t numSymResvForOddIabs = 6;

NS_LOG_COMPONENT_DEFINE ("IabSchedulingTest");

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
CheckSchedulingCallback (MmWaveEnbMac::MmWaveSchedTraceInfo schedParams)
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

      if (oddLayerNode)
        {
          NS_ABORT_IF ((
              iTti.m_dci.m_symStart <
              numSymResvForOddIabs)); // can only allocate at symbols indexed numSymResvForOddIabs + 1-12
          NS_ABORT_IF ((iTti.m_dci.m_symStart + iTti.m_dci.m_numSym - 1 > 12));
        }
      else
        {
          NS_ABORT_IF ((
              iTti.m_dci.m_symStart >
              numSymResvForOddIabs)); // can only allocate at symbols indexed 1-numSymResvForOddIabs
          NS_ABORT_IF ((iTti.m_dci.m_symStart + iTti.m_dci.m_numSym - 1 > numSymResvForOddIabs));
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
  NS_ABORT_MSG_IF (!m_isConnectionEstablished, "The UE could not connect to the DU");
}

double
PacketByteSizeFromBitrateAndIpi (DataRate rate, Time IPI)
{
  double size = rate.GetBitRate () * IPI.GetSeconds ();
  return size / 8;
}

void
RxSinkHeader (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &from,
              const Address &to, const SeqTsSizeHeader &seqTs)
{
  *stream->GetStream () << "Rx\t" << Simulator::Now ().GetNanoSeconds () << "\t" << seqTs.GetSize ()
                        << "\t" << (Simulator::Now () - seqTs.GetTs ()).GetNanoSeconds () << "\t"
                        << InetSocketAddress::ConvertFrom (to).GetPort () << std::endl;
}

void
TxSinkHeader (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &from,
              const Address &to, const SeqTsSizeHeader &seqTs)
{
  *stream->GetStream () << "Tx\t" << Simulator::Now ().GetNanoSeconds () << "\t" << seqTs.GetSize ()
                        << "\t" << (Simulator::Now () - seqTs.GetTs ()).GetNanoSeconds () << "\t"
                        << InetSocketAddress::ConvertFrom (to).GetPort () << std::endl;
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
  // Setup the traces
  app.Get (0)->TraceConnectWithoutContext ("TxWithSeqTsSize",
                                           MakeBoundCallback (&TxSinkHeader, traceStream));
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
  cmd.AddValue ("enableIabNodeB", "Whether to deploy a second IAB node", enableIabNodeB);
  cmd.Parse (argc, argv);
  // double interPacketInterval = 100;

  LogComponentEnable ("MmWaveFlexTtiMacScheduler", LOG_LEVEL_DEBUG);
  LogComponentEnableAll (LOG_PREFIX_ALL);

  Config::SetDefault ("ns3::MmWaveHelper::UseIdealRrc", BooleanValue (useIdealRrc));
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::NumSymResvForChildrenDu",
                      UintegerValue (numSymResvForOddIabs));

  Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper> ();
  Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
  mmwaveHelper->SetEpcHelper (epcHelper);
  mmwaveHelper->SetHarqEnabled (true);
  std::string dataSensorRate = "25Mbps";
  uint8_t numUe = 2;

  NodeContainer iabNode;
  NodeContainer ueNodes;
  NodeContainer enbNode;

  iabNode.Create (1);
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
  Ptr<ListPositionAllocator> iabPositionAlloc = CreateObject<ListPositionAllocator> ();
  iabPositionAlloc->Add (Vector (0.0, 0.0, 0.0));
  MobilityHelper iabmobility;
  iabmobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  iabmobility.SetPositionAllocator (iabPositionAlloc);
  iabmobility.Install (iabNode);

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
  NetDeviceContainer iabMmWaveDev = mmwaveHelper->InstallIabDevice (iabNode);

  auto iabDevA = DynamicCast<MmWaveIabNetDevice> (iabMmWaveDev.Get (0));

  NetDeviceContainer ueMmWaveDevA = mmwaveHelper->InstallUeDevice (ueNodes.Get (0));
  NetDeviceContainer ueMmWaveDevB = mmwaveHelper->InstallUeDevice (ueNodes.Get (1));
  NetDeviceContainer enbMmWaveDev = mmwaveHelper->InstallEnbDevice (enbNode);

  auto enbDev = DynamicCast<MmWaveEnbNetDevice> (enbMmWaveDev.Get (0));

  // Install the IP stack on the UEs
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIfaceA;
  ueIpIfaceA = epcHelper->AssignUeIpv4Address (ueMmWaveDevA);
  Ipv4InterfaceContainer ueIpIfaceB;
  ueIpIfaceB = epcHelper->AssignUeIpv4Address (ueMmWaveDevB);
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting =
          ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  mmwaveHelper->AttachIabTotDonorWithIndex (iabMmWaveDev, enbMmWaveDev, 0);
  mmwaveHelper->AttachToClosestEnb (ueMmWaveDevA, enbMmWaveDev);

  // Attach UE with IAB node
  // Pass all IAB nodes as arguments, as they need to be "registered" as other BSs in the simulation
  // and to be configured beamforming-wise
  mmwaveHelper->AttachToClosestIab (ueMmWaveDevB, iabMmWaveDev);

  AsciiTraceHelper asciiTraceHelper;

  // Base ports
  uint16_t ulDataSensorPort = 1235; // base port for the UL data stream from the sensors
  // uint16_t ulVideoAlarmPort = 1335;      // base port for the UL video stream from the sensors
  // uint16_t ulMRPort = 1435;              // base port for the UL video stream from the MRs
  // uint16_t dlUrllcPort = 1635;       // base port for the eMBB applications
  // uint16_t dlEmbbPort = 1735;        // base port for the URLLC applications

  // Install UL data reportings for the NR-Light data sensors
  Ptr<OutputStreamWrapper> dataSensorStream =
      asciiTraceHelper.CreateFileStream ("data-sensor-app-trace.txt");
  AppSetupParams dataSensorParams;
  dataSensorParams.basePort = ulDataSensorPort;
  dataSensorParams.startTime = MilliSeconds (100);
  dataSensorParams.endTime = Seconds (1); // Some cooldown time
  dataSensorParams.IPI = MilliSeconds (20); // On average, a packet every 200 ms
  dataSensorParams.rate = DataRate (dataSensorRate);
  dataSensorParams.simTime = Seconds (simTimeSeconds);
  dataSensorParams.sendSize = 512;
  for (unsigned int i = 0; i < ueNodes.GetN (); i++)
    {
      InstallUdpApplication (dataSensorStream, ueNodes.Get (i), internetIpIfaces.GetAddress (1),
                             dataSensorParams);
      InstallUdpPacketSink (dataSensorStream, internetIpIfaces.GetAddress (1), remoteHost,
                            dataSensorParams);
      dataSensorParams.basePort++;
    }

  // Install DL remote operation commands for the NR-Light Mobile Robots
  Ptr<OutputStreamWrapper> remoteCommandsMrStream =
      asciiTraceHelper.CreateFileStream ("mr-commands-app-trace.txt");
  AppSetupParams mrRemoteCommandsParams;
  InstallUdpApplication (remoteCommandsMrStream, remoteHost, ueIpIfaceA.GetAddress (0),
                         dataSensorParams);
  InstallUdpPacketSink (remoteCommandsMrStream, ueIpIfaceA.GetAddress (0), ueNodes.Get (0),
                        dataSensorParams);

  // Schedule check of UE connection to the DU
  auto ueMmwA = DynamicCast<MmWaveUeNetDevice> (ueMmWaveDevA.Get (0));
  ueMmwA->GetRrc ()->TraceConnectWithoutContext (
      "ConnectionEstablished", MakeBoundCallback (&ConnectionEstablishedCallback));

  auto ueMmwB = DynamicCast<MmWaveUeNetDevice> (ueMmWaveDevB.Get (0));
  ueMmwB->GetRrc ()->TraceConnectWithoutContext (
      "ConnectionEstablished", MakeBoundCallback (&ConnectionEstablishedCallback));

  // Schedule check of MT connection to the donor
  auto iabMmw = DynamicCast<MmWaveIabNetDevice> (iabMmWaveDev.Get (0));
  iabMmw->GetMtRrc ()->TraceConnectWithoutContext (
      "ConnectionEstablished", MakeBoundCallback (&ConnectionEstablishedCallback));

  // Check scheduling decisions
  // eNB devices
  Config::ConnectWithoutContextFailSafe (
      "/NodeList/*/DeviceList/*/ComponentCarrierMap/*/MmWaveEnbMac/SchedulingTraceEnb",
      MakeBoundCallback (CheckSchedulingCallback));

  // IAB devices
  Config::ConnectWithoutContextFailSafe (
      "/NodeList/*/DeviceList/*/DuComponentCarrierMap/*/MmWaveEnbMac/SchedulingTraceEnb",
      MakeBoundCallback (CheckSchedulingCallback));

  mmwaveHelper->EnableTraces ();

  Simulator::Schedule (Seconds (simTimeSeconds - 0.01), &CheckConnectionEstablished);

  Simulator::Stop (Seconds (1.0));
  Simulator::Run ();

  Simulator::Destroy ();

  return 0;
}
