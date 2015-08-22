/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

// Default Network Topology
//
//   Wifi 10.1.2.0
//                 AP
//  *    *    *    *
//  |    |    |    |    10.1.1.0
// n2   n3   n4   n0 -------------- n1
//                         LAN       |
//                                  Server


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiThroughput");

void CourseChange (std::string context, Ptr<const MobilityModel> model)
{
  Vector position = model->GetPosition ();
  NS_LOG_UNCOND (Simulator::Now () << context << " x = " << position.x << ", y = " << position.y);
}

void PrintPosition ()
{
  for (uint32_t i = 0; i < NodeList::GetNNodes (); ++i)
  {
    Ptr<MobilityModel> mob = NodeList::GetNode (i)->GetObject<MobilityModel> ();
    Vector Pos = mob->GetPosition ();
    std::cout << "n" << i << "'s position is: x = " << Pos.x << ", y = " << Pos.y << std::endl;
  }
}

static void RxDrop (Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
  file->Write (Simulator::Now (), p);
}

int 
main (int argc, char *argv[])
{
  bool verbose = true;
  uint32_t nWifi = 3;

  CommandLine cmd;
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);

  cmd.Parse (argc,argv);

  if (nWifi > 18)
    {
      std::cout << "Number of wifi nodes " << nWifi << 
                   " specified exceeds the mobility bounding box" << std::endl;
      exit (1);
    }

  NodeContainer csmaNodes;
  csmaNodes.Create (2);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);
  NodeContainer wifiApNode = csmaNodes.Get (0);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  //channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  //channel.AddPropagationLoss ("ns3::NakagamiPropagationLossModel");
  //channel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel",
    //                          "Exponent", DoubleValue (4),
    //                          "ReferenceDistance", DoubleValue (1.5),
    //                          "ReferenceLoss", DoubleValue (40));

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.Set ("TxPowerStart", DoubleValue (30));
  phy.Set ("TxPowerEnd", DoubleValue (30));
  phy.Set ("EnergyDetectionThreshold", DoubleValue (-65.0));

  phy.SetChannel (channel.Create ());

  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate6Mbps"));

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();

  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (csmaNodes);
  
  uint32_t i;
  for (i = 0; i < nWifi -1; ++i) 
  {
    mobility.Install (wifiStaNodes.Get (i));
  } 
  
  //mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (wifiStaNodes.Get (nWifi - 1));

  //Ptr<ConstantVelocityMobilityModel> mob = wifiStaNodes.Get (nWifi - 1)->GetObject<ConstantVelocityMobilityModel> ();
  //mob->SetVelocity (Vector (10, 0, 0));



  InternetStackHelper stack;
  stack.Install (csmaNodes);
 // stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer wifiStaInterfaces;
  wifiStaInterfaces = address.Assign (staDevices);
  address.Assign (apDevices);

  Ptr<Ipv4> ipv4 = NULL;
  Ipv4InterfaceAddress iaddr;
  Ipv4Address addri;

  //ipv4 = csmaNodes.Get (0)->GetObject<Ipv4> ();
  //iaddr = ipv4->GetAddress (1 , 0);
  //addri = iaddr.GetLocal ();
  //std::cout << "IP address of server is " << addri << std::endl;

  uint32_t nNodes = wifiStaNodes.GetN ();

  for (i = 0; i < nNodes; ++i)
  {
    ipv4 = wifiStaNodes.Get (i)->GetObject<Ipv4> ();
    iaddr = ipv4->GetAddress (1 , 0);
    addri = iaddr.GetLocal ();
    std::cout << "IP address of " << i << "th station node is " << addri << std::endl;
  }

  nNodes = csmaNodes.GetN ();
  for (i = 0; i < nNodes; ++i)
  {
    ipv4 = csmaNodes.Get (i)->GetObject<Ipv4> ();
    iaddr = ipv4->GetAddress (1, 0);
    addri = iaddr.GetLocal ();
    std::cout << "IP address of " << i << "th CSMA node is " << addri << std::endl;
  }

  uint16_t port1 = 9;
  uint16_t port2 = 10;

  //flowmonitor cannot track broadcast traffic
  //OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address ("255.255.255.255"), port)));
  OnOffHelper onoffapp1 ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address ("10.1.2.3"), port1)));
  OnOffHelper onoffapp2 ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address ("10.1.2.4"), port2)));

  onoffapp1.SetConstantRate (DataRate ("2048kb/s"), 576);
  onoffapp2.SetConstantRate (DataRate ("2048kb/s"), 576);

  ApplicationContainer serverapps, clientapps;

  serverapps.Add (onoffapp1.Install (wifiApNode));
  serverapps.Add (onoffapp2.Install (wifiStaNodes.Get (1)));
  serverapps.Start (Seconds (1.0));
  serverapps.Stop (Seconds (5.0));

  PacketSinkHelper sink1 ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address::GetAny (), port1)));
  PacketSinkHelper sink2 ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address::GetAny (), port2)));

  clientapps.Add (sink1.Install (wifiStaNodes.Get (nWifi - 1)));
  clientapps.Add (sink2.Install (wifiApNode));
  clientapps.Start (Seconds (1.0));
  clientapps.Stop (Seconds (5.0));  
  
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (5.0));

  phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap ("wifithroughput", apDevices.Get (0), true);
  phy.EnablePcap ("wifithroughput", staDevices.Get (nWifi - 1));
  phy.EnablePcap ("wifithroughput", staDevices.Get (0));
  csma.EnablePcap ("wifithroughput", csmaDevices.Get (1), true);

  //AsciiTraceHelper asciiTraceHelper;
  //Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("wifibroadcast.cwnd");
  PcapHelper pcapHelper;
  Ptr<PcapFileWrapper> file = pcapHelper.CreateFile ("wifithroughput.pcap", std::ios::out, PcapHelper::DLT_IEEE802_11);
  staDevices.Get (nWifi - 1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDrop, file));

  std::ostringstream oss;
  oss << "/NodeList/" << wifiStaNodes.Get (nWifi - 1)->GetId () << "/$ns3::MobilityModel/CourseChange";

  /*uint32_t numNodes = NodeList::GetNNodes ();
  std::cout << "There are " << numNodes << " nodes in the system." << std::endl;  
  Simulator::Schedule (Seconds (1), &PrintPosition);
  Simulator::Schedule (Seconds (5), &PrintPosition);
  Simulator::Schedule (Seconds (9), &PrintPosition);
*/

  Config::Connect (oss.str (), MakeCallback (&CourseChange));

  //Calculate throughput
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  //Start simulation
  NS_LOG_INFO ("Running Simulation...");
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());

  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    
    /*if ((t.sourceAddress == "10.1.2.4" && t.destinationAddress == "10.1.2.3"))
    {*/
      std::cout << "Flow " << i->first << "(" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
      std::cout << "  TX Bytes: " << i->second.txBytes << std::endl;
      std::cout << "  RX Bytes: " << i->second.rxBytes << std::endl;
      std::cout << "  Packet loss rate is " << double (i->second.txBytes - i->second.rxBytes) / double (i->second.txBytes) * 100 << "%" << std::endl;      
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024 << " Mbps" << std::endl;
      std::cout << "  Average packet delay is " << double (i->second.delaySum.GetSeconds ()) / double (i->second.rxPackets) << std::endl;

    //}
  }

  //monitor->SerializeToXmlFile ("wifibroadcast.flowmon", true, true);
  Simulator::Destroy ();
  NS_LOG_INFO ("Simulation completed.");
  return 0;
}
