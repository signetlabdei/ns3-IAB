/*
 * Copyright (c) 2019 SIGNET Lab, Department of Information Engineering,
 * University of Padova
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
 */

#include "two-ray-propagation-loss-model.h"
#include "ns3/boolean.h"
#include "ns3/channel-condition-model.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/node.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"

#include <complex>
#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TwoRayPropagationLossModel");

static const double M_C = 3.0e8; //!< propagation velocity in free space

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED(TwoRayPropagationLossModel);

TypeId
TwoRayPropagationLossModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TwoRayPropagationLossModel")
            .SetParent<PropagationLossModel>()
            .SetGroupName("Propagation")
            .AddConstructor<TwoRayPropagationLossModel>()
            .AddAttribute("Frequency",
                          "The centre frequency in Hz.",
                          DoubleValue(500.0e6),
                          MakeDoubleAccessor(&TwoRayPropagationLossModel::SetFrequency,
                                             &TwoRayPropagationLossModel::GetFrequency),
                          MakeDoubleChecker<double>())
            .AddAttribute(
                "ChannelConditionModel",
                "Pointer to the channel condition model.",
                PointerValue(),
                MakePointerAccessor(&TwoRayPropagationLossModel::SetChannelConditionModel,
                                    &TwoRayPropagationLossModel::GetChannelConditionModel),
                MakePointerChecker<ChannelConditionModel>())
            .AddAttribute("RainRate",
                          "The rain rate in mm/h.",
                          DoubleValue(0.1),
                          MakeDoubleAccessor(&TwoRayPropagationLossModel::m_rainRate),
                          MakeDoubleChecker<double>());
    return tid;
}

TwoRayPropagationLossModel::TwoRayPropagationLossModel()
    : PropagationLossModel()
{
    NS_LOG_FUNCTION(this);
}

TwoRayPropagationLossModel::~TwoRayPropagationLossModel()
{
    NS_LOG_FUNCTION(this);
}

void
TwoRayPropagationLossModel::DoDispose()
{
    m_channelConditionModel->Dispose();
    m_channelConditionModel = nullptr;
}

void
TwoRayPropagationLossModel::SetChannelConditionModel(Ptr<ChannelConditionModel> model)
{
    NS_LOG_FUNCTION(this);
    m_channelConditionModel = model;
}

Ptr<ChannelConditionModel>
TwoRayPropagationLossModel::GetChannelConditionModel() const
{
    NS_LOG_FUNCTION(this);
    return m_channelConditionModel;
}

void
TwoRayPropagationLossModel::SetFrequency(double f)
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT_MSG(f >= 500.0e6 && f <= 100.0e9,
                  "Frequency should be between 0.5 and 100 GHz but is " << f);
    m_frequency = f;

    // Eq. 7 from "Novel Maritime Channel Models for Millimeter Radiowaves"
    m_alpha = 1.091 * exp(-0.06256 * m_frequency) + 0.06982;
}

double
TwoRayPropagationLossModel::GetFrequency() const
{
    NS_LOG_FUNCTION(this);
    return m_frequency;
}

double
TwoRayPropagationLossModel::DoCalcRxPower(double txPowerDbm,
                                            Ptr<MobilityModel> a,
                                            Ptr<MobilityModel> b) const
{
    NS_LOG_FUNCTION(this);

    // check if the model is initialized
    NS_ASSERT_MSG(m_frequency != 0.0, "First set the centre frequency");

    // retrieve the channel condition
    NS_ASSERT_MSG(m_channelConditionModel, "First set the channel condition model");
    Ptr<ChannelCondition> cond = m_channelConditionModel->GetChannelCondition(a, b);

    // compute the 2D distance between a and b
    double distance2d = Calculate2dDistance(a->GetPosition(), b->GetPosition());
    NS_LOG_DEBUG("Computing distance between nodes at positions: " <<
                 a->GetPosition() << " " << b->GetPosition());

    // compute the 3D distance between a and b
    double distance3d = CalculateDistance(a->GetPosition(), b->GetPosition());

    // compute hUT and hBS
    std::pair<double, double> heights = GetUtAndBsHeights(a->GetPosition().z, b->GetPosition().z);

    double rxPow = txPowerDbm;
    rxPow -= GetLoss(cond, distance2d, distance3d, heights.first, heights.second);

    return rxPow;
}

double
TwoRayPropagationLossModel::GetLoss(Ptr<ChannelCondition> cond,
                                      double distance2d,
                                      double distance3d,
                                      double hUt,
                                      double hBs) const
{
    NS_LOG_FUNCTION(this);
    // channel condition is assumed to be always LOS

    double lambda = M_C / m_frequency;
    double R = -1; // reflection coefficient
    double distSq =  distance2d * distance2d;
    double deltaD = sqrt(std::pow(hUt + hBs, 2) + distSq) - sqrt(std::pow(hUt - hBs, 2) + distSq);

    // Eq. 5 from "Novel Maritime Channel Models for Millimeter Radiowaves"
    std::complex<double> complexTerm = 1.0 + R * exp (std::complex<double> (0, 1) * m_alpha * 2.0 * M_PI * deltaD / lambda);
    double loss = -20 * log10 (sqrt(std::norm(complexTerm)) * lambda / (4 * M_PI * distance3d));

    loss += GetRainAttenuation (distance3d);
    return loss;
}

std::pair<double, double>
TwoRayPropagationLossModel::GetUtAndBsHeights(double za, double zb) const
{
    // The default implementation assumes that the tallest node is the BS and the
    // smallest is the UT.
    double hUt = std::min(za, zb);
    double hBs = std::max(za, zb);

    return std::pair<double, double>(hUt, hBs);
}

int64_t
TwoRayPropagationLossModel::DoAssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this);

    return 0;
}


uint32_t
TwoRayPropagationLossModel::GetKey(Ptr<MobilityModel> a, Ptr<MobilityModel> b)
{
    // use the nodes ids to obtain an unique key for the channel between a and b
    // sort the nodes ids so that the key is reciprocal
    uint32_t x1 = std::min(a->GetObject<Node>()->GetId(), b->GetObject<Node>()->GetId());
    uint32_t x2 = std::max(a->GetObject<Node>()->GetId(), b->GetObject<Node>()->GetId());

    // use the cantor function to obtain the key
    uint32_t key = (((x1 + x2) * (x1 + x2 + 1)) / 2) + x2;

    return key;
}

Vector
TwoRayPropagationLossModel::GetVectorDifference(Ptr<MobilityModel> a, Ptr<MobilityModel> b)
{
    uint32_t x1 = a->GetObject<Node>()->GetId();
    uint32_t x2 = b->GetObject<Node>()->GetId();

    if (x1 < x2)
    {
        return b->GetPosition() - a->GetPosition();
    }
    else
    {
        return a->GetPosition() - b->GetPosition();
    }
}

double
TwoRayPropagationLossModel::Calculate2dDistance(Vector a, Vector b)
{
    double x = a.x - b.x;
    double y = a.y - b.y;
    double distance2D = sqrt(x * x + y * y);

    return distance2D;
}

// Maps containg the coefficients to compute the rain attenuation for vertical polarization.
// From Tables 2,4 of ITU-R P.838.3

const std::map<int, std::vector<double>> k_v{
    {1, {-3.80595, 0.56934, 0.81061}},
    {2, {-3.44965, -0.22911, 0.51059}},
    {3, {-0.39902, 0.73042, 0.11899}},
    {4, {0.50167, 1.07319, 0.27195}},
};

const std::map<int, std::vector<double>> alpha_v{
    {1, {-0.07771, 2.33840, -0.76284}},
    {2, {0.56727, 0.95545, 0.54039}},
    {3, {-0.20238, 1.14520, 0.26809}},
    {4, {-48.2991, 0.791669, 0.116226}},
    {5, {48.5833, 0.791459, 0.116479}},
};

const double m_k = -0.16398;
const double c_k = 0.63297;
const double m_alpha = -0.053739;
const double c_alpha = 0.83433;

double
TwoRayPropagationLossModel::GetRainAttenuation(double distance) const
{
    double alpha = 0;
    double k = 0;

    // Eq. 2 from ITU-R P.838.3
    for (auto elem : alpha_v)
    {
        std::vector<double> coeff = elem.second;
        alpha += coeff[0] * exp(-std::pow(((log10(m_frequency/1e9) - coeff[1]) / coeff[2]), 2));
    }
    alpha += m_alpha * log10(m_frequency/1e9) + c_alpha;

    // Eq. 3 from ITU-R P.838.3
    for (auto elem : k_v)
    {
        std::vector<double> coeff = elem.second;
        k += coeff[0] * exp(-std::pow(((log10(m_frequency/1e9) - coeff[1]) / coeff[2]), 2));
    }
    k += m_k * log10(m_frequency/1e9) + c_k;
    k = std::pow(10, k);

    // Eq. 1 from ITU-R P.838.3
    double rainAttenuation = k * std::pow (m_rainRate, alpha) * distance/1000;
    return rainAttenuation;
}

// namespace ns3
}
