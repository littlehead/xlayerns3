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
//#include <pthread.h>

#define PSIZE 8192
#define PKTSIZE 20

using namespace ns3;

char ring[PSIZE];
int begin = 0;
int end = 0;
char buffer[PKTSIZE + 1];
int AppNum = 0;

/*class Mutex
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

Mutex mtx;*/

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
  //mtx.lock ();
  if (!ringfull())
  {
    ring[end] = c;
    end = (end + 1) % PSIZE;
  }
  //mtx.unlock ();
}

char ringdequeue ()
{
  //mtx.lock ();
  char out;
 
  if (!ringempty())
  {
    out = ring[begin];
    begin = (begin + 1) % PSIZE;
   // mtx.unlock ();
    return out;
  }
  else
  {
   // mtx.unlock ();
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
  std::cout << "In bufwrite\n";
  while (!ringavail())  sleep(1);
  std::cout << "Space available to write\n";
  while (!ringfull() && size)
  {
    ringenqueue(*buffer++);
    size--;
  }
}

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
  //TotTxPkts = 0;
  //TotRxPkts = 0;
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
  void Setup (uint32_t nBytes);

private:

  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleEnqueue (void);
  void EnqueueBytes (void);

  uint32_t      m_nBytes;
  EventId       m_enqueueEvent;
  bool          m_running;
  //Time		m_interval;
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
MyApp2::Setup (uint32_t nBytes)
{
  m_nBytes = nBytes;
  //m_interval = Interval;
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
  std::cout << "generating random sting...\n";
  char *str;
  str = new char[m_nBytes + 1];
  gen_random (str, m_nBytes + 1);  
  std::cout << "Generated random string.\n";
  bufwrite (str, m_nBytes + 1);
  delete [] str;
  std::cout << m_nBytes << " bytes enqueued.\n";
  //if (++m_bytesEnqueued < m_nBytes)
  ScheduleEnqueue ();
}

void
MyApp2::ScheduleEnqueue (void)
{
  //std::cout << "scheduling another enqueue\n";
  if (m_running)
  {
    std::cout << "enqueue event still running\n";
 
    //std::cout << "scheduling another enqueue.\n";
    //Time tNext (m_interval);
   // double sectime = tNext.GetSeconds ();
   // std::cout << "enqueue interval is " << sectime << "\n";
    m_enqueueEvent = Simulator::Schedule (Seconds (1.0), &MyApp2::EnqueueBytes, this);
  }
}



int main (int argc, char * argv[])
{
	bool verbose = true;
	uint32_t nWifi = 5;
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
	app2->Setup (40);
	Apnode.Get (0)->AddApplication (app2);
	app2->SetStartTime (Seconds (1.0));
	app2->SetStopTime (Seconds (8.0));


	Simulator::Stop (Seconds (20.0));
	Simulator::Run ();
	Simulator::Destroy ();
	return 0;


}
