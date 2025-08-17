#!/bin/sh
V=$(git describe --tags --dirty --match misrc_tools-*)
[ $? -eq 0 ]  || exit 1
echo "$V" | cut -c 13-
