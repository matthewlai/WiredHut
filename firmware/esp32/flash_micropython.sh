#!/bin/bash

IMAGE=esp32-20191023-v1.11-497-gf301170c7.bin
esptool.py --port /dev/ttyUSB0 erase_flash
esptool.py --port /dev/ttyUSB0 --baud 460800 write_flash -z 0x1000 $IMAGE
