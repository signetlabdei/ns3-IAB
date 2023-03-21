/*
 * Copyright (c) 2005 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "bap-header.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (BapHeader);
NS_LOG_COMPONENT_DEFINE ("BapHeader");

/* The magic values below are used only for debugging.
 * They can be used to easily detect memory corruption
 * problems so you can see the patterns in memory.
 */
BapHeader::BapHeader ()
    : m_destAddress (0x03FF), // 2^10 - 1
      m_pathId (0x03FF), // 2^10 - 1
      m_dcField (BapDcField::Data),
      m_reservedBits (0)
{
}

BapHeader::~BapHeader ()
{
}

TypeId
BapHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::BapHeader")
                          .SetParent<Header> ()
                          .SetGroupName ("MmWave")
                          .AddConstructor<BapHeader> ();
  return tid;
}

TypeId
BapHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

void
BapHeader::SetDestinationAddress (uint16_t port)
{
  m_destAddress = port;
}

uint16_t
BapHeader::GetDestinationAddress () const
{
  return m_destAddress;
}

void
BapHeader::SetPathId (uint16_t id)
{
  m_pathId = id;
}

uint16_t
BapHeader::GetPathId () const
{
  return m_pathId;
}

void
BapHeader::SetDcField (BapDcField dc)
{
  m_dcField = dc;
}

BapHeader::BapDcField
BapHeader::GetDcField () const
{
  return BapDcField (m_dcField);
}

void
BapHeader::SetReservedBits (bool first, bool second, bool third)
{
  m_reservedBits = first * 4 + second * 2 + third;
  NS_ASSERT (m_reservedBits <= 7);
}

uint8_t
BapHeader::GetReservedBits ()
{
  return m_reservedBits;
}

void
BapHeader::Print (std::ostream &os) const
{
  os << "D/C: " << m_dcField << " reserved bits: " << m_reservedBits <<
        " path ID: " << m_pathId << " destination address: " << m_destAddress;
}

uint32_t
BapHeader::GetSerializedSize () const
{
  return 24;
}

void
BapHeader::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this);

  Buffer::Iterator i = start;
  uint8_t firstByte = 0;
  uint8_t secondByte = 0;
  uint8_t thirdByte = 0;

  firstByte |= (m_dcField << 7);
  firstByte |= (m_reservedBits << 4);
  firstByte |= (m_destAddress & 0x3C0) >> 6; // bits 10 to 7
  secondByte |= (m_destAddress & 0x3F) << 2; // bits 6 to 1
  secondByte |= (m_pathId & 0x300) >> 9; // bits 10 to 9
  thirdByte |= (m_pathId & 0xFF);

  i.WriteU8 (firstByte);
  i.WriteU8 (secondByte);
  i.WriteU8 (thirdByte);
}

uint32_t
BapHeader::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this);

  Buffer::Iterator i = start;
  uint8_t firstByte = i.ReadU8 ();
  uint8_t secondByte = i.ReadU8 ();
  uint8_t thirdByte = i.ReadU8 ();

  m_pathId = thirdByte + ((secondByte & 3) << 8);
  m_destAddress = ((secondByte & 0xFC) >> 2) + ((firstByte & 0xF) << 6);
  m_reservedBits = (firstByte & 0x70) >> 4;
  m_dcField = (firstByte & 0x80) >> 7;

  NS_ASSERT (m_pathId <= 0x3FF);
  NS_ASSERT (m_destAddress <= 0x3FF);
  NS_ASSERT (m_reservedBits <= 7);

  return GetSerializedSize ();
}

} // namespace ns3
