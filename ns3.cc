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

int totalVal;
int total_drops = 0;
bool first_drop = true;

// Function to record packet drops
static void
RxDrop (Ptr<const Packet> p)
{
  std::cout << "bow bow a\n";
  if (first_drop)
    {
      first_drop = false;
      //*stream->GetStream () << 0 << " " << 0 << std::endl;
    }
  //*stream->GetStream () << Simulator::Now ().GetSeconds () << " " << ++total_drops << std::endl;
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
  std::cout << "bow bow\n";
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

int
main (int argc, char *argv[])
{

  uint32_t maxBytes = 0;
  std::string prot = "TcpWestwood";
  double error = 0.000001;

  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
  Config::SetDefault ("ns3::TcpWestwood::FilterType", EnumValue (TcpWestwood::TUSTIN));

  std::string a_s = "bytes_" + prot + ".dat";
  std::string b_s = "drop_" + prot + ".dat";
  std::string c_s = "cw_" + prot + ".dat";

  // Create file streams for data storage
  Ptr<OutputStreamWrapper> total_bytes_data = ascii.CreateFileStream (a_s);
  Ptr<OutputStreamWrapper> dropped_packets_data = ascii.CreateFileStream (b_s);
  Ptr<OutputStreamWrapper> cw_data = ascii.CreateFileStream (c_s);

  // Explicitly create the nodes required by the topology (shown above).
  // NS_LOG_INFO ("Create nodes.");
  NodeContainer nodes;
  nodes.Create (2);

  //NS_LOG_INFO ("Create channels.");

  // Explicitly create the point-to-point link required by the topology (shown above).
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));
  pointToPoint.SetQueue ("ns3::DropTailQueue");

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  // Create error model
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (error));
  devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  // Install the internet stack on the nodes
  InternetStackHelper internet;
  internet.Install (nodes);

  // We've got the "hardware" in place.  Now we need to add IP addresses.
  //NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ipv4Container = ipv4.Assign (devices);

  //NS_LOG_INFO ("Create Applications.");

  // Create a BulkSendApplication and install it on node 0
  uint16_t port = 12344;
  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (ipv4Container.GetAddress (1), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  ApplicationContainer sourceApps = source.Install (nodes.Get (0));
  sourceApps.Start (Seconds (0.0));
  sourceApps.Stop (Seconds (1.80));

  // Create a PacketSinkApplication and install it on node 1
  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (1));

  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (1.80));

  tcpSink = DynamicCast<PacketSink> (sinkApps.Get (0));

  uint16_t cbrPort = 12345;

  double startTimes[5] = {0.2, 0.4, 0.6, 0.8, 1.0};
  double endTimes[5] = {1.8, 1.8, 1.2, 1.4, 1.6};

  for (int i = 0; i < 5; i++)
    {
      // Install applications: five CBR streams each saturating the channel
      ApplicationContainer cbrApps;
      ApplicationContainer cbrSinkApps;
      /*
      OnOffHelper is a helper class in ns-3 that is used to create traffic generators that generate traffic periodically. 
      It can be used to simulate different types of traffic such as Constant Bit Rate (CBR) or Exponential Bit Rate (EBR) traffic.
      */
      OnOffHelper onOffHelper ("ns3::UdpSocketFactory",
                               InetSocketAddress (ipv4Container.GetAddress (1), cbrPort + i));
      onOffHelper.SetAttribute ("PacketSize", UintegerValue (1024));
      onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      /*
        These lines of code set the OnTime and OffTime attributes of the OnOffHelper object 'onoff' to 
        ConstantRandomVariable objects with constant values of 1 and 0, respectively.

        The OnTime attribute specifies the duration of the ON period,
        which is the period during which the traffic generator generates traffic. 
        By setting the OnTime attribute to a ConstantRandomVariable with a constant value of 1 second,
        we ensure that the ON period duration is always 1 second.
        The OffTime attribute specifies the duration of the OFF period, 
        which is the period during which the traffic generator does not generate traffic. 
        By setting the OffTime attribute to a ConstantRandomVariable with a constant value of 0 seconds, 
        we ensure that there is no OFF period and the traffic generator generates traffic continuously.

        In other words, the OnOffHelper object 'onoff' will generate traffic continuously with a rate of 300 Kbps
        as specified by the SetAttribute("DataRate") function call, 
        and without any interruptions due to an OFF period because we have set the OffTime attribute to 0.

        Overall, these lines of code are used to configure the OnOffHelper object 'onoff' to generate CBR 
        traffic at a constant rate of 300 Kbps without any interruptions, which is the requirement specified in the problem statement.

    */

      onOffHelper.SetAttribute ("DataRate", StringValue ("300Kbps"));
      onOffHelper.SetAttribute ("StartTime", TimeValue (Seconds (startTimes[i])));
      onOffHelper.SetAttribute ("StopTime", TimeValue (Seconds (endTimes[i])));
      cbrApps.Add (onOffHelper.Install (nodes.Get (0)));
      // Packet sinks for each CBR agent

      PacketSinkHelper sink ("ns3::UdpSocketFactory",
                             InetSocketAddress (Ipv4Address::GetAny (), cbrPort + i));
      cbrSinkApps = sink.Install (nodes.Get (1));
      cbrSinkApps.Start (Seconds (0.0));
      cbrSinkApps.Stop (Seconds (1.8));
      cbrSinks[i] = DynamicCast<PacketSink> (cbrSinkApps.Get (0));
    }

  devices.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));

  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("tcp-comparision.tr"));
  pointToPoint.EnablePcapAll ("tcp-comparision", true);

  Simulator::Schedule (Seconds (0.00001), &TotalRx, total_bytes_data);
  Simulator::Schedule (Seconds (0.00001), &TraceCwnd, cw_data);

  // Flow monitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll ();

  Simulator::Stop (Seconds (1.80));
  Simulator::Run ();
}
