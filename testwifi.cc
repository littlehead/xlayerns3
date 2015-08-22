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
#include <pthread.h>

#define PSIZE 8192
#define PKTSIZE 20

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TestWifi");
  

//These numbers need to be reset every so often...

int AppNum = 0;
int TotRxBytes = 0;
int TotTxBytes = 0;
int TotRxPkts = 0;
int TotTxPkts = 0;
double PhyState;
double MacState;
char ring[PSIZE];
int begin = 0;
int end = 0;
char buffer[PKTSIZE + 1];

class Mutex
{
  private:
      pthread_mutex_t mutex;
  public:
      Mutex ();
      ~Mutex ();
      void lock ();
      void unlock ();
      bool trylock ();
};

Mutex::Mutex ()
{
  pthread_mutex_init (&mutex, 0);
}

Mutex::~Mutex ()
{
  pthread_mutex_destroy (&mutex);
}

void Mutex::lock ()
{
  pthread_mutex_lock (&mutex);
}

void Mutex::unlock ()
{
  pthread_mutex_unlock (&mutex);
}

bool Mutex::trylock ()
{
  return (pthread_mutex_trylock (&mutex) == 0);
}

Mutex mtx;

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
  mtx.lock ();
  if (!ringfull())
  {
    ring[end] = c;
    end = (end + 1) % PSIZE;
  }
  mtx.unlock ();
}

char ringdequeue ()
{
  mtx.lock ();
  char out;
 
  if (!ringempty())
  {
    out = ring[begin];
    begin = (begin + 1) % PSIZE;
    mtx.unlock ();
    return out;
  }
  else
  {
    mtx.unlock ();
    return '\0';
  }
}

void bufread (char *buffer, int size)
{
  while (!ringused())  {std::cout << "no data in queue.\n"; sleep(2);}
  while (!ringempty() && size)
  {
    *(buffer++) = ringdequeue();
    size--;
  }
}

void bufwrite (char *buffer, int size)
{
  while (!ringavail())  sleep(1);
  while (!ringfull() && size)
  {
    ringenqueue(*buffer++);
    size--;
  }
}


static void PhyRxTrace (std::string path, Ptr<const Packet> packet, uint16_t channelFreqMhz, uint16_t channelNumber, uint32_t rate, bool isShortPreamble, double signalDbm, double noiseDbm)
{
  Ptr<Packet> currentPacket;
  WifiMacHeader hdr;
  currentPacket = packet->Copy ();
  currentPacket->RemoveHeader (hdr);
  TotRxBytes += currentPacket->GetSize ();

   if (hdr.IsData ())
     TotRxPkts++;
  
  PhyState = signalDbm - noiseDbm;
  //NS_LOG_UNCOND ("A packet has been received with signal " << signalDbm << " and noise " << noiseDbm << "\n");
  
  //NS_LOG_UNCOND ("Number of bytes received now is " << TotRxBytes << "\n");
  //NS_LOG_UNCOND ("A packet has been received with signal " << signalDbm << " and noise " << noiseDbm << "\n");
}

static void PhyTxTrace (std::string path, Ptr<const Packet> packet, uint16_t channelFreqMhz, uint16_t channelNumber, uint32_t rate, bool isShortPreamble, uint8_t txPower)
{
  int psize;
  Ptr<Packet> currentPacket;
  WifiMacHeader hdr;
  currentPacket = packet->Copy ();
  currentPacket->RemoveHeader (hdr);
  psize = currentPacket->GetSize ();
  TotTxBytes += psize;

  if (hdr.IsData ())
  //if (psize == 20)
    TotTxPkts++;

     
	//channelOccuPercentage = (double) (TotTxBytes * 8) / (double) (6000000 * 10);
	//std::cout << "Channel Occupency is " << channelOccuPercentage * 100 << "%\n";

  //NS_LOG_UNCOND ("A packet has been sent.\n");
  //NS_LOG_UNCOND ("Number of bytes sent now is " << TotTxBytes << "\n");
}

void SetNewTxPwr (Ptr<Node> node)
{
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  Ptr<WifiNetDevice> wifiNetDev;
  wifiNetDev = DynamicCast<WifiNetDevice> (ipv4->GetNetDevice (1));
  Ptr<WifiPhy> wifiphy = wifiNetDev->GetPhy ();
  Ptr<YansWifiPhy> yansphy = DynamicCast<YansWifiPhy> (wifiphy);

  double CurrentMinPwr = yansphy->GetTxPowerStart ();
  double CurrentMaxPwr = yansphy->GetTxPowerEnd ();
  if (PhyState < 50)
  {
	yansphy->SetTxPowerStart (CurrentMinPwr + 20);
        yansphy->SetTxPowerEnd (CurrentMaxPwr + 20);
  }

  TotRxBytes = 0;
  TotTxBytes = 0;
  Simulator::Schedule (Seconds (1), &SetNewTxPwr, node);
  

}

void
PrintTX ()
{
  NS_LOG_UNCOND("At Second " << Simulator::Now ().GetSeconds () <<", Total transmitted number of packets is " << TotTxPkts << " with this app.\n");
}

void
PrintRX ()
{
  NS_LOG_UNCOND("At Second " << Simulator::Now ().GetSeconds () << ", Total received number of packets is " << TotRxPkts << " with this app.\n");
}

//For applications, create multiple bulksend applications, whose sizes comply to Pareto distribution. 
//Add these applications to transmitting node one by one.
//Configure application start/stop times according to Poisson distribution. 

class MyApp1 : public Application
{
public:

  MyApp1 ();
  virtual ~MyApp1 ();
  void Setup (Ptr<Socket> socket, Address address, DataRate dataRate);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>   m_socket;
  Address       m_peer;
  //uint32_t      m_nPackets;
  DataRate      m_dataRate;
  EventId       m_sendEvent;
  bool          m_running;
  //uint32_t      m_packetsSent;
};

MyApp1::MyApp1 ()
  : m_socket (0),
    m_peer (),
    //m_nPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false)
    //m_packetsSent (0)
{
}

MyApp1::~MyApp1()
{
  m_socket = 0;
}

void
MyApp1::Setup (Ptr<Socket> socket, Address address, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  //m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp1::StartApplication (void)
{
  AppNum++;
  NS_LOG_UNCOND("Starting Application #" << AppNum << "\n");
  TotTxPkts = 0;
  TotRxPkts = 0;
  m_running = true;
  //m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
MyApp1::StopApplication (void)
{
  m_running = false;
  
  if (m_sendEvent.IsRunning ())
    Simulator::Cancel (m_sendEvent);

  if (m_socket)
    m_socket->Close ();
}

void
MyApp1::SendPacket (void)
{
  bufread(buffer, PKTSIZE);
  Ptr<Packet> packet = Create<Packet> (reinterpret_cast<uint8_t*> (buffer), PKTSIZE);
  m_socket->Send (packet);
  std::cout << "Sent 1 packet.\n";
  //if (++m_packetsSent < m_nPackets)
    ScheduleTx ();
}

void
MyApp1::ScheduleTx (void)
{
  if (m_running)
  {
    Time tNext (Seconds (PKTSIZE * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
    m_sendEvent = Simulator::Schedule (tNext, &MyApp1::SendPacket, this);
  }
}

class MyApp2 : public Application
{
public:

  MyApp2 ();
  virtual ~MyApp2 ();
  void Setup (uint32_t nBytes, Time Interval);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleEnqueue (void);
  void EnqueueBytes (void);

  uint32_t      m_nBytes;
  EventId       m_enqueueEvent;
  bool          m_running;
  Time		m_interval;
  //uint32_t      m_bytesEnqueued;
};

MyApp2::MyApp2 ()
  : m_nBytes (0),
    m_enqueueEvent (),
    m_running (false)
    //m_bytesEnqueued (0)
{
}

MyApp2::~MyApp2()
{
}

void
MyApp2::Setup (uint32_t nBytes, Time Interval)
{
  m_nBytes = nBytes;
  m_interval = Interval;
}

void
MyApp2::StartApplication (void)
{
  AppNum++;
  NS_LOG_UNCOND("Starting Application #" << AppNum << "\n");
//  TotTxPkts = 0;
//  TotRxPkts = 0;
  m_running = true;
  //m_bytesEnqueued = 0;
  EnqueueBytes ();
}

void
MyApp2::StopApplication (void)
{
  m_running = false;
  
  if (m_enqueueEvent.IsRunning ())
    Simulator::Cancel (m_enqueueEvent);
}

void
MyApp2::EnqueueBytes (void)
{
  char *str;
  str = new char[m_nBytes + 1];
  gen_random (str, m_nBytes + 1);  

  bufwrite (str, m_nBytes + 1);
  delete [] str;
  std::cout << m_nBytes << " bytes enqueued.\n";
  //if (++m_bytesEnqueued < m_nBytes)
  ScheduleEnqueue ();
}

void
MyApp2::ScheduleEnqueue (void)
{
  if (m_running)
  {
    //std::cout << "scheduling another enqueue.\n";
    Time tNext (m_interval);
    double sectime = tNext.GetSeconds ();
    std::cout << "enqueue interval is " << sectime << "\n";
    m_enqueueEvent = Simulator::Schedule (tNext, &MyApp2::EnqueueBytes, this);
  }
}



int main (int argc, char * argv[])
{

//	double channelOccuPercentage = 0;
	bool verbose = true;
	uint32_t nWifi = 5;

	CommandLine cmd;
	cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
	cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);

	cmd.Parse (argc, argv);

	if (verbose)
	{
		LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
		LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
	}

	NodeContainer Apnode;
	NodeContainer Stanodes;
	Apnode.Create (1);
	Stanodes.Create (nWifi);

	YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
	YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
	phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
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

	mobility.SetPositionAllocator ("ns3::GridPositionAllocator", "MinX", DoubleValue (0.0), "MinY", DoubleValue (0.0), "DeltaX", DoubleValue (5.0), "DeltaY", DoubleValue (10.0), "GridWidth", UintegerValue (3), "LayoutType", StringValue ("RowFirst"));

	mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
	mobility.Install (Stanodes);

	mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	mobility.Install (Apnode);

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


	Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (Apnode.Get (0), TcpSocketFactory::GetTypeId ());
	Ptr<MyApp1> app1 = CreateObject<MyApp1> ();
	app1->Setup (ns3TcpSocket, sinkAddress, DataRate ("256kbps"));
	Apnode.Get (0)->AddApplication (app1);
	app1->SetStartTime (Seconds (9.0));
	app1->SetStopTime (Seconds (20.0));

	//ns3TcpSocket = Socket::CreateSocket (Apnode.Get (0), TcpSocketFactory::GetTypeId ());
	Ptr<MyApp2> app2 = CreateObject<MyApp2> ();
	app2->Setup (40, Seconds (1.0));
	Apnode.Get (0)->AddApplication (app2);
	app2->SetStartTime (Seconds (1.0));
	app2->SetStopTime (Seconds (8.0));


	Simulator::Stop (Seconds (20.0));

	Config::Connect ("/NodeList/5/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferRx", MakeCallback (&PhyRxTrace));
	phy.EnablePcap ("testwifi", apDevice);

	Config::Connect ("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferTx", MakeCallback (&PhyTxTrace));
	//Config::Connect ("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferTx", MakeCallback (&PhyTxTrace));
	//Config::Connect ("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferTx", MakeCallback (&PhyTxTrace));
	//Config::Connect ("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferTx", MakeCallback (&PhyTxTrace));
	//Config::Connect ("/NodeList/4/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferTx", MakeCallback (&PhyTxTrace));

	//Config::Connect ("/NodeList/5/DeviceList/*/$ns3::WifiNetDevice/Phy/MonitorSnifferTx", MakeCallback (&PhyTxTrace));
	Simulator::Schedule (Seconds (7.0), &PrintTX);
	Simulator::Schedule (Seconds (7.0), &PrintRX);
	Simulator::Schedule (Seconds (19.0), &PrintTX);
	Simulator::Schedule (Seconds (19.0), &PrintRX);



	SetNewTxPwr (Apnode.Get (0));
	Simulator::Run ();

	MacState = (double) (TotTxBytes * 8) / (double) (6000000 * 10);
	//channelOccuPercentage = (double) (TotTxBytes * 8) / (double) (6000000 * 10);
	//std::cout << "Channel Occupency is " << channelOccuPercentage * 100 << "%\n";

	Simulator::Destroy ();
	//NS_LOG_UNCOND("Total transmitted number of packets is " << TotTxPkts << "\n");
	//NS_LOG_UNCOND("Total received number of packets is " << TotRxPkts << "\n");

	return 0;
}
