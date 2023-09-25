#!/bin/sh
parent_dir="$(dirname "$(pwd)")"
echo $parent_dir

docker run -it  --network=host  -v $parent_dir:/ns3 -u $(id -u ${USER}):$(id -g ${USER}) ns3_ubuntu:1.00 /bin/bash
