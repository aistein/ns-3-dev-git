/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef PBS_H
#define PBS_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <utility>
#include "ns3/nstime.h"
#include "ns3/queue-disc.h"
#include "ns3/packet-filter.h"

namespace ns3 {

	class PbsPacketFilter : public PacketFilter
	{
		public:
			static TypeId GetTypeId ();
			PbsPacketFilter ();
		        virtual ~PbsPacketFilter ();
			void PrintStats (std::ofstream& stream);
			void StreamToCsv (std::ofstream& csv);
			void StreamPacketsToCsv (std::ofstream& csv);
			void StreamRawPrioToCsv (std::ofstream& csv);
			uint64_t GetTotalBytes (void);
			std::map<uint64_t, uint64_t> PeekLoadAtTime (void);

		private:
			virtual bool CheckProtocol (Ptr<QueueDiscItem> item) const;
			virtual int32_t DoClassify (Ptr<QueueDiscItem> item) const;
			double m_alpha;
			bool m_usePbs;
			uint32_t m_profile;

			uint64_t m_totalBytes;
			double m_prioLimits[8];
			void MakePrioLimits (void);

			typedef uint32_t FlowId;
			struct FlowStats
			{
				Time     timeFirstTxPacket;
				Time     timeLastTxPacket;
				Time     flowAge;
				uint64_t txBytes;
			        uint32_t txPackets;
				bool	 firstTx;
				std::map<uint16_t, uint64_t> prioHistory; // raw bytes per priority level
				std::map<uint16_t, uint32_t> prioPacketHistory; // number of packets per priority level
				std::vector<std::tuple<double, uint64_t, uint64_t>> rawPrioHistory; // acutal prio-calculation per packet of flow
			};

			typedef std::map<FlowId, FlowStats> FlowStatsContainer;
			typedef std::map<FlowId, FlowStats>::iterator FlowStatsContainerI;
			FlowStatsContainer m_flowStats;
			FlowStats& GetStatsForFlow (FlowId flowId);

			typedef std::map<uint64_t, uint64_t> SwitchLoadContainer;
			typedef std::map<uint64_t, uint64_t>::iterator SwitchLoadContainerI;
			SwitchLoadContainer m_loadAtTime;
			uint64_t& GetLoadStats (void);

	}; // PbsPacketFilter

} // namespace ns3

#endif /* PBS_H */

