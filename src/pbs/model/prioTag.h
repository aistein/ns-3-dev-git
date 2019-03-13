/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef PRIOTAG_H
#define PRIOTAG_H

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

	class PrioTag : public Tag
	{
 		
		public:
   			
			static TypeId GetTypeId (void);
   			virtual TypeId GetInstanceTypeId (void) const;
	  	 	virtual uint32_t GetSerializedSize (void) const;
   			virtual void Serialize (TagBuffer i) const;
   			virtual void Deserialize (TagBuffer i);
   			virtual void Print (std::ostream &os) const;
 
   			// these are our accessors to our tag structure
   			void SetPrioValue (uint8_t value);
  		 	uint8_t GetPrioValue (void) const;
 		
		private:
   			
			uint8_t m_PrioValue;  
 };

} // namespace ns3

#endif /* PRIOTAG_H */

