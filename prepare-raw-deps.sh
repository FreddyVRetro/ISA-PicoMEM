#!/bin/sh
# Run once in a clean checkout. Existing dependency directories are preserved.
set -eu
cd "$(dirname "$0")"
for dir in src/lib/tinyusb src/lib/tusb_xinput; do
 if test -e "$dir"; then
  echo "Dependency already exists; inspect rather than overwrite: $dir" >&2
  exit 1
 fi
done
git clone https://github.com/hathach/tinyusb.git src/lib/tinyusb
git -C src/lib/tinyusb checkout --detach 331c263400dd5ccf77264cab1fa6bbd9f374b44e
git -C src/lib/tinyusb apply ../../../patches/raw-usb-tinyusb.patch
git clone https://github.com/Ryzee119/tusb_xinput.git src/lib/tusb_xinput
git -C src/lib/tusb_xinput checkout --detach ae4f3a9fd7c57a6c8c36fdba7f7b1abc9b6a1a56
git -C src/lib/tusb_xinput apply ../../../patches/xinput-tinyusb-api.patch
git submodule update --init src/lib/rp2040-psram
