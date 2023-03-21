/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
*   Copyright (c) 2020, University of Padova, Dep. of Information Engineering, SIGNET lab
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
*   Author: Matteo Pagin <mattpagg@gmail.com>
*/

#ifndef SRC_MMWAVE_BAP_TRACE_H_
#define SRC_MMWAVE_BAP_TRACE_H_

#include <ns3/object.h>
#include <ns3/mmwave-phy-mac-common.h>
#include <ns3/mmwave-enb-mac.h>
#include <ns3/bap.h>
#include <fstream>

namespace ns3 {

namespace mmwave {

/**
 * This class contains the MmWave, BAP-related tracing entities
 *
 * The purpose of this class is to implement the MmWave, BAP-related callbacks and to
 * organize their setup (such as the traces' filename and their output streams)
 *
 */
class MmWaveBapTrace : public Object
{
public:
  MmWaveBapTrace ();
  virtual ~MmWaveBapTrace ();
  static TypeId GetTypeId (void);

  /**
  * Sets the filename for the BAP-related traces
  *
  * \param fileName the intended filename
  */
  void SetOutputFilename (std::string fileName);

  /**
  * Callback used to trace the reception of a packet at the BAP
  *
  * \param bapStats the pointer to an object of this class
  */
  static void ReportPacketRx (Ptr<MmWaveBapTrace> bapStats, BapHeader header, bool isPdu);

  /**
  * Callback used to trace the transmission of a packet at the BAP
  *
  * \param bapStats the pointer to an object of this class
  */
  static void ReportPacketTx (Ptr<MmWaveBapTrace> bapStats, BapHeader header, bool isPdu);

private:
  /**
  * Common trace function
  *
  * \param bapStats the pointer to an object of this class
  */
  static void inline ReportPacketTxRx (Ptr<MmWaveBapTrace> bapStats, std::string txRx,
                                       BapHeader header, bool isPdu);

  static std::ofstream m_bapTraceFile; //!< Output stream for the scheduling allocations trace
  static std::string m_bapTraceFilename; //!< Output filename for the scheduling allocations trace
};

} // namespace mmwave

} /* namespace ns3 */

#endif /* SRC_MMWAVE_BAP_TRACE_H_ */
