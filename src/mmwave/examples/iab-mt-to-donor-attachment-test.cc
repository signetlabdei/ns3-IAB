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

static void
ConnectionEstablishedCallback (uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  NS_LOG_FUNCTION (imsi << cellId);

  m_isConnectionEstablished = true;
}

static void
CheckConnectionEstablished ()
{
  NS_ABORT_MSG_IF (!m_isConnectionEstablished, "The MT could not connect to the donor");
}

int
main (int argc, char *argv[])
{
  // Command line arguments
  CommandLine cmd;
  cmd.AddValue ("useIdealRrc", "Enable RLC-AM", useIdealRrc);
  cmd.Parse (argc, argv);

  // LogComponentEnable ("LteUeRrc", LOG_LEVEL_FUNCTION);
  // LogComponentEnable ("LteEnbRrc", LOG_LEVEL_FUNCTION);
  // LogComponentEnable ("MmWaveLteRrcProtocolReal", LOG_LEVEL_LOGIC);
  // LogComponentEnableAll (LOG_PREFIX_TIME);

  Config::SetDefault ("ns3::MmWaveHelper::UseIdealRrc", BooleanValue (useIdealRrc));

  // Create the EPC
  Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper> ();
  Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
  mmwaveHelper->SetEpcHelper (epcHelper);
  mmwaveHelper->SetHarqEnabled (true);
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

  NodeContainer iabNode;
  iabNode.Create (1);
  NodeContainer enbNode;
  enbNode.Create (1);

  // Install Mobility Model
  Ptr<ListPositionAllocator> iabPositionAlloc = CreateObject<ListPositionAllocator> ();
  iabPositionAlloc->Add (Vector (0.0, 0.0, 0.0));
  MobilityHelper iabMobility;
  iabMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  iabMobility.SetPositionAllocator (iabPositionAlloc);
  iabMobility.Install (iabNode);

  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (50.0, 0.0, 0.0));
  MobilityHelper embMobility;
  embMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  embMobility.SetPositionAllocator (enbPositionAlloc);
  embMobility.Install (enbNode);

  // Install Devices to the nodes
  NetDeviceContainer iabMmWaveDev = mmwaveHelper->InstallIabDevice (iabNode);
  NetDeviceContainer enbMmWaveDev = mmwaveHelper->InstallEnbDevice (enbNode);

  // Install the IP stack on the MT
  // internet.Install (iabNode);
  // Ipv4InterfaceContainer iabIpIface;
  // iabIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (iabMmWaveDev));

  // Assign IP address to MT and set its default gateway
  // Ptr<Ipv4StaticRouting> iabStaticRouting = ipv4RoutingHelper.GetStaticRouting (iabNode.Get (0)->GetObject<Ipv4> ());
  // iabStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

  // Attach MT of the IAB node to the donor
  mmwaveHelper->AttachIabTotDonorWithIndex (iabMmWaveDev, enbMmWaveDev, 0);
  mmwaveHelper->EnableTraces ();
  ;

  // Schedule check of MT connection to the donor
  auto iabMmw = DynamicCast<MmWaveIabNetDevice> (iabMmWaveDev.Get (0));
  iabMmw->GetMtRrc ()->TraceConnectWithoutContext (
      "ConnectionEstablished", MakeBoundCallback (&ConnectionEstablishedCallback));

  Simulator::Schedule (Seconds (simTimeSeconds - 0.01), &CheckConnectionEstablished);

  Simulator::Stop (Seconds (1.0));
  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}
