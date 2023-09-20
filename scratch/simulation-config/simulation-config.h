#include <ns3/core-module.h>
#include <ns3/internet-module.h>
#include <ns3/point-to-point-helper.h>
#include <ns3/mobility-module.h>
#include <ns3/applications-module.h>
#include <ns3/buildings-module.h>
#include <ns3/node-list.h>
#include <ns3/lte-module.h>
#include <ns3/mmwave-module.h>
#include <ns3/trace-source-accessor.h>
#include <ns3/mmwave-beamforming-model.h>

using namespace ns3;
using namespace mmwave;

enum AppType {
  FTP_APP = 0, // FTP application
  UDP_APP = 1, // UDP application
  TCP_APP = 2 // TCP application
};

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

class Utils
{
public:
  static std::pair<Ptr<Node>, Ipv4InterfaceContainer>
  CreateInternet (Ptr<MmWavePointToPointEpcHelper> epcHelper);
  static Ipv4InterfaceContainer InstallUeInternet (Ptr<MmWavePointToPointEpcHelper> epcHelper,
                                                   NodeContainer ueNodes,
                                                   NetDeviceContainer ueNetDevices);

  static void InstallUdpApplication (Ptr<OutputStreamWrapper> traceStream, Ptr<Node> srcNode,
                                     Ipv4Address targetAddress, AppSetupParams params);
  static void InstallUdpPacketSink (Ptr<OutputStreamWrapper> traceStream, Ipv4Address srcAddress,
                                    Ptr<Node> targetNode, AppSetupParams params);

  static void SetTracesPath (std::string filePath);
  static double PacketByteSizeFromBitrateAndIpi (DataRate rate, Time IPI);
};

class CallbackSinks
{
public:
  static void RxSinkHeader (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet,
                            const Address &from, const Address &to, const SeqTsSizeHeader &seqTs);
  static void TxSinkHeader (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet,
                            const Address &from, const Address &to, const SeqTsSizeHeader &seqTs);
};

std::pair<Ptr<Node>, Ipv4InterfaceContainer>
Utils::CreateInternet (Ptr<MmWavePointToPointEpcHelper> epcHelper)
{
  // Create the Internet by connecting remoteHost to pgw. Setup routing too
  Ptr<Node> pgw = epcHelper->GetPgwNode ();

  // Create remotehost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ipv4InterfaceContainer internetIpIfaces;

  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.001)));

  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.255.0.0");
  internetIpIfaces = ipv4h.Assign (internetDevices);
  // interface 0 is localhost, 1 is the p2p device

  Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
      ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.255.0.0"), 1);

  return std::pair<Ptr<Node>, Ipv4InterfaceContainer> (remoteHost, internetIpIfaces);
}

Ipv4InterfaceContainer
Utils::InstallUeInternet (Ptr<MmWavePointToPointEpcHelper> epcHelper, NodeContainer ueNodes,
                          NetDeviceContainer ueNetDevices)
{
  // Install the IP stack on the UEs
  InternetStackHelper internet;
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (ueNetDevices);
  // Assign IP address to UEs, and install applications
  // Set the default gateway for the UE
  Ipv4StaticRoutingHelper ipv4RoutingHelper;

  for (uint32_t i = 0; i < ueNodes.GetN (); i++)
    {
      Ptr<Ipv4StaticRouting> ueStaticRouting =
          ipv4RoutingHelper.GetStaticRouting (ueNodes.Get (i)->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  return ueIpIface;
}

void
Utils::InstallUdpApplication (Ptr<OutputStreamWrapper> traceStream, Ptr<Node> srcNode,
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
  app.Get (0)->TraceConnectWithoutContext (
      "TxWithSeqTsSize", MakeBoundCallback (&CallbackSinks::TxSinkHeader, traceStream));
  app.Start (params.startTime);
  app.Stop (params.endTime);
}

void
Utils::InstallUdpPacketSink (Ptr<OutputStreamWrapper> traceStream, Ipv4Address srcAddress,
                             Ptr<Node> targetNode, AppSetupParams params)
{
  ApplicationContainer app;
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory",
                                     InetSocketAddress (srcAddress, params.basePort));
  app.Add (packetSinkHelper.Install (targetNode));
  app.Get (0)->TraceConnectWithoutContext (
      "RxWithSeqTsSize", MakeBoundCallback (&CallbackSinks::RxSinkHeader, traceStream));
  app.Start (Seconds (0));
  app.Stop (params.simTime);
}

double
Utils::PacketByteSizeFromBitrateAndIpi (DataRate rate, Time IPI)
{
  double size = rate.GetBitRate () * IPI.GetSeconds ();
  return size / 8;
}

void
CallbackSinks::RxSinkHeader (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet,
                             const Address &from, const Address &to, const SeqTsSizeHeader &seqTs)
{
  *stream->GetStream () << "Rx\t" << Simulator::Now ().GetNanoSeconds () << "\t" << seqTs.GetSize ()
                        << "\t" << (Simulator::Now () - seqTs.GetTs ()).GetNanoSeconds () << "\t"
                        << InetSocketAddress::ConvertFrom (to).GetPort () << std::endl;
}

void
CallbackSinks::TxSinkHeader (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet,
                             const Address &from, const Address &to, const SeqTsSizeHeader &seqTs)
{
  *stream->GetStream () << "Tx\t" << Simulator::Now ().GetNanoSeconds () << "\t" << seqTs.GetSize ()
                        << "\t" << (Simulator::Now () - seqTs.GetTs ()).GetNanoSeconds () << "\t"
                        << InetSocketAddress::ConvertFrom (to).GetPort () << std::endl;
}
