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
#include "ns3/traffic-control-module.h"
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
	bool fast = false;
	bool use_pbs = true;
	bool use_dctcp = true;
	bool oversub = false;
	uint32_t buffer_size = 42400;
	uint32_t threshold = 10;
	double load_multiplier = 6.0; // increase the load by a constant factor to account for TCP slow-start inefficiency!
        double load = 0.6; // load percentage


	CommandLine cmd;
	cmd.AddValue("profile", "identifier of the workload distribution to be tested (int)", profile);
	cmd.AddValue("tag", "desired postfix for output files (string)", tag);
	cmd.AddValue("alpha", "tunable parameter dictating scheduling policy (double)", alpha);
	cmd.AddValue("fast", "speed up simulation by using Mbps links instead of Gbps links (bool)", fast);
	cmd.AddValue("apps", "how many apps to run (int)", num_flows);
	cmd.AddValue("buff", "buffer size of each switch (int)", buffer_size);
	cmd.AddValue("thresh", "dctcp threshold value (int)", threshold);
	cmd.AddValue("usePbs", "switch to toggle PBS mode (bool)", use_pbs);
	cmd.AddValue("useDctcp", "switch to toggle DCTCP mode (bool)", use_dctcp);
	cmd.AddValue("load", "load factor (double)", load);
	cmd.AddValue("loadMultiplier", "load factor multiplier to increase number of flows (double)", load_multiplier);
	cmd.AddValue("oversub", "oversubscribed topo 2:1 (bool)", oversub);
	cmd.Parse (argc, argv);

	string cdf_filename = "";
	string output_directory = "";
	uint32_t sim_duration = 6000;
	switch (profile)
	{
		case 1: // W1
			cdf_filename = "./sizeDistributions/FacebookKeyValue_Sampled.txt";
			output_directory = "./results/W1/";
			tag = "w1_" + tag;
			break;
		case 2: // W2
			cdf_filename = "./sizeDistributions/Google_SearchRPC.txt";
			output_directory = "./results/W2/";
			tag = "w2_" + tag;
			break;
		case 3: // W3
			cdf_filename = "./sizeDistributions/Google_AllRPC.txt";
			output_directory = "./results/W3/";
			tag = "w3_" + tag;
			sim_duration = 9000;
			break;
		case 4: // W4
			cdf_filename = "./sizeDistributions/Facebook_HadoopDist_All.txt";
			output_directory = "./results/W4/";
			tag = "w4_" + tag;
			sim_duration = 24000;
			break;
		case 5: // W5
			cdf_filename = "./sizeDistributions/DCTCP_MsgSizeDist.txt";
			output_directory = "./results/W5/";
			tag = "w5_" + tag;
			sim_duration = 24000;
			break;
	}

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

	int target_swRand = 0;		// Random values for servers' address
	int target_hostRand = 0;	//

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
	if (use_pbs) {
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

// NetDeviceContainer for Network Load calculation via tracing
//
        NetDeviceContainer networkLoadTracer;

// Initialize Traffic Control Helper
//
	TrafficControlHelper tchHost;
	uint16_t handle = tchHost.SetRootQueueDisc ("ns3::PrioQueueDisc"); 
	tchHost.AddPacketFilter(handle, "ns3::PbsPacketFilter",
				"Alpha", DoubleValue (alpha),
				"Profile", UintegerValue (profile),
		       		"UsePbs", BooleanValue (use_pbs)
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

			if (!oversub) {
				networkLoadTracer.Add( link.Get(0) );
			}
		}
	}

	std::cout << "Finished connecting edge switches and hosts  "<< "\n";


//=========== Connect aggregate switches to edge switches ===========//
//

// Initialize PointtoPoint helper
//	
	PointToPointHelper p2p_edgeToAgg;
	if (oversub) { // 2:1 oversubscription
  		p2p_edgeToAgg.SetDeviceAttribute ("DataRate", StringValue ("20"+rateUnits));
	} else {
  		p2p_edgeToAgg.SetDeviceAttribute ("DataRate", StringValue ("40"+rateUnits));
	}
  	p2p_edgeToAgg.SetChannelAttribute ("Delay", StringValue ("100ns"));
			     
// Installations...
	NetDeviceContainer ae[num_agg][num_edge]; 	
	Ipv4InterfaceContainer ipAeContainer[num_agg][num_edge];
	for (j=0;j<num_agg;j++){
		for (h=0;h<num_edge;h++){
			ae[j][h] = p2p_edgeToAgg.Install(agg.Get(j), edge.Get(h));
			qdiscEdgeUp[j][h] = tchSwitch.Install(ae[j][h].Get(1));
			qdiscAgg[j][h] = tchSwitch.Install(ae[j][h].Get(0));

			if (oversub) {
				networkLoadTracer.Add( ae[j][h].Get(0) );
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

//=========== Initialize settings for BulkSend Application ===========//
//
	ifstream cdfFile;
	cdfFile.open(cdf_filename);
	if (!cdfFile) {
		cerr << "Unable to open file " << cdf_filename << endl;
		cerr << "usage: ./waf --run \"scratch/homatopology --cdfFile=<full path to flowsize CDF file> --alpha=x.x --fast=<true/false> --out=<name for output file>\"" << endl;
		exit(1);
	}

	string line;
	int linecnt = 1;
	double flowsize = 0.0;
	double prob = 0.0;
	cdfFile >> line; // discard first line, which is the average flow-size
	
	Ptr<EmpiricalRandomVariable> flow_size_rv = CreateObject<EmpiricalRandomVariable> ();

	// Each line contains : <number of bytes>   <cumulative probability>
	while (cdfFile >> line) {
		// even :: number of bytes
		if (++linecnt % 2 == 0) {
			flowsize = stof(line);
			//cout << "flow-size: " << flowsize << "\t\t";

		// odd :: probability of producing that number of bytes
		} else {
			prob = stof(line);
			flow_size_rv->CDF (flowsize, prob);
			//cout << "probability: " << prob << endl;
		}
	}
	cdfFile.close();	

//=============  Generate traffics for the simulation =============
//	
	// N FLOWS (TOTAL) - RANDOM SOURCE --> RANDOM DEST ; POISSON ARRIVALS
	//
	// -- In NS3, this is typically achieved using an OnOffApplication
	// -- 	in which OnTime is determined by a Uniform Random Variable
	// --   and OffTime is determined by an Exponential Random Variable.
	// -- We will use an Empirical Random Variable to select the flow sizes.
	//
	// -- To achieve a given aggregate network load, we will make use of
	// --   the property of Poisson Processes that the superposition of
	// --   many such processes from independent sources each with rate
	// --   lambda_i results in an aggreagate rate SUM_i (lambda_i).
	//
	// -- Note: If lambda is the rate, then 1/lambda is the average
	// --   interarrival time of the corresponding Exponential Distribution.
	//
	double link_rate = 10e9; // Bottleneck link rate
	double avgPacketSize = 1460; // bytes
	double meanFlowSize = 1138 * avgPacketSize; // num_packets
	switch (profile)
	{
		case 1:
			meanFlowSize = 407.1;
			break;
		case 2:
			meanFlowSize = 654.4;
			break;
		case 3:
			meanFlowSize = 3259.3;
			break;
		case 4:
			meanFlowSize = 136469.3;
			break;
		case 5:
			meanFlowSize = 1819002.1;
			break;
	}
	//double lam = (link_rate*load)/(meanFlowSize*8.0/1460*1500);
	double lam = (link_rate*load*load_multiplier)/(meanFlowSize*8.0);
	cout << "Lambda Per Source: " << lam << " flows/s per source\n";
	double mean_t = 1.0/lam * 10e9; // nanoseconds
	cout << "Mean Inter-Arrival Time: " << mean_t << " ns\n";

	int num_apps_per_host = max(1, (int)(num_flows / 144));
	ApplicationContainer app[num_edge][num_host][num_apps_per_host];
	ApplicationContainer sink;
	
	// 144 Poisson Process, 1-process-per-host 
	for (i = 0; i < num_edge; i++)
	{
		for (j = 0; j < num_host; j++)
		{
		// Create independent Exponential RV for local Poisson Process on this host
			Ptr<ExponentialRandomVariable> expRv = CreateObject<ExponentialRandomVariable> ();
			expRv->SetAttribute ("Mean", DoubleValue (mean_t) );

		// Initialize Starting Time for 1st app on this host
			uint64_t next_app_start_time = (uint64_t)(expRv->GetValue ());

		// Create applications on this host
			for (k = 0; k < num_apps_per_host; k++)
			{
			// Randomly select a target (server)
				target_swRand = rand() % num_edge + 0;
				target_hostRand = rand() % num_host + 0;
				while (target_swRand == i){
			 		target_swRand = rand() % num_edge + 0;
			 	} // to make sure that client and server are not in same rack
				Ipv4Address targetIp = ipContainer[target_swRand].GetAddress(target_hostRand);
				Address targetAddress = Address( InetSocketAddress( targetIp, port ));

			// Initialize BulkSend Application with address of target, and Flowsize from Empirical RV
				uint32_t bytesToSend = (uint32_t) flow_size_rv->GetValue();
				//uint32_t bytesToSend = (uint32_t)(407.1);
				BulkSendHelper bs = BulkSendHelper("ns3::TcpSocketFactory", targetAddress);
				bs.SetAttribute ("MaxBytes", UintegerValue (bytesToSend));
				bs.SetAttribute ("SendSize", UintegerValue (1460));

			// Install BulkSend Application to the sending node (client)
				NodeContainer bulksend;
				bulksend.Add(host[i].Get(j));
				app[i][j][k] = bs.Install (bulksend);

			// Set Approximate Poisson Arrival Start Time for next Flow on this host
				app[i][j][k].Start (NanoSeconds (next_app_start_time));
				next_app_start_time += (uint64_t)(expRv->GetValue ());

			} // num_apps_per_host
		} // num_hosts
	} // num_edges

	// Packet Sink in every potential target Host
	for(i=0;i<num_edge;i++){
		for(j=0;j<num_host;j++){
			PacketSinkHelper sh = PacketSinkHelper("ns3::TcpSocketFactory",
						Address(InetSocketAddress(Ipv4Address::GetAny(), port)));	
			sink = sh.Install(host[i].Get(j));
		}
	}
/*
	// FOR ONE FLOW
	ApplicationContainer app;
	ApplicationContainer sink;

	// Random jitter for first app start time
	//uint64_t next_app_start = rand() % num_apps_per_host*10000 + 0;
	double mean = 3.14;
	double bound = 0.0;
	Ptr<ExponentialRandomVariable> x = CreateObject<ExponentialRandomVariable> ();
	x->SetAttribute ("Mean", DoubleValue (mean));
	x->SetAttribute ("Bound", DoubleValue (bound));
	//uint64_t next_app_start = x->GetValue() * 1000000000; // times 1s in ns
	uint64_t next_app_start = 0;

	// Arbitrarily select a source (client)
	int source_swRand = 0;
	int source_hostRand = 0;

	// Randomly select a target (server)
	target_swRand = rand() % num_edge + 0;
	target_hostRand = rand() % num_host + 0;
	while (target_swRand == source_swRand){
		target_swRand = rand() % num_edge + 0;
		target_hostRand = rand() % num_host + 0;
	} // to make sure that client and server are different

	Ipv4Address targetIp = ipContainer[target_swRand].GetAddress(target_hostRand);
	InetSocketAddress targetSock = InetSocketAddress(targetIp, port);
	Address targetAddress = Address(targetSock);

	// Initialize Bulk Send Application 1 with address of target
	//uint32_t bytesToSend = (uint32_t) flow_size_rv->GetValue();
	//uint32_t bytesToSend = 100000000;
	//uint32_t bytesToSend = 1000000000;
	uint32_t bytesToSend = 100;
	cout << "Bytes To Send: " << bytesToSend << endl;
	BulkSendHelper bs = BulkSendHelper("ns3::TcpSocketFactory", targetAddress);
	bs.SetAttribute ("MaxBytes", UintegerValue (bytesToSend));

	// Install BulkSend Application to the sending node (client)
	NodeContainer bulksend;
	bulksend.Add(host[source_swRand].Get(source_hostRand));
	app = bs.Install (bulksend);

	// Set Ontime & Time to start next application 
	app.Start (NanoSeconds (next_app_start));
			
	// Packet Sink in every potential target Host
	for(i=0;i<num_edge;i++){
		for(j=0;j<num_host;j++){
			PacketSinkHelper sh = PacketSinkHelper("ns3::TcpSocketFactory",
						Address(InetSocketAddress(Ipv4Address::GetAny(), port)));	
			sink = sh.Install(host[i].Get(j));
		}
	}


*/	

	std::cout << "Finished creating BulkSend traffic"<<"\n";

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

// Add Tracing to Track Egress Traffic
//
	AsciiTraceHelper ascii;
	p2p_edgeToAgg.EnableAscii( ascii.CreateFileStream (output_directory + tag + ".load.tr"), networkLoadTracer );

// Run simulation.
//
  	NS_LOG_INFO ("Run Simulation.");
	Simulator::Stop (MicroSeconds(sim_duration));

// Start Simulation
  	Simulator::Run ();

  	monitor->CheckForLostPackets ();

	//Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
	//FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
	//for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
	//{
	//	Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
	//	std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
	//	std::cout << "  Source Port " << t.sourcePort << "\n";
	//	std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
	//	std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
	//	std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";
	//	std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
	//	std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
	//	std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 9.0 / 1000 / 1000  << " Mbps\n";

	//	uint64_t duration = (i->second.timeLastRxPacket - i->second.timeFirstTxPacket).GetNanoSeconds ();
	//	std::cout << "  Duration:   " << duration << "\n";
	//}

  	monitor->SerializeToXmlFile(filename, true, true);

	std::cout << "Simulation finished "<<"\n";

  	Simulator::Destroy ();
  	NS_LOG_INFO ("Done.");

// Print Results stored in PbsPacketFilters
//
	ofstream pbs_stats;
	ofstream pbs_egress;
	ofstream pbs_packets;
	ofstream pbs_prio;
	ofstream pbs_rawprio;
	ofstream pbs_categories;
	pbs_stats.open (output_directory + tag + ".stats");
	pbs_egress.open (output_directory + tag + ".egress");
	pbs_packets.open (output_directory + tag + ".packets");
	pbs_prio.open (output_directory + tag + ".prio");
	pbs_rawprio.open (output_directory + tag + ".rawprio");
	pbs_categories.open (output_directory + tag + ".categories");

	pbs_stats << "\n*** QueueDisc statistics (Host) ***\n";
	for (i = 0; i < num_edge; i++) {
		for (uint32_t k = 0; k < qdiscHost[i].GetN(); k++)
		{
			pbs_stats << "Host[" << i << "][" << k << "]\n";
			Ptr<QueueDisc> q = qdiscHost[i].Get (k);
			PbsPacketFilter *pf = dynamic_cast<PbsPacketFilter*>( PeekPointer (q->GetPacketFilter (0)) );
			pf->PrintStats (pbs_stats);
			pf->StreamToCsv (pbs_prio);
			pf->StreamPacketsToCsv (pbs_packets);
			pf->StreamRawPrioToCsv (pbs_rawprio);

			map<uint64_t, uint64_t> loadAtTime = pf->PeekLoadAtTime ();
			for (auto it = loadAtTime.begin(); it != loadAtTime.end(); ++it)
			{
				pbs_egress << it->first << "," << it->second << ",10" << rateUnits << "\n";
			}
		}
	}

	pbs_stats << "\n*** QueueDisc statistics (EdgeDown) ***\n";
	for (i = 0; i < num_edge; i++) {
		for (uint32_t k = 0; k < qdiscEdgeDown[i].GetN(); k++)
		{
			pbs_stats << "EdgeDown[" << i << "]\n";
			Ptr<QueueDisc> q = qdiscEdgeDown[i].Get (k);
			PbsSwitchPacketFilter *pf = dynamic_cast<PbsSwitchPacketFilter*>( PeekPointer (q->GetPacketFilter (0)) );
			pf->PrintStats (pbs_stats);
		}
	}

	pbs_stats << "\n*** QueueDisc statistics (EdgeUp) ***\n";
	for (i = 0; i < num_agg; i++) {
		for (int k = 0; k < num_edge; k++)
		{
			pbs_stats << "EdgeUp[" << i << "][" << k << "]\n";
			Ptr<QueueDisc> q = qdiscEdgeUp[i][k].Get (0);
			PbsSwitchPacketFilter *pf = dynamic_cast<PbsSwitchPacketFilter*>( PeekPointer (q->GetPacketFilter (0)) );
			pf->PrintStats (pbs_stats);
		}
	}

	// To calculate Network load: snapshot total egress from every switch every nanosecond
	// To get "total egress" we look at the packets enqueued to 1 group of packet filters
	// - agg - will see egress sent from agg->edges, which is limited by the max BsBw
	pbs_egress << "#<Time Bucket>,<Total Egress Bytes-Per-Second>,<Link Rate>\n";

	pbs_stats << "\n*** QueueDisc statistics (Agg) ***\n";
	pbs_categories << "flow_id,total_bytes,p0_bytes,p1_bytes,p2_bytes,p3_bytes,p4_bytes,p5_bytes,p6_bytes,p7_bytes,\n";
	for (i = 0; i < num_agg; i++) {
		for (int k = 0; k < num_edge; k++)
		{
			pbs_stats << "Agg[" << i << "][" << k << "]\n";
			Ptr<QueueDisc> q = qdiscAgg[i][k].Get (0);
			PbsSwitchPacketFilter *pf = dynamic_cast<PbsSwitchPacketFilter*>( PeekPointer (q->GetPacketFilter (0)) );
			pf->PrintStats (pbs_stats);

			pf->PrintFlowCategories (pbs_categories);

			//// for every down-facing aggregate-switch filter, for every nanosecond in which that filter is processing a packet
			//// record the egress in total bytes from that filter
			//map<uint64_t, uint64_t> loadAtTime = pf->PeekLoadAtTime ();
			//for (auto it = loadAtTime.begin(); it != loadAtTime.end(); ++it)
			//{
			//	pbs_egress << it->first << "," << it->second << ",40" << rateUnits << "\n";
			//}
		}
	}


	pbs_stats.close();
	pbs_egress.close();
	pbs_packets.close();
	pbs_prio.close();
	pbs_rawprio.close();
	pbs_categories.close();

	return 0;
}




