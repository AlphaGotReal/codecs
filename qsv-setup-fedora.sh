#!/usr/bin/bash
set -e

dnf upgrade -y
dnf install -y \
    libva-utils \
    intel-gpu-tools \
    libva-intel-media-driver

ls -l /dev/dri/
LIBVA_DRIVER_NAME=iHD vainfo --display drm --device /dev/dri/renderD129
