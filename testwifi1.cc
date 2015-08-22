#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

// Default Network Topology
//
//   Wifi 10.1.2.0
//                 AP
//  *    *    *    *
//  |    |    |    |    10.1.1.0
// n5   n6   n7   n0 -------------- n1
//                         LAN       |
//                                  Server


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleWifi");

void CourseChange (std::string context, Ptr<const MobilityModel> model)
{
  Vector position = model->GetPosition ();
  NS_LOG_UNCOND (Simulator::Now () << context << " x = " << position.x << ", y = " << position.y);
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

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
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
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

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

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (wifiStaNodes.Get (nWifi - 1));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);
  
  uint32_t i;
  for (i = 0; i < nWifi -1; ++i) 
  {
    mobility.Install (wifiStaNodes.Get (i));
  } 


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

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (wifiApNode);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (5.0));

  UdpEchoClientHelper echoClient (csmaInterfaces.GetAddress (0, 0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = 
    echoClient.Install (wifiStaNodes.Get (nWifi - 1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (5.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (5.0));

  phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap ("simplewifi", apDevices.Get (0));
  phy.EnablePcap ("simplewifi", staDevices.Get (nWifi - 1));
  csma.EnablePcap ("simplewifi", csmaDevices.Get (1), true);

  std::ostringstream oss;
  oss << "/NodeList/" << wifiStaNodes.Get (nWifi - 1)->GetId () << "/$ns3::MobilityModel/CourseChange";

  Config::Connect (oss.str (), MakeCallback (&CourseChange));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
