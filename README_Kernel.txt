################################################################################

1. How to Build
	- get Toolchain
			From Android Source Download site( http://source.android.com/source/downloading.html )
			Toolchain is included in Android source code.

	- edit Makefile
			edit "CROSS_COMPILE" to right toolchain path(You downloaded).
			ex) CROSS_COMPILE= $(android platform directory you download)/android/prebuilt/linux-x86/toolchain/arm-eabi-4.7/bin/arm-eabi-
			ex) CROSS_COMPILE=/usr/local/toolchain/arm-eabi-4.7/bin/arm-eabi-
	
	- make
			$ make ARCH=arm msm8226-sec_defconfig VARIANT_DEFCONFIG=msm8226-sec_matissewifi_defconfig
			$ make ARCH=arm

2. Output files
	- Kernel : arch/arm/boot/zImage
	- module : drivers/*/*.ko

3. How to Clean	
		$ make clean
################################################################################
