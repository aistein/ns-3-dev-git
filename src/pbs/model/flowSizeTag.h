/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef FLOWSIZETAG_H
#define FLOWSIZETAG_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "ns3/nstime.h"
#include "ns3/queue-disc.h"
#include "ns3/packet-filter.h"

#include "ns3/tag.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"

namespace ns3 {

	class FlowSizeTag : public Tag
	{
 		
		public:
   			
			static TypeId GetTypeId (void);
   			virtual TypeId GetInstanceTypeId (void) const;
	  	 	virtual uint32_t GetSerializedSize (void) const;
   			virtual void Serialize (TagBuffer i) const;
   			virtual void Deserialize (TagBuffer i);
   			virtual void Print (std::ostream &os) const;
 
   			// these are our accessors to our tag structure
   			void SetFlowSize (uint32_t value);
  		 	uint32_t GetFlowSize (void) const;
 		
		private:
   			
			uint32_t m_FlowSize;  
 };

} // namespace ns3

#endif /* FLOWSIZETAG_H */

