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
*   Authors: Matteo Pagin <paginmatte@dei.unipd.it>
*            Alessandro Traspadini <traspadini@dei.unipd.it>
*/

#ifndef SRC_MMWAVE_MODEL_MMWAVE_IAB_NET_DEVICE_H_
#define SRC_MMWAVE_MODEL_MMWAVE_IAB_NET_DEVICE_H_

#include "mmwave-net-device.h"
#include <ns3/event-id.h>
#include <ns3/traced-callback.h>
#include <ns3/nstime.h>
#include "mmwave-phy.h"
#include "mmwave-enb-phy.h"
#include "mmwave-enb-mac.h"
#include "mmwave-ue-mac.h"
#include "mmwave-mac-scheduler.h"
#include <vector>
#include <ns3/ipv4-interface-container.h>
#include <map>
#include <ns3/lte-enb-rrc.h>
#include <ns3/epc-ue-nas.h>
#include <ns3/lte-ue-rrc.h>
#include "mmwave-ue-phy.h"
#include "bap.h"

namespace ns3 {
/* Add forward declarations here */
class Packet;
class PacketBurst;
class Node;
class LteEnbComponentCarrierManager;
class LteUeComponentCarrierManager;

namespace mmwave {
//class MmWavePhy;
class MmWaveEnbPhy;
class MmWaveUePhy;
class MmWaveEnbMac;

class MmWaveIabNetDevice : public MmWaveNetDevice
{
public:
  static TypeId GetTypeId (void);

  MmWaveIabNetDevice ();

  virtual ~MmWaveIabNetDevice (void);
  virtual void DoDispose (void) override;
  virtual bool DoSend (Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber) override;

  Ptr<MmWaveUePhy> GetMtPhy (void) const;

  Ptr<MmWaveUePhy> GetMtPhy (uint8_t index);

  Ptr<MmWaveUeMac> GetMtMac (void);

  Ptr<MmWaveUeMac> GetMtMac (uint8_t index);

  Ptr<MmWaveEnbPhy> GetDuPhy (void) const;

  Ptr<MmWaveEnbPhy> GetDuPhy (uint8_t index);

  Ptr<MmWaveEnbMac> GetDuMac (void);

  Ptr<MmWaveEnbMac> GetDuMac (uint8_t index);

  Ptr<Bap> GetBap ();

  void SetBap (Ptr<Bap> bap);

  void SetIpInterface (Ipv4InterfaceContainer address);

  Ipv4InterfaceContainer GetIpInterface (void) const;

  uint16_t GetCellId () const;

  bool HasCellId (uint16_t cellId) const;

  void SetDuRrc (Ptr<LteEnbRrc> rrc);

  void SetMtRrc (Ptr<LteUeRrc> rrc);

  Ptr<LteEnbRrc> GetDuRrc (void);

  Ptr<LteUeRrc> GetMtRrc (void);

  Ptr<EpcUeNas> GetMtNas (void);

  void SetCcMap (std::map<uint8_t, Ptr<MmWaveComponentCarrier>> ccm) override;

  void SetDuCcMap (std::map<uint8_t, Ptr<MmWaveComponentCarrier>> ccm);

  void SetMtCcMap (std::map<uint8_t, Ptr<MmWaveComponentCarrier>> ccm);

  std::map<uint8_t, Ptr<MmWaveComponentCarrier>> GetCcMap (void) const override;

  std::map<uint8_t, Ptr<MmWaveComponentCarrier>> GetDuCcMap (void) const;

  std::map<uint8_t, Ptr<MmWaveComponentCarrier>> GetMtCcMap (void) const;

  uint64_t GetImsi () const;

  void SetTargetDonor (Ptr<MmWaveEnbNetDevice> enb);
  Ptr<MmWaveEnbNetDevice> GetTargetDonor (void);

  void SetIabParent (Ptr<MmWaveIabNetDevice> iabParent);
  Ptr<MmWaveIabNetDevice> GetIabParent (void);

  Ptr<PhasedArrayModel> GetAntenna (uint8_t ccId) const override;

  Ptr<PhasedArrayModel> GetDuAntenna (uint8_t ccId) const;
  Ptr<PhasedArrayModel> GetMtAntenna (uint8_t ccId) const;

protected:
  virtual void DoInitialize (void) override;
  void UpdateConfig ();

private:
  Ptr<MmWaveUePhy> m_mtPhy;

  Ptr<MmWaveUeMac> m_mtMac;

  Ptr<MmWaveEnbPhy> m_duPhy;

  Ptr<MmWaveEnbMac> m_duMac;

  Ptr<Bap> m_bap;

  Ptr<MmWaveEnbNetDevice> m_targetDonor {nullptr}; // TODO: check if needed

  Ptr<MmWaveIabNetDevice> m_iabParent {nullptr}; // TODO: check if needed

  Ptr<MmWaveMacScheduler> m_duScheduler;

  Ptr<LteEnbRrc> m_duRrc;

  Ptr<LteUeRrc> m_mtRrc;

  Ptr<EpcUeNas> m_mtNas;

  uint64_t m_imsi; /* IMSI for the MT part*/

  uint16_t m_cellId; /* Cell Identifer. To uniquely identify an E-nodeB  */

  uint8_t m_bandwidth; /* bandwidth in RBs (?) */

  Ptr<LteEnbComponentCarrierManager>
      m_duComponentCarrierManager; ///< the component carrier manager of this DU

  Ptr<LteUeComponentCarrierManager>
      m_mtComponentCarrierManager; ///< the component carrier manager of this MT

  bool m_duIsConfigured;

  bool m_isConstructed;

  bool m_isInitialized;

  Ipv4InterfaceContainer m_ipInterface;

  std::map<uint8_t, Ptr<MmWaveComponentCarrier>> m_mtCcMap; //!< ComponentCarrier map

  std::map<uint8_t, Ptr<MmWaveComponentCarrier>> m_duCcMap; //!< ComponentCarrier map
};
} // namespace mmwave
} // namespace ns3

#endif /* SRC_MMWAVE_MODEL_MMWAVE_IAB_NET_DEVICE_H_ */
