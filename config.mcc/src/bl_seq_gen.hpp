#ifndef BL_SEQ_GEN_HPP
#define BL_SEQ_GEN_HPP

#include <cstdint>

struct Gen {
  Gen(uint32_t seed = 1);

  // Generate the next value
  uint8_t gen();

  void sync(uint8_t byte);

  // Total values generated
  unsigned getCount() const;

  // Return the last value generated
  uint8_t getLast() const;

  private:
    unsigned pos = 0;
    unsigned count = 0;
    uint32_t seed;
    uint8_t last;
};

#endif // BL_SEQ_GEN_HPP
