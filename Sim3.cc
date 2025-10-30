#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/packet.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CongestionSimulationTCP");

// Stats
uint32_t packetsDropped = 0;
uint64_t totalBytesReceived = 0;

// Callbacks
void DropCallback(Ptr<const QueueDiscItem> item)
{
    packetsDropped++;
}

int main(int argc, char *argv[])
{
    // --- Simulation parameters ---
    std::string queueDiscType = "ns3::FqCoDelQueueDisc";
    uint32_t nSenders = 8;
    double simTime = 30.0;
    std::string bottleneckRate = "1Mbps";
    uint32_t queueSize = 100;

    CommandLine cmd;
    cmd.AddValue("queueDiscType", "Queue discipline type", queueDiscType);
    cmd.AddValue("nSenders", "Number of sender nodes", nSenders);
    cmd.AddValue("bottleneckRate", "Bottleneck link rate", bottleneckRate);
    cmd.AddValue("queueSize", "Queue size in packets", queueSize);
    cmd.Parse(argc, argv);

    // --- Create nodes ---
    NodeContainer senders;
    senders.Create(nSenders);
    NodeContainer switchNode;
    switchNode.Create(1);
    NodeContainer receiver;
    receiver.Create(1);

    // --- CSMA links: senders -> switch ---
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));

    NetDeviceContainer senderDevices;
    NetDeviceContainer switchCsmaDevices;
    
    for (uint32_t i = 0; i < nSenders; ++i)
    {
        NodeContainer link(senders.Get(i), switchNode.Get(0));
        NetDeviceContainer linkDevs = csma.Install(link);
        senderDevices.Add(linkDevs.Get(0));
        switchCsmaDevices.Add(linkDevs.Get(1));
    }

    // --- PointToPoint bottleneck: switch -> receiver ---
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(bottleneckRate));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer switchReceiverDevices = p2p.Install(switchNode.Get(0), receiver.Get(0));
    Ptr<NetDevice> switchBottleneckDev = switchReceiverDevices.Get(0);

    // --- Install Internet stack ---
    InternetStackHelper stack;
    
    // Configure TCP - use NewReno for better congestion control
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(1));
    
    stack.Install(senders);
    stack.Install(switchNode);
    stack.Install(receiver);

    // --- Assign IP addresses ---
    Ipv4AddressHelper address;
    
    for (uint32_t i = 0; i < nSenders; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        
        NetDeviceContainer linkDevices;
        linkDevices.Add(senderDevices.Get(i));
        linkDevices.Add(switchCsmaDevices.Get(i));
        address.Assign(linkDevices);
    }

    address.SetBase("10.1.100.0", "255.255.255.0");
    Ipv4InterfaceContainer switchReceiverInterfaces = address.Assign(switchReceiverDevices);

    // --- Set up routing ---
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // --- Traffic Control on bottleneck ---
    TrafficControlHelper tchUninstall;
    tchUninstall.Uninstall(switchBottleneckDev);
    
    TrafficControlHelper tch;
    tch.SetRootQueueDisc(queueDiscType);
    QueueDiscContainer qdiscs = tch.Install(switchBottleneckDev);
    
    // Set appropriate queue parameters based on queue disc type
    if (queueDiscType == "ns3::PfifoFastQueueDisc") {
        qdiscs.Get(0)->SetAttribute("MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)));
    } else if (queueDiscType == "ns3::RedQueueDisc") {
        qdiscs.Get(0)->SetAttribute("MinTh", DoubleValue(5));
        qdiscs.Get(0)->SetAttribute("MaxTh", DoubleValue(15));
        qdiscs.Get(0)->SetAttribute("MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)));
    } else if (queueDiscType == "ns3::CoDelQueueDisc") {
        qdiscs.Get(0)->SetAttribute("MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)));
    } else if (queueDiscType == "ns3::FqCoDelQueueDisc") {
        qdiscs.Get(0)->SetAttribute("MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)));
    } else if (queueDiscType == "ns3::PieQueueDisc") {
        qdiscs.Get(0)->SetAttribute("MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)));
    }
    
    qdiscs.Get(0)->TraceConnectWithoutContext("Drop", MakeCallback(&DropCallback));

    // --- TCP Server on receiver ---
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApps = sinkHelper.Install(receiver);
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simTime));

    // --- TCP Clients on senders ---
    Ipv4Address serverAddr = switchReceiverInterfaces.GetAddress(1);
    
    // Use random start times to avoid synchronization
    Ptr<UniformRandomVariable> startRng = CreateObject<UniformRandomVariable>();
    startRng->SetAttribute("Min", DoubleValue(1.0));
    startRng->SetAttribute("Max", DoubleValue(3.0));

    for (uint32_t i = 0; i < nSenders; ++i)
    {
        AddressValue remoteAddress(InetSocketAddress(serverAddr, port));
        BulkSendHelper source("ns3::TcpSocketFactory", remoteAddress.Get());
        source.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited transfer
        
        ApplicationContainer sourceApps = source.Install(senders.Get(i));
        double startTime = startRng->GetValue();
        sourceApps.Start(Seconds(startTime));
        sourceApps.Stop(Seconds(simTime - 1));
    }

    // --- Install Flow Monitor ---
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    NS_LOG_INFO("Starting simulation...");
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // --- Collect and print results ---
    monitor->CheckForLostPackets();
    
    // Get total bytes received from packet sink
    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps.Get(0));
    totalBytesReceived = sink1->GetTotalRx();

    // Flow monitor statistics
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    
    uint32_t totalPacketsSent = 0;
    uint32_t totalPacketsReceived = 0;
    uint32_t totalPacketsLost = 0;
    double totalDelay = 0.0;
    uint32_t flowCount = 0;
    
    std::vector<double> flowThroughputs;
    
    for (auto it = stats.begin(); it != stats.end(); ++it) 
    {
        totalPacketsSent += it->second.txPackets;
        totalPacketsReceived += it->second.rxPackets;
        totalPacketsLost += it->second.lostPackets;
        
        if (it->second.rxPackets > 0) {
            totalDelay += it->second.delaySum.GetSeconds();
            double throughput = it->second.rxBytes * 8.0 / (simTime * 1000000.0); // Mbps
            flowThroughputs.push_back(throughput);
            flowCount++;
        }
    }

    // Calculate fairness (Jain's fairness index)
    double sumThroughput = 0.0;
    double sumSquaredThroughput = 0.0;
    for (double throughput : flowThroughputs) {
        sumThroughput += throughput;
        sumSquaredThroughput += throughput * throughput;
    }
    double fairnessIndex = (sumThroughput * sumThroughput) / (flowCount * sumSquaredThroughput);

    // --- Print results ---
    std::cout << "==== TCP Simulation Results ====" << std::endl;
    std::cout << "Queue Discipline: " << queueDiscType << std::endl;
    std::cout << "Number of Senders: " << nSenders << std::endl;
    std::cout << "Bottleneck Rate: " << bottleneckRate << std::endl;
    std::cout << "Queue Size: " << queueSize << " packets" << std::endl;
    std::cout << "Total Bytes Received: " << totalBytesReceived << std::endl;
    std::cout << "Packets Sent:     " << totalPacketsSent << std::endl;
    std::cout << "Packets Received: " << totalPacketsReceived << std::endl;
    std::cout << "Packets Dropped:  " << packetsDropped << std::endl;
    std::cout << "Packets Lost:     " << totalPacketsLost << std::endl;
    
    double packetLossRate = (totalPacketsSent > 0) ? (100.0 * totalPacketsLost / totalPacketsSent) : 0;
    std::cout << "Packet Loss (%):  " << packetLossRate << "%" << std::endl;
    
    double goodput = (totalBytesReceived * 8.0) / (simTime * 1000000.0); // Mbps
    std::cout << "Goodput:          " << goodput << " Mbps" << std::endl;

    double avgDelay = (flowCount > 0) ? (totalDelay / flowCount * 1000) : 0; // ms
    std::cout << "Average Delay:    " << avgDelay << " ms" << std::endl;

    // Calculate utilization
    double theoreticalMax = (std::stod(bottleneckRate.substr(0, bottleneckRate.find("Mbps"))) * 1000000.0);
    double actualThroughput = (totalBytesReceived * 8.0) / simTime;
    double utilization = (actualThroughput / theoreticalMax) * 100.0;
    std::cout << "Link Utilization: " << utilization << "%" << std::endl;

    std::cout << "Fairness Index:   " << fairnessIndex << std::endl;
    std::cout << "Per-flow Throughputs: ";
    for (size_t i = 0; i < flowThroughputs.size(); ++i) {
        std::cout << flowThroughputs[i];
        if (i < flowThroughputs.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
    std::cout << "===============================" << std::endl;

    Simulator::Destroy();
    return 0;
}