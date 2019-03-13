/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "ns3/string.h"
#include "ns3/simulator.h"
#include "ns3/prioTag.h"
#include "ns3/queue-disc.h"
#include "pbs-switch.h"

namespace ns3 {

	NS_OBJECT_ENSURE_REGISTERED (PbsSwitchPacketFilter);
	
	TypeId
	PbsSwitchPacketFilter::GetTypeId (void)
	{
		static TypeId tid = TypeId ("ns3::PbsSwitchPacketFilter")
		  .SetParent<PacketFilter> ()
		  .SetGroupName ("TrafficControl")
		  .AddConstructor<PbsSwitchPacketFilter> ()
		  ;
		return tid;
	}
	
	PbsSwitchPacketFilter::PbsSwitchPacketFilter ()
	{
		m_totalBytes = 0;
	}
	
	PbsSwitchPacketFilter::~PbsSwitchPacketFilter ()
	{
	}

	// For each flow traversing this switch, how many of its bytes were assigned to which priority?
	void
	PbsSwitchPacketFilter::PrintFlowCategories (std::ofstream& stream)
	{
		for (auto stats_it = m_flowStats.begin(); stats_it != m_flowStats.end(); stats_it++)
		{
			FlowId flowId = stats_it->first;
			FlowStats &ref = stats_it->second;

			stream << flowId << "," << ref.txBytes << ",";
			for (auto prio_it = ref.prioHistory.begin(); prio_it != ref.prioHistory.end(); prio_it++)
			{
				stream << prio_it->second << ",";
			}
			stream << "\n";	

		}
	}

	void
	PbsSwitchPacketFilter::PrintStats (std::ofstream& stream)
	{
		stream << "================================================================================\n";
		stream << "Total Bytes: " << m_totalBytes << "\n\n";
		for (auto stats_it = m_flowStats.begin(); stats_it != m_flowStats.end(); stats_it++)
		{
			FlowId flowId = stats_it->first;
			FlowStats &ref = stats_it->second;

			stream << "FlowID: " << flowId << ",\tPackets Sent: " << ref.txPackets
			       << ",\tBytes Sent: " << ref.txBytes
			       << ",\tFlow Age: " << ref.flowAge.GetNanoSeconds() << " ns\n"
			       ;

			stream << "Priority History: " << "\n";
			for (auto prio_it = ref.prioHistory.begin(); prio_it != ref.prioHistory.end(); prio_it++)
			{
				stream << "Priority: " << prio_it->first << ", \%-txBytes: " 
				       << prio_it->second * 1.0 / ref.txBytes << "\n";
			}
			stream << "\n";	

		}
	}

	uint64_t
	PbsSwitchPacketFilter::GetTotalBytes (void)
	{
		return m_totalBytes;
	}

	std::vector<double>
	PbsSwitchPacketFilter::GetFlowRates (void)
	{
		std::vector<double> flow_rates;
		for (auto it = m_flowStats.begin(); it != m_flowStats.end(); ++it)
		{
			FlowStats &ref = it->second;
			uint64_t age = ref.flowAge.GetNanoSeconds ();
			if (age != 0)
		       		flow_rates.push_back ( ref.txBytes * 8.0 / age );
		}

		return flow_rates;
	}

	std::map<uint64_t, uint64_t>
	PbsSwitchPacketFilter::PeekLoadAtTime (void)
	{
		return (std::map<uint64_t, uint64_t>)m_loadAtTime;
	}

	inline PbsSwitchPacketFilter::FlowStats&
	PbsSwitchPacketFilter::GetStatsForFlow (FlowId flowId)
	{
		FlowStatsContainerI iter;
		iter = m_flowStats.find(flowId);
		if (iter == m_flowStats.end() )
		{
			PbsSwitchPacketFilter::FlowStats &ref = m_flowStats[flowId];
			ref.timeFirstTxPacket = Seconds (0);
			ref.timeLastTxPacket = Seconds (0);
			ref.flowAge = Seconds (0);
			ref.txBytes = 0;
			ref.txPackets = 0;
			for (uint16_t prio = 0; prio < 8; prio++)
			{
				ref.prioHistory[prio] = 0;
			}
			return ref;
		}
		else
		{
			return iter->second;
		}
	}

	inline uint64_t&
	PbsSwitchPacketFilter::GetLoadStats (void)
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
	PbsSwitchPacketFilter::DoClassify (Ptr<QueueDiscItem> item) const
	{
		//TODO: figure out how to use QueueDisc Timestamps instead of Simulator::Now()
		FlowId flowId = item->Hash();
		uint8_t prio;
		uint32_t packetsize = item->GetSize();

		// Record Aggregate Stats
		const_cast<PbsSwitchPacketFilter*>(this)->m_totalBytes += packetsize;
		const_cast<PbsSwitchPacketFilter*>(this)->GetLoadStats () += packetsize;

		// Forward Pre-Assigned Packet Priority
		PrioTag prioTag;
		Ptr<Packet> pkt = item->GetPacket();
		bool found = pkt->PeekPacketTag (prioTag);
		if (found)
		{
			prio = prioTag.GetPrioValue ();
		}
		else
		{
			prio = 0;
		}


		// Record Flow-specific stats
		FlowStats &ref = const_cast<PbsSwitchPacketFilter*>(this)->GetStatsForFlow(flowId);

		if (ref.timeFirstTxPacket == Seconds (0))
		{
			//ref.timeFirstTxPacket = item->GetTimeStamp ();
			ref.timeFirstTxPacket = Simulator::Now();
		}
		ref.txBytes += packetsize;
		ref.txPackets++;
		//ref.timeLastTxPacket = item->GetTimeStamp ();
		ref.timeLastTxPacket = Simulator::Now();
		ref.flowAge = ref.timeLastTxPacket - ref.timeFirstTxPacket;
		ref.prioHistory[prio] += packetsize;

		return prio;
	}

	bool
	PbsSwitchPacketFilter::CheckProtocol (Ptr<QueueDiscItem> item) const
	{
		return true;
	}

} // namespace ns3

