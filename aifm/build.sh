#!/bin/bash

# AIFM Core.
make clean

make -j$(nproc) bin/test_tcp_array_add || { echo 'Failed to build AIFM Core.'; exit 1; }

echo 'Successfully built test_tcp_array_add.'

#make -j$(nproc) || { echo 'Failed to build AIFM Core.'; exit 1; }

# Far-Mem Snappy.
#cd snappy
#rm -rf build
#mkdir build
#cd build
#cmake -DCMAKE_BUILD_TYPE=Release .. || { echo 'Failed to build Snappy.'; exit 1; }
#make -j
#cd ..
