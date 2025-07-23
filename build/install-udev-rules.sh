#!/bin/bash

RULES_OLD="/etc/udev/rules.d/hsdaoh.rules"
RULES_NEW="/etc/udev/rules.d/71-hsdaoh.rules"
RULES_TMP="/tmp/71-hsdaoh.rules"
SKIP_UDEV_INSTALL="no"
USERNAME=$(id -u -n)

echo "This script will install udev rules to be able to run misrc_capture without root/sudo and optionally add the current user to the plugdev group. "
echo "This script may ask for root permission to be able to install the rules."
echo ""
read -p "Would you like to proceed? (y/n) " -n 1 -r
echo ""
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
  exit 1
fi

if [ -e $RULES_OLD ]; then
  read -p "An older version of the udev rules were found, upgrade? (y/n) " -n 1 -r
  echo ""
  if [[ $REPLY =~ ^[Yy]$ ]]; then
    sudo rm "$RULES_OLD"
  else
    SKIP_UDEV_INSTALL="yes"
  fi
fi

if [ -e $RULES_NEW ]; then
  read -p "Udev rules were found, reinstall? (y/n) " -n 1 -r
  echo ""
  if [[ $REPLY =~ ^[Yy]$ ]]; then
    sudo rm "$RULES_NEW"
  else
    SKIP_UDEV_INSTALL="yes"
  fi
fi

if [[ ! "$SKIP_UDEV_INSTALL" == "yes" ]]; then
  cat > "$RULES_TMP" << "EOF"
#
# Copyright 2024 Steve Markgraf <steve@steve-m.de>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# MS2130
SUBSYSTEMS=="usb", ATTRS{idVendor}=="345f", ATTRS{idProduct}=="2130", ENV{ID_SOFTWARE_RADIO}="1", MODE="0660", GROUP="plugdev", TAG+="uaccess"

# MS2130 OEM
SUBSYSTEMS=="usb", ATTRS{idVendor}=="534d", ATTRS{idProduct}=="2130", ENV{ID_SOFTWARE_RADIO}="1", MODE="0660", GROUP="plugdev", TAG+="uaccess"

# MS2131
SUBSYSTEMS=="usb", ATTRS{idVendor}=="345f", ATTRS{idProduct}=="2131", ENV{ID_SOFTWARE_RADIO}="1", MODE="0660", GROUP="plugdev", TAG+="uaccess"
EOF
  sudo cp "$RULES_TMP" "$RULES_NEW"
  rm "$RULES_TMP"
fi

if id -nG "$USERNAME" | grep -qw "plugdev"; then
  echo "$USERNAME is already a member of the plugdev group, should be able to use misrc_capture without sudo."
else
  read -p "$USERNAME is not a member of the plugdev group, should $USERNAME be added the group? (only memebers of that group can use misrc_capture without root) (y/n) " -n 1 -r
  echo ""
  if [[ $REPLY =~ ^[Yy]$ ]]; then
    sudo usermod -a -G plugdev "$USERNAME"
  fi
fi

echo "For the new rule and/or the group membership to become active, you may have to restart your system."
