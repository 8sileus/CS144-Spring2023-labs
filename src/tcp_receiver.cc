#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  // Your code here.
  if ( message.SYN ) {
    zero_point_ = message.seqno;
  }
  if ( !zero_point_.has_value() ) {
    return;
  }

  uint64_t first_index { message.seqno.unwrap( zero_point_.value(), inbound_stream.bytes_pushed() ) - 1
                         + message.SYN };
  reassembler.insert( first_index, message.payload.release(), message.FIN, inbound_stream );
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  // Your code here.
  TCPReceiverMessage res;
  if ( zero_point_.has_value() ) {
    res.ackno = Wrap32::wrap( inbound_stream.bytes_pushed() + 1 + inbound_stream.is_closed(), zero_point_.value() );
  }
  res.window_size = std::min<uint64_t>( UINT16_MAX, inbound_stream.available_capacity() );
  return res;
}
