#!/bin/bash

sudo apt update -y
sudo apt install python3 python3-dev pkg-config sqlite3 cmake python3-pip -y
sudo apt install g++ -y
sudo apt install libjpeg-dev zlib1g-dev dvipng jq unzip texlive-latex-base cm-super -y

python3 -m pip install sem numpy matplotlib latex scienceplots tikzplotlib 
