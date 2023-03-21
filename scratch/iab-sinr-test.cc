/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
*   Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
*   Copyright (c) 2015, NYU WIRELESS, Tandon School of Engineering, New York University
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
*/

#include "ns3/epc-helper.h"
#include "ns3/epc-enb-application.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/command-line.h"
#include "ns3/mmwave-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/mmwave-iab-net-device.h"
#include "ns3/isotropic-antenna-model.h"
#include "ns3/channel-condition-model.h"
#include "ns3/uniform-planar-array.h"
#include "ns3/three-gpp-spectrum-propagation-loss-model.h"
#include <ns3/three-gpp-channel-model.h>
#include <sstream>
#include <string>

using namespace ns3;
using namespace mmwave;

double simTimeSeconds = 0.15;
bool useIdealRrc = false;
uint8_t m_receivedPacketCounter = 0;
double ipiMs = 100.0;
double appStartMs = 100.0;
unsigned int expectedPackets = std::ceil ((simTimeSeconds * 1e3 - appStartMs) / ipiMs)*9;

NS_LOG_COMPONENT_DEFINE ("IabE2EData1Node");

struct AppSetupParams
{
  uint16_t basePort;
  uint32_t sendSize;
  DataRate rate;
  Time IPI;
  Time startTime;
  Time endTime;
  Time simTime;
};

void
DoRun (int runIndex)
{
  Config::SetDefault ("ns3::MmWaveHelper::UseIdealRrc", BooleanValue (useIdealRrc));
  Config::SetDefault ("ns3::PhasedArrayModel::AntennaElement",
                      PointerValue (CreateObject<IsotropicAntennaModel> ()));
  Config::SetDefault ("ns3::ThreeGppPropagationLossModel::ShadowingEnabled", BooleanValue (false));

  Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper> ();
  mmwaveHelper->SetChannelConditionModelType ("ns3::AlwaysLosChannelConditionModel");

  Ptr<MmWavePointToPointEpcHelper> epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
  mmwaveHelper->SetEpcHelper (epcHelper);
  mmwaveHelper->SetHarqEnabled (true);
  mmwaveHelper->SetBeamformingModelType ("ns3::MmWaveSvdBeamforming");
  std::string dataSensorRate = "2kbps";

  uint32_t numIab = 9;

  NodeContainer iabNodes;
  NodeContainer enbNode;

  iabNodes.Create (numIab);
  enbNode.Create (1);

  Ptr<Node> pgw = epcHelper->GetPgwNode ();

  Ptr<MobilityModel> mmEnb = CreateObject<ConstantPositionMobilityModel> ();
  mmEnb->SetPosition (Vector (0.0, 0.0, 0.0));
  enbNode.Get (0)->AggregateObject (mmEnb);

  Ptr<MobilityModel> mmIab1 = CreateObject<ConstantPositionMobilityModel> ();
  mmIab1->SetPosition (Vector (20, 0.0, 0.0));
  Ptr<MobilityModel> mmIab2 = CreateObject<ConstantPositionMobilityModel> ();
  mmIab2->SetPosition (Vector (30, 10, 0.0));
  Ptr<MobilityModel> mmIab3 = CreateObject<ConstantPositionMobilityModel> ();
  mmIab3->SetPosition (Vector (20, 15, 0.0));
  Ptr<MobilityModel> mmIab4 = CreateObject<ConstantPositionMobilityModel> ();
  mmIab4->SetPosition (Vector (20, 20, 0.0));
  Ptr<MobilityModel> mmIab5 = CreateObject<ConstantPositionMobilityModel> ();
  mmIab5->SetPosition (Vector (40, 0.0, 0.0));
  Ptr<MobilityModel> mmIab6 = CreateObject<ConstantPositionMobilityModel> ();
  mmIab6->SetPosition (Vector (40, 5, 0.0));
  Ptr<MobilityModel> mmIab7 = CreateObject<ConstantPositionMobilityModel> ();
  mmIab7->SetPosition (Vector (50, 10, 0.0));
  Ptr<MobilityModel> mmIab8 = CreateObject<ConstantPositionMobilityModel> ();
  mmIab8->SetPosition (Vector (55, 15, 0.0));
  Ptr<MobilityModel> mmIab9 = CreateObject<ConstantPositionMobilityModel> ();
  mmIab9->SetPosition (Vector (60, 20, 0.0));

  // associate the nodes and the mobility models
  iabNodes.Get (0)->AggregateObject (mmIab1);
  iabNodes.Get (1)->AggregateObject (mmIab2);
  iabNodes.Get (2)->AggregateObject (mmIab3);
  iabNodes.Get (3)->AggregateObject (mmIab4);
  iabNodes.Get (4)->AggregateObject (mmIab5);
  iabNodes.Get (5)->AggregateObject (mmIab6);
  iabNodes.Get (6)->AggregateObject (mmIab7);
  iabNodes.Get (7)->AggregateObject (mmIab8);
  iabNodes.Get (8)->AggregateObject (mmIab9);

  // Install Devices to the nodes
  NetDeviceContainer iabMmWaveDev = mmwaveHelper->InstallIabDevice (iabNodes);
  NetDeviceContainer enbMmWaveDev = mmwaveHelper->InstallEnbDevice (enbNode);

  std::vector<Ptr<MobilityModel>> ChildMobilityList = {mmIab1, mmIab2, mmIab3, mmIab4, mmIab5,
                                                       mmIab6, mmIab7, mmIab8, mmIab9};

  std::vector<Ptr<MobilityModel>> ParentMobilityList = {mmEnb,  mmEnb,  mmEnb,  mmEnb, mmIab1,
                                                        mmIab1, mmIab2, mmIab7, mmIab7};

  std::vector<Ptr<NetDevice>> parentNodeList = {
      enbMmWaveDev.Get (0), enbMmWaveDev.Get (0), enbMmWaveDev.Get (0),
      enbMmWaveDev.Get (0), iabMmWaveDev.Get (0), iabMmWaveDev.Get (0),
      iabMmWaveDev.Get (1), iabMmWaveDev.Get (6), iabMmWaveDev.Get (6)};
  
  // uint8_t txAntennaElements[]{8, 8}; // tx antenna dimensions
  // uint8_t rxAntennaElements[]{8, 8}; // rx antenna dimensions

  for (uint8_t i = 0; i < iabMmWaveDev.GetN (); i++)
    {
      auto txIabDevice = DynamicCast<MmWaveIabNetDevice> (iabMmWaveDev.Get (i));
      auto rxIabDevice = DynamicCast<MmWaveIabNetDevice> (parentNodeList.at (i));
      auto rxEnbDevice = DynamicCast<MmWaveEnbNetDevice> (parentNodeList.at (i));

      auto txDuPhy = txIabDevice->GetDuPhy ();
      Ptr<SpectrumValue> txPsd = txDuPhy->CreateTxPowerSpectralDensity ();
      Ptr<SpectrumSignalParameters> txParams = Create<SpectrumSignalParameters> ();
      txParams->psd = txPsd->Copy ();

      // Ptr<PhasedArrayModel> txAntenna = CreateObjectWithAttributes<UniformPlanarArray> (
      // "NumColumns", UintegerValue (txAntennaElements[0]), "NumRows",
      // UintegerValue (txAntennaElements[1]), "AntennaElement",
      // PointerValue (CreateObject<IsotropicAntennaModel> ()));
      //  Ptr<PhasedArrayModel> rxAntenna = CreateObjectWithAttributes<UniformPlanarArray> (
      // "NumColumns", UintegerValue (rxAntennaElements[0]), "NumRows",
      // UintegerValue (rxAntennaElements[1]), "AntennaElement",
      // PointerValue (CreateObject<IsotropicAntennaModel> ()));

      Ptr<PhasedArrayModel> txAntenna = txIabDevice->GetDuAntenna (0);
      Ptr<PhasedArrayModel> rxAntenna;
      if (rxIabDevice)
        {
          rxAntenna = rxIabDevice->GetMtAntenna (0);
        }
      else if (rxEnbDevice)
        {
          rxAntenna = rxEnbDevice->GetAntenna (0);
        }

      Ptr<ChannelConditionModel> condModel = CreateObject<AlwaysLosChannelConditionModel> ();

      Ptr<ThreeGppSpectrumPropagationLossModel> lossModel =
          CreateObject<ThreeGppSpectrumPropagationLossModel> ();
      lossModel->SetChannelModelAttribute ("Frequency", DoubleValue (38e9));
      lossModel->SetChannelModelAttribute ("Scenario", StringValue ("RMa"));
      lossModel->SetChannelModelAttribute ("ChannelConditionModel", PointerValue (condModel));

      ObjectFactory m_bfModelFactory;
      m_bfModelFactory.SetTypeId (MmWaveSvdBeamforming::GetTypeId ());
      Ptr<MmWaveBeamformingModel> bfModel = m_bfModelFactory.Create<MmWaveBeamformingModel> ();
      bfModel->SetAttributeFailSafe ("Device", PointerValue (txIabDevice));
      bfModel->SetAttributeFailSafe ("Antenna", PointerValue (txAntenna));
      bfModel->SetAttributeFailSafe ("ChannelModel", PointerValue (lossModel->GetChannelModel ()));
      txIabDevice->GetMtPhy ()->GetUlSpectrumPhy ()->SetBeamformingModel (bfModel);

      if (rxIabDevice)
        {
          txIabDevice->GetMtPhy ()->GetUlSpectrumPhy ()->ConfigureBeamformingForGivenAntenna (
              rxIabDevice, rxAntenna);
        }
       else
         {
           txIabDevice->GetMtPhy ()->GetUlSpectrumPhy ()->ConfigureBeamformingForGivenAntenna (
               rxEnbDevice, rxAntenna);
         }



      Ptr<SpectrumValue> rxPsdOld = lossModel->DoCalcRxPowerSpectralDensity (
          txParams, ChildMobilityList.at (i), ParentMobilityList.at (i), txAntenna, rxAntenna);
      auto rxPw = Integral (*rxPsdOld);
      double rxPwDbm = 10 * log10 (rxPw) + 30;

      NS_LOG_UNCOND (+runIndex << " " << +i << " " << rxPwDbm);
    }

  // Simulator::Schedule (Seconds (simTimeSeconds - 0.01), &CheckAppReceptionCallback);

  // Simulator::Stop (Seconds (0.15));
  // Simulator::Run ();
  Simulator::Destroy ();

}

int
main (int argc, char *argv[])
{
  // Command line arguments
  CommandLine cmd;
  cmd.AddValue ("useIdealRrc", "Whether to us ea simplified RRC model", useIdealRrc);
  cmd.Parse (argc, argv);

  NS_LOG_UNCOND ("RealizationNumber IndexNode Power");

  uint16_t run = 100;
  for (auto i = 0; i < run; i++)
    {
      DoRun (i);
    }
  return 0;
}
