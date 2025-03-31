#!/bin/bash

set -e

rm -f /mnt/hgfs/ubuntu_share/Hi3861_wifiiot_app_allinone.bin  
python build.py BearPi-HM_Nano  
cp -rf out/BearPi-HM_Nano/Hi3861_wifiiot_app_allinone.bin /mnt/hgfs/ubuntu_share/Hi3861_wifiiot_app_allinone.bin
