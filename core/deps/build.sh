#!/bin/bash
# Copyright 2022 iLogtail Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


DESTINATION=/opt/logtail/deps
LIB_DIR=$DESTINATION/lib
LIB64_DIR=$DESTINATION/lib64

enter_cmake_build_dir()
{
    # rm -rf build-static
    mkdir build-static
    cd build-static
}

remove_old_lib()
{
    sudo rm -f $LIB_DIR/$1*
    sudo rm -f $LIB64_DIR/$1*
}

# gtest
remove_old_lib libgtest
remove_old_lib libgmock
rm -rf googletest-release-1.8.1
tar xf gtest-1.8.1.tar.gz
cd googletest-release-1.8.1
enter_cmake_build_dir
cmake -DCMAKE_INSTALL_PREFIX=$DESTINATION -DCMAKE_BUILD_TYPE=Release ..
make -j24
sudo make install
sudo ln -s $LIB64_DIR/libgtest.a $LIB_DIR/libgtest.a
cd ../..

# protobuf
remove_old_lib libprotobuf
rm -rf protobuf-3.6.1
tar xf protobuf-cpp-3.6.1.tar.gz
cd protobuf-3.6.1
ln -s `pwd`/../googletest-release-1.8.1 gtest
./configure --prefix=$DESTINATION --disable-shared --enable-static
make -j24
sudo make install
cd ..

# re2
remove_old_lib libre2
rm -rf re2-2018-09-01
tar xf re2-2018-09-01.tar.gz
cd re2-2018-09-01
enter_cmake_build_dir
cmake -DCMAKE_INSTALL_PREFIX=$DESTINATION -DCMAKE_BUILD_TYPE=Release ..
make -j24
sudo make install
cd ../..

# tcmalloc
remove_old_lib libtcmalloc
rm -rf gperftools-gperftools-2.7
tar xf gperftools-2.7.tar.gz
cd gperftools-gperftools-2.7
./autogen.sh
./configure --prefix=$DESTINATION --disable-shared --enable-static
make -j24
sudo make install
cd ..

# cityhash
remove_old_lib libcityhash
cd cityhash
sudo make clean
./configure --prefix=$DESTINATION --disable-shared --enable-static
make -j24
sudo make install
cd ..

# gflags
remove_old_lib libgflags
rm -rf gflags-2.2.1
tar xf gflags-2.2.1.tar.gz
cd gflags-2.2.1
enter_cmake_build_dir
cmake -DCMAKE_INSTALL_PREFIX=$DESTINATION -DCMAKE_BUILD_TYPE=Release ..
make -j24
sudo make install
cd ../..

# jsoncpp
remove_old_lib libjsoncpp
rm -rf jsoncpp-1.8.4
tar xf jsoncpp-1.8.4.tar.gz
cd jsoncpp-1.8.4
enter_cmake_build_dir
cmake -DCMAKE_INSTALL_PREFIX=$DESTINATION -DCMAKE_BUILD_TYPE=Release ..
make -j24
sudo make install
sudo ln -s $LIB64_DIR/libjsoncpp.a $LIB_DIR/libjsoncpp.a
cd ../../

# boost
remove_old_lib libboost
rm -rf boost_1_68_0
tar xf boost_1_68_0.tar.gz
cd boost_1_68_0
./bootstrap.sh --prefix=$DESTINATION
sudo env PATH=$PATH ./b2 install -j24
cd ..

# lz4
remove_old_lib liblz4
rm -rf lz4-1.8.3
tar xf lz4-1.8.3.tar.gz
cd lz4-1.8.3
make -j24
sudo make prefix=$DESTINATION install
cd ..

# zlib
remove_old_lib libz
rm -rf zlib-1.2.11
tar xf zlib-1.2.11.tar.gz
cd zlib-1.2.11
./configure --prefix=$DESTINATION --static
make -j24
sudo make install
cd ..

# curl
remove_old_lib libcurl
rm -rf curl-7.61.1
tar xf curl-7.61.1.tar.gz
cd curl-7.61.1
./configure --prefix=$DESTINATION --disable-shared --enable-static CFLAGS="-std=c90 -D_POSIX_C_SOURCE -Werror=implicit-function-declaration"
make -j24
sudo make install
cd ..

# unwind
remove_old_lib libunwind
cd libunwind-1.2.1
sudo make clean
./autogen.sh
./configure --prefix=$DESTINATION --disable-shared --enable-static
make -j24
sudo make install
