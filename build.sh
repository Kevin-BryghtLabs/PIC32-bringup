#!/bin/bash

set -e

/usr/bin/cmake --build _build/pic32-test/default/ -v --parallel

python postProcess.py \
  --inHex out/pic32-test/default.hex \
  --printInputSegments \
  --outAppC pic32_firmware_data.h \
  --printOutputSegments \
  --trimAppCrc \
  --pruneBocor \
  --outFlashHex ~/pic32-out/flash.hex
