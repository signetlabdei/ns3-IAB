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
 *          Support for real S1AP link
 */

#ifndef EPC_IAB_APPLICATION_H
#define EPC_IAB_APPLICATION_H

#include <ns3/address.h>
#include <ns3/socket.h>
#include <ns3/virtual-net-device.h>
#include <ns3/traced-callback.h>
#include <ns3/callback.h>
#include <ns3/ptr.h>
#include <ns3/object.h>
#include <ns3/lte-common.h>
#include <ns3/application.h>
#include <ns3/eps-bearer.h>
#include <ns3/epc-enb-s1-sap.h>
#include <ns3/epc-s1ap-sap.h>
#include <map>
#include <ns3/mmwave-net-device.h>
#include "epc-enb-application.h"

namespace ns3 {
class EpcEnbS1SapUser;
class EpcEnbS1SapProvider;

/**
 * \ingroup lte
 *
 * This application is installed inside eNBs and provides the bridge functionality for user data plane packets between the radio interface and the S1-U interface.
 */
class EpcIabApplication : public Application
{

  friend class MemberEpcEnbS1SapProvider<EpcIabApplication>;
  friend class MemberEpcS1apSapEnb<EpcIabApplication>;

  // inherited from Object
public:
  static TypeId GetTypeId (void);

protected:
  void DoDispose (void);

public:
  /**
   * Constructor

   * \param sgwS1uAddress the IPv4 address at which this eNB will be able to reach its SGW for S1-U communications
   * \param cellId the identifier of the enb
   */
  EpcIabApplication (uint16_t cellId);

  /**
   * Destructor
   *
   */
  virtual ~EpcIabApplication (void);

  /**
   * Set the S1 SAP User
   *
   * \param s the S1 SAP User
   */
  void SetS1SapUser (EpcEnbS1SapUser *s);

  /**
   *
   * \return the S1 SAP Provider
   */
  EpcEnbS1SapProvider *GetS1SapProvider ();

  /**
   * Set the S1AP provider for the S1AP eNB endpoint
   *
   * \param s the S1AP provider
   */
  void SetS1apSapMme (EpcS1apSapEnbProvider *s);

  /**
   *
   * \return the ENB side of the S1-AP SAP
   */
  EpcS1apSapEnb *GetS1apSapEnb ();

  /**
   * Method to be assigned to the recv callback of the LTE socket. It is called when the eNB receives a data packet from the radio interface that is to be forwarded to the SGW.
   *
   * \param socket pointer to the LTE socket
   */
  void RecvFromLteSocket (Ptr<Socket> socket);

  /**
   * Method to be assigned to the recv callback of the S1-U socket. It is called when the eNB receives a data packet from the SGW that is to be forwarded to the UE.
   *
   * \param socket pointer to the S1-U socket
   */
  void RecvFromS1uSocket (Ptr<Socket> socket);

  void RegisterNewIabAttachment (Ptr<mmwave::MmWaveNetDevice> iabDev,
                                 Ptr<mmwave::MmWaveNetDevice> parentDev);

  uint32_t GetIabNodeDepth (uint16_t iabCellId);

  /**
   * TracedCallback signature for data Packet reception event.
   *
   * \param [in] packet The data packet sent from the internet.
   */
  typedef void (*RxTracedCallback) (Ptr<Packet> packet);

private:
  // ENB S1 SAP provider methods
  void DoInitialUeMessage (uint64_t imsi, uint16_t rnti);
  void DoPathSwitchRequest (EpcEnbS1SapProvider::PathSwitchRequestParameters params);
  void DoUeContextRelease (uint16_t rnti);

  // S1-AP SAP ENB methods
  void DoInitialContextSetupRequest (uint64_t mmeUeS1Id, uint16_t enbUeS1Id,
                                     std::list<EpcS1apSapEnb::ErabToBeSetupItem> erabToBeSetupList);
  void DoPathSwitchRequestAcknowledge (
      uint64_t enbUeS1Id, uint64_t mmeUeS1Id, uint16_t cgi,
      std::list<EpcS1apSapEnb::ErabSwitchedInUplinkItem> erabToBeSwitchedInUplinkList);

  /**
   * \brief This function accepts bearer id corresponding to a particular UE and schedules indication of bearer release towards MME
   * \param imsi maps to mmeUeS1Id
   * \param rnti maps to enbUeS1Id
   * \param bearerId Bearer Identity which is to be de-activated
   */
  void DoReleaseIndication (uint64_t imsi, uint16_t rnti, uint32_t bearerId);
  /**
   * internal method used for the actual setup of the S1 Bearer
   *
   * \param teid
   * \param rnti
   * \param bid
   */
  void SetupS1Bearer (uint32_t teid, uint16_t rnti, uint32_t bid);

  /**
   * map of maps telling for each RNTI and BID the corresponding  S1-U TEID
   *
   */
  std::map<uint16_t, std::map<uint32_t, uint32_t>> m_rbidTeidMap;

  /**
   * map telling for each S1-U TEID the corresponding RNTI,BID
   *
   */
  std::map<uint32_t, EpcEnbApplication::EpsFlowId_t> m_teidRbidMap;

  /**
   * UDP port to be used for GTP
   */
  uint16_t m_gtpuUdpPort;

  /**
   * Provider for the S1 SAP
   */
  EpcEnbS1SapProvider *m_s1SapProvider;

  /**
   * User for the S1 SAP
   */
  EpcEnbS1SapUser *m_s1SapUser;

  /**
   * Provider for the methods of S1AP eNB endpoint
   *
   */
  EpcS1apSapEnbProvider *m_s1apSapEnbProvider;

  /**
   * ENB side of the S1-AP SAP eNB endpoint
   *
   */
  EpcS1apSapEnb *m_s1apSapEnb;

  /**
   * UE context info
   *
   */
  std::map<uint64_t, uint16_t> m_imsiRntiMap;

  uint16_t m_cellId;
};

} //namespace ns3

#endif /* EPC_ENB_APPLICATION_H */
