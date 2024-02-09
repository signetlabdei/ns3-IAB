/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
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
#include <vector>

using namespace ns3;
using namespace mmwave;

double simTimeSeconds = 0.2;
bool useIdealRrc = false;
double ipi = 50.0;
double appStartMs = 100.0;
unsigned int numSymResvForOddIabs = 0;
unsigned int numSymResvCtrl = 2238;
double rainRate = 0;
std::string dlDataSensorRate = "50Mbps";
std::string slotFormat = "dddsu";
double powerTx = 26;
double bw = 400e6;
double fc = 26e9;
unsigned int rlcBufferSize = 25 * 1024 * 1024;
std::string cbDir = "/home/rstudio/inmarsat-iab-releases/src/mmwave/model/Codebooks/";
std::string nRowsCols = "8,8";
// node positions string i.e. 'simname,x,y,z' note without spaces between comma!
std::string nodePosition = "test,3695.51813,1530.733729,10"; 

NS_LOG_COMPONENT_DEFINE ("viasat_uniform_grid");

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
  // Command line arguments
  CommandLine cmd;
  cmd.AddValue ("bw", "the system bandwidth", bw);
  cmd.AddValue ("fc", "the system carrier frequency", fc);
  cmd.AddValue ("nRowsCols", "UE antenna array number of rows, number of columns seperated by comma e.g. '4,6'", nRowsCols);
  cmd.AddValue ("useIdealRrc", "Whether to us ea simplified RRC model", useIdealRrc);
  cmd.AddValue ("numSymResvForOddIabs", "How many symbols to reserve for odd layer IAB nodes", numSymResvForOddIabs);
  cmd.AddValue ("numSymResvCtrl", "How many symbols to reserve for control messages", numSymResvCtrl);
  cmd.AddValue ("simTime", "The simulation time [s]", simTimeSeconds);
  cmd.AddValue ("ipi", "The inter-packet interval time [ms]", ipi);
  cmd.AddValue ("appStartMs", "The application start time [ms]", appStartMs);
  cmd.AddValue ("dataSensorRate", "The downlink data rate of the applications", dlDataSensorRate);
  cmd.AddValue ("rainRate", "The rain rate for the propagation model [mm/h]", rainRate);
  cmd.AddValue ("slotFormat", "The pattern of the slots format (d/u/s)", slotFormat);
  cmd.AddValue ("rlcBufferSize", "The pattern of the slots format (d/u/s)", rlcBufferSize);
  cmd.AddValue ("powerTx", "The transmission power [dBm]", powerTx);
  cmd.AddValue ("nodePosition", "Vector consisting of 'sim name' and x,y,z position of nodes [m]", nodePosition);
  cmd.Parse (argc, argv);
  
  Config::SetDefault ("ns3::MmWaveHelper::UseIdealRrc", BooleanValue (useIdealRrc));
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::SlotsFormat", StringValue(slotFormat));
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::NumSymReservedCtrl", UintegerValue (numSymResvCtrl));
  Config::SetDefault ("ns3::MmWavePhyMacCommon::Numerology", EnumValue(3));

  Config::SetDefault ("ns3::TwoRayPropagationLossModel::RainRate", DoubleValue (rainRate));
  Config::SetDefault ("ns3::TwoRayPropagationLossModel::Frequency", DoubleValue (fc));

  Config::SetDefault ("ns3::PhasedArrayModel::AntennaElement", PointerValue (CreateObject<ThreeGppAntennaModel> ()));
  Config::SetDefault ("ns3::ThreeGppPropagationLossModel::ShadowingEnabled", BooleanValue (true));
  Config::SetDefault ("ns3::MmWaveFlexTtiMacScheduler::NumSymResvForChildrenDu", UintegerValue (numSymResvForOddIabs));

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
  
  // extract antenna array count nrows and ncolumns
  std::stringstream nRowsColsStream;
  nRowsColsStream << nRowsCols;
  std::string segment2;
  std::vector<std::string> values;
  
  while(std::getline(nRowsColsStream, segment2, ','))
  {
    values.push_back(segment2);
  }
  
  std::string cbPath = cbDir + values[0] + "x" + values[1] + ".txt";
  
  mmwaveHelper->SetUePhasedArrayModelAttribute ("NumColumns", UintegerValue (std::stoi(values[1])));
  mmwaveHelper->SetUePhasedArrayModelAttribute ("NumRows", UintegerValue (std::stoi(values[0])));
  
  mmwaveHelper->SetEnbPhasedArrayModelAttribute ("NumColumns", UintegerValue (std::stoi(values[1])));
  mmwaveHelper->SetEnbPhasedArrayModelAttribute ("NumRows", UintegerValue (std::stoi(values[0])));
  
  mmwaveHelper->SetUeBeamformingCodebookAttribute ("CodebookFilename", StringValue (cbPath));
  mmwaveHelper->SetEnbBeamformingCodebookAttribute ("CodebookFilename", StringValue (cbPath));
  
  // extract name,x,y,z from nodePosition string
  std::stringstream nodePositionsStream;
  nodePositionsStream << nodePosition;
  std::string segment;
  std::vector<std::string> namexyz;
  
  while(std::getline(nodePositionsStream, segment, ','))
  {
    namexyz.push_back(segment);
  }
  
  uint32_t numUe = 1;

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
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("50Gb/s")));
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

  uePositionAlloc->Add(Vector(std::stoi(namexyz[1]),std::stoi(namexyz[2]),std::stoi(namexyz[3])));

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
