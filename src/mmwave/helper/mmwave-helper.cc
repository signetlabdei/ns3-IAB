/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
*   Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
*   Copyright (c) 2015, NYU WIRELESS, Tandon School of Engineering, New York University
*   Copyright (c) 2016, 2018, University of Padova, Dep. of Information Engineering, SIGNET lab.
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
*   Author: Marco Miozzo <marco.miozzo@cttc.es>
*           Nicola Baldo  <nbaldo@cttc.es>
*
*   Modified by: Marco Mezzavilla < mezzavilla@nyu.edu>
*                         Sourjya Dutta <sdutta@nyu.edu>
*                         Russell Ford <russell.ford@nyu.edu>
*                         Menglei Zhang <menglei@nyu.edu>
*
* Modified by: Michele Polese <michele.polese@gmail.com>
*                 Dual Connectivity and Handover functionalities
*
* Modified by: Tommaso Zugno <tommasozugno@gmail.com>
*                Integration of Carrier Aggregation
*/

#include <ns3/string.h>
#include <ns3/log.h>
#include <ns3/abort.h>
#include <ns3/pointer.h>
#include <iostream>
#include <string>
#include <sstream>
#include "mmwave-helper.h"
#include <ns3/abort.h>
#include <ns3/multi-model-spectrum-channel.h>
#include <ns3/uinteger.h>
#include <ns3/double.h>
#include <ns3/ipv4.h>
#include <ns3/mmwave-lte-rrc-protocol-real.h>
#include <ns3/epc-enb-application.h>
#include <ns3/epc-iab-application.h>
#include <ns3/epc-x2.h>
#include <ns3/friis-spectrum-propagation-loss.h>
#include <ns3/mmwave-rrc-protocol-ideal.h>
#include <ns3/lte-spectrum-phy.h>
#include <ns3/lte-chunk-processor.h>
#include <ns3/isotropic-antenna-model.h>
#include <ns3/mmwave-propagation-loss-model.h>
#include <ns3/lte-enb-component-carrier-manager.h>
#include <ns3/lte-ue-component-carrier-manager.h>
#include <ns3/cc-helper.h>
#include <ns3/object-map.h>
#include <ns3/three-gpp-spectrum-propagation-loss-model.h>
#include <ns3/channel-condition-model.h>
#include <ns3/three-gpp-propagation-loss-model.h>
#include <ns3/mmwave-beamforming-model.h>
#include <ns3/uniform-planar-array.h>
#include <ns3/file-beamforming-codebook.h>
#include <ns3/mmwave-iab-net-device.h>
#include <ns3/mmwave-point-to-point-epc-helper.h>
#include <ns3/epc-sgw-pgw-application.h>

namespace ns3 {

/* ... */
NS_LOG_COMPONENT_DEFINE ("MmWaveHelper");

namespace mmwave {

NS_OBJECT_ENSURE_REGISTERED (MmWaveHelper);

MmWaveHelper::MmWaveHelper (void)
    : m_imsiCounter (0),
      m_cellIdCounter (1),
      m_bapAddressCounter (1),
      m_bapPathIdCounter (1),
      m_harqEnabled (false),
      m_rlcAmEnabled (false),
      m_snrTest (false),
      m_useIdealRrc (false)
{
  NS_LOG_FUNCTION (this);
  m_channelFactory.SetTypeId (MultiModelSpectrumChannel::GetTypeId ());
  m_lteChannelFactory.SetTypeId (MultiModelSpectrumChannel::GetTypeId ());
  m_enbNetDeviceFactory.SetTypeId (MmWaveEnbNetDevice::GetTypeId ());
  m_iabNetDeviceFactory.SetTypeId (MmWaveIabNetDevice::GetTypeId ());
  m_lteEnbNetDeviceFactory.SetTypeId (LteEnbNetDevice::GetTypeId ());
  m_ueNetDeviceFactory.SetTypeId (MmWaveUeNetDevice::GetTypeId ());

  m_mcUeNetDeviceFactory.SetTypeId (McUeNetDevice::GetTypeId ());

  m_lteUeAntennaModelFactory.SetTypeId (IsotropicAntennaModel::GetTypeId ());
  m_lteEnbAntennaModelFactory.SetTypeId (IsotropicAntennaModel::GetTypeId ());

  m_uePhasedArrayModelFactory.SetTypeId (UniformPlanarArray::GetTypeId ());
  m_uePhasedArrayModelFactory.Set ("NumColumns", UintegerValue (2));
  m_uePhasedArrayModelFactory.Set ("NumRows", UintegerValue (2));
  m_enbPhasedArrayModelFactory.SetTypeId (UniformPlanarArray::GetTypeId ());
  m_enbPhasedArrayModelFactory.Set ("NumColumns", UintegerValue (8));
  m_enbPhasedArrayModelFactory.Set ("NumRows", UintegerValue (8));
  m_iabPhasedArrayModelFactory.SetTypeId (UniformPlanarArray::GetTypeId ());
  m_iabPhasedArrayModelFactory.Set ("NumColumns", UintegerValue (8));
  m_iabPhasedArrayModelFactory.Set ("NumRows", UintegerValue (8));

  m_ueBeamformingCodebookFactory.SetTypeId (FileBeamformingCodebook::GetTypeId ());
  m_enbBeamformingCodebookFactory.SetTypeId (FileBeamformingCodebook::GetTypeId ());
  m_iabBeamformingCodebookFactory.SetTypeId (FileBeamformingCodebook::GetTypeId ());

  m_bfModelFactory.SetTypeId (MmWaveSvdBeamforming::GetTypeId ());
}

MmWaveHelper::~MmWaveHelper (void)
{
  NS_LOG_FUNCTION (this);
}

TypeId
MmWaveHelper::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::MmWaveHelper")
          .SetParent<Object> ()
          .AddConstructor<MmWaveHelper> ()
          .AddAttribute ("PathlossModel",
                         "The type of path-loss model to be used. "
                         "The allowed values for this attributes are the type names "
                         "of any class inheriting from ns3::PropagationLossModel.",
                         StringValue ("ns3::ThreeGppUmaPropagationLossModel"),
                         MakeStringAccessor (&MmWaveHelper::SetPathlossModelType),
                         MakeStringChecker ())
          .AddAttribute ("ChannelModel",
                         "The type of MIMO channel model to be used. "
                         "The allowed values for this attributes are the type names "
                         "of any class inheriting from ns3::SpectrumPropagationLossModel.",
                         StringValue ("ns3::ThreeGppSpectrumPropagationLossModel"),
                         MakeStringAccessor (&MmWaveHelper::SetChannelModelType),
                         MakeStringChecker ())
          .AddAttribute (
              "Scheduler",
              "The type of scheduler to be used for MmWave eNBs. "
              "The allowed values for this attributes are the type names "
              "of any class inheriting from ns3::MmWaveMacScheduler.",
              StringValue ("ns3::MmWaveFlexTtiMacScheduler"),
              MakeStringAccessor (&MmWaveHelper::SetSchedulerType, &MmWaveHelper::GetSchedulerType),
              MakeStringChecker ())
          .AddAttribute ("HarqEnabled", "Enable Hybrid ARQ", BooleanValue (true),
                         MakeBooleanAccessor (&MmWaveHelper::m_harqEnabled), MakeBooleanChecker ())
          .AddAttribute ("RlcAmEnabled", "Enable RLC Acknowledged Mode", BooleanValue (false),
                         MakeBooleanAccessor (&MmWaveHelper::m_rlcAmEnabled), MakeBooleanChecker ())
          .AddAttribute ("LteScheduler",
                         "The type of scheduler to be used for LTE eNBs. "
                         "The allowed values for this attributes are the type names "
                         "of any class inheriting from ns3::FfMacScheduler.",
                         StringValue ("ns3::PfFfMacScheduler"),
                         MakeStringAccessor (&MmWaveHelper::SetLteSchedulerType,
                                             &MmWaveHelper::GetLteSchedulerType),
                         MakeStringChecker ())
          .AddAttribute ("LteFfrAlgorithm",
                         "The type of FFR algorithm to be used for LTE eNBs. "
                         "The allowed values for this attributes are the type names "
                         "of any class inheriting from ns3::LteFfrAlgorithm.",
                         StringValue ("ns3::LteFrNoOpAlgorithm"),
                         MakeStringAccessor (&MmWaveHelper::SetLteFfrAlgorithmType,
                                             &MmWaveHelper::GetLteFfrAlgorithmType),
                         MakeStringChecker ())
          .AddAttribute ("LteHandoverAlgorithm",
                         "The type of handover algorithm to be used for LTE eNBs. "
                         "The allowed values for this attributes are the type names "
                         "of any class inheriting from ns3::LteHandoverAlgorithm.",
                         StringValue ("ns3::NoOpHandoverAlgorithm"),
                         MakeStringAccessor (&MmWaveHelper::SetLteHandoverAlgorithmType,
                                             &MmWaveHelper::GetLteHandoverAlgorithmType),
                         MakeStringChecker ())
          .AddAttribute ("LtePathlossModel",
                         "The type of pathloss model to be used for the 2 LTE channels. "
                         "The allowed values for this attributes are the type names "
                         "of any class inheriting from ns3::PropagationLossModel.",
                         StringValue ("ns3::FriisPropagationLossModel"),
                         MakeStringAccessor (&MmWaveHelper::SetLtePathlossModelType),
                         MakeStringChecker ())
          .AddAttribute ("BeamformingModel", "The type of beamforming model to be used.",
                         StringValue ("ns3::MmWaveSvdBeamforming"),
                         MakeStringAccessor (&MmWaveHelper::SetBeamformingModelType),
                         MakeStringChecker ())
          .AddAttribute (
              "UsePdschForCqiGeneration",
              "If true, DL-CQI will be calculated from PDCCH as signal and PDSCH as interference "
              "If false, DL-CQI will be calculated from PDCCH as signal and PDCCH as interference "
              " ",
              BooleanValue (true), MakeBooleanAccessor (&MmWaveHelper::m_usePdschForCqiGeneration),
              MakeBooleanChecker ())
          .AddAttribute ("AnrEnabled",
                         "Activate or deactivate Automatic Neighbour Relation function",
                         BooleanValue (true), MakeBooleanAccessor (&MmWaveHelper::m_isAnrEnabled),
                         MakeBooleanChecker ())
          .AddAttribute ("UseIdealRrc", "Use Ideal or Real RRC", BooleanValue (false),
                         MakeBooleanAccessor (&MmWaveHelper::m_useIdealRrc), MakeBooleanChecker ())
          .AddAttribute ("BasicCellId", "The next value will be the first cellId",
                         UintegerValue (1), MakeUintegerAccessor (&MmWaveHelper::m_cellIdCounter),
                         MakeUintegerChecker<uint16_t> ())
          .AddAttribute ("BasicImsi", "The next value will be the first  imsi", UintegerValue (0),
                         MakeUintegerAccessor (&MmWaveHelper::m_imsiCounter),
                         MakeUintegerChecker<uint16_t> ())
          .AddAttribute ("EnbComponentCarrierManager",
                         "The type of Component Carrier Manager to be used for gNBs. "
                         "The allowed values for this attributes are the type names "
                         "of any class inheriting ns3::LteEnbComponentCarrierManager.",
                         StringValue ("ns3::MmWaveNoOpComponentCarrierManager"),
                         MakeStringAccessor (&MmWaveHelper::SetEnbComponentCarrierManagerType,
                                             &MmWaveHelper::GetEnbComponentCarrierManagerType),
                         MakeStringChecker ())
          .AddAttribute ("UeComponentCarrierManager",
                         "The type of Component Carrier Manager to be used for UEs. "
                         "The allowed values for this attributes are the type names "
                         "of any class inheriting ns3::LteUeComponentCarrierManager.",
                         StringValue ("ns3::SimpleUeComponentCarrierManager"),
                         MakeStringAccessor (&MmWaveHelper::SetUeComponentCarrierManagerType,
                                             &MmWaveHelper::GetUeComponentCarrierManagerType),
                         MakeStringChecker ())
          .AddAttribute ("LteEnbComponentCarrierManager",
                         "The type of Component Carrier Manager to be used for eNBs. "
                         "The allowed values for this attributes are the type names "
                         "of any class inheriting ns3::LteEnbComponentCarrierManager.",
                         StringValue ("ns3::NoOpComponentCarrierManager"),
                         MakeStringAccessor (&MmWaveHelper::SetLteEnbComponentCarrierManagerType,
                                             &MmWaveHelper::GetLteEnbComponentCarrierManagerType),
                         MakeStringChecker ())
          .AddAttribute ("UseCa",
                         "If true, Carrier Aggregation feature is enabled in the mmWave stack and "
                         "a valid Component Carrier Map is expected."
                         "If false, single carrier simulation.",
                         BooleanValue (false), MakeBooleanAccessor (&MmWaveHelper::m_useCa),
                         MakeBooleanChecker ())
          .AddAttribute ("LteUseCa",
                         "If true, Carrier Aggregation feature is enabled in the LTE stack and a "
                         "valid Component Carrier Map is expected."
                         "If false, single carrier simulation.",
                         BooleanValue (false), MakeBooleanAccessor (&MmWaveHelper::m_lteUseCa),
                         MakeBooleanChecker ())
          .AddAttribute ("NumberOfComponentCarriers",
                         "Set the number of mmWave Component Carriers to use "
                         "If it is more than one and m_useCa is false, it will raise an error ",
                         UintegerValue (1), MakeUintegerAccessor (&MmWaveHelper::m_noOfCcs),
                         MakeUintegerChecker<uint16_t> (MIN_NO_MMW_CC, MAX_NO_MMW_CC))
          .AddAttribute ("NumberOfLteComponentCarriers",
                         "Set the number of LTE Component Carriers to use "
                         "If it is more than one and m_lteUseCa is false, it will raise an error ",
                         UintegerValue (1), MakeUintegerAccessor (&MmWaveHelper::m_noOfLteCcs),
                         MakeUintegerChecker<uint16_t> (MIN_NO_CC, MAX_NO_CC));

  return tid;
}

void
MmWaveHelper::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_channel.clear ();
  m_componentCarrierPhyParams.clear ();
  m_lteComponentCarrierPhyParams.clear ();
  Object::DoDispose ();
}

void
MmWaveHelper::DoInitialize ()
{
  NS_LOG_FUNCTION (this);

  // cc initialization
  // if useCa=false and SetCcPhyParams() has not been called, setup a default CC.
  if (!m_useCa && m_componentCarrierPhyParams.size () == 0)
    {
      NS_LOG_INFO ("useCa=false and empty CC map. Create the default CC.");

      // For custom configurations use the method SetCcPhyParams
      Ptr<MmWavePhyMacCommon> phyMacConfig = CreateObject<MmWavePhyMacCommon> ();
      Ptr<MmWaveComponentCarrier> cc = CreateObject<MmWaveComponentCarrier> ();
      cc->SetConfigurationParameters (phyMacConfig);
      cc->SetAsPrimary (true);

      //create the ccMap
      std::map<uint8_t, MmWaveComponentCarrier> map;
      map[0] = *cc;

      this->SetCcPhyParams (map);
    }

  MmWaveChannelModelInitialization (); // channel initialization

  m_phyStats = CreateObject<MmWavePhyTrace> ();
  m_radioBearerStatsConnector = CreateObject<MmWaveBearerStatsConnector> ();
  m_enbStats = CreateObject<MmWaveMacTrace> ();
  m_bapStats = CreateObject<MmWaveBapTrace> ();

  // lte cc initialization
  // if m_lteUseCa=false and SetLteCcPhyParams() has not been called, setup a default LTE CC
  if (!m_lteUseCa && m_lteComponentCarrierPhyParams.size () == 0)
    {
      // create the map of LTE component carriers
      Ptr<CcHelper> lteCcHelper = CreateObject<CcHelper> ();
      lteCcHelper->SetNumberOfComponentCarriers (1);
      lteCcHelper->SetUlEarfcn (18100);
      lteCcHelper->SetDlEarfcn (100);
      lteCcHelper->SetDlBandwidth (100);
      lteCcHelper->SetUlBandwidth (100);
      std::map<uint8_t, ComponentCarrier> lteCcMap = lteCcHelper->EquallySpacedCcs ();
      lteCcMap.at (0).SetAsPrimary (true);
      this->SetLteCcPhyParams (lteCcMap);
    }

  LteChannelModelInitialization (); // lte channel initialization

  m_cnStats = 0; //core network stats calculator

  Object::DoInitialize ();
}

void
MmWaveHelper::MmWaveChannelModelInitialization (void)
{
  NS_LOG_FUNCTION (this);
  // setup of mmWave channel & related
  //create a channel for each CC
  for (std::map<uint8_t, MmWaveComponentCarrier>::iterator it =
           m_componentCarrierPhyParams.begin ();
       it != m_componentCarrierPhyParams.end (); ++it)
    {
      Ptr<SpectrumChannel> channel = m_channelFactory.Create<SpectrumChannel> ();
      Ptr<MmWavePhyMacCommon> phyMacCommon =
          m_componentCarrierPhyParams.at (it->first).GetConfigurationParameters ();

      // create the channel condition model (if needed)
      Ptr<ChannelConditionModel> ccm;
      if (!m_channelConditionModelType.empty ())
        {
          ccm = m_channelConditionModelFactory.Create<ChannelConditionModel> ();
        }

      // create the propagation loss model
      if (!m_pathlossModelType.empty ())
        {
          Ptr<PropagationLossModel> plm = m_pathlossModelFactory.Create<PropagationLossModel> ();
          if (plm)
            {
              plm->SetAttributeFailSafe ("Frequency",
                                         DoubleValue (phyMacCommon->GetCenterFrequency ()));

              // associate the channel condition model to the propagation loss model (if needed)
              if (ccm)
                {
                  plm->SetAttributeFailSafe ("ChannelConditionModel", PointerValue (ccm));
                }

              // set the propagation loss model in the channel
              channel->AddPropagationLossModel (plm);
            }

          // store the propagation loss model
          m_pathlossModel[it->first] = plm;
        }
      else
        {
          NS_LOG_WARN (this << " No PropagationLossModel!");
        }

      // create and configure the SpectrumPropagationLossModel
      if (!m_spectrumPropagationLossModelType.empty ())
        {

          // if the selected model is ThreeGppSpectrumPropagationLossModel we
          // need a special configuration procedure, otherwise, for the other
          // models, we try to configure the frequency

          if (m_spectrumPropagationLossModelType == "ns3::ThreeGppSpectrumPropagationLossModel")
            {
              Ptr<ThreeGppSpectrumPropagationLossModel> threeGppSplm =
                  m_spectrumPropagationLossModelFactory
                      .Create<ThreeGppSpectrumPropagationLossModel> ();
              threeGppSplm->SetChannelModelAttribute (
                  "Frequency", DoubleValue (phyMacCommon->GetCenterFrequency ()));

              // the ThreeGppSpectrumPropagationLossModel must have the same ChannelConditionModel as the
              // propagation loss model instace
              if (ccm) // the channel condition model was created using the factory
                {
                  threeGppSplm->SetChannelModelAttribute ("ChannelConditionModel",
                                                          PointerValue (ccm));
                }
              else if (
                  !m_pathlossModel
                       .empty ()) // the channel condition model was created inside the propagation loss model
                {
                  PointerValue ptr;
                  m_pathlossModel.at (it->first)->GetAttribute ("ChannelConditionModel", ptr);
                  ccm = ptr.Get<ChannelConditionModel> ();
                  threeGppSplm->SetChannelModelAttribute ("ChannelConditionModel",
                                                          PointerValue (ccm));
                }
              else
                {
                  NS_LOG_DEBUG (
                      "ChannelConditionModel not set for ThreeGppSpectrumPropagationLossModel");
                }
              // set the propagation loss model in the channel
              channel->AddPhasedArraySpectrumPropagationLossModel (threeGppSplm);
            }
          else
            {
              Ptr<SpectrumPropagationLossModel> splm =
                  m_spectrumPropagationLossModelFactory.Create<SpectrumPropagationLossModel> ();
              splm->SetAttributeFailSafe ("Frequency",
                                          DoubleValue (phyMacCommon->GetCenterFrequency ()));
              // set the propagation loss model in the channel
              channel->AddSpectrumPropagationLossModel (splm);
            }
        }
      else
        {
          NS_LOG_WARN (this << " No SpectrumPropagationLossModel!");
        }

      m_channel[it->first] = channel;
    } //end for
}

void
MmWaveHelper::LteChannelModelInitialization (void)
{
  NS_LOG_FUNCTION (this);
  // setup of LTE channels & related
  m_downlinkChannel = m_lteChannelFactory.Create<SpectrumChannel> ();
  m_uplinkChannel = m_lteChannelFactory.Create<SpectrumChannel> ();
  m_downlinkPathlossModel = m_dlPathlossModelFactory.Create ();
  Ptr<SpectrumPropagationLossModel> dlSplm =
      m_downlinkPathlossModel->GetObject<SpectrumPropagationLossModel> ();
  if (dlSplm)
    {
      NS_LOG_LOGIC (this << " using a SpectrumPropagationLossModel in DL");
      m_downlinkChannel->AddSpectrumPropagationLossModel (dlSplm);
    }
  else
    {
      NS_LOG_LOGIC (this << " using a PropagationLossModel in DL");
      Ptr<PropagationLossModel> dlPlm = m_downlinkPathlossModel->GetObject<PropagationLossModel> ();
      NS_ASSERT_MSG (dlPlm,
                     " " << m_downlinkPathlossModel
                         << " is neither PropagationLossModel nor SpectrumPropagationLossModel");
      m_downlinkChannel->AddPropagationLossModel (dlPlm);
    }

  m_uplinkPathlossModel = m_ulPathlossModelFactory.Create ();
  Ptr<SpectrumPropagationLossModel> ulSplm =
      m_uplinkPathlossModel->GetObject<SpectrumPropagationLossModel> ();
  if (ulSplm)
    {
      NS_LOG_LOGIC (this << " using a SpectrumPropagationLossModel in UL");
      m_uplinkChannel->AddSpectrumPropagationLossModel (ulSplm);
    }
  else
    {
      NS_LOG_LOGIC (this << " using a PropagationLossModel in UL");
      Ptr<PropagationLossModel> ulPlm = m_uplinkPathlossModel->GetObject<PropagationLossModel> ();
      NS_ASSERT_MSG (ulPlm,
                     " " << m_uplinkPathlossModel
                         << " is neither PropagationLossModel nor SpectrumPropagationLossModel");
      m_uplinkChannel->AddPropagationLossModel (ulPlm);
    }
  // TODO consider if adding LTE fading
  // TODO add mac & phy LTE stats
}

void
MmWaveHelper::SetLtePathlossModelType (std::string type)
{
  NS_LOG_FUNCTION (this << type);
  m_dlPathlossModelFactory = ObjectFactory ();
  m_dlPathlossModelFactory.SetTypeId (type);
  m_ulPathlossModelFactory = ObjectFactory ();
  m_ulPathlossModelFactory.SetTypeId (type);
}

void
MmWaveHelper::SetUeBeamformingCodebookType (std::string type)
{
  NS_LOG_FUNCTION (this << type);
  if (!type.empty ())
    {
      m_ueBeamformingCodebookFactory = ObjectFactory (type);
    }
}

void
MmWaveHelper::SetEnbBeamformingCodebookType (std::string type)
{
  NS_LOG_FUNCTION (this << type);
  if (!type.empty ())
    {
      m_enbBeamformingCodebookFactory = ObjectFactory (type);
    }
}

void
MmWaveHelper::SetBeamformingModelType (std::string type)
{
  NS_LOG_FUNCTION (this << type);
  m_bfModelFactory = ObjectFactory (type);
}

void
MmWaveHelper::SetChannelConditionModelType (std::string type)
{
  NS_LOG_FUNCTION (this << type);
  m_channelConditionModelType = type;
  if (!type.empty ())
    {
      m_channelConditionModelFactory = ObjectFactory ();
      m_channelConditionModelFactory.SetTypeId (type);
    }
}

void
MmWaveHelper::SetPathlossModelType (std::string type)
{
  NS_LOG_FUNCTION (this << type);
  m_pathlossModelType = type;
  if (!type.empty ())
    {
      m_pathlossModelFactory = ObjectFactory ();
      m_pathlossModelFactory.SetTypeId (type);
    }
}

Ptr<PropagationLossModel>
MmWaveHelper::GetPathLossModel (uint8_t index)
{
  NS_LOG_FUNCTION (this << index);
  NS_ASSERT_MSG (m_pathlossModel.find (index) != m_pathlossModel.end (),
                 "Unable to find the requested pathloss model");
  return m_pathlossModel.at (index)->GetObject<PropagationLossModel> ();
}

void
MmWaveHelper::SetChannelModelType (std::string type)
{
  NS_LOG_FUNCTION (this << type);
  m_spectrumPropagationLossModelType = type;
  if (!type.empty ())
    {
      m_spectrumPropagationLossModelFactory = ObjectFactory ();
      m_spectrumPropagationLossModelFactory.SetTypeId (type);
    }
}

void
MmWaveHelper::SetChannelModelAttribute (std::string name, const AttributeValue &value)
{
  NS_LOG_FUNCTION (this);
  m_spectrumPropagationLossModelFactory.Set (name, value);
}

void
MmWaveHelper::SetMmWaveEnbNetDeviceAttribute (std::string name, const AttributeValue &value)
{
  NS_LOG_FUNCTION (this);
  m_enbNetDeviceFactory.Set (name, value);
}

void
MmWaveHelper::SetMmWaveUeNetDeviceAttribute (std::string name, const AttributeValue &value)
{
  NS_LOG_FUNCTION (this);
  m_ueNetDeviceFactory.Set (name, value);
}

void
MmWaveHelper::SetMcUeNetDeviceAttribute (std::string name, const AttributeValue &value)
{
  NS_LOG_FUNCTION (this);
  m_mcUeNetDeviceFactory.Set (name, value);
}

void
MmWaveHelper::SetUeBeamformingCodebookAttribute (std::string name, const AttributeValue &value)
{
  NS_LOG_FUNCTION (this);
  m_ueBeamformingCodebookFactory.Set (name, value);
}

void
MmWaveHelper::SetEnbBeamformingCodebookAttribute (std::string name, const AttributeValue &value)
{
  NS_LOG_FUNCTION (this);
  m_enbBeamformingCodebookFactory.Set (name, value);
}

void
MmWaveHelper::SetIabBeamformingCodebookAttribute (std::string name, const AttributeValue &value)
{
  NS_LOG_FUNCTION (this);
  m_iabBeamformingCodebookFactory.Set (name, value);
}

void
MmWaveHelper::SetBeamformingModelAttribute (std::string name, const AttributeValue &value)
{
  NS_LOG_FUNCTION (this);
  m_bfModelFactory.Set (name, value);
}

void
MmWaveHelper::SetUePhasedArrayModelAttribute (std::string name, const AttributeValue &value)
{
  NS_LOG_FUNCTION (this << name);
  m_uePhasedArrayModelFactory.Set (name, value);
}

void
MmWaveHelper::SetEnbPhasedArrayModelAttribute (std::string name, const AttributeValue &value)
{
  NS_LOG_FUNCTION (this << name);
  m_enbPhasedArrayModelFactory.Set (name, value);
}

void
MmWaveHelper::SetSchedulerType (std::string type)
{
  NS_LOG_FUNCTION (this << type);
  m_schedulerFactory = ObjectFactory ();
  m_schedulerFactory.SetTypeId (type);
}
std::string
MmWaveHelper::GetSchedulerType () const
{
  return m_schedulerFactory.GetTypeId ().GetName ();
}

void
MmWaveHelper::SetLteSchedulerType (std::string type)
{
  NS_LOG_FUNCTION (this << type);
  m_lteSchedulerFactory = ObjectFactory ();
  m_lteSchedulerFactory.SetTypeId (type);
}

std::string
MmWaveHelper::GetLteSchedulerType () const
{
  return m_lteSchedulerFactory.GetTypeId ().GetName ();
}

std::string
MmWaveHelper::GetLteFfrAlgorithmType () const
{
  return m_lteFfrAlgorithmFactory.GetTypeId ().GetName ();
}

void
MmWaveHelper::SetLteFfrAlgorithmType (std::string type)
{
  NS_LOG_FUNCTION (this << type);
  m_lteFfrAlgorithmFactory = ObjectFactory ();
  m_lteFfrAlgorithmFactory.SetTypeId (type);
}

// TODO add attributes

std::string
MmWaveHelper::GetLteHandoverAlgorithmType () const
{
  return m_lteHandoverAlgorithmFactory.GetTypeId ().GetName ();
}

void
MmWaveHelper::SetLteHandoverAlgorithmType (std::string type)
{
  NS_LOG_FUNCTION (this << type);
  m_lteHandoverAlgorithmFactory = ObjectFactory ();
  m_lteHandoverAlgorithmFactory.SetTypeId (type);
}

void
MmWaveHelper::SetUePhasedArrayModelType (std::string type)
{
  NS_LOG_FUNCTION (this << type);
  m_uePhasedArrayModelFactory = ObjectFactory (type);
}

void
MmWaveHelper::SetEnbPhasedArrayModelType (std::string type)
{
  NS_LOG_FUNCTION (this << type);
  m_enbPhasedArrayModelFactory = ObjectFactory (type);
}

void
MmWaveHelper::SetHarqEnabled (bool harqEnabled)
{
  m_harqEnabled = harqEnabled;
}

bool
MmWaveHelper::GetHarqEnabled ()
{
  return m_harqEnabled;
}

void
MmWaveHelper::SetSnrTest (bool snrTest)
{
  m_snrTest = snrTest;
}

bool
MmWaveHelper::GetSnrTest ()
{
  return m_snrTest;
}

void
MmWaveHelper::SetCcPhyParams (std::map<uint8_t, MmWaveComponentCarrier> ccMapParams)
{
  NS_LOG_FUNCTION (this);
  m_componentCarrierPhyParams = ccMapParams;
}

std::map<uint8_t, MmWaveComponentCarrier>
MmWaveHelper::GetCcPhyParams ()
{
  NS_LOG_FUNCTION (this);
  return m_componentCarrierPhyParams;
}

void
MmWaveHelper::SetLteCcPhyParams (std::map<uint8_t, ComponentCarrier> ccMapParams)
{
  NS_LOG_FUNCTION (this);
  m_lteComponentCarrierPhyParams = ccMapParams;
}

NetDeviceContainer
MmWaveHelper::InstallUeDevice (NodeContainer c)
{
  NS_LOG_FUNCTION (this);
  Initialize (); // Run DoInitialize (), if necessary
  NetDeviceContainer devices;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = *i;
      Ptr<NetDevice> device = InstallSingleUeDevice (node);
      device->SetAddress (Mac64Address::Allocate ());
      devices.Add (device);
    }
  return devices;
}

NetDeviceContainer
MmWaveHelper::InstallMcUeDevice (NodeContainer c)
{
  NS_LOG_FUNCTION (this);
  Initialize (); // Run DoInitialize (), if necessary
  NetDeviceContainer devices;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = *i;
      Ptr<NetDevice> device = InstallSingleMcUeDevice (node);
      device->SetAddress (Mac64Address::Allocate ());
      devices.Add (device);
    }
  return devices;
}

NetDeviceContainer
MmWaveHelper::InstallInterRatHoCapableUeDevice (NodeContainer c)
{
  NS_LOG_FUNCTION (this);
  Initialize (); // Run DoInitialize (), if necessary
  NetDeviceContainer devices;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = *i;
      Ptr<NetDevice> device = InstallSingleInterRatHoCapableUeDevice (node);
      device->SetAddress (Mac64Address::Allocate ());
      devices.Add (device);
    }
  return devices;
}

NetDeviceContainer
MmWaveHelper::InstallEnbDevice (NodeContainer c)
{
  NS_LOG_FUNCTION (this);
  Initialize (); // Run DoInitialize (), if necessary
  NetDeviceContainer devices;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = *i;
      Ptr<NetDevice> device = InstallSingleEnbDevice (node);
      device->SetAddress (Mac64Address::Allocate ());
      devices.Add (device);
    }
  return devices;
}

NetDeviceContainer
MmWaveHelper::InstallLteEnbDevice (NodeContainer c)
{
  NS_LOG_FUNCTION (this);
  Initialize (); // Run DoInitialize (), if necessary
  NetDeviceContainer devices;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = *i;
      Ptr<NetDevice> device = InstallSingleLteEnbDevice (node);
      device->SetAddress (Mac64Address::Allocate ());
      devices.Add (device);
    }
  return devices;
}

NetDeviceContainer
MmWaveHelper::InstallIabDevice (NodeContainer c)
{
  NS_LOG_FUNCTION (this);
  Initialize (); // Run DoInitialize (), if necessary
  NetDeviceContainer devices;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = *i;
      Ptr<NetDevice> device = InstallSingleIabDevice (node);
      device->SetAddress (Mac64Address::Allocate ());
      devices.Add (device);
    }
  return devices;
}

Ptr<NetDevice>
MmWaveHelper::InstallSingleMcUeDevice (Ptr<Node> n)
{
  NS_LOG_FUNCTION (this);

  NS_ABORT_MSG_IF (m_imsiCounter >= 0xFFFFFFFF, "max num UEs exceeded");
  uint64_t imsi = ++m_imsiCounter;

  Ptr<McUeNetDevice> device = m_mcUeNetDeviceFactory.Create<McUeNetDevice> ();
  device->SetNode (n);

  // mmWave phy, mac and channel
  NS_ABORT_MSG_IF (
      m_componentCarrierPhyParams.size () == 0 && m_useCa,
      "If CA is enabled, before call this method you need to install Enbs --> InstallEnbDevice()");
  std::map<uint8_t, Ptr<MmWaveComponentCarrierUe>> mmWaveUeCcMap;

  for (std::map<uint8_t, MmWaveComponentCarrier>::iterator it =
           m_componentCarrierPhyParams.begin ();
       it != m_componentCarrierPhyParams.end (); ++it)
    {
      Ptr<MmWaveComponentCarrierUe> cc = CreateObject<MmWaveComponentCarrierUe> ();
      //cc->SetBandwidth ( it->second.GetBandwidth ());
      //cc->SetEarfcn ( it->second.GetUlEarfcn ());
      cc->SetConfigurationParameters (it->second.GetConfigurationParameters ());
      cc->SetAsPrimary (it->second.IsPrimary ());
      Ptr<MmWaveUeMac> mac = CreateObject<MmWaveUeMac> ();
      cc->SetMac (mac);
      // cc->GetPhy ()->Initialize (); // it is initialized within the LteUeNetDevice::DoInitialize ()
      mmWaveUeCcMap.insert (std::pair<uint8_t, Ptr<MmWaveComponentCarrierUe>> (it->first, cc));
    }

  for (std::map<uint8_t, Ptr<MmWaveComponentCarrierUe>>::iterator it = mmWaveUeCcMap.begin ();
       it != mmWaveUeCcMap.end (); ++it)
    {
      Ptr<MmWaveSpectrumPhy> ulPhy = CreateObject<MmWaveSpectrumPhy> ();
      Ptr<MmWaveSpectrumPhy> dlPhy = CreateObject<MmWaveSpectrumPhy> ();

      Ptr<MmWaveUePhy> phy = CreateObject<MmWaveUePhy> (dlPhy, ulPhy);

      Ptr<MmWaveHarqPhy> harq = Create<MmWaveHarqPhy> ();

      dlPhy->SetHarqPhyModule (harq);
      //ulPhy->SetHarqPhyModule (harq);
      phy->SetHarqPhyModule (harq);

      /*
      Ptr<LteChunkProcessor> pRs = Create<LteChunkProcessor> ();
      pRs->AddCallback (MakeCallback (&LteUePhy::ReportRsReceivedPower, phy));
      dlPhy->AddRsPowerChunkProcessor (pRs);

      Ptr<LteChunkProcessor> pInterf = Create<LteChunkProcessor> ();
      pInterf->AddCallback (MakeCallback (&LteUePhy::ReportInterference, phy));
      dlPhy->AddInterferenceCtrlChunkProcessor (pInterf);   // for RSRQ evaluation of UE Measurements

      Ptr<LteChunkProcessor> pCtrl = Create<LteChunkProcessor> ();
      pCtrl->AddCallback (MakeCallback (&LteSpectrumPhy::UpdateSinrPerceived, dlPhy));
      dlPhy->AddCtrlSinrChunkProcessor (pCtrl);
      */

      Ptr<mmWaveChunkProcessor> pData = Create<mmWaveChunkProcessor> ();
      pData->AddCallback (MakeCallback (&MmWaveUePhy::GenerateDlCqiReport, phy));
      pData->AddCallback (MakeCallback (&MmWaveSpectrumPhy::UpdateSinrPerceived, dlPhy));
      dlPhy->AddDataSinrChunkProcessor (pData);
      if (m_harqEnabled)
        {
          //In lte-helper this is done in the last for cycle
          dlPhy->SetPhyDlHarqFeedbackCallback (
              MakeCallback (&MmWaveUePhy::ReceiveLteDlHarqFeedback, phy));
        }

      /*Check if this is supported in mmwave
      if (m_usePdschForCqiGeneration)
      {
        // CQI calculation based on PDCCH for signal and PDSCH for interference
        pCtrl->AddCallback (MakeCallback (&LteUePhy::GenerateMixedCqiReport, phy));
        Ptr<LteChunkProcessor> pDataInterf = Create<LteChunkProcessor> ();
        pDataInterf->AddCallback (MakeCallback (&LteUePhy::ReportDataInterference, phy));
        dlPhy->AddInterferenceDataChunkProcessor (pDataInterf);
      }
      else
      {
        // CQI calculation based on PDCCH for both signal and interference
        pCtrl->AddCallback (MakeCallback (&LteUePhy::GenerateCtrlCqiReport, phy));
      }*/

      ulPhy->SetChannel (m_channel.at (it->first));
      dlPhy->SetChannel (m_channel.at (it->first));

      Ptr<MobilityModel> mm = n->GetObject<MobilityModel> ();
      NS_ASSERT_MSG (
          mm, "MobilityModel needs to be set on node before calling LteHelper::InstallUeDevice ()");
      dlPhy->SetMobility (mm);
      ulPhy->SetMobility (mm);

      Ptr<PhasedArrayModel> antenna = m_uePhasedArrayModelFactory.Create<PhasedArrayModel> ();
      NS_ASSERT_MSG (antenna, "error in creating the AntennaModel object");

      // initialize the 3GPP channel model

      [[maybe_unused]] Ptr<SpectrumPropagationLossModel> splm;
      [[maybe_unused]] Ptr<PhasedArraySpectrumPropagationLossModel> pSplm;

      Ptr<ThreeGppSpectrumPropagationLossModel> threeGppSplm;

      if (m_spectrumPropagationLossModelType == "ns3::ThreeGppSpectrumPropagationLossModel")
        {
          pSplm = m_channel.at (it->first)->GetPhasedArraySpectrumPropagationLossModel ();
          threeGppSplm = DynamicCast<ThreeGppSpectrumPropagationLossModel> (pSplm);
        }
      else
        {
          splm = m_channel.at (it->first)->GetSpectrumPropagationLossModel ();
          threeGppSplm = DynamicCast<ThreeGppSpectrumPropagationLossModel> (splm);
        }

      auto channelModel = threeGppSplm->GetChannelModel ();
      Ptr<MmWaveBeamformingModel> bfModel = m_bfModelFactory.Create<MmWaveBeamformingModel> ();
      bfModel->SetAttributeFailSafe ("Device", PointerValue (device));
      bfModel->SetAttributeFailSafe ("Antenna", PointerValue (antenna));
      bfModel->SetAttributeFailSafe ("ChannelModel", PointerValue (channelModel));
      dlPhy->SetBeamformingModel (bfModel);

      it->second->SetPhy (phy);
      it->second->SetAntenna (antenna);
    }

  // LTE phy, mac and channel
  NS_ABORT_MSG_IF (m_lteComponentCarrierPhyParams.size () == 0 && m_lteUseCa,
                   "If CA is enabled, before call this method you need to install Enbs --> "
                   "InstallLteEnbDevice()");
  std::map<uint8_t, Ptr<ComponentCarrierUe>> lteUeCcMap;

  for (std::map<uint8_t, ComponentCarrier>::iterator it = m_lteComponentCarrierPhyParams.begin ();
       it != m_lteComponentCarrierPhyParams.end (); ++it)
    {
      Ptr<ComponentCarrierUe> cc = CreateObject<ComponentCarrierUe> ();
      cc->SetUlBandwidth (it->second.GetUlBandwidth ());
      cc->SetDlBandwidth (it->second.GetDlBandwidth ());
      cc->SetDlEarfcn (it->second.GetDlEarfcn ());
      cc->SetUlEarfcn (it->second.GetUlEarfcn ());
      cc->SetAsPrimary (it->second.IsPrimary ());
      Ptr<LteUeMac> mac = CreateObject<LteUeMac> ();
      cc->SetMac (mac);
      // cc->GetPhy ()->Initialize (); // it is initialized within the LteUeNetDevice::DoInitialize ()
      lteUeCcMap.insert (std::pair<uint8_t, Ptr<ComponentCarrierUe>> (it->first, cc));
    }

  for (std::map<uint8_t, Ptr<ComponentCarrierUe>>::iterator it = lteUeCcMap.begin ();
       it != lteUeCcMap.end (); ++it)
    {
      Ptr<LteSpectrumPhy> dlPhy = CreateObject<LteSpectrumPhy> ();
      Ptr<LteSpectrumPhy> ulPhy = CreateObject<LteSpectrumPhy> ();

      Ptr<LteUePhy> phy = CreateObject<LteUePhy> (dlPhy, ulPhy);

      Ptr<LteHarqPhy> harq = Create<LteHarqPhy> ();
      dlPhy->SetHarqPhyModule (harq);
      ulPhy->SetHarqPhyModule (harq);
      phy->SetHarqPhyModule (harq);

      Ptr<LteChunkProcessor> pRs = Create<LteChunkProcessor> ();
      pRs->AddCallback (MakeCallback (&LteUePhy::ReportRsReceivedPower, phy));
      dlPhy->AddRsPowerChunkProcessor (pRs);

      Ptr<LteChunkProcessor> pInterf = Create<LteChunkProcessor> ();
      pInterf->AddCallback (MakeCallback (&LteUePhy::ReportInterference, phy));
      dlPhy->AddInterferenceCtrlChunkProcessor (pInterf); // for RSRQ evaluation of UE Measurements

      Ptr<LteChunkProcessor> pCtrl = Create<LteChunkProcessor> ();
      pCtrl->AddCallback (MakeCallback (&LteSpectrumPhy::UpdateSinrPerceived, dlPhy));
      dlPhy->AddCtrlSinrChunkProcessor (pCtrl);

      Ptr<LteChunkProcessor> pData = Create<LteChunkProcessor> ();
      pData->AddCallback (MakeCallback (&LteSpectrumPhy::UpdateSinrPerceived, dlPhy));
      dlPhy->AddDataSinrChunkProcessor (pData);

      if (m_usePdschForCqiGeneration)
        {
          // CQI calculation based on PDCCH for signal and PDSCH for interference
          pCtrl->AddCallback (MakeCallback (&LteUePhy::GenerateMixedCqiReport, phy));
          Ptr<LteChunkProcessor> pDataInterf = Create<LteChunkProcessor> ();
          pDataInterf->AddCallback (MakeCallback (&LteUePhy::ReportDataInterference, phy));
          dlPhy->AddInterferenceDataChunkProcessor (pDataInterf);
        }
      else
        {
          // CQI calculation based on PDCCH for both signal and interference
          pCtrl->AddCallback (MakeCallback (&LteUePhy::GenerateCtrlCqiReport, phy));
        }

      dlPhy->SetChannel (m_downlinkChannel);
      ulPhy->SetChannel (m_uplinkChannel);

      Ptr<MobilityModel> mm = n->GetObject<MobilityModel> ();
      NS_ASSERT_MSG (
          mm, "MobilityModel needs to be set on node before calling LteHelper::InstallUeDevice ()");
      dlPhy->SetMobility (mm);
      ulPhy->SetMobility (mm);

      Ptr<AntennaModel> antenna =
          (m_lteUeAntennaModelFactory.Create ())->GetObject<AntennaModel> ();
      NS_ASSERT_MSG (antenna, "error in creating the AntennaModel object");
      dlPhy->SetAntenna (antenna);
      ulPhy->SetAntenna (antenna);

      it->second->SetPhy (phy);
    }

  // mmWave CCM and RRC
  Ptr<LteUeComponentCarrierManager> mmWaveCcmUe =
      m_ueComponentCarrierManagerFactory.Create<LteUeComponentCarrierManager> ();

  Ptr<LteUeRrc> mmWaveRrc = CreateObject<LteUeRrc> ();
  mmWaveRrc->SetAttribute ("SecondaryRRC", BooleanValue (true));

  mmWaveRrc->SetLteMacSapProvider (mmWaveCcmUe->GetLteMacSapProvider ());
  // setting ComponentCarrierManager SAP
  mmWaveRrc->SetLteCcmRrcSapProvider (mmWaveCcmUe->GetLteCcmRrcSapProvider ());
  mmWaveCcmUe->SetLteCcmRrcSapUser (mmWaveRrc->GetLteCcmRrcSapUser ());
  // Set number of component carriers. Note: UE CCM would also set the
  // number of component carriers in UE RRC
  mmWaveCcmUe->SetNumberOfComponentCarriers (m_noOfCcs);

  // run intializeSap to create the proper number of sap provider/users
  mmWaveRrc->InitializeSap ();

  if (m_useIdealRrc)
    {
      Ptr<MmWaveUeRrcProtocolIdeal> rrcProtocol = CreateObject<MmWaveUeRrcProtocolIdeal> ();
      rrcProtocol->SetUeRrc (mmWaveRrc);
      mmWaveRrc->AggregateObject (rrcProtocol);
      rrcProtocol->SetLteUeRrcSapProvider (mmWaveRrc->GetLteUeRrcSapProvider ());
      mmWaveRrc->SetLteUeRrcSapUser (rrcProtocol->GetLteUeRrcSapUser ());
    }
  else
    {
      Ptr<MmWaveLteUeRrcProtocolReal> rrcProtocol = CreateObject<MmWaveLteUeRrcProtocolReal> ();
      rrcProtocol->SetUeRrc (mmWaveRrc);
      mmWaveRrc->AggregateObject (rrcProtocol);
      rrcProtocol->SetLteUeRrcSapProvider (mmWaveRrc->GetLteUeRrcSapProvider ());
      mmWaveRrc->SetLteUeRrcSapUser (rrcProtocol->GetLteUeRrcSapUser ());
    }

  if (m_epcHelper)
    {
      mmWaveRrc->SetUseRlcSm (false);
    }
  else
    {
      mmWaveRrc->SetUseRlcSm (true);
    }

  // mmWave SAPs
  for (std::map<uint8_t, Ptr<MmWaveComponentCarrierUe>>::iterator it = mmWaveUeCcMap.begin ();
       it != mmWaveUeCcMap.end (); ++it)
    {
      mmWaveRrc->SetLteUeCmacSapProvider (it->second->GetMac ()->GetUeCmacSapProvider (),
                                          it->first);
      it->second->GetMac ()->SetUeCmacSapUser (mmWaveRrc->GetLteUeCmacSapUser (it->first));
      it->second->GetMac ()->SetComponentCarrierId (it->first);

      it->second->GetPhy ()->SetUeCphySapUser (mmWaveRrc->GetLteUeCphySapUser (it->first));
      mmWaveRrc->SetLteUeCphySapProvider (it->second->GetPhy ()->GetUeCphySapProvider (),
                                          it->first);
      it->second->GetPhy ()->SetComponentCarrierId (it->first);

      it->second->GetPhy ()->SetPhySapUser (it->second->GetMac ()->GetPhySapUser ());
      it->second->GetMac ()->SetPhySapProvider (it->second->GetPhy ()->GetPhySapProvider ());

      it->second->GetPhy ()->SetConfigurationParameters (it->second->GetConfigurationParameters ());
      it->second->GetMac ()->SetConfigurationParameters (it->second->GetConfigurationParameters ());

      bool ccmTest = mmWaveCcmUe->SetComponentCarrierMacSapProviders (
          it->first, it->second->GetMac ()->GetUeMacSapProvider ());

      if (ccmTest == false)
        {
          NS_FATAL_ERROR ("Error in SetComponentCarrierMacSapProviders");
        }
    }

  device->SetAttribute ("Imsi", UintegerValue (imsi));
  //device->SetAttribute ("MmWaveUePhy", PointerValue(phy));
  //device->SetAttribute ("MmWaveUeMac", PointerValue(mac));
  device->SetMmWaveCcMap (mmWaveUeCcMap);
  device->SetAttribute ("MmWaveUeRrc", PointerValue (mmWaveRrc));
  device->SetAttribute ("MmWaveUeComponentCarrierManager", PointerValue (mmWaveCcmUe));

  for (std::map<uint8_t, Ptr<MmWaveComponentCarrierUe>>::iterator it = mmWaveUeCcMap.begin ();
       it != mmWaveUeCcMap.end (); ++it)
    {
      Ptr<MmWaveUePhy> ccPhy = it->second->GetPhy ();
      ccPhy->SetDevice (device);
      ccPhy->SetImsi (imsi);
      ccPhy->GetUlSpectrumPhy ()->SetDevice (device);
      ccPhy->GetDlSpectrumPhy ()->SetDevice (device);
      ccPhy->GetDlSpectrumPhy ()->SetPhyRxDataEndOkCallback (
          MakeCallback (&MmWaveUePhy::PhyDataPacketReceived, ccPhy));
      ccPhy->GetDlSpectrumPhy ()->SetPhyRxCtrlEndOkCallback (
          MakeCallback (&MmWaveUePhy::ReceiveControlMessageList, ccPhy));
      //ccPhy->GetDlSpectrumPhy ()->SetLtePhyRxPssCallback (MakeCallback (&LteUePhy::ReceivePss, ccPhy));
      //ccPhy->GetDlSpectrumPhy ()->SetLtePhyDlHarqFeedbackCallback (MakeCallback (&LteUePhy::ReceiveLteDlHarqFeedback, ccPhy)); this is done before
    }

  // LTE CCM and RRC
  Ptr<LteUeComponentCarrierManager> lteCcmUe =
      m_ueComponentCarrierManagerFactory.Create<LteUeComponentCarrierManager> ();

  Ptr<LteUeRrc> lteRrc = CreateObject<LteUeRrc> ();
  lteRrc->m_numberOfMmWaveComponentCarriers = m_noOfCcs;

  lteRrc->SetLteMacSapProvider (lteCcmUe->GetLteMacSapProvider ());
  lteRrc->SetMmWaveMacSapProvider (mmWaveCcmUe->GetLteMacSapProvider ());
  // setting ComponentCarrierManager SAP
  lteRrc->SetLteCcmRrcSapProvider (lteCcmUe->GetLteCcmRrcSapProvider ());
  lteCcmUe->SetLteCcmRrcSapUser (lteRrc->GetLteCcmRrcSapUser ());
  // Set number of component carriers. Note: UE CCM would also set the
  // number of component carriers in UE RRC
  lteCcmUe->SetNumberOfComponentCarriers (m_noOfLteCcs);

  // run intializeSap to create the proper number of sap provider/users
  lteRrc->InitializeSap ();

  if (m_useIdealRrc)
    {
      Ptr<MmWaveUeRrcProtocolIdeal> rrcProtocol = CreateObject<MmWaveUeRrcProtocolIdeal> ();
      rrcProtocol->SetUeRrc (lteRrc);
      lteRrc->AggregateObject (rrcProtocol);
      rrcProtocol->SetLteUeRrcSapProvider (lteRrc->GetLteUeRrcSapProvider ());
      lteRrc->SetLteUeRrcSapUser (rrcProtocol->GetLteUeRrcSapUser ());
    }
  else
    {
      Ptr<MmWaveLteUeRrcProtocolReal> rrcProtocol = CreateObject<MmWaveLteUeRrcProtocolReal> ();
      rrcProtocol->SetUeRrc (lteRrc);
      lteRrc->AggregateObject (rrcProtocol);
      rrcProtocol->SetLteUeRrcSapProvider (lteRrc->GetLteUeRrcSapProvider ());
      lteRrc->SetLteUeRrcSapUser (rrcProtocol->GetLteUeRrcSapUser ());
    }

  if (m_epcHelper)
    {
      lteRrc->SetUseRlcSm (false);
    }

  Ptr<EpcUeNas> lteNas = CreateObject<EpcUeNas> ();

  lteNas->SetAsSapProvider (lteRrc->GetAsSapProvider ());
  lteRrc->SetAsSapUser (lteNas->GetAsSapUser ());
  lteNas->SetMmWaveAsSapProvider (mmWaveRrc->GetAsSapProvider ());
  mmWaveRrc->SetAsSapUser (lteNas->GetAsSapUser ());

  for (std::map<uint8_t, Ptr<ComponentCarrierUe>>::iterator it = lteUeCcMap.begin ();
       it != lteUeCcMap.end (); ++it)
    {
      lteRrc->SetLteUeCmacSapProvider (it->second->GetMac ()->GetLteUeCmacSapProvider (),
                                       it->first);
      it->second->GetMac ()->SetLteUeCmacSapUser (lteRrc->GetLteUeCmacSapUser (it->first));
      it->second->GetMac ()->SetComponentCarrierId (it->first);

      it->second->GetPhy ()->SetLteUeCphySapUser (lteRrc->GetLteUeCphySapUser (it->first));
      lteRrc->SetLteUeCphySapProvider (it->second->GetPhy ()->GetLteUeCphySapProvider (),
                                       it->first);
      it->second->GetPhy ()->SetComponentCarrierId (it->first);

      it->second->GetPhy ()->SetLteUePhySapUser (it->second->GetMac ()->GetLteUePhySapUser ());
      it->second->GetMac ()->SetLteUePhySapProvider (
          it->second->GetPhy ()->GetLteUePhySapProvider ());

      bool ccmTest = lteCcmUe->SetComponentCarrierMacSapProviders (
          it->first, it->second->GetMac ()->GetLteMacSapProvider ());

      if (ccmTest == false)
        {
          NS_FATAL_ERROR ("Error in SetComponentCarrierMacSapProviders");
        }
    }

  for (std::map<uint8_t, Ptr<MmWaveComponentCarrierUe>>::iterator it = mmWaveUeCcMap.begin ();
       it != mmWaveUeCcMap.end (); ++it)
    {
      lteRrc->SetMmWaveUeCmacSapProvider (it->second->GetMac ()->GetUeCmacSapProvider (),
                                          it->first);
    }

  device->SetLteCcMap (lteUeCcMap);
  device->SetAttribute ("LteUeRrc", PointerValue (lteRrc));
  device->SetAttribute ("EpcUeNas", PointerValue (lteNas));
  device->SetAttribute ("LteUeComponentCarrierManager", PointerValue (lteCcmUe));
  device->SetAttribute ("Imsi", UintegerValue (imsi));

  for (std::map<uint8_t, Ptr<ComponentCarrierUe>>::iterator it = lteUeCcMap.begin ();
       it != lteUeCcMap.end (); ++it)
    {
      Ptr<LteUePhy> ccPhy = it->second->GetPhy ();
      ccPhy->SetDevice (device);
      ccPhy->GetUlSpectrumPhy ()->SetDevice (device);
      ccPhy->GetDlSpectrumPhy ()->SetDevice (device);
      ccPhy->GetDlSpectrumPhy ()->SetLtePhyRxDataEndOkCallback (
          MakeCallback (&LteUePhy::PhyPduReceived, ccPhy));
      ccPhy->GetDlSpectrumPhy ()->SetLtePhyRxCtrlEndOkCallback (
          MakeCallback (&LteUePhy::ReceiveLteControlMessageList, ccPhy));
      ccPhy->GetDlSpectrumPhy ()->SetLtePhyRxPssCallback (
          MakeCallback (&LteUePhy::ReceivePss, ccPhy));
      ccPhy->GetDlSpectrumPhy ()->SetLtePhyDlHarqFeedbackCallback (
          MakeCallback (&LteUePhy::ReceiveLteDlHarqFeedback, ccPhy));
    }

  lteNas->SetDevice (device);

  n->AddDevice (device);

  lteNas->SetForwardUpCallback (MakeCallback (&McUeNetDevice::Receive, device));

  if (m_epcHelper)
    {
      m_epcHelper->AddUe (device, device->GetImsi ());
    }

  device->Initialize ();
  return device;
}

Ptr<NetDevice>
MmWaveHelper::InstallSingleInterRatHoCapableUeDevice (Ptr<Node> n)
{
  NS_LOG_FUNCTION (this);

  // Use a McUeNetDevice but install a single RRC
  Ptr<McUeNetDevice> device = m_mcUeNetDeviceFactory.Create<McUeNetDevice> ();
  NS_ABORT_MSG_IF (m_imsiCounter >= 0xFFFFFFFF, "max num UEs exceeded");
  /*
  uint64_t imsi = ++m_imsiCounter;

  // Phy part of MmWave
  Ptr<MmWaveSpectrumPhy> mmWaveUlPhy = CreateObject<MmWaveSpectrumPhy> ();
  Ptr<MmWaveSpectrumPhy> mmWaveDlPhy = CreateObject<MmWaveSpectrumPhy> ();

  Ptr<MmWaveUePhy> mmWavePhy = CreateObject<MmWaveUePhy> (mmWaveDlPhy, mmWaveUlPhy);

  Ptr<MmWaveHarqPhy> mmWaveHarq = Create<MmWaveHarqPhy> (m_phyMacCommon->GetNumHarqProcess ());

  mmWaveDlPhy->SetHarqPhyModule (mmWaveHarq);
  mmWavePhy->SetHarqPhyModule (mmWaveHarq);

  Ptr<mmWaveChunkProcessor> mmWavepData = Create<mmWaveChunkProcessor> ();
  mmWavepData->AddCallback (MakeCallback (&MmWaveUePhy::GenerateDlCqiReport, mmWavePhy));
  mmWavepData->AddCallback (MakeCallback (&MmWaveSpectrumPhy::UpdateSinrPerceived, mmWaveDlPhy));
  mmWaveDlPhy->AddDataSinrChunkProcessor (mmWavepData);
  if(m_harqEnabled)
  {
          mmWaveDlPhy->SetPhyDlHarqFeedbackCallback (MakeCallback (&MmWaveUePhy::ReceiveLteDlHarqFeedback, mmWavePhy));
  }

  // hack to allow periodic computation of SINR at the eNB, without pilots
  if(m_channelModelType == "ns3::MmWaveBeamforming")
  {
          mmWavePhy->AddSpectrumPropagationLossModel(m_beamforming);
  }
  else if(m_channelModelType == "ns3::MmWaveChannelMatrix")
  {
          mmWavePhy->AddSpectrumPropagationLossModel(m_channelMatrix);
  }
  else if(m_channelModelType == "ns3::MmWaveChannelRaytracing")
  {
          mmWavePhy->AddSpectrumPropagationLossModel(m_raytracing);
  }
  if (!m_pathlossModelType.empty ())
  {
          Ptr<PropagationLossModel> splm = m_pathlossModel->GetObject<PropagationLossModel> ();
          if( splm )
          {
                  mmWavePhy->AddPropagationLossModel (splm);
          }
  }
  else
  {
          NS_LOG_UNCOND (this << " No PropagationLossModel!");
  }

  // Phy part of LTE
  Ptr<LteSpectrumPhy> lteDlPhy = CreateObject<LteSpectrumPhy> ();
  Ptr<LteSpectrumPhy> lteUlPhy = CreateObject<LteSpectrumPhy> ();

  Ptr<LteUePhy> ltePhy = CreateObject<LteUePhy> (lteDlPhy, lteUlPhy);

  Ptr<LteHarqPhy> lteHarq = Create<LteHarqPhy> ();
  lteDlPhy->SetHarqPhyModule (lteHarq);
  lteUlPhy->SetHarqPhyModule (lteHarq);
  ltePhy->SetHarqPhyModule (lteHarq);

  Ptr<LteChunkProcessor> pRs = Create<LteChunkProcessor> ();
  pRs->AddCallback (MakeCallback (&LteUePhy::ReportRsReceivedPower, ltePhy));
  lteDlPhy->AddRsPowerChunkProcessor (pRs);

  Ptr<LteChunkProcessor> pInterf = Create<LteChunkProcessor> ();
  pInterf->AddCallback (MakeCallback (&LteUePhy::ReportInterference, ltePhy));
  lteDlPhy->AddInterferenceCtrlChunkProcessor (pInterf); // for RSRQ evaluation of UE Measurements

  Ptr<LteChunkProcessor> pCtrl = Create<LteChunkProcessor> ();
  pCtrl->AddCallback (MakeCallback (&LteSpectrumPhy::UpdateSinrPerceived, lteDlPhy));
  lteDlPhy->AddCtrlSinrChunkProcessor (pCtrl);

  Ptr<LteChunkProcessor> pData = Create<LteChunkProcessor> ();
  pData->AddCallback (MakeCallback (&LteSpectrumPhy::UpdateSinrPerceived, lteDlPhy));
  lteDlPhy->AddDataSinrChunkProcessor (pData);

  if (m_usePdschForCqiGeneration)
  {
          // CQI calculation based on PDCCH for signal and PDSCH for interference
          pCtrl->AddCallback (MakeCallback (&LteUePhy::GenerateMixedCqiReport, ltePhy));
          Ptr<LteChunkProcessor> pDataInterf = Create<LteChunkProcessor> ();
          pDataInterf->AddCallback (MakeCallback (&LteUePhy::ReportDataInterference, ltePhy));
          lteDlPhy->AddInterferenceDataChunkProcessor (pDataInterf);
  }
  else
  {
          // CQI calculation based on PDCCH for both signal and interference
          pCtrl->AddCallback (MakeCallback (&LteUePhy::GenerateCtrlCqiReport, ltePhy));
  }

  // Set MmWave channel
  mmWaveUlPhy->SetChannel(m_channel);
  mmWaveDlPhy->SetChannel(m_channel);
  // Set LTE channel
  lteUlPhy->SetChannel(m_uplinkChannel);
  lteDlPhy->SetChannel(m_downlinkChannel);

  Ptr<MobilityModel> mm = n->GetObject<MobilityModel> ();
  NS_ASSERT_MSG (mm, "MobilityModel needs to be set on node before calling MmWaveHelper::InstallUeDevice ()");
  mmWaveUlPhy->SetMobility(mm);
  mmWaveDlPhy->SetMobility(mm);
  lteUlPhy->SetMobility(mm);
  lteDlPhy->SetMobility(mm);

  // Antenna model for mmWave and for LTE
  Ptr<AntennaModel> antenna = (m_ueAntennaModelFactory.Create ())->GetObject<AntennaModel> ();
  DynamicCast<AntennaArrayModel> (antenna)->SetPlanesNumber(m_noUePanels);
  DynamicCast<AntennaArrayModel> (antenna)->SetDeviceType(true);
  DynamicCast<AntennaArrayModel> (antenna)->SetTotNoArrayElements(m_noRxAntenna);
  NS_ASSERT_MSG (antenna, "error in creating the AntennaModel object");
  mmWaveUlPhy->SetAntenna (antenna);
  mmWaveDlPhy->SetAntenna (antenna);
  antenna = (m_lteUeAntennaModelFactory.Create ())->GetObject<AntennaModel> ();
  lteUlPhy->SetAntenna (antenna);
  lteDlPhy->SetAntenna (antenna);

  // ----------------------- mmWave MAC and connections -------------
  Ptr<MmWaveUeMac> mmWaveMac = CreateObject<MmWaveUeMac> ();

  mmWavePhy->SetConfigurationParameters (m_phyMacCommon);
  mmWaveMac->SetConfigurationParameters (m_phyMacCommon);
  mmWaveMac->SetAttribute("InterRatHoCapable", BooleanValue(true));

  mmWavePhy->SetPhySapUser (mmWaveMac->GetPhySapUser());
  mmWaveMac->SetPhySapProvider (mmWavePhy->GetPhySapProvider());

  device->SetNode(n);
  device->SetAttribute ("MmWaveUePhy", PointerValue(mmWavePhy));
  device->SetAttribute ("MmWaveUeMac", PointerValue(mmWaveMac));

  mmWavePhy->SetDevice (device);
  mmWavePhy->SetImsi (imsi);
  //mmWavePhy->SetForwardUpCallback (MakeCallback (&McUeNetDevice::Receive, device));
  mmWaveDlPhy->SetDevice(device);
  mmWaveUlPhy->SetDevice(device);

  mmWaveDlPhy->SetPhyRxDataEndOkCallback (MakeCallback (&MmWaveUePhy::PhyDataPacketReceived, mmWavePhy));
  mmWaveDlPhy->SetPhyRxCtrlEndOkCallback (MakeCallback (&MmWaveUePhy::ReceiveControlMessageList, mmWavePhy));

  // ----------------------- LTE stack ----------------------
  Ptr<LteUeMac> lteMac = CreateObject<LteUeMac> ();
  Ptr<LteUeRrc> rrc = CreateObject<LteUeRrc> (); //  single rrc

  if (m_useIdealRrc)
  {
          Ptr<MmWaveUeRrcProtocolIdeal> rrcProtocol = CreateObject<MmWaveUeRrcProtocolIdeal> ();
          rrcProtocol->SetUeRrc (rrc);
          rrc->AggregateObject (rrcProtocol);
          rrcProtocol->SetLteUeRrcSapProvider (rrc->GetLteUeRrcSapProvider ());
          rrc->SetLteUeRrcSapUser (rrcProtocol->GetLteUeRrcSapUser ());
  }
  else
  {
          Ptr<MmWaveLteUeRrcProtocolReal> rrcProtocol = CreateObject<MmWaveLteUeRrcProtocolReal> ();
          rrcProtocol->SetUeRrc (rrc);
          rrc->AggregateObject (rrcProtocol);
          rrcProtocol->SetLteUeRrcSapProvider (rrc->GetLteUeRrcSapProvider ());
          rrc->SetLteUeRrcSapUser (rrcProtocol->GetLteUeRrcSapUser ());
  }

  if (m_epcHelper != 0)
  {
          rrc->SetUseRlcSm (false);
  }

  Ptr<EpcUeNas> lteNas = CreateObject<EpcUeNas> ();

  lteNas->SetAsSapProvider (rrc->GetAsSapProvider ());
  rrc->SetAsSapUser (lteNas->GetAsSapUser ());

  // CMAC SAP
  lteMac->SetLteUeCmacSapUser (rrc->GetLteUeCmacSapUser ());
  mmWaveMac->SetUeCmacSapUser (rrc->GetLteUeCmacSapUser ());
  rrc->SetLteUeCmacSapProvider (lteMac->GetLteUeCmacSapProvider ());
  rrc->SetMmWaveUeCmacSapProvider (mmWaveMac->GetUeCmacSapProvider());

  // CPHY SAP
  ltePhy->SetLteUeCphySapUser (rrc->GetLteUeCphySapUser ());
  mmWavePhy->SetUeCphySapUser (rrc->GetLteUeCphySapUser ());
  rrc->SetLteUeCphySapProvider (ltePhy->GetLteUeCphySapProvider ());
  rrc->SetMmWaveUeCphySapProvider (mmWavePhy->GetUeCphySapProvider());

  // MAC SAP
  rrc->SetLteMacSapProvider (lteMac->GetLteMacSapProvider ());
  rrc->SetMmWaveMacSapProvider (mmWaveMac->GetUeMacSapProvider());

  rrc->SetAttribute ("InterRatHoCapable", BooleanValue(true));

  ltePhy->SetLteUePhySapUser (lteMac->GetLteUePhySapUser ());
  lteMac->SetLteUePhySapProvider (ltePhy->GetLteUePhySapProvider ());

  device->SetAttribute ("LteUePhy", PointerValue (ltePhy));
  device->SetAttribute ("LteUeMac", PointerValue (lteMac));
  device->SetAttribute ("LteUeRrc", PointerValue (rrc));
  device->SetAttribute ("EpcUeNas", PointerValue (lteNas));
  device->SetAttribute ("Imsi", UintegerValue(imsi));

  ltePhy->SetDevice (device);
  lteDlPhy->SetDevice (device);
  lteUlPhy->SetDevice (device);
  lteNas->SetDevice (device);

  lteDlPhy->SetLtePhyRxDataEndOkCallback (MakeCallback (&LteUePhy::PhyPduReceived, ltePhy));
  lteDlPhy->SetLtePhyRxCtrlEndOkCallback (MakeCallback (&LteUePhy::ReceiveLteControlMessageList, ltePhy));
  lteDlPhy->SetLtePhyRxPssCallback (MakeCallback (&LteUePhy::ReceivePss, ltePhy));
  lteDlPhy->SetLtePhyDlHarqFeedbackCallback (MakeCallback (&LteUePhy::ReceiveLteDlHarqFeedback, ltePhy));
  lteNas->SetForwardUpCallback (MakeCallback (&McUeNetDevice::Receive, device));

  if (m_epcHelper != 0)
  {
          m_epcHelper->AddUe (device, device->GetImsi ());
  }

  n->AddDevice(device);
  device->Initialize();
  */

  return device;
}

Ptr<NetDevice>
MmWaveHelper::InstallSingleUeDevice (Ptr<Node> n)
{
  NS_LOG_FUNCTION (this);

  NS_ABORT_MSG_IF (m_componentCarrierPhyParams.size () == 0,
                   "Before call this method you need to install Enbs --> InstallEnbDevice()");

  Ptr<MmWaveUeNetDevice> device = m_ueNetDeviceFactory.Create<MmWaveUeNetDevice> ();
  device->SetNode (n);

  std::map<uint8_t, Ptr<MmWaveComponentCarrier>> ueCcMap;
  for (std::map<uint8_t, MmWaveComponentCarrier>::iterator it =
           m_componentCarrierPhyParams.begin ();
       it != m_componentCarrierPhyParams.end (); ++it)
    {
      Ptr<MmWaveComponentCarrierUe> cc = CreateObject<MmWaveComponentCarrierUe> ();
      //cc->SetBandwidth ( it->second.GetBandwidth ());
      //cc->SetEarfcn ( it->second.GetUlEarfcn ());
      cc->SetConfigurationParameters (it->second.GetConfigurationParameters ());
      cc->SetAsPrimary (it->second.IsPrimary ());
      Ptr<MmWaveUeMac> mac = CreateObject<MmWaveUeMac> ();
      cc->SetMac (mac);
      // cc->GetPhy ()->Initialize (); // it is initialized within the LteUeNetDevice::DoInitialize ()
      ueCcMap.insert (std::pair<uint8_t, Ptr<MmWaveComponentCarrier>> (it->first, cc));
    }

  for (auto it = ueCcMap.begin (); it != ueCcMap.end (); ++it)
    {
      Ptr<MmWaveSpectrumPhy> ulPhy = CreateObject<MmWaveSpectrumPhy> ();
      Ptr<MmWaveSpectrumPhy> dlPhy = CreateObject<MmWaveSpectrumPhy> ();

      Ptr<MmWaveUePhy> phy = CreateObject<MmWaveUePhy> (dlPhy, ulPhy);

      Ptr<MmWaveHarqPhy> harq = Create<MmWaveHarqPhy> ();

      dlPhy->SetHarqPhyModule (harq);
      //ulPhy->SetHarqPhyModule (harq);
      phy->SetHarqPhyModule (harq);

      Ptr<mmWaveChunkProcessor> pData = Create<mmWaveChunkProcessor> ();
      pData->AddCallback (MakeCallback (&MmWaveUePhy::GenerateDlCqiReport, phy));
      pData->AddCallback (MakeCallback (&MmWaveSpectrumPhy::UpdateSinrPerceived, dlPhy));
      dlPhy->AddDataSinrChunkProcessor (pData);
      if (m_harqEnabled)
        {
          //In lte-helper this is done in the last for cycle
          dlPhy->SetPhyDlHarqFeedbackCallback (
              MakeCallback (&MmWaveUePhy::ReceiveLteDlHarqFeedback, phy));
        }

      ulPhy->SetChannel (m_channel.at (it->first));
      dlPhy->SetChannel (m_channel.at (it->first));

      Ptr<MobilityModel> mm = n->GetObject<MobilityModel> ();
      NS_ASSERT_MSG (
          mm, "MobilityModel needs to be set on node before calling LteHelper::InstallUeDevice ()");
      dlPhy->SetMobility (mm);
      ulPhy->SetMobility (mm);

      Ptr<PhasedArrayModel> antenna = m_uePhasedArrayModelFactory.Create<PhasedArrayModel> ();
      NS_ASSERT_MSG (antenna, "error in creating the AntennaModel object");

      // initialize the 3GPP channel model

      [[maybe_unused]] Ptr<SpectrumPropagationLossModel> splm;
      [[maybe_unused]] Ptr<PhasedArraySpectrumPropagationLossModel> pSplm;

      Ptr<ThreeGppSpectrumPropagationLossModel> threeGppSplm;

      if (m_spectrumPropagationLossModelType == "ns3::ThreeGppSpectrumPropagationLossModel")
        {
          pSplm = m_channel.at (it->first)->GetPhasedArraySpectrumPropagationLossModel ();
          threeGppSplm = DynamicCast<ThreeGppSpectrumPropagationLossModel> (pSplm);
        }
      else
        {
          splm = m_channel.at (it->first)->GetSpectrumPropagationLossModel ();
          threeGppSplm = DynamicCast<ThreeGppSpectrumPropagationLossModel> (splm);
        }

      auto channelModel = threeGppSplm->GetChannelModel ();

      Ptr<MmWaveBeamformingModel> bfModel = m_bfModelFactory.Create<MmWaveBeamformingModel> ();
      bfModel->SetAttributeFailSafe ("Device", PointerValue (device));
      bfModel->SetAttributeFailSafe ("Antenna", PointerValue (antenna));
      bfModel->SetAttributeFailSafe ("ChannelModel", PointerValue (channelModel));

      if (pSplm)
        {
          bfModel->SetAttributeFailSafe ("PhasedArraySpectrumPropagationLossModel",
                                         PointerValue (pSplm));
        }
      else if (splm)
        {
          bfModel->SetAttributeFailSafe ("SpectrumPropagationLossModel", PointerValue (splm));
        }

      bfModel->SetAttributeFailSafe ("MmWavePhyMacCommon",
                                     PointerValue (it->second->GetConfigurationParameters ()));
      if (m_bfModelFactory.GetTypeId () == MmWaveCodebookBeamforming::GetTypeId ())
        {
          DynamicCast<MmWaveCodebookBeamforming> (bfModel)->SetBeamformingCodebookFactory (
              m_ueBeamformingCodebookFactory);
        }
      bfModel->Initialize ();

      dlPhy->SetBeamformingModel (bfModel);

      DynamicCast<MmWaveComponentCarrierUe> (it->second)->SetPhy (phy);
      it->second->SetAntenna (antenna);
    }

  Ptr<LteUeComponentCarrierManager> ccmUe =
      m_ueComponentCarrierManagerFactory.Create<LteUeComponentCarrierManager> ();

  Ptr<LteUeRrc> rrc = CreateObject<LteUeRrc> ();
  rrc->SetLteMacSapProvider (ccmUe->GetLteMacSapProvider ());
  // setting ComponentCarrierManager SAP
  rrc->SetLteCcmRrcSapProvider (ccmUe->GetLteCcmRrcSapProvider ());
  ccmUe->SetLteCcmRrcSapUser (rrc->GetLteCcmRrcSapUser ());
  // Set number of component carriers. Note: UE CCM would also set the
  // number of component carriers in UE RRC
  ccmUe->SetNumberOfComponentCarriers (m_noOfCcs);

  // run intializeSap to create the proper number of sap provider/users
  rrc->InitializeSap ();

  if (m_useIdealRrc)
    {
      Ptr<MmWaveUeRrcProtocolIdeal> rrcProtocol = CreateObject<MmWaveUeRrcProtocolIdeal> ();
      rrcProtocol->SetUeRrc (rrc);
      rrc->AggregateObject (rrcProtocol);
      rrcProtocol->SetLteUeRrcSapProvider (rrc->GetLteUeRrcSapProvider ());
      rrc->SetLteUeRrcSapUser (rrcProtocol->GetLteUeRrcSapUser ());
    }
  else
    {
      Ptr<MmWaveLteUeRrcProtocolReal> rrcProtocol = CreateObject<MmWaveLteUeRrcProtocolReal> ();
      rrcProtocol->SetUeRrc (rrc);
      rrc->AggregateObject (rrcProtocol);
      rrcProtocol->SetLteUeRrcSapProvider (rrc->GetLteUeRrcSapProvider ());
      rrc->SetLteUeRrcSapUser (rrcProtocol->GetLteUeRrcSapUser ());
    }

  if (m_epcHelper)
    {
      rrc->SetUseRlcSm (false);
    }
  else
    {
      rrc->SetUseRlcSm (true);
    }

  Ptr<EpcUeNas> nas = CreateObject<EpcUeNas> ();

  nas->SetAsSapProvider (rrc->GetAsSapProvider ());
  rrc->SetAsSapUser (nas->GetAsSapUser ());

  for (auto it = ueCcMap.begin (); it != ueCcMap.end (); ++it)
    {
      Ptr<MmWaveComponentCarrierUe> ccUe = DynamicCast<MmWaveComponentCarrierUe> (it->second);
      rrc->SetLteUeCmacSapProvider (ccUe->GetMac ()->GetUeCmacSapProvider (), it->first);
      ccUe->GetMac ()->SetUeCmacSapUser (rrc->GetLteUeCmacSapUser (it->first));
      ccUe->GetMac ()->SetComponentCarrierId (it->first);

      ccUe->GetPhy ()->SetUeCphySapUser (rrc->GetLteUeCphySapUser (it->first));
      rrc->SetLteUeCphySapProvider (ccUe->GetPhy ()->GetUeCphySapProvider (), it->first);
      ccUe->GetPhy ()->SetComponentCarrierId (it->first);

      ccUe->GetPhy ()->SetPhySapUser (ccUe->GetMac ()->GetPhySapUser ());
      ccUe->GetMac ()->SetPhySapProvider (ccUe->GetPhy ()->GetPhySapProvider ());

      ccUe->GetPhy ()->SetConfigurationParameters (ccUe->GetConfigurationParameters ());
      ccUe->GetMac ()->SetConfigurationParameters (ccUe->GetConfigurationParameters ());

      bool ccmTest = ccmUe->SetComponentCarrierMacSapProviders (
          it->first, ccUe->GetMac ()->GetUeMacSapProvider ());

      if (ccmTest == false)
        {
          NS_FATAL_ERROR ("Error in SetComponentCarrierMacSapProviders");
        }
    }

  NS_ABORT_MSG_IF (m_imsiCounter >= 0xFFFFFFFF, "max num UEs exceeded");
  uint64_t imsi = ++m_imsiCounter;

  device->SetAttribute ("Imsi", UintegerValue (imsi));
  //device->SetAttribute ("MmWaveUePhy", PointerValue(phy));
  //device->SetAttribute ("MmWaveUeMac", PointerValue(mac));
  device->SetCcMap (ueCcMap);
  device->SetAttribute ("EpcUeNas", PointerValue (nas));
  device->SetAttribute ("mmWaveUeRrc", PointerValue (rrc));
  device->SetAttribute ("LteUeComponentCarrierManager", PointerValue (ccmUe));

  for (auto it = ueCcMap.begin (); it != ueCcMap.end (); ++it)
    {
      Ptr<MmWaveUePhy> ccPhy = DynamicCast<MmWaveComponentCarrierUe> (it->second)->GetPhy ();
      ccPhy->SetDevice (device);
      ccPhy->SetImsi (imsi);
      ccPhy->GetUlSpectrumPhy ()->SetDevice (device);
      ccPhy->GetDlSpectrumPhy ()->SetDevice (device);
      ccPhy->GetDlSpectrumPhy ()->SetPhyRxDataEndOkCallback (
          MakeCallback (&MmWaveUePhy::PhyDataPacketReceived, ccPhy));
      ccPhy->GetDlSpectrumPhy ()->SetPhyRxCtrlEndOkCallback (
          MakeCallback (&MmWaveUePhy::ReceiveControlMessageList, ccPhy));
      //ccPhy->GetDlSpectrumPhy ()->SetLtePhyRxPssCallback (MakeCallback (&LteUePhy::ReceivePss, ccPhy));
      //ccPhy->GetDlSpectrumPhy ()->SetLtePhyDlHarqFeedbackCallback (MakeCallback (&LteUePhy::ReceiveLteDlHarqFeedback, ccPhy)); this is done before
    }

  nas->SetDevice (device);

  n->AddDevice (device);

  nas->SetForwardUpCallback (MakeCallback (&MmWaveUeNetDevice::Receive, device));

  if (m_epcHelper)
    {
      m_epcHelper->AddUe (device, device->GetImsi ());
    }

  device->Initialize ();

  return device;
}

Ptr<NetDevice>
MmWaveHelper::InstallSingleEnbDevice (Ptr<Node> n)
{
  //NS_ABORT_MSG_IF (m_cellIdCounter == 65535, "max num eNBs exceeded");
  uint16_t cellId = m_cellIdCounter; //TODO remove, eNB has no cellId

  //Before calling InstallEnbDevice:
  //1) create a std::map where the key is index of
  //component carrier starting from 0, where 0 refers to PCC. The value is
  //an instance of MmWaveComponentCarrier which contains the attribute members for the
  //configuration of the phy paramenters
  //2) call SetCcPhyParams
  NS_ASSERT_MSG (m_componentCarrierPhyParams.size () != 0,
                 "Cannot create enb ccm map. Call SetCcPhyParams first.");
  Ptr<MmWaveEnbNetDevice> device = m_enbNetDeviceFactory.Create<MmWaveEnbNetDevice> ();
  device->SetNode (n);

  // create and set the BAP layer
  Ptr<Bap> bap = CreateObject<Bap> ();
  bap->SetLocalAddress (m_bapAddressCounter++);
  bap->SetDonor ();
  //bap->SetDonorNetDevice (device);
  device->SetBap (bap);

  // create component carrier map for this eNb device
  std::map<uint8_t, Ptr<MmWaveComponentCarrier>> ccMap;
  for (auto it = m_componentCarrierPhyParams.begin (); it != m_componentCarrierPhyParams.end ();
       ++it)
    {
      Ptr<MmWaveComponentCarrierEnb> cc = CreateObject<MmWaveComponentCarrierEnb> ();
      cc->SetConfigurationParameters (it->second.GetConfigurationParameters ());
      cc->SetAsPrimary (it->second.IsPrimary ());
      NS_ABORT_MSG_IF (m_cellIdCounter == 65535, "max num cells exceeded");
      cc->SetCellId (m_cellIdCounter++);
      ccMap[it->first] = cc;
    }
  NS_ABORT_MSG_IF (m_useCa && ccMap.size () < 2,
                   "You have to either specify carriers or disable carrier aggregation");
  NS_ASSERT (ccMap.size () == m_noOfCcs);

  for (auto it = ccMap.begin (); it != ccMap.end (); ++it)
    {
      NS_LOG_DEBUG (this << "component carrier map size " << (uint16_t) ccMap.size ());
      Ptr<MmWaveComponentCarrierEnb> ccEnb = DynamicCast<MmWaveComponentCarrierEnb> (it->second);

      Ptr<MmWaveSpectrumPhy> ulPhy = CreateObject<MmWaveSpectrumPhy> ();
      Ptr<MmWaveSpectrumPhy> dlPhy = CreateObject<MmWaveSpectrumPhy> ();

      Ptr<MmWaveEnbPhy> phy = CreateObject<MmWaveEnbPhy> (dlPhy, ulPhy);

      Ptr<MmWaveHarqPhy> harq = Create<MmWaveHarqPhy> ();
      dlPhy->SetHarqPhyModule (harq);
      phy->SetHarqPhyModule (harq);

      Ptr<mmWaveChunkProcessor> pData = Create<mmWaveChunkProcessor> ();
      if (!m_snrTest)
        {
          pData->AddCallback (MakeCallback (&MmWaveEnbPhy::GenerateDataCqiReport, phy));
          pData->AddCallback (MakeCallback (&MmWaveSpectrumPhy::UpdateSinrPerceived, dlPhy));
        }
      dlPhy->AddDataSinrChunkProcessor (pData);

      phy->SetConfigurationParameters (ccEnb->GetConfigurationParameters ());

      ulPhy->SetChannel (m_channel.at (it->first));
      dlPhy->SetChannel (m_channel.at (it->first));

      Ptr<MobilityModel> mm = n->GetObject<MobilityModel> ();
      NS_ASSERT_MSG (
          mm,
          "MobilityModel needs to be set on node before calling MmWaveHelper::InstallEnbDevice ()");
      ulPhy->SetMobility (mm);
      dlPhy->SetMobility (mm);

      // hack to allow periodic computation of SINR at the eNB, without pilots
      if (m_spectrumPropagationLossModelType == "ns3::ThreeGppSpectrumPropagationLossModel")
        {
          phy->AddPhasedArraySpectrumPropagationLossModel (
              m_channel.at (it->first)->GetPhasedArraySpectrumPropagationLossModel ());
        }
      else
        {
          phy->AddSpectrumPropagationLossModel (
              m_channel.at (it->first)->GetSpectrumPropagationLossModel ());
        }

      if (!m_pathlossModelType.empty ())
        {
          Ptr<PropagationLossModel> splm =
              m_pathlossModel.at (it->first)->GetObject<PropagationLossModel> ();
          phy->AddPropagationLossModel (splm);
        }
      else
        {
          NS_LOG_WARN (this << " No PropagationLossModel!");
        }

      NS_LOG_DEBUG ("Create antenna");
      Ptr<PhasedArrayModel> antenna = m_enbPhasedArrayModelFactory.Create<PhasedArrayModel> ();
      NS_ASSERT_MSG (antenna, "error in creating the AntennaModel object");

      // initialize the 3GPP channel model
      [[maybe_unused]] Ptr<SpectrumPropagationLossModel> splm;
      [[maybe_unused]] Ptr<PhasedArraySpectrumPropagationLossModel> pSplm;

      Ptr<ThreeGppSpectrumPropagationLossModel> threeGppSplm;

      if (m_spectrumPropagationLossModelType == "ns3::ThreeGppSpectrumPropagationLossModel")
        {
          pSplm = m_channel.at (it->first)->GetPhasedArraySpectrumPropagationLossModel ();
          threeGppSplm = DynamicCast<ThreeGppSpectrumPropagationLossModel> (pSplm);
        }
      else
        {
          splm = m_channel.at (it->first)->GetSpectrumPropagationLossModel ();
          threeGppSplm = DynamicCast<ThreeGppSpectrumPropagationLossModel> (splm);
        }

      auto channelModel = threeGppSplm->GetChannelModel ();

      Ptr<MmWaveBeamformingModel> bfModel = m_bfModelFactory.Create<MmWaveBeamformingModel> ();
      bfModel->SetAttributeFailSafe ("Device", PointerValue (device));
      bfModel->SetAttributeFailSafe ("Antenna", PointerValue (antenna));
      bfModel->SetAttributeFailSafe ("ChannelModel", PointerValue (channelModel));

      if (pSplm)
        {
          bfModel->SetAttributeFailSafe ("PhasedArraySpectrumPropagationLossModel",
                                         PointerValue (pSplm));
        }
      else if (splm)
        {
          bfModel->SetAttributeFailSafe ("SpectrumPropagationLossModel", PointerValue (splm));
        }

      bfModel->SetAttributeFailSafe ("MmWavePhyMacCommon",
                                     PointerValue (it->second->GetConfigurationParameters ()));
      if (m_bfModelFactory.GetTypeId () == MmWaveCodebookBeamforming::GetTypeId ())
        {
          DynamicCast<MmWaveCodebookBeamforming> (bfModel)->SetBeamformingCodebookFactory (
              m_enbBeamformingCodebookFactory);
        }
      bfModel->Initialize ();

      dlPhy->SetBeamformingModel (bfModel);

      NS_LOG_DEBUG ("Create the mac");
      Ptr<MmWaveEnbMac> mac = CreateObject<MmWaveEnbMac> ();
      mac->SetConfigurationParameters (ccEnb->GetConfigurationParameters ());
      Ptr<MmWaveMacScheduler> sched = m_schedulerFactory.Create<MmWaveMacScheduler> ();

      /*to use the dummy ffrAlgorithm, I changed the bandwidth to 25 in EnbNetDevice
      m_ffrAlgorithmFactory = ObjectFactory ();
      m_ffrAlgorithmFactory.SetTypeId ("ns3::LteFrNoOpAlgorithm");
      Ptr<LteFfrAlgorithm> ffrAlgorithm = m_ffrAlgorithmFactory.Create<LteFfrAlgorithm> ();
      */
      sched->ConfigureCommonParameters (ccEnb->GetConfigurationParameters ());

      /**********************************************************
      //To do later?
      *mac->SetMmWaveMacSchedSapProvider(sched->GetMacSchedSapProvider());
      *sched->SetMacSchedSapUser (mac->GetMmWaveMacSchedSapUser());
      *mac->SetMmWaveMacCschedSapProvider(sched->GetMacCschedSapProvider());
      *sched->SetMacCschedSapUser (mac->GetMmWaveMacCschedSapUser());

      *phy->SetPhySapUser (mac->GetPhySapUser());
      *mac->SetPhySapProvider (phy->GetPhySapProvider());
      *************************************************************/

      ccEnb->SetMac (mac);
      ccEnb->SetMacScheduler (sched);
      ccEnb->SetPhy (phy);
      it->second->SetAntenna (antenna);

      // Check that the error model has been set in a consistent manner
      TypeIdValue dlPhySpectrumEm, ulPhySpectrumEm, tempAmcEm;
      dlPhy->GetAttribute ("ErrorModelType", dlPhySpectrumEm);
      ulPhy->GetAttribute ("ErrorModelType", ulPhySpectrumEm);
      Ptr<MmWaveAmc> tempAmc = CreateObject<MmWaveAmc> (ccEnb->GetConfigurationParameters ());
      tempAmc->GetAttribute ("ErrorModelType", tempAmcEm);
      NS_ASSERT_MSG (
          (dlPhySpectrumEm.Get () == ulPhySpectrumEm.Get ()) &&
              (dlPhySpectrumEm.Get () == tempAmcEm.Get ()),
          "The same error model must be set in the MmWaveSpectrumPhy and MmWaveAmc classes!");
    }

  Ptr<LteEnbRrc> rrc = CreateObject<LteEnbRrc> ();
  Ptr<LteEnbComponentCarrierManager> ccmEnbManager =
      m_enbComponentCarrierManagerFactory.Create<LteEnbComponentCarrierManager> ();

  //ComponentCarrierManager SAP
  rrc->SetLteCcmRrcSapProvider (ccmEnbManager->GetLteCcmRrcSapProvider ());
  ccmEnbManager->SetLteCcmRrcSapUser (rrc->GetLteCcmRrcSapUser ());
  // Set number of component carriers. Note: eNB CCM would also set the
  // number of component carriers in eNB RRC
  ccmEnbManager->SetNumberOfComponentCarriers (m_noOfCcs);

  // create the MmWaveComponentCarrierConf map used for the RRC setup
  std::map<uint8_t, LteEnbRrc::MmWaveComponentCarrierConf> ccConfMap;
  for (auto it = ccMap.begin (); it != ccMap.end (); ++it)
    {
      LteEnbRrc::MmWaveComponentCarrierConf ccConf;
      Ptr<MmWaveComponentCarrierEnb> ccEnb = DynamicCast<MmWaveComponentCarrierEnb> (it->second);

      ccConf.m_ccId = ccEnb->GetConfigurationParameters ()->GetCcId ();
      ccConf.m_cellId = ccEnb->GetCellId ();
      ccConf.m_bandwidth = ccEnb->GetBandwidthInRb ();

      ccConfMap[it->first] = ccConf;
    }
  rrc->ConfigureMmWaveCarriers (ccConfMap);

  std::map<uint8_t, double> bandwidthMap;
  for (auto it = ccMap.begin (); it != ccMap.end (); ++it)
    {
      Ptr<MmWavePhyMacCommon> phyMacConfig = it->second->GetConfigurationParameters ();
      bandwidthMap[it->first] = phyMacConfig->GetBandwidth ();
      NS_LOG_DEBUG ("bandwidth " << +it->first << " = " << bandwidthMap[it->first] / 1e6 << " MHz");
    }

  ccmEnbManager->SetBandwidthMap (bandwidthMap);

  if (m_useIdealRrc)
    {
      Ptr<MmWaveEnbRrcProtocolIdeal> rrcProtocol = CreateObject<MmWaveEnbRrcProtocolIdeal> ();
      rrcProtocol->SetLteEnbRrcSapProvider (rrc->GetLteEnbRrcSapProvider ());
      rrc->SetLteEnbRrcSapUser (rrcProtocol->GetLteEnbRrcSapUser ());
      rrc->AggregateObject (rrcProtocol);
      rrcProtocol->SetCellId (cellId);
    }
  else
    {
      Ptr<MmWaveLteEnbRrcProtocolReal> rrcProtocol = CreateObject<MmWaveLteEnbRrcProtocolReal> ();
      rrcProtocol->SetLteEnbRrcSapProvider (rrc->GetLteEnbRrcSapProvider ());
      rrc->SetLteEnbRrcSapUser (rrcProtocol->GetLteEnbRrcSapUser ());
      rrc->AggregateObject (rrcProtocol);
      rrcProtocol->SetCellId (cellId);
    }

  if (m_epcHelper)
    {
      EnumValue epsBearerToRlcMapping;
      rrc->GetAttribute ("EpsBearerToRlcMapping", epsBearerToRlcMapping);
      // it does not make sense to use RLC/SM when also using the EPC
      if (epsBearerToRlcMapping.Get () == LteEnbRrc::RLC_SM_ALWAYS)
        {
          if (m_rlcAmEnabled)
            {
              rrc->SetAttribute ("EpsBearerToRlcMapping", EnumValue (LteEnbRrc::RLC_AM_ALWAYS));
            }
          else
            {
              rrc->SetAttribute ("EpsBearerToRlcMapping",
                                 EnumValue (LteEnbRrc::RLC_UM_LOWLAT_ALWAYS));
            }
        }
    }

  rrc->SetAttribute ("mmWaveDevice", BooleanValue (true));

  // This RRC attribute is used to connect each new RLC instance with the MAC layer
  // (for function such as TransmitPdu, ReportBufferStatusReport).
  // Since in this new architecture, the component carrier manager acts a proxy, it
  // will have its own LteMacSapProvider interface, RLC will see it as through original MAC
  // interface LteMacSapProvider, but the function call will go now through LteEnbComponentCarrierManager
  // instance that needs to implement functions of this interface, and its task will be to
  // forward these calls to the specific MAC of some of the instances of component carriers. This
  // decision will depend on the specific implementation of the component carrier manager.
  rrc->SetLteMacSapProvider (ccmEnbManager->GetLteMacSapProvider ());

  bool ccmTest;
  for (auto it = ccMap.begin (); it != ccMap.end (); ++it)
    {
      Ptr<MmWaveComponentCarrierEnb> ccEnb = DynamicCast<MmWaveComponentCarrierEnb> (it->second);
      ccEnb->GetPhy ()->SetMmWaveEnbCphySapUser (rrc->GetLteEnbCphySapUser (it->first));
      rrc->SetLteEnbCphySapProvider (ccEnb->GetPhy ()->GetMmWaveEnbCphySapProvider (), it->first);

      rrc->SetLteEnbCmacSapProvider (ccEnb->GetMac ()->GetEnbCmacSapProvider (), it->first);
      ccEnb->GetMac ()->SetEnbCmacSapUser (rrc->GetLteEnbCmacSapUser (it->first));

      ccEnb->GetPhy ()->SetComponentCarrierId (it->first);
      ccEnb->GetMac ()->SetComponentCarrierId (it->first);
      //FFR SAP
      /* not used in mmwave
it->second->GetFfMacScheduler ()->SetLteFfrSapProvider (it->second->GetFfrAlgorithm ()->GetLteFfrSapProvider ());
it->second->GetFfrAlgorithm ()->SetLteFfrSapUser (it->second->GetFfMacScheduler ()->GetLteFfrSapUser ());
rrc->SetLteFfrRrcSapProvider (it->second->GetFfrAlgorithm ()->GetLteFfrRrcSapProvider (), it->first);
it->second->GetFfrAlgorithm ()->SetLteFfrRrcSapUser (rrc->GetLteFfrRrcSapUser (it->first));
//FFR SAP END*/

      // PHY <--> MAC SAP
      ccEnb->GetPhy ()->SetPhySapUser (ccEnb->GetMac ()->GetPhySapUser ());
      ccEnb->GetMac ()->SetPhySapProvider (ccEnb->GetPhy ()->GetPhySapProvider ());
      // PHY */<--> MAC SAP END

      //Scheduler SAP
      ccEnb->GetMac ()->SetMmWaveMacSchedSapProvider (
          ccEnb->GetMacScheduler ()->GetMacSchedSapProvider ());
      ccEnb->GetMac ()->SetMmWaveMacCschedSapProvider (
          ccEnb->GetMacScheduler ()->GetMacCschedSapProvider ());

      ccEnb->GetMacScheduler ()->SetMacSchedSapUser (ccEnb->GetMac ()->GetMmWaveMacSchedSapUser ());
      ccEnb->GetMacScheduler ()->SetMacCschedSapUser (
          ccEnb->GetMac ()->GetMmWaveMacCschedSapUser ());
      // Scheduler SAP END

      ccEnb->GetMac ()->SetLteCcmMacSapUser (ccmEnbManager->GetLteCcmMacSapUser ());
      ccmEnbManager->SetCcmMacSapProviders (it->first,
                                            ccEnb->GetMac ()->GetLteCcmMacSapProvider ());

      // insert the pointer to the LteMacSapProvider interface of the MAC layer of the specific component carrier
      ccmTest =
          ccmEnbManager->SetMacSapProvider (it->first, ccEnb->GetMac ()->GetUeMacSapProvider ());

      if (ccmTest == false)
        {
          NS_FATAL_ERROR ("Error in SetComponentCarrierMacSapProviders");
        }
    }

  device->SetAttribute ("CellId", UintegerValue (cellId));
  device->SetAttribute ("LteEnbComponentCarrierManager", PointerValue (ccmEnbManager));
  device->SetCcMap (ccMap);

  //device->SetAttribute ("MmWaveEnbPhy", PointerValue (phy));
  //device->SetAttribute ("MmWaveEnbMac", PointerValue (mac));
  //device->SetAttribute ("mmWaveScheduler", PointerValue(sched));
  device->SetAttribute ("LteEnbRrc", PointerValue (rrc));

  for (auto it = ccMap.begin (); it != ccMap.end (); ++it)
    {
      Ptr<MmWaveEnbPhy> ccPhy = DynamicCast<MmWaveComponentCarrierEnb> (it->second)->GetPhy ();
      ccPhy->SetDevice (device);
      ccPhy->GetUlSpectrumPhy ()->SetDevice (device);
      ccPhy->GetDlSpectrumPhy ()->SetDevice (device);
      ccPhy->GetDlSpectrumPhy ()->SetPhyRxDataEndOkCallback (
          MakeCallback (&MmWaveEnbPhy::PhyDataPacketReceived, ccPhy));
      ccPhy->GetDlSpectrumPhy ()->SetPhyRxCtrlEndOkCallback (
          MakeCallback (&MmWaveEnbPhy::PhyCtrlMessagesReceived, ccPhy));
      ccPhy->GetDlSpectrumPhy ()->SetPhyUlHarqFeedbackCallback (
          MakeCallback (&MmWaveEnbPhy::ReceiveUlHarqFeedback, ccPhy));
      NS_LOG_LOGIC ("set the propagation model frequencies");
    } //end for

  //mac->SetForwardUpCallback (MakeCallback (&MmWaveEnbNetDevice::Receive, device));
  rrc->SetForwardUpCallback (MakeCallback (&MmWaveEnbNetDevice::Receive, device));
  rrc->SetBap (bap);

  device->Initialize ();
  n->AddDevice (device);

  for (auto it = ccMap.begin (); it != ccMap.end (); ++it)
    {
      //m_channel->AddRx (dlPhy); substitute
      m_channel.at (it->first)->AddRx (
          DynamicCast<MmWaveComponentCarrierEnb> (it->second)
              ->GetPhy ()
              ->GetDlSpectrumPhy ()); //TODO check if Dl and Ul are the same
    }

  if (m_epcHelper)
    {
      NS_LOG_INFO ("adding this eNB to the EPC");
      m_epcHelper->AddDu (n, device, device->GetCellId ());
      Ptr<EpcEnbApplication> enbApp = n->GetApplication (0)->GetObject<EpcEnbApplication> ();
      NS_ASSERT_MSG (enbApp, "cannot retrieve EpcEnbApplication");

      rrc->SetBapAddressFromImsiCallback (
          MakeCallback (&EpcEnbApplication::GetBapAddressFromImsi, enbApp));

      for (auto it = ccMap.begin (); it != ccMap.end (); ++it)
        {
          Ptr<MmWaveComponentCarrierEnb> ccEnb =
              DynamicCast<MmWaveComponentCarrierEnb> (it->second);
          ccEnb->GetMacScheduler ()->SetDepthCallback (
              MakeCallback (&EpcEnbApplication::GetIabNodeDepth, enbApp));
          ccEnb->GetMacScheduler ()->SetCellId (device->GetCellId ());
          ccEnb->GetMac ()->SetCellId (device->GetCellId ());
        }

      bap->SetEpcEnbApplication (enbApp);

      // S1 SAPs
      rrc->SetS1SapProvider (enbApp->GetS1SapProvider ());
      enbApp->SetS1SapUser (rrc->GetS1SapUser ());

      // X2 SAPs
      Ptr<EpcX2> x2 = n->GetObject<EpcX2> ();
      x2->SetEpcX2SapUser (rrc->GetEpcX2SapUser ());
      rrc->SetEpcX2SapProvider (x2->GetEpcX2SapProvider ());
      rrc->SetEpcX2RlcProvider (x2->GetEpcX2RlcProvider ());
    }

  return device;
}

Ptr<NetDevice>
MmWaveHelper::InstallSingleIabDevice (Ptr<Node> n)
{
  //NS_ABORT_MSG_IF (m_cellIdCounter == 65535, "max num eNBs exceeded");
  uint16_t cellId = m_cellIdCounter;

  //Before calling InstallEnbDevice:
  //1) create a std::map where the key is index of
  //component carrier starting from 0, where 0 refers to PCC. The value is
  //an instance of MmWaveComponentCarrier which contains the attribute members for the
  //configuration of the phy paramenters
  //2) call SetCcPhyParams
  NS_ASSERT_MSG (m_componentCarrierPhyParams.size () != 0,
                 "Cannot create enb ccm map. Call SetCcPhyParams first.");
  Ptr<MmWaveIabNetDevice> iabDevice = m_iabNetDeviceFactory.Create<MmWaveIabNetDevice> ();
  iabDevice->SetNode (n);

  // create and set the BAP layer
  Ptr<Bap> bap = CreateObject<Bap> ();
  bap->SetLocalAddress (m_bapAddressCounter++);
  iabDevice->SetBap (bap);

  // create component carrier map for this DU
  std::map<uint8_t, Ptr<MmWaveComponentCarrier>> ccDuMap;
  for (auto it = m_componentCarrierPhyParams.begin (); it != m_componentCarrierPhyParams.end ();
       ++it)
    {
      Ptr<MmWaveComponentCarrierEnb> cc = CreateObject<MmWaveComponentCarrierEnb> ();
      cc->SetConfigurationParameters (it->second.GetConfigurationParameters ());
      cc->SetAsPrimary (it->second.IsPrimary ());
      NS_ABORT_MSG_IF (m_cellIdCounter == 65535, "max num cells exceeded");
      cc->SetCellId (m_cellIdCounter++);
      ccDuMap[it->first] = cc;
    }
  NS_ABORT_MSG_IF (m_useCa && ccDuMap.size () < 2,
                   "You have to either specify carriers or disable carrier aggregation");
  NS_ASSERT (ccDuMap.size () == m_noOfCcs);

  std::map<uint8_t, Ptr<MmWaveComponentCarrier>> ccMtMap;
  for (auto it = m_componentCarrierPhyParams.begin (); it != m_componentCarrierPhyParams.end ();
       ++it)
    {
      Ptr<MmWaveComponentCarrierUe> cc = CreateObject<MmWaveComponentCarrierUe> ();
      cc->SetConfigurationParameters (it->second.GetConfigurationParameters ());
      cc->SetAsPrimary (it->second.IsPrimary ());
      Ptr<MmWaveUeMac> mac = CreateObject<MmWaveUeMac> ();
      cc->SetMac (mac);
      ccMtMap.insert (std::pair<uint8_t, Ptr<MmWaveComponentCarrier>> (it->first, cc));
    }

  std::set<uint8_t> ccIds;
  // retrieve cc ids
  for (auto it = m_componentCarrierPhyParams.begin (); it != m_componentCarrierPhyParams.end ();
       ++it)
    {
      ccIds.insert (it->first);
    }

  NS_ASSERT (ccMtMap.size () == ccDuMap.size ());

  for (auto ccId : ccIds)
    {
      NS_LOG_DEBUG (this << "component carrier map size " << (uint16_t) ccDuMap.size ());
      Ptr<MmWaveComponentCarrierEnb> ccDu = DynamicCast<MmWaveComponentCarrierEnb> (ccDuMap[ccId]);

      Ptr<MmWaveSpectrumPhy> ulDuPhy = CreateObject<MmWaveSpectrumPhy> ();
      Ptr<MmWaveSpectrumPhy> dlDuPhy = CreateObject<MmWaveSpectrumPhy> ();

      Ptr<MmWaveEnbPhy> duPhy = CreateObject<MmWaveEnbPhy> (dlDuPhy, ulDuPhy);

      Ptr<MmWaveHarqPhy> duHarq = Create<MmWaveHarqPhy> ();
      dlDuPhy->SetHarqPhyModule (duHarq);
      duPhy->SetHarqPhyModule (duHarq);

      Ptr<MmWaveSpectrumPhy> ulMtPhy = CreateObject<MmWaveSpectrumPhy> ();
      Ptr<MmWaveSpectrumPhy> dlMtPhy = CreateObject<MmWaveSpectrumPhy> ();

      Ptr<MmWaveUePhy> mtPhy = CreateObject<MmWaveUePhy> (dlMtPhy, ulMtPhy);

      Ptr<MmWaveHarqPhy> mtHarq = Create<MmWaveHarqPhy> ();

      dlMtPhy->SetHarqPhyModule (mtHarq);
      mtPhy->SetHarqPhyModule (mtHarq);

      Ptr<mmWaveChunkProcessor> pDuData = Create<mmWaveChunkProcessor> ();
      if (!m_snrTest)
        {
          pDuData->AddCallback (MakeCallback (&MmWaveEnbPhy::GenerateDataCqiReport, duPhy));
          pDuData->AddCallback (MakeCallback (&MmWaveSpectrumPhy::UpdateSinrPerceived, dlDuPhy));
        }
      dlDuPhy->AddDataSinrChunkProcessor (pDuData);

      Ptr<mmWaveChunkProcessor> pMtData = Create<mmWaveChunkProcessor> ();
      pMtData->AddCallback (MakeCallback (&MmWaveUePhy::GenerateDlCqiReport, mtPhy));
      pMtData->AddCallback (MakeCallback (&MmWaveSpectrumPhy::UpdateSinrPerceived, dlMtPhy));
      dlMtPhy->AddDataSinrChunkProcessor (pMtData);
      if (m_harqEnabled)
        {
          //In lte-helper this is done in the last for cycle
          dlMtPhy->SetPhyDlHarqFeedbackCallback (
              MakeCallback (&MmWaveUePhy::ReceiveLteDlHarqFeedback, mtPhy));
        }

      duPhy->SetConfigurationParameters (ccDu->GetConfigurationParameters ());

      ulDuPhy->SetChannel (m_channel.at (ccId));
      dlDuPhy->SetChannel (m_channel.at (ccId));

      ulMtPhy->SetChannel (m_channel.at (ccId));
      dlMtPhy->SetChannel (m_channel.at (ccId));

      Ptr<MobilityModel> mm = n->GetObject<MobilityModel> ();
      NS_ASSERT_MSG (
          mm,
          "MobilityModel needs to be set on node before calling MmWaveHelper::InstallEnbDevice ()");
      ulDuPhy->SetMobility (mm);
      dlDuPhy->SetMobility (mm);

      dlMtPhy->SetMobility (mm);
      ulMtPhy->SetMobility (mm);

      // hack to allow periodic computation of SINR at the eNB, without pilots
      if (m_spectrumPropagationLossModelType == "ns3::ThreeGppSpectrumPropagationLossModel")
        {
          duPhy->AddPhasedArraySpectrumPropagationLossModel (
              m_channel.at (ccId)->GetPhasedArraySpectrumPropagationLossModel ());
        }
      else
        {
          duPhy->AddSpectrumPropagationLossModel (
              m_channel.at (ccId)->GetSpectrumPropagationLossModel ());
        }

      if (!m_pathlossModelType.empty ())
        {
          Ptr<PropagationLossModel> duPlm =
              m_pathlossModel.at (ccId)->GetObject<PropagationLossModel> ();
          duPhy->AddPropagationLossModel (duPlm);
        }
      else
        {
          NS_LOG_WARN (this << " No PropagationLossModel!");
        }

      NS_LOG_DEBUG ("Create antenna");
      Ptr<PhasedArrayModel> duAntenna = m_iabPhasedArrayModelFactory.Create<PhasedArrayModel> ();
      NS_ASSERT_MSG (duAntenna, "error in creating the AntennaModel object");

      // initialize the 3GPP channel model
      [[maybe_unused]] Ptr<SpectrumPropagationLossModel> duSplm;
      [[maybe_unused]] Ptr<PhasedArraySpectrumPropagationLossModel> duPSplm;

      Ptr<ThreeGppSpectrumPropagationLossModel> duThreeGppSplm;

      if (m_spectrumPropagationLossModelType == "ns3::ThreeGppSpectrumPropagationLossModel")
        {
          duPSplm = m_channel.at (ccId)->GetPhasedArraySpectrumPropagationLossModel ();
          duThreeGppSplm = DynamicCast<ThreeGppSpectrumPropagationLossModel> (duPSplm);
        }
      else
        {
          duSplm = m_channel.at (ccId)->GetSpectrumPropagationLossModel ();
          duThreeGppSplm = DynamicCast<ThreeGppSpectrumPropagationLossModel> (duSplm);
        }

      Ptr<PhasedArrayModel> mtAntenna = m_iabPhasedArrayModelFactory.Create<PhasedArrayModel> ();
      NS_ASSERT_MSG (mtAntenna, "error in creating the AntennaModel object");

      [[maybe_unused]] Ptr<SpectrumPropagationLossModel> mtSplm;
      [[maybe_unused]] Ptr<PhasedArraySpectrumPropagationLossModel> mtPSplm;

      Ptr<ThreeGppSpectrumPropagationLossModel> mtThreeGppSplm;

      if (m_spectrumPropagationLossModelType == "ns3::ThreeGppSpectrumPropagationLossModel")
        {
          mtPSplm = m_channel.at (ccId)->GetPhasedArraySpectrumPropagationLossModel ();
          mtThreeGppSplm = DynamicCast<ThreeGppSpectrumPropagationLossModel> (mtPSplm);
        }
      else
        {
          mtSplm = m_channel.at (ccId)->GetSpectrumPropagationLossModel ();
          mtThreeGppSplm = DynamicCast<ThreeGppSpectrumPropagationLossModel> (mtSplm);
        }

      auto duChannelModel = duThreeGppSplm->GetChannelModel ();

      Ptr<MmWaveBeamformingModel> duBfModel = m_bfModelFactory.Create<MmWaveBeamformingModel> ();
      duBfModel->SetAttributeFailSafe ("Device", PointerValue (iabDevice));
      duBfModel->SetAttributeFailSafe ("Antenna", PointerValue (duAntenna));
      duBfModel->SetAttributeFailSafe ("ChannelModel", PointerValue (duChannelModel));

      if (duPSplm)
        {
          duBfModel->SetAttributeFailSafe ("PhasedArraySpectrumPropagationLossModel",
                                           PointerValue (duPSplm));
        }
      else if (duSplm)
        {
          duBfModel->SetAttributeFailSafe ("SpectrumPropagationLossModel", PointerValue (duSplm));
        }

      duBfModel->SetAttributeFailSafe ("MmWavePhyMacCommon",
                                       PointerValue (ccDuMap[ccId]->GetConfigurationParameters ()));
      if (m_bfModelFactory.GetTypeId () == MmWaveCodebookBeamforming::GetTypeId ())
        {
          DynamicCast<MmWaveCodebookBeamforming> (duBfModel)->SetBeamformingCodebookFactory (
              m_iabBeamformingCodebookFactory);
        }
      duBfModel->Initialize ();

      dlDuPhy->SetBeamformingModel (duBfModel);

      auto mtChannelModel = mtThreeGppSplm->GetChannelModel ();

      Ptr<MmWaveBeamformingModel> mtBfModel = m_bfModelFactory.Create<MmWaveBeamformingModel> ();
      mtBfModel->SetAttributeFailSafe ("Device", PointerValue (iabDevice));
      mtBfModel->SetAttributeFailSafe ("Antenna", PointerValue (mtAntenna));
      mtBfModel->SetAttributeFailSafe ("ChannelModel", PointerValue (mtChannelModel));

      if (mtPSplm)
        {
          mtBfModel->SetAttributeFailSafe ("PhasedArraySpectrumPropagationLossModel",
                                           PointerValue (mtPSplm));
        }
      else if (mtSplm)
        {
          mtBfModel->SetAttributeFailSafe ("SpectrumPropagationLossModel", PointerValue (mtSplm));
        }

      mtBfModel->SetAttributeFailSafe ("MmWavePhyMacCommon",
                                       PointerValue (ccMtMap[ccId]->GetConfigurationParameters ()));
      if (m_bfModelFactory.GetTypeId () == MmWaveCodebookBeamforming::GetTypeId ())
        {
          DynamicCast<MmWaveCodebookBeamforming> (mtBfModel)->SetBeamformingCodebookFactory (
              m_iabBeamformingCodebookFactory);
        }
      mtBfModel->Initialize ();

      dlMtPhy->SetBeamformingModel (mtBfModel);

      NS_LOG_DEBUG ("Create the mac");
      Ptr<MmWaveEnbMac> duMac = CreateObject<MmWaveEnbMac> ();
      duMac->SetCellId (ccDu->GetCellId ());
      duMac->SetConfigurationParameters (ccDu->GetConfigurationParameters ());
      Ptr<MmWaveMacScheduler> duSched = m_schedulerFactory.Create<MmWaveMacScheduler> ();

      duSched->ConfigureCommonParameters (ccDu->GetConfigurationParameters ());

      ccDu->SetMac (duMac);
      ccDu->SetMacScheduler (duSched);
      ccDu->SetPhy (duPhy);
      ccDuMap[ccId]->SetAntenna (duAntenna);

      DynamicCast<MmWaveComponentCarrierUe> (ccMtMap[ccId])->SetPhy (mtPhy);
      ccMtMap[ccId]->SetAntenna (mtAntenna);

      // Check that the error model has been set in a consistent manner
      TypeIdValue dlDuPhySpectrumEm, ulDuPhySpectrumEm, tempDuAmcEm;
      dlDuPhy->GetAttribute ("ErrorModelType", dlDuPhySpectrumEm);
      ulDuPhy->GetAttribute ("ErrorModelType", ulDuPhySpectrumEm);
      Ptr<MmWaveAmc> tempAmc = CreateObject<MmWaveAmc> (ccDu->GetConfigurationParameters ());
      tempAmc->GetAttribute ("ErrorModelType", tempDuAmcEm);
      NS_ASSERT_MSG (
          (dlDuPhySpectrumEm.Get () == ulDuPhySpectrumEm.Get ()) &&
              (dlDuPhySpectrumEm.Get () == tempDuAmcEm.Get ()),
          "The same error model must be set in the MmWaveSpectrumPhy and MmWaveAmc classes!");
    }

  Ptr<LteEnbRrc> duRrc = CreateObject<LteEnbRrc> ();
  Ptr<LteEnbComponentCarrierManager> ccmDuManager =
      m_enbComponentCarrierManagerFactory.Create<LteEnbComponentCarrierManager> ();

  //ComponentCarrierManager SAP
  duRrc->SetLteCcmRrcSapProvider (ccmDuManager->GetLteCcmRrcSapProvider ());
  ccmDuManager->SetLteCcmRrcSapUser (duRrc->GetLteCcmRrcSapUser ());
  // Set number of component carriers. Note: eNB CCM would also set the
  // number of component carriers in eNB RRC
  ccmDuManager->SetNumberOfComponentCarriers (m_noOfCcs);

  Ptr<LteUeRrc> mtRrc = CreateObject<LteUeRrc> ();
  Ptr<LteUeComponentCarrierManager> ccmMtManager =
      m_ueComponentCarrierManagerFactory.Create<LteUeComponentCarrierManager> ();

  mtRrc->SetLteMacSapProvider (ccmMtManager->GetLteMacSapProvider ());
  // setting ComponentCarrierManager SAP
  mtRrc->SetLteCcmRrcSapProvider (ccmMtManager->GetLteCcmRrcSapProvider ());
  ccmMtManager->SetLteCcmRrcSapUser (mtRrc->GetLteCcmRrcSapUser ());
  // Set number of component carriers. Note: UE CCM would also set the
  // number of component carriers in UE RRC
  ccmMtManager->SetNumberOfComponentCarriers (m_noOfCcs);

  // run intializeSap to create the proper number of sap provider/users
  mtRrc->InitializeSap ();

  mtRrc->SetBap (bap);
  bap->SetMtRrc (mtRrc);

  // create the MmWaveComponentCarrierConf map used for the RRC setup
  std::map<uint8_t, LteEnbRrc::MmWaveComponentCarrierConf> ccConfMap;
  for (auto it = ccDuMap.begin (); it != ccDuMap.end (); ++it)
    {
      LteEnbRrc::MmWaveComponentCarrierConf ccConf;
      Ptr<MmWaveComponentCarrierEnb> ccDu = DynamicCast<MmWaveComponentCarrierEnb> (it->second);

      ccConf.m_ccId = ccDu->GetConfigurationParameters ()->GetCcId ();
      ccConf.m_cellId = ccDu->GetCellId ();
      ccConf.m_bandwidth = ccDu->GetBandwidthInRb ();

      ccConfMap[it->first] = ccConf;
    }
  duRrc->ConfigureMmWaveCarriers (ccConfMap);

  duRrc->SetBap (bap);

  std::map<uint8_t, double> duBandwidthMap;
  for (auto it = ccDuMap.begin (); it != ccDuMap.end (); ++it)
    {
      Ptr<MmWavePhyMacCommon> phyDuMacConfig = it->second->GetConfigurationParameters ();
      duBandwidthMap[it->first] = phyDuMacConfig->GetBandwidth ();
      NS_LOG_DEBUG ("bandwidth " << +it->first << " = " << duBandwidthMap[it->first] / 1e6
                                 << " MHz");
    }

  ccmDuManager->SetBandwidthMap (duBandwidthMap);

  if (m_useIdealRrc)
    {
      Ptr<MmWaveEnbRrcProtocolIdeal> duRrcProtocol = CreateObject<MmWaveEnbRrcProtocolIdeal> ();
      duRrcProtocol->SetLteEnbRrcSapProvider (duRrc->GetLteEnbRrcSapProvider ());
      duRrc->SetLteEnbRrcSapUser (duRrcProtocol->GetLteEnbRrcSapUser ());
      duRrc->AggregateObject (duRrcProtocol);
      duRrcProtocol->SetCellId (cellId);
    }
  else
    {
      Ptr<MmWaveLteEnbRrcProtocolReal> duRrcProtocol = CreateObject<MmWaveLteEnbRrcProtocolReal> ();
      duRrcProtocol->SetLteEnbRrcSapProvider (duRrc->GetLteEnbRrcSapProvider ());
      duRrc->SetLteEnbRrcSapUser (duRrcProtocol->GetLteEnbRrcSapUser ());
      duRrc->AggregateObject (duRrcProtocol);
      duRrcProtocol->SetCellId (cellId);
    }

  if (m_epcHelper)
    {
      auto p2pEpcHelper = DynamicCast<MmWavePointToPointEpcHelper> (m_epcHelper);
      if (p2pEpcHelper)
        {
          auto sgwPgwApp = p2pEpcHelper->GetEpcSgwPgwApplication ();
          duRrc->SetDonorIpAddressCallback (
              MakeCallback (&EpcSgwPgwApplication::GetDonorIpAddress, sgwPgwApp));
          bap->SetDonorBapAddressCallback (
              MakeCallback (&EpcSgwPgwApplication::GetDonorBapAddress, sgwPgwApp));
        }

      EnumValue epsBearerToRlcMapping;
      duRrc->GetAttribute ("EpsBearerToRlcMapping", epsBearerToRlcMapping);
      // it does not make sense to use RLC/SM when also using the EPC
      if (epsBearerToRlcMapping.Get () == LteEnbRrc::RLC_SM_ALWAYS)
        {
          if (m_rlcAmEnabled)
            {
              duRrc->SetAttribute ("EpsBearerToRlcMapping", EnumValue (LteEnbRrc::RLC_AM_ALWAYS));
            }
          else
            {
              duRrc->SetAttribute ("EpsBearerToRlcMapping",
                                   EnumValue (LteEnbRrc::RLC_UM_LOWLAT_ALWAYS));
            }
        }
    }

  if (m_useIdealRrc)
    {
      Ptr<MmWaveUeRrcProtocolIdeal> mtRrcProtocol = CreateObject<MmWaveUeRrcProtocolIdeal> ();
      mtRrcProtocol->SetUeRrc (mtRrc);
      mtRrc->AggregateObject (mtRrcProtocol);
      mtRrcProtocol->SetLteUeRrcSapProvider (mtRrc->GetLteUeRrcSapProvider ());
      mtRrc->SetLteUeRrcSapUser (mtRrcProtocol->GetLteUeRrcSapUser ());
    }
  else
    {
      Ptr<MmWaveLteUeRrcProtocolReal> mtRrcProtocol = CreateObject<MmWaveLteUeRrcProtocolReal> ();
      mtRrcProtocol->SetUeRrc (mtRrc);
      mtRrc->AggregateObject (mtRrcProtocol);
      mtRrcProtocol->SetLteUeRrcSapProvider (mtRrc->GetLteUeRrcSapProvider ());
      mtRrc->SetLteUeRrcSapUser (mtRrcProtocol->GetLteUeRrcSapUser ());
    }

  if (m_epcHelper)
    {
      mtRrc->SetUseRlcSm (false);
    }
  else
    {
      mtRrc->SetUseRlcSm (true);
    }

  duRrc->SetAttribute ("mmWaveDevice", BooleanValue (true));

  // This RRC attribute is used to connect each new RLC instance with the MAC layer
  // (for function such as TransmitPdu, ReportBufferStatusReport).
  // Since in this new architecture, the component carrier manager acts a proxy, it
  // will have its own LteMacSapProvider interface, RLC will see it as through original MAC
  // interface LteMacSapProvider, but the function call will go now through LteEnbComponentCarrierManager
  // instance that needs to implement functions of this interface, and its task will be to
  // forward these calls to the specific MAC of some of the instances of component carriers. This
  // decision will depend on the specific implementation of the component carrier manager.
  duRrc->SetLteMacSapProvider (ccmDuManager->GetLteMacSapProvider ());

  bool ccmDuTest;
  for (auto it = ccDuMap.begin (); it != ccDuMap.end (); ++it)
    {
      Ptr<MmWaveComponentCarrierEnb> ccDu = DynamicCast<MmWaveComponentCarrierEnb> (it->second);
      ccDu->GetPhy ()->SetMmWaveEnbCphySapUser (duRrc->GetLteEnbCphySapUser (it->first));
      duRrc->SetLteEnbCphySapProvider (ccDu->GetPhy ()->GetMmWaveEnbCphySapProvider (), it->first);

      duRrc->SetLteEnbCmacSapProvider (ccDu->GetMac ()->GetEnbCmacSapProvider (), it->first);
      ccDu->GetMac ()->SetEnbCmacSapUser (duRrc->GetLteEnbCmacSapUser (it->first));

      ccDu->GetPhy ()->SetComponentCarrierId (it->first);
      ccDu->GetMac ()->SetComponentCarrierId (it->first);

      // PHY <--> MAC SAP
      ccDu->GetPhy ()->SetPhySapUser (ccDu->GetMac ()->GetPhySapUser ());
      ccDu->GetMac ()->SetPhySapProvider (ccDu->GetPhy ()->GetPhySapProvider ());
      // PHY <--> MAC SAP END

      //Scheduler SAP
      ccDu->GetMac ()->SetMmWaveMacSchedSapProvider (
          ccDu->GetMacScheduler ()->GetMacSchedSapProvider ());
      ccDu->GetMac ()->SetMmWaveMacCschedSapProvider (
          ccDu->GetMacScheduler ()->GetMacCschedSapProvider ());

      ccDu->GetMacScheduler ()->SetMacSchedSapUser (ccDu->GetMac ()->GetMmWaveMacSchedSapUser ());
      ccDu->GetMacScheduler ()->SetMacCschedSapUser (ccDu->GetMac ()->GetMmWaveMacCschedSapUser ());
      // Scheduler SAP END

      ccDu->GetMac ()->SetLteCcmMacSapUser (ccmDuManager->GetLteCcmMacSapUser ());
      ccmDuManager->SetCcmMacSapProviders (it->first, ccDu->GetMac ()->GetLteCcmMacSapProvider ());

      // insert the pointer to the LteMacSapProvider interface of the MAC layer of the specific component carrier
      ccmDuTest =
          ccmDuManager->SetMacSapProvider (it->first, ccDu->GetMac ()->GetUeMacSapProvider ());

      if (ccmDuTest == false)
        {
          NS_FATAL_ERROR ("Error in SetComponentCarrierMacSapProviders");
        }
    }

  iabDevice->SetAttribute ("CellId", UintegerValue (cellId));
  iabDevice->SetAttribute ("LteEnbComponentCarrierManager", PointerValue (ccmDuManager));
  iabDevice->SetDuCcMap (ccDuMap);
  iabDevice->SetAttribute ("LteEnbRrc", PointerValue (duRrc));

  for (auto it = ccDuMap.begin (); it != ccDuMap.end (); ++it)
    {
      Ptr<MmWaveEnbPhy> ccDuPhy = DynamicCast<MmWaveComponentCarrierEnb> (it->second)->GetPhy ();
      ccDuPhy->SetDevice (iabDevice);
      ccDuPhy->GetUlSpectrumPhy ()->SetDevice (iabDevice);
      ccDuPhy->GetDlSpectrumPhy ()->SetDevice (iabDevice);
      ccDuPhy->GetDlSpectrumPhy ()->SetPhyRxDataEndOkCallback (
          MakeCallback (&MmWaveEnbPhy::PhyDataPacketReceived, ccDuPhy));
      ccDuPhy->GetDlSpectrumPhy ()->SetPhyRxCtrlEndOkCallback (
          MakeCallback (&MmWaveEnbPhy::PhyCtrlMessagesReceived, ccDuPhy));
      ccDuPhy->GetDlSpectrumPhy ()->SetPhyUlHarqFeedbackCallback (
          MakeCallback (&MmWaveEnbPhy::ReceiveUlHarqFeedback, ccDuPhy));
      NS_LOG_LOGIC ("set the propagation model frequencies");
    }

  duRrc->SetForwardUpCallback (MakeCallback (&MmWaveEnbNetDevice::Receive, iabDevice));

  Ptr<EpcUeNas> mtNas = CreateObject<EpcUeNas> ();

  mtNas->SetAsSapProvider (mtRrc->GetAsSapProvider ());
  mtRrc->SetAsSapUser (mtNas->GetAsSapUser ());

  for (auto it = ccMtMap.begin (); it != ccMtMap.end (); ++it)
    {
      Ptr<MmWaveComponentCarrierUe> ccMt = DynamicCast<MmWaveComponentCarrierUe> (it->second);
      mtRrc->SetLteUeCmacSapProvider (ccMt->GetMac ()->GetUeCmacSapProvider (), it->first);
      ccMt->GetMac ()->SetUeCmacSapUser (mtRrc->GetLteUeCmacSapUser (it->first));
      ccMt->GetMac ()->SetComponentCarrierId (it->first);

      ccMt->GetPhy ()->SetUeCphySapUser (mtRrc->GetLteUeCphySapUser (it->first));
      mtRrc->SetLteUeCphySapProvider (ccMt->GetPhy ()->GetUeCphySapProvider (), it->first);
      ccMt->GetPhy ()->SetComponentCarrierId (it->first);

      ccMt->GetPhy ()->SetPhySapUser (ccMt->GetMac ()->GetPhySapUser ());
      ccMt->GetMac ()->SetPhySapProvider (ccMt->GetPhy ()->GetPhySapProvider ());

      ccMt->GetPhy ()->SetConfigurationParameters (ccMt->GetConfigurationParameters ());
      ccMt->GetMac ()->SetConfigurationParameters (ccMt->GetConfigurationParameters ());

      bool ccmMtTest = ccmMtManager->SetComponentCarrierMacSapProviders (
          it->first, ccMt->GetMac ()->GetUeMacSapProvider ());

      if (ccmMtTest == false)
        {
          NS_FATAL_ERROR ("Error in SetComponentCarrierMacSapProviders");
        }
    }

  NS_ABORT_MSG_IF (m_imsiCounter >= 0xFFFFFFFF, "max num UEs exceeded");
  uint64_t imsi = ++m_imsiCounter;

  bap->SetThisIabNodeImsi (imsi);
  iabDevice->SetAttribute ("Imsi", UintegerValue (imsi));
  iabDevice->SetMtCcMap (ccMtMap);
  iabDevice->SetAttribute ("EpcUeNas", PointerValue (mtNas));
  iabDevice->SetAttribute ("LteUeRrc", PointerValue (mtRrc));
  iabDevice->SetAttribute ("LteUeComponentCarrierManager", PointerValue (ccmMtManager));

  iabDevice->Initialize ();

  for (auto it = ccDuMap.begin (); it != ccDuMap.end (); ++it)
    {

      Ptr<MmWaveComponentCarrierEnb> ccDu = DynamicCast<MmWaveComponentCarrierEnb> (it->second);
      m_channel.at (it->first)->AddRx (
          ccDu->GetPhy ()->GetDlSpectrumPhy ()); //TODO check if Dl and Ul are the same
      ccDu->GetMacScheduler ()->SetCellId (iabDevice->GetCellId ());
    }

  n->AddDevice (iabDevice);

  if (m_epcHelper)
    {
      NS_LOG_INFO ("adding this eNB to the EPC");
      m_epcHelper->AddDu (n, iabDevice, iabDevice->GetCellId (), true);
      Ptr<EpcIabApplication> duApp = n->GetApplication (0)->GetObject<EpcIabApplication> ();
      NS_ASSERT_MSG (duApp, "cannot retrieve EpcIabApplication");

      // S1 SAPs
      duRrc->SetS1SapProvider (duApp->GetS1SapProvider ());
      duApp->SetS1SapUser (duRrc->GetS1SapUser ());

      // X2 SAPs
      Ptr<EpcX2> x2 = n->GetObject<EpcX2> ();
      x2->SetEpcX2SapUser (duRrc->GetEpcX2SapUser ());
      duRrc->SetEpcX2SapProvider (x2->GetEpcX2SapProvider ());
      duRrc->SetEpcX2RlcProvider (x2->GetEpcX2RlcProvider ());
    }

  for (auto it = ccMtMap.begin (); it != ccMtMap.end (); ++it)
    {
      Ptr<MmWaveUePhy> ccMtPhy = DynamicCast<MmWaveComponentCarrierUe> (it->second)->GetPhy ();
      ccMtPhy->SetDevice (iabDevice);
      ccMtPhy->SetImsi (imsi);
      ccMtPhy->GetUlSpectrumPhy ()->SetDevice (iabDevice);
      ccMtPhy->GetDlSpectrumPhy ()->SetDevice (iabDevice);
      ccMtPhy->GetDlSpectrumPhy ()->SetPhyRxDataEndOkCallback (
          MakeCallback (&MmWaveUePhy::PhyDataPacketReceived, ccMtPhy));
      ccMtPhy->GetDlSpectrumPhy ()->SetPhyRxCtrlEndOkCallback (
          MakeCallback (&MmWaveUePhy::ReceiveControlMessageList, ccMtPhy));
      //ccPhy->GetDlSpectrumPhy ()->SetLtePhyRxPssCallback (MakeCallback (&LteUePhy::ReceivePss, ccPhy));
      //ccPhy->GetDlSpectrumPhy ()->SetLtePhyDlHarqFeedbackCallback (MakeCallback (&LteUePhy::ReceiveLteDlHarqFeedback, ccPhy)); this is done before
    }

  mtNas->SetDevice (iabDevice);

  mtNas->SetForwardUpCallback (MakeCallback (&MmWaveIabNetDevice::Receive, iabDevice));

  if (m_epcHelper)
    {
      m_epcHelper->AddUe (iabDevice, iabDevice->GetImsi ());
    }

  return iabDevice;
}

Ptr<NetDevice>
MmWaveHelper::InstallSingleLteEnbDevice (Ptr<Node> n)
{
  uint16_t cellId = m_cellIdCounter; // \todo Remove, eNB has no cell ID

  Ptr<LteEnbNetDevice> dev = m_lteEnbNetDeviceFactory.Create<LteEnbNetDevice> ();
  Ptr<LteHandoverAlgorithm> handoverAlgorithm =
      m_lteHandoverAlgorithmFactory.Create<LteHandoverAlgorithm> ();

  NS_ASSERT_MSG (m_lteComponentCarrierPhyParams.size () != 0, "Cannot create enb ccm map.");
  // create component carrier map for this eNb device
  std::map<uint8_t, Ptr<ComponentCarrierEnb>> ccMap;
  for (std::map<uint8_t, ComponentCarrier>::iterator it = m_lteComponentCarrierPhyParams.begin ();
       it != m_lteComponentCarrierPhyParams.end (); ++it)
    {
      Ptr<ComponentCarrierEnb> cc = CreateObject<ComponentCarrierEnb> ();
      cc->SetUlBandwidth (it->second.GetUlBandwidth ());
      cc->SetDlBandwidth (it->second.GetDlBandwidth ());
      cc->SetDlEarfcn (it->second.GetDlEarfcn ());
      cc->SetUlEarfcn (it->second.GetUlEarfcn ());
      cc->SetAsPrimary (it->second.IsPrimary ());
      NS_ABORT_MSG_IF (m_cellIdCounter == 65535, "max num cells exceeded");
      cc->SetCellId (m_cellIdCounter++);
      ccMap[it->first] = cc;
    }
  NS_ABORT_MSG_IF (m_lteUseCa && ccMap.size () < 2,
                   "You have to either specify carriers or disable carrier aggregation");
  NS_ASSERT (ccMap.size () == m_noOfLteCcs);

  for (std::map<uint8_t, Ptr<ComponentCarrierEnb>>::iterator it = ccMap.begin ();
       it != ccMap.end (); ++it)
    {
      NS_LOG_DEBUG (this << "component carrier map size " << (uint16_t) ccMap.size ());
      Ptr<LteSpectrumPhy> dlPhy = CreateObject<LteSpectrumPhy> ();
      Ptr<LteSpectrumPhy> ulPhy = CreateObject<LteSpectrumPhy> ();
      Ptr<LteEnbPhy> phy = CreateObject<LteEnbPhy> (dlPhy, ulPhy);

      Ptr<LteHarqPhy> harq = Create<LteHarqPhy> ();
      dlPhy->SetHarqPhyModule (harq);
      ulPhy->SetHarqPhyModule (harq);
      phy->SetHarqPhyModule (harq);

      Ptr<LteChunkProcessor> pCtrl = Create<LteChunkProcessor> ();
      pCtrl->AddCallback (MakeCallback (&LteEnbPhy::GenerateCtrlCqiReport, phy));
      ulPhy->AddCtrlSinrChunkProcessor (pCtrl); // for evaluating SRS UL-CQI

      Ptr<LteChunkProcessor> pData = Create<LteChunkProcessor> ();
      pData->AddCallback (MakeCallback (&LteEnbPhy::GenerateDataCqiReport, phy));
      pData->AddCallback (MakeCallback (&LteSpectrumPhy::UpdateSinrPerceived, ulPhy));
      ulPhy->AddDataSinrChunkProcessor (pData); // for evaluating PUSCH UL-CQI

      Ptr<LteChunkProcessor> pInterf = Create<LteChunkProcessor> ();
      pInterf->AddCallback (MakeCallback (&LteEnbPhy::ReportInterference, phy));
      ulPhy->AddInterferenceDataChunkProcessor (pInterf); // for interference power tracing

      dlPhy->SetChannel (m_downlinkChannel);
      ulPhy->SetChannel (m_uplinkChannel);

      Ptr<MobilityModel> mm = n->GetObject<MobilityModel> ();
      NS_ASSERT_MSG (
          mm,
          "MobilityModel needs to be set on node before calling LteHelper::InstallEnbDevice ()");
      dlPhy->SetMobility (mm);
      ulPhy->SetMobility (mm);

      Ptr<AntennaModel> antenna =
          (m_lteEnbAntennaModelFactory.Create ())->GetObject<AntennaModel> ();
      NS_ASSERT_MSG (antenna, "error in creating the AntennaModel object");
      dlPhy->SetAntenna (antenna);
      ulPhy->SetAntenna (antenna);

      Ptr<LteEnbMac> mac = CreateObject<LteEnbMac> ();
      Ptr<FfMacScheduler> sched = m_lteSchedulerFactory.Create<FfMacScheduler> ();
      Ptr<LteFfrAlgorithm> ffrAlgorithm = m_lteFfrAlgorithmFactory.Create<LteFfrAlgorithm> ();
      it->second->SetMac (mac);
      it->second->SetFfMacScheduler (sched);
      it->second->SetFfrAlgorithm (ffrAlgorithm);

      it->second->SetPhy (phy);
    }

  Ptr<LteEnbRrc> rrc = CreateObject<LteEnbRrc> ();
  Ptr<LteEnbComponentCarrierManager> ccmEnbManager =
      m_lteEnbComponentCarrierManagerFactory.Create<LteEnbComponentCarrierManager> ();

  //ComponentCarrierManager SAP
  rrc->SetLteCcmRrcSapProvider (ccmEnbManager->GetLteCcmRrcSapProvider ());
  ccmEnbManager->SetLteCcmRrcSapUser (rrc->GetLteCcmRrcSapUser ());
  // Set number of component carriers. Note: eNB CCM would also set the
  // number of component carriers in eNB RRC
  ccmEnbManager->SetNumberOfComponentCarriers (m_noOfLteCcs);

  rrc->ConfigureCarriers (ccMap);

  if (m_useIdealRrc)
    {
      Ptr<MmWaveEnbRrcProtocolIdeal> rrcProtocol = CreateObject<MmWaveEnbRrcProtocolIdeal> ();
      rrcProtocol->SetLteEnbRrcSapProvider (rrc->GetLteEnbRrcSapProvider ());
      rrc->SetLteEnbRrcSapUser (rrcProtocol->GetLteEnbRrcSapUser ());
      rrc->AggregateObject (rrcProtocol);
      rrcProtocol->SetCellId (cellId);
    }
  else
    {
      Ptr<MmWaveLteEnbRrcProtocolReal> rrcProtocol = CreateObject<MmWaveLteEnbRrcProtocolReal> ();
      rrcProtocol->SetLteEnbRrcSapProvider (rrc->GetLteEnbRrcSapProvider ());
      rrc->SetLteEnbRrcSapUser (rrcProtocol->GetLteEnbRrcSapUser ());
      rrc->AggregateObject (rrcProtocol);
      rrcProtocol->SetCellId (cellId);
    }

  if (m_epcHelper)
    {
      EnumValue epsBearerToRlcMapping;
      rrc->GetAttribute ("EpsBearerToRlcMapping", epsBearerToRlcMapping);
      // it does not make sense to use RLC/SM when also using the EPC

      // ***************** RDF EDIT 6/9/2016 ***************** //
      //    if (epsBearerToRlcMapping.Get () == LteEnbRrc::RLC_SM_ALWAYS)
      //      {
      //        rrc->SetAttribute ("EpsBearerToRlcMapping", EnumValue (LteEnbRrc::RLC_UM_ALWAYS));
      //      }

      if (m_rlcAmEnabled)
        {
          rrc->SetAttribute ("EpsBearerToRlcMapping", EnumValue (LteEnbRrc::RLC_AM_ALWAYS));
        }
      else
        {
          rrc->SetAttribute ("EpsBearerToRlcMapping", EnumValue (LteEnbRrc::RLC_UM_LOWLAT_ALWAYS));
        }
    }

  rrc->SetLteHandoverManagementSapProvider (
      handoverAlgorithm->GetLteHandoverManagementSapProvider ());
  handoverAlgorithm->SetLteHandoverManagementSapUser (rrc->GetLteHandoverManagementSapUser ());

  // This RRC attribute is used to connect each new RLC instance with the MAC layer
  // (for function such as TransmitPdu, ReportBufferStatusReport).
  // Since in this new architecture, the component carrier manager acts a proxy, it
  // will have its own LteMacSapProvider interface, RLC will see it as through original MAC
  // interface LteMacSapProvider, but the function call will go now through LteEnbComponentCarrierManager
  // instance that needs to implement functions of this interface, and its task will be to
  // forward these calls to the specific MAC of some of the instances of component carriers. This
  // decision will depend on the specific implementation of the component carrier manager.
  rrc->SetLteMacSapProvider (ccmEnbManager->GetLteMacSapProvider ());

  bool ccmTest;
  for (std::map<uint8_t, Ptr<ComponentCarrierEnb>>::iterator it = ccMap.begin ();
       it != ccMap.end (); ++it)
    {
      it->second->GetPhy ()->SetLteEnbCphySapUser (rrc->GetLteEnbCphySapUser (it->first));
      rrc->SetLteEnbCphySapProvider (it->second->GetPhy ()->GetLteEnbCphySapProvider (), it->first);

      rrc->SetLteEnbCmacSapProvider (it->second->GetMac ()->GetLteEnbCmacSapProvider (), it->first);
      it->second->GetMac ()->SetLteEnbCmacSapUser (rrc->GetLteEnbCmacSapUser (it->first));

      it->second->GetPhy ()->SetComponentCarrierId (it->first);
      it->second->GetMac ()->SetComponentCarrierId (it->first);
      //FFR SAP
      it->second->GetFfMacScheduler ()->SetLteFfrSapProvider (
          it->second->GetFfrAlgorithm ()->GetLteFfrSapProvider ());
      it->second->GetFfrAlgorithm ()->SetLteFfrSapUser (
          it->second->GetFfMacScheduler ()->GetLteFfrSapUser ());
      rrc->SetLteFfrRrcSapProvider (it->second->GetFfrAlgorithm ()->GetLteFfrRrcSapProvider (),
                                    it->first);
      it->second->GetFfrAlgorithm ()->SetLteFfrRrcSapUser (rrc->GetLteFfrRrcSapUser (it->first));
      //FFR SAP END

      // PHY <--> MAC SAP
      it->second->GetPhy ()->SetLteEnbPhySapUser (it->second->GetMac ()->GetLteEnbPhySapUser ());
      it->second->GetMac ()->SetLteEnbPhySapProvider (
          it->second->GetPhy ()->GetLteEnbPhySapProvider ());
      // PHY <--> MAC SAP END

      //Scheduler SAP
      it->second->GetMac ()->SetFfMacSchedSapProvider (
          it->second->GetFfMacScheduler ()->GetFfMacSchedSapProvider ());
      it->second->GetMac ()->SetFfMacCschedSapProvider (
          it->second->GetFfMacScheduler ()->GetFfMacCschedSapProvider ());

      it->second->GetFfMacScheduler ()->SetFfMacSchedSapUser (
          it->second->GetMac ()->GetFfMacSchedSapUser ());
      it->second->GetFfMacScheduler ()->SetFfMacCschedSapUser (
          it->second->GetMac ()->GetFfMacCschedSapUser ());
      // Scheduler SAP END

      it->second->GetMac ()->SetLteCcmMacSapUser (ccmEnbManager->GetLteCcmMacSapUser ());
      ccmEnbManager->SetCcmMacSapProviders (it->first,
                                            it->second->GetMac ()->GetLteCcmMacSapProvider ());

      // insert the pointer to the LteMacSapProvider interface of the MAC layer of the specific component carrier
      ccmTest = ccmEnbManager->SetMacSapProvider (it->first,
                                                  it->second->GetMac ()->GetLteMacSapProvider ());

      if (ccmTest == false)
        {
          NS_FATAL_ERROR ("Error in SetComponentCarrierMacSapProviders");
        }
    }

  dev->SetNode (n);
  dev->SetAttribute ("CellId", UintegerValue (cellId));
  dev->SetAttribute ("LteEnbComponentCarrierManager", PointerValue (ccmEnbManager));
  dev->SetCcMap (ccMap);
  std::map<uint8_t, Ptr<ComponentCarrierEnb>>::iterator it = ccMap.begin ();
  dev->SetAttribute ("LteEnbRrc", PointerValue (rrc));
  dev->SetAttribute ("LteHandoverAlgorithm", PointerValue (handoverAlgorithm));
  dev->SetAttribute ("LteFfrAlgorithm", PointerValue (it->second->GetFfrAlgorithm ()));

  if (m_isAnrEnabled)
    {
      Ptr<LteAnr> anr = CreateObject<LteAnr> (cellId);
      rrc->SetLteAnrSapProvider (anr->GetLteAnrSapProvider ());
      anr->SetLteAnrSapUser (rrc->GetLteAnrSapUser ());
      dev->SetAttribute ("LteAnr", PointerValue (anr));
    }

  for (it = ccMap.begin (); it != ccMap.end (); ++it)
    {
      Ptr<LteEnbPhy> ccPhy = it->second->GetPhy ();
      ccPhy->SetDevice (dev);
      ccPhy->GetUlSpectrumPhy ()->SetDevice (dev);
      ccPhy->GetDlSpectrumPhy ()->SetDevice (dev);
      ccPhy->GetUlSpectrumPhy ()->SetLtePhyRxDataEndOkCallback (
          MakeCallback (&LteEnbPhy::PhyPduReceived, ccPhy));
      ccPhy->GetUlSpectrumPhy ()->SetLtePhyRxCtrlEndOkCallback (
          MakeCallback (&LteEnbPhy::ReceiveLteControlMessageList, ccPhy));
      ccPhy->GetUlSpectrumPhy ()->SetLtePhyUlHarqFeedbackCallback (
          MakeCallback (&LteEnbPhy::ReceiveLteUlHarqFeedback, ccPhy));
      NS_LOG_LOGIC ("set the propagation model frequencies");
      double dlFreq = LteSpectrumValueHelper::GetCarrierFrequency (it->second->m_dlEarfcn);
      NS_LOG_LOGIC ("DL freq: " << dlFreq);
      bool dlFreqOk =
          m_downlinkPathlossModel->SetAttributeFailSafe ("Frequency", DoubleValue (dlFreq));
      if (!dlFreqOk)
        {
          NS_LOG_WARN ("DL propagation model does not have a Frequency attribute");
        }

      double ulFreq = LteSpectrumValueHelper::GetCarrierFrequency (it->second->m_ulEarfcn);

      NS_LOG_LOGIC ("UL freq: " << ulFreq);
      bool ulFreqOk =
          m_uplinkPathlossModel->SetAttributeFailSafe ("Frequency", DoubleValue (ulFreq));
      if (!ulFreqOk)
        {
          NS_LOG_WARN ("UL propagation model does not have a Frequency attribute");
        }
    } //end for
  rrc->SetForwardUpCallback (MakeCallback (&LteEnbNetDevice::Receive, dev));
  dev->Initialize ();
  n->AddDevice (dev);

  for (it = ccMap.begin (); it != ccMap.end (); ++it)
    {
      m_uplinkChannel->AddRx (it->second->GetPhy ()->GetUlSpectrumPhy ());
    }

  if (m_epcHelper)
    {
      NS_LOG_INFO ("adding this eNB to the EPC");
      m_epcHelper->AddDu (n, dev, dev->GetCellId ());
      Ptr<EpcEnbApplication> enbApp = n->GetApplication (0)->GetObject<EpcEnbApplication> ();
      NS_ASSERT_MSG (enbApp, "cannot retrieve EpcEnbApplication");

      // S1 SAPs
      rrc->SetS1SapProvider (enbApp->GetS1SapProvider ());
      enbApp->SetS1SapUser (rrc->GetS1SapUser ());

      // X2 SAPs
      Ptr<EpcX2> x2 = n->GetObject<EpcX2> ();
      x2->SetEpcX2SapUser (rrc->GetEpcX2SapUser ());
      rrc->SetEpcX2SapProvider (x2->GetEpcX2SapProvider ());
      rrc->SetEpcX2PdcpProvider (x2->GetEpcX2PdcpProvider ());
    }

  return dev;
}

// only for mmWave-only devices
void
MmWaveHelper::AttachToClosestEnb (NetDeviceContainer ueDevices, NetDeviceContainer enbDevices)
{
  NS_LOG_FUNCTION (this);

  for (NetDeviceContainer::Iterator i = ueDevices.Begin (); i != ueDevices.End (); i++)
    {
      AttachToClosestEnb (*i, enbDevices);
    }
}

// for MC devices
void
MmWaveHelper::AttachToClosestEnb (NetDeviceContainer ueDevices, NetDeviceContainer mmWaveEnbDevices,
                                  NetDeviceContainer lteEnbDevices)
{
  NS_LOG_FUNCTION (this);

  for (NetDeviceContainer::Iterator i = ueDevices.Begin (); i != ueDevices.End (); i++)
    {
      AttachMcToClosestEnb (*i, mmWaveEnbDevices, lteEnbDevices);
    }
}

// only for mmWave-only devices
void
MmWaveHelper::AttachToClosestIab (NetDeviceContainer ueDevices, NetDeviceContainer iabDevices)
{
  NS_LOG_FUNCTION (this);

  for (NetDeviceContainer::Iterator i = ueDevices.Begin (); i != ueDevices.End (); i++)
    {
      AttachToClosestIab (*i, iabDevices);
    }
}

// for InterRatHoCapable devices
void
MmWaveHelper::AttachIrToClosestEnb (NetDeviceContainer ueDevices,
                                    NetDeviceContainer mmWaveEnbDevices,
                                    NetDeviceContainer lteEnbDevices)
{
  NS_LOG_FUNCTION (this);

  /*
  // set initial conditions on beamforming before attaching the UE to the eNBs
  if(m_channelModelType == "ns3::MmWaveBeamforming")
  {
          m_beamforming->Initial(ueDevices,mmWaveEnbDevices);
  }
  else if(m_channelModelType == "ns3::MmWaveChannelMatrix")
  {
          m_channelMatrix->Initial(ueDevices,mmWaveEnbDevices);
  }
  else if(m_channelModelType == "ns3::MmWaveChannelRaytracing")
  {
          m_raytracing->Initial(ueDevices,mmWaveEnbDevices);
  }
  else if(m_channelModelType == "ns3::MmWave3gppChannel")
  {
          m_3gppChannel->Initial(ueDevices,mmWaveEnbDevices);
  }

  for (NetDeviceContainer::Iterator i = ueDevices.Begin(); i != ueDevices.End(); i++)
  {
          AttachIrToClosestEnb(*i, mmWaveEnbDevices, lteEnbDevices);
  }
  */
}

void
MmWaveHelper::AttachToClosestEnb (Ptr<NetDevice> ueDevice, NetDeviceContainer enbDevices)
{
  NS_LOG_FUNCTION (this << ueDevice << enbDevices.GetN ());
  NS_ASSERT_MSG (enbDevices.GetN () > 0, "empty enb device container");
  Vector uePos = ueDevice->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();

  // find the closest BS
  double minDistance = std::numeric_limits<double>::infinity ();
  int closestEnbIndex = -1;
  for (uint32_t i = 0; i < enbDevices.GetN (); ++i)
    {
      Vector enbPos = enbDevices.Get (i)->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
      double distance = CalculateDistance (uePos, enbPos);

      if (distance < minDistance)
        {
          minDistance = distance;
          closestEnbIndex = i;
        }
    }
  NS_ASSERT_MSG (closestEnbIndex >= 0, "Closest eNB not found!");

  AttachToEnbWithIndex (ueDevice, enbDevices, closestEnbIndex);
}

void
MmWaveHelper::AttachIabTotIabWithIndex (NetDeviceContainer iabDevices, uint32_t indexIabNode,
                                        uint32_t indexIabParent, NetDeviceContainer enbDevices,
                                        uint32_t indexDonor)
{
  NS_LOG_FUNCTION (this << iabDevices.GetN () << enbDevices.GetN ());
  NS_ASSERT_MSG (iabDevices.GetN () > 0 && enbDevices.GetN () > 0, "empty device container");

  // select the IAB  with the given index
  Ptr<NetDevice> childNetDev = iabDevices.Get (indexIabNode);
  Ptr<NetDevice> parentNetDev = iabDevices.Get (indexIabParent);
  Ptr<MmWaveEnbNetDevice> donorEnbDev =
      DynamicCast<MmWaveEnbNetDevice> (enbDevices.Get (indexDonor));

  NS_ASSERT (donorEnbDev);

  auto donorApp = donorEnbDev->GetNode ()->GetApplication (0);
  Ptr<EpcEnbApplication> donorEnbApp = DynamicCast<EpcEnbApplication> (donorApp);

  NS_ASSERT (donorEnbApp);

  // cast the device to IAB node
  Ptr<MmWaveIabNetDevice> childIabDev = childNetDev->GetObject<MmWaveIabNetDevice> ();
  Ptr<MmWaveIabNetDevice> parentIabDev = parentNetDev->GetObject<MmWaveIabNetDevice> ();

  auto addedPaths =
      donorEnbApp->RegisterNewIabAttachment (m_bapPathIdCounter, childIabDev, parentIabDev);
  m_bapPathIdCounter += addedPaths;
  childIabDev->GetBap ()->SetNextHopCallback (
      MakeCallback (&EpcEnbApplication::GetNexBapPathHop, donorEnbApp));
  childIabDev->GetBap ()->SetPathIdCallback (
      MakeCallback (&EpcEnbApplication::GetPathId, donorEnbApp));

  auto duNodeCcMap = childIabDev->GetDuCcMap ();

  for (auto it = duNodeCcMap.begin (); it != duNodeCcMap.end (); ++it)
    {
      Ptr<MmWaveComponentCarrierEnb> ccIabNode =
          DynamicCast<MmWaveComponentCarrierEnb> (it->second);
      ccIabNode->GetMacScheduler ()->SetDepthCallback (
          MakeCallback (&EpcEnbApplication::GetIabNodeDepth, donorEnbApp));
      ccIabNode->SetCellId (childIabDev->GetCellId ());
    }

  // Necessary operations to connect MT to donor at lower layers
  // TODO: test this
  for (NetDeviceContainer::Iterator i = enbDevices.Begin (); i != enbDevices.End (); ++i)
    {
      Ptr<MmWaveEnbNetDevice> mmWaveEnb = (*i)->GetObject<MmWaveEnbNetDevice> ();
      std::map<uint8_t, Ptr<MmWaveComponentCarrier>> enbCcMap = mmWaveEnb->GetCcMap ();

      // TODO here I have to pair UE CC and eNB CC with the same CCid, CCs with different
      // IDs cannot communicate
      for (const auto &itEnb : enbCcMap)
        {
          Ptr<MmWaveComponentCarrierEnb> ccEnb =
              DynamicCast<MmWaveComponentCarrierEnb> (itEnb.second);

          uint16_t mmWaveCellId = ccEnb->GetCellId ();
          Ptr<MmWavePhyMacCommon> configParams = ccEnb->GetPhy ()->GetConfigurationParameters ();
          ccEnb->GetPhy ()->AddUePhy (childIabDev->GetImsi (), childNetDev);

          // register MmWave eNBs informations in the MmWaveUePhy
          std::map<uint8_t, Ptr<MmWaveComponentCarrier>> mtCcMap = childIabDev->GetMtCcMap ();
          for (const auto &itCc : mtCcMap)
            {
              DynamicCast<MmWaveComponentCarrierUe> (itCc.second)
                  ->GetPhy ()
                  ->RegisterOtherEnb (mmWaveCellId, configParams, mmWaveEnb);
            }
        }
    }

  std::map<uint8_t, Ptr<MmWaveComponentCarrier>> iabParentCcMap = parentIabDev->GetDuCcMap ();

  for (const auto &itParent : iabParentCcMap)
    {
      Ptr<MmWaveComponentCarrierEnb> ccParent =
          DynamicCast<MmWaveComponentCarrierEnb> (itParent.second);

      uint16_t mmWaveCellId = ccParent->GetCellId ();
      Ptr<MmWavePhyMacCommon> configParams = ccParent->GetPhy ()->GetConfigurationParameters ();

      // associate the PHY layers
      ccParent->GetPhy ()->AddUePhy (childIabDev->GetImsi (), childNetDev);

      // register IAB parent DU informations in the MmWaveUePhy
      std::map<uint8_t, Ptr<MmWaveComponentCarrier>> mtCcMap = childIabDev->GetMtCcMap ();
      for (const auto &itCc : mtCcMap)
        {
          DynamicCast<MmWaveComponentCarrierUe> (itCc.second)
              ->GetPhy ()
              ->RegisterOtherIab (mmWaveCellId, configParams, parentIabDev);
        }
      // associate the MAC layers
      childIabDev->GetMtPhy ()->RegisterToIab (mmWaveCellId, configParams);
      parentIabDev->GetDuMac ()->AssociateUeMAC (childIabDev->GetImsi ());
    }

  // connect MT the target donor
  Ptr<EpcUeNas> ueNas = childIabDev->GetMtNas ();
  ueNas->Connect (parentIabDev->GetCellId (), parentIabDev->GetEarfcn ());

  // TODO: test this
  // TODO: modify this to propagate to the whole IAB network
  if (m_epcHelper)
    {
      auto p2pEpcHelper = DynamicCast<MmWavePointToPointEpcHelper> (m_epcHelper);
      if (p2pEpcHelper)
        {
          p2pEpcHelper->GetNonConstEpcSgwPgwApplication ()->SetIabNodeDonorCellId (
              childIabDev->GetCellId (), donorEnbDev->GetCellId ());
        }

      childIabDev->GetMtRrc ()->SetBapEnb (parentIabDev->GetBap ());
      childIabDev->GetMtRrc ()->SetBapAddressFromCellIdCallback (
          MakeCallback (&EpcEnbApplication::GetBapAddressFromCellId, donorEnbApp));
      childIabDev->GetDuRrc ()->SetBapAddressFromImsiCallback (
          MakeCallback (&EpcEnbApplication::GetBapAddressFromImsi, donorEnbApp));
      // activate default EPS bearer
      m_epcHelper->ActivateEpsBearer (childNetDev, childIabDev->GetImsi (), EpcTft::Default (),
                                      EpsBearer (EpsBearer::NGBR_VIDEO_TCP_DEFAULT));
    }

  childIabDev->SetIabParent (parentIabDev);
}

void
MmWaveHelper::AttachIabTotDonorWithIndex (NetDeviceContainer iabDevices,
                                          NetDeviceContainer enbDevices, uint32_t index)
{
  NS_LOG_FUNCTION (this << iabDevices.GetN () << enbDevices.GetN () << index);
  NS_ASSERT_MSG (enbDevices.GetN () > 0 && iabDevices.GetN () > 0, "empty device containers");

  for (uint32_t i = 0; i < iabDevices.GetN (); ++i)
    {
      AttachIabTotDonorWithIndex (iabDevices.Get (i), enbDevices, index);
    }
}

void
MmWaveHelper::AttachIabTotDonorWithIndex (Ptr<NetDevice> iabDevice, NetDeviceContainer enbDevices,
                                          uint32_t index)
{
  NS_LOG_FUNCTION (this << iabDevice << enbDevices.GetN () << index);
  NS_ASSERT_MSG (enbDevices.GetN () > 0, "empty enb device container");

  // select the donor with the given index
  Ptr<NetDevice> targetDevice = enbDevices.Get (index);
  NS_ASSERT (targetDevice);

  Ptr<MmWaveEnbNetDevice> targetEnbDevice = targetDevice->GetObject<MmWaveEnbNetDevice> ();

  auto donorApp = targetEnbDevice->GetNode ()->GetApplication (0);
  Ptr<EpcEnbApplication> donorEnbApp = DynamicCast<EpcEnbApplication> (donorApp);
  NS_ASSERT (donorEnbApp);
  // cast the device to IAB node
  Ptr<MmWaveIabNetDevice> childIabDev = iabDevice->GetObject<MmWaveIabNetDevice> ();

  auto addedPaths =
      donorEnbApp->RegisterNewIabAttachment (m_bapPathIdCounter, childIabDev, targetEnbDevice);
  m_bapPathIdCounter += addedPaths;
  childIabDev->GetBap ()->SetNextHopCallback (
      MakeCallback (&EpcEnbApplication::GetNexBapPathHop, donorEnbApp));
  childIabDev->GetBap ()->SetPathIdCallback (
      MakeCallback (&EpcEnbApplication::GetPathId, donorEnbApp));

  auto duCcMap = childIabDev->GetDuCcMap ();

  for (auto it = duCcMap.begin (); it != duCcMap.end (); ++it)
    {
      Ptr<MmWaveComponentCarrierEnb> ccIab = DynamicCast<MmWaveComponentCarrierEnb> (it->second);
      ccIab->GetMacScheduler ()->SetDepthCallback (
          MakeCallback (&EpcEnbApplication::GetIabNodeDepth, donorEnbApp));
      ccIab->SetCellId (childIabDev->GetCellId ());
    }

  // Necessary operations to connect MT to donor at lower layers
  // TODO: test this
  for (NetDeviceContainer::Iterator i = enbDevices.Begin (); i != enbDevices.End (); ++i)
    {
      Ptr<MmWaveEnbNetDevice> mmWaveEnb = (*i)->GetObject<MmWaveEnbNetDevice> ();
      std::map<uint8_t, Ptr<MmWaveComponentCarrier>> enbCcMap = mmWaveEnb->GetCcMap ();

      // TODO here I have to pair UE CC and eNB CC with the same CCid, CCs with different
      // IDs cannot communicate
      for (const auto &itEnb : enbCcMap)
        {
          Ptr<MmWaveComponentCarrierEnb> ccEnb =
              DynamicCast<MmWaveComponentCarrierEnb> (itEnb.second);

          uint16_t mmWaveCellId = ccEnb->GetCellId ();
          Ptr<MmWavePhyMacCommon> configParams = ccEnb->GetPhy ()->GetConfigurationParameters ();
          ccEnb->GetPhy ()->AddUePhy (childIabDev->GetImsi (), iabDevice);

          // register MmWave eNBs informations in the MmWaveUePhy
          std::map<uint8_t, Ptr<MmWaveComponentCarrier>> mtCcMap = childIabDev->GetMtCcMap ();
          for (const auto &itCc : mtCcMap)
            {
              DynamicCast<MmWaveComponentCarrierUe> (itCc.second)
                  ->GetPhy ()
                  ->RegisterOtherEnb (mmWaveCellId, configParams, mmWaveEnb);
            }
        }
    }

  // TODO check: the initial access is performed by the PCC, the method RegisterToEnb
  // should be called only on the PCC PHY. The configuration of the SCCs will be
  // performed by UE RRC during the connection setup phase
  // TODO: test this
  uint16_t cellId = targetDevice->GetObject<MmWaveEnbNetDevice> ()->GetCellId ();
  Ptr<MmWavePhyMacCommon> configParams =
      targetDevice->GetObject<MmWaveEnbNetDevice> ()->GetPhy ()->GetConfigurationParameters ();

  // associate the PHY layers
  targetDevice->GetObject<MmWaveEnbNetDevice> ()->GetPhy ()->AddUePhy (childIabDev->GetImsi (),
                                                                       iabDevice);

  // associate the MAC layers
  childIabDev->GetMtPhy ()->RegisterToEnb (cellId, configParams);
  targetDevice->GetObject<MmWaveEnbNetDevice> ()->GetMac ()->AssociateUeMAC (
      childIabDev->GetImsi ());

  // connect MT the target donor
  Ptr<EpcUeNas> ueNas = childIabDev->GetMtNas ();
  ueNas->Connect (targetDevice->GetObject<MmWaveEnbNetDevice> ()->GetCellId (),
                  targetDevice->GetObject<MmWaveEnbNetDevice> ()->GetEarfcn ());

  // TODO: test this
  // TODO: modify this to propagate to the whole IAB network
  if (m_epcHelper)
    {
      auto p2pEpcHelper = DynamicCast<MmWavePointToPointEpcHelper> (m_epcHelper);
      if (p2pEpcHelper)
        {
          p2pEpcHelper->GetNonConstEpcSgwPgwApplication ()->SetIabNodeDonorCellId (
              childIabDev->GetCellId (),
              targetDevice->GetObject<MmWaveEnbNetDevice> ()->GetCellId ());
        }
      childIabDev->GetMtRrc ()->SetBapEnb (targetEnbDevice->GetBap ());
      childIabDev->GetMtRrc ()->SetBapAddressFromCellIdCallback (
          MakeCallback (&EpcEnbApplication::GetBapAddressFromCellId, donorEnbApp));
      childIabDev->GetDuRrc ()->SetBapAddressFromImsiCallback (
          MakeCallback (&EpcEnbApplication::GetBapAddressFromImsi, donorEnbApp));

      // activate default EPS bearer
      m_epcHelper->ActivateEpsBearer (iabDevice, childIabDev->GetImsi (), EpcTft::Default (),
                                      EpsBearer (EpsBearer::NGBR_VIDEO_TCP_DEFAULT));
    }

  childIabDev->SetTargetDonor (targetDevice->GetObject<MmWaveEnbNetDevice> ());
}

void
MmWaveHelper::AttachMcToClosestEnb (Ptr<NetDevice> ueDevice, NetDeviceContainer mmWaveEnbDevices,
                                    NetDeviceContainer lteEnbDevices)
{
  NS_LOG_FUNCTION (this);
  Ptr<McUeNetDevice> mcDevice = ueDevice->GetObject<McUeNetDevice> ();

  NS_ASSERT_MSG (mmWaveEnbDevices.GetN () > 0 && lteEnbDevices.GetN () > 0,
                 "empty lte or mmwave enb device container");

  // Find the closest LTE station
  Vector uepos = ueDevice->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
  double minDistance = std::numeric_limits<double>::infinity ();
  Ptr<NetDevice> lteClosestEnbDevice;
  for (NetDeviceContainer::Iterator i = lteEnbDevices.Begin (); i != lteEnbDevices.End (); ++i)
    {
      Vector enbpos = (*i)->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
      double distance = CalculateDistance (uepos, enbpos);
      if (distance < minDistance)
        {
          minDistance = distance;
          lteClosestEnbDevice = *i;
        }
    }
  NS_ASSERT (lteClosestEnbDevice);
  NS_ASSERT (lteClosestEnbDevice->GetObject<LteEnbNetDevice> ()); // stop if it is not an LTE eNB

  // Necessary operation to connect MmWave UE to eNB at lower layers
  for (NetDeviceContainer::Iterator i = mmWaveEnbDevices.Begin (); i != mmWaveEnbDevices.End ();
       ++i)
    {
      Ptr<MmWaveEnbNetDevice> mmWaveEnb = (*i)->GetObject<MmWaveEnbNetDevice> ();
      std::map<uint8_t, Ptr<MmWaveComponentCarrier>> mmWaveEnbCcMap = mmWaveEnb->GetCcMap ();

      for (auto itEnb = mmWaveEnbCcMap.begin (); itEnb != mmWaveEnbCcMap.end (); ++itEnb)
        {
          Ptr<MmWaveComponentCarrierEnb> ccEnb =
              DynamicCast<MmWaveComponentCarrierEnb> (itEnb->second);
          uint16_t mmWaveCellId = ccEnb->GetCellId ();
          Ptr<MmWavePhyMacCommon> configParams = ccEnb->GetPhy ()->GetConfigurationParameters ();
          ccEnb->GetPhy ()->AddUePhy (mcDevice->GetImsi (), ueDevice);
          // register MmWave eNBs informations in the MmWaveUePhy

          std::map<uint8_t, Ptr<MmWaveComponentCarrierUe>> ueCcMap = mcDevice->GetMmWaveCcMap ();
          for (auto itUe = ueCcMap.begin (); itUe != ueCcMap.end (); ++itUe)
            {
              itUe->second->GetPhy ()->RegisterOtherEnb (mmWaveCellId, configParams, mmWaveEnb);
            }
          //closestMmWave->GetMac ()->AssociateUeMAC (mcDevice->GetImsi ()); //TODO this does not do anything
          NS_LOG_INFO ("mmWaveCellId " << mmWaveCellId);
        }
    }

  // Attach the MC device the LTE eNB, the best MmWave eNB will be selected automatically
  Ptr<LteEnbNetDevice> enbLteDevice = lteClosestEnbDevice->GetObject<LteEnbNetDevice> ();
  Ptr<EpcUeNas> lteUeNas = mcDevice->GetNas ();
  lteUeNas->Connect (enbLteDevice->GetCellId (),
                     enbLteDevice->GetDlEarfcn ()); // the MmWaveCell will be automatically selected

  if (m_epcHelper)
    {
      // activate default EPS bearer
      m_epcHelper->ActivateEpsBearer (ueDevice, lteUeNas, mcDevice->GetImsi (), EpcTft::Default (),
                                      EpsBearer (EpsBearer::NGBR_VIDEO_TCP_DEFAULT));
    }

  mcDevice->SetLteTargetEnb (enbLteDevice);
}

void
MmWaveHelper::AttachToClosestIab (Ptr<NetDevice> ueDevice, NetDeviceContainer iabDevices)
{
  NS_LOG_FUNCTION (this << ueDevice << iabDevices.GetN ());
  NS_ASSERT_MSG (iabDevices.GetN () > 0, "empty enb device container");
  Vector uePos = ueDevice->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();

  // find the closest IAB
  double minDistance = std::numeric_limits<double>::infinity ();
  int closestIabIndex = -1;
  for (uint32_t i = 0; i < iabDevices.GetN (); ++i)
    {
      Vector iabPos = iabDevices.Get (i)->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
      double distance = CalculateDistance (uePos, iabPos);

      if (distance < minDistance)
        {
          minDistance = distance;
          closestIabIndex = i;
        }
    }
  NS_ASSERT_MSG (closestIabIndex >= 0, "Closest eNB not found!");

  AttachToIabWithIndex (ueDevice, iabDevices, closestIabIndex);
}

void
MmWaveHelper::AttachIrToClosestEnb (Ptr<NetDevice> ueDevice, NetDeviceContainer mmWaveEnbDevices,
                                    NetDeviceContainer lteEnbDevices)
{
  NS_LOG_FUNCTION (this);
  Ptr<McUeNetDevice> mcDevice = ueDevice->GetObject<McUeNetDevice> ();
  Ptr<LteUeRrc> ueRrc = mcDevice->GetLteRrc ();

  NS_ASSERT_MSG (ueRrc, "McUeDevice with undefined rrc");

  NS_ASSERT_MSG (mmWaveEnbDevices.GetN () > 0 && lteEnbDevices.GetN () > 0,
                 "empty lte or mmwave enb device container");

  // Find the closest LTE station
  Vector uepos = ueDevice->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
  double minDistance = std::numeric_limits<double>::infinity ();
  Ptr<NetDevice> lteClosestEnbDevice;
  for (NetDeviceContainer::Iterator i = lteEnbDevices.Begin (); i != lteEnbDevices.End (); ++i)
    {
      Ptr<LteEnbNetDevice> lteEnb = (*i)->GetObject<LteEnbNetDevice> ();
      uint16_t cellId = lteEnb->GetCellId ();
      ueRrc->AddLteCellId (cellId);
      // Let the RRC know that the UE in this simulation is InterRatHoCapable
      Ptr<LteEnbRrc> enbRrc = lteEnb->GetRrc ();
      enbRrc->SetInterRatHoMode ();
      Vector enbpos = (*i)->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
      double distance = CalculateDistance (uepos, enbpos);
      if (distance < minDistance)
        {
          minDistance = distance;
          lteClosestEnbDevice = *i;
        }
    }
  NS_ASSERT (lteClosestEnbDevice);

  // Necessary operation to connect MmWave UE to eNB at lower layers
  minDistance = std::numeric_limits<double>::infinity ();
  Ptr<NetDevice> closestEnbDevice;
  for (NetDeviceContainer::Iterator i = mmWaveEnbDevices.Begin (); i != mmWaveEnbDevices.End ();
       ++i)
    {
      Ptr<MmWaveEnbNetDevice> mmWaveEnb = (*i)->GetObject<MmWaveEnbNetDevice> ();
      uint16_t mmWaveCellId = mmWaveEnb->GetCellId ();
      ueRrc->AddMmWaveCellId (mmWaveCellId);
      Ptr<MmWavePhyMacCommon> configParams = mmWaveEnb->GetPhy ()->GetConfigurationParameters ();
      mmWaveEnb->GetPhy ()->AddUePhy (mcDevice->GetImsi (), ueDevice);
      // register MmWave eNBs informations in the MmWaveUePhy
      mcDevice->GetMmWavePhy ()->RegisterOtherEnb (mmWaveCellId, configParams, mmWaveEnb);
      //closestMmWave->GetMac ()->AssociateUeMAC (mcDevice->GetImsi ()); //TODO this does not do anything
      NS_LOG_INFO ("mmWaveCellId " << mmWaveCellId);

      // Let the RRC know that the UE in this simulation is InterRatHoCapable
      Ptr<LteEnbRrc> enbRrc = mmWaveEnb->GetRrc ();
      enbRrc->SetInterRatHoMode ();
      Vector enbpos = (*i)->GetNode ()->GetObject<MobilityModel> ()->GetPosition ();
      double distance = CalculateDistance (uepos, enbpos);
      if (distance < minDistance)
        {
          minDistance = distance;
          closestEnbDevice = *i;
        }
    }

  // Attach the MC device the Closest LTE eNB
  Ptr<LteEnbNetDevice> enbLteDevice = lteClosestEnbDevice->GetObject<LteEnbNetDevice> ();
  Ptr<EpcUeNas> lteUeNas = mcDevice->GetNas ();
  lteUeNas->Connect (enbLteDevice->GetCellId (),
                     enbLteDevice->GetDlEarfcn ()); // force connection to the LTE eNB

  if (m_epcHelper)
    {
      // activate default EPS bearer
      m_epcHelper->ActivateEpsBearer (ueDevice, lteUeNas, mcDevice->GetImsi (), EpcTft::Default (),
                                      EpsBearer (EpsBearer::NGBR_VIDEO_TCP_DEFAULT));
    }

  // set initial targets
  Ptr<MmWaveEnbNetDevice> enbDevice = closestEnbDevice->GetObject<MmWaveEnbNetDevice> ();
  mcDevice->SetLteTargetEnb (enbLteDevice);
  mcDevice->SetMmWaveTargetEnb (enbDevice);
}

void
MmWaveHelper::AttachToEnbWithIndex (Ptr<NetDevice> ueDevice, NetDeviceContainer enbDevices,
                                    uint32_t index)
{
  NS_LOG_FUNCTION (this << ueDevice << enbDevices.GetN () << index);
  NS_ASSERT_MSG (enbDevices.GetN () > 0, "empty enb device container");

  // select the eNB with the given index
  Ptr<NetDevice> targetEnbDevice = enbDevices.Get (index);
  NS_ASSERT (targetEnbDevice);

  // connect the UE to the target BS
  Ptr<MmWaveUeNetDevice> mmWaveUe = ueDevice->GetObject<MmWaveUeNetDevice> ();

  // Necessary operation to connect MmWave UE to eNB at lower layers
  for (NetDeviceContainer::Iterator i = enbDevices.Begin (); i != enbDevices.End (); ++i)
    {
      Ptr<MmWaveEnbNetDevice> mmWaveEnb = (*i)->GetObject<MmWaveEnbNetDevice> ();

      std::map<uint8_t, Ptr<MmWaveComponentCarrier>> enbCcMap = mmWaveEnb->GetCcMap ();

      // TODO here I have to pair UE CC and eNB CC with the same CCid, CCs with different
      // IDs cannot communicate
      for (const auto &itEnb : enbCcMap)
        {
          Ptr<MmWaveComponentCarrierEnb> ccEnb =
              DynamicCast<MmWaveComponentCarrierEnb> (itEnb.second);

          uint16_t mmWaveCellId = ccEnb->GetCellId ();
          Ptr<MmWavePhyMacCommon> configParams = ccEnb->GetPhy ()->GetConfigurationParameters ();
          ccEnb->GetPhy ()->AddUePhy (mmWaveUe->GetImsi (), ueDevice);
          // register MmWave eNBs informations in the MmWaveUePhy

          std::map<uint8_t, Ptr<MmWaveComponentCarrier>> ueCcMap = mmWaveUe->GetCcMap ();
          for (const auto &itUe : ueCcMap)
            {
              DynamicCast<MmWaveComponentCarrierUe> (itUe.second)
                  ->GetPhy ()
                  ->RegisterOtherEnb (mmWaveCellId, configParams, mmWaveEnb);
            }
          //closestMmWave->GetMac ()->AssociateUeMAC (mcDevice->GetImsi ()); //TODO this does not do anything
          NS_LOG_INFO ("mmWaveCellId " << mmWaveCellId);
        }
    }

  // TODO check: the initial access is performed by the PCC, the method RegisterToEnb
  // should be called only on the PCC PHY. The configuration of the SCCs will be
  // performed by UE RRC during the connection setup phase
  uint16_t cellId = targetEnbDevice->GetObject<MmWaveEnbNetDevice> ()->GetCellId ();
  Ptr<MmWavePhyMacCommon> configParams =
      targetEnbDevice->GetObject<MmWaveEnbNetDevice> ()->GetPhy ()->GetConfigurationParameters ();

  // this has alread been called in the for loop above
  targetEnbDevice->GetObject<MmWaveEnbNetDevice> ()->GetPhy ()->AddUePhy (
      ueDevice->GetObject<MmWaveUeNetDevice> ()->GetImsi (), ueDevice);
  ueDevice->GetObject<MmWaveUeNetDevice> ()->GetPhy ()->RegisterToEnb (cellId, configParams);
  targetEnbDevice->GetObject<MmWaveEnbNetDevice> ()->GetMac ()->AssociateUeMAC (
      ueDevice->GetObject<MmWaveUeNetDevice> ()->GetImsi ());

  // connect to the target one
  Ptr<EpcUeNas> ueNas = ueDevice->GetObject<MmWaveUeNetDevice> ()->GetNas ();
  ueNas->Connect (targetEnbDevice->GetObject<MmWaveEnbNetDevice> ()->GetCellId (),
                  targetEnbDevice->GetObject<MmWaveEnbNetDevice> ()->GetEarfcn ());

  if (m_epcHelper)
    {
      // activate default EPS bearer
      m_epcHelper->ActivateEpsBearer (
          ueDevice, ueDevice->GetObject<MmWaveUeNetDevice> ()->GetImsi (), EpcTft::Default (),
          EpsBearer (EpsBearer::NGBR_VIDEO_TCP_DEFAULT));
    }

  // tricks needed for the simplified LTE-only simulations
  //if (m_epcHelper == 0)
  //{
  ueDevice->GetObject<MmWaveUeNetDevice> ()->SetTargetEnb (
      targetEnbDevice->GetObject<MmWaveEnbNetDevice> ());
  //}
}

void
MmWaveHelper::AttachToIabWithIndex (Ptr<NetDevice> ueDevice, NetDeviceContainer iabDevices,
                                    uint32_t index)
{
  NS_LOG_FUNCTION (this << ueDevice << iabDevices.GetN () << index);
  NS_ASSERT_MSG (iabDevices.GetN () > 0, "empty iab device container");

  // select the IAB with the given index
  Ptr<NetDevice> targetIabDevice = iabDevices.Get (index);
  NS_ASSERT (targetIabDevice);

  // connect the UE to the target IAB
  Ptr<MmWaveUeNetDevice> mmWaveUe = ueDevice->GetObject<MmWaveUeNetDevice> ();

  // Necessary operation to connect MmWave UE to IAB at lower layers
  for (NetDeviceContainer::Iterator i = iabDevices.Begin (); i != iabDevices.End (); ++i)
    {
      Ptr<MmWaveIabNetDevice> mmWaveIab = (*i)->GetObject<MmWaveIabNetDevice> ();

      std::map<uint8_t, Ptr<MmWaveComponentCarrier>> duCcMap = mmWaveIab->GetDuCcMap ();

      // TODO here I have to pair UE CC and eNB CC with the same CCid, CCs with different
      // IDs cannot communicate
      for (const auto &itIab : duCcMap)
        {
          Ptr<MmWaveComponentCarrierEnb> ccIab =
              DynamicCast<MmWaveComponentCarrierEnb> (itIab.second);

          uint16_t mmWaveCellId = ccIab->GetCellId ();
          Ptr<MmWavePhyMacCommon> configParams = ccIab->GetPhy ()->GetConfigurationParameters ();
          ccIab->GetPhy ()->AddUePhy (mmWaveUe->GetImsi (), ueDevice);
          // register MmWave IABs informations in the MmWaveUePhy

          std::map<uint8_t, Ptr<MmWaveComponentCarrier>> ueCcMap = mmWaveUe->GetCcMap ();
          for (const auto &itUe : ueCcMap)
            {
              DynamicCast<MmWaveComponentCarrierUe> (itUe.second)
                  ->GetPhy ()
                  ->RegisterOtherIab (mmWaveCellId, configParams, mmWaveIab);
            }
          //closestMmWave->GetMac ()->AssociateUeMAC (mcDevice->GetImsi ()); //TODO this does not do anything
          NS_LOG_INFO ("mmWaveCellId " << mmWaveCellId);
        }
    }

  // TODO check: the initial access is performed by the PCC, the method RegisterToEnb
  // should be called only on the PCC PHY. The configuration of the SCCs will be
  // performed by UE RRC during the connection setup phase
  uint16_t cellId = targetIabDevice->GetObject<MmWaveIabNetDevice> ()->GetCellId ();
  Ptr<MmWavePhyMacCommon> configParams =
      targetIabDevice->GetObject<MmWaveIabNetDevice> ()->GetDuPhy ()->GetConfigurationParameters ();

  // this has alread been called in the for loop above
  targetIabDevice->GetObject<MmWaveIabNetDevice> ()->GetDuPhy ()->AddUePhy (
      ueDevice->GetObject<MmWaveUeNetDevice> ()->GetImsi (), ueDevice);
  ueDevice->GetObject<MmWaveUeNetDevice> ()->GetPhy ()->RegisterToIab (cellId, configParams);
  targetIabDevice->GetObject<MmWaveIabNetDevice> ()->GetDuMac ()->AssociateUeMAC (
      ueDevice->GetObject<MmWaveUeNetDevice> ()->GetImsi ());

  // connect to the target one
  Ptr<EpcUeNas> ueNas = ueDevice->GetObject<MmWaveUeNetDevice> ()->GetNas ();
  ueNas->Connect (
      targetIabDevice->GetObject<MmWaveIabNetDevice> ()->GetCellId (),
      targetIabDevice->GetObject<MmWaveIabNetDevice> ()->GetEarfcn ()); //TODO: check state earfcn

  if (m_epcHelper)
    {
      // activate default EPS bearer
      m_epcHelper->ActivateEpsBearer (
          ueDevice, ueDevice->GetObject<MmWaveUeNetDevice> ()->GetImsi (), EpcTft::Default (),
          EpsBearer (EpsBearer::NGBR_VIDEO_TCP_DEFAULT));
    }

  // tricks needed for the simplified LTE-only simulations
  //if (m_epcHelper == 0)
  //{
  ueDevice->GetObject<MmWaveUeNetDevice> ()->SetTargetIab (
      targetIabDevice->GetObject<MmWaveIabNetDevice> ());
  //}
}

void
MmWaveHelper::AddX2Interface (NodeContainer enbNodes)
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT_MSG (m_epcHelper, "X2 interfaces cannot be set up when the EPC is not used");

  for (NodeContainer::Iterator i = enbNodes.Begin (); i != enbNodes.End (); ++i)
    {
      for (NodeContainer::Iterator j = i + 1; j != enbNodes.End (); ++j)
        {
          AddX2Interface (*i, *j);
        }
    }

  // print stats
  m_cnStats = CreateObject<CoreNetworkStatsCalculator> ();

  // add traces
  Config::Connect ("/NodeList/*/$ns3::EpcX2/RxPDU",
                   MakeCallback (&CoreNetworkStatsCalculator::LogX2Packet, m_cnStats));
}

void
MmWaveHelper::AddX2Interface (Ptr<Node> enbNode1, Ptr<Node> enbNode2)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("setting up the X2 interface");

  m_epcHelper->AddX2Interface (enbNode1, enbNode2);
}

void
MmWaveHelper::AddX2Interface (NodeContainer lteEnbNodes, NodeContainer mmWaveEnbNodes)
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT_MSG (m_epcHelper, "X2 interfaces cannot be set up when the EPC is not used");

  for (NodeContainer::Iterator i = mmWaveEnbNodes.Begin (); i != mmWaveEnbNodes.End (); ++i)
    {
      for (NodeContainer::Iterator j = i + 1; j != mmWaveEnbNodes.End (); ++j)
        {
          AddX2Interface (*i, *j);
        }
    }
  for (NodeContainer::Iterator i = mmWaveEnbNodes.Begin (); i != mmWaveEnbNodes.End (); ++i)
    {
      // get the position of the mmWave eNB
      Vector mmWavePos = (*i)->GetObject<MobilityModel> ()->GetPosition ();
      double minDistance = std::numeric_limits<double>::infinity ();
      Ptr<Node> closestLteNode;
      for (NodeContainer::Iterator j = lteEnbNodes.Begin (); j != lteEnbNodes.End (); ++j)
        {
          AddX2Interface (*i, *j);
          Vector ltePos = (*j)->GetObject<MobilityModel> ()->GetPosition ();
          double distance = CalculateDistance (mmWavePos, ltePos);
          if (distance < minDistance)
            {
              minDistance = distance;
              closestLteNode = *j;
            }
        }

      // get closestLteNode cellId and store it in the MmWaveEnb RRC
      Ptr<LteEnbNetDevice> closestEnbDevice =
          closestLteNode->GetDevice (0)->GetObject<LteEnbNetDevice> ();
      if (closestEnbDevice)
        {
          uint16_t lteCellId = closestEnbDevice->GetRrc ()->GetCellId ();
          NS_LOG_LOGIC ("ClosestLteCellId " << lteCellId);
          (*i)->GetDevice (0)->GetObject<MmWaveEnbNetDevice> ()->GetRrc ()->SetClosestLteCellId (
              lteCellId);
        }
      else
        {
          NS_FATAL_ERROR ("LteDevice not retrieved");
        }
    }
  for (NodeContainer::Iterator i = lteEnbNodes.Begin (); i != lteEnbNodes.End (); ++i)
    {
      for (NodeContainer::Iterator j = i + 1; j != lteEnbNodes.End (); ++j)
        {
          AddX2Interface (*i, *j);
        }
    }
  // print stats
  if (!m_cnStats)
    {
      m_cnStats = CreateObject<CoreNetworkStatsCalculator> ();
    }

  // add traces
  Config::Connect ("/NodeList/*/$ns3::EpcX2/RxPDU",
                   MakeCallback (&CoreNetworkStatsCalculator::LogX2Packet, m_cnStats));
}

void
MmWaveHelper::SetEpcHelper (Ptr<EpcHelper> epcHelper)
{
  m_epcHelper = epcHelper;
}

class MmWaveDrbActivator : public SimpleRefCount<MmWaveDrbActivator>
{
public:
  MmWaveDrbActivator (Ptr<NetDevice> ueDevice, EpsBearer bearer);
  static void ActivateCallback (Ptr<MmWaveDrbActivator> a, std::string context, uint64_t imsi,
                                uint16_t cellId, uint16_t rnti);
  void ActivateDrb (uint64_t imsi, uint16_t cellId, uint16_t rnti);

private:
  bool m_active;
  Ptr<NetDevice> m_ueDevice;
  EpsBearer m_bearer;
  uint64_t m_imsi;
};

MmWaveDrbActivator::MmWaveDrbActivator (Ptr<NetDevice> ueDevice, EpsBearer bearer)
    : m_active (false), m_ueDevice (ueDevice), m_bearer (bearer)
{
  if (m_ueDevice->GetObject<MmWaveUeNetDevice> ())
    { // mmWave
      m_imsi = m_ueDevice->GetObject<MmWaveUeNetDevice> ()->GetImsi ();
    }
  else if (m_ueDevice->GetObject<McUeNetDevice> ())
    {
      m_imsi = m_ueDevice->GetObject<McUeNetDevice> ()->GetImsi (); // TODO support for LTE part
    }
}

void
MmWaveDrbActivator::ActivateCallback (Ptr<MmWaveDrbActivator> a, std::string context, uint64_t imsi,
                                      uint16_t cellId, uint16_t rnti)
{
  NS_LOG_FUNCTION (a << context << imsi << cellId << rnti);
  a->ActivateDrb (imsi, cellId, rnti);
}

void
MmWaveDrbActivator::ActivateDrb (uint64_t imsi, uint16_t cellId, uint16_t rnti)
{
  NS_LOG_FUNCTION (this << imsi << cellId << rnti << m_active);
  if ((!m_active) && (imsi == m_imsi))
    {
      Ptr<LteUeRrc> ueRrc = m_ueDevice->GetObject<MmWaveUeNetDevice> ()->GetRrc ();
      NS_ASSERT (ueRrc->GetState () == LteUeRrc::CONNECTED_NORMALLY);
      uint16_t rnti = ueRrc->GetRnti ();
      Ptr<MmWaveEnbNetDevice> enbLteDevice =
          m_ueDevice->GetObject<MmWaveUeNetDevice> ()->GetTargetEnb ();
      Ptr<LteEnbRrc> enbRrc = enbLteDevice->GetObject<MmWaveEnbNetDevice> ()->GetRrc ();
      NS_ASSERT (ueRrc->GetCellId () == enbLteDevice->GetCellId ());
      Ptr<UeManager> ueManager = enbRrc->GetUeManager (rnti);
      NS_ASSERT (ueManager->GetState () == UeManager::CONNECTED_NORMALLY ||
                 ueManager->GetState () == UeManager::CONNECTION_RECONFIGURATION);
      EpcEnbS1SapUser::DataRadioBearerSetupRequestParameters params;
      params.rnti = rnti;
      params.bearer = m_bearer;
      params.bearerId = 0;
      params.gtpTeid = 0; // don't care
      enbRrc->GetS1SapUser ()->DataRadioBearerSetupRequest (params);
      m_active = true;
    }
}

// TODO this does not support yet Mc devices
void
MmWaveHelper::ActivateDataRadioBearer (NetDeviceContainer ueDevices, EpsBearer bearer)
{
  NS_LOG_FUNCTION (this);
  for (NetDeviceContainer::Iterator i = ueDevices.Begin (); i != ueDevices.End (); ++i)
    {
      ActivateDataRadioBearer (*i, bearer);
    }
}
void
MmWaveHelper::ActivateDataRadioBearer (Ptr<NetDevice> ueDevice, EpsBearer bearer)
{
  NS_LOG_FUNCTION (this << ueDevice);
  //NS_ASSERT_MSG (m_epcHelper == 0, "this method must not be used when the EPC is being used");

  // Normally it is the EPC that takes care of activating DRBs
  // when the UE gets connected. When the EPC is not used, we achieve
  // the same behavior by hooking a dedicated DRB activation function
  // to the Enb RRC Connection Established trace source

  Ptr<MmWaveEnbNetDevice> enbmmWaveDevice =
      ueDevice->GetObject<MmWaveUeNetDevice> ()->GetTargetEnb ();

  Ptr<MmWaveIabNetDevice> iabMmWaveDevice =
      ueDevice->GetObject<MmWaveUeNetDevice> ()->GetTargetIab ();

  if (enbmmWaveDevice)
    {
      std::ostringstream path;
      path << "/NodeList/" << enbmmWaveDevice->GetNode ()->GetId () << "/DeviceList/"
           << enbmmWaveDevice->GetIfIndex () << "/LteEnbRrc/ConnectionEstablished";
      Ptr<MmWaveDrbActivator> arg = Create<MmWaveDrbActivator> (ueDevice, bearer);
      Config::Connect (path.str (), MakeBoundCallback (&MmWaveDrbActivator::ActivateCallback, arg));
    }
  else
    {
      std::ostringstream path;
      path << "/NodeList/" << iabMmWaveDevice->GetNode ()->GetId () << "/DeviceList/"
           << iabMmWaveDevice->GetIfIndex () << "/LteEnbRrc/ConnectionEstablished";
      Ptr<MmWaveDrbActivator> arg = Create<MmWaveDrbActivator> (ueDevice, bearer);
      Config::Connect (path.str (), MakeBoundCallback (&MmWaveDrbActivator::ActivateCallback, arg));
    }
}

void
MmWaveHelper::EnableTraces (void)
{
  EnableDlPhyTrace ();
  EnableUlPhyTrace ();
  //EnableEnbSchedTrace ();
  //EnableTransportBlockTrace (); //the callback does nothing
  //EnableRlcTraces ();
  //EnablePdcpTraces ();
  //EnableMcTraces ();
}

void
MmWaveHelper::EnableEnbSchedTrace ()
{
  // eNB net devices
  Config::ConnectWithoutContextFailSafe (
      "/NodeList/*/DeviceList/*/ComponentCarrierMap/*/MmWaveEnbMac/SchedulingTraceEnb",
      MakeBoundCallback (&MmWaveMacTrace::ReportEnbSchedulingInfo, m_enbStats));

  // IAB devices
  Config::ConnectWithoutContextFailSafe (
      "/NodeList/*/DeviceList/*/DuComponentCarrierMap/*/MmWaveEnbMac/SchedulingTraceEnb",
      MakeBoundCallback (&MmWaveMacTrace::ReportEnbSchedulingInfo, m_enbStats));
}

void
MmWaveHelper::EnableBapTrace ()
{
  // // eNB net devices, TX packets
  // Config::ConnectWithoutContextFailSafe (
  //     "/NodeList/*/DeviceList/*/Bap/TxPacket",
  //     MakeBoundCallback (&MmWaveBapTrace::ReportPacketTx, m_bapStats));

  // // IAB devices, TX packets
  // Config::ConnectWithoutContextFailSafe (
  //     "/NodeList/*/DeviceList//*/Bap/TxPacket",
  //     MakeBoundCallback (&MmWaveBapTrace::ReportPacketTx, m_bapStats));

  //TODO: add packet rx cb
}

// TODO traces for MC

void
MmWaveHelper::EnableDlPhyTrace (void)
{
  //NS_LOG_FUNCTION_NOARGS ();
  //Config::Connect ("/NodeList/*/DeviceList/*/MmWaveUePhy/ReportCurrentCellRsrpSinr",
  //        MakeBoundCallback (&MmWavePhyTrace::ReportCurrentCellRsrpSinrCallback, m_phyStats));

  Config::ConnectWithoutContextFailSafe (
      "/NodeList/*/DeviceList/*/LteEnbComponentCarrierManager/*/MmWaveEnbPhy/"
      "ReportDlPhyTransmission",
      MakeBoundCallback (&MmWavePhyTrace::ReportDlPhyTransmissionCallback, m_phyStats));

  // IAB device DU
  Config::ConnectWithoutContextFailSafe (
      "/NodeList/*/DeviceList/*/ComponentCarrierMap/*/DuComponentCarrierMap/"
      "ReportDlPhyTransmission",
      MakeBoundCallback (&MmWavePhyTrace::ReportDlPhyTransmissionCallback, m_phyStats));

  // regulare mmWave UE device
  Config::ConnectFailSafe (
      "/NodeList/*/DeviceList/*/ComponentCarrierMap/*/MmWaveUePhy/DlSpectrumPhy/RxPacketTraceUe",
      MakeBoundCallback (&MmWavePhyTrace::RxPacketTraceUeCallback, m_phyStats));

  // MC ue device
  Config::ConnectFailSafe (
      "/NodeList/*/DeviceList/*/MmWaveComponentCarrierMapUe/*/MmWaveUePhy/DlSpectrumPhy/"
      "RxPacketTraceUe",
      MakeBoundCallback (&MmWavePhyTrace::RxPacketTraceUeCallback, m_phyStats));

  // IAB device MT
  Config::ConnectFailSafe (
      "/NodeList/*/DeviceList/*/MtComponentCarrierMap/*/MmWaveUePhy/DlSpectrumPhy/"
      "RxPacketTraceUe",
      MakeBoundCallback (&MmWavePhyTrace::RxPacketTraceUeCallback, m_phyStats));
}

void
MmWaveHelper::EnableUlPhyTrace (void)
{
  NS_LOG_FUNCTION_NOARGS ();
  Config::ConnectWithoutContextFailSafe (
      "/NodeList/*/DeviceList/*/ComponentCarrierMap/*/MmWaveUePhy/ReportUlPhyTransmission",
      MakeBoundCallback (&MmWavePhyTrace::ReportUlPhyTransmissionCallback, m_phyStats));

  // IAB MT
  Config::ConnectWithoutContextFailSafe (
      "/NodeList/*/DeviceList/*/MtComponentCarrierMap/*/MmWaveUePhy/ReportUlPhyTransmission",
      MakeBoundCallback (&MmWavePhyTrace::ReportUlPhyTransmissionCallback, m_phyStats));

  Config::ConnectFailSafe (
      "/NodeList/*/DeviceList/*/ComponentCarrierMap/*/MmWaveEnbPhy/DlSpectrumPhy/RxPacketTraceEnb",
      MakeBoundCallback (&MmWavePhyTrace::RxPacketTraceEnbCallback, m_phyStats));

  // IAB device DU
  Config::ConnectFailSafe (
      "/NodeList/*/DeviceList/*/DuComponentCarrierMap/*/MmWaveEnbPhy/DlSpectrumPhy/"
      "RxPacketTraceEnb",
      MakeBoundCallback (&MmWavePhyTrace::RxPacketTraceEnbCallback, m_phyStats));
}

void
MmWaveHelper::EnableTransportBlockTrace ()
{
  NS_LOG_FUNCTION_NOARGS ();
  Config::ConnectFailSafe (
      "/NodeList/*/DeviceList/*/ComponentCarrierMapUe/*/MmWaveUePhy/ReportDownlinkTbSize",
      MakeBoundCallback (&MmWavePhyTrace::ReportDownLinkTBSize, m_phyStats));
}

void
MmWaveHelper::EnableRlcTraces (void)
{
  NS_ASSERT_MSG (!m_rlcStats,
                 "please make sure that MmWaveHelper::EnableRlcTraces is called at most once");
  m_rlcStats = CreateObject<MmWaveBearerStatsCalculator> ("RLC");
  m_radioBearerStatsConnector->EnableRlcStats (m_rlcStats);
}

Ptr<MmWaveBearerStatsCalculator>
MmWaveHelper::GetRlcStats (void)
{
  return m_rlcStats;
}

void
MmWaveHelper::EnablePdcpTraces (void)
{
  NS_ASSERT_MSG (!m_pdcpStats,
                 "please make sure that MmWaveHelper::EnablePdcpTraces is called at most once");
  m_pdcpStats = CreateObject<MmWaveBearerStatsCalculator> ("PDCP");
  m_radioBearerStatsConnector->EnablePdcpStats (m_pdcpStats);
}

Ptr<MmWaveBearerStatsCalculator>
MmWaveHelper::GetPdcpStats (void)
{
  return m_pdcpStats;
}

void
MmWaveHelper::EnableMcTraces (void)
{
  NS_ASSERT_MSG (!m_mcStats,
                 "please make sure that MmWaveHelper::EnableMcTraces is called at most once");
  m_mcStats = CreateObject<McStatsCalculator> ();
  m_radioBearerStatsConnector->EnableMcStats (m_mcStats);
}

Ptr<McStatsCalculator>
MmWaveHelper::GetMcStats (void)
{
  return m_mcStats;
}

std::string
MmWaveHelper::GetEnbComponentCarrierManagerType () const
{
  return m_enbComponentCarrierManagerFactory.GetTypeId ().GetName ();
}

void
MmWaveHelper::SetEnbComponentCarrierManagerType (std::string type)
{
  NS_LOG_FUNCTION (this << type);
  m_enbComponentCarrierManagerFactory = ObjectFactory ();
  m_enbComponentCarrierManagerFactory.SetTypeId (type);
}

std::string
MmWaveHelper::GetLteEnbComponentCarrierManagerType () const
{
  return m_lteEnbComponentCarrierManagerFactory.GetTypeId ().GetName ();
}

void
MmWaveHelper::SetLteEnbComponentCarrierManagerType (std::string type)
{
  NS_LOG_FUNCTION (this << type);
  m_lteEnbComponentCarrierManagerFactory = ObjectFactory ();
  m_lteEnbComponentCarrierManagerFactory.SetTypeId (type);
}

std::string
MmWaveHelper::GetUeComponentCarrierManagerType () const
{
  return m_ueComponentCarrierManagerFactory.GetTypeId ().GetName ();
}

void
MmWaveHelper::SetUeComponentCarrierManagerType (std::string type)
{
  NS_LOG_FUNCTION (this << type);
  m_ueComponentCarrierManagerFactory = ObjectFactory ();
  m_ueComponentCarrierManagerFactory.SetTypeId (type);
}

} // namespace mmwave
} // namespace ns3
