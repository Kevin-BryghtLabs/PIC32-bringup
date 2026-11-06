/*
 * File:   BL_led.h
 * Author: Justin
 *
 * Created on May 27, 2021, 12:29 PM
 */

#ifndef BL_BLE_PARSER_H
#define	BL_BLE_PARSER_H

#include <stdint.h>
#include "bl_ble_packet_headers.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Function prototypes
 *******************************************************************************/
void sendFirmwareVersion(void);
void sendTouchCcVals(void);
void DecodePacketData(const uint8_t bleData[],int dataLength);

#ifdef __cplusplus
}
#endif

#endif	/* BL_LED_H */

