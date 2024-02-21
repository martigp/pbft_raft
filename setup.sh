#!/bin/bash

# Download everything necessary for protobufs
brew install abseil
mkdir -p install
git submodule add https://github.com/protocolbuffers/protobuf.git
cd protobuf
git submodule update --init --recursive
export ABSEIL_PATH=/opt/homebrew/Cellar/abseil/20230802.1
cmake . -DCMAKE_INSTALL_PREFIX=../install -DCMAKE_CXX_STANDARD=17 \
        -Dprotobuf_ABSL_PROVIDER=package -DCMAKE_PREFIX_PATH=$(ABSEIL_PATH)

cmake --build . --parallel 10
cmake --install .

# Download everything necessary for libconfig++
cd ..
git submodule add https://github.com/hyperrealm/libconfig
cd libconfig/
cmake . -DCMAKE_INSTALL_PREFIX=../install -DCMAKE_CXX_STANDARD=17
cmake --build . --parallel 10
cmake --install .
cp lib/libconfig++.pc.in ../install/lib/pkgconfig/libconfig++.pc
cd ..
mkdir -p install/runtime_libs
cd install/
cp lib/*.dylib ./runtime_libs/
export DYLD_LIBRARY_PATH=$(PWD)/runtime_libs
export PATH=$(PATH):$(PWD)/bin


