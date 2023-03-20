#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/error-model.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/enum.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

//NS_LOG_COMPONENT_DEFINE ("TcpComparision");

AsciiTraceHelper ascii;
Ptr<PacketSink> cbrSinks[5], tcpSink;
std::string bytesDroppedName, packetsDroppedName, congestionWindowName;

int totalVal;
int total_drops = 0;
bool first_drop = true;

// Function to record packet drops
static void
RxDrop (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p)
{
  if (first_drop)
    {
      first_drop = false;
      *stream->GetStream () << 0 << " " << 0 << std::endl;
    }
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << ++total_drops << std::endl;
}
// Function to find the total cumulative recieved bytes
static void
TotalRx (Ptr<OutputStreamWrapper> stream)
{
  totalVal = tcpSink->GetTotalRx ();

  for (int i = 0; i < 5; i++)
    {
      totalVal += cbrSinks[i]->GetTotalRx ();
    }

  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << totalVal << std::endl;

  Simulator::Schedule (Seconds (0.0001), &TotalRx, stream);
}

// Function to record Congestion Window values
static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  //std::cout << "bow bow\n";
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newCwnd << std::endl;
}

//Trace Congestion window length
static void
TraceCwnd (Ptr<OutputStreamWrapper> stream)
{
  //Trace changes to the congestion window
  Config::ConnectWithoutContext ("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
                                 MakeBoundCallback (&CwndChange, stream));
}

static void
setAttributesForOnOffHelper (OnOffHelper cbrApp, double cbrStartTimes[], double cbrEndTimes[],
                             int no)
{

  cbrApp.SetAttribute ("PacketSize", UintegerValue (1024));
  cbrApp.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  cbrApp.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  cbrApp.SetAttribute ("DataRate", StringValue ("300Kbps"));
  cbrApp.SetAttribute ("StartTime", TimeValue (Seconds (cbrStartTimes[no])));
  cbrApp.SetAttribute ("StopTime", TimeValue (Seconds (cbrEndTimes[no])));
}

static void
setDefaultTcpFlavour (std::string tcpFlavour = "TcpWestwood")
{
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
  Config::SetDefault ("ns3::TcpWestwood::FilterType", EnumValue (TcpWestwood::TUSTIN));
  bytesDroppedName = tcpFlavour + "_bytes.dat";
  packetsDroppedName = tcpFlavour + "_drop.dat";
  congestionWindowName = tcpFlavour + "_cw.dat";
}

static void
bindCallBackForStats (NetDeviceContainer devices, Ptr<OutputStreamWrapper> total_bytes_data,
                      Ptr<OutputStreamWrapper> dropped_packets_data,
                      Ptr<OutputStreamWrapper> cw_data)
{
  devices.Get (1)->TraceConnectWithoutContext ("PhyRxDrop",
                                               MakeBoundCallback (&RxDrop, dropped_packets_data));

  Simulator::Schedule (Seconds (0.00001), &TotalRx, total_bytes_data);
  Simulator::Schedule (Seconds (0.00001), &TraceCwnd, cw_data);
}

static void
setErrorRate (NetDeviceContainer devices, double errorRate)
{
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (errorRate));
  devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
}

int
main (int argc, char *argv[])
{

  uint32_t maxBytes = 0;
  double error = 0.0001;
  double simTime = 1.80;

  setDefaultTcpFlavour ();

  Ptr<OutputStreamWrapper> total_bytes_data = ascii.CreateFileStream (bytesDroppedName);
  Ptr<OutputStreamWrapper> dropped_packets_data = ascii.CreateFileStream (packetsDroppedName);
  Ptr<OutputStreamWrapper> cw_data = ascii.CreateFileStream (congestionWindowName);

  NodeContainer nodes;
  nodes.Create (2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));
  p2p.SetQueue ("ns3::DropTailQueue");

  NetDeviceContainer devices;
  devices = p2p.Install (nodes);

  setErrorRate (devices, error);
  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ipv4Con = ipv4.Assign (devices);

  uint16_t port = 21;
  BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (ipv4Con.GetAddress (1), port));
  source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (0.0));
  sourceApps.Stop (Seconds (simTime));

  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (1));

  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simTime));

  tcpSink = DynamicCast<PacketSink> (sinkApps.Get (0));

  uint16_t cbrPort = 12345;

  double cbrStartTimes[5] = {0.2, 0.4, 0.6, 0.8, 1.0};
  double cbrEndTimes[5] = {1.8, 1.8, 1.2, 1.4, 1.6};

  for (int i = 0; i < 5; i++)
    {
      ApplicationContainer cbrApps;
      ApplicationContainer cbrSinkApps;

      OnOffHelper onOffHelper ("ns3::UdpSocketFactory",
                               InetSocketAddress (ipv4Con.GetAddress (1), cbrPort + i));

      setAttributesForOnOffHelper (onOffHelper, cbrStartTimes, cbrEndTimes, i);

      cbrApps.Add (onOffHelper.Install (nodes.Get (0)));

      PacketSinkHelper sink ("ns3::UdpSocketFactory",
                             InetSocketAddress (Ipv4Address::GetAny (), cbrPort + i));
      cbrSinkApps = sink.Install (nodes.Get (1));
      cbrSinkApps.Start (Seconds (0.0));
      cbrSinkApps.Stop (Seconds (simTime));
      cbrSinks[i] = DynamicCast<PacketSink> (cbrSinkApps.Get (0));
    }

  bindCallBackForStats (devices, total_bytes_data, dropped_packets_data, cw_data);

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
}
