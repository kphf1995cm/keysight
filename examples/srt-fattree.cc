/*
* Created By Kuang Peng On 2018/4/8
*/
#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <unistd.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unordered_map>

#include "ns3/flow-monitor-module.h"
#include "ns3/bridge-helper.h"
#include "ns3/bridge-net-device.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-nix-vector-helper.h"
#include "ns3/random-variable-stream.h"
#include "ns3/keysight.h"
#include "ns3/keysight_para.h"
/*
- The code is constructed in the following order:
1. Creation of Node Containers
2. Initialize settings for UdpEcho Client/Server Application
3. Connect hosts to edge switches
4. Connect edge switches to aggregate switches
5. Connect aggregate switches to core switches
6. Start Simulation

- Addressing scheme:
1. Address of host: 10.pod.edge.0 /24
2. Address of edge and aggregation switch: 10.pod.(agg + pod/2).0 /24
3. Address of core switch: 10.(group + pod).core.0 /24
(Note: there are k/2 group of core switch)

- Application Setting:
- PacketSize: (Send packet data size (byte))
- Interval: (Send packet interval time (s))
- MaxPackets: (Send packet max number)

- Channel Setting:
- DataRate: (Gbps)
- Delay: (ms)

- Simulation Settings:
- Number of pods (k): 4-24 (run the simulation with varying values of k)
- Number of hosts: 16-3456
- UdpEchoClient/Server Communication Pairs:
[k] client connect with [hostNum-k-1] server (k from 0 to halfHostNum-1)
- Simulation running time: 100 seconds
- Traffic flow pattern:
- Routing protocol: Nix-Vector
*/


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("KeysightExample");

unsigned long getTickCount(void)
{
	unsigned long currentTime = 0;
#ifdef WIN32
	currentTime = GetTickCount();
#endif
	struct timeval current;
	gettimeofday(&current, NULL);
	currentTime = current.tv_sec * 1000 + current.tv_usec / 1000;
#ifdef OS_VXWORKS
	ULONGA timeSecond = tickGet() / sysClkRateGet();
	ULONGA timeMilsec = tickGet() % sysClkRateGet() * 1000 / sysClkRateGet();
	currentTime = timeSecond * 1000 + timeMilsec;
#endif
	return currentTime;
}

// Function to create address string from numbers
//
char * toString(int a, int b, int c, int d) {

	int first = a;
	int second = b;
	int third = c;
	int fourth = d;

	char *address = new char[30];
	char firstOctet[30], secondOctet[30], thirdOctet[30], fourthOctet[30];

	bzero(address, 30);

	snprintf(firstOctet, 10, "%d", first);
	strcat(firstOctet, ".");
	snprintf(secondOctet, 10, "%d", second);
	strcat(secondOctet, ".");
	snprintf(thirdOctet, 10, "%d", third);
	strcat(thirdOctet, ".");
	snprintf(fourthOctet, 10, "%d", fourth);

	strcat(thirdOctet, fourthOctet);
	strcat(secondOctet, thirdOctet);
	strcat(firstOctet, secondOctet);
	strcat(address, firstOctet);

	return address;
}

// Count All Switch 
//
uint64_t packetSum = 0;
uint64_t tuplePostcardSum = 0;
uint64_t keysightPostcardSum = 0;

//Output tuple
//
void ShowPacketAndTuple(Ptr<Node> s)
{
	std::cout << "Receive Packet Sum(Including Ipv4, Arp packets): " << s->m_packetNum << std::endl;
	packetSum += (uint64_t)s->m_packetNum;
	tuple_t* tp = &s->m_tuple;
	std::cout << "Everflow distinct flow count:" << tp->distinct_flow_count << std::endl;
	tuplePostcardSum += (uint64_t)tp->distinct_flow_count;
}
// Output keysight
//
void ShowKeysight(Ptr<Node> s)
{
	keysight_t* ks = &s->m_keysight;
	std::cout << "Keysight postcard count:" << ks->postcard_count << std::endl;
	keysightPostcardSum += (uint64_t)ks->postcard_count;
}

// Output All Switch Sum
//
void ShowDifference()
{
	std::cout << "All Switch Receive Packet Sum(Including Ipv4, Arp packets):" << packetSum << std::endl;
	std::cout << "All Switch Everflow distinct flow count:" << tuplePostcardSum << std::endl;
	std::cout << "All Switch Keysight postcard count:" << keysightPostcardSum << std::endl;
}


int
main(int argc, char *argv[])
{
	unsigned long start = getTickCount();
	LogComponentEnable("KeysightExample", LOG_LEVEL_LOGIC);

	int pod = 4;

	// Initialize other variables
	//
	int i = 0;
	int j = 0;
	int h = 0;

	// Initialize parameters for UdpEcho Client/Server application
	//

	
	 
	
	// OnOff Application
	//
	unsigned int onoffPacketSize = 512;            // send packet size (OnOff byte or bit ?)
	char onOffDataRate[] = "10Mbps";// TO DO: reset a reasonable value
	unsigned int minBytes = 1024;
	unsigned int maxBytes = 1024 * 1000;
	unsigned int miceBytes = 10 * 1024;// mice flow 10KB (80% quantity but only occupys 20% flow sum)
	unsigned int elephantBytes = 160 * 1024; //elephant flow 160KB (20% quantity but occupys 80% flow sum)

	// Tcp/Udp Echo Application
	//
	int port = 9;
	unsigned int echoPacketSize = 100;// send packet data size (byte)
	double interval = 1; // send packet interval time (ms)
	unsigned int minPackets = 1;
	unsigned int maxPackets = 100;
	unsigned int micePackets = 10 ;// mice flow 10KB (80% quantity but only occupys 20% flow sum)
	unsigned int elephantPackets = 160; //elephant flow 160KB (20% quantity but occupys 80% flow sum)

											 // Initialize parameters for Csma and PointToPoint protocol
											 //
	char defaultDataRate[] = "10Gbps";   // 10Gbps
	double delay = 0.001;           // 0.001 (ms)

	char coreAggDataRate[] = "100Gbps";
	char AggEdgeDataRate[] = "40Gbps";
	char EdgeHostDataRate[] = "10Gbps";


	// Initalize parameters for UdpEcho Client/Server Appilication 
	//
	int clientStartTime = 1; // UdpEchoClient Start Time (s)
	int clientStopTime = 101;
	int serverStartTime = 0; // UdpEchoServer Start Time (s)
	int serverStopTime = 102;

	int pairNum = 100;
	int routeProtocolType = 0; // [0 NixVector] [1 ECMP]
	int flowType = 0;// [0 exponential] [1 normal] [2 constant] [3 28distribution]
	int generateTrafficWay = 0;//[0 Onoff Random Pair][1 UdpEcho Random Pair] [2 UdpEcho Front End Pair]
	int txProtocolType = 0;// [0 Tcp] [1 Udp]

						   //=========== Define command parameter ===========
						   //
	CommandLine cmd;
	cmd.AddValue("pod", "Numbers of pod", pod);
	cmd.AddValue("pair", "Numbers of communication pairs", pairNum);
	cmd.AddValue("route", "Route Protocol Type [0 NixVector] [1 ECMP]", routeProtocolType);
	cmd.AddValue("flow", "Flow Distribution Type [0 exponential] [1 normal] [2 constant] [3 28distribution]", flowType);
	cmd.AddValue("traffic", "Application Generate Traffic Way [0 Onoff Random Pair][1 UdpEcho Random Pair] [2 TcpEcho Random Pair][3 UdpEcho Front End Pair]", generateTrafficWay);
	cmd.AddValue("txproto", "Transport Layer Protocol [0 Tcp] [1 Udp]", txProtocolType);
	cmd.AddValue("minbytes", "Flow min bytes", minBytes);
	cmd.AddValue("maxbytes", "Flow max bytes", maxBytes);
	cmd.AddValue("minpackets", "Flow min packets", minPackets);
	cmd.AddValue("maxpackets", "Flow max packets", maxPackets);
	cmd.AddValue("bfalg", "[0 BSBF][1 BSBFSD][2 RLBSBF][3 STABLE_BF][4 IDEAL_BF][5 KEYSIGHT_SBF]", KeysightPara::g_BF_ALG);
	cmd.AddValue("bfsize", "", KeysightPara::g_BF_SIZE)
	cmd.AddValue("bfnum", "", KeysightPara::g_BF_NUM)
	cmd.AddValue("bfmax", "", KeysightPara::g_BF_MAX)
	cmd.AddValue("perwindowpkt", "", KeysightPara::g_PACKET_PER_WINDOW)
	cmd.AddValue("windownum", "", KeysightPara::g_WINDOW_NUM)
	cmd.AddValue("bucketnum", "", KeysightPara::g_BUCKET_NUM)
	cmd.Parse(argc, argv);

	//=========== Define parameters based on value of k ===========//
	//
	int k = pod;                    // number of ports per switch
	int num_pod = k;                // number of pod
	int num_host = (k / 2);         // number of hosts under a switch
	int num_edge = (k / 2);         // number of edge switch in a pod
	int num_bridge = num_edge;      // number of bridge in a pod
	int num_agg = (k / 2);          // number of aggregation switch in a pod
	int num_group = k / 2;          // number of group of core switches
	int num_core = (k / 2);         // number of core switch in a group
	int total_host = k*k*k / 4;     // number of hosts in the entire net							

									// Output some useful information
									//	
	std::cout << "Value of k =  " << k << "\n";
	std::cout << "Total number of hosts =  " << total_host << "\n";
	std::cout << "Number of hosts under each switch =  " << num_host << "\n";
	std::cout << "Number of edge switch under each pod =  " << num_edge << "\n";
	std::cout << "Numbers of communication pairs =" << pairNum << "\n";
	std::cout << "------------- " << "\n";

	// Initialize Internet Stack and Routing Protocols
	//	
	InternetStackHelper internet;
	Ipv4NixVectorHelper nixRouting;
	Ipv4StaticRoutingHelper staticRouting;
	Ipv4ListRoutingHelper list;
	list.Add(staticRouting, 0);
	list.Add(nixRouting, 10);
	internet.SetRoutingHelper(list);

	//=========== Creation of Node Containers ===========//
	//
	NodeContainer core[num_group];				// NodeContainer for core switches
	for (i = 0; i<num_group; i++) {
		core[i].Create(num_core);
		internet.Install(core[i]);
	}
	NodeContainer agg[num_pod];				// NodeContainer for aggregation switches
	for (i = 0; i<num_pod; i++) {
		agg[i].Create(num_agg);
		internet.Install(agg[i]);
	}
	NodeContainer edge[num_pod];				// NodeContainer for edge switches
	for (i = 0; i<num_pod; i++) {
		edge[i].Create(num_edge);
		internet.Install(edge[i]);
	}
	NodeContainer bridge[num_pod];				// NodeContainer for edge bridges
	for (i = 0; i<num_pod; i++) {
		bridge[i].Create(num_bridge);
		internet.Install(bridge[i]);
	}
	NodeContainer host[num_pod][num_bridge];		// NodeContainer for hosts
	for (i = 0; i<k; i++) {
		for (j = 0; j<num_bridge; j++) {
			host[i][j].Create(num_host);
			internet.Install(host[i][j]);
		}
	}

	std::cout << "Finished creating Node Containers" << "\n";

	//=========== Initialize settings for UdpEcho Client/Server Application ===========//
	//

	// Generate traffics for the simulation
	//	

	Config::SetDefault("ns3::Ipv4RawSocketImpl::Protocol", StringValue("2"));

	if (generateTrafficWay == 0)
	{
		int client_i, server_i;
		char* addr;
		Ptr<ns3::UniformRandomVariable> urv = CreateObject<UniformRandomVariable>();
		Ptr<ns3::ExponentialRandomVariable> erv = CreateObject<ExponentialRandomVariable>();
		Ptr<ns3::NormalRandomVariable> nrv = CreateObject<NormalRandomVariable>();
		for (int i = 0; i < pairNum; i++)
		{
			client_i = urv->GetValue(0, total_host);//parameter:(min,max)range:[0,total_host)
			server_i = urv->GetValue(0, total_host);
			while (server_i == client_i)
				server_i = urv->GetValue(0, total_host);

			int s_p, s_q, s_t;
			s_p = server_i / (num_edge*num_host);
			s_q = (server_i - s_p*num_edge*num_host) / num_host;
			s_t = server_i%num_host;

			int p, q, t;
			p = client_i / (num_edge*num_host);
			q = (client_i - p*num_edge*num_host) / num_host;
			t = client_i%num_host;

			// Specify dst ip (server ip)
			addr = toString(10, s_p, s_q, s_t + 2);
			Ipv4Address dstIp = Ipv4Address(addr);
			delete addr;
			InetSocketAddress dst = InetSocketAddress(dstIp);

			// Install client
			OnOffHelper onOff("ns3::TcpSocketFactory", dst);
			if (txProtocolType == 0) //tcp
				onOff = OnOffHelper("ns3::TcpSocketFactory", dst);
			else //udp
				onOff = OnOffHelper("ns3::UdpSocketFactory", dst);
			onOff.SetAttribute("PacketSize", UintegerValue(onoffPacketSize));
			onOff.SetAttribute("DataRate", StringValue(onOffDataRate));
			onOff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=2]"));
			onOff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
			if (flowType == 0) //exponential
				onOff.SetAttribute("MaxBytes", UintegerValue(erv->GetValue((minBytes + maxBytes) / 2, maxBytes)));//parameter:(mean,bound) range:[0,bound)
			if (flowType == 1) //normal
				onOff.SetAttribute("MaxBytes", UintegerValue(nrv->GetValue((minBytes + maxBytes) / 2, 2, (maxBytes - minBytes) / 2)));//parameter:(mean,variance,bound) range:[mean-bound,mean+bound]
			if (flowType == 2) //constant
				onOff.SetAttribute("MaxBytes", UintegerValue(minBytes));//for test
			if (flowType == 3) //28distribution
			{
				if (urv->GetValue(0, 100) >= 20) // 80%
					onOff.SetAttribute("MaxBytes", UintegerValue(miceBytes));
				else
					onOff.SetAttribute("MaxBytes", UintegerValue(elephantBytes));
			}
			ApplicationContainer clientApp = onOff.Install(host[p][q].Get(t));
			clientApp.Start(Seconds(clientStartTime));
			clientApp.Stop(Seconds(clientStopTime));

			// Install server
			PacketSinkHelper sink("ns3::TcpSocketFactory", dst);
			if (txProtocolType == 0) //tcp
				sink = PacketSinkHelper("ns3::TcpSocketFactory", dst);
			else //udp
				sink = PacketSinkHelper("ns3::UdpSocketFactory", dst);
			ApplicationContainer serverApp = sink.Install(host[s_p][s_q].Get(s_t));
			serverApp.Start(Seconds(serverStartTime));
			serverApp.Stop(Seconds(serverStopTime));

		}
	}
	if (generateTrafficWay == 1)//random traffic generated by UdpEcho
	{
		char *addr;
		int client_i, server_i;
		Ptr<ns3::UniformRandomVariable> urv = CreateObject<UniformRandomVariable>();
		Ptr<ns3::ExponentialRandomVariable> erv = CreateObject<ExponentialRandomVariable>();
		Ptr<ns3::NormalRandomVariable> nrv = CreateObject<NormalRandomVariable>();
		for (int i = 0; i < pairNum; i++)
		{
			client_i = urv->GetValue(0, total_host);//parameter:(min,max)range:[0,total_host)
			server_i = urv->GetValue(0, total_host);
			while (server_i == client_i)
				server_i = urv->GetValue(0, total_host);
			int s_p, s_q, s_t;
			s_p = server_i / (num_edge*num_host);
			s_q = (server_i - s_p*num_edge*num_host) / num_host;
			s_t = server_i%num_host;

			int p, q, t;
			p = client_i / (num_edge*num_host);
			q = (client_i - p*num_edge*num_host) / num_host;
			t = client_i%num_host;

			//specify dst ip (server ip)
			addr = toString(10, s_p, s_q, s_t + 2);
			Ipv4Address dstIp = Ipv4Address(addr);
			delete addr;
			
			//Install client app
			UdpEchoClientHelper echoClient(dstIp, port);
			if (flowType == 0) //exponential
				echoClient.SetAttribute("MaxPackets", UintegerValue(erv->GetValue((minPackets + maxPackets) / 2, maxPackets)));//parameter:(mean,bound) range:[0,bound)
			if (flowType == 1) //normal
				echoClient.SetAttribute("MaxPackets", UintegerValue(nrv->GetValue((minPackets + maxPackets) / 2, 2, (maxPackets - minPackets) / 2)));//parameter:(mean,variance,bound) range:[mean-bound,mean+bound]
			if (flowType == 2) //constant
				echoClient.SetAttribute("MaxPackets", UintegerValue(minPackets));//for test
			if (flowType == 3) //28distribution
			{
				if (urv->GetValue(0, 100) >= 20) // 80%
					echoClient.SetAttribute("MaxPackets", UintegerValue(micePackets));
				else
					echoClient.SetAttribute("MaxPackets", UintegerValue(elephantPackets));
			}
			echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(interval)));
			echoClient.SetAttribute("PacketSize", UintegerValue(echoPacketSize));
			ApplicationContainer clientApp = echoClient.Install(host[p][q].Get(t));
			clientApp.Start(Seconds(clientStartTime));
			clientApp.Stop(Seconds(clientStopTime));

			//Install server app
			UdpEchoServerHelper echoServer(port);
			ApplicationContainer serverApp = echoServer.Install(host[s_p][s_q].Get(s_t));
			serverApp.Start(Seconds(serverStartTime));
			serverApp.Stop(Seconds(serverStopTime));
		}
	}
	if (generateTrafficWay == 2)//random traffic generated by TcpEcho
	{
		char *addr;
		int client_i, server_i;
		Ptr<ns3::UniformRandomVariable> urv = CreateObject<UniformRandomVariable>();
		Ptr<ns3::ExponentialRandomVariable> erv = CreateObject<ExponentialRandomVariable>();
		Ptr<ns3::NormalRandomVariable> nrv = CreateObject<NormalRandomVariable>();
		for (int i = 0; i < pairNum; i++)
		{
			client_i = urv->GetValue(0, total_host);//parameter:(min,max)range:[0,total_host)
			server_i = urv->GetValue(0, total_host);
			while (server_i == client_i)
				server_i = urv->GetValue(0, total_host);
			int s_p, s_q, s_t;
			s_p = server_i / (num_edge*num_host);
			s_q = (server_i - s_p*num_edge*num_host) / num_host;
			s_t = server_i%num_host;

			int p, q, t;
			p = client_i / (num_edge*num_host);
			q = (client_i - p*num_edge*num_host) / num_host;
			t = client_i%num_host;

			//specify dst ip (server ip)
			addr = toString(10, s_p, s_q, s_t + 2);
			Ipv4Address dstIp = Ipv4Address(addr);
			delete addr;

			//Install client app
			TcpEchoClientHelper echoClient(dstIp, port);
			if (flowType == 0) //exponential
				echoClient.SetAttribute("MaxPackets", UintegerValue(erv->GetValue((minPackets + maxPackets) / 2, maxPackets)));//parameter:(mean,bound) range:[0,bound)
			if (flowType == 1) //normal
				echoClient.SetAttribute("MaxPackets", UintegerValue(nrv->GetValue((minPackets + maxPackets) / 2, 2, (maxPackets - minPackets) / 2)));//parameter:(mean,variance,bound) range:[mean-bound,mean+bound]
			if (flowType == 2) //constant
				echoClient.SetAttribute("MaxPackets", UintegerValue(minPackets));//for test
			if (flowType == 3) //28distribution
			{
				if (urv->GetValue(0, 100) >= 20) // 80%
					echoClient.SetAttribute("MaxPackets", UintegerValue(micePackets));
				else
					echoClient.SetAttribute("MaxPackets", UintegerValue(elephantPackets));
			}
			echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(interval)));
			echoClient.SetAttribute("PacketSize", UintegerValue(echoPacketSize));
			ApplicationContainer clientApp = echoClient.Install(host[p][q].Get(t));
			clientApp.Start(Seconds(clientStartTime));
			clientApp.Stop(Seconds(clientStopTime));

			//Install server app
			TcpEchoServerHelper echoServer(port);
			ApplicationContainer serverApp = echoServer.Install(host[s_p][s_q].Get(s_t));
			serverApp.Start(Seconds(serverStartTime));
			serverApp.Stop(Seconds(serverStopTime));
		}
	}
	if (generateTrafficWay == 3)
	{
		char *addr;
		int half_host_num = total_host / 2;
		//half_host_num = 1;
		for (int client_i = 0; client_i < half_host_num; client_i++)
		{
			int server_i = total_host - client_i - 1;

			int s_p, s_q, s_t;
			s_p = server_i / (num_edge*num_host);
			s_q = (server_i - s_p*num_edge*num_host) / num_host;
			s_t = server_i%num_host;

			int p, q, t;
			p = client_i / (num_edge*num_host);
			q = (client_i - p*num_edge*num_host) / num_host;
			t = client_i%num_host;

			//specify dst ip (server ip)
			addr = toString(10, s_p, s_q, s_t + 2);
			Ipv4Address dstIp = Ipv4Address(addr);
			delete addr;

			//install server app
			UdpEchoServerHelper echoServer(port);
			ApplicationContainer serverApp = echoServer.Install(host[s_p][s_q].Get(s_t));
			serverApp.Start(Seconds(serverStartTime));
			serverApp.Stop(Seconds(serverStopTime));

			//install client app
			UdpEchoClientHelper echoClient(dstIp, port);
			echoClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
			echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(interval)));
			echoClient.SetAttribute("PacketSize", UintegerValue(echoPacketSize));
			ApplicationContainer clientApp = echoClient.Install(host[p][q].Get(t));
			clientApp.Start(Seconds(clientStartTime));
			clientApp.Stop(Seconds(clientStopTime));
		}
	}

	std::cout << "Finished creating UdpEchoClient/UdpEchoServer traffic" << "\n";


	// Inintialize Address Helper
	//	
	Ipv4AddressHelper address;

	// Initialize PointtoPoint helper
	//	
	PointToPointHelper p2p;
	p2p.SetDeviceAttribute("DataRate", StringValue(defaultDataRate));
	p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(delay)));

	// Initialize Csma helper
	//
	CsmaHelper csma;
	csma.SetChannelAttribute("DataRate", StringValue(defaultDataRate));
	csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(delay)));

	//=========== Connect edge switches to hosts ===========//
	//	
	NetDeviceContainer hostSw[num_pod][num_bridge];
	NetDeviceContainer bridgeDevices[num_pod][num_bridge];
	Ipv4InterfaceContainer ipContainer[num_pod][num_bridge];
	//**********************************Reset DataRate**************************************
	csma.SetChannelAttribute("DataRate", StringValue(EdgeHostDataRate));
	//**************************************************************************************
	for (i = 0; i<num_pod; i++) {
		for (j = 0; j<num_bridge; j++) {
			NetDeviceContainer link1 = csma.Install(NodeContainer(edge[i].Get(j), bridge[i].Get(j)));
			hostSw[i][j].Add(link1.Get(0));
			bridgeDevices[i][j].Add(link1.Get(1));

			for (h = 0; h< num_host; h++) {
				NetDeviceContainer link2 = csma.Install(NodeContainer(host[i][j].Get(h), bridge[i].Get(j)));
				hostSw[i][j].Add(link2.Get(0));
				bridgeDevices[i][j].Add(link2.Get(1));
			}

			BridgeHelper bHelper;
			bHelper.Install(bridge[i].Get(j), bridgeDevices[i][j]);
			//Assign address
			char *subnet;
			subnet = toString(10, i, j, 0);
			address.SetBase(subnet, "255.255.255.0");
			//*******DELETE ADDRESS***************
			delete subnet;
			//***********************************
			ipContainer[i][j] = address.Assign(hostSw[i][j]);
		}
	}
	std::cout << "Finished connecting edge switches and hosts  " << "\n";

	//=========== Connect aggregate switches to edge switches ===========//
	//
	NetDeviceContainer ae[num_pod][num_agg][num_edge];
	Ipv4InterfaceContainer ipAeContainer[num_pod][num_agg][num_edge];
	//**********************************Reset DataRate**************************************
	p2p.SetDeviceAttribute("DataRate", StringValue(AggEdgeDataRate));
	//**************************************************************************************
	for (i = 0; i<num_pod; i++) {
		for (j = 0; j<num_agg; j++) {
			for (h = 0; h<num_edge; h++) {
				ae[i][j][h] = p2p.Install(agg[i].Get(j), edge[i].Get(h));

				int second_octet = i;
				int third_octet = j + (k / 2);
				int fourth_octet;
				if (h == 0) fourth_octet = 1;
				else fourth_octet = h * 2 + 1;
				//Assign subnet
				char *subnet;
				subnet = toString(10, second_octet, third_octet, 0);
				//Assign base
				char *base;
				base = toString(0, 0, 0, fourth_octet);
				address.SetBase(subnet, "255.255.255.0", base);
				//*******DELETE ADDRESS***************
				delete subnet;
				delete base;
				//***********************************

				ipAeContainer[i][j][h] = address.Assign(ae[i][j][h]);
			}
		}
	}
	std::cout << "Finished connecting aggregation switches and edge switches  " << "\n";

	//=========== Connect core switches to aggregate switches ===========//
	//
	NetDeviceContainer ca[num_group][num_core][num_pod];
	Ipv4InterfaceContainer ipCaContainer[num_group][num_core][num_pod];
	int fourth_octet = 1;
	//**********************************Reset DataRate**************************************
	p2p.SetDeviceAttribute("DataRate", StringValue(coreAggDataRate));
	//**************************************************************************************
	for (i = 0; i<num_group; i++) {
		for (j = 0; j < num_core; j++) {
			fourth_octet = 1;
			for (h = 0; h < num_pod; h++) {
				ca[i][j][h] = p2p.Install(core[i].Get(j), agg[h].Get(i));

				int second_octet = k + i;
				int third_octet = j;
				//Assign subnet
				char *subnet;
				subnet = toString(10, second_octet, third_octet, 0);
				//Assign base
				char *base;
				base = toString(0, 0, 0, fourth_octet);
				address.SetBase(subnet, "255.255.255.0", base);
				//*******DELETE ADDRESS***************
				delete subnet;
				delete base;
				//***********************************

				ipCaContainer[i][j][h] = address.Assign(ca[i][j][h]);
				fourth_octet += 2;
			}
		}
	}
	std::cout << "Finished connecting core switches and aggregation switches  " << "\n";
	std::cout << "------------- " << "\n";


	//=========== Start the simulation ===========//
	//

	std::cout << "Start Simulation.. " << "\n";

	Ipv4GlobalRoutingHelper::PopulateRoutingTables();

	// Calculate Throughput using Flowmonitor
	//
	//  FlowMonitorHelper flowmon;
	//  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

	// Run simulation.
	//
	unsigned long simulate_start = getTickCount();
	NS_LOG_INFO("Run Simulation.");
	Simulator::Stop(Seconds(serverStopTime + 1));
	Packet::EnablePrinting();

	Simulator::Run();

	//  monitor->CheckForLostPackets ();
	//	monitor->SerializeToXmlFile(filename, true, true);

	std::cout << "Simulation finished " << "\n";

	//=========== Show  Switch Received Packet Num ===========//
	//

	std::cout << "=========== Show  Switch Received Packet Num ===========" << std::endl;
	// Show Core Switch 
	for (i = 0; i < num_group; i++)
	{
		for (j = 0; j < num_core; j++)
		{
			std::cout << "Core Switch [group,core] [" << i << "," << j << "]:" << std::endl;
			ShowPacketAndTuple(core[i].Get(j));
			ShowKeysight(core[i].Get(j));
		}
	}

	// Show Agg Switch
	for (i = 0; i < num_pod; i++)
	{
		for (j = 0; j < num_agg; j++)
		{
			std::cout << "Agg Switch [pod,agg] [" << i << "," << j << "]:" << std::endl;
			ShowPacketAndTuple(agg[i].Get(j));
			ShowKeysight(agg[i].Get(j));
		}
	}

	// Show Edge Switch
	for (i = 0; i < num_pod; i++)
	{
		for (j = 0; j < num_edge; j++)
		{
			std::cout << "Edge Switch [pod,edge] [" << i << "," << j << "]:" << std::endl;
			ShowPacketAndTuple(edge[i].Get(j));
			ShowKeysight(edge[i].Get(j));
		}
	}

	std::cout << "==========================================================" << std::endl;

	//=========== Show Host Received Packet Num ===========//
	//
	std::cout << "============= Show Host Received Packet Num ==============" << std::endl;
	for (i = 0; i < num_pod; i++)
	{
		for (j = 0; j < num_edge; j++)
		{
			for (k = 0; k < num_host; k++)
			{
				std::cout << "Host [pod,edge,host] [" << i << "," << j << "," << k << "]:" << std::endl;
				ShowPacketAndTuple(host[i][j].Get(k));
				ShowKeysight(host[i][j].Get(k));
			}
		}
	}
	std::cout << "==========================================================" << std::endl;

	Simulator::Destroy();
	NS_LOG_INFO("Done.");
	std::cout << "Pod num:" << pod << std::endl;
	ShowDifference();
	unsigned long end = getTickCount();
	std::cout << "Simulate Running time: " << end - simulate_start << "ms" << std::endl;
	std::cout << "Running time: " << end - start << "ms" << std::endl;
	std::cout << "Keysight Running Successfully!!!" << std::endl;
	return 0;
}





