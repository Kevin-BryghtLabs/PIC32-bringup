/*
 * File:   BL_ble_packet_headers.h
 * Author: Richard
 */

#ifndef BL_BLE_PACKET_HEADERS_H
#define	BL_BLE_PACKET_HEADERS_H

/*******************************************************************************
 * Chessup Packet header IDs.
 *
 * We want to keep these consistent, when possible, between ChessUp1 and ChessUp2.
 *
 * We want to keep them consistent across processors as well,
 * both ChessUp1(SamD54<->nRF52) and ChessUp2(SamL10<->ESP32).
 *
 * There are a few times noted below when the same ID is used
 * twice, but usually in different directions.
 *
 * Additionally, a few IDs are reserved to try to avoid ambiguity
 * with the microchip bootloader protocol.
 *
 *******************************************************************************/

enum
{
    COLOR_INFORMATION_START     = 0x00, //app2dev
    COLOR_INFORMATION_END       = 0x20, //app2dev
    RESPONSE_OKAY               = 0x21,
    RECEIVED_MOVE_OK            = 0x22, //  Have received move from phone
    PROMOTION_OK                = 0x23,
    START_GAME_OK               = 0x24,
    MOVES_COLORS_OK             = 0x25,
    ERROR_CODE                  = 0x26,

    //This block(0x30->0x36) is used on ChessUp1 between CPUs.
    NRF52_HEADER                = 0x30,
    BATT_CHARGE                 = 0x31,
    BLE_STATUS                  = 0x32, //Request the ble status from the nrf52
    BATT_UPDATE                 = 0x33,
    NRF52_FW_VER                = 0x34, //Both a request(single header byte) and response(with version info)
    NRF52_SLEEP                 = 0x36,

    TOUCH_CALIBRATION_FAILED    = 0x4a,
    ALIAS_BL_GUARD              = 0x4d, // first byte of bootloader command guard. This header is reserved since it may be confused with bootloader requests.
    TOUCH_CALIBRATION_DONE      = 0x4f,

    //dev2app: The following 0x50->0x54 block is reserved for bootloader responses
    //These should only occur in bootloader mode, but try to avoid them.
    ALIAS_BL_RESP_OK            = 0x50,
    ALIAS_BL_RESP_ERROR         = 0x51,
    ALIAS_BL_RESP_INVALID       = 0x52,
    ALIAS_BL_RESP_CRC_OK        = 0x53,
    ALIAS_BL_RESP_CRC_FAIL      = 0x54,

    //Note that some of these are aliased with bootloader responses
    CHESS_COM_MODE              = 0x50, //app2dev: Chess.com mode
    CHESS_COM_POSITION          = 0x51, //app2dev: Receive board position
    GAME_END_TYPE               = 0x52,
    DATA_FEED_BASELINE_ACK      = 0x53, //dev->PC acknowledge baseline received
    USB_FEED_DATA_MODE          = 0x54, //PC tool -> device app mode change request
    HEROKU_PACKET               = 0x55,

    //app2dev
    SEND_TOUCH_FILTER_CFG       = 0x60, //Send touch filter config
    SEND_TOUCH_BLE              = 0x61,
    TOUCH_DIFF_BASE             = 0x62,
    TOUCH_DIFF_BASE_OFF         = 0x63,

    //
    CU_RESET_GAME               = 0x64,
    MOVE_COLORS                 = 0x65, // Colors for the quality of moves
    FEN_STRING                  = 0x66, // App send board a FEN string to start a game
    SEND_BOARD_STATE            = 0x67, // App requests the board state
    STOP_TOUCH_BLE              = 0x68,
    SEND_TOUCH_BASELINE         = 0x69,
    QUIET_FEN                   = 0x6A, // Similar to the FEN string load but without all the flashing
    PULL_RECENT_MOVES           = 0x6B, // Used by the app to retrieve recent move made in a game
    SEND_POSITION               = 0x6C, // Send the board state

    //WIFI Section
    WIFI_STATUS                 = 0x70, //app2dev: Empty  => status request, dev2app: Filled => status response or update
    WIFI_SCAN                   = 0x71, /*  app2dev: Empty  => Scan Request
                                            dev2app: Filled => 1 scan result
                                            dev2app: Empty  => End of scan results */
    WIFI_CONNECT                = 0x72,
    WIFI_RESET                  = 0x73, // app2dev: Empty => reset all. content=SSID => reset one.
    AUTH_STATUS                 = 0x74, // 0B: request. 4B: uint32_t bitfield: 16x2bits(for each CRED_* below)
    CRED_CHESSCOM               = 0x75,
    CRED_CHESSUP                = 0x76,
    CRED_LICHESS                = 0x77,
    CRED_TIMEZONE               = 0x78, // TimeZone JSON, sent in credential-protocol

    LESSON_BLINK_LIGHT          = 0x80, // A command used for lessons to blink lights
    LESSON_ROLL_LIGHT           = 0x81, // A command used for lessons to roll out the lights


    TEXT_ENTRY_STATE            = 0x82, //1B header, 1B text-type, optional(UTF8 text, NULL-terminated)
    TEXT_ENTRY_DONE             = 0x83, //1B header, 1B text-type, optional(UTF8 text, NULL-terminated)
    PALLETE_UPDATE2             = 0x84, //Replacement for PALLETE_UPDATE(Workaround ChessUp1 not parsing BLE packet size)
    SETLEDS                     = 0x85, // List of [IndexU8, RedU8, GreenU8, BlueU8]
    CLEAR_POSITIONS             = 0x86, // Debug clear the light from piece positions
    PIECE_POSITIONS             = 0x87, // Debug turn lights on where pieces are
    PALLETE_UPDATE              = 0x88, // Various formats
    TEST_MODE                   = 0x89, // Turns lights off but chess game still works
    GAME_MODE                   = 0x8A,
    LESSON_MODE                 = 0x8B,
    LESSON_END_LIGHTS           = 0x8C,
    LIVE_LIGHT                  = 0x8D,
    ON_TOUCH_COLORS             = 0x8E,
    ERASE_ON_TOUCH              = 0x8F,

    TOUCH_EVENTS_FLAG           = 0x90,
    PROMOTION_RECOMMEND         = 0x96,
    PAWN_PROMOTION              = 0x97,
    SET_AI                      = 0x98,
    AI_MOVED                    = 0x99,
    BOOK_SET                    = 0x9A, // The tell the board to light up the book icon or turn it off

    BLE_TOUCH                   = 0xA2, //dev2app: Send the location of the piece touched
    BLE_MOVE                    = 0xA3,
    PIECE_PLACE                 = 0xA4,

    START_POS_STATUS            = 0xB0,
    FEN_COMPLETE                = 0xB1,
    BOARD_INFO                  = 0xB2, //dev2app: board info, i.e. FW version, etc
    ASSISTANCE                  = 0xB3,
    WHITE_SETTINGS              = 0xB3, // I don't think this is used but check with app team
    BLACK_SETTINGS              = 0xB4,
    WIN_ON_TIME                 = 0xB6,
    TOUCH_LIMIT                 = 0xB7, // This is temporary we will make this selectable per player and put it in the white and black setting above
    TOUCH_LIMIT_DATA            = 0xB8,
    GAME_SETTING                = 0xB9,
    GAME_HISTORY                = 0xBA,
    TOUCH_RELEASE               = 0xBB,
    END_GAME_STATUS             = 0xBC, // Header for telling the phone that the game ended with a draw/resignation
    UNDO_MOVE                   = 0xBD, // Header for sending undo move to the phone
    GET_BATTERY_LVL             = 0xBE, // Command from phone to request battery level.
    CLEAR_GAME_HISTORY          = 0xBF, // Clear the store game history

    BLE_CONNECTED               = 0xC2, // Reserved for compatibility with ChessUp1, which used it as a message on PCB.
    REJECT_CONNECTION           = 0xC3, // Reserved for compatibility with ChessUp1, which used it as a message on PCB.
    BLE_DISCONNECTED            = 0xC4, // Reserved for compatibility with ChessUp1, which used it as a message on PCB.
    PHONE_REQUEST_CONN          = 0xC5, // Reserved for compatibility with ChessUp1, which used it as a message on PCB.
    NRF_FW_UPDATE_START         = 0xC6, // Reserved for compatibility with ChessUp1, which used it as a message on PCB.
    NRF_FW_UP_COMP              = 0xC7, // Reserved for compatibility with ChessUp1, which used it as a message on PCB.
    NRF_FW_VESION               = 0xC8, // Reserved for compatibility with ChessUp1, which used it as a message on PCB.
    REQUEST_BOARD_INFO          = 0xC9,
    REQUEST_SCREEN_CAPTURE      = 0xCA,

    ENABLE_DATA_STREAMING       = 0xDA, // Header to enable touch data streaming
    BL_REGION                   = 0xDB, // Header for tell the app which bootloader is being used
    BOOTLOADER_UPDATE           = 0xDC,
    FIRMWARE_UPDATE             = 0xDD,
    CALIBRATE                   = 0xDE,
    RECALIBRATE                 = 0xDF,

    UI_ADVANCE                  = 0xE0,
    PHONE_CHARGE                = 0xE1,
    PHONE_NOCHARGE              = 0xE2,
    ALS_OFF                     = 0xE3,
    ALS_ON                      = 0xE4,
    ALS_STREAM_ON               = 0xE5,
    ALS_STREAM_OFF              = 0xE6,
    USER_SETTINGS               = 0xE7,
    FACTORY_CONTROL             = 0xE8,
    UI_CONTROL                  = 0xE9,

    PCAP_BLE_BOARD_RX           = 0xF0, //PacketCapture
    PCAP_BLE_BOARD_TX           = 0xF1,

    ECHO_TEST                   = 0xF2, //Send this packet back, minus ECHO_TEST header
    PROFILE_STATUS              = 0xF3, //0B: query, 2B:[1B idx, 1B count], 18B: [1B idx|1B count|16B GUID]
    DATA_TRANSFER               = 0xF4,

    TOUCH_CC_VALS               = 0xF8, //Compensation capacitor. 0B: query. 2x uint16_t: {square, val}, 2B x 64: full set
    PIECE_ID_DATA               = 0xF9,
    PIECE_ID_CFG                = 0xFA,
    PIECE_ID_FREQS              = 0xFB,
    SEND_DATA_FILTERED          = 0xFD, //Touch data streaming, filtered
    DATA_FEED_BASELINE          = 0xFC, //Baseline data
    SEND_DATA                   = 0xFE, //Touch data streaming, raw

    APP2DEV_COUNT               = 0x100 //No more than 256 packet IDs may exist, as they need to fit in 1-byte
};

//SEND_POSITION 0x6C
typedef enum
{
   BIT_BOARD = 0x01,
}BOARD_POSITION;
#endif	/* BL_BLE_PACKET_HEADERS_H */

