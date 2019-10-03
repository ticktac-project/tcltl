#!/bin/sh
set -x -e
git clone https://github.com/catchorg/Catch2.git
mkdir -p Catch2/build
cd Catch2/build
cmake ..
make
sudo PATH="$PATH" make install
cd ../..
rm -rf Catch2
