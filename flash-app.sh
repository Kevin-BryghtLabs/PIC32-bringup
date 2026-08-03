#!/bin/bash

cmdfile=$(mktemp "/tmp/jlink-XXXXXX")

echo "erase 0x2000 0x10000" >> ${cmdfile}
echo "loadfile ${HOME}/pic32-out/flash.hex" >> ${cmdfile}

cat ${cmdfile}

JLinkExe -device PIC32CM6408 -if SWD -speed 4000 -CommandFile ${cmdfile}

rm ${cmdfile}
