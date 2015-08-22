#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <random>

#define PSIZE 8192
#define PKTSIZE 30

using namespace ns3;

char ring[PSIZE];
int begin = 0;
int end = 0;
//char buffer[PKTSIZE + 1];
int pktNum = 0;
int TotApTxPkts;
int TotStaTxPkts;
int TotRxPkts;


void gen_random (char *s, const int len)
{
  static const char alphanum[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";
  int i;

  for (i = 0; i < len; i++)
    s[i] = alphanum[rand () % (sizeof(alphanum) - 1)];
  s[len] = 0;
}

int ringempty() {return begin == end;}
int ringfull() {return (end + 1) % PSIZE == begin;}
int ringused() {return (end - begin + PSIZE) % PSIZE;}
int ringavail() {return PSIZE - 1 - ringused();}

void ringenqueue (char c)
{
  if (!ringfull())
  {
    ring[end] = c;
    end = (end + 1) % PSIZE;
  }
}

char ringdequeue ()
{
  char out;
 
  if (!ringempty())
  {
    out = ring[begin];
    begin = (begin + 1) % PSIZE;
    return out;
  }
  else
  {
    return '\0';
  }
}

bool bufread (char *buffer, int size)
{
  //std::cout << "ring size is " << ringused() << " when reading.\n";

  if (ringused() >= size)
  {
    while (!ringempty() && size)
    {
      *(buffer++) = ringdequeue();
      size--;
    }
    //std::cout << "ring size is " << ringused() << " after reading.\n";
    return true;
  }
  else
    return false;
}

void bufwrite (char *buffer, int size)
{
  if (ringavail() >= size)
  {
    while (!ringfull() && size)
    {
      ringenqueue(*buffer++);
      size--;
    }
  }
  //std::cout << "ring size is " << ringused() << "\n";
}


class MyApp : public Application
{
public:

  MyApp ();
  virtual ~MyApp ();
  void Setup (Ptr<Socket> socket, Address address);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void StoreData (void);
  void ScheduleStore (void);
  void SendData (void);
  void ScheduleSend (void);

  Ptr<Socket>  			 m_socket;
  Address      			 m_peer;
  Ptr<ParetoRandomVariable>      m_dataLength;
  EventId			 m_storeEvent;
  Ptr<ExponentialRandomVariable> m_dataArrivalRate;
  EventId       		 m_sendEvent;
  bool          		 m_running;
};

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    //m_dataLength (CreateObject<ParetoRandomVariable> ()),
    m_storeEvent (),
    //m_dataArrivalRate (CreateObject<ExponentialRandomVariable> ()),
    m_sendEvent (),
    m_running (false)
{ 
  double meanLength = 50;
  double meanRate = 0.2;
  m_dataLength = CreateObject<ParetoRandomVariable> ();
  m_dataLength->SetAttribute ("Mean", DoubleValue (meanLength));
  m_dataArrivalRate = CreateObject<ExponentialRandomVariable> ();
  m_dataArrivalRate->SetAttribute ("Mean", DoubleValue (meanRate));
}

MyApp::~MyApp()
{
  m_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address)
{
  m_socket = socket;
  m_peer = address;
  //m_nBytes = nBytes;
}

void
MyApp::StartApplication (void)
{
  TotApTxPkts = 0;
  TotStaTxPkts = 0;
  TotRxPkts = 0;
  m_running = true;
  //m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  StoreData ();
  SendData ();
}

void
MyApp::StopApplication (void)
{
    m_running = false;
  
  if (m_storeEvent.IsRunning ())
    Simulator::Cancel (m_storeEvent);
  if (m_sendEvent.IsRunning ())
    Simulator::Cancel (m_sendEvent);

  if (m_socket)
    m_socket->Close ();
}

void MyApp::StoreData (void)
{
  uint32_t nBytes = m_dataLength->GetInteger();
  char *str;
  str = new char[nBytes + 1];
  gen_random (str, nBytes);
  bufwrite (str, nBytes);
  delete [] str;
  std::cout << "Stored " << nBytes << " bytes\n";
  ScheduleStore ();
}

void MyApp::ScheduleStore (void)
{
  if (m_running)
  {
    Time tNextStore;
    tNextStore = Seconds (m_dataArrivalRate->GetValue ());
    //uint32_t nBytes = m_dataLength->GetInteger();
    m_storeEvent = Simulator::Schedule (tNextStore, &MyApp::StoreData, this);
  }
}

void MyApp::SendData (void)
{
  char *buffer;
  bool isRead;
  buffer = new char[PKTSIZE + 1];
  isRead = bufread (buffer, PKTSIZE);
  if (isRead)
  {
    Ptr<Packet> packet = Create<Packet> (reinterpret_cast<uint8_t*> (buffer), PKTSIZE);
    m_socket->Send (packet);
    pktNum++;
  }
  delete [] buffer;
  std::cout << "Sent " << pktNum << " packets.\n";
  ScheduleSend ();
}

void MyApp::ScheduleSend (void)
{
  if (m_running)
  {
    //Time tNextSend;
    //tNextSend = Seconds (m_dataSendRate->GetValue ());
    m_sendEvent = Simulator::Schedule (Seconds (0.2), &MyApp::SendData, this);
  }
}

static void PhyRxTrace (std::string path, Ptr<const Packet> packet, uint16_t channelFreqMhz, uint16_t channelNumber, uint32_t rate, bool isShortPreamble, double signalDbm, double noiseDbm)
{
  Ptr<Packet> currentPacket;
  WifiMacHeader hdr;
  currentPacket = packet->Copy ();
  //TotRxBytes += currentPacket->GetSize ();

  currentPacket->RemoveHeader (hdr);
 
  //char nodeNr = path.at(10);
  if (hdr.IsData ())
     TotRxPkts++;
  
  //PhyState = signalDbm - noiseDbm;
  //NS_LOG_UNCOND ("A packet has been received with signal " << signalDbm << " and noise " << noiseDbm << "\n");
  
  //NS_LOG_UNCOND ("Number of bytes received now is " << TotRxBytes << "\n");
  //NS_LOG_UNCOND ("A packet has been received with signal " << signalDbm << " and noise " << noiseDbm << "\n");
}

static void PhyTxTrace (std::string path, Ptr<const Packet> packet, uint16_t channelFreqMhz, uint16_t channelNumber, uint32_t rate, bool isShortPreamble, uint8_t txPower)
{
  //int psize;
  Ptr<Packet> currentPacket;
  WifiMacHeader hdr;
  currentPacket = packet->Copy ();
  //psize = currentPacket->GetSize ();
  //TotTxBytes += psize;

  currentPacket->RemoveHeader (hdr);
  char nodeNr = path.at(10);

  if (hdr.IsData ())
  {
    if (nodeNr == '0')
      TotApTxPkts++;
    else
  //if (psize == 20)
      TotStaTxPkts++;
  }
     
	//channelOccuPercentage = (double) (TotTxBytes * 8) / (double) (6000000 * 10);
	//std::cout << "Channel Occupency is " << channelOccuPercentage * 100 << "%\n";

  //NS_LOG_UNCOND ("A packet has been sent.\n");
  //NS_LOG_UNCOND ("Number of bytes sent now is " << TotTxBytes << "\n");
}

/*void VaryTxPwr (Ptr<Node> node)
{
  //Ptr<NetDevice> dev = NodeList::GetNode (0)->GetDevice (1);
  //Ptr<WifiNetDevice> wd = DynamicCast<WifiNetDevice> dev;
  //Ptr<YansWifiPhy> yansphy = DynamicCast<YansWifiPhy> wd->GetPhy ();
  

  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  Ptr<WifiNetDevice> wifiNetDev;
  wifiNetDev = DynamicCast<WifiNetDevice> (ipv4->GetNetDevice (1));
  Ptr<WifiPhy> wifiphy = wifiNetDev->GetPhy ();
  Ptr<YansWifiPhy> yansphy = DynamicCast<YansWifiPhy> (wifiphy);

  double CurrentMinPwr = yansphy->GetTxPowerStart ();
  double CurrentMaxPwr = yansphy->GetTxPowerEnd ();

  yansphy->SetTxPowerStart (CurrentMinPwr + 50);
  yansphy->SetTxPowerEnd (CurrentMaxPwr + 50);
}*/



int main (int argc, char * argv[])
{
	bool verbose = true;
	uint32_t nWifi = 5;
	NodeContainer Apnode;
	NodeContainer Stanodes;
	Apnode.Create (1);
	Stanodes.Create (nWifi);

	//YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
	YansWifiChannelHelper channel;
	channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
	channel.AddPropagationLoss ("ns3::NakagamiPropagationLossModel",
				    "m0", DoubleValue (1.0),
				    "m1", DoubleValue (1.0),
				    "m2", DoubleValue (1.0));
	YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
	phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
	phy.Set ("TxPowerStart", DoubleValue (40.0));
	phy.Set ("TxPowerEnd", DoubleValue (40.0));
	//phy.Set ("EnergyDetectionThreshold", DoubleValue (-70.0));
	phy.SetChannel (channel.Create ());

	WifiHelper wifi = WifiHelper::Default ();
	wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager");

	NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();

	Ssid ssid = Ssid ("ns-3-testwifi");
	mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));

	NetDeviceContainer staDevices;
	staDevices = wifi.Install (phy, mac, Stanodes);

	mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));

	NetDeviceContainer apDevice;
	apDevice = wifi.Install (phy, mac, Apnode);

	MobilityHelper mobility;

	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
	positionAlloc->Add (Vector (0.0, 0.0, 0.0));
	positionAlloc->Add (Vector (-60.0, 40.0, 0.0));
	positionAlloc->Add (Vector (-30.0, 40.0, 0.0));
	positionAlloc->Add (Vector (0.0, 40.0, 0.0));
	positionAlloc->Add (Vector (10.0, 40.0, 0.0));
	positionAlloc->Add (Vector (20.0, 10.0, 0.0));
	mobility.SetPositionAllocator (positionAlloc);

	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

	mobility.Install (Apnode);
	mobility.Install (Stanodes);
	/*mobility.SetPositionAllocator ("ns3::GridPositionAllocator", "MinX", DoubleValue (0.0), "MinY", DoubleValue (0.0), "DeltaX", DoubleValue (10.0), "DeltaY", DoubleValue (20.0), "GridWidth", UintegerValue (3), "LayoutType", StringValue ("RowFirst"));

	mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue (Rectangle (-100, 100, -100, 100)));
	mobility.Install (Stanodes);

	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (Apnode);*/

	InternetStackHelper stack;
	stack.Install (Stanodes);
	stack.Install (Apnode);

	Ipv4AddressHelper address;

	address.SetBase ("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer wifiStaInterfaces;
	Ipv4InterfaceContainer wifiApInterface;
	wifiStaInterfaces = address.Assign (staDevices);
	wifiApInterface = address.Assign (apDevice);
         
	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

	uint16_t sinkPort = 8080;
	Address sinkAddress (InetSocketAddress (wifiStaInterfaces.GetAddress (nWifi - 1), sinkPort));
	PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
	ApplicationContainer sinkApps = packetSinkHelper.Install (Stanodes.Get(nWifi - 1));
	sinkApps.Start (Seconds (0.0));
	sinkApps.Stop (Seconds (20.0));


	Ptr<Socket> ns3TcpSocket1 = Socket::CreateSocket (Apnode.Get (0), TcpSocketFactory::GetTypeId ());
	Ptr<MyApp> app1 = CreateObject<MyApp> ();
	app1->Setup (ns3TcpSocket1, sinkAddress);
	Apnode.Get (0)->AddApplication (app1);
	app1->SetStartTime (Seconds (1.0));
	app1->SetStopTime (Seconds (20.0));

	/*Ptr<Socket> ns3TcpSocket2 = Socket::CreateSocket (Stanodes.Get (0), TcpSocketFactory::GetTypeId ());
	Ptr<MyApp> app2 = CreateObject<MyApp> ();
	app2->Setup (ns3TcpSocket2, sinkAddress);
	Stanodes.Get (0)->AddApplication (app2);
	app2->SetStartTime (Seconds (1.0));
	app2->SetStopTime (Seconds (20.0));*/

	Simulator::Stop (Seconds (20.0));
	//Simulator::Schedule (Seconds (10.5), &VaryTxPwr, Apnode.Get (0));

	phy.EnablePcap ("AppQ", apDevice.Get (0), true);
	phy.EnablePcap ("AppQ", staDevices.Get (nWifi - 1), true);
	Config::Connect ("/NodeList/5/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx", MakeCallback (&PhyRxTrace));
        Config::Connect ("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferTx", MakeCallback (&PhyTxTrace));
	Config::Connect ("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferTx", MakeCallback (&PhyTxTrace));


	Simulator::Run ();
	Simulator::Destroy ();

	NS_LOG_UNCOND("Total transmitted packets from AP is " << TotApTxPkts << "\n");
	NS_LOG_UNCOND("Total transmitted packets from Station is " << TotStaTxPkts << "\n");
	NS_LOG_UNCOND("Total received number of packets is " << TotRxPkts << "\n");

	return 0;


}
