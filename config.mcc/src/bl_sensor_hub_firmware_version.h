#ifndef BL_SENSOR_HUB_FIRMWARE_VERSION_H
#define BL_SENSOR_HUB_FIRMWARE_VERSION_H

//This header is special.
//This header controls the PIC32 firmware version.
//This header also controls the value the ESP32 expects PIC32 version to be.

//If the ESP32 detects a PIC32 version that does not match these,
//it will flash the PIC32 to update it to this version.

//Changing these values requires rebuilding the PIC32 firmware,
//which requires rebuilding the ESP32 firmware.

#define SENSOR_HUB_VERSION_MAJOR 0
#define SENSOR_HUB_VERSION_MINOR 0
#define SENSOR_HUB_VERSION_MICRO 2

#endif /* BL_SENSOR_HUB_FIRMWARE_VERSION_H */

