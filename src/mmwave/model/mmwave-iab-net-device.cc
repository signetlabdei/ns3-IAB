/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
*   Copyright (c) 2022 University of Padova, Dep. of Information Engineering, SIGNET lab.
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License version 2 as
*   published by the Free Software Foundation;
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*   Authors: Matteo Pagin, Alessandro Traspadini
*/

#include <ns3/llc-snap-header.h>
#include <ns3/simulator.h>
#include <ns3/callback.h>
#include <ns3/node.h>
#include <ns3/packet.h>
#include "mmwave-net-device.h"
#include <ns3/packet-burst.h>
#include <ns3/uinteger.h>
#include <ns3/trace-source-accessor.h>
#include <ns3/pointer.h>
#include <ns3/enum.h>
#include <ns3/uinteger.h>
#include <ns3/object-map.h>
#include "mmwave-enb-net-device.h"
#include "mmwave-ue-net-device.h"
#include "mmwave-iab-net-device.h"
#include <ns3/lte-enb-rrc.h>
#include <ns3/ipv4-l3-protocol.h>
#include <ns3/ipv6-l3-protocol.h>
#include <ns3/abort.h>
#include <ns3/log.h>
#include <ns3/lte-enb-component-carrier-manager.h>
#include <ns3/lte-ue-component-carrier-manager.h>
#include "mmwave-component-carrier-enb.h"
#include "mmwave-component-carrier-ue.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MmWaveIabNetDevice");

namespace mmwave {

NS_OBJECT_ENSURE_REGISTERED (MmWaveIabNetDevice);

TypeId
MmWaveIabNetDevice::GetTypeId ()
{
  static TypeId tid =
      TypeId ("ns3::MmWaveIabNetDevice")
          .SetParent<MmWaveNetDevice> ()
          .AddConstructor<MmWaveIabNetDevice> ()
          .AddAttribute ("LteEnbComponentCarrierManager",
                         "The ComponentCarrierManager associated to this EnbNetDevice",
                         PointerValue (),
                         MakePointerAccessor (&MmWaveIabNetDevice::m_duComponentCarrierManager),
                         MakePointerChecker<LteEnbComponentCarrierManager> ())
          .AddAttribute ("LteUeComponentCarrierManager",
                         "The ComponentCarrierManager associated to this UeNetDevice",
                         PointerValue (),
                         MakePointerAccessor (&MmWaveIabNetDevice::m_mtComponentCarrierManager),
                         MakePointerChecker<LteUeComponentCarrierManager> ())
          .AddAttribute ("MtComponentCarrierMap", "The ComponentCarrierMap associated to the MT",
                         ObjectMapValue (), MakeObjectMapAccessor (&MmWaveIabNetDevice::m_mtCcMap),
                         MakeObjectMapChecker<ComponentCarrierUe> ())
          .AddAttribute ("DuComponentCarrierMap", "The ComponentCarrierMap associated to the DU",
                         ObjectMapValue (), MakeObjectMapAccessor (&MmWaveIabNetDevice::m_duCcMap),
                         MakeObjectMapChecker<ComponentCarrierEnb> ())

          .AddAttribute ("LteEnbRrc", "The RRC layer associated with the DU", PointerValue (),

                         MakePointerAccessor (&MmWaveIabNetDevice::m_duRrc),
                         MakePointerChecker<LteEnbRrc> ())
          .AddAttribute ("LteUeRrc", "The RRC layer associated with the MT", PointerValue (),
                         MakePointerAccessor (&MmWaveIabNetDevice::m_mtRrc),
                         MakePointerChecker<LteUeRrc> ())
          .AddAttribute ("EpcUeNas", "The NAS associated to this UeNetDevice", PointerValue (),
                         MakePointerAccessor (&MmWaveIabNetDevice::m_mtNas),
                         MakePointerChecker<EpcUeNas> ())
          .AddAttribute ("CellId", "Cell Identifier", UintegerValue (0),
                         MakeUintegerAccessor (&MmWaveIabNetDevice::m_cellId),
                         MakeUintegerChecker<uint16_t> ())
          .AddAttribute ("Imsi", "International Mobile Subscriber Identity assigned to this UE",
                         UintegerValue (0), MakeUintegerAccessor (&MmWaveIabNetDevice::m_imsi),
                         MakeUintegerChecker<uint64_t> ());
  return tid;
}

MmWaveIabNetDevice::MmWaveIabNetDevice ()
    //:m_cellId(0),
    : m_duComponentCarrierManager (nullptr),
      m_mtComponentCarrierManager (nullptr),
      m_duIsConfigured (false),
      m_isConstructed (false),
      m_isInitialized (false)
{
  NS_LOG_FUNCTION (this);
}

MmWaveIabNetDevice::~MmWaveIabNetDevice ()
{
  NS_LOG_FUNCTION (this);
}

void
MmWaveIabNetDevice::DoInitialize (void)
{
  NS_LOG_FUNCTION (this);
  if (!m_isInitialized)
    {
      m_isConstructed = true;
      UpdateConfig ();
      for (auto it = m_duCcMap.begin (); it != m_duCcMap.end (); ++it)
        {
          it->second->Initialize ();
        }
      for (auto it = m_mtCcMap.begin (); it != m_mtCcMap.end (); ++it)
        {
          Ptr<MmWaveComponentCarrierUe> ccMt = DynamicCast<MmWaveComponentCarrierUe> (it->second);
          ccMt->GetPhy ()->Initialize ();
          ccMt->GetMac ()->Initialize ();
        }
      m_duRrc->Initialize ();
      m_mtRrc->Initialize ();
      m_duComponentCarrierManager->Initialize ();
      m_isInitialized = true;
    }
}

void
MmWaveIabNetDevice::DoDispose ()
{
  NS_LOG_FUNCTION (this);

  m_duRrc->Dispose ();
  m_mtRrc->Dispose ();
  m_duRrc = nullptr;
  m_mtRrc = nullptr;

  m_duComponentCarrierManager->Dispose ();
  m_mtComponentCarrierManager->Dispose ();
  m_duComponentCarrierManager = nullptr;
  m_mtComponentCarrierManager = nullptr;

  // MmWaveComponentCarrierIab::DoDispose() will call DoDispose
  // of its PHY, MAC, FFR and scheduler instance
  NS_ASSERT (m_duCcMap.size () == m_mtCcMap.size ());

  for (uint32_t i = 0; i < m_ccMap.size (); i++)
    {
      m_duCcMap.at (i)->Dispose ();
      m_duCcMap.at (i) = nullptr;
      m_mtCcMap.at (i)->Dispose ();
      m_mtCcMap.at (i) = nullptr;
    }

  MmWaveNetDevice::DoDispose ();
}

Ptr<MmWaveEnbPhy>
MmWaveIabNetDevice::GetDuPhy (void) const
{
  NS_LOG_FUNCTION (this);
  return DynamicCast<MmWaveComponentCarrierEnb> (m_duCcMap.at (0))->GetPhy ();
}

Ptr<MmWaveEnbPhy>
MmWaveIabNetDevice::GetDuPhy (uint8_t index)
{
  return DynamicCast<MmWaveComponentCarrierEnb> (m_duCcMap.at (index))->GetPhy ();
}

Ptr<MmWaveUePhy>
MmWaveIabNetDevice::GetMtPhy (void) const
{
  NS_LOG_FUNCTION (this);
  return DynamicCast<MmWaveComponentCarrierUe> (m_mtCcMap.at (0))->GetPhy ();
}

Ptr<MmWaveUePhy>
MmWaveIabNetDevice::GetMtPhy (uint8_t index)
{
  return DynamicCast<MmWaveComponentCarrierUe> (m_mtCcMap.at (index))->GetPhy ();
}

uint16_t
MmWaveIabNetDevice::GetCellId () const
{
  NS_LOG_FUNCTION (this);
  return m_cellId;
}

bool
MmWaveIabNetDevice::HasCellId (uint16_t cellId) const
{
  for (auto &it : m_duCcMap)
    {

      if (DynamicCast<MmWaveComponentCarrierEnb> (it.second)->GetCellId () == cellId)
        {
          return true;
        }
    }
  return false;
}

Ptr<MmWaveEnbMac>
MmWaveIabNetDevice::GetDuMac (void)
{
  return DynamicCast<MmWaveComponentCarrierEnb> (m_duCcMap.at (0))->GetMac ();
}

Ptr<MmWaveEnbMac>
MmWaveIabNetDevice::GetDuMac (uint8_t index)
{
  return DynamicCast<MmWaveComponentCarrierEnb> (m_duCcMap.at (index))->GetMac ();
}

Ptr<MmWaveUeMac>
MmWaveIabNetDevice::GetMtMac (void)
{
  return DynamicCast<MmWaveComponentCarrierUe> (m_mtCcMap.at (0))->GetMac ();
}

Ptr<MmWaveUeMac>
MmWaveIabNetDevice::GetMtMac (uint8_t index)
{
  return DynamicCast<MmWaveComponentCarrierUe> (m_mtCcMap.at (index))->GetMac ();
}

void
MmWaveIabNetDevice::SetDuRrc (Ptr<LteEnbRrc> rrc)
{
  m_duRrc = rrc;
}

Ptr<LteEnbRrc>
MmWaveIabNetDevice::GetDuRrc (void)
{
  return m_duRrc;
}

void
MmWaveIabNetDevice::SetMtRrc (Ptr<LteUeRrc> rrc)
{
  m_mtRrc = rrc;
}

Ptr<LteUeRrc>
MmWaveIabNetDevice::GetMtRrc (void)
{
  return m_mtRrc;
}

Ptr<EpcUeNas>
MmWaveIabNetDevice::GetMtNas (void)
{
  return m_mtNas;
}

void
MmWaveIabNetDevice::SetTargetDonor (Ptr<MmWaveEnbNetDevice> enb)
{
  NS_LOG_FUNCTION (this << enb);
  m_targetDonor = enb;
}

Ptr<MmWaveEnbNetDevice>
MmWaveIabNetDevice::GetTargetDonor (void)
{
  NS_LOG_FUNCTION (this);
  return m_targetDonor;
}

void
MmWaveIabNetDevice::SetIabParent (Ptr<MmWaveIabNetDevice> iabParent)
{
  NS_LOG_FUNCTION (this << iabParent);
  m_iabParent = iabParent;
}

Ptr<MmWaveIabNetDevice>
MmWaveIabNetDevice::GetIabParent (void)
{
  NS_LOG_FUNCTION (this);
  return m_iabParent;
}

Ptr<Bap>
MmWaveIabNetDevice::GetBap ()
{
  NS_LOG_FUNCTION (this);
  return m_bap;
}

void
MmWaveIabNetDevice::SetBap (Ptr<Bap> bap)
{
  NS_LOG_FUNCTION (this);
  m_bap = bap;
}

bool
MmWaveIabNetDevice::DoSend (Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
{
  NS_LOG_FUNCTION (this << packet << dest << protocolNumber);
  NS_ABORT_MSG_IF (protocolNumber != Ipv4L3Protocol::PROT_NUMBER &&
                       protocolNumber != Ipv6L3Protocol::PROT_NUMBER,
                   "unsupported protocol " << protocolNumber << ", only IPv4/IPv6 is supported");

  m_bap->TransmitBapSduViaNonPduInterface (packet);

  return true;
}

void
MmWaveIabNetDevice::UpdateConfig () // TODO:
{
  NS_LOG_FUNCTION (this);
  // DU part
  if (m_isConstructed)
    {
      if (!m_duIsConfigured)
        {
          NS_LOG_LOGIC (this << " Configure cell " << m_cellId);
          NS_ASSERT (!m_duCcMap.empty ());
          std::map<uint8_t, LteEnbRrc::MmWaveComponentCarrierConf> ccConfMap;
          for (auto it = m_duCcMap.begin (); it != m_duCcMap.end (); ++it)
            {
              Ptr<MmWaveComponentCarrierEnb> ccEnb =
                  DynamicCast<MmWaveComponentCarrierEnb> (it->second);
              LteEnbRrc::MmWaveComponentCarrierConf ccConf;
              ccConf.m_ccId = ccEnb->GetConfigurationParameters ()->GetCcId ();
              ccConf.m_cellId = ccEnb->GetCellId ();
              ccConf.m_bandwidth = ccEnb->GetBandwidthInRb ();

              ccConfMap[it->first] = ccConf;
            }
          m_duRrc->ConfigureCell (ccConfMap);
          m_duIsConfigured = true;
        }

      // MT part
      m_mtNas->SetImsi (m_imsi);
      m_mtRrc->SetImsi (m_imsi);

      // TODO: Are these needed ?
      //m_rrc->SetCsgId (m_csgId, m_csgIndication);
      // m_mtNas->SetCsgId (m_csgId); // this also handles propagation to RRC
    }
  else
    {
      /*
    * DU Lower layers are not ready yet, so do nothing now and expect
    * ``DoInitialize`` to re-invoke this function.
    */

      /*
    * MT NAS and RRC instances are not be ready yet, so do nothing now and
    * expect ``DoInitialize`` to re-invoke this function.
    */
    }
}

void
MmWaveIabNetDevice::SetCcMap (std::map<uint8_t, Ptr<MmWaveComponentCarrier>> ccm)
{
  NS_ABORT_MSG ("This function must not be called. Use SetDuCcMap and SetMuCcMap as needed.");
}

void
MmWaveIabNetDevice::SetDuCcMap (std::map<uint8_t, Ptr<MmWaveComponentCarrier>> ccm)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT_MSG (!m_duIsConfigured, "attempt to set CC map after configuration");
  m_duCcMap = ccm;
}

void
MmWaveIabNetDevice::SetMtCcMap (std::map<uint8_t, Ptr<MmWaveComponentCarrier>> ccm)
{
  NS_LOG_FUNCTION (this);
  m_mtCcMap = ccm;
}

std::map<uint8_t, Ptr<MmWaveComponentCarrier>>
MmWaveIabNetDevice::GetCcMap () const
{
  NS_ABORT_MSG ("This function must not be called. Use GetDuCcMap and GetMuCcMap as needed.");

  return std::map<uint8_t, Ptr<MmWaveComponentCarrier>>{};
}

std::map<uint8_t, Ptr<MmWaveComponentCarrier>>
MmWaveIabNetDevice::GetDuCcMap () const
{
  NS_LOG_FUNCTION (this);

  return m_duCcMap;
}

std::map<uint8_t, Ptr<MmWaveComponentCarrier>>
MmWaveIabNetDevice::GetMtCcMap () const
{
  return m_mtCcMap;
}

uint64_t
MmWaveIabNetDevice::GetImsi () const
{
  NS_LOG_FUNCTION (this);
  return m_imsi;
}

Ptr<PhasedArrayModel>
MmWaveIabNetDevice::GetAntenna (uint8_t ccId) const
{
  NS_ABORT_MSG ("This function must not be called. Use GetDuAntenna and GetMtAntenna as needed.");

  return m_ccMap.at (ccId)->GetAntenna ();
}

Ptr<PhasedArrayModel>
MmWaveIabNetDevice::GetDuAntenna (uint8_t ccId) const
{
  NS_LOG_FUNCTION (this << +ccId);
  return m_duCcMap.at (ccId)->GetAntenna ();
}

Ptr<PhasedArrayModel>
MmWaveIabNetDevice::GetMtAntenna (uint8_t ccId) const
{
  NS_LOG_FUNCTION (this << +ccId);
  return m_mtCcMap.at (ccId)->GetAntenna ();
}

} // namespace mmwave
} // namespace ns3
