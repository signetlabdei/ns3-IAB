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
 * Modified by: Michele Polese <michele.polese@gmail.com>
 *          Dual Connectivity functionalities
 */

#include "epc-sgw-pgw-application.h"
#include "ns3/log.h"
#include "ns3/mac48-address.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv6.h"
#include "ns3/ipv6-header.h"
#include "ns3/ipv6-l3-protocol.h"
#include "ns3/inet-socket-address.h"
#include "ns3/epc-gtpu-header.h"
#include "ns3/abort.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("EpcSgwPgwApplication");

/////////////////////////
// UeInfo
/////////////////////////

EpcSgwPgwApplication::UeInfo::UeInfo ()
{
  NS_LOG_FUNCTION (this);
}

void
EpcSgwPgwApplication::UeInfo::AddBearer (Ptr<EpcTft> tft, uint32_t bearerId, uint32_t teid)
{
  NS_LOG_FUNCTION (this << tft << teid);
  m_teidByBearerIdMap[bearerId] = teid;

  return m_tftClassifier.Add (tft, bearerId);
}

void
EpcSgwPgwApplication::UeInfo::RemoveBearer (uint32_t bearerId)
{
  NS_LOG_FUNCTION (this << bearerId);
  m_teidByBearerIdMap.erase (bearerId);
}

uint32_t
EpcSgwPgwApplication::UeInfo::Classify (Ptr<Packet> p, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (this << p);
  // we hardcode DOWNLINK direction since the PGW is espected to
  // classify only downlink packets (uplink packets will go to the
  // internet without any classification).
  return m_tftClassifier.Classify (p, EpcTft::DOWNLINK, protocolNumber);
}

Ipv4Address
EpcSgwPgwApplication::UeInfo::GetServingDuAddress ()
{
  return m_servingDuAddr;
}

void
EpcSgwPgwApplication::UeInfo::SetServingDuAddress (Ipv4Address enbAddr)
{
  m_servingDuAddr = enbAddr;
}

Ipv4Address
EpcSgwPgwApplication::UeInfo::GetDonorAddress ()
{
  return m_donorAddr;
}

void
EpcSgwPgwApplication::UeInfo::SetDonorAddress (Ipv4Address donorAddr)
{
  // Since IAB handover is not currently supported, the donor Ipv4 address is updated with the
  // address of the serving DU in case of a path change request (handover). This requires the UE
  // to be directly connected to the donor during the handover.
  m_donorAddr = donorAddr;
}

uint16_t
EpcSgwPgwApplication::UeInfo::GetServingDuBapAddress () const
{
  return m_servingDuBapAddr;
}

void
EpcSgwPgwApplication::UeInfo::SetServingDuBapAddress (uint16_t bapAddr)
{
  // Since IAB handover is not currently supported, the donor BAP address is updated with the
  // address of the serving DU in case of a path change request (handover). This requires the UE
  // to be directly connected to the donor during the handover.
  m_servingDuBapAddr = bapAddr;
}

uint16_t
EpcSgwPgwApplication::UeInfo::GetDonorBapAddress () const
{
  return m_donorBapAddr;
}

void
EpcSgwPgwApplication::UeInfo::SetDonorBapAddress (uint16_t bapAddr)
{
  m_donorBapAddr = bapAddr;
}

Ipv4Address
EpcSgwPgwApplication::UeInfo::GetUeAddr ()
{
  return m_ueAddr;
}

void
EpcSgwPgwApplication::UeInfo::SetUeAddr (Ipv4Address ueAddr)
{
  m_ueAddr = ueAddr;
}

Ipv6Address
EpcSgwPgwApplication::UeInfo::GetUeAddr6 ()
{
  return m_ueAddr6;
}

void
EpcSgwPgwApplication::UeInfo::SetUeAddr6 (Ipv6Address ueAddr)
{
  m_ueAddr6 = ueAddr;
}

/////////////////////////
// EpcSgwPgwApplication
/////////////////////////

TypeId
EpcSgwPgwApplication::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::EpcSgwPgwApplication")
          .SetParent<Object> ()
          .SetGroupName ("Lte")
          .AddTraceSource ("RxFromTun", "Receive data packets from internet in Tunnel net device",
                           MakeTraceSourceAccessor (&EpcSgwPgwApplication::m_rxTunPktTrace),
                           "ns3::EpcSgwPgwApplication::RxTracedCallback")
          .AddTraceSource ("RxFromS1u", "Receive data packets from S1 U Socket",
                           MakeTraceSourceAccessor (&EpcSgwPgwApplication::m_rxS1uPktTrace),
                           "ns3::EpcSgwPgwApplication::RxTracedCallback");
  return tid;
}

void
EpcSgwPgwApplication::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_s1uSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
  m_s1uSocket = 0;
  delete (m_s11SapSgw);
}

EpcSgwPgwApplication::EpcSgwPgwApplication (const Ptr<VirtualNetDevice> tunDevice,
                                            const Ptr<Socket> s1uSocket)
    : m_s1uSocket (s1uSocket),
      m_tunDevice (tunDevice),
      m_gtpuUdpPort (2152), // fixed by the standard
      m_teidCount (0),
      m_s11SapMme (0)
{
  NS_LOG_FUNCTION (this << tunDevice << s1uSocket);
  m_s1uSocket->SetRecvCallback (MakeCallback (&EpcSgwPgwApplication::RecvFromS1uSocket, this));
  m_s11SapSgw = new MemberEpcS11SapSgw<EpcSgwPgwApplication> (this);
}

EpcSgwPgwApplication::~EpcSgwPgwApplication ()
{
  NS_LOG_FUNCTION (this);
}

bool
EpcSgwPgwApplication::RecvFromTunDevice (Ptr<Packet> packet, const Address &source,
                                         const Address &dest, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (this << source << dest << protocolNumber << packet << packet->GetSize ());
  m_rxTunPktTrace (packet->Copy ());
  Ptr<Packet> pCopy = packet->Copy ();

  // get IP address of UE
  if (protocolNumber == Ipv4L3Protocol::PROT_NUMBER)
    {
      Ipv4Header ipv4Header;
      pCopy->RemoveHeader (ipv4Header);
      Ipv4Address ueAddr = ipv4Header.GetDestination ();
      NS_LOG_LOGIC ("packet addressed to UE " << ueAddr);
      // find corresponding UeInfo address
      std::map<Ipv4Address, Ptr<UeInfo>>::iterator it = m_ueInfoByAddrMap.find (ueAddr);
      if (it == m_ueInfoByAddrMap.end ())
        {
          NS_LOG_WARN ("unknown UE address " << ueAddr);
        }
      else
        {
          Ipv4Address enbAddr = it->second->GetDonorAddress ();
          uint32_t teid = it->second->Classify (packet, protocolNumber);
          if (teid == 0)
            {
              NS_LOG_WARN ("no matching bearer for this packet");
            }
          else
            {
              SendToS1uSocket (packet, enbAddr, teid);
            }
        }
    }
  else if (protocolNumber == Ipv6L3Protocol::PROT_NUMBER)
    {
      Ipv6Header ipv6Header;
      pCopy->RemoveHeader (ipv6Header);
      Ipv6Address ueAddr = ipv6Header.GetDestination ();
      NS_LOG_LOGIC ("packet addressed to UE " << ueAddr);
      // find corresponding UeInfo address
      std::map<Ipv6Address, Ptr<UeInfo>>::iterator it = m_ueInfoByAddrMap6.find (ueAddr);
      if (it == m_ueInfoByAddrMap6.end ())
        {
          NS_LOG_WARN ("unknown UE address " << ueAddr);
        }
      else
        {
          Ipv4Address enbAddr = it->second->GetServingDuAddress ();
          uint32_t teid = it->second->Classify (packet, protocolNumber);
          if (teid == 0)
            {
              NS_LOG_WARN ("no matching bearer for this packet");
            }
          else
            {
              SendToS1uSocket (packet, enbAddr, teid);
            }
        }
    }
  else
    {
      NS_ABORT_MSG ("EpcSgwPgwApplication::RecvFromTunDevice - Unknown IP type...");
    }

  // there is no reason why we should notify the TUN
  // VirtualNetDevice that he failed to send the packet: if we receive
  // any bogus packet, it will just be silently discarded.
  const bool succeeded = true;
  return succeeded;
}

void
EpcSgwPgwApplication::RecvFromS1uSocket (Ptr<Socket> socket)
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

  SendToTunDevice (packet, teid);

  m_rxS1uPktTrace (packet->Copy ());
}

void
EpcSgwPgwApplication::SendToTunDevice (Ptr<Packet> packet, uint32_t teid)
{
  NS_LOG_FUNCTION (this << packet << teid);
  NS_LOG_LOGIC (" packet size: " << packet->GetSize () << " bytes");

  uint8_t ipType;
  packet->CopyData (&ipType, 1);
  ipType = (ipType >> 4) & 0x0f;

  if (ipType == 0x04)
    {
      m_tunDevice->Receive (packet, 0x0800, m_tunDevice->GetAddress (), m_tunDevice->GetAddress (),
                            NetDevice::PACKET_HOST);
    }
  else if (ipType == 0x06)
    {
      m_tunDevice->Receive (packet, 0x86DD, m_tunDevice->GetAddress (), m_tunDevice->GetAddress (),
                            NetDevice::PACKET_HOST);
    }
  else
    {
      NS_ABORT_MSG ("EpcSgwPgwApplication::SendToTunDevice - Unknown IP type...");
    }
}

void
EpcSgwPgwApplication::SendToS1uSocket (Ptr<Packet> packet, Ipv4Address enbAddr, uint32_t teid)
{
  NS_LOG_FUNCTION (this << packet << enbAddr << teid);
  GtpuHeader gtpu;
  gtpu.SetTeid (teid);
  // From 3GPP TS 29.281 v10.0.0 Section 5.1
  // Length of the payload + the non obligatory GTP-U header
  gtpu.SetLength (packet->GetSize () + gtpu.GetSerializedSize () - 8);
  packet->AddHeader (gtpu);
  uint32_t flags = 0;
  m_s1uSocket->SendTo (packet, flags, InetSocketAddress (enbAddr, m_gtpuUdpPort));
}

void
EpcSgwPgwApplication::SetS11SapMme (EpcS11SapMme *s)
{
  m_s11SapMme = s;
}

EpcS11SapSgw *
EpcSgwPgwApplication::GetS11SapSgw ()
{
  return m_s11SapSgw;
}

void
EpcSgwPgwApplication::AddEnb (uint16_t cellId, Ipv4Address enbAddr, Ipv4Address sgwAddr,
                              uint16_t bapAddr, bool isIabNode, uint64_t imsi)
{
  NS_LOG_FUNCTION (this << cellId << enbAddr << sgwAddr);
  NS_ASSERT((isIabNode && imsi != UINT64_MAX) ||
            (!isIabNode && imsi == UINT64_MAX));

  EnbInfo enbInfo;
  enbInfo.enbAddr = enbAddr;
  enbInfo.sgwAddr = sgwAddr;
  enbInfo.bapAddr = bapAddr;
  enbInfo.isIabNode = isIabNode;
  enbInfo.donorCellId = 0; // this will be once the attachment towards a donor has been performed
  enbInfo.imsi = imsi;

  m_enbInfoByCellId[cellId] = enbInfo;
}

void
EpcSgwPgwApplication::SetIabNodeDonorCellId (uint16_t iabNodeCellId, uint16_t donorCellId)
{
  auto enbInfoIt = m_enbInfoByCellId.find (iabNodeCellId);
  NS_ASSERT (enbInfoIt != m_enbInfoByCellId.end () && enbInfoIt->second.isIabNode);

  enbInfoIt->second.donorCellId = donorCellId;
}

void
EpcSgwPgwApplication::AddUe (uint64_t imsi)
{
  NS_LOG_FUNCTION (this << imsi);
  Ptr<UeInfo> ueInfo = Create<UeInfo> ();
  m_ueInfoByImsiMap[imsi] = ueInfo;
}

void
EpcSgwPgwApplication::SetUeAddress (uint64_t imsi, Ipv4Address ueAddr)
{
  NS_LOG_FUNCTION (this << imsi << ueAddr);
  std::map<uint64_t, Ptr<UeInfo>>::iterator ueit = m_ueInfoByImsiMap.find (imsi);
  NS_ASSERT_MSG (ueit != m_ueInfoByImsiMap.end (), "unknown IMSI " << imsi);
  m_ueInfoByAddrMap[ueAddr] = ueit->second;
  ueit->second->SetUeAddr (ueAddr);
}

void
EpcSgwPgwApplication::SetUeAddress6 (uint64_t imsi, Ipv6Address ueAddr)
{
  NS_LOG_FUNCTION (this << imsi << ueAddr);
  std::map<uint64_t, Ptr<UeInfo>>::iterator ueit = m_ueInfoByImsiMap.find (imsi);
  NS_ASSERT_MSG (ueit != m_ueInfoByImsiMap.end (), "unknown IMSI " << imsi);
  m_ueInfoByAddrMap6[ueAddr] = ueit->second;
  ueit->second->SetUeAddr6 (ueAddr);
}

void
EpcSgwPgwApplication::DoCreateSessionRequest (EpcS11SapSgw::CreateSessionRequestMessage req)
{
  NS_LOG_FUNCTION (this << req.imsi);
  std::map<uint64_t, Ptr<UeInfo>>::iterator ueit = m_ueInfoByImsiMap.find (req.imsi);
  NS_ASSERT_MSG (ueit != m_ueInfoByImsiMap.end (), "unknown IMSI " << req.imsi);
  uint16_t cellId = req.uli.gci;
  uint16_t cellIdPairEntry = cellId;
  auto enbit = m_enbInfoByCellId.find (cellId);
  NS_ASSERT_MSG (enbit != m_enbInfoByCellId.end (), "unknown CellId " << cellId);

  Ipv4Address servingDuAddr = enbit->second.enbAddr;
  uint16_t servingDuBapAddr = enbit->second.bapAddr;
  // if IAB-node, retrieve the IP address of the donor as well
  Ipv4Address donorAddr = servingDuAddr;
  uint16_t donorBapAddr = servingDuBapAddr;
  if (enbit->second.isIabNode && enbit->second.donorCellId != 0)
    {
      uint16_t donorCellId = enbit->second.donorCellId;
      cellIdPairEntry = donorCellId;
      enbit = m_enbInfoByCellId.find (donorCellId);
      NS_ASSERT_MSG (enbit != m_enbInfoByCellId.end (), "unknown donor CellId " << cellId);
      donorAddr = enbit->second.enbAddr;
      donorBapAddr = enbit->second.bapAddr;
    }
  // When donorCellId == 0 the IAB DU has no parent donor (standalone root).
  // donorAddr/donorBapAddr already equal servingDuAddr/servingDuBapAddr.

  ueit->second->SetServingDuAddress (servingDuAddr);
  ueit->second->SetServingDuBapAddress (servingDuBapAddr);
  ueit->second->SetDonorAddress (donorAddr);
  ueit->second->SetDonorBapAddress (donorBapAddr);

  EpcS11SapMme::CreateSessionResponseMessage res;
  res.teid = req.imsi; // trick to avoid the need for allocating TEIDs on the S11 interface

  for (std::list<EpcS11SapSgw::BearerContextToBeCreated>::iterator bit =
           req.bearerContextsToBeCreated.begin ();
       bit != req.bearerContextsToBeCreated.end (); ++bit)
    {
      // simple sanity check. If you ever need more than 4M teids
      // throughout your simulation, you'll need to implement a smarter teid
      // management algorithm.
      NS_ABORT_IF (m_teidCount == 0xFFFFFFFF);
      uint32_t teid = ++m_teidCount;
      ueit->second->AddBearer (bit->tft, bit->epsBearerId, teid);

      // Store bearerId -> imsi mapping so that RecvFromS1uSocket can identify IAB MT bearers.
      // The GTP TEID used by PGW is the bearerId (from Classify()), not the real SGW teid.
      m_terminatingBearerIdToImsiMap[std::make_pair(cellIdPairEntry, (uint64_t)bit->epsBearerId)] = req.imsi;
      EpcS11SapMme::BearerContextCreated bearerContext;
      bearerContext.sgwFteid.teid = teid;
      bearerContext.sgwFteid.address = enbit->second.sgwAddr;
      bearerContext.epsBearerId = bit->epsBearerId;
      bearerContext.bearerLevelQos = bit->bearerLevelQos;
      bearerContext.tft = bit->tft;
      res.bearerContextsCreated.push_back (bearerContext);
    }
  m_s11SapMme->CreateSessionResponse (res);
}

void
EpcSgwPgwApplication::DoModifyBearerRequest (EpcS11SapSgw::ModifyBearerRequestMessage req)
{
  NS_LOG_FUNCTION (this << req.teid);
  uint64_t imsi = req.teid; // trick to avoid the need for allocating TEIDs on the S11 interface
  std::map<uint64_t, Ptr<UeInfo>>::iterator ueit = m_ueInfoByImsiMap.find (imsi);
  NS_ASSERT_MSG (ueit != m_ueInfoByImsiMap.end (), "unknown IMSI " << imsi);
  uint16_t cellId = req.uli.gci;
  std::map<uint16_t, EnbInfo>::iterator enbit = m_enbInfoByCellId.find (cellId);
  NS_ASSERT_MSG (enbit != m_enbInfoByCellId.end (), "unknown CellId " << cellId);
  Ipv4Address enbAddr = enbit->second.enbAddr;
  uint16_t bapAddr = enbit->second.bapAddr;
  ueit->second->SetServingDuAddress (enbAddr);
  ueit->second->SetServingDuBapAddress (bapAddr);

  // Update the donor address. This assumes that the UE is connected directly to the donor in case of path switch request (handover).
  ueit->second->SetDonorAddress (enbAddr);
  ueit->second->SetDonorBapAddress (bapAddr);

  // no actual bearer modification: for now we just support the minimum needed for path switch request (handover)
  EpcS11SapMme::ModifyBearerResponseMessage res;
  res.teid = imsi; // trick to avoid the need for allocating TEIDs on the S11 interface
  res.cause = EpcS11SapMme::ModifyBearerResponseMessage::REQUEST_ACCEPTED;
  m_s11SapMme->ModifyBearerResponse (res);
}

void
EpcSgwPgwApplication::DoDeleteBearerCommand (EpcS11SapSgw::DeleteBearerCommandMessage req)
{
  NS_LOG_FUNCTION (this << req.teid);
  uint64_t imsi = req.teid; // trick to avoid the need for allocating TEIDs on the S11 interface
  std::map<uint64_t, Ptr<UeInfo>>::iterator ueit = m_ueInfoByImsiMap.find (imsi);
  NS_ASSERT_MSG (ueit != m_ueInfoByImsiMap.end (), "unknown IMSI " << imsi);

  EpcS11SapMme::DeleteBearerRequestMessage res;
  res.teid = imsi;

  for (std::list<EpcS11SapSgw::BearerContextToBeRemoved>::iterator bit =
           req.bearerContextsToBeRemoved.begin ();
       bit != req.bearerContextsToBeRemoved.end (); ++bit)
    {
      EpcS11SapMme::BearerContextRemoved bearerContext;
      bearerContext.epsBearerId = bit->epsBearerId;
      res.bearerContextsRemoved.push_back (bearerContext);
    }
  //schedules Delete Bearer Request towards MME
  m_s11SapMme->DeleteBearerRequest (res);
}

void
EpcSgwPgwApplication::DoDeleteBearerResponse (EpcS11SapSgw::DeleteBearerResponseMessage req)
{
  NS_LOG_FUNCTION (this << req.teid);
  uint64_t imsi = req.teid; // trick to avoid the need for allocating TEIDs on the S11 interface
  std::map<uint64_t, Ptr<UeInfo>>::iterator ueit = m_ueInfoByImsiMap.find (imsi);
  NS_ASSERT_MSG (ueit != m_ueInfoByImsiMap.end (), "unknown IMSI " << imsi);

  for (std::list<EpcS11SapSgw::BearerContextRemovedSgwPgw>::iterator bit =
           req.bearerContextsRemoved.begin ();
       bit != req.bearerContextsRemoved.end (); ++bit)
    {
      //Function to remove de-activated bearer contexts from S-Gw and P-Gw side
      ueit->second->RemoveBearer (bit->epsBearerId);
    }
}

Ipv4Address
EpcSgwPgwApplication::GetDonorIpAddress (uint64_t imsi) const
{
  NS_LOG_FUNCTION (this);
  Ptr<UeInfo> ueInfo = m_ueInfoByImsiMap.find (imsi)->second;

  return ueInfo->GetDonorAddress ();
}

uint16_t
EpcSgwPgwApplication::GetDonorBapAddress (uint64_t imsi) const
{
  NS_LOG_FUNCTION (this);
  Ptr<UeInfo> ueInfo = m_ueInfoByImsiMap.find (imsi)->second;

  return ueInfo->GetDonorBapAddress ();
}

std::optional<uint16_t>
EpcSgwPgwApplication::GetBapAddressWhereBearerTerminates (uint16_t cellid, uint64_t bid) const
{
  auto key = std::make_pair(cellid, bid);
  auto it = m_terminatingBearerIdToImsiMap.find(key);
  if (it == m_terminatingBearerIdToImsiMap.end())
    {
      return std::nullopt; // Regular PDU bearer — no IAB routing needed
    }

  // Find corresponding IMSI. This refers to the IMSI of the UE endpoint,
  // whenever this is PDU traffic, and to the terminating IAB node MT's IMSI
  // whenever it refers to non-PDU traffic.
  uint64_t imsiTerminatingNode = it->second;

  // Find corresponding BAP address, if it is indeed non-PDU traffic and 
  // the bearer terminates at an IAB node.
  for(auto enb : m_enbInfoByCellId)
  {
    if (enb.second.imsi == imsiTerminatingNode)
    {
      return enb.second.bapAddr;
    }
  }

  // PDU traffic
  return std::nullopt;
}

} // namespace ns3
