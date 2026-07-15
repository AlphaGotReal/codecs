#! /usr/bin/env bash

if [ "$EUID" -ne 0 ]; then
  echo "Please run this script as root (e.g., sudo ./setup_ffmpeg.sh)"
  exit 1
fi

if [ -f "/usr/bin/ffmpeg" ]; then
  echo "==========================================================="
  echo "ERROR: An existing FFmpeg installation was detected."
  echo "Location: /usr/bin/ffmpeg"
  echo ""
  echo "To avoid library conflicts with the Jellyfin build and your C code,"
  echo "you must remove the system FFmpeg first."
  echo ""
  echo "Run this command to remove it:"
  echo "    sudo apt purge ffmpeg"
  echo ""
  echo "After removing it, run this script again."
  echo "==========================================================="
  exit 1
fi

echo "No conflicting FFmpeg found. Proceeding with Jellyfin FFmpeg setup..."

curl -fsSL https://repo.jellyfin.org/jellyfin_team.gpg.key | gpg --dearmor --yes -o /etc/apt/keyrings/jellyfin.gpg
echo "deb [arch=$( dpkg --print-architecture ) signed-by=/etc/apt/keyrings/jellyfin.gpg] https://repo.jellyfin.org/ubuntu jammy main" | tee /etc/apt/sources.list.d/jellyfin.list
apt update
apt install -y jellyfin-ffmpeg6

echo "Symlinking binaries..."
ln -sf /usr/lib/jellyfin-ffmpeg/ffmpeg /usr/local/bin/ffmpeg
ln -sf /usr/lib/jellyfin-ffmpeg/ffprobe /usr/local/bin/ffprobe

echo "Symlinking development libraries and headers..."

mkdir -p /usr/local/include
ln -sf /usr/lib/jellyfin-ffmpeg/include/* /usr/local/include/

mkdir -p /usr/local/lib
ln -sf /usr/lib/jellyfin-ffmpeg/lib/*.so* /usr/local/lib/

mkdir -p /usr/local/lib/pkgconfig
ln -sf /usr/lib/jellyfin-ffmpeg/lib/pkgconfig/* /usr/local/lib/pkgconfig/

echo "Updating dynamic linker cache..."
echo "/usr/local/lib" > /etc/ld.so.conf.d/jellyfin-ffmpeg.conf
ldconfig

echo "Setup complete!"
