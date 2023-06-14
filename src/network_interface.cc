#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

constexpr size_t ARP_CACHE_TIME = 30 * 1000;
const size_t ARP_RQUEST_TIME = 5 * 1000;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t addr = next_hop.ipv4_numeric();
  if ( arp_table_.count( addr ) ) {
    EthernetFrame ef;
    ef.header.src = ethernet_address_;
    ef.header.dst = arp_table_[addr].first;
    ef.header.type = EthernetHeader::TYPE_IPv4;
    ef.payload = serialize( dgram );
    frames_.emplace( std::move( ef ) );
  } else {
    if ( !waiting_arp_.count( addr ) ) {
      ARPMessage arp_msg;
      arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
      arp_msg.sender_ethernet_address = ethernet_address_;
      arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
      arp_msg.target_ethernet_address = {};
      arp_msg.target_ip_address = addr;

      EthernetFrame ef;
      ef.header.src = ethernet_address_;
      ef.header.dst = ETHERNET_BROADCAST;
      ef.header.type = EthernetHeader::TYPE_ARP;
      ef.payload = serialize( arp_msg );
      frames_.emplace( std::move( ef ) );

      waiting_arp_.emplace( addr, WaitingDatagram { .remaining_time_ { ARP_RQUEST_TIME }, .datagrams_ {} } );
    }
    waiting_arp_[addr].datagrams_.emplace_back( dgram );
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST ) {
    return std::nullopt;
  }

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram res;
    if ( !parse( res, frame.payload ) ) {
      return std::nullopt;
    }
    return res;
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp_msg;
    if ( !parse( arp_msg, frame.payload ) ) {
      return std::nullopt;
    }

    bool ok { arp_msg.opcode == ARPMessage::OPCODE_REQUEST
              && arp_msg.target_ip_address == ip_address_.ipv4_numeric() };

    if ( ok ) {
      ARPMessage arp_reply_msg;
      arp_reply_msg.opcode = ARPMessage::OPCODE_REPLY;
      arp_reply_msg.sender_ethernet_address = ethernet_address_;
      arp_reply_msg.sender_ip_address = ip_address_.ipv4_numeric();
      arp_reply_msg.target_ethernet_address = arp_msg.sender_ethernet_address;
      arp_reply_msg.target_ip_address = arp_msg.sender_ip_address;

      EthernetFrame ef;
      ef.header.src = ethernet_address_;
      ef.header.dst = arp_msg.sender_ethernet_address;
      ef.header.type = EthernetHeader::TYPE_ARP;
      ef.payload = serialize( arp_reply_msg );

      frames_.emplace( std::move( ef ) );
    }

    ok |= arp_msg.opcode == ARPMessage::OPCODE_REPLY && arp_msg.target_ethernet_address == ethernet_address_;

    if(ok){
      arp_table_.emplace( arp_msg.sender_ip_address,
                          std::make_pair( arp_msg.sender_ethernet_address, ARP_CACHE_TIME ) );
      auto addr = arp_msg.sender_ip_address;

      if ( waiting_arp_.count( addr ) ) {
        for ( const auto& datagram : waiting_arp_[addr].datagrams_ ) {
          send_datagram( datagram, Address::from_ipv4_numeric( addr ) );
        }
      }
      waiting_arp_.erase( addr );
    }
  }
  return std::nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick ) {
  for ( auto it = arp_table_.begin(); it != arp_table_.end(); ){
    if ( it->second.second <= ms_since_last_tick ) {
      it = arp_table_.erase( it );
    } else {
      it->second.second -= ms_since_last_tick;
      ++it;
    }
  }

  for ( auto& [addr, data] : waiting_arp_ ) {
    auto& [time, _] = data;
    if ( time <= ms_since_last_tick ) {
      ARPMessage arp_msg;
      arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
      arp_msg.sender_ethernet_address = ethernet_address_;
      arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
      arp_msg.target_ethernet_address = {};
      arp_msg.target_ip_address = addr;

      EthernetFrame ef;
      ef.header.src = ethernet_address_;
      ef.header.dst = ETHERNET_BROADCAST;
      ef.header.type = EthernetHeader::TYPE_ARP;
      ef.payload = serialize( arp_msg );
      frames_.emplace( std::move( ef ) );

      time = ARP_RQUEST_TIME;
    } else {
      time -= ms_since_last_tick;
    }
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  optional<EthernetFrame> res {};
  if ( !frames_.empty() ) {
    res = std::move( frames_.front() );
    frames_.pop();
  }
  return res;
}
