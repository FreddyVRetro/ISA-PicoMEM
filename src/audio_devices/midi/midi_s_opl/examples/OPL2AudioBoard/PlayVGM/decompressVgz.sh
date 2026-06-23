#!/bin/bash

# This script will decompress all *.vgz-files in a directory to *.vgm
#
# Note: Requires gzip

for file in *.vgz
do
  cp "$file" "${file%.vgz}.vgm.gz"
  gzip -d "${file%.vgz}.vgm.gz"
done