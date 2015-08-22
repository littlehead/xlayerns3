/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
//#include "xlayer-modulel.h"
#include "types.h"
#include <algorithm>


int fact(int x);
QOS *add_to_list(float x, float y, float z, int b1, QOS **list);
void deallocmem(QOS *list);
QOS *optqosfrontier(QOS *QOS_lower, float State, int ActionB, float ThroughputWeight, float DelayWeight, float CostWeight);
QOS *calcphyqos(float PhyState, int ModLevel, float PktTime, int PktSize);
void calcqosphy(float PhyState, int ModLevel, float PktTime, int PktSize, QOS **QOSPhy);
RETLY3 *stval2(float next_s1, float next_s2, QOS *Z3, APPST curr_s3, float NEXT_VAL[][5][25], float ThroughputWeight, float DelayWeight, float CostWeight, ValueIterationParameters Param);
RETLY2 *stval1(float next_s1, float curr_s2, QOS *Z3, APPST curr_s3, float NEXT_VAL[][5][25], float ThroughputWeight, float DelayWeight, float CostWeight, ValueIterationParameters Param);
RETLY1 *stvalfunc(float curr_s1, float curr_s2, QOS *Z3, APPST curr_s3, float NEXT_VAL[][5][25], float ThroughputWeight, float DelayWeight, float CostWeight, ValueIterationParameters Param);

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

NS_LOG_COMPONENT_DEFINE ("WifiBroadcast");


class CrossLayer {
public:
  static float m_a1[10];
  static float m_s1[10];
  static int m_a2[2];
  static int m_a3[3];
  static int m_b1[4];
  static int m_b2;
  
  static float m_s2[5];
  ValueIterationParameters m_Param;
  float m_ThruWght;
  float m_DelWght;
  float m_CstWght;
  static APPST m_s3[25];
  float m_stateval[10][5][25];
  float m_opteps[10][5][25];
  float m_opttau[10][5][25];
  float m_optome[10][5][25];
  float m_opta1[10][5][25];
  int m_opta2[10][5][25];
  int m_opta3[10][5][25];
  int m_optb1[10][5][25];

  CrossLayer ();
  CrossLayer (float PacketTime, int PacketSize, float ThroughputWeight, float DelayWeight, float CostWeight);
  void Optimizer ();
  RETOPT *GetOpt (float PhyState, float MacState, APPST AppState);
};

float CrossLayer::m_a1[10];
float CrossLayer::m_s1[10];
int CrossLayer::m_a2[2];
int CrossLayer::m_a3[3];
int CrossLayer::m_b1[4];
int CrossLayer::m_b2;
float CrossLayer::m_s2[5];
APPST CrossLayer::m_s3[25];

CrossLayer::CrossLayer (float PacketTime, int PacketSize, float ThroughputWeight, float DelayWeight, float CostWeight)
{
    int i, j;
    
    
    for (i = 0; i < 10; i++)
    {
        m_a1[i] = 0.2 + 0.2 * i;
        m_s1[i] = 0.4 + 0.4 * i;
    }
    
    m_a2[0] = 0;
    m_a2[1] = 1;
    
    for (i = 0; i < 3; i++)
    {
        m_a3[i] = m_b1[i] = i + 1;
    }
    
    m_b1[3] = 4;
    m_b2 = 5;
    
    for (i = 0; i < 5; i++)
        m_s2[i] = 0.2 + 0.2 * i;
    
    m_Param.lamda1 = 1;
    m_Param.lamda2 = 1;
    m_Param.lamda3 = 1;
    m_Param.lamdag = 0.1;
    m_Param.gama = 0.9;
    m_Param.T = 0.02;
    m_Param.Tp = 0.0008;
    m_Param.PktSize = 4;
    m_Param.DopplerFreq = 50;
 
  m_ThruWght = ThroughputWeight;
  m_DelWght = DelayWeight;
  m_CstWght = CostWeight;
    for (i = 0; i < 5; i++)
    {
        for (j = 0; j < 5; j++)
        {
            m_s3[i * 5 + j].x = i;
            m_s3[i * 5 + j].y = j;
        }
    }

    Optimizer ();
}
    
void
CrossLayer::Optimizer ()
{
    int i, j, k, m;
    int x;
    int PktSize;
    float next_val[10][5][25];
    QOS *Z1 = NULL;
    QOS *Z3 = NULL;
    RETLY1 *stval_tmp;
    clock_t begin, end;
    double proc_time = 0.0;
    //double ave_proc_time;
    float diff, tmp_diff, ave_tmp_diff, act_diff;
    float thruwght, delwght, cstwght, Tp;
    
    thruwght = m_ThruWght;
    delwght = m_DelWght;
    cstwght = m_CstWght;
    PktSize = m_Param.PktSize;
    Tp = m_Param.Tp;
    
    x = 0;
    
    for (k = 0; k < 10; k++)
    {
        for (j = 0; j < 5; j++)
        {
            for (m = 0; m < 25; m++)
                next_val[k][j][m] = 0;
        }
    }
    
    diff = 10;
    
    while (diff > epsilon)
    {
    
        tmp_diff = 0;
        
        for (k = 0; k < 10; k++)        //iterate on s1
        {
            //QOS calculation
            for (i = 0; i < 4; i++)   //for each b1, calculate Z1(b1)
                calcqosphy(m_s1[k], m_b1[i], Tp, PktSize, &Z1);
            
            
            
            for (j = 0; j < 5; j++)      //iterate on s2
            {
               
                Z3 = optqosfrontier(Z1, m_s2[j], m_b2, thruwght, delwght, cstwght);
                
                
                for (m = 0; m < 25; m++)  //iterate on s3
                {
                    //value iteration
                    
                    begin = clock();
                    stval_tmp = stvalfunc(m_s1[k], m_s2[j], Z3, m_s3[m], next_val, thruwght, delwght, cstwght, m_Param);
                    end = clock();
                    proc_time += (double)(end - begin)/CLOCKS_PER_SEC;
                    m_stateval[k][j][m] = stval_tmp->x;
                    m_opteps[k][j][m] = stval_tmp->zx;
                    m_opttau[k][j][m] = stval_tmp->zy;
                    m_optome[k][j][m] = stval_tmp->zz;
                    m_opta1[k][j][m] = stval_tmp->a1;
                    m_opta2[k][j][m] = stval_tmp->a2;
                    m_opta3[k][j][m] = stval_tmp->a3;
                    m_optb1[k][j][m] = stval_tmp->b1;
                    act_diff = m_stateval[k][j][m] - next_val[k][j][m];
                    tmp_diff += fabsf (act_diff);
                    
                }
                deallocmem(Z3);
                Z3 = NULL;
            }
            
            deallocmem(Z1);
            Z1 = NULL;
        }
        
        for (k = 0; k < 10; k++)        //iterate on s1
        {
            for (j = 0; j < 5; j++)      //iterate on s2
            {
                for (m = 0; m < 25; m++)  //iterate on s3
                    next_val[k][j][m] = m_stateval[k][j][m];
            }
        }
        
        x++;
        std::cout << x << "th iteration." << std::endl;
        
        ave_tmp_diff = tmp_diff / 1250;
        
        //std::cout << "stval difference is " << ave_tmp_diff << "after this iteration." << std::endl;
        
        if(ave_tmp_diff < diff)
            diff = ave_tmp_diff;
    }
    
    //ave_proc_time = proc_time / (x * 10 * 5 * 25);
    //std::cout << "average processing time for each stval calc is " << ave_proc_time << "s" << std::endl;
        
}

RETOPT
*CrossLayer::GetOpt (float PhyState, float MacState, APPST AppState)
{
  RETOPT *optresults = new RETOPT;
  int r, s, t, k, j, m;

  r = s = t = k = j = m = 0;

  while (k < 10){
    if (PhyState == m_s1[k]){
      r = k;
      break;
    } k++;
  }

  while (j < 5){
    if (MacState == m_s2[j]){
      s = j;
      break;
    } j++;
  }

  while (m < 25){
    if ((AppState.x == m_s3[m].x) && (AppState.y == m_s3[m].y)){
      t = m;
      break;
    } m++;
  }

  optresults->value = m_stateval[r][s][t];
  optresults->extaction1 = m_opta1[r][s][t];
  optresults->extaction2 = m_opta2[r][s][t];
  optresults->extaction3 = m_opta3[r][s][t];
  optresults->intaction1 = m_optb1[r][s][t];
  optresults->qosx = m_opteps[r][s][t];
  optresults->qosy = m_opttau[r][s][t];
  optresults->qosz = m_optome[r][s][t];

  return optresults;
}



/*void CourseChange (std::string context, Ptr<const MobilityModel> model)
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
}*/

void VaryTxPwr (Ptr<Node> node)
{
  /*Ptr<NetDevice> dev = NodeList::GetNode (0)->GetDevice (1);
  Ptr<WifiNetDevice> wd = DynamicCast<WifiNetDevice> dev;
  Ptr<YansWifiPhy> yansphy = DynamicCast<YansWifiPhy> wd->GetPhy ();
  */

  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  Ptr<WifiNetDevice> wifiNetDev;
  wifiNetDev = DynamicCast<WifiNetDevice> (ipv4->GetNetDevice (2));
  Ptr<WifiPhy> wifiphy = wifiNetDev->GetPhy ();
  Ptr<YansWifiPhy> yansphy = DynamicCast<YansWifiPhy> (wifiphy);

  double CurrentMinPwr = yansphy->GetTxPowerStart ();
  double CurrentMaxPwr = yansphy->GetTxPowerEnd ();

  yansphy->SetTxPowerStart (CurrentMinPwr + 30);
  yansphy->SetTxPowerEnd (CurrentMaxPwr + 30);
}

void PrintRetryLimits (Ptr<NetDevice> apDev)
{

  Ptr<WifiNetDevice> apWifiDev = DynamicCast<WifiNetDevice> (apDev);
  Ptr<WifiRemoteStationManager> manager = apWifiDev->GetRemoteStationManager ();
  NS_ASSERT (manager != 0);
  std::cout << "Max retry limit at simulation time " << Simulator::Now () << " second is " << manager->GetMaxSlrc () << std::endl;

  manager->SetMaxSlrc (5);
  std::cout << "Max retry limit is " << manager->GetMaxSlrc () << " after change." << std::endl;

}

void ChangeDataRate (Ptr<Node> node, std::string dataRate)
{
  Ptr<Application> app = node->GetApplication (0);
  Ptr<OnOffApplication> onoff = DynamicCast<OnOffApplication> (app);
  DataRateValue rate;
  onoff->GetAttribute ("DataRate", rate);
  std::cout << "Application data rate was " << rate.Get () << std::endl;
  onoff->SetAttribute ("DataRate", StringValue (dataRate));
  onoff->GetAttribute ("DataRate", rate);
  std::cout << "Application data rate is now " << rate.Get () << std::endl;
}

/*Congestion Window callback for TCP

static void CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
}


static void RxDrop (Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
  file->Write (Simulator::Now (), p);
}*/

// Suggestion: 
// a) declare class instance of optimizer here. 
// b) write a callback function that is different than the ones you have, that
//    1) reads data
//    2) calls the optimizer
//    3) writes results
// c) schedule that callback at regular intervals. 

 CrossLayer cr(0.0008, 4, 0.33, 0.33, 0.33); // will run the optimizer.  



/**** When reading PhyState, it should be an SNR rage, not an exact value****/
void ConfigureParameters ()
{

/*************** Read state, (PhyState, MacState, AppState) ****************/

  float PhyState = 0.4;
  float MacState = 0.2;
  APPST AppState = {0,4};
  RETOPT *OptParams = cr.GetOpt (PhyState, MacState, AppState);

  std::cout << "Optimal external action 1 is " << OptParams->extaction1 << "\n";
  std::cout << "Optimal external action 2 is " << OptParams->extaction2 << "\n";
  std::cout << "Optimal external action 3 is " << OptParams->extaction3 << "\n";
  std::cout << "Optimal internal action 1 is " << OptParams->intaction1 << "\n";


/****************Set transmit power according to optimization***************
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  Ptr<WifiNetDevice> wifiNetDev;
  wifiNetDev = DynamicCast<WifiNetDevice> (ipv4->GetNetDevice (2));
  Ptr<WifiPhy> wifiphy = wifiNetDev->GetPhy ();
  Ptr<YansWifiPhy> yansphy = DynamicCast<YansWifiPhy> (wifiphy);

  double CurrentMinPwr = yansphy->GetTxPowerStart ();
  double CurrentMaxPwr = yansphy->GetTxPowerEnd ();

**extaction1 has to be adjusted correctly to real power level**
  yansphy->SetTxPowerStart (OptParams->extaction1);
  yansphy->SetTxPowerEnd (OptParams->extaction1);

*********Set application layer data rate according to optimization*********
  Ptr<Application> app = node->GetApplication (0);
  Ptr<OnOffApplication> onoff = DynamicCast<OnOffApplication> (app);
  DataRateValue rate;
  onoff->GetAttribute ("DataRate", rate);
  std::cout << "Application data rate was " << rate.Get () << std::endl;
**extaction3 has to be adjusted correctly to real data rate**
  onoff->SetAttribute ("DataRate", StringValue (OptParams->extaction3));
  onoff->GetAttribute ("DataRate", rate);
  std::cout << "Application data rate is now " << rate.Get () << std::endl;
*/
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

  YansWifiChannelHelper channel;
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  //channel.AddPropagationLoss ("ns3::NakagamiPropagationLossModel");
  channel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel",
                              "Exponent", DoubleValue (4),
                              "ReferenceDistance", DoubleValue (1.5),
                              "ReferenceLoss", DoubleValue (40));

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.Set ("ShortGuardEnabled", BooleanValue (false));
  phy.Set ("ChannelBonding", BooleanValue (false));
  phy.Set ("TxPowerStart", DoubleValue (10));
  phy.Set ("TxPowerEnd", DoubleValue (10));
  phy.SetChannel (channel.Create ());

  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);
  //wifi.SetStandard (WIFI_PHY_STANDARD_80211a);
  //wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate6_5MbpsBW20MHz"));
  //wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("OfdmRate6Mbps"));

  HtWifiMacHelper mac = HtWifiMacHelper::Default ();
  //NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();

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
                                 "GridWidth", UintegerValue (2),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (csmaNodes);
  
  uint32_t i;
  for (i = 0; i < nWifi; ++i) 
  {
    mobility.Install (wifiStaNodes.Get (i));
  } 
  
  //mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  //mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
  //                           "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  /*mobility.Install (wifiStaNodes.Get (nWifi - 1));

  Ptr<ConstantVelocityMobilityModel> mob = wifiStaNodes.Get (nWifi - 1)->GetObject<ConstantVelocityMobilityModel> ();
  mob->SetVelocity (Vector (10, 0, 0));
*/


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

/*  uint32_t nNodes = wifiStaNodes.GetN ();

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
  }*/

  uint16_t port = 9;

  //OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address ("255.255.255.255"), port)));
  OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address ("10.1.2.3"), port)));

  onoff.SetConstantRate (DataRate ("2048kb/s"), 576);

  ApplicationContainer app = onoff.Install (wifiApNode);
  app.Start (Seconds (1.0));
  app.Stop (Seconds (2.0));

  //PacketSinkHelper sink ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address::GetAny (), port)));

  //app = sink.Install (wifiStaNodes);
  //app.Start (Seconds (1.0));
  //app.Stop (Seconds (5.0));  
  
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (2.0));

  phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap ("wifibroadcast", apDevices.Get (0), true);
  phy.EnablePcap ("wifibroadcast", staDevices.Get (nWifi - 1));
  phy.EnablePcap ("wifibroadcast", staDevices.Get (0));
  csma.EnablePcap ("wifibroadcast", csmaDevices.Get (1), true);

  //AsciiTraceHelper asciiTraceHelper;
  //Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("wifibroadcast.cwnd");
  PcapHelper pcapHelper;
  Ptr<PcapFileWrapper> file = pcapHelper.CreateFile ("wifibroadcast.pcap", std::ios::out, PcapHelper::DLT_IEEE802_11);
  //staDevices.Get (nWifi - 1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDrop, file));

  std::ostringstream oss;
  oss << "/NodeList/" << wifiStaNodes.Get (nWifi - 1)->GetId () << "/$ns3::MobilityModel/CourseChange";

  uint32_t numNodes = NodeList::GetNNodes ();
  std::cout << "There are " << numNodes << " nodes in the system." << std::endl;  
  //Simulator::Schedule (Seconds (1.2), &PrintPosition);
  //Simulator::Schedule (Seconds (1.5), &VaryTxPwr, wifiApNode.Get (0));
  //Simulator::Schedule (Seconds (1.7), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode", StringValue ("OfdmRate13MbpsBW20MHz"));
  //Simulator::Schedule (Seconds (1.7), Config::Set, "NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/ControlMode", StringValue ("OfdmRate13MbpsBW20MHz"));
  //Simulator::Schedule (Seconds (1.5), &PrintRetryLimits, apDevices.Get (0));
  //Simulator::Schedule (Seconds (1.7), &ChangeDataRate, wifiApNode.Get (0), "4096kb/s");
 // Config::Connect (oss.str (), MakeCallback (&CourseChange));
  Simulator::Schedule (Seconds (1.5), &ConfigureParameters);
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
    if ((t.sourceAddress == "10.1.2.4" && t.destinationAddress == "10.1.2.3"))
    {
      std::cout << "Flow " << i->first << "(" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
      std::cout << "  TX Bytes: " << i->second.txBytes << std::endl;
      std::cout << "  RX Bytes: " << i->second.rxBytes << std::endl;
      
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024 << " Mbps" << std::endl;
    }
  }

  monitor->SerializeToXmlFile ("wifibroadcast.flowmon", true, true);
  Simulator::Destroy ();
  NS_LOG_INFO ("Simulation completed.");
  return 0;
}


int fact(int x)
{
    while (x != 0)
        return x * fact(x - 1);
    return 1;
}


// adding data to the end of list, returning list head
QOS *add_to_list(float x, float y, float z, int b1, QOS **list)
{
    QOS *listtemp;
    QOS *listtemp1;
    
    listtemp1 = new QOS;
    //printf("Creating list failed.\n");
    //return NULL;
    
    listtemp1->x = x;
    listtemp1->y = y;
    listtemp1->z = z;
    listtemp1->b1 = b1;
    listtemp1->next = NULL;
    
    
    if(!(*list))
    {
        //printf("adding first element...\n");
    
        *list = listtemp1;
    }
    else
    {
        //printf("adding next element...\n");
        listtemp = *list;
        while(listtemp->next != NULL)
            listtemp = listtemp->next;
        listtemp->next = listtemp1;
    }
    
    return *list;
}

void deallocmem(QOS *list)
{
    QOS *temp = list;
    QOS *node = NULL;
    while (temp != NULL)        //wipes off memory block
    {
        node = temp;
        temp = temp->next;
        delete node;
    }
    //list = NULL;                //clears pointer  why can't I combine these two into one function?
}                                 //list is now local variable, so this only changes the local one. In main, Z1 is
                                  //still not changed.

QOS *calcupperqos(QOS *QOS_lower, float State, int ActionB)
{
    QOS *QOS_up = NULL;
    QOS_up = new QOS;
    QOS_up->x = (float)pow((double)QOS_lower->x, (double)(ActionB + 1));
    QOS_up->y = (1 - (float)pow((double)QOS_lower->x, (double)ActionB)) * QOS_lower->y / ((1 - QOS_lower->x) * State);
    QOS_up->z = (1 - (float)pow((double)QOS_lower->x, (double)ActionB)) * QOS_lower->z / (1 - QOS_lower->x);
    QOS_up->b1 = QOS_lower->b1;
    QOS_up->next = NULL;
    
    return QOS_up;
}

QOS *calcphyqos(float PhyState, int ModLevel, float PktTime, int PktSize)
{
    QOS *QOS_phy = NULL;
    float BER;
    //float test;
    int mry;
    
    QOS_phy = new QOS;
    //test = sqrtf(PhyState) * sin(pi/powf((float)2, (float)ModLevel));
    mry = pow(2, ModLevel);
    BER = (1 / (float)ModLevel) * erfcf(sqrtf(PhyState) * sin(pi/powf((float)2, (float)mry)));
    QOS_phy->x = (float)(1 - powf((1 - BER), (float)PktSize));
    QOS_phy->y = (float)(PktTime / ModLevel);
    QOS_phy->z = powf((float)ModLevel/4, 1/3)/500;
    QOS_phy->b1 = ModLevel;
    QOS_phy->next = NULL;
   
    return QOS_phy;
}

QOS *optqosfrontier(QOS *QOS_lower, float State, int ActionB, float ThroughputWeight, float DelayWeight, float CostWeight)
{
    int flag = 0;
    QOS *QOS_upper = NULL;
    QOS *QOS_intermediate = NULL;
    QOS *QOS_traverse = NULL;
    //QOS *QOS_temp = NULL;
    //float thruwght, delwght, cstwght;
    
    //thruwght = ThroughputWeight;
    //delwght = DelayWeight;
    //cstwght = CostWeight;
    while(QOS_lower != NULL)
    {
        flag = 0;
        QOS_intermediate = calcupperqos(QOS_lower, State, ActionB);
        
        QOS_traverse = QOS_upper;
        //QOS_temp = QOS_upper;
        
        while(QOS_traverse != NULL)
        {
            /*if((fabsf(thruwght - delwght) + fabsf(cstwght - delwght)) > 0.01)
            {
                if(((QOS_intermediate->x - QOS_traverse->x) * thruwght / QOS_traverse->x + (QOS_intermediate->y - QOS_traverse->y) * delwght / QOS_traverse->y + (QOS_intermediate->z - QOS_traverse->z) * cstwght / QOS_traverse->z > 0) || ((QOS_intermediate->y / (1 - QOS_intermediate->x)) > 0.005))
                {
                    flag = 1;
                    break;
                }
            }
            else
            {*/
                if((QOS_traverse->x <= QOS_intermediate->x)&&(QOS_traverse->y <= QOS_intermediate->y)&&(QOS_traverse->z <= QOS_intermediate->z))
                {
                    flag = 1;
                    break;
                }
            //}
            QOS_traverse = QOS_traverse->next;
        }
        
        if(flag == 0)
        {
            add_to_list(QOS_intermediate->x, QOS_intermediate->y, QOS_intermediate->z, QOS_intermediate->b1, &QOS_upper);
            
        }
        
        QOS_lower = QOS_lower->next;
        free(QOS_intermediate);
    }
    
    return QOS_upper;
}



void calcqosphy(float PhyState, int ModLevel, float PktTime, int PktSize, QOS **QOSPhy)
{
    int flag = 0;
    QOS *QOS_traverse = NULL;
    QOS *QOS_intermediate = NULL;
    
    
    flag = 0;
    QOS_intermediate = calcphyqos(PhyState, ModLevel, PktTime, PktSize);
            
    QOS_traverse = *QOSPhy;
            
    while(QOS_traverse != NULL)
    {
        if((QOS_traverse->x <= QOS_intermediate->x)&&(QOS_traverse->y <= QOS_intermediate->y)&&(QOS_traverse->z <= QOS_intermediate->z))
        {
            flag = 1;
            break;
        }
        QOS_traverse = QOS_traverse->next;
    }
            
    if(flag == 0)
        add_to_list(QOS_intermediate->x, QOS_intermediate->y, QOS_intermediate->z, QOS_intermediate->b1, QOSPhy);
            
    free(QOS_intermediate);
    
}

float db2rt(float state)
{
    return (powf(10, (state / 10)));
}

float omega(float state, float ave_SINR)
{
    return (expf(-db2rt(state)/ave_SINR) - expf(-(db2rt(state + 0.4))/ave_SINR));
}

float xing(float state, float ave_SINR, float DopplerFreq)
{
    return (sqrtf(2 * pi * db2rt(state) / ave_SINR) * DopplerFreq * expf(-db2rt(state)/ave_SINR));
}

RETLY3 *stval2(float next_s1, float next_s2, QOS *Z3, APPST curr_s3, float NEXT_VAL[][5][25], float ThroughputWeight, float DelayWeight, float CostWeight, ValueIterationParameters Param)
{
    float sum_val12 = 0.0;
    float thrupt, delay;
    float Rin;
    float prob3;
    int n3, nLost, nSent, precalcnexts3x;
    //int totavailpkts;
    int i, j, k, m;
    APPST next_s3;
    RETLY3 *retstruct3 = new RETLY3;
    //float thruwght, delwght, cstwght;
    //float test;
    
    //thruwght = ThroughputWeight;
    //delwght = DelayWeight;
    //cstwght = CostWeight;
    
    Rin = 0.0;
    retstruct3->x = 0.0;
    
    j = 0;
    k = 0;
    
    //find next s1, s2
    while (fabs(CrossLayer::m_s1[j] - next_s1) > 0.05)
        j++;
    
    while (fabs(CrossLayer::m_s2[k] - next_s2) > 0.05)
        k++;
   
    while(Z3 != NULL)
    {
        n3 = (1 - Z3->x) * Param.T / Z3->y;
        nLost = curr_s3.x - n3;
        
	//modify Rin here
        thrupt = (n3 - Param.lamdag * std::max(nLost, 0));  //normalize throughput
        //totavailpkts = curr_s3.x + curr_s3.y;
        //thrupt = min(n3, totavailpkts);
        delay = Z3->y/(1-Z3->x);         //normalize delay for each packet
        //Rin = thruwght * thrupt + delwght * (1/delay);  //add delay to Rin
        Rin = thrupt + 0.0001 * (1/delay);
        //Rin = thrupt;
        
        for(i = 0; i < 3; i++)      //iterate on a3
        {
            sum_val12 = Rin - Param.lamda3 * CrossLayer::m_a3[i];
            nSent = n3 - curr_s3.x;
            precalcnexts3x = curr_s3.y - std::max(nSent, 0);
           
            if(precalcnexts3x < 0)
                next_s3.x = 0;
            else
                next_s3.x = precalcnexts3x;
            
            for(next_s3.y = 0; next_s3.y < 5; next_s3.y++)
            {
                m = 0;
                while ((CrossLayer::m_s3[m].x != next_s3.x)||(CrossLayer::m_s3[m].y != next_s3.y))
                {
                    m++;
                    if(m == 25)
                        break;
                }
        
                if (m == 25)
                    prob3 = 0;
                else
                    prob3 = (float)pow((double)CrossLayer::m_a3[i], (double)next_s3.y) * exp((double)(-CrossLayer::m_a3[i])) / fact(next_s3.y);
                
                //test = exp((double)(-CrossLayer::m_a3[i]));
                sum_val12 += Param.gama * prob3 * NEXT_VAL[j][k][m];
                
            }
            
            if (sum_val12 > retstruct3->x)
            {
                retstruct3->x = sum_val12;
                retstruct3->b1 = Z3->b1;
                retstruct3->a3 = CrossLayer::m_a3[i];
                retstruct3->zx = Z3->x;
                retstruct3->zy = Z3->y;
                retstruct3->zz = Z3->z;
            }
        }
        Z3 = Z3->next;
        
    }
    
    return retstruct3;
    
}

RETLY2 *stval1(float next_s1, float curr_s2, QOS *Z3, APPST curr_s3, float NEXT_VAL[][5][25], float ThroughputWeight, float DelayWeight, float CostWeight, ValueIterationParameters Param)
{
    int i, j;
    float sum_val1 = 0.0;
    float prob2 = 1;        //CDMA
    RETLY2 *retstruct2 = new RETLY2;
    RETLY3 *val12 = new RETLY3;
    float thruwght, delwght, cstwght;
    
    thruwght = ThroughputWeight;
    delwght = DelayWeight;
    cstwght = CostWeight;
    
    retstruct2->x = 0.0;
    
    for(i = 0; i < 2; i++)   //iterate on a2
    {
        sum_val1 = - Param.lamda2 * CrossLayer::m_a2[i];
        for(j = 0; j < 5; j++)  //for all next_s2, add value together
        {
            val12 = stval2(next_s1, CrossLayer::m_s2[j], Z3, curr_s3, NEXT_VAL, thruwght, delwght, cstwght, Param);
            
            /*if(i == 0)
            {
                if(s2[j] >= curr_s2)
                {
                    if(curr_s2 > 0.85)
                        prob2 = 0.1 / 2;
                    else
                        prob2 = 0.1 * 0.2 / (2 *(1 - curr_s2));
                }
                else
                    prob2 = 0.9 * 0.2 / (2 * (curr_s2 - 0.2));
                
            }
            else
            {
                if(s2[j] >= curr_s2)
                {
                    if(curr_s2 > 0.85)
                        prob2 = 0.9 / 2;
                    else
                        prob2 = 0.9 * 0.2 / (2 *(1 - curr_s2));
                }
                else
                    prob2 = 0.1 * 0.2 / (2 * (curr_s2 - 0.2));
            }*/
            prob2 = 0.2;
            
            sum_val1 += prob2 * val12->x;
        }
        retstruct2->a3 = val12->a3;
        retstruct2->b1 = val12->b1;
        retstruct2->zx = val12->zx;
        retstruct2->zy = val12->zy;
        retstruct2->zz = val12->zz;
        
        if(sum_val1 > retstruct2->x)
        {
            retstruct2->x = sum_val1;
            retstruct2->a2 = CrossLayer::m_a2[i];
        }
        
    }
    
    delete val12;
    return retstruct2;
}

RETLY1 *stvalfunc(float curr_s1, float curr_s2, QOS *Z3, APPST curr_s3, float NEXT_VAL[][5][25], float ThroughputWeight, float DelayWeight, float CostWeight, ValueIterationParameters Param)
{
    int i, j, k, num;
    float sum_valcurr = 0.0;
    float tot_prob, ave_snr;
    float *prob1, *valx;
    float stateL;
    float *next_s1;
    RETLY1 *val_curr = new RETLY1;
    RETLY2 *val1 = new RETLY2;
    float thruwght, delwght, cstwght;
    
    thruwght = ThroughputWeight;
    delwght = DelayWeight;
    cstwght = CostWeight;
    
    val_curr->x = 0.0;
    
    if ((curr_s1 > 0.5) && (curr_s1) < 3.9)
    {
        num = 3;
        next_s1 = new float[3];
        next_s1[0] = curr_s1 - 0.4;
        next_s1[1] = curr_s1;
        next_s1[2] = curr_s1 + 0.4;
    }
    else if (curr_s1 < 0.5)
    {
        num = 2;
        next_s1 = new float[2];
        next_s1[0] = curr_s1;
        next_s1[1] = curr_s1 + 0.4;
        
    }
    else
    {
        num = 2;
        next_s1 = new float[2];
        next_s1[0] = curr_s1 - 0.4;
        next_s1[1] = curr_s1;
    }

    for(i = 0; i < 10; i++)    //iterate on a1
    {
        sum_valcurr = - Param.lamda1 * CrossLayer::m_a1[i];
        ave_snr = CrossLayer::m_a1[i] / 0.46;
        
        prob1 = new float[num];
        valx = new float[num];
        tot_prob = 0;
        
        for(j = 0; j < num; j++)
        {   //calculate p(s1'|s1, a1)
            if(next_s1[j] == curr_s1)
            {
                if(next_s1[j] < 0.5)
                    prob1[j] = 1 - (Param.Tp / omega(curr_s1, ave_snr)) * xing(0.4, ave_snr, Param.DopplerFreq);
                else if(next_s1[j] > 3.9)
                    prob1[j] = 1 - (Param.Tp / omega(curr_s1, ave_snr)) * xing(3.2, ave_snr, Param.DopplerFreq);
                else
                    prob1[j] = 1 - (Param.Tp / omega(curr_s1, ave_snr)) * (xing((next_s1[j] + 0.4), ave_snr, Param.DopplerFreq) - xing(next_s1[j], ave_snr, Param.DopplerFreq));
            }
            else
            {
                stateL = std::max(curr_s1, next_s1[j]);
                prob1[j] = (Param.Tp / omega(curr_s1, ave_snr)) * xing(stateL, ave_snr, Param.DopplerFreq);
            }
            
            if(prob1[j] < 0)
                prob1[j] = 0;
            else
                tot_prob += prob1[j];
         
            //adding values for all next_s1
            val1 = stval1(next_s1[j], curr_s2, Z3, curr_s3, NEXT_VAL, thruwght, delwght, cstwght, Param);
            valx[j] = val1->x;
            
        }
        
        for (k = 0; k < num; k++)
        {
            prob1[k] = prob1[k] / tot_prob;
            sum_valcurr += prob1[k] * valx[k];
            
        }
        
        val_curr->b1 = val1->b1;
        val_curr->a2 = val1->a2;
        val_curr->a3 = val1->a3;
        val_curr->zx = val1->zx;
        val_curr->zy = val1->zy;
        val_curr->zz = val1->zz;
        
        if(sum_valcurr > val_curr->x)
        {
            val_curr->x = sum_valcurr;
            val_curr->a1 = CrossLayer::m_a1[i];
        }
        
    }
    
    delete val1;
    delete next_s1;
    return val_curr;
}

