#include "hub_log.h"
#include "bl_ble_packet_headers.h"
#include "bl_uart.h"
#include <stdarg.h>
#include <stdio.h>

// Packet must fit in a uint8_t length
static char buffer[128];

int hubLog(char type, const char * fmt, ...) {
  va_list args;

  va_start(args, fmt);
  int res = vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  if (res >= 0) {
    unsigned length = res;
    if (length >= sizeof(buffer)) {
      length = sizeof(buffer) - 1;
    }
    // Send the null byte
    length += 1;
    sendGenericStart(DEBUG_MESSAGE, length + 1);
    sendGenericData1(type);
    sendGenericData(buffer, length);
    sendGenericDone();
  }
}

