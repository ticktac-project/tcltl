#!/bin/sh
set -e -x
A='next/download?job=debian-stable-gcc'
wget -O spot.zip "https://gitlab.lrde.epita.fr/spot/spot/-/jobs/artifacts/$A"
unzip spot.zip
v=`ls spot-*.tar.*`
tar xvf "$v"
v=${v%.tar*}
cd $v
./configure --disable-static
make
# clang++ may not be on the root $PATH
sudo PATH="$PATH" make install
sudo ldconfig
cd ..
rm -rf $v
