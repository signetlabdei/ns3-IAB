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
#include "ns3/epc-enb-application.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/command-line.h"
#include "ns3/mmwave-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/mmwave-iab-net-device.h"
#include "ns3/isotropic-antenna-model.h"
#include <sstream>
#include <string>

using namespace ns3;
using namespace mmwave;

double simTimeSeconds = 0.15;
bool useIdealRrc = false;
uint8_t m_receivedPacketCounter = 0;
double ipiMs = 100.0;
double appStartMs = 100.0;
unsigned int expectedPackets = std::ceil ((simTimeSeconds * 1e3 - appStartMs) / ipiMs) * 9;
unsigned int numSymResvForOddIabs = 6;

NS_LOG_COMPONENT_DEFINE ("IabInmarsatScenario");

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
  NS_ABORT_MSG_IF (m_receivedPacketCounter != expectedPackets,
                   "Received " << (uint32_t) m_receivedPacketCounter << " packets, expected "
                               << expectedPackets);
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
  *stream->GetStream () << "Rx\t" << Simulator::Now ().GetNanoSeconds () << " "
                        << packet->GetSize () << "\n";
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
  app.Get (0)->TraceConnectWithoutContext ("Rx", MakeBoundCallback (RxSinkHeader, traceStream));
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
  Config::SetDefault ("ns3::PhasedArrayModel::AntennaElement",
                      PointerValue (CreateObject<IsotropicAntennaModel> ()));
  Config::SetDefault ("ns3::ThreeGppPropagationLossModel::ShadowingEnabled", BooleanValue (false));
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::NumSymResvForChildrenDu",
                      UintegerValue (numSymResvForOddIabs));

  Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper> ();
  mmwaveHelper->SetChannelConditionModelType ("ns3::AlwaysLosChannelConditionModel");

  Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
  mmwaveHelper->SetEpcHelper (epcHelper);
  mmwaveHelper->SetHarqEnabled (true);
  mmwaveHelper->SetBeamformingModelType ("ns3::MmWaveSvdBeamforming");
  std::string dataSensorRate = "2kbps";

  mmwaveHelper->SetIabBeamformingCodebookAttribute (
      "CodebookFilename", StringValue ("src/mmwave/model/Codebooks/8x8.txt"));
  mmwaveHelper->SetEnbBeamformingCodebookAttribute (
      "CodebookFilename", StringValue ("src/mmwave/model/Codebooks/8x8.txt"));

  uint32_t numIab = 9;

  NodeContainer iabNodes;
  NodeContainer enbNode;

  iabNodes.Create (numIab);
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
  // Layer 1
  iabPositionAlloc->Add (Vector (-80.0, 30.0, 5.0));
  iabPositionAlloc->Add (Vector (-50.0, 50.0, 5.0));
  iabPositionAlloc->Add (Vector (50.0, 50.0, 5.0));
  iabPositionAlloc->Add (Vector (80.0, 30.0, 5.0));
  // Layer 2
  iabPositionAlloc->Add (Vector (-130.0, 30.0, 5.0));
  iabPositionAlloc->Add (Vector (-80.0, 80.0, 5.0));
  iabPositionAlloc->Add (Vector (-50.0, 100.0, 5.0));
  // Layer 3
  iabPositionAlloc->Add (Vector (-80.0, 150.0, 5.0));
  iabPositionAlloc->Add (Vector (-30, 150.0, 5.0));

  iabmobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  iabmobility.SetPositionAllocator (iabPositionAlloc);
  iabmobility.Install (iabNodes);

  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (0.0, 0.0, 3.0));
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator (enbPositionAlloc);
  enbMobility.Install (enbNode);

  // Install Devices to the nodes
  NetDeviceContainer iabMmWaveDev = mmwaveHelper->InstallIabDevice (iabNodes);
  NetDeviceContainer enbMmWaveDev = mmwaveHelper->InstallEnbDevice (enbNode);

  for (unsigned int i = 0; i < iabNodes.GetN (); i++)
    {
      Ptr<Ipv4StaticRouting> iabStaticRouting =
          ipv4RoutingHelper.GetStaticRouting (iabNodes.Get (i)->GetObject<Ipv4> ());
      iabStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  mmwaveHelper->AttachIabTotDonorWithIndex (iabMmWaveDev.Get (0), enbMmWaveDev, 0);
  mmwaveHelper->AttachIabTotDonorWithIndex (iabMmWaveDev.Get (1), enbMmWaveDev, 0);
  mmwaveHelper->AttachIabTotDonorWithIndex (iabMmWaveDev.Get (2), enbMmWaveDev, 0);
  mmwaveHelper->AttachIabTotDonorWithIndex (iabMmWaveDev.Get (3), enbMmWaveDev, 0);
  mmwaveHelper->AttachIabTotIabWithIndex (iabMmWaveDev, 4, 0, enbMmWaveDev, 0);
  mmwaveHelper->AttachIabTotIabWithIndex (iabMmWaveDev, 5, 0, enbMmWaveDev, 0);
  mmwaveHelper->AttachIabTotIabWithIndex (iabMmWaveDev, 6, 1, enbMmWaveDev, 0);
  mmwaveHelper->AttachIabTotIabWithIndex (iabMmWaveDev, 7, 6, enbMmWaveDev, 0);
  mmwaveHelper->AttachIabTotIabWithIndex (iabMmWaveDev, 8, 6, enbMmWaveDev, 0);

  unsigned int ccId = 0;
  for (unsigned int i = 0; i < iabNodes.GetN (); i++)
    {
      Ptr<MmWaveIabNetDevice> iabDevTest = DynamicCast<MmWaveIabNetDevice> (iabMmWaveDev.Get(i));
      iabDevTest->GetDuAntenna (ccId)->SetAttribute ("BearingAngle", DoubleValue (0.0));  //TODO: change this
      iabDevTest->GetMtAntenna (ccId)->SetAttribute ("BearingAngle", DoubleValue (0.0));  //TODO: change this
    }

  // // Base ports
  uint16_t ulDataSensorPort = 1235; // base port for the UL data stream from the sensors
  AsciiTraceHelper asciiTraceHelper;

  // // Install UL data applications on the IAB node
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

  for (unsigned int i = 0; i < iabNodes.GetN (); i++)
    {
      InstallUdpApplication (ulNodeAppStream, iabNodes.Get (i), internetIpIfaces.GetAddress (1),
                             ulAppParams);
      InstallUdpPacketSink (ulNodeAppStream, internetIpIfaces.GetAddress (1), remoteHost,
                            ulAppParams);
      ulAppParams.basePort++;
    }

  mmwaveHelper->EnableTraces ();

  Simulator::Schedule (Seconds (simTimeSeconds - 0.01), &CheckAppReceptionCallback);

  Simulator::Stop (Seconds (0.15));
  Simulator::Run ();

  Simulator::Destroy ();

  return 0;
}
