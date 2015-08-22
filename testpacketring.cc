#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
//#include <vector>

#define PSIZE 8192
#define PKTSIZE 20

using namespace ns3;

Ptr<Packet> ring[PSIZE];
int begin = 0;
int end = 0;
Ptr<Packet> buffer[10];


int ringempty() {return begin == end;}
int ringfull() {return (end + 1) % PSIZE == begin;}
int ringused() {return (end - begin + PSIZE) % PSIZE;}
int ringavail() {return PSIZE - 1 - ringused();}

void ringenqueue (Ptr<Packet> pkt)
{
  if (!ringfull())
  {
    ring[end] = pkt;
    end = (end + 1) % PSIZE;
  }
}

Ptr<Packet> ringdequeue ()
{
  Ptr<Packet> out;
  if (!ringempty())
  {
    out = ring[begin];
    begin = (begin + 1) % PSIZE;
    return out;
  }
  else
    return 0;
}

void bufread (Ptr<Packet> buffer[], int size)
{
  while (!ringused())  sleep(1);
  while (!ringempty() && size)
  {
    *(buffer++) = ringdequeue();
    size--;
  }
}

void bufwrite (Ptr<Packet> buffer[], int size)
{
  while (!ringavail())  sleep(1);
  while (!ringfull() && size)
  {
    ringenqueue(*buffer++);
    size--;
  }
}

int main()
{
  int i;
  Ptr<Packet> data[10];
  for (i = 0; i < 10; i++)
    data[i] = Create<Packet> (PKTSIZE);
  
  std::cout << "begin = " << begin << "\n";
  std::cout << "end = " << end << "\n";
  std::cout << "used = " << ringused() << "\n";
  std::cout << "avail = " << ringavail() << "\n";

  bufwrite (data, 10);
  std::cout << "begin = " << begin << "\n";
  std::cout << "end = " << end << "\n";
  std::cout << "used = " << ringused() << "\n";
  std::cout << "avail = " << ringavail() << "\n";

  bufread(buffer, 10);
  std::cout << "begin = " << begin << "\n";
  std::cout << "end = " << end << "\n";
  std::cout << "used = " << ringused() << "\n";
  std::cout << "avail = " << ringavail() << "\n";

  for (i = 0; i < 10; i++)
//    buffer[i]->Print (std::cout);
    NS_LOG_UNCOND("Content of packet is " << *buffer[i]);
  return 0;
}
