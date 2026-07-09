#! /usr/bin/bash

apt install intel-media-va-driver-non-free vainfo intel-gpu-tools
usermod -aG render $USER
usermod -aG video $USER

wget -qO - https://repositories.intel.com/gpu/intel-graphics.key | \
  sudo gpg --dearmor --output /usr/share/keyrings/intel-graphics.gpg

apt update
apt upgrade

vainfo --display drm --device /dev/dri/renderD128
