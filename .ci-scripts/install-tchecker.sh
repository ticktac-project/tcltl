#!/bin/sh
set -x -e
git clone https://github.com/ticktac-project/tchecker.git
mkdir -p tchecker/build
cd tchecker/build
cmake .. -DLIBTCHECKER_ENABLE_SHARED=1
make
sudo PATH="$PATH" make install
sudo ldconfig
cd ../..
rm -rf tchecker
