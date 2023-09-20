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

#include "ns3/applications-module.h"
#include "ns3/config-store.h"
#include "ns3/core-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mmwave-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include <ns3/buildings-helper.h>
#include <ns3/isotropic-antenna-model.h>

using namespace ns3;
using namespace mmwave;

int
main(int argc, char* argv[])
{

    uint16_t numEnb = 1;
    uint16_t numUe = 1;
    double bw = 400e6;
    double fc = 26e9;
    double dist = 100.0;
    double simTime = 10.0;
    std::string cbPath = "src/mmwave/model/Codebooks/4x6.txt";

    // Command line arguments
    CommandLine cmd;
    cmd.AddValue("dist", "Distance", dist);
    cmd.AddValue ("cbPath", "The full path to the codebook", cbPath);

    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::MmWaveFlexTtiMacScheduler::UlSchedOnly", BooleanValue(true));
    Config::SetDefault("ns3::ThreeGppChannelModel::UpdatePeriod", TimeValue(Seconds(0.01 * simTime)));
    Config::SetDefault("ns3::PhasedArrayModel::AntennaElement",
                       PointerValue(CreateObject<IsotropicAntennaModel>()));
    Config::SetDefault("ns3::ThreeGppPropagationLossModel::ShadowingEnabled", BooleanValue(false));
    Config::SetDefault("ns3::LteEnbRrc::SystemInformationPeriodicity",
                       TimeValue(MilliSeconds(1.0)));
    Config::SetDefault("ns3::MmWaveAmc::Ber", DoubleValue(0.001));
    Config::SetDefault("ns3::MmWaveHelper::UseIdealRrc", BooleanValue(true));
    Config::SetDefault("ns3::MmWavePhyMacCommon::CenterFreq", DoubleValue(fc));
    Config::SetDefault("ns3::MmWavePhyMacCommon::Bandwidth", DoubleValue(bw));

    Ptr<MmWaveHelper> mmwHelper = CreateObject<MmWaveHelper>();
    mmwHelper->SetChannelConditionModelType("ns3::AlwaysLosChannelConditionModel");
    mmwHelper->SetHarqEnabled(true);

    mmwHelper->SetUePhasedArrayModelAttribute("NumColumns", UintegerValue(6));
    mmwHelper->SetUePhasedArrayModelAttribute("NumRows", UintegerValue(4));
    mmwHelper->SetEnbPhasedArrayModelAttribute("NumColumns", UintegerValue(6));
    mmwHelper->SetEnbPhasedArrayModelAttribute("NumRows", UintegerValue(4));
    mmwHelper->SetBeamformingModelType("ns3::MmWaveCodebookBeamforming");

    mmwHelper->SetUeBeamformingCodebookAttribute("CodebookFilename", StringValue(cbPath));
    mmwHelper->SetEnbBeamformingCodebookAttribute("CodebookFilename", StringValue(cbPath));

    /* A configuration example.
     * mmwHelper->GetCcPhyParams ().at (0).GetConfigurationParameters
     * ()->SetAttribute("SymbolPerSlot", UintegerValue(30)); */

    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(numEnb);
    ueNodes.Create(numUe);

    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(0.0, 0.0, 0.0));

    MobilityHelper enbmobility;
    enbmobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbmobility.SetPositionAllocator(enbPositionAlloc);
    enbmobility.Install(enbNodes);
    BuildingsHelper::Install(enbNodes);

    MobilityHelper uemobility;
    Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator>();
    uePositionAlloc->Add(Vector(dist, 0.0, 0.0));


    uemobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    uemobility.SetPositionAllocator(uePositionAlloc);
    uemobility.Install(ueNodes);
    BuildingsHelper::Install(ueNodes);

    NetDeviceContainer enbNetDev = mmwHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueNetDev = mmwHelper->InstallUeDevice(ueNodes);

    mmwHelper->AttachToClosestEnb(ueNetDev, enbNetDev);
    mmwHelper->EnableTraces();

    // Activate a data radio bearer
    enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
    EpsBearer bearer(q);
    mmwHelper->ActivateDataRadioBearer(ueNetDev, bearer);

    Simulator::Stop(Seconds(simTime));
    NS_LOG_UNCOND("Simulation running for " << simTime << " seconds");
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
