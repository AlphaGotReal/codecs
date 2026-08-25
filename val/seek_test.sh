#! /usr/bin/bash

N=$1
echo "number of frames: $N"

for value in 250 100 60 50 30 20 10 3; do
  echo "seek test with GOP=$value"
  ./pyav_loader.py --video "../vids/test_decode_$value.mp4" -n $N
done
