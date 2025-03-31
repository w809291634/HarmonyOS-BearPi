#!/bin/bash

set -e

rm -rf out/
rm -rf test/xdevice/dist/xdevice-0.0.0-py3.8.egg
rm -rf test/xts/tools/build/__pycache__/utils.cpython-38.pyc
rm -rf vendor/hisi/hi3861/hi3861/.sconsign.dblite
rm -rf vendor/hisi/hi3861/hi3861/build/build_tmp/
rm -rf vendor/hisi/hi3861/hi3861/build/scripts/__pycache__
rm -rf vendor/hisi/hi3861/hi3861/ohos/libs/
rm -rf vendor/hisi/hi3861/hi3861/output/bin/
rm -rf vendor/hisi/hi3861/hi3861/tools/nvtool/__pycache__
