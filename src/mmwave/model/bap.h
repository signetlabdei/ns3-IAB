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

#ifndef BAP_H
#define BAP_H

#include <ns3/traced-value.h>
#include <ns3/trace-source-accessor.h>
#include <ns3/object.h>
#include <ns3/lte-rlc-sap.h>
#include <ns3/epc-enb-application.h>
#include <ns3/inner-pdcp.h>
#include "bap-header.h"
#include <ns3/mmwave-net-device.h>
#include <ns3/lte-ue-rrc.h>

namespace ns3 {
class InnerPdcp;
class LteUeRrc;

/**
 * IAB BAP entity, see 3GPP TS 38.340
 */
class Bap : public Object
{
  /// allow BapRlcSapUser class friend access
  friend class BapRlcSapUser;
  friend class InnerPdcp;

  using NextHopAddressCallback = Callback<std::pair<uint16_t, Mode>, uint16_t, uint16_t, uint16_t>;
  using PathIdCallback = Callback<uint16_t, uint16_t, uint16_t>;
  using DonorBapAddressCallback = Callback<uint16_t, uint64_t>;

public:
  Bap ();
  virtual ~Bap ();
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual void DoDispose ();

  /**
   * Set the local, i.e., of the device this instance is installed to,
   * BAP address
   *
   * \param address the 10 bit BAP address
   */
  void SetLocalAddress (uint16_t address);

  /**
   * Retrieves the local, i.e., of the device this instance is installed to,
   * BAP address
   *
   * \return the 10 bit BAP address of this BAP instance
   */
  uint16_t GetLocalAddress () const;

  void SetDonor ();

  /**
   * Whether the device this BAP is installed to is a IAB donor
   */
  bool IsDonor () const;

  /**
   * \return the RLC SAP User interface offered to the RLC by this BAP
   */
  LteRlcSapUser *GetLteRlcSapUser ();

  void SetThisIabNodeImsi (uint64_t imsi);

  /**
   * TracedCallback for transmission event.
   *
   * \param header The BAP header of the RX packet. If not a PDU,
   *               this field reports the BAP header which has been added
   * \param isPdu Whether the RX packet is a BAP PDU or an SDU
   */
  typedef void (*PacketTxTracedCallback) (BapHeader header, bool isPdu);

  /**
   * TracedCallback signature for reception event.
   *
   * \param header The BAP header of the TX packet. If not a PDU,
   *               this field reports the BAP header which has been removed
   * \param isPdu Whether the TX packet is a BAP PDU or an SDU
   */
  typedef void (*PacketRxTracedCallback) (BapHeader header, bool isPdu);

  void SetNextHopCallback (Callback<std::pair<uint16_t, Mode>, uint16_t, uint16_t, uint16_t> cb);

  void SetPathIdCallback (Callback<uint16_t, uint16_t, uint16_t> cb);

  void SetDonorBapAddressCallback (Callback<uint16_t, uint64_t> cb);

  void TransmitBapSduViaNonPduInterface (Ptr<Packet> p, uint16_t destBapAddress = 0);

  void SetInnerPdcp (Ptr<InnerPdcp> innerPdcp);

  void AddPairRlcMap (uint16_t, LteRlcSapProvider *);

  void SetEpcEnbApplication (Ptr<EpcEnbApplication> app);

  void SetMtRrc (Ptr<LteUeRrc> mtRrc);

protected:
  //TODO: Check and possibly update interface towards upper inner PDCP. Are more parameters needed ?

  /**
   * Interface provided to upper inner PDCP
   *
   * Transmit a BAP SDU at its exit point in the backhaul network, i.e., this method should be called
   * only be the last IAB-node in the path, before passing the packet to the inner PDCP and then
   * to the RLC layer of its DU.
   * The outermost header of the corresponding packet is thus an IPvX header.
   *
   * \param p packet
   */
  void DoTransmitBapSdu (Ptr<Packet> p);

  // TODO: Check and possibly update interface towards lower RLC

  /**
   * Interface provided to the subtending RLC instance
   *
   * Transmit a BAP PDU which has already crossed at least one hop in the backhaul network.
   * The outermost header of the corresponding packet is thus a BAP header.
   *
   * \param p packet
   */
  void DoTransmitBapPdu (Ptr<Packet> p, uint16_t destBapAddress, uint16_t pathId);

  /**
   * Interface provided to upper inner PDCP
   *
   * Transmit a BAP SDU at its exit point in the backhaul network, i.e., this method
   * should be called by the IAB-donor only.
   * The outermost header of the corresponding packet is thus an IPvX header.
   *
   * \param p packet
   */
  void DoReceiveBapSdu (Ptr<Packet> p, bool isLanConnection, BapHeader hedaer);

  // TODO: Check and possibly update interface towards lower RLC

  /**
   * Interface provided to the subtending RLC instance
   *
   * Receive a BAP PDU which has already crossed at least one hop in the backhaul network.
   * The outermost header of the corresponding packet is thus a BAP header.
   *
   * \param p packet
   */
  void DoReceiveBapPdu (Ptr<Packet> p);

  /**
   * Used to inform of a packet delivery to either the RLC SAP provider
   * or the so-called inner PDCP
   */
  TracedCallback<BapHeader, uint16_t> m_txPdu;
  /**
   * Used to inform of a packet reception from either the RLC SAP provider
   * or the so-called inner PDCP
   */
  TracedCallback<BapHeader, uint16_t> m_rxPdu;

  /**
   * Map associating the BAP address of the possible next hops to the
   * RLC SAP provider interface of the corresponding default DRB
   * //TODO: dedicated DRBs ?
   **/
  std::map<uint16_t, LteRlcSapProvider *> m_bapAddressRlcMap;
  std::map<Ipv4Address, uint16_t> m_ipBapAddressMap;

  LteRlcSapUser
      *m_rlcSapUser; //<! RLC SAP user interface provided to the lower layers for receiving packets
  bool m_isDonor; //<! Whether this BAP instance is installed on an IAB-donor

private:
  uint16_t m_localAddress; //<! The BAP address of this instance
  uint64_t m_imsi;
  uint32_t m_bid;
  uint16_t m_rnti;

  Ptr<EpcEnbApplication> m_epcEnbApplication{
      nullptr}; //<! A ptr to the EPC eNB application, if installed on the donor
  Ptr<InnerPdcp> m_innerPdcp;

  Ptr<LteUeRrc> m_iabMtRrc{nullptr};

  //<! Callback for retrieving the BAP address of the next hop
  NextHopAddressCallback m_nextHopAddressCallback{
      MakeNullCallback<std::pair<uint16_t, Mode>, uint16_t, uint16_t, uint16_t> ()};

  PathIdCallback m_pathIdCallback{MakeNullCallback<uint16_t, uint16_t, uint16_t> ()};

  DonorBapAddressCallback m_donorBapAddressCallback{MakeNullCallback<uint16_t, uint64_t> ()};
};

} // namespace ns3

#endif // BAP_H
