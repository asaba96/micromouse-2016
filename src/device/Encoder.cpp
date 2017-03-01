#include <cmath>
#include "Encoder.h"

Encoder gEncoderLF(4, 2);
Encoder gEncoderRF(8, 7);
Encoder gEncoderLB(5, 6);
Encoder gEncoderRB(9, 10);

Motion::Matrix<Encoder&> gEncoder(
  gEncoderLF, gEncoderRF,
  gEncoderLB, gEncoderRB
);

Encoder::Encoder(uint8_t pin1, uint8_t pin2) : encoder_(pin1, pin2)
{}

Motion::LengthUnit Encoder::displacement()
{
  return Motion::LengthUnit::fromCounts(encoder_.extrapolate());
}

void Encoder::displacement(Motion::LengthUnit value)
{
  encoder_.write(nearbyint(value.counts()));
}

Motion::LengthUnit Encoder::velocity()
{
  return Motion::LengthUnit::fromCounts(1e6 * encoder_.stepRate());
}
