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

if [[ ("$OSTYPE" == "cygwin"*) ]]; then
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
if [[ ("$OSTYPE" == "cygwin"*) ]]; then
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

curl -L --silent -o "FFmpeg-n7.1.1.tar.gz" "https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n7.1.1.tar.gz"
tar xzf FFmpeg-n7.1.1.tar.gz
cd FFmpeg-n7.1.1
./configure --enable-static --disable-shared --disable-programs --enable-gpl --enable-version3 --disable-avdevice --disable-avcodec --disable-avformat --disable-swscale --disable-postproc --disable-avfilter --disable-doc --prefix="${WORKSPACE}"
make
make install
cd ../

curl -L --silent -o "hsdaoh-9ef881d8904eac22a832186d78a25e53365095cd.tar.gz" "https://github.com/Stefan-Olt/hsdaoh/archive/9ef881d8904eac22a832186d78a25e53365095cd.tar.gz"
tar xzf hsdaoh-9ef881d8904eac22a832186d78a25e53365095cd.tar.gz
cd hsdaoh-9ef881d8904eac22a832186d78a25e53365095cd
# I cannot get cmake to not build the shared library
sed "s/SHARED/STATIC/g" ./src/CMakeLists.txt >./src/CMakeLists.txt.patched
rm ./src/CMakeLists.txt
cat ./src/CMakeLists.txt.patched | tr '\n' '\r' | sed -e 's/executables.*\r# Install/\r# Install/' | sed -e 's/install(TARGETS hsdaoh_file.*)//' | tr '\r' '\n' > ./src/CMakeLists.txt
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX="${WORKSPACE}" -DINSTALL_UDEV_RULES=False ../
if [[ ("$OSTYPE" == "cygwin"*) ]]; then
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
