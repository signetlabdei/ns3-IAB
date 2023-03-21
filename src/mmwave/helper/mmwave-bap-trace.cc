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

#include <ns3/log.h>
#include "mmwave-bap-trace.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MmWaveBapTrace");

namespace mmwave {

NS_OBJECT_ENSURE_REGISTERED (MmWaveBapTrace);

std::ofstream MmWaveBapTrace::m_bapTraceFile{};
std::string MmWaveBapTrace::m_bapTraceFilename{};

MmWaveBapTrace::MmWaveBapTrace ()
{
}

MmWaveBapTrace::~MmWaveBapTrace ()
{
  if (m_bapTraceFile.is_open ())
    {
      m_bapTraceFile.close ();
    }
}

TypeId
MmWaveBapTrace::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::MmWaveBapTrace")
          .SetParent<Object> ()
          .AddConstructor<MmWaveBapTrace> ()
          .AddAttribute (
              "OutputFilename", "Name of the file where the TX and RX packets will be saved.",
              StringValue ("BapTrace.txt"), MakeStringAccessor (&MmWaveBapTrace::SetOutputFilename),
              MakeStringChecker ());
  return tid;
}

void
MmWaveBapTrace::ReportPacketTx (Ptr<MmWaveBapTrace> bapStats, BapHeader header, bool isPdu)
{
  ReportPacketTxRx (bapStats, "TX", header, isPdu);
}

void
MmWaveBapTrace::ReportPacketRx (Ptr<MmWaveBapTrace> bapStats, BapHeader header, bool isPdu)
{
  ReportPacketTxRx (bapStats, "RX", header, isPdu);
}

void inline MmWaveBapTrace::ReportPacketTxRx (Ptr<MmWaveBapTrace> bapStats, std::string txRx,
                                              BapHeader header, bool isPdu)
{
  // Open the output file if it is not open yet
  if (!m_bapTraceFile.is_open ())
    {
      m_bapTraceFile.open (m_bapTraceFilename.c_str ());
      if (!m_bapTraceFile.is_open ())
        {
          NS_FATAL_ERROR ("Could not open tracefile");
        }
      m_bapTraceFile << "time\ttx_rx\tis_pdu\tbap_dest\tbap_path_id\tdata_ctrl" << std::endl;
    }

  m_bapTraceFile << Simulator::Now () << "\t" << txRx << "\t" << isPdu << "\t"
                 << header.GetDestinationAddress () << "\t" << header.GetPathId () << "\t"
                 << header.GetDcField () << "\n";
}

void
MmWaveBapTrace::SetOutputFilename (std::string fileName)
{
  NS_LOG_INFO ("Filename: " << fileName);
  m_bapTraceFilename = fileName;
}

} // namespace mmwave

} /* namespace ns3 */
