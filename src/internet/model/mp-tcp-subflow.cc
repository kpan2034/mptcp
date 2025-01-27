/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 Morteza Kheirkhah
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
 * Author: Morteza Kheirkhah <m.kheirkhah@sussex.ac.uk>
 */

#include <iostream>
#include "ns3/mp-tcp-typedefs.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "mp-tcp-subflow.h"

NS_LOG_COMPONENT_DEFINE("MpTcpSubflow");

namespace ns3{

NS_OBJECT_ENSURE_REGISTERED(MpTcpSubFlow);

TypeId
MpTcpSubFlow::GetTypeId(void)
{
  static TypeId tid = TypeId("ns3::MpTcpSubFlow")
      .SetParent<Object>()
      .AddConstructor<MpTcpSubFlow>()
      .AddTraceSource("cWindow",
          "The congestion control window to trace.",
           MakeTraceSourceAccessor(&MpTcpSubFlow::cwnd));
  return tid;
}

MpTcpSubFlow::MpTcpSubFlow() :
    routeId(0),
    state(CLOSED),
    sAddr(Ipv4Address::GetZero()),
    sPort(0),
    dAddr(Ipv4Address::GetZero()),
    dPort(0),
    oif(0),
    mapDSN(0),
    lastMeasuredRtt(Seconds(0.0))
{
  connected = false;
  TxSeqNumber = rand() % 1000;
  RxSeqNumber = 0;
  bandwidth = 0;
  cwnd = 0;
  ssthresh = 65535;
  maxSeqNb = TxSeqNumber - 1;
  highestAck = 0;
  rtt = CreateObject<RttMeanDeviation>();
  rtt->Gain(0.1);
  Time estimate;
  estimate = Seconds(1.5);
  rtt->SetCurrentEstimate(estimate);
  cnRetries = 3;
  Time est = MilliSeconds(200);
  cnTimeout = est;
  initialSequnceNumber = 0;
  m_retxThresh = 3;
  m_inFastRec = false;
  m_limitedTx = false;
  m_dupAckCount = 0;
  /* WLIA */
  m_accountedFor = 0;
  m_prevAckNo = 0;
  m_lastAck = 0.0;
  m_currentBW = 0.0;
  m_lastBW = 0.0;
  m_lastSampleBW = 0.0;
  m_baseRtt = Seconds(0.0);
  m_minRtt = Seconds(0.0);
  win_size = 0;
  dwnd = 0;

  vegasAlpha = 2;
  vegasBeta = 4;

  PktCount = 0;
  m_recover = SequenceNumber32(0);
  m_gotFin = false;
  AccumulativeAck = false;
  m_limitedTxCount = 0;
}

MpTcpSubFlow::~MpTcpSubFlow()
{
  m_endPoint = 0;
  routeId = 0;
  sAddr = Ipv4Address::GetZero();
  oif = 0;
  state = CLOSED;
  bandwidth = 0;
  cwnd = 0;
  maxSeqNb = 0;
  highestAck = 0;
  for (list<DSNMapping *>::iterator it = mapDSN.begin(); it != mapDSN.end(); ++it)
    {
      DSNMapping * ptrDSN = *it;
      delete ptrDSN;
    }
  mapDSN.clear();
}

bool
MpTcpSubFlow::Finished(void)
{
  return (m_gotFin && m_finSeq.GetValue() < RxSeqNumber);
}

void
MpTcpSubFlow::StartTracing(string traced)
{
  //NS_LOG_UNCOND("("<< routeId << ") MpTcpSubFlow -> starting tracing of: "<< traced);
  TraceConnectWithoutContext(traced, MakeCallback(&MpTcpSubFlow::CwndTracer, this)); //"CongestionWindow"
}

void
MpTcpSubFlow::CwndTracer(uint32_t oldval, uint32_t newval)
{
  //NS_LOG_UNCOND("Subflow "<< routeId <<": Moving cwnd from " << oldval << " to " << newval);
  cwndTracer.push_back(make_pair(Simulator::Now().GetSeconds(), newval));
  sstTracer.push_back(make_pair(Simulator::Now().GetSeconds(), ssthresh));
  rttTracer.push_back(make_pair(Simulator::Now().GetSeconds(), rtt->GetCurrentEstimate().GetMilliSeconds()));
  rtoTracer.push_back(make_pair(Simulator::Now().GetSeconds(), rtt->RetransmitTimeout().GetMilliSeconds()));
}

void
MpTcpSubFlow::AddDSNMapping(uint8_t sFlowIdx, uint64_t dSeqNum, uint16_t dLvlLen, uint32_t sflowSeqNum, uint32_t ack/*,
    Ptr<Packet> pkt*/)
{
  NS_LOG_FUNCTION_NOARGS();
  mapDSN.push_back(new DSNMapping(sFlowIdx, dSeqNum, dLvlLen, sflowSeqNum, ack/*, pkt*/));
}

void
MpTcpSubFlow::SetFinSequence(const SequenceNumber32& s)
{
  NS_LOG_FUNCTION (this);
  m_gotFin = true;
  m_finSeq = s;
  if (RxSeqNumber == m_finSeq.GetValue())
    ++RxSeqNumber;
}

DSNMapping *
MpTcpSubFlow::GetunAckPkt()
{
  NS_LOG_FUNCTION(this);
  DSNMapping * ptrDSN = 0;
  for (list<DSNMapping *>::iterator it = mapDSN.begin(); it != mapDSN.end(); ++it)
    {
      DSNMapping * ptr = *it;
      if (ptr->subflowSeqNumber == highestAck + 1)
        {
          ptrDSN = ptr;
          break;
        }
    }
  return ptrDSN;
}
}
