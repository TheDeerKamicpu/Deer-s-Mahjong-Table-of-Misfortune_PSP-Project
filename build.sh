#!/usr/bin/env bash

set -e


export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PSPDEV/bin:$PATH"


if ! command -v psp-config >/dev/null 2>&1; then
    echo "ERROR: psp-config not found"
    exit 1
fi


if ! python3 -c "import PIL" >/dev/null 2>&1; then
    python3 -m pip install --user Pillow
fi


if ! command -v ffmpeg >/dev/null 2>&1; then
    sudo apt-get update
    sudo apt-get install -y ffmpeg
fi


python3 tools/build_assets.py


make clean

make

make package


echo ""
echo "========================================"
echo "DEER'S MAHJONG PSP BUILD COMPLETE"
echo "========================================"
echo ""
echo "COPY THIS FOLDER TO:"
echo "PPSSPP/memstick/PSP/GAME/"
echo ""
echo "dist/DEERSMAHJONG"
echo ""