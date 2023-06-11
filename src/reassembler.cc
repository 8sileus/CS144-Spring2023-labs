#include "reassembler.hh"

#include <iostream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  max_index_ = std::max( max_index_, next_index_ + output.available_capacity() );

  if ( is_last_substring ) {
    end_index_ = first_index + data.size();
    if ( next_index_ == end_index_ ) {
      output.close();
    }
  }

  // out of range
  if ( first_index + data.size() <= next_index_ || first_index >= max_index_ || max_index_ == next_index_ ) {
    return;
  }
  std::cout <<"origin :" <<data << std::endl;
  // slice front
  if ( first_index < next_index_ && first_index + data.size() > next_index_ ) {
    data = data.substr( next_index_ - first_index );
    // std::cout << "slice front:" << data << std::endl;
    first_index = next_index_;
  }
  // slice back
  if ( first_index <= max_index_ && first_index + data.size() > max_index_ ) {
    data = data.substr( 0, max_index_ - first_index );
    // std::cout << "slice back:" << data << std::endl;
  }
  pushToBuffer( first_index, std::move( data ) );

  for ( auto it = buffer_.begin(); it != buffer_.end() && it->first == next_index_; it = buffer_.erase( it ) ) {
    next_index_ += it->second.size();
    output.push( std::move( it->second ) );
  }

  if ( next_index_ == end_index_ ) {
    output.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  uint64_t res { 0 };
  for ( auto& [_, s] : buffer_ ) {
    res += s.size();
  }
  return res;
}

void Reassembler::pushToBuffer( uint64_t left_index, std::string&& data )
{
  uint64_t right_index_ = left_index + data.size();
  auto it = buffer_.lower_bound( left_index );
  while ( it != buffer_.end() ) {
    if ( it->first + it->second.size() <= right_index_ ) {
      it = buffer_.erase( it );
    } else {
      break;
    }
  }
  if ( it != buffer_.end() ) {
    // 判断是否被包含
    if ( it->first == left_index ) {
      return;
    }
    data = data.substr( 0, it->first - left_index );
  }
  if ( it != buffer_.begin() ) {
    auto pre_it = prev( it );
    uint64_t prev_right_index = pre_it->first + pre_it->second.size();
    // 判断是否被包含
    if(prev_right_index>=right_index_){
      return;
    }
    if ( prev_right_index >= left_index ) {
      pre_it->second.append( data.substr( prev_right_index - left_index ) );
      return;
    }
  }
  buffer_.emplace( left_index, std::move( data ) );
}