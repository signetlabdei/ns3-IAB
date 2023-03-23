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
#include <sstream>
#include <string>

using namespace ns3;
using namespace mmwave;

static bool m_isConnectionEstablished = false;
double simTimeSeconds = 1.0;
bool useIdealRrc = false;

NS_LOG_COMPONENT_DEFINE ("IabInstallationTest");

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

int
main (int argc, char *argv[])
{
  // Command line arguments
  CommandLine cmd;
  cmd.AddValue ("useIdealRrc", "Whether to us ea simplified RRC model", useIdealRrc);
  cmd.Parse (argc, argv);
  // double interPacketInterval = 100;

  // LogComponentEnable("MultiModelSpectrumChannel",LOG_LEVEL_INFO);
  // LogComponentEnableAll(LOG_PREFIX_ALL);
  // LogComponentEnable("LteUeRrc", LOG_LEVEL_FUNCTION);
  // LogComponentEnable("LteEnbRrc", LOG_LEVEL_FUNCTION);
  // LogComponentEnableAll(LOG_LEVEL_WARN);
  LogComponentEnableAll (LOG_PREFIX_ALL);

  Config::SetDefault ("ns3::MmWaveHelper::UseIdealRrc", BooleanValue (useIdealRrc));

  Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper> ();
  Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
  mmwaveHelper->SetEpcHelper (epcHelper);
  mmwaveHelper->SetHarqEnabled (true);
  std::string dataSensorRate = "500Kbps";

  NodeContainer iabNodeA;
  NodeContainer iabNodeB;
  NodeContainer ueNode;
  NodeContainer enbNode;

  iabNodeA.Create (1);
  iabNodeB.Create (1);
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
  Ptr<ListPositionAllocator> iabPositionAllocA = CreateObject<ListPositionAllocator> ();
  iabPositionAllocA->Add (Vector (0.0, 0.0, 0.0));
  MobilityHelper iabmobilityA;
  iabmobilityA.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  iabmobilityA.SetPositionAllocator (iabPositionAllocA);
  iabmobilityA.Install (iabNodeA);

  Ptr<ListPositionAllocator> iabPositionAllocB = CreateObject<ListPositionAllocator> ();
  iabPositionAllocB->Add (Vector (10.0, 0.0, 0.0));
  MobilityHelper iabmobilityB;
  iabmobilityB.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  iabmobilityB.SetPositionAllocator (iabPositionAllocB);
  iabmobilityB.Install (iabNodeB);

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
  NetDeviceContainer iabMmWaveDevA = mmwaveHelper->InstallIabDevice (iabNodeA);
  NetDeviceContainer ueMmWaveDev = mmwaveHelper->InstallUeDevice (ueNode);
  NetDeviceContainer enbMmWaveDev = mmwaveHelper->InstallEnbDevice (enbNode);
  NetDeviceContainer iabMmWaveDevB;

  // Install the IP stack on the UEs
  internet.Install (ueNode);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueMmWaveDev));
  // Assign IP address to UEs, and install applications

  // Set the default gateway for the UE
  Ptr<Ipv4StaticRouting> ueStaticRouting =
      ipv4RoutingHelper.GetStaticRouting (ueNode.Get (0)->GetObject<Ipv4> ());
  ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

  mmwaveHelper->AttachIabTotDonorWithIndex (iabMmWaveDevA, enbMmWaveDev, 0);

  iabMmWaveDevB = mmwaveHelper->InstallIabDevice (iabNodeB);
  mmwaveHelper->AttachIabTotDonorWithIndex (iabMmWaveDevB, enbMmWaveDev, 0);

  // Attach UE with IAB node
  // Pass all IAB nodes as arguments, as they need to be "registered" as other BSs in the simulation
  // and to be configured beamforming-wise
  mmwaveHelper->AttachToClosestIab (ueMmWaveDev, NetDeviceContainer (iabMmWaveDevA, iabMmWaveDevB));

  // Schedule check of UE connection to the DU
  auto ueMmw = DynamicCast<MmWaveUeNetDevice> (ueMmWaveDev.Get (0));
  ueMmw->GetRrc ()->TraceConnectWithoutContext ("ConnectionEstablished",
                                                MakeBoundCallback (&ConnectionEstablishedCallback));

  // Schedule check of MT connection to the donor
  auto iabMmwA = DynamicCast<MmWaveIabNetDevice> (iabMmWaveDevA.Get (0));
  iabMmwA->GetMtRrc ()->TraceConnectWithoutContext (
      "ConnectionEstablished", MakeBoundCallback (&ConnectionEstablishedCallback));

  auto iabMmwB = DynamicCast<MmWaveIabNetDevice> (iabMmWaveDevB.Get (0));
  iabMmwB->GetMtRrc ()->TraceConnectWithoutContext (
      "ConnectionEstablished", MakeBoundCallback (&ConnectionEstablishedCallback));

  auto enbMmw = DynamicCast<MmWaveEnbNetDevice> (enbMmWaveDev.Get (0));

  auto applicationEnb = enbMmw->GetNode ()->GetApplication (0);
  Ptr<EpcEnbApplication> epcApplicationEnb = DynamicCast<EpcEnbApplication> (applicationEnb);
  uint32_t depthIabA = epcApplicationEnb->GetIabNodeDepth (iabMmwA->GetCellId ());
  uint32_t depthIabB = epcApplicationEnb->GetIabNodeDepth (iabMmwB->GetCellId ());
  uint32_t depthDonor = epcApplicationEnb->GetIabNodeDepth (enbMmw->GetCellId ());

  // Check IAB nodes depth
  NS_ASSERT (depthIabA == 1);
  NS_ASSERT (depthIabB == 1);
  NS_ASSERT (depthDonor == 0);

  mmwaveHelper->EnableTraces ();

  Simulator::Schedule (Seconds (simTimeSeconds - 0.01), &CheckConnectionEstablished);

  Simulator::Stop (Seconds (1.0));
  Simulator::Run ();

  Simulator::Destroy ();

  return 0;
}
