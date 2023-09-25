#!/bin/sh

docker build --network=host --tag ns3_ubuntu:1.00 -f Dockerfile .

