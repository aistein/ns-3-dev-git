/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "ns3/string.h"
#include "ns3/simulator.h"
#include "ns3/queue-disc.h"
#include "prioTag.h"

namespace ns3 {

	NS_OBJECT_ENSURE_REGISTERED (PrioTag);
	
	TypeId PrioTag::GetTypeId (void)
	{
   		static TypeId tid = TypeId ("ns3::PrioTag")
     			.SetParent<Tag> ()
     			.AddConstructor<PrioTag> ()
     			.AddAttribute ("PrioValue",
                    		"A priority value",
                    		EmptyAttributeValue (),
                    		MakeUintegerAccessor (&PrioTag::GetPrioValue),
                    		MakeUintegerChecker<uint8_t> ());
  	 	return tid;
 	}

 	TypeId PrioTag::GetInstanceTypeId (void) const
 	{
   		return GetTypeId ();
 	}

 	uint32_t PrioTag::GetSerializedSize (void) const
 	{
   		return 1;
 	}

 	void PrioTag::Serialize (TagBuffer i) const
 	{
   		i.WriteU8 (m_PrioValue);
 	}

	void PrioTag::Deserialize (TagBuffer i)
	{
		m_PrioValue = i.ReadU8 ();
	}

	void PrioTag::Print (std::ostream &os) const
	{
		os << "priorityValue=" << (uint32_t)m_PrioValue;
	}

 	void PrioTag::SetPrioValue (uint8_t value)
 	{
   		m_PrioValue = value;
 	}

 	uint8_t PrioTag::GetPrioValue (void) const
 	{
   		return m_PrioValue;
 	}

} // namespace ns3

