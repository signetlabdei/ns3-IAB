/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
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
*/

#include "ns3/buffer.h"
#include "ns3/test.h"
#include "ns3/bap-header.h"

NS_LOG_COMPONENT_DEFINE ("BapHeaderTestSuite");

using namespace ns3;
/**
* This test case checks if the MmWaveHelper correctly initializes the
* device antennas
*/
class BapHeaderTestCase : public TestCase
{
public:
  /**
  * Constructor
  */
  BapHeaderTestCase (uint16_t destinationAddress, uint16_t pathId, bool firstReservedBit,
                     bool secondReservedBit, bool ThirdReservedBit, BapHeader::BapDcField dcField)
      : TestCase ("Veryfing the correct serialization of the BAP header"),
        m_destinationAddress (destinationAddress),
        m_pathId (pathId),
        m_firstReservedBit (firstReservedBit),
        m_secondReservedBit (secondReservedBit),
        m_thirdReservedBit (ThirdReservedBit),
        m_dcField (dcField)
  {
  }

  /**
  * Destructor
  */
  virtual ~BapHeaderTestCase ();

private:
  /**
  * Run the test
  */
  virtual void DoRun (void);
  uint16_t m_destinationAddress;
  uint16_t m_pathId;
  bool m_firstReservedBit;
  bool m_secondReservedBit;
  bool m_thirdReservedBit;
  BapHeader::BapDcField m_dcField;
};

BapHeaderTestCase::~BapHeaderTestCase ()
{
}

void
BapHeaderTestCase::DoRun (void)
{
  BapHeader header;
  BapHeader dest;
  // uint16_t destinationAddress = 0x331;
  // uint16_t pathId = 0xED;
  // bool firstReservedBit = false;
  // bool secondReservedBit = true;
  // bool thirdReservedBit = false;
  // BapHeader::BapDcField dcField = BapHeader::BapDcField::Data;
  header.SetDestinationAddress (m_destinationAddress);
  header.SetPathId (m_pathId);
  header.SetDcField (m_dcField);
  header.SetReservedBits (m_firstReservedBit, m_secondReservedBit, m_thirdReservedBit);

  Buffer buffer;
  buffer.AddAtStart (header.GetSerializedSize ());
  header.Serialize (buffer.Begin ());
  dest.Deserialize (buffer.Begin ());

  NS_TEST_ASSERT_MSG_EQ (header.GetSerializedSize (), 24, "Header not correctly serialized");

  uint16_t destDestinationAddress = dest.GetDestinationAddress ();
  uint16_t destPathId = dest.GetPathId ();
  BapHeader::BapDcField destDcField = dest.GetDcField ();
  bool destFirstReservedBit = (dest.GetReservedBits () & 0x4);
  bool destSecondReservedBit = (dest.GetReservedBits () & 0x2);
  bool destThirdReservedBit = (dest.GetReservedBits () & 0x1);

  NS_TEST_ASSERT_MSG_EQ (m_destinationAddress, destDestinationAddress,
                         "Different Destination Address, BAP header not correctly serialized");
  NS_TEST_ASSERT_MSG_EQ (m_pathId, destPathId,
                         "Different Path ID, BAP header not correctly serialized");
  NS_TEST_ASSERT_MSG_EQ (m_dcField, destDcField,
                         "Different DC Field, BAP header not correctly serialized");
  NS_TEST_ASSERT_MSG_EQ (m_firstReservedBit, destFirstReservedBit,
                         "Different first reserved bit, BAP header not correctly serialized");
  NS_TEST_ASSERT_MSG_EQ (m_secondReservedBit, destSecondReservedBit,
                         "Different second reserved bit, BAP header not correctly serialized");
  NS_TEST_ASSERT_MSG_EQ (m_thirdReservedBit, destThirdReservedBit,
                         "Different third reserved bit, BAP header not correctly serialized");
}

/**
* This suite tests if the MmWaveHelper correctly initializes the channel model
*/
class BapHeaderTestSuite : public TestSuite
{
public:
  BapHeaderTestSuite ();
};

BapHeaderTestSuite::BapHeaderTestSuite () : TestSuite ("bap-header-test", TestSuite::Type::UNIT)
{
  // TestDuration for TestCase can be QUICK, EXTENSIVE or TAKES_FOREVER
  AddTestCase (new BapHeaderTestCase (0x331, 0xED, false, true, false, BapHeader::BapDcField::Data),
               TestCase::Duration::QUICK);
  AddTestCase (
      new BapHeaderTestCase (0x111, 0xAD, false, false, false, BapHeader::BapDcField::Data),
      TestCase::Duration::QUICK);
  AddTestCase (new BapHeaderTestCase (0x231, 0xBD, true, true, true, BapHeader::BapDcField::Data),
               TestCase::Duration::QUICK);
  AddTestCase (new BapHeaderTestCase (0x0F1, 0xAF, false, true, true, BapHeader::BapDcField::Data),
               TestCase::Duration::QUICK);
  AddTestCase (new BapHeaderTestCase (0x8A, 0x1F, true, true, false, BapHeader::BapDcField::Data),
               TestCase::Duration::QUICK);
  AddTestCase (
      new BapHeaderTestCase (0x331, 0xED, false, true, false, BapHeader::BapDcField::Control),
      TestCase::Duration::QUICK);
  AddTestCase (
      new BapHeaderTestCase (0x111, 0xAD, false, false, false, BapHeader::BapDcField::Control),
      TestCase::Duration::QUICK);
  AddTestCase (
      new BapHeaderTestCase (0x231, 0xBD, true, true, true, BapHeader::BapDcField::Control),
      TestCase::Duration::QUICK);
  AddTestCase (
      new BapHeaderTestCase (0x0F1, 0xAF, false, true, true, BapHeader::BapDcField::Control),
      TestCase::Duration::QUICK);
  AddTestCase (
      new BapHeaderTestCase (0x8A, 0x1F, true, true, false, BapHeader::BapDcField::Control),
      TestCase::Duration::QUICK);
}

// Do not forget to allocate an instance of this TestSuite
static BapHeaderTestSuite bapHeaderTestSuite;
