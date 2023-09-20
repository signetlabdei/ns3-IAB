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
#include "ns3/isotropic-antenna-model.h"
#include "ns3/three-gpp-antenna-model.h"
#include <sstream>
#include <string>

using namespace ns3;
using namespace mmwave;

double simTimeSeconds = 2;
bool useIdealRrc = false;
double ipi = 20.0;
double appStartMs = 100.0;
unsigned int numSymResvForOddIabs = 6;
std::string dataSensorRate = "10Mbps";
unsigned int numSymResvCtrl = 2238;
std::string slotFormat = "dddsu";
double bw = 400e6;
double fc = 26e9;

NS_LOG_COMPONENT_DEFINE ("IabInmarsatScenario1");

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
PrintNodeListToFile (Ptr<OutputStreamWrapper> outStream)
{
  for (NodeList::Iterator it = NodeList::Begin (); it != NodeList::End (); ++it)
    {
      Ptr<Node> node = *it;
      int nDevs = node->GetNDevices ();
      for (int j = 0; j < nDevs; j++)
        {
          Ptr<MmWaveUeNetDevice> mmuedev = node->GetDevice (j)->GetObject<MmWaveUeNetDevice> ();
          Ptr<MmWaveEnbNetDevice> mmenbdev = node->GetDevice (j)->GetObject<MmWaveEnbNetDevice> ();
          Ptr<MmWaveIabNetDevice> iabDev = node->GetDevice (j)->GetObject<MmWaveIabNetDevice> ();
          if (mmuedev)
            {
              Vector pos = node->GetObject<MobilityModel> ()->GetPosition ();
              *outStream->GetStream () << "ue " << mmuedev->GetImsi ()  << " " << pos.x << " " << pos.y
                      << std::endl;
            }
          else if (mmenbdev)
            {
              Vector pos = node->GetObject<MobilityModel> ()->GetPosition ();
              *outStream->GetStream () << "donor " << mmenbdev->GetCellId ()  << " " << pos.x << " " << pos.y
                      << std::endl;
            }
          else if (iabDev)
            {
              Vector pos = node->GetObject<MobilityModel> ()->GetPosition ();
              *outStream->GetStream () << "iabnode " << iabDev->GetCellId ()  << " " << pos.x << " " << pos.y
                      << std::endl;
            }
        }
    }
}

unsigned int
PacketByteSizeFromBitrateAndIpi (DataRate rate, Time IPI)
{
  double size = rate.GetBitRate () * IPI.GetSeconds ();
  return std::ceil (size / 8);
}

void
PrintIabTopologyInformation (Ptr<OutputStreamWrapper> stream, Ptr<EpcEnbApplication> donorApp,
                             NetDeviceContainer iabDevs)
{
  *stream->GetStream () << "cell_id imsi parent_cell_id parent_imsi\n";

  for (unsigned int devIdx = 0; devIdx < iabDevs.GetN (); devIdx++)
  {
    auto mmwIabDev = DynamicCast<MmWaveIabNetDevice> (iabDevs.Get (devIdx));
    uint16_t parentCellId;
    uint16_t childCellId = mmwIabDev->GetCellId ();
    uint32_t childImsi = mmwIabDev->GetImsi ();
    uint32_t parentImsi = donorApp->GetParentInfoFromChildCellId (childCellId, parentCellId);

    *stream->GetStream () << childCellId << " " << childImsi << " "
                          << parentCellId << " " << parentImsi << std::endl;
  }

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

  std::string iabCbPath = "src/mmwave/model/Codebooks/8x8.txt";
  std::string ueCbPath = "src/mmwave/model/Codebooks/2x2.txt";
  std::string enbCbPath = "src/mmwave/model/Codebooks/8x8.txt";

  // Command line arguments
  CommandLine cmd;
  cmd.AddValue("bw", "the system bandwidth", bw);
  cmd.AddValue("fc", "the system carrier frequency", fc);
  cmd.AddValue ("iabCbPath", "The full path to the IAB codebook", iabCbPath);
  cmd.AddValue ("enbCbPath", "The full path to the eNB codebook", enbCbPath);
  cmd.AddValue ("useIdealRrc", "Whether to us ea simplified RRC model", useIdealRrc);
  cmd.AddValue ("numSymResvForOddIabs", "How many symbols to reserve for odd layer IAB nodes",
                numSymResvForOddIabs);
  cmd.AddValue ("simTime", "The simulation time [s]", simTimeSeconds);
  cmd.AddValue ("ipi", "The inter-packet interval time [ms]", ipi);
  cmd.AddValue ("appStartMs", "The application start time [ms]", appStartMs);
  cmd.AddValue ("dataSensorRate", "The data rate of the applications", dataSensorRate);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::MmWaveHelper::UseIdealRrc", BooleanValue (useIdealRrc));
  Config::SetDefault ("ns3::PhasedArrayModel::AntennaElement",
                      PointerValue (CreateObject<ThreeGppAntennaModel> ()));
  Config::SetDefault ("ns3::ThreeGppPropagationLossModel::ShadowingEnabled", BooleanValue (false));
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::NumSymResvForChildrenDu",
                      UintegerValue (numSymResvForOddIabs));

  // Enable additional APP-level headers to retrieve delay
  Config::SetDefault ("ns3::UdpClient::EnableSeqTsSizeHeader", BooleanValue (true));
  Config::SetDefault ("ns3::PacketSink::EnableSeqTsSizeHeader", BooleanValue (true));

  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::SlotsFormat",
                      StringValue(slotFormat));
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::NumSymReservedCtrl",
                      UintegerValue (numSymResvCtrl));

  Config::SetDefault("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue (fc));
  Config::SetDefault("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue (bw));

  // Increase RLC buffers size
  Config::SetDefault ("ns3::LteRlcAm::MaxTxBufferSize", UintegerValue (25 * 1024 * 1024));
  Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue (25 * 1024 * 1024));

  Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper> ();
  mmwaveHelper->SetChannelConditionModelType ("ns3::AlwaysLosChannelConditionModel");

  Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
  mmwaveHelper->SetEpcHelper (epcHelper);
  mmwaveHelper->SetHarqEnabled (true);
  mmwaveHelper->SetBeamformingModelType ("ns3::MmWaveCodebookBeamforming");

  mmwaveHelper->SetIabBeamformingCodebookAttribute ("CodebookFilename", StringValue (iabCbPath));
   mmwaveHelper->SetUeBeamformingCodebookAttribute ("CodebookFilename", StringValue (ueCbPath));
  mmwaveHelper->SetEnbBeamformingCodebookAttribute ("CodebookFilename", StringValue (enbCbPath));

  uint32_t numIab = 2;

  NodeContainer iabNodes;
  NodeContainer ueNodes;
  NodeContainer enbNode;

  iabNodes.Create (numIab);
  ueNodes.Create (1);
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
  iabPositionAlloc->Add (Vector (-80.0, 50.0, 5.0));
  iabPositionAlloc->Add (Vector (-50.0, 80.0, 5.0));

  iabmobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  iabmobility.SetPositionAllocator (iabPositionAlloc);
  iabmobility.Install (iabNodes);

  Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator> ();
  uePositionAlloc->Add (Vector (-100.0, 40.0, 0.0));
  MobilityHelper uemobility;
  uemobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  uemobility.SetPositionAllocator (uePositionAlloc);
  uemobility.Install (ueNodes);

  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (0.0, 0.0, 5.0));
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator (enbPositionAlloc);
  enbMobility.Install (enbNode);

  // Install Devices to the nodes
  NetDeviceContainer iabMmWaveDev = mmwaveHelper->InstallIabDevice (iabNodes);
  NetDeviceContainer ueMmWaveDevs = mmwaveHelper->InstallUeDevice (ueNodes);
  NetDeviceContainer enbMmWaveDev = mmwaveHelper->InstallEnbDevice (enbNode);

  for (uint32_t i = 0; i < iabMmWaveDev.GetN (); i++)
    {
      Ptr<MmWaveIabNetDevice> iabDevTest = DynamicCast<MmWaveIabNetDevice> (iabMmWaveDev.Get (i));
      std::cout<<"IAB node "<<i<<" IMSI:  "<<iabDevTest->GetImsi ()<<" CellID: "<<iabDevTest->GetCellId ()<<std::endl;
    }
  Ptr<MmWaveUeNetDevice> ueDevTest = DynamicCast<MmWaveUeNetDevice> (ueMmWaveDevs.Get (0));
  std::cout<<"UE node IMSI:  "<<ueDevTest->GetImsi ()<<std::endl;

  Ptr<MmWaveEnbNetDevice> enbDevTest = DynamicCast<MmWaveEnbNetDevice> (enbMmWaveDev.Get (0));
  std::cout<<"Donor node CellID:  "<<enbDevTest->GetCellId ()<<std::endl;

  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueMmWaveDevs));
  // Assign IP address to UEs, and install applications
  for (uint32_t u = 0; u < ueNodes.GetN (); u++)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting =
          ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  Ipv4InterfaceContainer iabIpIface;
  for (unsigned int i = 0; i < iabNodes.GetN (); i++)
    {
      Ptr<MmWaveIabNetDevice> iabDevice = iabMmWaveDev.Get (i)->GetObject<MmWaveIabNetDevice> ();
      iabIpIface.Add (iabDevice->GetIpInterface ());
      Ptr<Ipv4StaticRouting> iabStaticRouting =
          ipv4RoutingHelper.GetStaticRouting (iabNodes.Get (i)->GetObject<Ipv4> ());
      iabStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  mmwaveHelper->AttachIabTotDonorWithIndex (iabMmWaveDev.Get (0), enbMmWaveDev, 0);
  mmwaveHelper->AttachIabTotDonorWithIndex (iabMmWaveDev.Get (1), enbMmWaveDev, 0);
  mmwaveHelper->AttachToClosestIab (ueMmWaveDevs, iabMmWaveDev);

  std::cout<<"UE target IAB IMSI: "<<ueDevTest->GetTargetIab ()->GetImsi ()<<std::endl;

  unsigned int ccId = 0;
  for (unsigned int i = 0; i < iabNodes.GetN (); i++)
    {
      Ptr<MmWaveIabNetDevice> iabDevTest = DynamicCast<MmWaveIabNetDevice> (iabMmWaveDev.Get (i));
      iabDevTest->GetMtAntenna (ccId)->SetAttribute ("BearingAngle", DoubleValue (-M_PI / 2));
      iabDevTest->GetDuAntenna (ccId)->SetAttribute ("BearingAngle", DoubleValue (M_PI / 2));
    }

  // Base ports
  uint16_t dlDataSensorPort = 1235; // base port for the UL data stream from the sensors
  AsciiTraceHelper asciiTraceHelper;

  // Install DL data applications on the IAB node
  AppSetupParams dlAppParams;
  dlAppParams.basePort = dlDataSensorPort;
  dlAppParams.startTime = MilliSeconds (appStartMs);
  dlAppParams.endTime = Seconds (simTimeSeconds); // Some cooldown time
  dlAppParams.IPI = MicroSeconds (ipi);
  dlAppParams.rate = DataRate (dataSensorRate);
  dlAppParams.simTime = Seconds (simTimeSeconds);
  dlAppParams.sendSize = 512;
  Ptr<OutputStreamWrapper> dlNodeAppStream =
      asciiTraceHelper.CreateFileStream ("DL-app-trace-node.txt");
  *dlNodeAppStream->GetStream () << "rx_tx time packet_size port_to port_from delay\n";

  for (unsigned int i = 0; i < ueNodes.GetN (); i++)
    {
      InstallUdpApplication (dlNodeAppStream, remoteHost, ueIpIface.GetAddress (i),
                         dlAppParams);
      InstallUdpPacketSink (dlNodeAppStream, ueIpIface.GetAddress (i), ueNodes.Get (i),
                        dlAppParams);
      dlAppParams.basePort++;
    }

  mmwaveHelper->EnableTraces ();

  // Trace the IAB topology
  auto donorApp = enbNode.Get (0)->GetApplication (0)->GetObject<EpcEnbApplication> ();
  Ptr<OutputStreamWrapper> iabTopologyStream =
      asciiTraceHelper.CreateFileStream ("IAB-topology.txt");
  PrintIabTopologyInformation (iabTopologyStream, donorApp, iabMmWaveDev);

  Ptr<OutputStreamWrapper> scenTopologyStream =
      asciiTraceHelper.CreateFileStream ("scenario-topology.txt");
  PrintNodeListToFile (scenTopologyStream);

  Simulator::Stop (Seconds (simTimeSeconds));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
