#! /usr/bin/bash

dnf install -y \
    intel-media-driver \
    libva-utils \
    intel-gpu-tools \
    oneVPL-devel

usermod -aG render "$USER"
usermod -aG video "$USER"

dnf upgrade -y

vainfo --display drm --device /dev/dri/renderD128
