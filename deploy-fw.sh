#!/bin/bash

hexFile=out/pic32-test/default.hex
localHeaderName=pic32_firmware_data.h
espHeaderName=BL_sensor_hub_firmware_data.h
espPath=/home/kevin/source/CUL/main/ESP32/rapid/components/bl_sensor_hub_firmware/

if [[ ! -f "${hexFile}" ]]; then
  echo "No hex file found (are you in the project directory)?" > /dev/stderr
  exit 1;
fi

set -e

python postProcess.py \
  --inHex ${hexFile} \
  --printInputSegments \
  --outAppC ${localHeaderName} \
  --printOutputSegments \
  --trimAppCrc \
  --pruneBocor

cp ${localHeaderName} ${espPath}/${espHeaderName}

