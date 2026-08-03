#ifndef BL_FLETCHER16_H
#define BL_FLETCHER16_H

#include <cstdint>

#define FLETCHER_MOD 255
class fletcher16
{
    public:
        void reset()
        {
            state1 = 0;
            state2 = 0;
        }

        fletcher16(void)
        {
            reset();
        }

        fletcher16(const uint8_t *data, int count)
        {
            fletcher16();
            update(data, count);
        }

        void update(const uint8_t *data, int count)
        {
           while(count)
           {
              state1 = ((uint32_t)state1 + *data) % FLETCHER_MOD;
              state2 = ((uint32_t)state2 + state1) % FLETCHER_MOD;
              data++;
              count--;
           }
        }

        void update(const uint8_t data)
        {
           update(&data, 1);
        }

        //Emit the checksum value
        uint16_t finish(void)
        {
           return (state2 << 8) | state1;
        }

        //Emit a modified checksum value, so that a checksum including this value == 0
        uint16_t gencheck(void)
        {
            uint32_t csum = finish();

            uint32_t f0 = csum & 0xff;
            uint32_t f1 = (csum >> 8) & 0xff;
            uint32_t c0 = 0xff - ((f0 + f1) % FLETCHER_MOD);
            uint32_t c1 = 0xff - ((f0 + c0) % FLETCHER_MOD);
            //Pack so we can transmit from a little-endian CPU.
            //Everything on ChessUp2 is little-endian.
            return (uint16_t)((c1 << 8) | c0);
        }

        bool check(void)
        {
            return finish() == 0;
        }

    private:
        uint8_t state1 = 0;
        uint8_t state2 = 0;
};

#endif

