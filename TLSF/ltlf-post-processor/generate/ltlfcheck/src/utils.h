//
// Created by philipp on 24/07/23.
//

#pragma once

#include <iostream>
#include <math.h>

// Conditional output
struct my_out_c{
  inline static bool v_ = false;
};

extern my_out_c my_out;

template<class T>
my_out_c& operator<<(my_out_c& mo, const T& t){
  if (mo.v_)
    std::cout << t;
  return mo;
}

// Bits for binary encoding
inline unsigned
nbits(unsigned n) {
  return std::max(1., std::ceil(std::log2(n)));
}