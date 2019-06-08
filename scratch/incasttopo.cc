/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019 
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
 * Authors: Kunal Mahajan <mkunal@cs.columbia.edu>, Alex Stein <as5281@columbia.edu>,
 * 	    Pearce Kieser <pck2119@columbia.edu>
 */

#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <cstdio>
#include <ctime>

#include <algorithm>
#include <vector>
#include <cctype>
#include <locale>

#include "ns3/log.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-helper.h"
#include "ns3/bridge-net-device.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-nix-vector-helper.h"
#include "ns3/network-module.h"
#include "ns3/nstime.h"
#include "ns3/point-to-point-module.h"
#include "ns3/prio-queue-disc.h"
#include "ns3/packet.h"
#include "ns3/packet-filter.h"
#include "ns3/pbs.h"
#include "ns3/traffic-control-helper.h"
#include "ns3/prioTag.h"
#include "ns3/pbs-switch.h"
/*
	- This work goes along with the paper "Towards Reproducible Performance Studies of Datacenter Network Architectures Using An Open-Source Simulation Approach"

	- The code is constructed in the following order:
		1. Creation of Node Containers 
		2. Initialize settings for On/Off Application
		3. Connect hosts to edge switches
		4. Connect edge switches to aggregate switches
		6. Start Simulation

	- Addressing scheme:
		1. Address of host: 10.0.edge.0 /24
		2. Address of edge and aggregation switch: 10.0.edge.0 /16
		
	- TODO: Descibre BulkSend Application and remove On/Off description
	- On/Off Traffic of the simulation: addresses of client and server are randomly selected everytime
		1. On - Timing Sampled from EmpiricalRandomVariable (created via CDF supplied from command line)
		2. Off - Timing Sampled from UniformRandomVariable // TODO: what should the "off" time actually be?
	
	- Simulation Settings:
                - Number of nodes: 16*4 = 144
		- Simulation running time: 6 seconds
		- Packet size: 1024 bytes
		- Data rate for packet sending: 1 Mbps
		- Data rate for device channel: 1000 Mbps
		- Delay time for device: 0.001 ms
		- Communication pairs selection: Random Selection with uniform probability
		- Traffic flow pattern: Exponential random traffic
		- Routing protocol: Nix-Vector

        - Statistics Output:
                - Flowmonitor XML output file: homastats.xml is located in the ns-3.29/ folder
		- TODO: describe all other outputs            


*/


using namespace ns3;
using namespace std;
NS_LOG_COMPONENT_DEFINE ("Homa-Architecture");

// Function to create address string from numbers
//
char * toString(int a,int b, int c, int d){

	int first = a;
	int second = b;
	int third = c;
	int fourth = d;

	char *address =  new char[30];
	char firstOctet[30], secondOctet[30], thirdOctet[30], fourthOctet[30];	
	//address = firstOctet.secondOctet.thirdOctet.fourthOctet;

	bzero(address,30);

	snprintf(firstOctet,10,"%d",first);
	strcat(firstOctet,".");
	snprintf(secondOctet,10,"%d",second);
	strcat(secondOctet,".");
	snprintf(thirdOctet,10,"%d",third);
	strcat(thirdOctet,".");
	snprintf(fourthOctet,10,"%d",fourth);

	strcat(thirdOctet,fourthOctet);
	strcat(secondOctet,thirdOctet);
	strcat(firstOctet,secondOctet);
	strcat(address,firstOctet);

	return address;
}



// Main function
//
int main(int argc, char *argv[])
{
//============= Enable Command Line Parser & Custom Options ================//
//
	uint32_t profile = 1;
	string tag = "";
	double alpha = 0.001;
	uint32_t num_flows = 1;
	bool debug = false;
	bool fast = false;
	bool use_pbs = true;
	bool use_dctcp = true;
	bool same_buff = false;
	bool nonblind = false;
	uint32_t buffer_size = 42400;
	uint32_t incast_flow_size = 450000;
	uint32_t threshold = 10;
	double load_multiplier = 6.0; // increase the load by a constant factor to account for TCP slow-start inefficiency!
        double load = 0.6; // load percentage
	int incast_servers = 1;

	CommandLine cmd;
	cmd.AddValue("profile", "identifier of the workload distribution to be tested (int)", profile);
	cmd.AddValue("tag", "desired postfix for output files (string)", tag);
	cmd.AddValue("alpha", "tunable parameter dictating scheduling policy (double)", alpha);
	cmd.AddValue("fast", "speed up simulation by using Mbps links instead of Gbps links (bool)", fast);
	cmd.AddValue("apps", "how many apps to run (int)", num_flows);
	cmd.AddValue("buff", "buffer size of each switch (int)", buffer_size);
	cmd.AddValue("sameBuff", "let the buffer size of each PBS queue equal that of the entire DCTCP queue (bool)", same_buff);
	cmd.AddValue("debug", "enable pcap tracing on all hosts and switchs for debugging (bool)", debug);
	cmd.AddValue("thresh", "dctcp threshold value (int)", threshold);
	cmd.AddValue("usePbs", "switch to toggle PBS mode (bool)", use_pbs);
	cmd.AddValue("useDctcp", "switch to toggle DCTCP mode (bool)", use_dctcp);
	cmd.AddValue("load", "load factor (double)", load);
	cmd.AddValue("loadMultiplier", "load factor multiplier to increase number of flows (double)", load_multiplier);
	cmd.AddValue("incast_servers", "number of incast servers (int)", incast_servers);
	cmd.AddValue("incastFlowSize", "size in B of incast flow (int)", incast_flow_size);
	cmd.AddValue("nonblind", "non-blind version of PBS (bool)", nonblind);
	cmd.Parse (argc, argv);

	// settings for incast workload
	string output_directory = "./results/incast/";
	tag = "incast_" + tag;
	uint32_t sim_duration = 5 * num_flows + 1; // seconds

//=========== Define parameters based on value of k ===========//
//
	int num_host = 16;		// number of hosts under a switch
	int num_edge = 9;		// number of edge switch in a pod
	int num_bridge = num_edge;	// number of bridge in a pod
	int num_agg = 4;		// number of aggregation switch in a pod
	int total_host = num_host*num_edge;	// number of hosts in the entire network	
	string filename = output_directory + tag + ".xml";	// filename for Flow Monitor xml output file

// Define variables for BulkSend Application
// These values will be used to serve the purpose that addresses of server and client are selected randomly
// Note: the format of host's address is 10.pod.switch.(host+2)
//
	srand( time(NULL) );

// Initialize other variables
//
	int i = 0;	
	int j = 0;	
	int k = 0;
	int a = 0;
	int h = 0;
	if (a == 0) { // TODO: remove this ???
		a = 0;
	}
	if (k == 0) {
		k = 0;
	}

// Initialize parameters for BulkSend application(s)
//
	int port = 9;
	cout << "Number of Flows = " << num_flows << "\n";

// Initialize parameters for Csma and PointToPoint protocol
//
	std::string rateUnits = "Gbps";
	if (fast) {
		rateUnits = "Mbps";
	}
	std::cout << "Link Rate Units = " << rateUnits << "\n";
	
// Output some useful information
//	
	std::cout << "Total number of hosts =  "<< total_host<<"\n";
	std::cout << "Number of hosts under each switch =  "<< num_host<<"\n";
	std::cout << "Number of edge switch under each pod =  "<< num_edge<<"\n";
	std::cout << "------------- "<<"\n";

	ns3::PacketMetadata::Enable ();

// Initialize Internet Stack and Routing Protocols
//	
	// Basic Settings
	if (use_pbs && !same_buff) {
		buffer_size /= 8;
	}
	Config::SetDefault ("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue (true));
	Config::SetDefault ("ns3::TcpSocketBase::EcnMode", StringValue ("ClassicEcn"));
	Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (1));
	Config::SetDefault ("ns3::RedQueueDisc::UseHardDrop", BooleanValue (false));
	Config::SetDefault ("ns3::RedQueueDisc::LinkDelay", TimeValue (NanoSeconds(0)) );
	Config::SetDefault ("ns3::RedQueueDisc::MaxSize", QueueSizeValue (QueueSize (to_string(buffer_size) + "p")));
	// DCTCP Settings
	if (use_dctcp) {
		Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpDctcp::GetTypeId ()));
		Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (threshold));
		Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (threshold));
		Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (true));
	}

	InternetStackHelper internet;

//=========== Creation of Node Containers ===========//
//
	NodeContainer agg;				// NodeContainer for aggregation switches
	agg.Create (num_agg);
	internet.Install (agg);
	
	NodeContainer edge;				// NodeContainer for edge switches
	edge.Create (num_bridge);
	internet.Install (edge);
	
	NodeContainer bridge;				// NodeContainer for edge bridges
	bridge.Create (num_bridge);
	internet.Install (bridge);

	NodeContainer host[num_edge];		// NodeContainer for hosts
	for (i=0;i<num_edge;i++){  	
		host[i].Create (num_host);		
		internet.Install (host[i]);
	}

//=========== Connect edge switches to hosts ===========//

// NetDeviceContainer for Debug Tracing
//
	NetDeviceContainer debugTracer;

// Initialize Traffic Control Helper
//
	TrafficControlHelper tchHost;
	uint16_t handle = tchHost.SetRootQueueDisc ("ns3::PrioQueueDisc"); 
	tchHost.AddPacketFilter(handle, "ns3::PbsPacketFilter",
				"Alpha", DoubleValue (alpha),
				"Profile", UintegerValue (profile),
		       		"UsePbs", BooleanValue (use_pbs),
				"NonBlind", BooleanValue (nonblind)
	);
	TrafficControlHelper::ClassIdList cls = tchHost.AddQueueDiscClasses (handle, 8, "ns3::QueueDiscClass"); 
	tchHost.AddChildQueueDiscs (handle, cls, "ns3::RedQueueDisc"); // Must use RED to support ECN
	QueueDiscContainer qdiscHost[num_bridge];

	TrafficControlHelper tchSwitch;
        uint16_t handleSwitch = tchSwitch.SetRootQueueDisc ("ns3::PrioQueueDisc"); 
	tchSwitch.AddPacketFilter(handle, "ns3::PbsSwitchPacketFilter");
        TrafficControlHelper::ClassIdList clsSwitch = tchSwitch.AddQueueDiscClasses (handleSwitch, 8, "ns3::QueueDiscClass"); 
        tchSwitch.AddChildQueueDiscs (handleSwitch, clsSwitch, "ns3::RedQueueDisc"); // Must use RED to support ECN
	QueueDiscContainer qdiscEdgeDown[num_edge];
	QueueDiscContainer qdiscEdgeUp[num_agg][num_edge];      
        QueueDiscContainer qdiscAgg[num_agg][num_edge];

// Inintialize Address Helper
//	
  	Ipv4AddressHelper address;

	NetDeviceContainer hostSw[num_bridge];		
	NetDeviceContainer pbsDevices[num_edge];
	NetDeviceContainer edgeDevicesDown[num_edge];
	Ipv4InterfaceContainer ipContainer[num_bridge];

	PointToPointHelper p2p_hostToEdge;
	p2p_hostToEdge.SetDeviceAttribute ("DataRate", StringValue ("10"+rateUnits));
  	p2p_hostToEdge.SetChannelAttribute ("Delay", StringValue ("10ns"));

	for (j=0; j<num_edge; j++){
		for (h=0; h<num_host; h++){
			NetDeviceContainer link = p2p_hostToEdge.Install( edge.Get(j), host[j].Get(h) );
			qdiscHost[j].Add ( tchHost.Install( link.Get (1) ) );
			qdiscEdgeDown[j].Add ( tchSwitch.Install( link.Get (0) ) );
			//Assign subnet
			char *subnet;
			subnet = toString(11, j, h, 0);
			address.SetBase (subnet, "255.255.255.0");
			Ipv4InterfaceContainer tempContainer = address.Assign( link );
			ipContainer[j].Add (tempContainer.Get(1) );

			if (debug) {
				debugTracer.Add( link.Get(0) );
				debugTracer.Add( link.Get(1) );
			}
		}
	}

	std::cout << "Finished connecting edge switches and hosts  "<< "\n";

//=========== Connect aggregate switches to edge switches ===========//
//

// Initialize PointtoPoint helper
//	
	PointToPointHelper p2p_edgeToAgg;
  	p2p_edgeToAgg.SetDeviceAttribute ("DataRate", StringValue ("40"+rateUnits));
  	p2p_edgeToAgg.SetChannelAttribute ("Delay", StringValue ("100ns"));
			     
// Installations...
	NetDeviceContainer ae[num_agg][num_edge]; 	
	Ipv4InterfaceContainer ipAeContainer[num_agg][num_edge];
	for (j=0;j<num_agg;j++){
		for (h=0;h<num_edge;h++){
			ae[j][h] = p2p_edgeToAgg.Install(agg.Get(j), edge.Get(h));
			qdiscEdgeUp[j][h] = tchSwitch.Install(ae[j][h].Get(1));
			qdiscAgg[j][h] = tchSwitch.Install(ae[j][h].Get(0));

			if (debug) {
				debugTracer.Add( ae[j][h].Get(0) );
				debugTracer.Add( ae[j][h].Get(1) );
			}

			int second_octet = 0;
			int third_octet = j+num_host;
			int fourth_octet;
			if (h==0) fourth_octet = 1;
			else fourth_octet = h*2+1;
			//Assign subnet
			char *subnet;
			subnet = toString(10, second_octet, third_octet, 0);
			//Assign base
			char *base;
			base = toString(0, 0, 0, fourth_octet);
			address.SetBase (subnet, "255.255.255.0", base);
			ipAeContainer[j][h] = address.Assign(ae[j][h]);
		}			
	}		

	std::cout << "Finished connecting aggregation switches and edge switches  "<< "\n";

//=========== Workload Generation (Unicast) ===========//
//
	// N FLOWS (TOTAL) - N SOURCES ->  1 FIXED DESTINATION
	//
	ApplicationContainer app[incast_servers][num_flows];
	ApplicationContainer sink;

	// Approx Time to Complete 450kB flow over TCP = ...
	// (prop-delay + handshake-or-ack-pkt-size * 8-bits / botlneck-rate) * 3 +
	// (prop-delay + (flowsize + 40 * flowsize//1460) * 8-bits / botlneck-rate)
	// (220e-9 + 40*8 / 10e9)*3 + (220e-9 + 463e3*8 / 10e9)
	//int time_per_flow = 371000; // nanoseconds
	int time_per_flow = 47424368; // nanoseconds (for 128-degree incast only)
	
	int target_edge = 0;
        int target_host = 0;
	Ipv4Address targetIp = ipContainer[target_edge].GetAddress(target_host);
	Address targetAddress = Address( InetSocketAddress( targetIp, port ));
	// 1-process-per-host . Total number of hosts = incast_servers
	//for (i = 0; i < (int)num_flows; i++)
	//{
		vector<int> senders;
		//for (int i=0; i<incast_servers; ++i) senders.push_back(i);
		for (int i=0; i<incast_servers; ++i) senders.push_back( rand() % (total_host - num_host) + num_host );
		random_shuffle ( senders.begin(), senders.end() );
		auto it = senders.begin();
		for (j = 0; j < incast_servers; j++)
		{
			// select a source (client)
			//int src_edge = (int)(rand() % (num_edge-1) + 0) + 1;
			//int src_host = (int)(rand() % (num_host-1) + 0);
			//int src_edge = (*it) / num_host + 1;
			int src_edge = (*it) / num_host;
			int src_host = (*it) % num_host;
			std::cout << "src=" << (*it) << "\tsrc_edge=" << src_edge << "\tsrc_host=" << src_host << std::endl;
			++it;

			// Initialize BulkSend Application with address of target, and Flowsize
			uint32_t bytesToSend = incast_flow_size;

			//BulkSendHelper bs = BulkSendHelper("ns3::TcpSocketFactory", targetAddress);
			//bs.SetAttribute ("MaxBytes", UintegerValue (bytesToSend));
			//bs.SetAttribute ("SendSize", UintegerValue (1460));

			IncastHelper is = IncastHelper("ns3::TcpSocketFactory", targetAddress);
			is.SetAttribute ("MaxBytes", UintegerValue (bytesToSend));
			is.SetAttribute ("SendSize", UintegerValue (1460));
			//is.SetAttribute ("NumFlows", UintegerValue (1));
			is.SetAttribute ("NumFlows", UintegerValue (num_flows));

			// Install BulkSend Application to the sending node (client)
				//NodeContainer bulksend;
				//bulksend.Add(host[src_edge].Get(src_host));
				//app[j][i] = bs.Install (bulksend);
				//app[j][i].Start (NanoSeconds (time_per_flow * i + (rand() % 1000))); // up to 1us random jitter
			NodeContainer incast;
			incast.Add(host[src_edge].Get(src_host));
			// TODO: fixme - there is no meaningful 'i' anymore!
			app[j][i] = is.Install (incast);
			//app[j][i].Start (NanoSeconds (time_per_flow * i + (rand() % 1000))); // up to 1us random jitter
			app[j][i].Start (NanoSeconds (time_per_flow * 0 + 100)); // up to 1us random jitter
		}
	//}

	// Packet Sink in one fixed destination host
	for(i=0;i<num_edge;i++){
                for(j=0;j<num_host;j++){
                        PacketSinkHelper sh = PacketSinkHelper("ns3::TcpSocketFactory",
                                                Address(InetSocketAddress(Ipv4Address::GetAny(), port)));       
                        sink = sh.Install(host[i].Get(j));
                }
        }
	std::cout << "Finished creating Incast traffic"<<"\n";

//=========== Start the simulation ===========//
//
	std::cout << "Start Simulation.. "<<"\n";

// Populate Routing Tables
//
  	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

// Calculate Throughput using Flowmonitor
//
  	FlowMonitorHelper flowmon;
	Ptr<FlowMonitor> monitor = flowmon.InstallAll();

// Add Tracing for Debugging
//
	if (debug) {
		AsciiTraceHelper ascii;
		p2p_edgeToAgg.EnableAscii( ascii.CreateFileStream (output_directory + tag + ".load.tr"), debugTracer);
		p2p_edgeToAgg.EnablePcapAll( output_directory + tag );
	}

// Run simulation.
//
  	NS_LOG_INFO ("Run Simulation.");
	Simulator::Stop (Seconds(sim_duration));

// Start Simulation
  	Simulator::Run ();

  	monitor->CheckForLostPackets ();

  	monitor->SerializeToXmlFile(filename, true, true);

	std::cout << "Simulation finished "<<"\n";

  	Simulator::Destroy ();
  	NS_LOG_INFO ("Done.");

// Print Results stored in PbsPacketFilters
//
	ofstream pbs_prio;
	ofstream pbs_rawprio;
	pbs_prio.open (output_directory + tag + ".prio");
	pbs_rawprio.open (output_directory + tag + ".rawprio");

	for (i = 0; i < num_edge; i++) {
		for (uint32_t k = 0; k < qdiscHost[i].GetN(); k++)
		{
			Ptr<QueueDisc> q = qdiscHost[i].Get (k);
			PbsPacketFilter *pf = dynamic_cast<PbsPacketFilter*>( PeekPointer (q->GetPacketFilter (0)) );
			pf->StreamToCsv (pbs_prio);
			pf->StreamRawPrioToCsv (pbs_rawprio);
		}
	}

	pbs_prio.close();
	pbs_rawprio.close();

	return 0;
}




