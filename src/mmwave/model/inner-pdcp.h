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

#ifndef INNER_PDCP_H
#define INNER_PDCP_H

#include <ns3/traced-value.h>
#include <ns3/trace-source-accessor.h>

#include <ns3/object.h>

#include <ns3/lte-rlc-sap.h>
#include <ns3/ipv4-header.h>
#include <ns3/ipv6-header.h>
#include <ns3/udp-header.h>
#include <ns3/epc-gtpu-header.h>
#include <ns3/bap.h>

namespace ns3 {

class Bap;

/**
 * INNER PDCP
 */
class InnerPdcp : public Object
{
  friend class InnerPdcpRlcSapUser;
  friend class Bap;

public:
  InnerPdcp ();
  virtual ~InnerPdcp ();
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual void DoDispose ();

  /**
   * \return the RLC SAP User interface offered to the RLC by this BAP
   */
  LteRlcSapUser *GetLteRlcSapUser ();

  // /**
  //  *
  //  *
  //  * \param rnti
  //  */
  // void SetRnti (uint16_t rnti);

  // /**
  //  *
  //  *
  //  * \param lcId
  //  */
  // void SetLcId (uint8_t lcId);

  // void SetHeaders (Ipv4Header ipHeader, UdpHeader udpHeader);

  // void SetHeaders (Ipv6Header ipHeader, UdpHeader udpHeader);

  // void SetIpHeader (Ipv4Header ipHeader);

  // void SetIpHeader (Ipv6Header ipHeader);

  // void SetUdpHeader (UdpHeader udpHeader);

  //   void SetGtpHeader (GtpuHeader gtpHeader);

  void AddHeaders (Ptr<Packet> p, Ipv4Header ipHeader) const;
  void AddHeaders (Ptr<Packet> p, Ipv6Header ipHeader) const;

  void RemoveHeaders (Ptr<Packet> p) const;

  void SetDonorIpAddress (Ipv4Address ip);

  void SetBap (Ptr<Bap> bap);

  void SetPdcpSapProvider (LteRlcSapUser *pdcpInterface);

protected:
  /**
   *
   * \param p packet
   */
  void DoReceiveInnerPdcpSdu (Ptr<Packet> p);

  /**
   *
   * \param p packet
   */
  void DoReceiveInnerPdcpPdu (Ptr<Packet> p);

  uint16_t m_rnti; ///< RNTI
  uint8_t m_lcid; ///< LCID
  // Ipv4Header m_ipv4Header; ///< IPv4 Header
  // Ipv6Header m_ipv6Header; ///< IPv6 Header
  // UdpHeader m_udpHeader; ///< UDP Header
  // GtpuHeader m_gtpHeader; ///< GTP Header
  LteRlcSapUser
      *m_rlcSapUser; //<! RLC SAP user interface provided to the lower layers for receiving packets
  LteRlcSapUser *m_pdcpInterface;
  Ipv4Address m_donorIpAddress;
  Ptr<Bap> m_bap;
};

} // namespace ns3

#endif // INNER_PDCP_H
