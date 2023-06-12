#include "wrapping_integers.hh"

#include <iostream>
#include <numeric>
using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return Wrap32 { static_cast<uint32_t>( n ) + zero_point.raw_value_ };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  auto temp = Wrap32::wrap( checkpoint, zero_point );
  uint32_t a = temp.raw_value_ - raw_value_;
  uint32_t b = raw_value_ - temp.raw_value_;
  if ( a < b ) {
    if ( a > checkpoint ) {
      checkpoint += ( 1ull << 32 );
    }
    return checkpoint - a;
  }
  return checkpoint + b;
}
