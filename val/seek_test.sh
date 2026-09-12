#! /usr/bin/bash

N=$1
echo "number of frames: $N"
echo " "

for value in 250 100 60 50 30 20 10 3; do
  echo "seek test with GOP=$value"
  video="../vids/test_decode_$value.mp4"
  ./pyav_loader.py --video $video -n $N
  echo $(ls -l "$video")
  echo " "
done

for value in 250 100 60 50 30 20 10 3; do
  echo "seek test with GOP=$value"
  video="../vids/test_decode_$value.mp4"
  ./pyav_nvdec_loader.py --video $video -n $N
  echo $(ls -l "$video")
  echo " "
done
