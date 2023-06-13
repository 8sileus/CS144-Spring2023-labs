#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return send_seqno_ - ack_seqno_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return retrans_cnt_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  optional<TCPSenderMessage> res {};
  if ( !unsend_msgs_.empty() && send_seqno_ > 0 ) {
    res = std::move( unsend_msgs_.front() );
    unsend_msgs_.pop();
  }
  return res;
}

void TCPSender::push( Reader& outbound_stream )
{
  // Your code here.
  uint64_t cur_window_size = window_size_ ? window_size_ : 1;
  while ( cur_window_size > sequence_numbers_in_flight() ) {
    auto len = cur_window_size - sequence_numbers_in_flight();
    std::string data {
      outbound_stream.peek().substr( 0, std::min( TCPConfig::MAX_PAYLOAD_SIZE, len - ( send_seqno_ == 0 ) ) ) };
    TCPSenderMessage msg {
      .seqno { Wrap32::wrap( send_seqno_, isn_ ) },
      .SYN { send_seqno_ == 0 },
      .payload { std::move( data ) },
    };
    outbound_stream.pop( msg.payload.size() );

    if ( !sended_fin && outbound_stream.is_finished() && msg.payload.size() < len - ( send_seqno_ == 0 ) ) {
      msg.FIN = true;
      sended_fin = true;
    }

    if ( msg.sequence_length() == 0 ) {
      break;
    }
    if ( sended_msgs_.empty() ) {
      RTO_ms_ = initial_RTO_ms_;
      time_ = 0;
    }

    sended_msgs_.emplace( send_seqno_, msg );
    send_seqno_ += msg.sequence_length();
    unsend_msgs_.push( std::move( msg ) );

    if(msg.FIN){
      break;
    }
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // Your code here.
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap( send_seqno_, isn_ );
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  if ( msg.ackno.has_value() ) {
    auto new_ack_seq = msg.ackno.value().unwrap( isn_, send_seqno_ );
    if ( new_ack_seq > send_seqno_ ) {
      return;
    }
    auto it = sended_msgs_.begin();
    while ( it != sended_msgs_.end() && it->first + it->second.sequence_length() <= new_ack_seq ) {
      ack_seqno_ += it->second.sequence_length();
      it = sended_msgs_.erase( it );
      RTO_ms_ = initial_RTO_ms_;
      if ( !sended_msgs_.empty() ) {
        time_ = 0;
      }
    }
    retrans_cnt_ = 0;
  }
  window_size_ = msg.window_size;
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  time_ += ms_since_last_tick;
  if ( time_ >= RTO_ms_ && !sended_msgs_.empty() ) {
    const auto& [k, msg] = *sended_msgs_.begin();
    if ( window_size_ > 0 ) {
      RTO_ms_ *= 2;
    }
    time_ = 0;
    ++retrans_cnt_;
    unsend_msgs_.push( msg );
  }
}
