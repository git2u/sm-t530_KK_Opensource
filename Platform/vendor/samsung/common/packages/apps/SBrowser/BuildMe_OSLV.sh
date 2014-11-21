#!/bin/bash
#
# Author Mohan Reddy V
#
# Description:  Developed APK script to make the SBrowser_Chrome_Test_Shell apk for chrome23 or later versions
#
# Copyright (c) 2012 SAMSUNG

SBROWSER_BASE_PATH=$PWD
echo "SBROWSER_BASE_PATH =  ${SBROWSER_BASE_PATH}"


cd ${SBROWSER_BASE_PATH}/src
echo $PWD

export PATH="$PATH":${SBROWSER_BASE_PATH}/depot_tools
#settting the ANDROID SDK ROOT
export ANDROID_SDK_ROOT=${SBROWSER_BASE_PATH}/src/third_party/android_tools/sdk
#setiing the   ANDROID NDK ROOT
export ANDROID_NDK_ROOT=${SBROWSER_BASE_PATH}/src/third_party/android_tools/ndk
export PATH=${SBROWSER_BASE_PATH}/src/third_party/android_tools/sdk/platform-tools/:$PATH
#set  ANDROID SOURCE ROOT #can be used when platform dependencies are there
#ANDROID_ICS_SRC=$3
export SBR_ANDROID_SRC_ROOT=$1
export ANDROID_PRODUCT_OUT=$SBR_ANDROID_SRC_ROOT/out/target/product/generic

NDK_ROOT=$ANDROID_NDK_ROOT
SDK_ROOT=$ANDROID_SDK_ROOT

export SBROWSER_ENABLE_SUB_COMPOSING=0

echo "============================================"
echo "Building libsbrowser.so		       "
echo "============================================"


SBROWSER_LIB_NAME="libsbrowser.so"


echo "The libsbrowser name is $SBROWSER_LIB_NAME"

cp -r out_jni out
./make_sbrowser_oslv.sh $SBR_ANDROID_SRC_ROOT $ANDROID_PRODUCT_OUT


