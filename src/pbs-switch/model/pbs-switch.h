/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef PBS_SWITCH_H
#define PBS_SWITCH_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "ns3/nstime.h"
#include "ns3/queue-disc.h"
#include "ns3/packet-filter.h"

namespace ns3 {

	class PbsSwitchPacketFilter : public PacketFilter
	{
		public:
			static TypeId GetTypeId ();
			PbsSwitchPacketFilter ();
		        virtual ~PbsSwitchPacketFilter ();
			void PrintStats (std::ofstream& stream);
			void PrintFlowCategories (std::ofstream& stream);
			uint64_t GetTotalBytes (void);
			std::vector<double> GetFlowRates (void);
			std::map<uint64_t, uint64_t> PeekLoadAtTime (void);

		private:
			virtual bool CheckProtocol (Ptr<QueueDiscItem> item) const;
			virtual int32_t DoClassify (Ptr<QueueDiscItem> item) const;
			uint64_t m_totalBytes;

			typedef uint32_t FlowId;
			struct FlowStats
			{
				Time     timeFirstTxPacket;
				Time     timeLastTxPacket;
				Time     flowAge;
				uint64_t txBytes;
			        uint32_t txPackets;
				std::map<uint16_t, uint64_t> prioHistory;
			};

			typedef std::map<FlowId, FlowStats> FlowStatsContainer;
			typedef std::map<FlowId, FlowStats>::iterator FlowStatsContainerI;
			FlowStatsContainer m_flowStats;
			FlowStats& GetStatsForFlow (FlowId flowId);

			typedef std::map<uint64_t, uint64_t> SwitchLoadContainer;
			typedef std::map<uint64_t, uint64_t>::iterator SwitchLoadContainerI;
			SwitchLoadContainer m_loadAtTime;
			uint64_t& GetLoadStats (void);

	}; // PbsSwitchPacketFilter

} // namespace ns3

#endif /* PBS_SWITCH_H */

