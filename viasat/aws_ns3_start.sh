#!/bin/bash

DIRECTORY="inmarsat-iab-releases"
ZIPFILE="inmarsat-iab-releases.zip"
cd /home/ubuntu
if [ ! -d "$DIRECTORY" ]; then
  echo "$DIRECTORY does not exist."

  while ! [ -f inmarsat-iab-releases.zip ] ;
  do
      sleep 10
  done

  size=$(stat -c%s "inmarsat-iab-releases.zip")
  while [ "$size" -lt 88000000 ] ;
  do	 
    size=$(stat -c%s "inmarsat-iab-releases.zip")
    echo "File size is: $size"
    sleep 10
  done

  unzip inmarsat-iab-releases.zip
fi

cd inmarsat-iab-releases/

./ns3 clean 

./ns3 configure --build-profile=optimized --enable-modules=mmwave

./ns3 

python3 sem-run-simulations.py > output.txt 2>&1 &

python3 sem-run-simulations.py > output_remaining.txt 2>&1 &

#python3 sem-parse-results.py
