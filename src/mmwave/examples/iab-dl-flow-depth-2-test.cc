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

double simTimeSeconds = 0.5;
bool useIdealRrc = false;
std::string slotFormat = "dddsu";
unsigned int numSymResvDlCtrl = 1000;
unsigned int numSymResvUlCtrl = 1000;
double rainRate = 10;
double ipi = 25.0;
double appStartMs = 100.0;
unsigned int numSymResvForOddIabs = 6;
unsigned int numSymResvForDl = 3;
std::string dataSensorRate = "1Mbps";
double powerTx = 30;
double bw = 400e6;
double fc = 26e9;
unsigned int rlcBufferSize = 25 * 1024 * 1024;
double scenScale = 1.0;
uint32_t numIab = 2;
uint32_t numReceivingIab = 1;
uint8_t m_receivedPacketCounter = 0;
unsigned int expectedPackets = std::ceil ((simTimeSeconds * 1e3 - appStartMs) / ipi) * numReceivingIab;


NS_LOG_COMPONENT_DEFINE ("IabDlFlowDepth2Test");

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
  unsigned int roundedSize = std::ceil (size / 8);
  // NS_LOG_UNCOND("Rate " << rate << " ipi " << IPI << 
  //               " rounded packet size " << roundedSize);
  return roundedSize;
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
  client.SetAttribute ("MaxPackets", UintegerValue (UINT32_MAX));
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
  app.Get (0)->TraceConnectWithoutContext ("RxWithSeqTsSize",
                                           MakeBoundCallback (RxSinkHeader, traceStream));
  app.Start (Seconds (0));
  app.Stop (params.simTime);
}

int
main (int argc, char *argv[])
{
  LogComponentEnableAll(LOG_PREFIX_TIME);
  LogComponentEnableAll(LOG_PREFIX_FUNC);

  // Command line arguments
  CommandLine cmd;
  cmd.AddValue("bw", "the system bandwidth", bw);
  cmd.AddValue("fc", "the system carrier frequency", fc);
  cmd.AddValue ("useIdealRrc", "Whether to us ea simplified RRC model", useIdealRrc);
  cmd.AddValue ("numSymResvForOddIabs", "How many symbols to reserve for odd layer IAB nodes",
                numSymResvForOddIabs);
  cmd.AddValue ("numSymResvDlCtrl", "How many symbols to reserve for DL control messages",
                numSymResvDlCtrl);
  cmd.AddValue ("numSymResvUlCtrl", "How many symbols to reserve for UL control messages",
                numSymResvUlCtrl);

  cmd.AddValue ("simTime", "The simulation time [s]", simTimeSeconds);
  cmd.AddValue ("ipi", "The inter-packet interval time [ms]", ipi);
  cmd.AddValue ("appStartMs", "The application start time [ms]", appStartMs);
  cmd.AddValue ("dataSensorRate", "The downlink data rate of the applications", dataSensorRate);
  cmd.AddValue ("rainRate", "The rain rate for the propagation model [mm/h]", rainRate);
  cmd.AddValue ("slotFormat", "The pattern of the slots format (d/u/s)", slotFormat);
  cmd.AddValue ("rlcBufferSize", "The pattern of the slots format (d/u/s)", rlcBufferSize);
  cmd.AddValue ("scenScale", "Overall scale of the simulation scenario", scenScale);
  cmd.Parse (argc, argv);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::MmWaveHelper::UseIdealRrc", BooleanValue (useIdealRrc));

  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::SlotsFormat",
                      StringValue(slotFormat));
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::NumSymReservedDlCtrl",
                      UintegerValue (numSymResvDlCtrl));
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::NumSymReservedUlCtrl",
                      UintegerValue (numSymResvUlCtrl));

  Config::SetDefault ("ns3::MmWavePhyMacCommon::Numerology",
                      EnumValue(3));
  Config::SetDefault ("ns3::PhasedArrayModel::AntennaElement",
                      PointerValue (CreateObject<ThreeGppAntennaModel> ()));
  Config::SetDefault ("ns3::ThreeGppPropagationLossModel::ShadowingEnabled", BooleanValue (false));
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::NumSymResvForChildrenDu",
                      UintegerValue (numSymResvForOddIabs));
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::NumSymResvForDl",
                      UintegerValue (numSymResvForDl));



  // Enable additional APP-level headers to retrieve delay
  Config::SetDefault ("ns3::UdpClient::EnableSeqTsSizeHeader", BooleanValue (true));
  Config::SetDefault ("ns3::PacketSink::EnableSeqTsSizeHeader", BooleanValue (true));

  Config::SetDefault("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue (fc));
  Config::SetDefault("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue (bw));

  // Increase RLC buffers size
  Config::SetDefault ("ns3::LteRlcAm::MaxTxBufferSize", UintegerValue (rlcBufferSize));
  Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue (rlcBufferSize));

  Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper> ();

  Config::SetDefault ("ns3::TwoRayPropagationLossModel::RainRate", DoubleValue (rainRate));
  Config::SetDefault("ns3::TwoRayPropagationLossModel::Frequency", DoubleValue (fc));
  mmwaveHelper->SetPathlossModelType ("ns3::TwoRayPropagationLossModel");
  mmwaveHelper->SetChannelConditionModelType ("ns3::AlwaysLosChannelConditionModel");

  Config::SetDefault ("ns3::MmWaveEnbPhy::TxPower", DoubleValue (powerTx));
  Config::SetDefault ("ns3::MmWaveUePhy::TxPower", DoubleValue (powerTx));

  Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
  mmwaveHelper->SetEpcHelper (epcHelper);
  mmwaveHelper->SetHarqEnabled (true);

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
  iabPositionAlloc->Add (Vector (-600*scenScale, 375*scenScale, 5.0));
  // iabPositionAlloc->Add (Vector (-375*scenScale, 600*scenScale, 5.0));
  // Layer 2
  iabPositionAlloc->Add (Vector (-975*scenScale, 675*scenScale, 5.0));
  // iabPositionAlloc->Add (Vector (-525*scenScale, 875*scenScale, 5.0));

  iabmobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  iabmobility.SetPositionAllocator (iabPositionAlloc);
  iabmobility.Install (iabNodes);

  Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
  enbPositionAlloc->Add (Vector (0.0, 0.0, 5.0));
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.SetPositionAllocator (enbPositionAlloc);
  enbMobility.Install (enbNode);

  // Install Devices to the nodes
  mmwaveHelper->SetEnbPhasedArrayModelAttribute("BearingAngle", DoubleValue (M_PI / 2));
  NetDeviceContainer iabMmWaveDev = mmwaveHelper->InstallIabDevice (iabNodes);
  NetDeviceContainer enbMmWaveDev = mmwaveHelper->InstallEnbDevice (enbNode);

  unsigned int ccId = 0;
  for (unsigned int i = 0; i < iabNodes.GetN (); i++)
    {
      Ptr<MmWaveIabNetDevice> iabDevTest = DynamicCast<MmWaveIabNetDevice> (iabMmWaveDev.Get (i));
      // NS_LOG_UNCOND("IAB device " << i << " has " << " BAP ADDRESS " << iabDevTest->GetBap ()->GetLocalAddress ()
      //                             << " IMSI " << iabDevTest->GetImsi());
      iabDevTest->GetMtAntenna (ccId)->SetAttribute ("BearingAngle", DoubleValue (-M_PI / 2));
      iabDevTest->GetDuAntenna (ccId)->SetAttribute ("BearingAngle", DoubleValue (+M_PI / 2));
    }
  // NS_LOG_UNCOND("DONOR device " << " has BAP ADDRESS " << 
  //               enbMmWaveDev.Get (0)->GetObject<MmWaveEnbNetDevice> ()->GetBap ()->GetLocalAddress ());

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
  // mmwaveHelper->AttachIabTotDonorWithIndex (iabMmWaveDev.Get (1), enbMmWaveDev, 0);
  mmwaveHelper->AttachIabTotIabWithIndex (iabMmWaveDev, 1, 0, enbMmWaveDev, 0);
  // mmwaveHelper->AttachIabTotIabWithIndex (iabMmWaveDev, 3, 1, enbMmWaveDev, 0);

  // Base ports
  uint16_t dlDataSensorPort = 1135;
//   uint16_t ulDataSensorPort = 1235; // base port for the UL data stream from the sensors
  AsciiTraceHelper asciiTraceHelper;

  // Install DL data applications on the 2nd layer IAB node
  AppSetupParams dlAppParams;
  dlAppParams.basePort = dlDataSensorPort;
  dlAppParams.startTime = MilliSeconds (appStartMs);
  dlAppParams.endTime = Seconds (simTimeSeconds); // Some cooldown time
  dlAppParams.IPI = MilliSeconds (ipi);
  dlAppParams.rate = DataRate (dataSensorRate);
  dlAppParams.simTime = Seconds (simTimeSeconds);
  dlAppParams.sendSize = 512;
  Ptr<OutputStreamWrapper> dlNodeAppStream =
      asciiTraceHelper.CreateFileStream ("DL-app-trace-node.txt");
  *dlNodeAppStream->GetStream () << "rx_tx time packet_size port_to port_from delay\n";

  InstallUdpApplication (dlNodeAppStream, remoteHost, iabIpIface.GetAddress (1),
                      dlAppParams);
  InstallUdpPacketSink (dlNodeAppStream, iabIpIface.GetAddress (1), iabNodes.Get (1),
                    dlAppParams);
  dlAppParams.basePort++;

  DataRate ulDataSensorRate = DataRate (dataSensorRate);
  ulDataSensorRate = ulDataSensorRate * 0.2;

   // Sanity check: packets must be bigger than the SeqTs header
  SeqTsSizeHeader header;
  auto packetSize = PacketByteSizeFromBitrateAndIpi(DataRate (ulDataSensorRate), MilliSeconds (ipi));
  NS_ABORT_MSG_IF (packetSize < header.GetSerializedSize () + 1, "The packet size is too small: either increase IPI or data rate");

//   // Install UL data applications on the IAB nodes
//   AppSetupParams ulAppParams;
//   ulAppParams.basePort = ulDataSensorPort;
//   ulAppParams.startTime = MilliSeconds (appStartMs);
//   ulAppParams.endTime = Seconds (simTimeSeconds); // Some cooldown time
//   ulAppParams.IPI = MicroSeconds (ipi);
//   ulAppParams.rate = DataRate (ulDataSensorRate);
//   ulAppParams.simTime = Seconds (simTimeSeconds);
//   ulAppParams.sendSize = 512;
//   Ptr<OutputStreamWrapper> ulNodeAppStream =
//       asciiTraceHelper.CreateFileStream ("UL-app-trace-node.txt");
//   *ulNodeAppStream->GetStream () << "rx_tx time packet_size port_to port_from delay\n";

//   for (unsigned int i = 0; i < iabNodes.GetN (); i++)
//     {
//       InstallUdpApplication (ulNodeAppStream, iabNodes.Get (i), internetIpIfaces.GetAddress (1),
//                              ulAppParams);
//       InstallUdpPacketSink (ulNodeAppStream, internetIpIfaces.GetAddress (1), remoteHost,
//                             ulAppParams);
//       ulAppParams.basePort++;
//     }

  mmwaveHelper->EnableTraces ();

  // Trace the IAB topology
  auto donorApp = enbNode.Get (0)->GetApplication (0)->GetObject<EpcEnbApplication> ();
  Ptr<OutputStreamWrapper> iabTopologyStream =
      asciiTraceHelper.CreateFileStream ("IAB-topology.txt");

  Ptr<OutputStreamWrapper> scenTopologyStream =
      asciiTraceHelper.CreateFileStream ("scenario-topology.txt");

  Simulator::Schedule (Seconds (simTimeSeconds - 0.01), &CheckAppReceptionCallback);

  Simulator::Stop (Seconds (simTimeSeconds));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
