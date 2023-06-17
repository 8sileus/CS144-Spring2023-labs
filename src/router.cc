#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  if ( prefix_length == 0 ) {
    root_->interface_num_ = interface_num;
    root_->next_hop_ = next_hop;
    root_->is_end_ = true;
    return;
  }

  auto cur = root_.get();
  for ( uint8_t i = 0; i < prefix_length; ++i ) {
    auto next = ( route_prefix >> ( 31 - i ) & 1 );
    if ( cur->children_[next] == nullptr ) {
      cur->children_[next].reset( new TrieNode );
    }
    cur = cur->children_[next].get();
  }
  cur->next_hop_ = next_hop;
  cur->is_end_ = true;
  cur->interface_num_ = interface_num;
}

void Router::route()
{
  for ( auto& it : interfaces_ ) {
    while ( auto op = it.maybe_receive() ) {
      auto datagram = op.value();
      auto dst = datagram.header.dst;
      // std::cout << "sendDatagram dst:" << Address::from_ipv4_numeric( dst ).ip() << std::endl;
      TrieNode* cur { root_.get() };
      TrieNode* node { cur };
      for ( int i = 0; i < 32; ++i ) {
        auto next = ( dst >> ( 31 - i ) & 1 );
        if ( !cur->children_[next] ) {
          break;
        }
        cur = cur->children_[next].get();
        if ( cur->is_end_ ) {
          node = cur;
        }
      }

      if ( datagram.header.ttl > 1 && node->is_end_ ) {
        --datagram.header.ttl;
        datagram.header.compute_checksum();
        auto next_hop = node->next_hop_.has_value() ? node->next_hop_.value()
                                                    : Address::from_ipv4_numeric( datagram.header.dst );
        interface( node->interface_num_ ).send_datagram( datagram, next_hop );
        // std::cout << "send  " << Address::from_ipv4_numeric( datagram.header.dst ).ip() << " ->  " << next_hop.ip()
        //           << " in interface: " << node->interface_num_ << "\n";
      }
    }
  }
}
