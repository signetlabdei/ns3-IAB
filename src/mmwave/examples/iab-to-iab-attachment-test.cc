/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
*   Copyright (c) 2022 University of Padova, Dep. of Information Engineering, SIGNET lab.
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
*   Authors: Matteo Pagin <paginmatte@dei.unipd.it>
*            Alessandro Traspadini <traspadini@dei.unipd.it>
*/

#include "ns3/epc-helper.h"
#include "ns3/epc-enb-application.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/command-line.h"
#include "ns3/mmwave-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/mmwave-iab-net-device.h"
#include <sstream>
#include <iostream>
#include <string>

using namespace ns3;
using namespace mmwave;

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

enum TxRx
{
  TX,
  RX
};

struct BapEvent
 {
  BapEvent (TxRx mode, BapHeader header, uint16_t localAddr)
  {
    m_txRx = mode;
    m_header = header;
    m_localAddr = localAddr;
  }

  uint16_t m_localAddr;
  BapHeader m_header;
  TxRx m_txRx;
 };

std::ostream& operator << (std::ostream& o, BapEvent& a)
{
    o << "TX/RX: " << a.m_txRx << " BAP header: | " << a.m_header <<
          " | \n currently at BAP address " << a.m_localAddr;
    return o;
}

std::list<BapEvent> m_bapEvents {};
static bool m_isConnectionEstablished = false;
double simTimeSeconds = 1.0;
bool useIdealRrc = false;

NS_LOG_COMPONENT_DEFINE ("IabToIabAttachmentTest");


static void
BapPacketReceived (BapHeader header, uint16_t localAddr)
{
  BapEvent event = BapEvent (TxRx::RX, header, localAddr);
  m_bapEvents.push_back (event);
}

static void
BapPacketTransmitted (BapHeader header, uint16_t localAddr)
{ 
  BapEvent event = BapEvent (TxRx::TX, header, localAddr);
  m_bapEvents.push_back (event);
}

static void 
CheckBapEvents ()
{
  for (auto e : m_bapEvents)
  {
    NS_LOG_UNCOND (e);
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
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::MmWaveHelper::UseIdealRrc", BooleanValue (useIdealRrc));

  Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper> ();
  Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
  mmwaveHelper->SetEpcHelper (epcHelper);
  mmwaveHelper->SetHarqEnabled (true);
  std::string dataSensorRate = "500Kbps";
  uint8_t nIab = 3;

  NodeContainer iabNodes;
  NodeContainer ueNode;
  NodeContainer enbNode;

  iabNodes.Create (nIab);
  ueNode.Create (1);
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

  MobilityHelper iabmobility;
  Ptr<ListPositionAllocator> iabPositionAlloc = CreateObject<ListPositionAllocator> ();
  Ptr<UniformRandomVariable> distRv = CreateObject<UniformRandomVariable> ();

  for (unsigned i = 0; i < nIab; i++)
    {
      double dist = distRv->GetValue (0, 50);
      iabPositionAlloc->Add (Vector (dist, 0.0, 0.0));
    }
  iabmobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  iabmobility.SetPositionAllocator (iabPositionAlloc);
  iabmobility.Install (iabNodes);

  Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator> ();
  uePositionAlloc->Add (Vector (1.0, 0.0, 0.0));
  MobilityHelper uemobility;
  uemobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  uemobility.SetPositionAllocator (uePositionAlloc);
  uemobility.Install (ueNode);

  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (50.0, 0.0, 0.0));
  MobilityHelper embMobility;
  embMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  embMobility.SetPositionAllocator (enbPositionAlloc);
  embMobility.Install (enbNode);

  // Install Devices to the nodes
  NetDeviceContainer iabMmWaveDevs = mmwaveHelper->InstallIabDevice (iabNodes);
  NetDeviceContainer ueMmWaveDev = mmwaveHelper->InstallUeDevice (ueNode);
  NetDeviceContainer enbMmWaveDev = mmwaveHelper->InstallEnbDevice (enbNode);

  // Install the IP stack on the UEs
  internet.Install (ueNode);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueMmWaveDev));
  // Assign IP address to UEs, and install applications

  // Set the default gateway for the UE
  Ptr<Ipv4StaticRouting> ueStaticRouting =
      ipv4RoutingHelper.GetStaticRouting (ueNode.Get (0)->GetObject<Ipv4> ());
  ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

  mmwaveHelper->AttachIabTotDonorWithIndex (iabMmWaveDevs.Get (0), enbMmWaveDev, 0);

  // Define IAB network topology
  uint32_t indexIabNodeB = 1;
  uint32_t indexIabParentB = 0;
  uint32_t indexIabNodeC = 2;
  uint32_t indexIabParentC = 1;
  uint32_t indexDonor = 0;

  mmwaveHelper->AttachIabTotIabWithIndex (iabMmWaveDevs, indexIabNodeB, indexIabParentB,
                                          enbMmWaveDev, indexDonor);
  mmwaveHelper->AttachIabTotIabWithIndex (iabMmWaveDevs, indexIabNodeC, indexIabParentC,
                                          enbMmWaveDev, indexDonor);

  // Attach UE with IAB node
  mmwaveHelper->AttachToClosestIab (ueMmWaveDev, NetDeviceContainer (iabMmWaveDevs.Get (2)));

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
  dataSensorParams.IPI = MilliSeconds (500); // On average, a packet every 200 ms
  dataSensorParams.rate = DataRate (dataSensorRate);
  dataSensorParams.simTime = Seconds (simTimeSeconds);
  dataSensorParams.sendSize = 512;
  for (unsigned int i = 0; i < ueNode.GetN (); i++)
    {
      InstallUdpApplication (dataSensorStream, ueNode.Get (i), internetIpIfaces.GetAddress (1),
                             dataSensorParams);
      InstallUdpPacketSink (dataSensorStream, internetIpIfaces.GetAddress (1), remoteHost,
                            dataSensorParams);
      dataSensorParams.basePort++;
    }

  // Schedule check
  auto ueMmw = DynamicCast<MmWaveUeNetDevice> (ueMmWaveDev.Get (0));
  ueMmw->GetRrc ()->TraceConnectWithoutContext ("ConnectionEstablished",
                                                MakeBoundCallback (&ConnectionEstablishedCallback));

  auto iabMmwA = DynamicCast<MmWaveIabNetDevice> (iabMmWaveDevs.Get (0));
  iabMmwA->GetMtRrc ()->TraceConnectWithoutContext (
      "ConnectionEstablished", MakeBoundCallback (&ConnectionEstablishedCallback));

  auto iabMmwB = DynamicCast<MmWaveIabNetDevice> (iabMmWaveDevs.Get (1));
  iabMmwB->GetMtRrc ()->TraceConnectWithoutContext (
      "ConnectionEstablished", MakeBoundCallback (&ConnectionEstablishedCallback));

  auto iabMmwC = DynamicCast<MmWaveIabNetDevice> (iabMmWaveDevs.Get (2));
  iabMmwC->GetMtRrc ()->TraceConnectWithoutContext (
      "ConnectionEstablished", MakeBoundCallback (&ConnectionEstablishedCallback));

  auto enbMmw = DynamicCast<MmWaveEnbNetDevice> (enbMmWaveDev.Get (0));

  auto applicationEnb = enbMmw->GetNode ()->GetApplication (0);
  Ptr<EpcEnbApplication> epcApplicationEnb = DynamicCast<EpcEnbApplication> (applicationEnb);
  uint32_t depthIabA = epcApplicationEnb->GetIabNodeDepth (iabMmwA->GetCellId ());
  uint32_t depthIabB = epcApplicationEnb->GetIabNodeDepth (iabMmwB->GetCellId ());
  uint32_t depthIabC = epcApplicationEnb->GetIabNodeDepth (iabMmwC->GetCellId ());
  uint32_t depthDonor = epcApplicationEnb->GetIabNodeDepth (enbMmw->GetCellId ());

  // Check IAB nodes depth
  NS_ASSERT (depthDonor == 0);
  NS_ASSERT (depthIabA == 1);
  NS_ASSERT (depthIabB == 2);
  NS_ASSERT (depthIabC == 3);

  //Check GetIabParent method
  NS_ASSERT (iabMmwA == iabMmwB->GetIabParent ());
  NS_ASSERT (iabMmwB == iabMmwC->GetIabParent ());

  auto addrNodeA = iabMmwA->GetBap ()->GetLocalAddress ();
  auto addrNodeB = iabMmwB->GetBap ()->GetLocalAddress ();
  auto addrNodeC = iabMmwC->GetBap ()->GetLocalAddress ();

  // Check GetBapAddressFromImsi method
  NS_ASSERT (epcApplicationEnb->GetBapAddressFromImsi (iabMmwA->GetImsi ()) == addrNodeA);
  NS_ASSERT (epcApplicationEnb->GetBapAddressFromImsi (iabMmwB->GetImsi ()) == addrNodeB);
  NS_ASSERT (epcApplicationEnb->GetBapAddressFromImsi (iabMmwC->GetImsi ()) == addrNodeC);

  NS_LOG_UNCOND ("Sending packet along route with BAP addresses: (Node-C) " 
                 << addrNodeC << " -> (Node-B) " << addrNodeB << " -> (Node-A) "
                 << addrNodeA << " -> (Donor) " << enbMmw->GetBap ()->GetLocalAddress ());
  
  NS_LOG_UNCOND ("Sending packet along route with IMSIs: (Node-C) " 
                 << iabMmwC->GetImsi () << " -> (Node-B) " << iabMmwB->GetImsi () << 
                 " -> (Node-A) " << iabMmwA->GetImsi ());
  
   NS_LOG_UNCOND ("Sending packet along route with cell IDs: (Node-C) " 
                 << iabMmwC->GetCellId () << " -> (Node-B) " << iabMmwB->GetCellId () << 
                 " -> (Node-A) " << iabMmwA->GetCellId () 
                 << " -> (Donor) " << enbMmw->GetCellId ());
      
  // Ensure that the data packet takes the proper route
  enbMmw->GetBap ()->TraceConnectWithoutContext ("TxPacket", MakeBoundCallback (&BapPacketTransmitted));
  enbMmw->GetBap ()->TraceConnectWithoutContext ("RxPacket", MakeBoundCallback (&BapPacketReceived));
  iabMmwA->GetBap ()->TraceConnectWithoutContext ("TxPacket", MakeBoundCallback (&BapPacketTransmitted));
  iabMmwA->GetBap ()->TraceConnectWithoutContext ("RxPacket", MakeBoundCallback (&BapPacketReceived));
  iabMmwB->GetBap ()->TraceConnectWithoutContext ("TxPacket", MakeBoundCallback (&BapPacketTransmitted));
  iabMmwB->GetBap ()->TraceConnectWithoutContext ("RxPacket", MakeBoundCallback (&BapPacketReceived));
  iabMmwC->GetBap ()->TraceConnectWithoutContext ("TxPacket", MakeBoundCallback (&BapPacketTransmitted));
  iabMmwC->GetBap ()->TraceConnectWithoutContext ("RxPacket", MakeBoundCallback (&BapPacketReceived));

  mmwaveHelper->EnableTraces ();

  Simulator::Schedule (Seconds (simTimeSeconds - 0.01), &CheckConnectionEstablished);
  Simulator::Schedule (Seconds (simTimeSeconds - 0.01), &CheckBapEvents);

  Simulator::Stop (Seconds (1.0));
  Simulator::Run ();
 
  Simulator::Destroy ();

  return 0;
}
