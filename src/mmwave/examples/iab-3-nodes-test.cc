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
#include <string>

using namespace ns3;
using namespace mmwave;

double simTimeSeconds = 0.25;
bool useIdealRrc = false;
uint8_t m_receivedPacketCounter = 0;
double ipiMs = 100.0;
double appStartMs = 100.0;
unsigned int expectedPackets = std::ceil ((simTimeSeconds * 1e3 - appStartMs) / ipiMs)*3;
unsigned int numSymResvForOddIabs = 6;

NS_LOG_COMPONENT_DEFINE ("IabE2EData1Node");

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

static void
AppReceptionCallback (uint32_t packetSize)
{
  m_receivedPacketCounter++;
}

static void
CheckAppReceptionCallback ()
{
  NS_ABORT_MSG_IF (m_receivedPacketCounter != expectedPackets , 
                  "Received "<< (uint32_t) m_receivedPacketCounter<<" packets, expected " << expectedPackets);
}

double
PacketByteSizeFromBitrateAndIpi (DataRate rate, Time IPI)
{
  double size = rate.GetBitRate () * IPI.GetSeconds ();
  return size / 8;
}

static void
RxSinkHeader (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &from)
{
  *stream->GetStream () << "Rx\t" << Simulator::Now ().GetNanoSeconds ()  << " " << packet->GetSize () << "\n";
  AppReceptionCallback (packet->GetSize ());
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
  app.Get (0)->TraceConnectWithoutContext ("Rx",
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
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::NumSymResvForChildrenDu",
                      UintegerValue (numSymResvForOddIabs));

  Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper> ();
  Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
  mmwaveHelper->SetEpcHelper (epcHelper);
  mmwaveHelper->SetHarqEnabled (true);
  std::string dataSensorRate = "8kbps";

  double minDistance = 10.0; // donor-IAB distance in meters
  double maxDistance = 200.0; // donor-IAB distance in meters

  uint32_t numIab = 3;

  NodeContainer iabNode;
  NodeContainer enbNode;

  iabNode.Create (numIab);
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

  for (uint32_t u = 0; u < numIab; ++u)
  {
    double dist = distRv->GetValue (minDistance, maxDistance);
    iabPositionAlloc->Add (Vector (dist, 0.0, 0.0));
  }

  iabmobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  iabmobility.SetPositionAllocator (iabPositionAlloc);
  iabmobility.Install (iabNode);

  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (0.0, 0.0, 0.0));
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator (enbPositionAlloc);
  enbMobility.Install (enbNode);

  // Install Devices to the nodes
  NetDeviceContainer iabMmWaveDev = mmwaveHelper->InstallIabDevice (iabNode);
  NetDeviceContainer enbMmWaveDev = mmwaveHelper->InstallEnbDevice (enbNode);

  // Set default gateway for IAB nodes

  for (unsigned int i = 0; i < iabNode.GetN (); i++)
  {
    Ptr<Ipv4StaticRouting> iabStaticRouting =
      ipv4RoutingHelper.GetStaticRouting (iabNode.Get (i)->GetObject<Ipv4> ());
    iabStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
  }

  mmwaveHelper->AttachIabTotDonorWithIndex (iabMmWaveDev.Get (0), enbMmWaveDev, 0);
  mmwaveHelper->AttachIabTotIabWithIndex (iabMmWaveDev, 1, 0, enbMmWaveDev, 0);
  mmwaveHelper->AttachIabTotIabWithIndex (iabMmWaveDev, 2, 0, enbMmWaveDev, 0);

  // Base ports
  uint16_t ulDataSensorPort = 1235; // base port for the UL data stream from the sensors
  AsciiTraceHelper asciiTraceHelper;

  // Install UL data applications on the IAB node
  AppSetupParams ulAppParams;
  ulAppParams.basePort = ulDataSensorPort;
  ulAppParams.startTime = MilliSeconds (appStartMs);
  ulAppParams.endTime = Seconds (1); // Some cooldown time
  ulAppParams.IPI = MilliSeconds (ipiMs);
  ulAppParams.rate = DataRate (dataSensorRate);
  ulAppParams.simTime = Seconds (simTimeSeconds);
  ulAppParams.sendSize = 512;
  Ptr<OutputStreamWrapper> ulNodeAppStream =
      asciiTraceHelper.CreateFileStream ("UL-app-trace-node.txt");
  for (unsigned int i = 0; i < iabNode.GetN (); i++)
  {
    InstallUdpApplication (ulNodeAppStream, iabNode.Get (i), internetIpIfaces.GetAddress (1),
                           ulAppParams);
    InstallUdpPacketSink (ulNodeAppStream, internetIpIfaces.GetAddress (1), remoteHost,
                          ulAppParams);
    ulAppParams.basePort++;
  }

  mmwaveHelper->EnableTraces ();  

  Simulator::Schedule (Seconds (simTimeSeconds - 0.01), &CheckAppReceptionCallback);

  Simulator::Stop (Seconds (0.25));
  Simulator::Run ();

  Simulator::Destroy ();

  return 0;
}
