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

#include "epc-iab-application.h"
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

NS_LOG_COMPONENT_DEFINE ("EpcIabApplication");

TypeId
EpcIabApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::EpcIabApplication").SetParent<Object> ().SetGroupName ("Lte");
  return tid;
}

void
EpcIabApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  delete m_s1SapProvider;
  delete m_s1apSapEnb;
}

EpcIabApplication::EpcIabApplication (uint16_t cellId)
    : m_gtpuUdpPort (2152), // fixed by the standard
      m_s1SapUser (0),
      m_s1apSapEnbProvider (0),
      m_cellId (cellId)
{
  NS_LOG_FUNCTION (this);

  m_s1SapProvider = new MemberEpcEnbS1SapProvider<EpcIabApplication> (this);
  m_s1apSapEnb = new MemberEpcS1apSapEnb<EpcIabApplication> (this);
}

EpcIabApplication::~EpcIabApplication (void)
{
  NS_LOG_FUNCTION (this);
}

void
EpcIabApplication::SetS1SapUser (EpcEnbS1SapUser *s)
{
  m_s1SapUser = s;
}

EpcEnbS1SapProvider *
EpcIabApplication::GetS1SapProvider ()
{
  return m_s1SapProvider;
}

void
EpcIabApplication::SetS1apSapMme (EpcS1apSapEnbProvider *s)
{
  m_s1apSapEnbProvider = s;
}

EpcS1apSapEnb *
EpcIabApplication::GetS1apSapEnb ()
{
  return m_s1apSapEnb;
}

void
EpcIabApplication::DoInitialUeMessage (uint64_t imsi, uint16_t rnti)
{
  NS_LOG_FUNCTION (this);
  // side effect: create entry if not exist
  NS_LOG_DEBUG ("Creating info for IMSI " << imsi << " and RNTI " << rnti << " at the IAB node");
  m_imsiRntiMap[imsi] = rnti;
  m_s1apSapEnbProvider->SendInitialUeMessage (
      imsi, rnti, imsi, m_cellId); // TODO if more than one MME is used, extend this call
}

void
EpcIabApplication::DoPathSwitchRequest (EpcEnbS1SapProvider::PathSwitchRequestParameters params)
{
  NS_FATAL_ERROR ("Not supported for IAB nodes");
}

void
EpcIabApplication::DoUeContextRelease (uint16_t rnti)
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
EpcIabApplication::DoInitialContextSetupRequest (
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

      EpcEnbApplication::EpsFlowId_t rbid (rnti, erabIt->erabId);
      // side effect: create entries if not exist
      m_rbidTeidMap[rnti][erabIt->erabId] = erabIt->erabId;
      m_teidRbidMap[erabIt->erabId] = rbid;
    }
}

void
EpcIabApplication::DoPathSwitchRequestAcknowledge (
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
EpcIabApplication::DoReleaseIndication (uint64_t imsi, uint16_t rnti, uint32_t bearerId)
{
  NS_LOG_FUNCTION (this << bearerId);
  std::list<EpcS1apSapMme::ErabToBeReleasedIndication> erabToBeReleaseIndication;
  EpcS1apSapMme::ErabToBeReleasedIndication erab;
  erab.erabId = bearerId;
  erabToBeReleaseIndication.push_back (erab);
  //From 3GPP TS 23401-950 Section 5.4.4.2, enB sends EPS bearer Identity in Bearer Release Indication message to MME
  m_s1apSapEnbProvider->SendErabReleaseIndication (imsi, rnti, erabToBeReleaseIndication);
}

} // namespace ns3
