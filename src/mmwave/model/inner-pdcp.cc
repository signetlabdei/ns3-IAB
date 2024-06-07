/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011-2012 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 * Author: Manuel Requena <manuel.requena@cttc.es>
 */

#include <ns3/log.h>
#include <ns3/simulator.h>

#include <ns3/lte-pdcp.h>
#include <ns3/lte-pdcp-header.h>
#include <ns3/lte-pdcp-sap.h>
#include <ns3/lte-pdcp-tag.h>
#include "inner-pdcp.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (InnerPdcp);
NS_LOG_COMPONENT_DEFINE ("InnerPdcp");

/// BapRlcSapUser class
class InnerPdcpRlcSapUser : public LteRlcSapUser
{
public:
  /**
   * Constructor
   *
   * \param bap The instance of the BAP layer which provides the RLC SAP user functionality
   */
  InnerPdcpRlcSapUser (InnerPdcp *innerPdcp);

  // Interface provided to lower RLC entity (implemented from LteRlcSapUser)
  // virtual void ReceivePdcpSdu (Ptr<Packet> p);

  virtual void ReceivePdcpPdu (Ptr<Packet> p);

private:
  InnerPdcpRlcSapUser ();
  InnerPdcp
      *m_innerPdcp; ///< the instance of the BAP layer which provides the RLC SAP user functionality
};

InnerPdcpRlcSapUser::InnerPdcpRlcSapUser (InnerPdcp *innerPdcp) : m_innerPdcp (innerPdcp)
{
}

InnerPdcpRlcSapUser::InnerPdcpRlcSapUser ()
{
}

// void
// InnerPdcpRlcSapUser::ReceivePdcpSdu (Ptr<Packet> p)
// {
//   m_innerPdcp->DoReceiveInnerPdcpSdu (p);
// }

void
InnerPdcpRlcSapUser::ReceivePdcpPdu (Ptr<Packet> p)
{
  m_innerPdcp->DoReceiveInnerPdcpSdu (p);
}

InnerPdcp::InnerPdcp () : m_rnti (0), m_lcid (0), m_bap (nullptr)
{
  NS_LOG_FUNCTION (this);
  m_rlcSapUser = new InnerPdcpRlcSapUser (this);
}

InnerPdcp::~InnerPdcp ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
InnerPdcp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::InnerPdcp").SetParent<Object> ().SetGroupName ("MmWave");
  //   .AddTraceSource ("TxPDU", "PDU transmission notified to the RLC.",
  //                    MakeTraceSourceAccessor (&LtePdcp::m_txPdu),
  //                    "ns3::LtePdcp::PduTxTracedCallback")
  //   .AddTraceSource ("RxPDU", "PDU received.", MakeTraceSourceAccessor (&LtePdcp::m_rxPdu),
  //                    "ns3::LtePdcp::PduRxTracedCallback");
  return tid;
}

void
InnerPdcp::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  delete (m_rlcSapUser);
}

// void
// InnerPdcp::SetRnti (uint16_t rnti)
// {
//   NS_LOG_FUNCTION (this << (uint32_t) rnti);
//   m_rnti = rnti;
// }

// void
// InnerPdcp::SetLcId (uint8_t lcId)
// {
//   NS_LOG_FUNCTION (this << (uint32_t) lcId);
//   m_lcid = lcId;
// }

// void
// InnerPdcp::SetHeaders (Ipv4Header ipHeader, UdpHeader udpHeader)
// {
//   NS_LOG_FUNCTION (this);
//   m_ipv4Header = ipHeader;
//   m_udpHeader = udpHeader;
// }

// void
// InnerPdcp::SetHeaders (Ipv6Header ipHeader, UdpHeader udpHeader)
// {
//   NS_LOG_FUNCTION (this);
//   m_ipv6Header = ipHeader;
//   m_udpHeader = udpHeader;
// }

// void
// InnerPdcp::SetIpHeader (Ipv4Header ipHeader)
// {
//   NS_LOG_FUNCTION (this);
//   m_ipv4Header = ipHeader;
// }

// void
// InnerPdcp::SetIpHeader (Ipv6Header ipHeader)
// {
//   NS_LOG_FUNCTION (this);
//   m_ipv6Header = ipHeader;
// }

// void
// InnerPdcp::SetUdpHeader (UdpHeader udpHeader)
// {
//   NS_LOG_FUNCTION (this);
//   m_udpHeader = udpHeader;
// }

// void
// InnerPdcp::SetGtpHeader (GtpuHeader gtpHeader)
// {
//   NS_LOG_FUNCTION (this);
//   m_gtpHeader = gtpHeader;
// }

LteRlcSapUser *
InnerPdcp::GetLteRlcSapUser ()
{
  NS_LOG_FUNCTION (this);
  return m_rlcSapUser;
}

void
InnerPdcp::AddHeaders (Ptr<Packet> p, Ipv4Header ipHeader) const
{
  NS_LOG_FUNCTION (this);
  GtpuHeader gtpHeader;
  UdpHeader udpHeader;

  p->AddHeader (gtpHeader);
  p->AddHeader (udpHeader);
  p->AddHeader (ipHeader);
}

void
InnerPdcp::AddHeaders (Ptr<Packet> p, Ipv6Header ipHeader) const
{
  NS_LOG_FUNCTION (this);
  GtpuHeader gtpHeader;
  UdpHeader udpHeader;

  p->AddHeader (gtpHeader);
  p->AddHeader (udpHeader);
  p->AddHeader (ipHeader);
}

void
InnerPdcp::RemoveHeaders (Ptr<Packet> p) const
{
  NS_LOG_FUNCTION (this);
  Ipv4Header ipv4Header;
  Ipv4Header ipv6Header;
  UdpHeader udpHeader;
  GtpuHeader gtpHeader;

  if (p->PeekHeader (ipv4Header) != 0)
    {
      p->RemoveHeader (ipv4Header);
    }
  else if (p->PeekHeader (ipv6Header) != 0)
    {
      p->RemoveHeader (ipv6Header);
    }
  else
    {
      NS_ABORT_MSG ("Unknown IP type");
    }

  p->RemoveHeader (udpHeader);
  p->RemoveHeader (gtpHeader);
}

void
InnerPdcp::DoReceiveInnerPdcpSdu (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this);
  Ipv4Header ipHeader;
  ipHeader.SetDestination (m_donorIpAddress);
  AddHeaders (p, ipHeader);
  if (m_bap)
    {
      m_bap->DoTransmitBapSdu (p);
    }
  else
    {
      NS_ABORT_MSG ("Pointer to BAP not assigned");
    }
}

void
InnerPdcp::DoReceiveInnerPdcpPdu (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this);
  RemoveHeaders (p);
  m_pdcpInterface->ReceivePdcpPdu (p);
}

void
InnerPdcp::SetDonorIpAddress (Ipv4Address ip)
{
  NS_LOG_FUNCTION (this);
  m_donorIpAddress = ip;
}

void
InnerPdcp::SetBap (Ptr<Bap> bap)
{
  NS_LOG_FUNCTION (this);
  m_bap = bap;
}

void
InnerPdcp::SetPdcpSapProvider (LteRlcSapUser *pdcpInterface)
{
  NS_LOG_FUNCTION (this);
  m_pdcpInterface = pdcpInterface;
}

} // namespace ns3
