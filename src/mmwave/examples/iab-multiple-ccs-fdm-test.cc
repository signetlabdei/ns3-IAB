/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/applications-module.h"
#include "ns3/config-store.h"
#include "ns3/core-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/isotropic-antenna-model.h"
#include "ns3/log.h"
#include "ns3/mmwave-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include <ns3/buildings-helper.h>
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/point-to-point-helper.h"

#include <map>

using namespace ns3;
using namespace mmwave;

/* In this example a single UE is connected with a single MmWave BS. The UE is
 * placed at distance ueDist from the BS and it does not move. The system
 * bandwidth is fixed at 1GHz. If CA is enabled, 2 CCs are used and each of them
 * uses half of the total bandwidth.
 */

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

unsigned int
PacketByteSizeFromBitrateAndIpi (DataRate rate, Time IPI)
{
  double size = rate.GetBitRate () * IPI.GetSeconds ();
  return std::ceil (size / 8);
}

static void
RxSinkHeader (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &from,
              const Address &to)
{
  *stream->GetStream () << "Rx " << Simulator::Now ().GetNanoSeconds () << " "
                        << packet->GetSize () << " "
                        << InetSocketAddress::ConvertFrom (to).GetPort () << " "
                        << InetSocketAddress::ConvertFrom (from).GetPort () << " "
                        << "0" << "\n";
}

static void
TxSinkHeader (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &from,
              const Address &to)
{
  *stream->GetStream () << "Tx " << Simulator::Now ().GetNanoSeconds () << " "
                        << packet->GetSize () << " "
                        << InetSocketAddress::ConvertFrom (to).GetPort () << " "
                        << InetSocketAddress::ConvertFrom (from).GetPort () << " "
                        << "0" << "\n";
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
  app.Get (0)->TraceConnectWithoutContext ("TxWithAddresses",
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
  app.Get (0)->TraceConnectWithoutContext ("RxWithAddresses",
                                           MakeBoundCallback (RxSinkHeader, traceStream));
  app.Start (Seconds (0));
  app.Stop (params.simTime);
}

int
main(int argc, char* argv[])
{
    double ueDist = 200.0;
    double iabInterDist = 500.0;
    bool blockage = false;
    double totalBandwidth = 200e6;
    double frequency0 = 28e9;
    double frequency1 = 29e9;
    double simTime = 1;
    double channelUpdatePeriodMs = 10.0;
    bool useTwoGnbsAndCcs = true;

    CommandLine cmd;
    cmd.AddValue("channelUpdatePeriodMs", "The update period of the channel [ms]", 
                 channelUpdatePeriodMs);
    cmd.AddValue("fc0", "CC 0 central frequency", frequency0);
    cmd.AddValue("fc1", "CC 1 central frequency", frequency1);
    cmd.AddValue("bandwidth", "System bandwidth in Hz", totalBandwidth);
    cmd.Parse(argc, argv);

    //LogComponentEnableAll(LOG_PREFIX_NODE);
    //LogComponentEnableAll(LOG_PREFIX_TIME);

    // CC 0
    // create the CC map
    Ptr<MmWavePhyMacCommon> phyMacConfig0 = CreateObject<MmWavePhyMacCommon>();
    phyMacConfig0->SetBandwidth(totalBandwidth / 2);
    phyMacConfig0->SetCentreFrequency(frequency0);
    Ptr<MmWaveComponentCarrier> cc0 = CreateObject<MmWaveComponentCarrier>();
    cc0->SetConfigurationParameters(phyMacConfig0);
    cc0->SetAsPrimary(true);

    Ptr<MmWaveComponentCarrier> cc1 = CreateObject<MmWaveComponentCarrier>();
    Ptr<MmWavePhyMacCommon> phyMacConfig1 = CreateObject<MmWavePhyMacCommon>();
    std::map<uint8_t, MmWaveComponentCarrier> ccMap;
    ccMap[0] = *cc0;

    // CC 1
    phyMacConfig1->SetBandwidth(totalBandwidth / 2);
    phyMacConfig1->SetCentreFrequency(frequency1);
    phyMacConfig1->SetCcId(1); 
    cc1->SetConfigurationParameters(phyMacConfig1);
    cc1->SetAsPrimary(true);
    ccMap[1] = *cc1;

    // print CC parameters
    for (uint8_t i = 0; i < ccMap.size(); i++)
    {
        std::cout << "Component Carrier "
                  << (uint32_t)(ccMap[i].GetConfigurationParameters()->GetCcId()) << " frequency : "
                  << ccMap[i].GetConfigurationParameters()->GetCenterFrequency() / 1e9 << " GHz,"
                  << " bandwidth : " << totalBandwidth / 2 / 1e6 << " MHz," << std::endl;
    }

    // create and set the helper
    Config::SetDefault("ns3::MmWaveHelper::UseMultiplePrimaryCarriers", BooleanValue (useTwoGnbsAndCcs));
    Config::SetDefault("ns3::MmWaveHelper::ChannelModel",
                       StringValue("ns3::ThreeGppSpectrumPropagationLossModel"));
    Config::SetDefault("ns3::ThreeGppChannelModel::Scenario", StringValue("UMa"));
    Config::SetDefault("ns3::ThreeGppChannelModel::Blockage",
                       BooleanValue(blockage)); // Enable/disable the blockage model
    Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod", 
                       TimeValue(MilliSeconds(channelUpdatePeriodMs)));
    Config::SetDefault("ns3::MmWaveHelper::PathlossModel",
                       StringValue("ns3::ThreeGppUmaPropagationLossModel"));
    Config::SetDefault("ns3::ThreeGppPropagationLossModel::ShadowingEnabled",
                       BooleanValue(false));

    // by default, isotropic antennas are used. To use the 3GPP radiation pattern instead, use the
    // <ThreeGppAntennaArrayModel> beware: proper configuration of the bearing and downtilt angles
    // is needed
    Config::SetDefault("ns3::PhasedArrayModel::AntennaElement",
                       PointerValue(CreateObject<IsotropicAntennaModel>()));

    Ptr<MmWaveHelper> helper = CreateObject<MmWaveHelper>();
    helper->SetCcPhyParams(ccMap);
    helper->SetChannelConditionModelType("ns3::AlwaysLosChannelConditionModel");
    Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
    helper->SetEpcHelper (epcHelper);
    helper->Initialize();

    // create the enb nodes
    NodeContainer enbNode;
    enbNode.Create (1);

    // create the iab nodes
    NodeContainer iabNodes;
    iabNodes.Create(2);

    // set mobility
    Ptr<ListPositionAllocator> iabPositionAlloc = CreateObject<ListPositionAllocator>();
    iabPositionAlloc->Add(Vector(iabInterDist, 0.0, 15.0));
    iabPositionAlloc->Add(Vector(2*iabInterDist, 0.0, 15.0));

    MobilityHelper iabmobility;
    iabmobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    iabmobility.SetPositionAllocator(iabPositionAlloc);
    iabmobility.Install(iabNodes);

    
    // create the struct to set the enb device configuration
    MmWaveHelper::IabMtDuCcIdInfo iab1CcIdInfo = {0, 1};
    MmWaveHelper::IabMtDuCcIdInfo iab2CcIdInfo = {1, 0};

    auto iabDev1 = helper->InstallSingleIabDevice(iabNodes.Get(0), iab1CcIdInfo);
    auto iabDev2 = helper->InstallSingleIabDevice(iabNodes.Get(1), iab2CcIdInfo);
    NetDeviceContainer iabNetDevices (iabDev1, iabDev2);

    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
    enbPositionAlloc->Add (Vector (0.0, 0.0, 15.0));
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator (enbPositionAlloc);
    enbMobility.Install (enbNode);

    NetDeviceContainer enbMmWaveDev = helper->InstallSingleEnbDevice (enbNode.Get(0), std::optional<uint8_t> (1));

    std::cout << "IAB devices installed" << std::endl;

    // create ue nodes
    NodeContainer ueNodes;
    uint8_t numUes = 1;
    ueNodes.Create(numUes);
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

    for (unsigned int i = 0; i < iabNodes.GetN (); i++)
    {
      Ptr<Ipv4StaticRouting> iabStaticRouting =
          ipv4RoutingHelper.GetStaticRouting (iabNodes.Get (i)->GetObject<Ipv4> ());
      iabStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

    // set mobility
    std::cout << "Distance : " << (uint32_t)ueDist << " m" << std::endl;
    MobilityHelper uemobility;
    uemobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator>();
    uePositionAlloc->Add(Vector(iabInterDist, ueDist, 1.6));
    uemobility.SetPositionAllocator(uePositionAlloc);
    uemobility.Install(ueNodes);
    BuildingsHelper::Install(ueNodes);

    // install ue device
    auto ueDev1 = helper->InstallSingleUeDevice(ueNodes.Get(0), std::optional<uint8_t> (1));
    NetDeviceContainer ueNetDevices (ueDev1);

    std::cout << "UE devices installed" << std::endl;

    // Taken from IAB script
    internet.Install (ueNodes);
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address (ueNetDevices);
    for (uint32_t u = 0; u < ueNetDevices.GetN (); ++u)
        {
        Ptr<Node> ueNode = ueNodes.Get (u);
        // Set the default gateway for the UE
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
        ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
        }

    helper->AttachIabTotDonorWithIndex (iabDev1, enbMmWaveDev, 0);
    helper->AttachIabTotIabWithIndex (iabNetDevices, 1, 0, enbMmWaveDev, 0);
    helper->AttachToClosestIab (ueNetDevices, iabDev2);

    // Base ports
    uint16_t dlDataSensorPort = 1235; // base port for the DL data stream from the sensors
    AsciiTraceHelper asciiTraceHelper;

    // Install DL data applications on the IAB node
    AppSetupParams dlAppParams;
    dlAppParams.basePort = dlDataSensorPort;
    dlAppParams.startTime = MilliSeconds (100);
    dlAppParams.endTime = Seconds (simTime); // Some cooldown time
    dlAppParams.IPI = MicroSeconds (200);
    dlAppParams.rate = DataRate ("5Mbps");
    dlAppParams.simTime = Seconds (simTime);
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

    helper->EnableTraces();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
