#!/bin/bash

# This file is part of vm-timing-report.
#
# Copyright (C) 2014 Intel Corporation
# Author: Ikey Doherty <michael.i.doherty@intel.com>
#
# vm-timing-eport is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.

MOUNTPOINT="./MOUNT"
ROOTIMAGE="rootfs.img"
BFILES="./BFILES"

# fallocate doesn't work on all distros
dd if=/dev/zero of=$ROOTIMAGE bs=1 seek=2048M count=0

if [[ ! -d $MOUNTPOINT ]]; then
        mkdir $MOUNTPOINT
fi

mkfs -t ext4 -F $ROOTIMAGE
mount -o loop $ROOTIMAGE $MOUNTPOINT

debootstrap --arch amd64 trusty $MOUNTPOINT
cp -v /etc/resolv.conf $MOUNTPOINT/etc/resolv.conf
# Kernels wont install under Ubuntu without proc (scans for PAE support..)
mount -o bind /proc $MOUNTPOINT/proc

chroot $MOUNTPOINT apt-get install linux-image-generic --yes --force-yes

umount $MOUNTPOINT/proc

if [[ ! -d "$BFILES" ]]; then
        mkdir "$BFILES"
fi

cp -v $MOUNTPOINT/boot/vmlinu* $BFILES/vmlinuz
cp -v $MOUNTPOINT/boot/initr* $BFILES/initrd


# Generate an fstab always using /dev/vda
echo "devpts /dev/pts devpts mode=0620,gid=5 0 0" > $MOUNTPOINT/etc/fstab
echo "proc   /proc    proc   defaults        0 0" >> $MOUNTPOINT/etc/fstab
echo "/dev/vda	 /	 ext4	 defaults,rw	 0	 0" >> $MOUNTPOINT/etc/fstab

# Init files
install -m 0700 /usr/share/vm-timing-report/vm-report.init $MOUNTPOINT/etc/init.d/vm-report
install -m 0755 /usr/share/vm-timing-report/vm-report.sh $MOUNTPOINT/usr/bin/vm-report

chroot $MOUNTPOINT update-rc.d vm-report defaults
chroot $MOUNTPOINT update-rc.d vm-report enable

sleep 1
sync
umount $MOUNTPOINT
