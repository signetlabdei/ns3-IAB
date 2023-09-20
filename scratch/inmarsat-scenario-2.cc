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
#include "ns3/isotropic-antenna-model.h"
#include "ns3/three-gpp-antenna-model.h"
#include <sstream>
#include <string>

using namespace ns3;
using namespace mmwave;

double simTimeSeconds = 10;
bool useIdealRrc = false;
double ipi = 20.0;
double appStartMs = 100.0;
unsigned int numSymResvForOddIabs = 0;
unsigned int numSymResvCtrl = 2238;
double rainRate = 10;
std::string dlDataSensorRate = "75Mbps";
std::string slotFormat = "dddsu";
double powerTx = 30;
double bw = 400e6;
double fc = 26e9;
unsigned int configurationPhy = 0;
unsigned int ueAntennaNum = 64;
unsigned int nAntColumn = 8;
unsigned int nAntRow = 8;
unsigned int rlcBufferSize = 25 * 1024 * 1024;

NS_LOG_COMPONENT_DEFINE ("IabInmarsatScenario2");

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
  std::string ueCbPath = "src/mmwave/model/Codebooks/8x8.txt";
  std::string enbCbPath = "src/mmwave/model/Codebooks/8x8.txt";

  // Command line arguments
  CommandLine cmd;
  cmd.AddValue("bw", "the system bandwidth", bw);
  cmd.AddValue("fc", "the system carrier frequency", fc);
  cmd.AddValue ("iabCbPath", "The full path to the IAB codebook", ueCbPath);
  cmd.AddValue ("enbCbPath", "The full path to the eNB codebook", enbCbPath);
  cmd.AddValue ("useIdealRrc", "Whether to us ea simplified RRC model", useIdealRrc);
  cmd.AddValue ("numSymResvForOddIabs", "How many symbols to reserve for odd layer IAB nodes",
                numSymResvForOddIabs);
  cmd.AddValue ("numSymResvCtrl", "How many symbols to reserve for control messages",
                numSymResvCtrl);
  cmd.AddValue ("simTime", "The simulation time [s]", simTimeSeconds);
  cmd.AddValue ("ipi", "The inter-packet interval time [ms]", ipi);
  cmd.AddValue ("appStartMs", "The application start time [ms]", appStartMs);
  cmd.AddValue ("dataSensorRate", "The downlink data rate of the applications", dlDataSensorRate);
  cmd.AddValue ("rainRate", "The rain rate for the propagation model [mm/h]", rainRate);
  cmd.AddValue ("slotFormat", "The pattern of the slots format (d/u/s)", slotFormat);
  cmd.AddValue ("rlcBufferSize", "The pattern of the slots format (d/u/s)", rlcBufferSize);
  cmd.AddValue ("ueAntennaNum", "The number of antennas at the UE", ueAntennaNum);
  cmd.AddValue ("powerTx", "The transmission power [dBm]", powerTx);
  cmd.AddValue ("configurationPhy", "The configuration of the PHY (0: 4x6, 1: 8x8)", configurationPhy);
  cmd.Parse (argc, argv);

  if (configurationPhy==0)
  {
    powerTx = 26;
    ueAntennaNum = 24;
    nAntColumn = 6;
    nAntRow = 4;
    ueCbPath.erase(ueCbPath.end()-7, ueCbPath.end());
    ueCbPath.append("4x6.txt");
    enbCbPath.erase(enbCbPath.end()-7, enbCbPath.end());
    enbCbPath.append("4x6.txt");
  }
  else
  {
    powerTx = 36;
    ueAntennaNum = 64;
    nAntColumn = 8;
    nAntRow = 8;
  }

  Config::SetDefault ("ns3::MmWaveHelper::UseIdealRrc", BooleanValue (useIdealRrc));

  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::SlotsFormat",
                      StringValue(slotFormat));
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::NumSymReservedCtrl",
                      UintegerValue (numSymResvCtrl));
  Config::SetDefault ("ns3::MmWavePhyMacCommon::Numerology",
                      EnumValue(3));

  Config::SetDefault ("ns3::TwoRayPropagationLossModel::RainRate", DoubleValue (rainRate));
  Config::SetDefault("ns3::TwoRayPropagationLossModel::Frequency", DoubleValue (fc));

  Config::SetDefault ("ns3::PhasedArrayModel::AntennaElement",
                      PointerValue (CreateObject<ThreeGppAntennaModel> ()));
  Config::SetDefault ("ns3::ThreeGppPropagationLossModel::ShadowingEnabled", BooleanValue (true));
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::NumSymResvForChildrenDu",
                      UintegerValue (numSymResvForOddIabs));

  Config::SetDefault ("ns3::MmWaveEnbPhy::TxPower", DoubleValue (powerTx));
  Config::SetDefault ("ns3::MmWaveUePhy::TxPower", DoubleValue (powerTx));

  // Enable additional APP-level headers to retrieve delay
  Config::SetDefault ("ns3::UdpClient::EnableSeqTsSizeHeader", BooleanValue (true));
  Config::SetDefault ("ns3::PacketSink::EnableSeqTsSizeHeader", BooleanValue (true));

  Config::SetDefault("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue (fc));
  Config::SetDefault("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue (bw));

  // Increase RLC buffers size
  Config::SetDefault ("ns3::LteRlcAm::MaxTxBufferSize", UintegerValue (rlcBufferSize));
  Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue (rlcBufferSize));

  Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper> ();
  mmwaveHelper->SetChannelConditionModelType ("ns3::AlwaysLosChannelConditionModel");

  mmwaveHelper->SetPathlossModelType ("ns3::TwoRayPropagationLossModel");

  Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
  mmwaveHelper->SetEpcHelper (epcHelper);
  mmwaveHelper->SetHarqEnabled (true);
  mmwaveHelper->SetBeamformingModelType ("ns3::MmWaveCodebookBeamforming");
  
  mmwaveHelper->SetUePhasedArrayModelAttribute ("NumColumns",
                                                UintegerValue (nAntColumn));
  mmwaveHelper->SetUePhasedArrayModelAttribute ("NumRows",
                                                UintegerValue (nAntRow));

  mmwaveHelper->SetEnbPhasedArrayModelAttribute ("NumColumns",
                                                UintegerValue (nAntColumn));
  mmwaveHelper->SetEnbPhasedArrayModelAttribute ("NumRows",
                                                UintegerValue (nAntRow));

  mmwaveHelper->SetUeBeamformingCodebookAttribute ("CodebookFilename", StringValue (ueCbPath));
  mmwaveHelper->SetEnbBeamformingCodebookAttribute ("CodebookFilename", StringValue (enbCbPath));

  uint32_t numUe = 5;

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
  MobilityHelper ueMobility;
  Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator> ();

  uePositionAlloc->Add (Vector (500.0, 50.0, 5.0));
  uePositionAlloc->Add (Vector (1000.0, 250.0, 5.0));
  uePositionAlloc->Add (Vector (2500.0, 500.0, 5.0));
  uePositionAlloc->Add (Vector (3500.0, 750.0, 5.0));
  uePositionAlloc->Add (Vector (4750.0, 1250.0, 5.0));

  ueMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  ueMobility.SetPositionAllocator (uePositionAlloc);
  ueMobility.Install (ueNodes);

  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (0.0, 0.0, 5.0));
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator (enbPositionAlloc);
  enbMobility.Install (enbNode);

  // Install Devices to the nodes
  NetDeviceContainer ueMmWaveDevs = mmwaveHelper->InstallUeDevice (ueNodes);
  NetDeviceContainer enbMmWaveDev = mmwaveHelper->InstallEnbDevice (enbNode);

  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (ueMmWaveDevs);
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting =
          ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  mmwaveHelper->AttachToClosestEnb (ueMmWaveDevs, enbMmWaveDev);

  unsigned int ccId = 0;
  for (unsigned int i = 0; i < ueNodes.GetN (); i++)
    {
      Ptr<MmWaveUeNetDevice> ueDevTest = DynamicCast<MmWaveUeNetDevice> (ueMmWaveDevs.Get (i));
      ueDevTest->GetAntenna (ccId)->SetAttribute ("BearingAngle", DoubleValue (-M_PI / 2));
    }

  // Base ports
  uint16_t dlDataSensorPort = 1235; // base port for the DL data stream from the sensors
  uint16_t ulDataSensorPort = 1435; // base port for the UL data stream from the sensors
  AsciiTraceHelper asciiTraceHelper;

  // Install DL data applications on the IAB node
  AppSetupParams dlAppParams;
  dlAppParams.basePort = dlDataSensorPort;
  dlAppParams.startTime = MilliSeconds (appStartMs);
  dlAppParams.endTime = Seconds (simTimeSeconds); // Some cooldown time
  dlAppParams.IPI = MicroSeconds (ipi);
  dlAppParams.rate = DataRate (dlDataSensorRate);
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

  DataRate ulDataSensorRate = DataRate (dlDataSensorRate);
  ulDataSensorRate = ulDataSensorRate * 0.1;
  
  // Install UL data applications on the IAB node
  AppSetupParams ulAppParams;
  ulAppParams.basePort = ulDataSensorPort;
  ulAppParams.startTime = MilliSeconds (appStartMs);
  ulAppParams.endTime = Seconds (simTimeSeconds); // Some cooldown time
  ulAppParams.IPI = MicroSeconds (ipi);
  ulAppParams.rate = ulDataSensorRate;
  ulAppParams.simTime = Seconds (simTimeSeconds);
  ulAppParams.sendSize = 512;
  Ptr<OutputStreamWrapper> ulNodeAppStream =
      asciiTraceHelper.CreateFileStream ("UL-app-trace-node.txt");
  *ulNodeAppStream->GetStream () << "rx_tx time packet_size port_to port_from delay\n";
  
  for (unsigned int i = 0; i < ueNodes.GetN (); i++)
    {
      InstallUdpApplication (ulNodeAppStream, ueNodes.Get (i), internetIpIfaces.GetAddress (1),
                             ulAppParams);
      InstallUdpPacketSink (ulNodeAppStream, internetIpIfaces.GetAddress (1), remoteHost,
                            ulAppParams);
      ulAppParams.basePort++;
    }

  mmwaveHelper->EnableTraces ();

  Simulator::Stop (Seconds (simTimeSeconds));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
