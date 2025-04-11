
#include "iir.hpp"

#include <array>

IIRFilter2::IIRFilter2(){
  x1 = 0;
  x2 = 0;
  y1 = 0;
  y2 = 0;
  init({0,0,0,0,0,0});
}

IIRFilter2::IIRFilter2(std::array<float, 6> coef) {
  x1 = 0;
  x2 = 0;
  y1 = 0;
  y2 = 0;
  init(coef);
};


void IIRFilter2::init(std::array<float, 6> coef) {
  b0 = coef[0];
  b1 = coef[1];
  b2 = coef[2];
  a0 = coef[3];
  a1 = coef[4];
  a2 = coef[5];
}

float IIRFilter2::add_sample(float x) {
  float y = (-a1 * y1 - a2 * y2 + b0 * x + b1 * x1 + b2 * x2) / a0;
  x2 = x1;
  x1 = x;
  y2 = y1;
  y1 = y;
  return y;
};


std::array<float, 6> hpf0_05_coef(){
  constexpr float b0 = 0.99778102;
  constexpr float b1 = -1.99556205;
  constexpr float b2 = 0.99778102;
  constexpr float a0 = 1. ;
  constexpr float a1 = -1.99555712;
  constexpr float a2 = 0.99556697;
  return {b0, b1, b2, a0, a1, a2};
};