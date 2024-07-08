#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

if [ $? -ne 0 ]; then
    echo "Could not create output directory!"
    exit 1
fi

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    wget -O patch.diff https://github.com/torvalds/linux/commit/e33a814e772cdc36436c8c188d8c42d019fda639.diff
    git apply patch.diff
    make -j8 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    # make -j8 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make -j8 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs

    cd ..
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p "${OUTDIR}/rootfs"

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone git://busybox.net/busybox.git --depth 1 --single-branch --branch ${BUSYBOX_VERSION}
    cd busybox
    # git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
else
    cd busybox
fi

# TODO: Make and install busybox
make distclean
make defconfig
make -j8 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make -j8 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=../rootfs/ install

cd ../rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
CROSS_COMPILE_GCC=$(which ${CROSS_COMPILE}gcc)
CROSS_COMPILE_LIBRARY_DIR=$(dirname $CROSS_COMPILE_GCC)/../aarch64-none-linux-gnu/libc

cp ${CROSS_COMPILE_LIBRARY_DIR}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/
cp ${CROSS_COMPILE_LIBRARY_DIR}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64/
cp ${CROSS_COMPILE_LIBRARY_DIR}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64/
cp ${CROSS_COMPILE_LIBRARY_DIR}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64/

# TODO: Make device nodes
sudo mknod -m 622 ${OUTDIR}/rootfs/dev/console c 5 1
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3

# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}
cp writer ${OUTDIR}/rootfs/home

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp finder.sh ${OUTDIR}/rootfs/home
cp finder-test.sh ${OUTDIR}/rootfs/home
cp autorun-qemu.sh ${OUTDIR}/rootfs/home
mkdir -p ${OUTDIR}/rootfs/home/conf
cp conf/username.txt ${OUTDIR}/rootfs/home/conf/
cp conf/assignment.txt ${OUTDIR}/rootfs/home/conf/

# TODO: Chown the root directory
sudo chown -R root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz
if [ -e ${OUTDIR}/initramfs.cpio.gz ]; then
    rm ${OUTDIR}/initramfs.cpio.gz
fi
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip ${OUTDIR}/initramfs.cpio
