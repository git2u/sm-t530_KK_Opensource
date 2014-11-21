#!/bin/bash
#
# Author Mohan Reddy V
#
# Description:  Script for building the SBrowser
#
# Copyright (c) 2012 SAMSUNG

# If current path isn't the Chrome's src directory, CHROME_SRC must be set
# to the Chrome's src directory.

function help() {
cat <<EOF
Invoke ". make_schrome.sh ANDROID_SDK_ROOT ANDROID_NDK_ROOT ANDROID_ICS_SRC " from your shell
EOF
}

#set  ANDROID SDK ROOT
export ANDROID_SDK_ROOT=`pwd`/third_party/android_tools/sdk
#set  ANDROID NDK ROOT
export ANDROID_NDK_ROOT=`pwd`/third_party/android_tools/ndk
#set  ANDROID SOURCE ROOT
#ANDROID_ICS_SRC=$3
export SBR_ANDROID_SRC_ROOT=$1
export ANDROID_PRODUCT_OUT=$2

echo "MOHAN SBR_ANDROID_SRC_ROOT is: $SBR_ANDROID_SRC_ROOT"
echo "MOHAN ANDROID_PRODUCT_OUT is: $ANDROID_PRODUCT_OUT"

export SBROWSER_SO=`pwd`/out/Release/lib/libsbrowser.so
export GCC_VER=46 #change value to 48 when building with toolchain 4.8
export TOOL_VER=4.6 #change to 4.8 when building the code with 4.8 toolchain
if [ ! -d "${ANDROID_NDK_ROOT}" ]; then
  help
  exit 1
fi

if [ ! -d "${ANDROID_SDK_ROOT}" ]; then
  help
  exit 1
fi

#if [ ! -d "${ANDROID_ICS_SRC}" ]; then
#  help
#  exit 1
#fi
if [ -f "${SBROWSER_SO}" ]; then
	rm ${SBROWSER_SO}
fi
	
rm `pwd`/../SBrowser/libs/armeabi-v7a/libsbrowser.so

echo "ANDROID SDK ROOT ${ANDROID_SDK_ROOT} "
echo "ANDROID_NDK_ROOT ${ANDROID_NDK_ROOT} "
#echo "ANDROID_ICS_SRC ${ANDROID_ICS_SRC} "

echo "============================================"
echo "setup android environment		       "
echo "============================================"
. build/android/envsetup.sh #$ANDROID_ICS_SRC


echo " GYP_DEFINES = ${GYP_DEFINES} " 

echo "============================================"
echo "setup chrome GYP environment		       "
echo "============================================"

android_gyp -Denable_sbrowser_apk=1 -Denable_sbrowser=1 -Denable_ndktoolchain_48=0 -Denable_qcom_v8=1 -Denable_sec_v8=0 -Denable_sbrowser_fingerprint=0 -Denable_sub_composing_region=1 -Denable_so_reduction=0 -Denable_data_compression=0 -Denable_sbrowser_qcom_skia=1


echo "============================================"
echo "building SBrowser native code  	          "
echo "============================================"


ninja -C out/Release -j`grep processor /proc/cpuinfo|wc -l` libsbrowser

if [ -f "${SBROWSER_SO}" ]; then
	echo "Native build is finished... and successfull"

	echo "copy  so from out folder to Apk lib folder..."
	cp ${SBROWSER_SO}  `pwd`/../SBrowser/libs/armeabi-v7a/.

	echo "Stripping the sbrowser so"
	`pwd`/third_party/android_tools/ndk/toolchains/arm-linux-androideabi-4.6/prebuilt/linux-x86_64/bin/arm-linux-androideabi-strip  `pwd`/../SBrowser/libs/armeabi-v7a/libsbrowser.so

	echo "copy chrome.pak and resources.pak files to Apk assets folder..."
	cp out/assets/sbrowser/*.pak  `pwd`/../SBrowser/assets/.

	echo "Debug libsbrowser.so is located in : `pwd`/out/Release/lib"
	echo "Stripped libsbrowser.so is located in:  `pwd`/../SBrowser/libs/armeabi-v7a"
else
	echo " Build failed check the build error"
	exit 1
fi


