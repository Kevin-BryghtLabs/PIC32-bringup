#ifndef BL_UTIL_H
#define BL_UTIL_H

#include <stdint.h>
#include <string.h>

static uint16_t load_u16(const void *p)
{
    uint16_t ret;
    memcpy(&ret, p, sizeof(ret));
    return ret;
}

#endif // BL_UTIL_H
