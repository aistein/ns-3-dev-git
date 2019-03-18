/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "ns3/string.h"
#include "ns3/simulator.h"
#include "ns3/queue-disc.h"
#include "flowSizeTag.h"

namespace ns3 {

	NS_OBJECT_ENSURE_REGISTERED (FlowSizeTag);
	
	TypeId FlowSizeTag::GetTypeId (void)
	{
   		static TypeId tid = TypeId ("ns3::FlowSizeTag")
     			.SetParent<Tag> ()
     			.AddConstructor<FlowSizeTag> ()
     			.AddAttribute ("FlowSize",
                    		"a-priori byte-size of flow associated with this packet",
                    		EmptyAttributeValue (),
                    		MakeUintegerAccessor (&FlowSizeTag::GetFlowSize),
                    		MakeUintegerChecker<uint32_t> ());
  	 	return tid;
 	}

 	TypeId FlowSizeTag::GetInstanceTypeId (void) const
 	{
   		return GetTypeId ();
 	}

 	uint32_t FlowSizeTag::GetSerializedSize (void) const
 	{
   		return 1;
 	}

 	void FlowSizeTag::Serialize (TagBuffer i) const
 	{
   		i.WriteU32 (m_FlowSize);
 	}

	void FlowSizeTag::Deserialize (TagBuffer i)
	{
		m_FlowSize = i.ReadU32 ();
	}

	void FlowSizeTag::Print (std::ostream &os) const
	{
		os << "flowSize=" << (uint32_t)m_FlowSize;
	}

 	void FlowSizeTag::SetFlowSize (uint32_t value)
 	{
   		m_FlowSize = value;
 	}

 	uint32_t FlowSizeTag::GetFlowSize (void) const
 	{
   		return m_FlowSize;
 	}

} // namespace ns3

