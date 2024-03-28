/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 * Copyright (c) 2016, University of Padova, Dep. of Information Engineering, SIGNET lab
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
 * Author: Jaume Nin <jnin@cttc.cat>
 *         Nicola Baldo <nbaldo@cttc.cat>
 *
 * Modified by Michele Polese <michele.polese@gmail.com>
 *     (support for RRC_CONNECTED->RRC_IDLE state transition + support for real S1AP link)
 */

#include "epc-enb-application.h"
#include "ns3/log.h"
#include "ns3/mac48-address.h"
#include "ns3/ipv4.h"
#include "ns3/inet-socket-address.h"
#include "ns3/uinteger.h"
#include <ns3/mmwave-net-device.h>
#include <ns3/mmwave-enb-net-device.h>
#include <ns3/mmwave-iab-net-device.h>
#include "epc-gtpu-header.h"
#include "eps-bearer-tag.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("EpcEnbApplication");

EpcEnbApplication::EpsFlowId_t::EpsFlowId_t ()
{
}

EpcEnbApplication::EpsFlowId_t::EpsFlowId_t (const uint16_t a, const uint32_t b)
    : m_rnti (a), m_bid (b)
{
}

bool
operator== (const EpcEnbApplication::EpsFlowId_t &a, const EpcEnbApplication::EpsFlowId_t &b)
{
  return ((a.m_rnti == b.m_rnti) && (a.m_bid == b.m_bid));
}

bool
operator<(const EpcEnbApplication::EpsFlowId_t &a, const EpcEnbApplication::EpsFlowId_t &b)
{
  return ((a.m_rnti < b.m_rnti) || ((a.m_rnti == b.m_rnti) && (a.m_bid < b.m_bid)));
}

TypeId
EpcEnbApplication::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::EpcEnbApplication")
          .SetParent<Object> ()
          .SetGroupName ("Lte")
          .AddTraceSource ("RxFromEnb", "Receive data packets from LTE Enb Net Device",
                           MakeTraceSourceAccessor (&EpcEnbApplication::m_rxLteSocketPktTrace),
                           "ns3::EpcEnbApplication::RxTracedCallback")
          .AddTraceSource ("RxFromS1u", "Receive data packets from S1-U Net Device",
                           MakeTraceSourceAccessor (&EpcEnbApplication::m_rxS1uSocketPktTrace),
                           "ns3::EpcEnbApplication::RxTracedCallback");
  return tid;
}

void
EpcEnbApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_lteSocket = 0;
  m_lteSocket6 = 0;
  m_s1uSocket = 0;
  delete m_s1SapProvider;
  delete m_s1apSapEnb;
  m_iabTopology.clear ();
  m_bapPathIdPathMap.clear ();
}

EpcEnbApplication::EpcEnbApplication (Ptr<Socket> lteSocket, Ptr<Socket> lteSocket6,
                                      Ptr<Socket> s1uSocket, Ipv4Address enbS1uAddress,
                                      Ipv4Address sgwS1uAddress, uint16_t cellId)
    : m_lteSocket (lteSocket),
      m_lteSocket6 (lteSocket6),
      m_s1uSocket (s1uSocket),
      m_enbS1uAddress (enbS1uAddress),
      m_sgwS1uAddress (sgwS1uAddress),
      m_gtpuUdpPort (2152), // fixed by the standard
      m_s1SapUser (0),
      m_s1apSapEnbProvider (0),
      m_cellId (cellId)
{
  NS_LOG_FUNCTION (this << lteSocket << s1uSocket << sgwS1uAddress);
  m_s1uSocket->SetRecvCallback (MakeCallback (&EpcEnbApplication::RecvFromS1uSocket, this));
  m_lteSocket->SetRecvCallback (MakeCallback (&EpcEnbApplication::RecvFromLteSocket, this));
  m_lteSocket6->SetRecvCallback (MakeCallback (&EpcEnbApplication::RecvFromLteSocket, this));
  m_s1SapProvider = new MemberEpcEnbS1SapProvider<EpcEnbApplication> (this);
  m_s1apSapEnb = new MemberEpcS1apSapEnb<EpcEnbApplication> (this);
}

EpcEnbApplication::~EpcEnbApplication (void)
{
  NS_LOG_FUNCTION (this);
}

void
EpcEnbApplication::SetDonor (Ptr<NetDevice> donorDev)
{
  NS_LOG_FUNCTION (this);

  auto mmwDonorDev = DynamicCast<mmwave::MmWaveNetDevice> (donorDev);
  if (mmwDonorDev)
    {
      m_iabTopology.push_back (std::make_pair (mmwDonorDev, 0));
    }
}

void
EpcEnbApplication::SetS1SapUser (EpcEnbS1SapUser *s)
{
  m_s1SapUser = s;
}

EpcEnbS1SapProvider *
EpcEnbApplication::GetS1SapProvider ()
{
  return m_s1SapProvider;
}

void
EpcEnbApplication::SetS1apSapMme (EpcS1apSapEnbProvider *s)
{
  m_s1apSapEnbProvider = s;
}

EpcS1apSapEnb *
EpcEnbApplication::GetS1apSapEnb ()
{
  return m_s1apSapEnb;
}

void
EpcEnbApplication::DoInitialUeMessage (uint64_t imsi, uint16_t rnti)
{
  NS_LOG_FUNCTION (this);
  // side effect: create entry if not exist
  NS_LOG_DEBUG ("Creating info for IMSI " << imsi << " and RNTI " << rnti << " at the donor");
  m_imsiRntiMap[imsi] = rnti;
  m_s1apSapEnbProvider->SendInitialUeMessage (
      imsi, rnti, imsi, m_cellId); // TODO if more than one MME is used, extend this call
}

void
EpcEnbApplication::DoPathSwitchRequest (EpcEnbS1SapProvider::PathSwitchRequestParameters params)
{
  NS_LOG_FUNCTION (this);
  uint16_t enbUeS1Id = params.rnti;
  uint64_t mmeUeS1Id = params.mmeUeS1Id;
  uint64_t imsi = mmeUeS1Id;
  // side effect: create entry if not exist

  NS_LOG_DEBUG ("Updating info for IMSI " << imsi << " and RNTI " << params.rnti);
  m_imsiRntiMap[imsi] = params.rnti;

  uint16_t gci = params.cellId;
  std::list<EpcS1apSapMme::ErabSwitchedInDownlinkItem> erabToBeSwitchedInDownlinkList;
  for (std::list<EpcEnbS1SapProvider::BearerToBeSwitched>::iterator bit =
           params.bearersToBeSwitched.begin ();
       bit != params.bearersToBeSwitched.end (); ++bit)
    {
      EpsFlowId_t flowId;
      flowId.m_rnti = params.rnti;
      flowId.m_bid = bit->epsBearerId;

      EpsFlowId_t rbid (params.rnti, bit->epsBearerId);
      // side effect: create entries if not exist
      m_rbidTeidMap[params.rnti][bit->epsBearerId] = bit->epsBearerId;
      m_teidRbidMap[bit->epsBearerId] = rbid;

      EpcS1apSapMme::ErabSwitchedInDownlinkItem erab;
      erab.erabId = bit->epsBearerId;
      erab.enbTransportLayerAddress = m_enbS1uAddress;
      erab.enbTeid = bit->teid;

      erabToBeSwitchedInDownlinkList.push_back (erab);
    }
  m_s1apSapEnbProvider->SendPathSwitchRequest (enbUeS1Id, mmeUeS1Id, gci,
                                               erabToBeSwitchedInDownlinkList);
}

void
EpcEnbApplication::DoUeContextRelease (uint16_t rnti)
{
  NS_LOG_FUNCTION (this << rnti);
  auto rntiIt = m_rbidTeidMap.find (rnti);
  if (rntiIt != m_rbidTeidMap.end ())
    {
      for (auto bidIt = rntiIt->second.begin (); bidIt != rntiIt->second.end (); ++bidIt)
        {
          uint32_t teid = bidIt->second;
          m_teidRbidMap.erase (teid);
        }
      m_rbidTeidMap.erase (rntiIt);
    }
}

void
EpcEnbApplication::DoInitialContextSetupRequest (
    uint64_t mmeUeS1Id, uint16_t enbUeS1Id,
    std::list<EpcS1apSapEnb::ErabToBeSetupItem> erabToBeSetupList)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("In EnpEnbApplication DoInitialContextSetupRequest size of the erabToBeSetupList is "
               << erabToBeSetupList.size ());

  for (std::list<EpcS1apSapEnb::ErabToBeSetupItem>::iterator erabIt = erabToBeSetupList.begin ();
       erabIt != erabToBeSetupList.end (); ++erabIt)
    {
      // request the RRC to setup a radio bearer
      NS_LOG_DEBUG("From EpcEnbApplication add bearer " << erabIt->erabId
                    << " teid " << erabIt->sgwTeid << " for IMSI " << mmeUeS1Id);
      uint64_t imsi = mmeUeS1Id;
      std::map<uint64_t, uint16_t>::iterator imsiIt = m_imsiRntiMap.find (imsi);
      NS_ASSERT_MSG (imsiIt != m_imsiRntiMap.end (), "unknown IMSI");
      uint16_t rnti = imsiIt->second;

      struct EpcEnbS1SapUser::DataRadioBearerSetupRequestParameters params;
      params.rnti = rnti;
      params.bearer = erabIt->erabLevelQosParameters;
      params.bearerId = erabIt->erabId;
      params.gtpTeid = erabIt->sgwTeid;
      m_s1SapUser->DataRadioBearerSetupRequest (params);

      EpsFlowId_t rbid (rnti, erabIt->erabId);
      // side effect: create entries if not exist
      m_rbidTeidMap[rnti][erabIt->erabId] = erabIt->erabId;
      m_teidRbidMap[erabIt->erabId] = rbid;
    }
}

void
EpcEnbApplication::DoPathSwitchRequestAcknowledge (
    uint64_t enbUeS1Id, uint64_t mmeUeS1Id, uint16_t gci,
    std::list<EpcS1apSapEnb::ErabSwitchedInUplinkItem> erabToBeSwitchedInUplinkList)
{
  NS_LOG_FUNCTION (this);

  uint64_t imsi = mmeUeS1Id;
  std::map<uint64_t, uint16_t>::iterator imsiIt = m_imsiRntiMap.find (imsi);
  NS_ASSERT_MSG (imsiIt != m_imsiRntiMap.end (), "unknown IMSI");
  uint16_t rnti = imsiIt->second;
  EpcEnbS1SapUser::PathSwitchRequestAcknowledgeParameters params;
  params.rnti = rnti;
  m_s1SapUser->PathSwitchRequestAcknowledge (params);
}

void
EpcEnbApplication::RecvFromLteSocket (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this);
  if (m_lteSocket6)
    {
      NS_ASSERT (socket == m_lteSocket || socket == m_lteSocket6);
    }
  else
    {
      NS_ASSERT (socket == m_lteSocket);
    }
  Ptr<Packet> packet = socket->Recv ();

  /// \internal
  /// Workaround for \bugid{231}
  //SocketAddressTag satag;
  //packet->RemovePacketTag (satag);

  EpsBearerTag tag;
  bool found = packet->RemovePacketTag (tag);
  NS_ASSERT (found);
  uint16_t rnti = tag.GetRnti ();
  uint32_t bid = tag.GetBid ();
  NS_LOG_LOGIC ("received packet with RNTI=" << (uint32_t) rnti << ", BID=" << (uint32_t) bid);
  //std::cstd::cout << rnti  << " "  << (uint16_t) bid << "\n";
  auto rntiIt = m_rbidTeidMap.find (rnti);
  if (rntiIt == m_rbidTeidMap.end ())
    {
      NS_LOG_WARN ("UE context not found, discarding packet when receiving from lteSocket");
    }
  else
    {
      auto bidIt = rntiIt->second.find (bid);
      NS_ASSERT (bidIt != rntiIt->second.end ());
      uint32_t teid = bidIt->second;
      m_rxLteSocketPktTrace (packet->Copy ());
      SendToS1uSocket (packet, teid);
    }
}

void
EpcEnbApplication::RecvFromS1uSocket (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_ASSERT (socket == m_s1uSocket);
  Ptr<Packet> packet = socket->Recv ();
  GtpuHeader gtpu;
  packet->RemoveHeader (gtpu);
  uint32_t teid = gtpu.GetTeid ();

  /// \internal
  /// Workaround for \bugid{231}
  //SocketAddressTag tag;
  //packet->RemovePacketTag (tag);

  std::map<uint32_t, EpsFlowId_t>::iterator it = m_teidRbidMap.find (teid);

  std::optional<uint16_t> bapAddrTerminatingIabNode = std::nullopt; 
  if (!m_getBapAddressWhereBearerTerminatesCallback.IsNull())
    {
      bapAddrTerminatingIabNode = 
        m_getBapAddressWhereBearerTerminatesCallback(m_cellId, teid);
    }

  if (bapAddrTerminatingIabNode.has_value()) // Non-PDU traffic whose bearer terminates either 
                                               // at this (if donor) or at a child IAB-node
    {
      NS_LOG_DEBUG("Forwarding to BAP, dst address " << bapAddrTerminatingIabNode.value());
      auto donorDevice = DynamicCast<mmwave::MmWaveEnbNetDevice> (GetNode()->GetDevice(0));
      donorDevice->GetBap ()-> TransmitBapSduViaNonPduInterface (packet, bapAddrTerminatingIabNode.value());
    }
  else if (it != m_teidRbidMap.end()) // PDU-traffic
    {
      NS_LOG_DEBUG("Forwarding to LTE socket");
      m_rxS1uSocketPktTrace(packet->Copy());
      SendToLteSocket(packet, it->second.m_rnti, it->second.m_bid);
    }
  else
    {
      NS_LOG_WARN ("UE context not found, discarding packet when receiving from s1uSocket");
      packet = 0;
    }
}

void
EpcEnbApplication::SendToLteSocket (Ptr<Packet> packet, uint16_t rnti, uint32_t bid)
{
  NS_LOG_FUNCTION (this << packet << rnti << bid << packet->GetSize ());
  //std::cout << rnti  << " "  << (uint16_t) bid << "\n";
  EpsBearerTag tag (rnti, bid);
  packet->AddPacketTag (tag);
  uint8_t ipType;

  packet->CopyData (&ipType, 1);
  ipType = (ipType >> 4) & 0x0f;

  int sentBytes;
  if (ipType == 0x04)
    {
      sentBytes = m_lteSocket->Send (packet);
    }
  else if (ipType == 0x06)
    {
      sentBytes = m_lteSocket6->Send (packet);
    }
  else
    {
      NS_ABORT_MSG ("EpcEnbApplication::SendToLteSocket - Unknown IP type...");
    }

  NS_ASSERT (sentBytes > 0);
}

void
EpcEnbApplication::SendToS1uSocket (Ptr<Packet> packet, uint32_t teid)
{
  NS_LOG_FUNCTION (this << packet << teid << packet->GetSize ());
  GtpuHeader gtpu;
  gtpu.SetTeid (teid);
  // From 3GPP TS 29.281 v10.0.0 Section 5.1
  // Length of the payload + the non obligatory GTP-U header
  gtpu.SetLength (packet->GetSize () + gtpu.GetSerializedSize () - 8);
  packet->AddHeader (gtpu);
  uint32_t flags = 0;
  m_s1uSocket->SendTo (packet, flags, InetSocketAddress (m_sgwS1uAddress, m_gtpuUdpPort));
}

void
EpcEnbApplication::DoReleaseIndication (uint64_t imsi, uint16_t rnti, uint32_t bearerId)
{
  NS_LOG_FUNCTION (this << bearerId);
  std::list<EpcS1apSapMme::ErabToBeReleasedIndication> erabToBeReleaseIndication;
  EpcS1apSapMme::ErabToBeReleasedIndication erab;
  erab.erabId = bearerId;
  erabToBeReleaseIndication.push_back (erab);

  //From 3GPP TS 23401-950 Section 5.4.4.2, enB sends EPS bearer Identity in Bearer Release Indication message to MME
  m_s1apSapEnbProvider->SendErabReleaseIndication (imsi, rnti, erabToBeReleaseIndication);
}

unsigned int
EpcEnbApplication::RegisterNewIabAttachment (uint16_t bapPathIdCounter,
                                             Ptr<mmwave::MmWaveNetDevice> iabDev,
                                             Ptr<mmwave::MmWaveNetDevice> parentDev)
{
  auto iabNetDev = DynamicCast<mmwave::MmWaveIabNetDevice> (iabDev);
  NS_ABORT_MSG_IF (!iabNetDev,
                   "RegisterNewIabAttachment called from a device different than IAB node.");
  NS_ABORT_MSG_IF (iabNetDev->GetBap ()->GetLocalAddress () == 0,
                   "Set the local address of the IAB device first.");

  auto parentIsDonor = DynamicCast<mmwave::MmWaveEnbNetDevice> (parentDev);
  bool found = false;

  // Check if the parent is this donor
  if (parentIsDonor)
    {
      m_iabTopology.push_back (std::make_pair (iabDev, 0));
      found = true;
    }
  else
    {
      for (uint32_t index = 0; index < m_iabTopology.size (); index++)
        {
          if (parentDev == m_iabTopology.at (index).first)
            {
              m_iabTopology.push_back (std::make_pair (iabDev, index));
              found = true;
              break;
            }
        }
    }

  NS_ABORT_MSG_IF (!found, "The specified parent could not be found");

  // Store the newly added path to the corresponding map. For now, only single route
  // from donor to the IAB node, and vice versa.
  // PATH ID of this new route is bapPathIdCounter
  unsigned int numAddedPaths = 0;
  std::list<uint16_t> newBapPath{iabNetDev->GetBap ()->GetLocalAddress ()};
  uint32_t parentIdx = m_iabTopology.back ().second; // Newly added element is at the back
  //TODO: Refactor and make a method to find the parent ?
  while (parentIdx != 0)
    {
      // retrieve the parent
      parentDev = m_iabTopology.at (parentIdx).first;
      parentIdx = m_iabTopology.at (parentIdx).second;

      // retrieve its BAP address
      auto iabParentDev = DynamicCast<mmwave::MmWaveIabNetDevice> (parentDev);
      newBapPath.push_back (iabParentDev->GetBap ()->GetLocalAddress ());
    }
  // Add the BAP of the donor
  auto donorEnbDev = DynamicCast<mmwave::MmWaveEnbNetDevice> (m_iabTopology.at (0).first);
  newBapPath.push_back (donorEnbDev->GetBap ()->GetLocalAddress ());
  // Store this path in the related map
  NS_ABORT_MSG_IF (m_bapPathIdPathMap.find (bapPathIdCounter) != m_bapPathIdPathMap.end (),
                   "The BAT PATH IDs must be unique");
  m_bapPathIdPathMap[bapPathIdCounter] = newBapPath;
  numAddedPaths++;

  return numAddedPaths;
}

std::pair<uint16_t, Mode>
EpcEnbApplication::GetNexBapPathHop (uint16_t localBapAddress, uint16_t destBapAddress,
                                     uint16_t bapPathId) const
{
  NS_LOG_FUNCTION (this);

  NS_ABORT_MSG_IF (m_bapPathIdPathMap.find (bapPathId) == m_bapPathIdPathMap.end (),
                   "The specified BAP PATH ID can not be found");
  NS_ABORT_MSG_IF (localBapAddress == destBapAddress, "Already at the final hop");

  auto &path = m_bapPathIdPathMap.find (bapPathId)->second;
  bool isDonorDest = (path.back () == destBapAddress);
  bool isNodeDest = (path.front () == destBapAddress);
  Mode mode = isDonorDest ? Mode::UL : Mode::DL;
  NS_ABORT_MSG_IF (!isDonorDest && !isNodeDest, "Wrong destination address and PATH ID pair");

  uint16_t nextHop = 0;
  auto itCurrentHop = std::find (path.begin (), path.end (), localBapAddress);
  if (isDonorDest)
    {
      nextHop = *(++itCurrentHop);
      //TODO: should never happen...?
      if (nextHop == localBapAddress)
        {
          return std::make_pair (destBapAddress, mode);
        }
    }
  else if (isNodeDest)
    {
      nextHop = *(--itCurrentHop);
    }

  return std::make_pair (nextHop, mode);
}

uint32_t
EpcEnbApplication::GetIabNodeDepth (uint16_t iabCellId) const
{
  NS_LOG_FUNCTION (this);

  Ptr<mmwave::MmWaveEnbNetDevice> donor =
      DynamicCast<mmwave::MmWaveEnbNetDevice> (m_iabTopology.at (0).first);
  if (iabCellId == donor->GetCellId ())
    {
      return 0;
    }

  uint32_t depthCounter = 0;
  uint32_t parentDevIndex = UINT32_MAX;

  while (parentDevIndex != 0)
    {
      for (const auto &it : m_iabTopology)
        {
          Ptr<mmwave::MmWaveIabNetDevice> iabDevice =
              DynamicCast<mmwave::MmWaveIabNetDevice> (it.first);
          if (iabDevice && iabCellId == iabDevice->GetCellId ())
            {
              parentDevIndex = it.second;
              depthCounter++;
              if (parentDevIndex != 0) // Parent node is not a Donor
                {
                  auto iabParentDevice = DynamicCast<mmwave::MmWaveIabNetDevice> (
                      m_iabTopology.at (parentDevIndex).first);
                  NS_ABORT_MSG_IF (!iabParentDevice, "Parent device is not an IAB node");
                  iabCellId = iabParentDevice->GetCellId ();
                }
              continue;
            }
        }
    }
  return depthCounter;
}

uint32_t 
EpcEnbApplication::GetParentInfoFromChildCellId (uint16_t childCellId, uint16_t &parentCellId) const
{
  NS_LOG_FUNCTION (this);

  Ptr<mmwave::MmWaveEnbNetDevice> donor =
      DynamicCast<mmwave::MmWaveEnbNetDevice> (m_iabTopology.at (0).first);
  if (childCellId == donor->GetCellId ())
    {
      NS_FATAL_ERROR ("This method should be used with IAB nodes only");
    }

  // Default case: donor is the parent
  parentCellId = donor->GetCellId ();
  uint32_t parentImsi = 0;
  uint32_t parentDevIndex = UINT32_MAX;

  for (const auto &it : m_iabTopology)
    {
      Ptr<mmwave::MmWaveIabNetDevice> iabDevice =
          DynamicCast<mmwave::MmWaveIabNetDevice> (it.first);
      if (iabDevice && childCellId == iabDevice->GetCellId ())
        {
          parentDevIndex = it.second;
          if (parentDevIndex != 0) // Parent node is not a Donor
            {
              auto iabParentDevice =
                  DynamicCast<mmwave::MmWaveIabNetDevice> (m_iabTopology.at (parentDevIndex).first);
              NS_ABORT_MSG_IF (!iabParentDevice, "Parent device is not an IAB node");
              parentCellId = iabParentDevice->GetCellId ();
              parentImsi = iabParentDevice->GetImsi ();
            }
          continue;
        }
    }
    
  return parentImsi;
}

uint16_t
EpcEnbApplication::GetPathId (uint16_t localBapAddress, uint16_t destBapAddress) const
{
  NS_LOG_FUNCTION (this);
  for (const auto &it : m_bapPathIdPathMap)
    {
      auto list = it.second;
      if ((list.front () == localBapAddress && list.back () == destBapAddress) ||
          (list.front () == destBapAddress && list.back () == localBapAddress))
        {
          return it.first;
        }
    }
  NS_ABORT_MSG ("Path with given local address " << localBapAddress << " and destination address "
                                                 << destBapAddress << " not found");
}

uint16_t
EpcEnbApplication::GetIabNodeImsiFromPathIdAndOtherEndpoint (uint16_t endpointBapAddress,
                                                             uint16_t pathId) const
{
  NS_LOG_FUNCTION (this);
  auto pathIdIt = m_bapPathIdPathMap.find (pathId);
  NS_ABORT_MSG_IF (pathIdIt == m_bapPathIdPathMap.end (), "given BAP PATH ID not found");

  auto list = pathIdIt->second;
  auto IabNodeBapAddress = (list.front () == endpointBapAddress) ? list.back () : list.front ();

  for (const auto &it : m_iabTopology)
    {
      Ptr<mmwave::MmWaveIabNetDevice> iabDevice =
          DynamicCast<mmwave::MmWaveIabNetDevice> (it.first);
      if (iabDevice && iabDevice->GetBap ()->GetLocalAddress () == IabNodeBapAddress)
        {
          return iabDevice->GetImsi ();
        }
    }

  NS_ABORT_MSG ("The IMSI related to the specified IDs can not be found");
}

std::optional<uint16_t>
EpcEnbApplication::GetBapAddressFromImsi (uint64_t imsi) const
{
  NS_LOG_FUNCTION (this);
  for (const auto &it : m_iabTopology)
    {
      auto iabDevice = DynamicCast<mmwave::MmWaveIabNetDevice> (it.first);
      if (iabDevice // Skip the donor as we are just looking at IAB nodes for DL bearers
          && iabDevice->GetImsi () == imsi)
        {
          return std::optional<uint16_t> (iabDevice->GetBap ()->GetLocalAddress ());
        }
    }

  // We should be dealing with a UE. Neglect
  return std::nullopt;
}

uint16_t
EpcEnbApplication::GetBapAddressFromCellId (uint16_t cellId) const
{
  NS_LOG_FUNCTION (this);
  for (const auto &it : m_iabTopology)
    {
      auto iabDevice = DynamicCast<mmwave::MmWaveIabNetDevice> (it.first);
      auto enbDevice = DynamicCast<mmwave::MmWaveEnbNetDevice> (it.first);
      if (iabDevice && iabDevice->GetCellId () == cellId)
        {
          return iabDevice->GetBap ()->GetLocalAddress ();
        }
      else if (enbDevice && enbDevice->GetCellId () == cellId)
        {
          return enbDevice->GetBap ()->GetLocalAddress ();
        }
    }
  NS_ABORT_MSG ("Cell ID not found");
}

void 
EpcEnbApplication::SetBapAddressWhereBearerTerminatesCallback (
  Callback<std::optional<uint16_t>, uint16_t, uint64_t> cb)
{
  NS_ASSERT(!cb.IsNull());
  m_getBapAddressWhereBearerTerminatesCallback = cb;
}

} // namespace ns3
