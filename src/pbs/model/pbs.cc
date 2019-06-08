/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <algorithm>
#include <iostream>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
#include <map>
#include <tuple>
#include "ns3/fatal-error.h"
#include "ns3/string.h"
#include "ns3/simulator.h"
#include "ns3/queue-disc.h"
#include "ns3/uinteger.h"
#include "ns3/pointer.h"
#include "ns3/tcp-header.h"
#include "ns3/application-container.h"
#include "ns3/socket.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/bulk-send-application.h"
#include "prioTag.h"
#include "flowSizeTag.h"
#include "pbs.h"

namespace ns3 {

	NS_OBJECT_ENSURE_REGISTERED (PbsPacketFilter);
	
	TypeId
	PbsPacketFilter::GetTypeId (void)
	{
		static TypeId tid = TypeId ("ns3::PbsPacketFilter")
		  .SetParent<PacketFilter> ()
		  .SetGroupName ("TrafficControl")
		  .AddConstructor<PbsPacketFilter> ()
		  .AddAttribute ("Alpha", 
				 "The parameter for tuning scheduling policy.",
		  	       	 DoubleValue (0.001),
		               	 MakeDoubleAccessor (&PbsPacketFilter::m_alpha),
		  	       	 MakeDoubleChecker <double> ())	       
		  .AddAttribute ("Profile",
				 "The workload distribution type helps to tune priority boundaries.",
				 UintegerValue (0),
				 MakeUintegerAccessor (&PbsPacketFilter::m_profile),
				 MakeUintegerChecker <uint32_t> ())
		  .AddAttribute ("UsePbs", 
				 "The flag indicating whether or not to use PBS for scheduling priorities.",
		  	       	 BooleanValue (true),
		               	 MakeBooleanAccessor (&PbsPacketFilter::m_usePbs),
		  	       	 MakeBooleanChecker ())	       
		  .AddAttribute ("NonBlind", 
				 "The flag indicating whether or not PBS is operating in blind context.",
		  	       	 BooleanValue (false),
		               	 MakeBooleanAccessor (&PbsPacketFilter::m_nonBlind),
		  	       	 MakeBooleanChecker ())	       
		  ;
		return tid;
	}
	
	PbsPacketFilter::PbsPacketFilter ()
	{
		m_totalBytes = 0;
	}
	
	PbsPacketFilter::~PbsPacketFilter ()
	{
	}

	void
	PbsPacketFilter::PrintStats (std::ofstream& stream)
	{
		stream << "================================================================================\n";
		stream << "Alpha: " << m_alpha << "\n";
		stream << "Total Bytes: " << m_totalBytes << "\n\n";
		for (auto stats_it = m_flowStats.begin(); stats_it != m_flowStats.end(); stats_it++)
		{
			FlowId flowId = stats_it->first;
			FlowStats &ref = stats_it->second;

			stream << "FlowID: " << flowId << ",\tPackets Sent: " << ref.txPackets
			       << ",\tBytes Sent: " << ref.txBytes
			       << ",\tFlow Age: " << ref.flowAge.GetNanoSeconds() << " ns\n"
			       ;

			stream << "Priority Limits: ";
			for (int i = 0; i < 8; i++)
			{
				stream << m_prioLimits[i] << ", ";
			}
			stream << "\n";

			stream << "Priority History: " << "\n";
			for (auto prio_it = ref.prioHistory.begin(); prio_it != ref.prioHistory.end(); prio_it++)
			{
				stream << "Priority: " << prio_it->first << ", \%-txBytes: " 
				       << prio_it->second * 1.0 / ref.txBytes << "\n";
			}
			stream << "\n";	

			stream << "Raw Priority History: " << "\n";
			for (auto raw_it = ref.rawPrioHistory.begin(); raw_it != ref.rawPrioHistory.end(); raw_it++)
			{
				stream << std::get<0>(*raw_it) << "," << std::get<1>(*raw_it) << ","
				       << std::get<2>(*raw_it) << "\n";
			}
			stream << "\n\n";	
		}

		stream.flush();
	}

	void
	PbsPacketFilter::StreamRawPrioToCsv (std::ofstream& csv)
	{
		for (auto stats_it = m_flowStats.begin(); stats_it != m_flowStats.end(); stats_it++)
		{
			FlowId flowId = stats_it->first;
			FlowStats &ref = stats_it->second;

			csv << flowId << ",";
			for (auto raw_it = ref.rawPrioHistory.begin(); raw_it != ref.rawPrioHistory.end(); raw_it++)
			{
				// format:raw_prio,txBytes,flowAge,
				csv << std::get<0>(*raw_it) << "," << std::get<1>(*raw_it) << ","
				    << std::get<2>(*raw_it) << ",";
			}
			csv << "\n";	
		}
	}
	void
	PbsPacketFilter::StreamPacketsToCsv (std::ofstream& csv)
	{
		for (auto stats_it = m_flowStats.begin(); stats_it != m_flowStats.end(); stats_it++)
		{
			FlowId flowId = stats_it->first;
			FlowStats &ref = stats_it->second;

			csv << flowId << ",";
			for (auto prio_it = ref.prioPacketHistory.begin(); prio_it != ref.prioPacketHistory.end(); prio_it++)
			{
				// number of packets from flow going into this priority bin
				csv << prio_it->second << ",";
			}
			csv << "\n";	
		}
	}
	
	void
	PbsPacketFilter::StreamToCsv (std::ofstream& csv)
	{
		for (auto stats_it = m_flowStats.begin(); stats_it != m_flowStats.end(); stats_it++)
		{
			FlowId flowId = stats_it->first;
			FlowStats &ref = stats_it->second;

			csv << flowId << ",";
			for (auto prio_it = ref.prioHistory.begin(); prio_it != ref.prioHistory.end(); prio_it++)
			{
				// raw bytes of flow going into this priority bin
				csv << prio_it->second << ",";
			}
			csv << "\n";	
		}
	}

	uint64_t
	PbsPacketFilter::GetTotalBytes (void)
	{
		return m_totalBytes;
	}

	inline PbsPacketFilter::FlowStats&
	PbsPacketFilter::GetStatsForFlow (FlowId flowId)
	{
		FlowStatsContainerI iter;
		iter = m_flowStats.find(flowId);
		if (iter == m_flowStats.end() )
		{
			PbsPacketFilter::FlowStats &ref = m_flowStats[flowId];
			ref.timeFirstTxPacket = Seconds (0);
			ref.timeLastTxPacket = Seconds (0);
			ref.flowAge = Seconds (0);
			ref.txBytes = 0;
			ref.flowSize = 20000000;
			ref.txPackets = 0;
			ref.firstTx = true;
			for (uint16_t prio = 0; prio < 8; prio++)
			{
				ref.prioHistory[prio] = 0;
				ref.prioPacketHistory[prio] = 0;
			}
			return ref;
		}
		else
		{
			return iter->second;
		}
	}

	void
	PbsPacketFilter::MakePrioLimits (void)
	{
		std::vector<double> limits;

		// Note: Do not "overfit" the bucket boundaries, else varying alpha is hard to demonstrate
		switch (m_profile) {
			case 1: // W1 (alpha = 10)
				if (m_nonBlind) {
					limits = {1.64e-08, 3.15e-19, 6.08e-30, 1.17e-40, 2.26e-51, 4.37e-62, 8.42e-72, 1.62e-83}; // generalized
					//limits = {5.10708e-41, 3.12373e-41, 2.67044e-41, 2.19076e-41, 1.87042e-41, 1.59852e-41, 1.37976e-41, 1.18287e-41}; // overfitted
					//limits = {3.456e-39, 2.574e-41, 1.957e-41, 1.541e-41, 1.224e-41, 1.56267e-44, 1.56257e-44, 1.56255e-44};
				} else {
					limits = {2.5e-19, 1e-21, 4.4e-24, 1.9e-26, 7.8e-29, 3.3e-31, 1.4e-33, 5.8e-36};
				}
				break;
			case 2: // W2 (alpha = 10)
				if (m_nonBlind) {
					limits = {3.7e-07, 4.8e-18, 6.3e-29, 8.24e-40, 1.07e-50, 1.41e-61, 1.84e-72, 3.13e-94}; // generalized
					//limits = {1.87761e-39, 2.22075e-40, 6.0904e-41, 2.93441e-41, 2.17523e-41, 1.5628e-44, 1.5627e-44, 1.56258e-44}; // overfitted
					//limits = {3.96e-40, 9.21e-41, 3.61e-41, 2.63e-41, 1.89e-41, 1.5628e-44, 1.56269e-44, 1.56258e-44};
				} else {
					limits = {6.1e-20, 1.5e-23, 3.6e-27, 8.7e-31, 2.1e-34, 5.1e-38, 1.2e-41, 2.9e-45};
				}
				break;
			case 3: // W3 (alpha = 10)
				if (m_nonBlind) {
					limits = {4.08e-06, 3.95e-17, 3.82e-28, 3.69e-39, 3.58e-50, 3.45e-61, 3.34e-72, 3.24e-82}; // generalized
					//limits = {7.46415e-36, 2.4216e-38, 5.70689e-39, 2.40171e-39, 1.00258e-39, 4.90344e-40, 1.85818e-40, 4.10319e-41}; // overfitted
					//limits = {4.1604e-37, 2.75841e-38, 7.28671e-39, 3.57929e-39, 1.72651e-39, 6.96279e-40, 2.26281e-40, 2.82834e-41};
					//limits = {3.6527e-38, 1.1489e-38, 5.433e-39, 2.8375e-39, 1.316e-39, 4.943e-40, 1.4986e-40};
				} else {
					limits = {5.8e-21, 7.3e-26, 9e-31, 1.1e-35, 1.3e-40, 1.6e-45, 2e-50, 2.5e-55};
				}
				break;
			case 4: // W4 (alpha = 10)
				if (m_nonBlind) {
					//limits = {1.75064e-35, 3.27967e-37, 1.03718e-37, 4.73471e-38, 2.3662e-38, 1.16651e-38, 5.71894e-39, 1.90036e-39}; // overfitted
					//limits = {1.42897e-41, 3.32968e-49, 1.58941e-54, 4.02692e-58, 4.00434e-62, 4.20953e-67, 1.11941e-67, 1.86391e-68};
					limits = {1.62e-05, 1.32e-16, 1.07e-27, 8.76e-39, 7.13e-50, 5.8e-61, 4.72e-72, 3.84e-83}; // generalized
				} else {
					limits = {2.6e-21, 1.2e-26, 5.3e-32, 2.4e-37, 1.1e-42, 4.9e-48, 2.2e-53, 1e-58};
				}
				break;
			case 5: // W5 (alpha = 10)
				if (m_nonBlind) {
					//limits = {4.28436e-29, 4.7178e-36, 9.17517e-37, 2.76119e-37, 1.10387e-37, 5.29671e-38, 2.4704e-38, 9.65756e-39}; // overfitted
					//limits = {1.33137e-48, 3.86956e-55, 1.43643e-59, 3.79086e-63, 3.35128e-65, 1.13623e-66, 3.13629e-67, 8.19613e-68};
					limits = {4.71e-05, 3.94e-16, 3.3e-27, 2.76e-38, 2.31e-49, 1.94e-60, 1.62e-71, 1.36e-82}; // generalized
				} else {
					limits = {1.2e-21, 2.7e-27, 6e-33, 1.3e-38, 3e-44, 6.5e-50, 1.4e-55, 3.2e-61};
				}
				break;
			case 6: // Incast 
				if (m_nonBlind) {
					limits = {7.0772E-65, 1.2458E-65, 2.1929E-66, 3.8601E-67, 6.7949E-68, 1.1961E-68, 2.1054E-69, 3.7061E-70};
				} else {
					//limits = {2.6e-21, 1.2e-26, 5.3e-32, 2.4e-37, 1.1e-42, 4.9e-48, 2.2e-53, 1e-58};
					limits = {3.6073E-20, 5.2077E-24, 7.5181E-28, 1.0854E-31, 1.5669E-35, 2.2620E-39, 3.2656E-43, 4.7144E-47};
				}
				break;
			case 7: // background flows
				limits = {1.2e-21, 2.7e-27, 6e-33, 1.3e-38, 3e-44, 6.5e-50, 1.4e-55, 3.2e-61};
				break;
			default:
				std::cout << m_profile << std::endl;
				NS_FATAL_ERROR("invalid profile specified.");
		}
		for (int i=0; i<8; i++) {
			m_prioLimits[i] = limits[i];
		}
	}

	std::map<uint64_t, uint64_t>
	PbsPacketFilter::PeekLoadAtTime (void)
	{
		return (std::map<uint64_t, uint64_t>)m_loadAtTime;
	}

	inline uint64_t&
	PbsPacketFilter::GetLoadStats (void)
	{
		uint64_t currTime;
		SwitchLoadContainerI iter;
		currTime = Simulator::Now().GetNanoSeconds ();
		iter = m_loadAtTime.find(currTime);
		if ( iter == m_loadAtTime.end() )
		{
			uint64_t &ref = m_loadAtTime[currTime];
			ref = 0;
			return ref;
		}
		return iter->second;
	}
	
	int32_t
	PbsPacketFilter::DoClassify (Ptr<QueueDiscItem> item) const
	{
		FlowId flowId = item->Hash();
		double raw_prio;
		uint8_t bin_prio = uint8_t(0);
		uint32_t packetsize = item->GetSize();
		PrioTag prioTag;

		// TODO: To add non-TCP based congestion controls, this logic needs to be rid of TCP-specific dependencies

		// check if handshaking packet via SYN
		Ptr<Packet> pkt = item->GetPacket ();
		TcpHeader tcpHdr;
		if ( pkt->PeekHeader (tcpHdr) ) {
			//uint8_t tcp_mask = TcpHeader::SYN | TcpHeader::ACK | TcpHeader::FIN;
			uint8_t tcp_mask = TcpHeader::SYN | TcpHeader::FIN;
			if ( ((tcpHdr.GetFlags() & tcp_mask) == (tcp_mask)) || (tcpHdr.GetSourcePort() == 9) ) {
				bin_prio = 0;
			} else {

				const_cast<PbsPacketFilter*>(this)->m_totalBytes += packetsize;
				const_cast<PbsPacketFilter*>(this)->GetLoadStats () += packetsize;

				FlowStats &ref = const_cast<PbsPacketFilter*>(this)->GetStatsForFlow(flowId);
				bool firstFlag = false;
				if (ref.firstTx == true)
				{
					ref.timeFirstTxPacket = Simulator::Now();
					ref.firstTx = false;
					firstFlag = true;
					if (m_nonBlind) {
						FlowSizeTag flowSizeTag;
						if (pkt->RemovePacketTag (flowSizeTag)) {
							ref.flowSize = flowSizeTag.GetFlowSize ();
						} else {
							std::cout << "-E- No FlowSizeTag on non-handshake TCP packet!" << std::endl;
						}
					}
				}
				ref.txBytes += packetsize;
				ref.txPackets++;
				ref.timeLastTxPacket = Simulator::Now();
				ref.flowAge = ref.timeLastTxPacket - ref.timeFirstTxPacket;

				if ( !m_usePbs ) {
					bin_prio = 0;
				} else {
					const_cast<PbsPacketFilter*>(this)->MakePrioLimits();
					if (m_nonBlind) {
						uint32_t bytes_remaining = std::max((uint64_t)1, ref.flowSize - ref.txBytes);
						raw_prio = ref.flowAge.GetNanoSeconds () / pow (bytes_remaining, m_alpha);
					} else {
						raw_prio = ref.flowAge.GetNanoSeconds () / pow (ref.txBytes, m_alpha);
					}
					ref.rawPrioHistory.push_back(std::make_tuple(raw_prio, ref.txBytes, ref.flowAge.GetNanoSeconds()));
					if (!firstFlag || m_nonBlind) {
						for (bin_prio = 7; bin_prio >= 0; bin_prio--)
						{
							if (raw_prio <= m_prioLimits[bin_prio])
								break;
						}
						bin_prio = std::max(bin_prio, uint8_t(0)); // becasue loop above can produce prio=-1
					} else { // first packet in blind scenario always prio-0
						bin_prio = 0;
					}
				}
				ref.prioHistory[bin_prio] += packetsize;
				ref.prioPacketHistory[bin_prio]++;

			} // end if SYN flag set
		} else { // was not a TCP Header
			bin_prio = 0;
		}

		prioTag.SetPrioValue (bin_prio);
		pkt->AddPacketTag (prioTag);

		return bin_prio;
	}

	bool
	PbsPacketFilter::CheckProtocol (Ptr<QueueDiscItem> item) const
	{
		return true;
	}

} // namespace ns3

