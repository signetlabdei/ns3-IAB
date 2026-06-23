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

#ifndef TWO_RAY_PROPAGATION_LOSS_MODEL_H
#define TWO_RAY_PROPAGATION_LOSS_MODEL_H

#include "ns3/channel-condition-model.h"
#include "ns3/propagation-loss-model.h"

namespace ns3
{

/**
 * \ingroup propagation
 *
 * \brief Two-ray ground-reflection propagation loss model with rain attenuation.
 *
 * Models maritime/IAB backhaul pathloss using a two-ray interference model
 * (direct + ground-reflected ray) combined with ITU-R P.838-3 rain attenuation.
 * Reference: "Novel Maritime Channel Models for Millimeter Radiowaves".
 */
class TwoRayPropagationLossModel : public PropagationLossModel
{
  public:
    static TypeId GetTypeId();

    TwoRayPropagationLossModel();
    ~TwoRayPropagationLossModel() override;

    TwoRayPropagationLossModel(const TwoRayPropagationLossModel&) = delete;
    TwoRayPropagationLossModel& operator=(const TwoRayPropagationLossModel&) = delete;

    void SetChannelConditionModel(Ptr<ChannelConditionModel> model);
    Ptr<ChannelConditionModel> GetChannelConditionModel() const;

    void SetFrequency(double f);
    double GetFrequency() const;

  private:
    double DoCalcRxPower(double txPowerDbm,
                         Ptr<MobilityModel> a,
                         Ptr<MobilityModel> b) const override;

    int64_t DoAssignStreams(int64_t stream) override;

    double GetLoss(Ptr<ChannelCondition> cond,
                   double distance2D,
                   double distance3D,
                   double hUt,
                   double hBs) const;

    std::pair<double, double> GetUtAndBsHeights(double za, double zb) const;

    static uint32_t GetKey(Ptr<MobilityModel> a, Ptr<MobilityModel> b);
    static Vector GetVectorDifference(Ptr<MobilityModel> a, Ptr<MobilityModel> b);

    double GetRainAttenuation(double distance) const;

    double m_rainRate;

  protected:
    void DoDispose() override;

    static double Calculate2dDistance(Vector a, Vector b);

    Ptr<ChannelConditionModel> m_channelConditionModel;
    bool m_shadowingEnabled;
    double m_frequency;
    double m_alpha;
};

} // namespace ns3

#endif /* TWO_RAY_PROPAGATION_LOSS_MODEL_H */
