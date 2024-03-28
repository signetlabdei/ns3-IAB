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

#include <ns3/log.h>
#include <ns3/simulator.h>
#include <ns3/ipv4-header.h>
#include <ns3/ipv4-address.h>
#include <ns3/eps-bearer-tag.h>
#include "bap.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (Bap);
NS_LOG_COMPONENT_DEFINE ("Bap");

/// BapRlcSapUser class
class BapRlcSapUser : public LteRlcSapUser
{
public:
  /**
   * Constructor
   *
   * \param bap The instance of the BAP layer which provides the RLC SAP user functionality
   */
  BapRlcSapUser (Bap *bap);

  // Interface provided to lower RLC entity (implemented from LteRlcSapUser)
  virtual void ReceivePdcpPdu (Ptr<Packet> p);

private:
  BapRlcSapUser ();
  Bap *m_bap; ///< the instance of the BAP layer which provides the RLC SAP user functionality
};

BapRlcSapUser::BapRlcSapUser (Bap *bap) : m_bap (bap)
{
}

BapRlcSapUser::BapRlcSapUser ()
{
}

void
BapRlcSapUser::ReceivePdcpPdu (Ptr<Packet> p)
{
  m_bap->DoReceiveBapPdu (p);
}

Bap::Bap () : m_isDonor (false), m_localAddress (0), m_imsi (0)
{
  NS_LOG_FUNCTION (this);
  m_rlcSapUser = new BapRlcSapUser (this);
}

Bap::~Bap ()
{
  NS_LOG_FUNCTION (this);
  m_bapAddressRlcMap.clear ();
}

TypeId
Bap::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::Bap")
          .SetParent<Object> ()
          .SetGroupName ("MmWave")
          .AddTraceSource ("TxPacket", "PDU transmission notified by the BAP to the RLC.",
                           MakeTraceSourceAccessor (&Bap::m_txPdu), "ns3::Bap::PduTxTracedCallback")
          .AddTraceSource ("RxPacket", "PDU received at the BAP.",
                           MakeTraceSourceAccessor (&Bap::m_rxPdu),
                           "ns3::Bap::PduRxTracedCallback");
  return tid;
}

void
Bap::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  delete (m_rlcSapUser);
}

void
Bap::SetLocalAddress (uint16_t address)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (address <= 0x3FF);

  m_localAddress = address;
}

uint16_t
Bap::GetLocalAddress () const
{
  return m_localAddress;
}

void
Bap::SetDonor ()
{
  NS_LOG_FUNCTION (this);
  m_isDonor = true;
}

bool
Bap::IsDonor () const
{
  NS_LOG_FUNCTION (this);
  return m_isDonor;
}

LteRlcSapUser *
Bap::GetLteRlcSapUser ()
{
  NS_LOG_FUNCTION (this);
  return m_rlcSapUser;
}

void
Bap::SetNextHopCallback (Callback<std::pair<uint16_t, Mode>, uint16_t, uint16_t, uint16_t> cb)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (!cb.IsNull ());

  m_nextHopAddressCallback = cb;
}

void
Bap::SetPathIdCallback (Callback<uint16_t, uint16_t, uint16_t> cb)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (!cb.IsNull ());

  m_pathIdCallback = cb;
}

void
Bap::SetDonorBapAddressCallback (Callback<uint16_t, uint64_t> cb)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (!cb.IsNull ());

  m_donorBapAddressCallback = cb;
}

void
Bap::SetThisIabNodeImsi (uint64_t imsi)
{
  NS_LOG_FUNCTION (this);

  m_imsi = imsi;
}

void
Bap::DoTransmitBapSdu (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this);
  uint16_t destBapAddress = m_donorBapAddressCallback (m_imsi);

  BapHeader bapHeader;
  bapHeader.SetDestinationAddress (destBapAddress);

  uint16_t pathId = m_pathIdCallback (m_localAddress, destBapAddress);
  bapHeader.SetPathId (pathId);
  bapHeader.SetDcField (BapHeader::BapDcField::Data);

  NS_LOG_DEBUG ("At BAP with local address " << m_localAddress << 
                " Tx BAP SDU with header: " << bapHeader << 
                " and size (before adding header): " << p->GetSize ());
  
  p->AddHeader (bapHeader);
  m_txPdu (bapHeader, m_localAddress); // signal the TX of a BAP PDU
  DoTransmitBapPdu (p, destBapAddress, pathId);
  // uint16_t nextBapAddress = m_nextHopAddressCallback (m_localAddress, destBapAddress, pathId).first;
  // LteRlcSapProvider *rlcSapProvider = m_bapAddressRlcMap.find (nextBapAddress)->second;

  // rlcSapProvider->TransmitBapPdu (p);
}

void
Bap::DoTransmitBapPdu (Ptr<Packet> p, uint16_t destBapAddress, uint16_t pathId)
{
  NS_LOG_FUNCTION (this);

  uint16_t nextHopAddress;
  Mode mode;

  if (m_epcEnbApplication) // If this is a Donor
  {
    std::tie(nextHopAddress, mode) = m_epcEnbApplication->GetNexBapPathHop (m_localAddress, destBapAddress, pathId);
  }
  else
  {
    NS_ASSERT (!m_nextHopAddressCallback.IsNull ());
    std::tie(nextHopAddress, mode) = m_nextHopAddressCallback (m_localAddress, destBapAddress, pathId);
  }


  NS_ASSERT (m_bapAddressRlcMap.find (nextHopAddress) !=
             m_bapAddressRlcMap.end ());
  LteRlcSapProvider *rlcSapProvider = m_bapAddressRlcMap.find (nextHopAddress)->second;

  BapHeader bapHeader;
  p->PeekHeader (bapHeader);

  NS_LOG_DEBUG ("At BAP with local address " << m_localAddress << 
                " Tx BAP PDU with header: " << bapHeader <<
                " address next hop: " << nextHopAddress <<
                " and size (with header): " << p->GetSize ());
  NS_LOG_DEBUG ("Tx.ing to RLC SAP provider: " << rlcSapProvider);
  rlcSapProvider->TransmitBapPdu (p);
}

void
Bap::DoReceiveBapSdu (Ptr<Packet> p, bool isFromNonPduInterface, BapHeader header)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("At BAP with local address " << m_localAddress << 
                " Rx BAP SDU of size (without header): " << p->GetSize ());

  if (!isFromNonPduInterface)
    {
      // Forward to the inner PDCP to remove the IP, UDP and GTP headers
      m_innerPdcp->DoReceiveInnerPdcpPdu (p);
    }
    else
    {
      if (m_isDonor)
      {
          // Retrieve the IMSI of the sending IAB node
          auto imsi =
              m_epcEnbApplication->GetIabNodeImsiFromPathIdAndOtherEndpoint(m_localAddress,
                                                                            header.GetPathId());

          // Default to default DRB for now
          uint32_t teid = imsi * 11 + 2; // TODO: retrieve this using TFT classifiers

          // Forwards to the S1-U interface
          m_epcEnbApplication->SendToS1uSocket(p, teid);
      }
      else
      {
          m_iabMtRrc->DoReceiveBapSdu(p);
      }
    }
}

void
Bap::DoReceiveBapPdu (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this);
  BapHeader bapHeader;
  p->RemoveHeader (bapHeader);

  NS_LOG_DEBUG ("At BAP with local address " << m_localAddress << 
                   " RX BAP PDU with header: " << bapHeader <<
                   " and size (without header): " << p->GetSize ());
  m_rxPdu (bapHeader, m_localAddress); // signal the RX of a BAP PDU

  // Check whether the BAP PDU has reached its destination within the IAB network
  uint16_t destAddress = bapHeader.GetDestinationAddress ();
  uint16_t pathId = bapHeader.GetPathId ();
  if (destAddress != m_localAddress)
    {
      p->AddHeader (bapHeader);
      DoTransmitBapPdu (p, destAddress, pathId);
    }
  else
    {
      uint8_t reservedBits = bapHeader.GetReservedBits ();
      bool isFromNonPduInterface =
          ((reservedBits >> 2) & 1); // Check the most significant bit

      // Forward the BAP SDU to the outgoing BAP interface
      DoReceiveBapSdu (p, isFromNonPduInterface, bapHeader);
    }
}

void
Bap::SetInnerPdcp (Ptr<InnerPdcp> innerPdcp)
{
  NS_LOG_FUNCTION (this);
  m_innerPdcp = innerPdcp;
}

void
Bap::TransmitBapSduViaNonPduInterface (Ptr<Packet> p, uint16_t destBapAddress)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG("Trasmitting packet via non-SDU interface to BAP " << destBapAddress);

  uint16_t pathId;

  if (m_epcEnbApplication) // If this is a Donor
  {
    pathId = m_epcEnbApplication->GetPathId (m_localAddress, destBapAddress);
    NS_LOG_DEBUG("At the donor, retrieved path ID " << pathId);
  }
  else
  {
    destBapAddress = m_donorBapAddressCallback (m_imsi);
    pathId = m_pathIdCallback (m_localAddress, destBapAddress);
    NS_LOG_DEBUG("At an IAB node, retrieved path ID " << pathId);
  }

  BapHeader bapHeader;
  bapHeader.SetDestinationAddress (destBapAddress);

  bapHeader.SetPathId (pathId);
  bapHeader.SetDcField (BapHeader::BapDcField::Data);
  bapHeader.SetReservedBits (1, // Non PDU Session bit flag
                             0, 0); // Unused reserved bits

  p->AddHeader (bapHeader);
  m_txPdu (bapHeader, m_localAddress); // signal the TX of a BAP PDU
  DoTransmitBapPdu (p, destBapAddress, pathId);
}

void
Bap::AddPairRlcMap (uint16_t destAddress, LteRlcSapProvider *rlcSapProvider)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT (m_bapAddressRlcMap.find (destAddress) == 
             m_bapAddressRlcMap.end ());

  NS_LOG_DEBUG ("At BAP with address " << m_localAddress 
                << " stored RLC MAP entry " << rlcSapProvider
                << " for address: "  << destAddress);
  m_bapAddressRlcMap.insert ({destAddress, rlcSapProvider});
}

void
Bap::SetEpcEnbApplication (Ptr<EpcEnbApplication> app)
{
  NS_LOG_FUNCTION (this);
  m_epcEnbApplication = app;
}

void
Bap::SetMtRrc (Ptr<LteUeRrc> mtRrc)
{
  NS_LOG_FUNCTION(this);
  m_iabMtRrc = mtRrc;
}
} // namespace ns3
