/*******************************************************************************
 * Header files includes
 *******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "bl_ble_parser.h"
//#include "BL_board_utils.h"
//#include "BL_delay.h"
//#include "BL_app_state.h"
//#include "BL_board_model_ids.h"
//#include "BL_piece_id.h"
//#include "BL_piece_id_packet.h"
#include "bl_piece_id.hpp"
#include "bl_sensor_hub_firmware_version.h"
#include "bl_touch.h"
#include "bl_uart.h"

#include "timer.h" // For `delay`, can remove if we get the standard headers
#include "leds.h"

static uint32_t piece_id_tone_table[12][3];/*1x freq hz, 1x drain delays, 1x adc delays*/

/*******************************************************************************
 * Global constants
 *******************************************************************************/

//#define MAX(a, b)    ((a) > (b) ? (a) : (b))
//
//#define TX_BUFFER_SIZE

//void touchSendCCVals();
//void touchSetCCVals(const uint8_t * ccData, size_t size);
//void startTouchCalibration();

static uint16_t load_u16(const void *p)
{
    uint16_t ret;
    memcpy(&ret, p, sizeof(ret));
    return ret;
}

#if 0
void sendTouchCcVals(void)
{
    //Query for CC vals
    sendGenericStart(TOUCH_CC_VALS, sizeof(uint16_t) * 64);
    for(int sq = 0; sq < 64; sq++)
    {
      uint16_t temp = get_sensor_cc_val(sq);
      sendGenericData(&temp, sizeof(temp));
    }
    sendGenericDone();
}
#endif

void sendFirmwareVersion(void){
    static const uint8_t verData[3] = {SENSOR_HUB_VERSION_MAJOR, SENSOR_HUB_VERSION_MINOR, SENSOR_HUB_VERSION_MICRO};
    sendGeneric(NRF52_FW_VER, sizeof(verData), verData);
}

/*******************************************************************************
 * Function Name: DecodePacketData
 ********************************************************************************
 * Summary:
 *  This function parses the data that comes over BLE and calls the appropriate
 *  functions depending on the header of the BLE data.
 *
 * Parameter:
 *  bleData: The data that came over BLE on the UART
 *  dataLength: The length of the data that came over BLE
 *
 *******************************************************************************/
void DecodePacketData(const uint8_t bleData[], int dataLength) {
  // Reset board
  if(bleData[0] == CU_RESET_GAME){
    //resetGame(1);
  }

  else if(bleData[0] == BOOTLOADER_UPDATE){
    //bootloaderFirmwareUpdate();
  }
  else if (bleData[0] == FIRMWARE_UPDATE){
    delay(100);
    //firmwareUpdate();
  }
  else if (bleData[0] == SETLEDS){
    if (dataLength == 64 * 3 + 1) {
      ledUpdateAll(&bleData[1], 64 * 3);
    }
  }
  else if (bleData[0] == CALIBRATE){
    touchStartCalibration();
  }
  else if(bleData[0] == RECALIBRATE){
    touchStartCalibration();
  }
  // Used for sending touch data over BLE
  else if(bleData[0] == ENABLE_DATA_STREAMING){
//    BLEDataStream = 1;
  }
  else if(bleData[0] == NRF52_FW_VER){
    //Send FW version
    sendFirmwareVersion();
  }
#if 0
  else if(bleData[0] == PIECE_ID_CFG && dataLength >= 1)
  {
      const unsigned pidcfg_len = dataLength - 1;
      const void * pidcfg_data = bleData + 1;
      if(pidcfg_len == 0){
          //Query for configs - skip scan-tone map since ESP32 controls scan-order.
          //sendGeneric(PIECE_ID_CFG, sizeof(piece_id_scan_tones), piece_id_scan_tones);
          sendGeneric(PIECE_ID_CFG, sizeof(piece_id_blip_cfg), &piece_id_blip_cfg);
      }
      else if(pidcfg_len == sizeof(piece_id_blip_t)) {
          memcpy(&piece_id_blip_cfg, pidcfg_data, sizeof(piece_id_blip_cfg));
      }
      else if(pidcfg_len == sizeof(piece_id_scan_tones)) {
          //Full config set (8x8x16bit)
          memcpy(piece_id_scan_tones, pidcfg_data, sizeof(piece_id_scan_tones));
      }
      else if(pidcfg_len == sizeof(piece_id_cfg_tones_t)) {
          //Config bits - apply 1s to all active squares, apply 0s to all squares
          piece_id_cfg_tones_t tones;
          memcpy(&tones, pidcfg_data, sizeof(tones));
          for(unsigned rank = 0; rank < 8; ++rank) {
              for(unsigned file = 0; file < 8; ++file) {
                  uint64_t bit = 1ULL << (rank * 8 + file);
                  if(tones.bitboard & bit)
                  {
                    piece_id_scan_tones[rank][file] |= tones.tone_set;
                  }
              }
          }
      }
  }
#endif
  else if(bleData[0] == TOUCH_CC_VALS && dataLength == 1) {
    touchSendCCVals();
  }
  //else if(bleData[0] == TOUCH_CC_VALS && dataLength == 1 + DEF_NUM_SENSORS * sizeof(uint16_t)) {
  else if(bleData[0] == TOUCH_CC_VALS && dataLength > 1) {
    touchSetCCVals(&bleData[1], dataLength - 1);
  }
  else if(bleData[0] == PIECE_ID_FREQS && dataLength == 1){
    sendGeneric(PIECE_ID_FREQS, sizeof(piece_id_tone_table), &piece_id_tone_table);
  }
  else if(bleData[0] == PIECE_ID_FREQS) { // && dataLength == 1 + sizeof(piece_id_tone_table)){
    hubLog('I', "Got Freqs, dataLen %u vs %u", dataLength, 1 + sizeof(piece_id_tone_table));
    memcpy(&piece_id_tone_table, bleData + 1, sizeof(piece_id_tone_table));
    for(unsigned i = 0; i < 12; ++i){
      pieceIdSetFrequencyParams(i, piece_id_tone_table[i][0], piece_id_tone_table[i][1], piece_id_tone_table[i][2]);
    }
    pieceIdSetFrequencyCount(12);
  }
  else if(bleData[0] == UI_ADVANCE){
    sendGeneric(bleData[0], dataLength - 1, bleData + 1);
  }
}
