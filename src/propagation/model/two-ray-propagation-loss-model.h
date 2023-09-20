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
 * \brief Base class for the 3GPP propagation models
 */
class TwoRayPropagationLossModel : public PropagationLossModel
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();

    /**
     * Constructor
     */
    TwoRayPropagationLossModel();

    /**
     * Destructor
     */
    ~TwoRayPropagationLossModel() override;

    // Delete copy constructor and assignment operator to avoid misuse
    TwoRayPropagationLossModel(const TwoRayPropagationLossModel&) = delete;
    TwoRayPropagationLossModel& operator=(const TwoRayPropagationLossModel&) = delete;

    /**
     * \brief Set the channel condition model used to determine the channel
     *        state (e.g., the LOS/NLOS condition)
     * \param model pointer to the channel condition model
     */
    void SetChannelConditionModel(Ptr<ChannelConditionModel> model);

    /**
     * \brief Returns the associated channel condition model
     * \return the channel condition model
     */
    Ptr<ChannelConditionModel> GetChannelConditionModel() const;

    /**
     * \brief Set the central frequency of the model
     * \param f the central frequency in the range in Hz, between 500.0e6 and 100.0e9 Hz
     */
    void SetFrequency(double f);

    /**
     * \brief Return the current central frequency
     * \return The current central frequency
     */
    double GetFrequency() const;

  private:
    /**
     * Computes the received power by applying the pathloss model described in
     * 3GPP TR 38.901
     *
     * \param txPowerDbm tx power in dBm
     * \param a tx mobility model
     * \param b rx mobility model
     * \return the rx power in dBm
     */
    double DoCalcRxPower(double txPowerDbm,
                         Ptr<MobilityModel> a,
                         Ptr<MobilityModel> b) const override;

    int64_t DoAssignStreams(int64_t stream) override;

    /**
     * \brief Computes the pathloss between a and b
     * \param cond the channel condition
     * \param distance2D the 2D distance between tx and rx in meters
     * \param distance3D the 3D distance between tx and rx in meters
     * \param hUt the height of the UT in meters
     * \param hBs the height of the BS in meters
     * \return pathloss value in dB
     */
    double GetLoss(Ptr<ChannelCondition> cond,
                   double distance2D,
                   double distance3D,
                   double hUt,
                   double hBs) const;


    /**
     * \brief Determines hUT and hBS. The default implementation assumes that
     *        the tallest node is the BS and the smallest is the UT. The derived classes
     * can change the default behavior by overriding this method.
     * \param za the height of the first node in meters
     * \param zb the height of the second node in meters
     * \return std::pair of heights in meters, the first element is hUt and the second is hBs
     */
    std::pair<double, double> GetUtAndBsHeights(double za, double zb) const;


    /**
     * \brief Returns an unique key for the channel between a and b.
     *
     * The key is the value of the Cantor function calculated by using as
     * first parameter the lowest node ID, and as a second parameter the highest
     * node ID.
     *
     * \param a tx mobility model
     * \param b rx mobility model
     * \return channel key
     */
    static uint32_t GetKey(Ptr<MobilityModel> a, Ptr<MobilityModel> b);

    /**
     * \brief Get the difference between the node position
     *
     * The difference is calculated as (b-a) if Id(a) < Id (b), or
     * (a-b) if Id(b) <= Id(a).
     *
     * \param a First node
     * \param b Second node
     * \return the difference between the node vector position
     */
    static Vector GetVectorDifference(Ptr<MobilityModel> a, Ptr<MobilityModel> b);

    /**
     * Computes the rain attenuation based on the model described in
     * ITU-R P.838-3
     *
     * \param distance 3D distance between tx and rx in meters
     * \return rain attenuation loss in dB
     */
    double GetRainAttenuation (double distance) const;

    double m_rainRate;

  protected:
    void DoDispose() override;

    /**
     * \brief Computes the 2D distance between two 3D vectors
     * \param a the first 3D vector
     * \param b the second 3D vector
     * \return the 2D distance between a and b
     */
    static double Calculate2dDistance(Vector a, Vector b);

    Ptr<ChannelConditionModel> m_channelConditionModel; //!< pointer to the channel condition model
    bool m_shadowingEnabled;                            //!< enable/disable shadowing
    double m_frequency;
    double m_alpha;
};

} // namespace ns3

#endif /* TWO_RAY_PROPAGATION_LOSS_MODEL_H */
