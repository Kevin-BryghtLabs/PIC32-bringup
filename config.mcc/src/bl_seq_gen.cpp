#include "bl_seq_gen.hpp"

Gen::Gen(uint32_t seed) : seed(seed) {}

uint8_t Gen::gen() {
  if (pos % 256 == 0) {
    last = 0x40 | ((pos >> 6) & 0x3F);
  }
  else {
    last = pos & 0x3F;
  }
  pos++;
  count++;

  return last;
}

void Gen::sync(uint8_t data) {
  pos--;
  if ((data & 0xC0) == 0) {
    // Normal counting byte, set lower 6 bits of pos
    pos = (pos & ~0x3F) | data;
  }
  else if ((data & 0xC0) == 0x40) {
    // Sentinel byte, set next 6 bits of pos
    pos = (pos & ~0x0FC0) | (data << 6);
  }
  else {
    // Not a generator byte; can't synt to this
  }
  pos++;
}

unsigned Gen::getCount() const {
  return count;
}

uint8_t Gen::getLast() const {
  return last;
}
