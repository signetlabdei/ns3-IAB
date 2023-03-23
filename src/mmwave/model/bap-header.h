/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 University of Padova, Dep. of Information Engineering, SIGNET lab.
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
 *   Authors: Matteo Pagin <paginmatte@dei.unipd.it>
 *            Alessandro Traspadini <traspadini@dei.unipd.it>
 */

#ifndef BAP_HEADER_H
#define BAP_HEADER_H

#include <ns3/header.h>

namespace ns3 {
/**
 * \ingroup bap
 * \brief Packet header for BAP packets
 *
 * This class has fields corresponding to those in a network BAP header
 * (port numbers, payload size, checksum) as well as methods for serialization
 * to and deserialization from a byte buffer.
 */
class BapHeader : public Header
{
public:
  enum BapDcField : bool { Data = true, Control = false };
  /**
     * \brief Constructor
     *
     * Creates a null header
     */
  BapHeader ();
  ~BapHeader () override;

  /**
     * \param port the destination port for this BAPHeader
     */
  void SetDestinationAddress (uint16_t port);
  /**
     * \return the destination port for this BAPHeader
     */
  uint16_t GetDestinationAddress () const;

  void SetPathId (uint16_t id);

  uint16_t GetPathId () const;

  void SetDcField (BapDcField dc);

  BapDcField GetDcField () const;

  void SetReservedBits (bool first, bool second, bool third);

  uint8_t GetReservedBits ();

  /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const override;
  void Print (std::ostream &os) const override;
  uint32_t GetSerializedSize () const override;
  void Serialize (Buffer::Iterator start) const override;
  uint32_t Deserialize (Buffer::Iterator start) override;

private:
  uint16_t m_destAddress;
  uint16_t m_pathId;
  bool m_dcField;
  uint8_t m_reservedBits;
};

} // namespace ns3

#endif /* BAP_HEADER */
