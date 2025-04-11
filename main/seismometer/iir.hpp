#pragma once

#include <array>

class IIRFilter2 {
  public:
    IIRFilter2();
    IIRFilter2(std::array<float, 6> coef);
      void init(std::array<float, 6> coef);
      float add_sample(float x);
  private:
    float b0, b1, b2, a0, a1, a2;
    float x1, x2, y1, y2;
};


std::array<float, 6> hpf0_05_coef();