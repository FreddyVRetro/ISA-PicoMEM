#!/bin/sh
set -eu
cd "$(dirname "$0")"
cc=${CC:-cc}
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT HUP INT TERM
common='-std=c11 -Wall -Wextra -fsanitize=address,undefined -g'
$cc $common -Werror test-core.c ../../src/rawblock/core.c -o "$out/core"
"$out/core"
$cc $common -Werror -Istubs test-service.c ../../src/rawblock/core.c -o "$out/service"
"$out/service"
case $(uname -s) in Darwin) gc='-Wl,-dead_strip';; *) gc='-Wl,--gc-sections';; esac
$cc $common -DPM_RAW_USB_RO=1 -I. -I../../src/lib/tinyusb/src -ffunction-sections -fdata-sections $gc test-msc.c -o "$out/msc"
"$out/msc"
