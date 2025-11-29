#!/bin/bash

CWD=$(pwd)
WORKSPACE="$CWD/workspace"
CFLAGS="-I$WORKSPACE/include"
LDFLAGS="-L$WORKSPACE/lib"
export PATH="${WORKSPACE}/bin:$PATH"
PKG_CONFIG_PATH="$WORKSPACE/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/local/lib/pkgconfig"
export PKG_CONFIG_PATH

if [[ ("$OSTYPE" == "darwin"*) ]]; then
  export MACOSX_DEPLOYMENT_TARGET=10.15
  export MACOS_DEPLOYMENT_TARGET=10.15
  if [[ ("$(uname -m)" == "arm64") ]]; then
    export MACOSX_DEPLOYMENT_TARGET=11.0
    export MACOS_DEPLOYMENT_TARGET=11.0
  fi
fi

mkdir -p "$WORKSPACE"

if [[ ("$OSTYPE" != "cygwin"*) && ("$OSTYPE" != "msys"*) ]]; then
  curl -L --silent -o pkg-config-0.29.2.tar.gz "https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz"
  tar xzf pkg-config-0.29.2.tar.gz
  cd pkg-config-0.29.2
  if [[ "$OSTYPE" == "darwin"* ]]; then
    export CFLAGS="-Wno-error=int-conversion"
  fi
  ./configure --silent --prefix="${WORKSPACE}" --with-pc-path="${WORKSPACE}"/lib/pkgconfig --with-internal-glib
  make
  make install
  cd ../
fi

if [[ ("$OSTYPE" == "darwin"*) || ("$OSTYPE" == "cygwin"*) || ("$OSTYPE" == "msys"*) ]]; then
  curl -L --silent -o "libusb-1.0.29.tar.bz2" "https://github.com/libusb/libusb/releases/download/v1.0.29/libusb-1.0.29.tar.bz2"
  tar xjf libusb-1.0.29.tar.bz2
  cd libusb-1.0.29
  ./configure --prefix="${WORKSPACE}" --disable-shared --enable-static
  make
  make install
  cd ../
fi

if [[ ("$OSTYPE" == "cygwin"*) || ("$OSTYPE" == "msys"*) ]]; then
  curl -L --silent -o "libuvc-41d0e0403abc5356e6aaeda690329467ef8f3a31.tar.gz" "https://github.com/steve-m/libuvc/archive/41d0e0403abc5356e6aaeda690329467ef8f3a31.tar.gz"
  tar xzf libuvc-41d0e0403abc5356e6aaeda690329467ef8f3a31.tar.gz
  cd libuvc-41d0e0403abc5356e6aaeda690329467ef8f3a31
else
  curl -L --silent -o "libuvc-0.0.7.tar.gz" "https://github.com/libuvc/libuvc/archive/refs/tags/v0.0.7.tar.gz"
  tar xzf libuvc-0.0.7.tar.gz
  cd libuvc-0.0.7
fi
# now we have to edit the cmake file...
sed "s/BUILD_UVC_SHARED TRUE/BUILD_UVC_SHARED FALSE/g" CMakeLists.txt >CMakeLists.patched
rm CMakeLists.txt
sed "s/find_package(JpegPkg QUIET)//g" CMakeLists.patched >CMakeLists.txt
mkdir build
cd build
cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DJPEG_FOUND=False -DBUILD_EXAMPLE=False -DBUILD_TEST=False -DCMAKE_INSTALL_PREFIX="${WORKSPACE}" ../
if [[ ("$OSTYPE" == "cygwin"*) || ("$OSTYPE" == "msys"*) ]]; then
  cmake --build .
  cmake --install .
else
  make
  make install
fi
cd ../../

curl -L --silent -o "flac-1.5.0.tar.xz" "https://github.com/xiph/flac/releases/download/1.5.0/flac-1.5.0.tar.xz"
tar xf flac-1.5.0.tar.xz
cd flac-1.5.0
./configure --disable-shared --enable-static --disable-ogg --disable-programs --disable-examples --prefix="${WORKSPACE}"
make
make install
cd ../

curl -L --silent -o "soxr-437e06c739eb825f229b58fa50a565f33f82cbd3.tar.gz" "https://github.com/Stefan-Olt/soxr/archive/437e06c739eb825f229b58fa50a565f33f82cbd3.tar.gz"
tar xzf soxr-437e06c739eb825f229b58fa50a565f33f82cbd3.tar.gz
cd soxr-437e06c739eb825f229b58fa50a565f33f82cbd3
mkdir build
cd build
cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DBUILD_SHARED_LIBS=OFF -DWITH_OPENMP=OFF -DCMAKE_INSTALL_PREFIX="${WORKSPACE}" ../
if [[ ("$OSTYPE" == "cygwin"*) || ("$OSTYPE" == "msys"*) ]]; then
  cmake --build .
  cmake --install .
else
  make
  make install
fi
cd ../../

curl -L --silent -o "hsdaoh-ecd5f835ffad911e7b0b73d905e70cddc898c1ab.tar.gz" "https://github.com/Stefan-Olt/hsdaoh/archive/ecd5f835ffad911e7b0b73d905e70cddc898c1ab.tar.gz"
tar xzf hsdaoh-ecd5f835ffad911e7b0b73d905e70cddc898c1ab.tar.gz
cd hsdaoh-ecd5f835ffad911e7b0b73d905e70cddc898c1ab
# I cannot get cmake to not build the shared library
sed "s/SHARED/STATIC/g" ./src/CMakeLists.txt >./src/CMakeLists.txt.patched
rm ./src/CMakeLists.txt
cat ./src/CMakeLists.txt.patched | tr '\n' '\r' | sed -e 's/executables.*\r# Install/\r# Install/' | sed -e 's/install(TARGETS hsdaoh_file.*)//' | tr '\r' '\n' > ./src/CMakeLists.txt
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX="${WORKSPACE}" -DINSTALL_UDEV_RULES=False ../
if [[ ("$OSTYPE" == "cygwin"*) || ("$OSTYPE" == "msys"*) ]]; then
  cmake --build .
  cmake --install .
else
  make
  make install
fi
cd ../../

cd ../misrc_tools
meson setup ../build/misrc --prefix="${WORKSPACE}" --buildtype=release --default-library=static --libdir="${WORKSPACE}"/lib
ninja -C ../build/misrc
ninja -C ../build/misrc install
