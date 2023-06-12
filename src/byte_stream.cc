#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity )
{
  buffer_.resize( capacity );
}

void ByteStream::makeSpace()
{
  std::copy( buffer_.data() + start_, buffer_.data() + len_, buffer_.data() );
  len_ -= start_;
  start_ = 0;
}

void Writer::push( string data )
{
  uint64_t len = data.size();
  if ( len > available_capacity() ) {
    data.resize( available_capacity() );
    len = data.size();
  }

  if ( len <= capacity_ - len_ ) {
    std::copy( data.begin(), data.end(), buffer_.begin() + len_ );
    len_ += len;
    pushed_bytes_ += len;
  } else {
    makeSpace();
    std::copy( data.begin(), data.end(), buffer_.begin() + len_ );
    len_ += len;
    pushed_bytes_ += len;
  }
}

void Writer::close()
{
  // Your code here.
  closed_ = true;
}

void Writer::set_error()
{
  // Your code here.
  has_err_ = true;
}

bool Writer::is_closed() const
{
  // Your code here.
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_ - len_ + start_;
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return pushed_bytes_;
}

string_view Reader::peek() const
{
  // Your code here.
  return { buffer_.data() + start_, len_ - start_ };
}

bool Reader::is_finished() const
{
  // Your code here.
  return closed_ && len_ == start_;
}

bool Reader::has_error() const
{
  // Your code here.
  return has_err_;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  if ( len > len_ - start_ ) {
    len = len_ - start_;
  }
  start_ += len;
  popped_bytes_ += len;
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return len_ - start_;
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return popped_bytes_;
}
