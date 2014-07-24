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

RAWFS="$1"
DEVS=""
MOUNTPOINT="./MOUNT"
TMPMOUNT="./TMP"
ROOTIMAGE="rootfs.img"
BFILES="./BFILES"

if [[ ! -f "$RAWFS" ]] ; then
        echo "$i does not exist - exiting"
        exit 1
fi

# kpartx it and find what parts are available
while  read -r line
do
        splits=($line)
        if [[ $line == *add* ]]; then
                part="${splits[2]}"
                # Handle cases where its /dev/loop0p1, etc.
                if [[ -e "/dev/mapper/$part" ]]; then
                        DEVS="$DEVS /dev/mapper/$part"
                else
                        DEVS="$DEVS /dev/$part"
                fi
        fi
        #echo $line
done < <(kpartx -av "$RAWFS")

echo "Found devices: $DEVS"

if [[ ! -d $TMPMOUNT  ]]; then
        mkdir $TMPMOUNT
fi

set +x
# find partitions
for device in $DEVS ; do
        echo "$device"
        mount -o loop $device $TMPMOUNT
        if [[ $? -ne 0 ]]; then
                echo "Unable to probe $device"
                continue
        fi
        # Attempt to find boot files
        if [[ -n "$(shopt -s nullglob; echo $TMPMOUNT/boot/vmlinuz*)" ]]; then
                echo "Backing up boot files"

                # Back up the boot files
                if [[ ! -d $BFILES ]]; then
                        mkdir $BFILES
                fi
                cp -v $TMPMOUNT/boot/initrd* $BFILES/initrd
                cp -v $TMPMOUNT/boot/vmlinu* $BFILES/vmlinuz
        elif [[ -n "$(shopt -s nullglob; echo $TMPMOUNT/vmlinuz*)" ]]; then
                # Same thing as above, but this is a boot partition
                echo "Backing up boot files from boot partition"
                # Back up the boot files
                if [[ ! -d $BFILES ]]; then
                        mkdir $BFILES
                fi
                cp -v $TMPMOUNT/initrd* $BFILES/initrd
                cp -v $TMPMOUNT/vmlinu* $BFILES/vmlinuz
        fi

        if [[ ! -e $TMPMOUNT/usr/lib/systemd/systemd ]]; then
                echo "systemd not detected - skipping"
        else
                echo "Found install on $device"
                MAINPART="$device"
        fi
        sleep 1
        umount $TMPMOUNT
done

if [[ -z $MAINPART ]]; then
        echo "Could not locate usable system. Aborting"
        kpartx -dv "$RAWFS"
        exit 1
fi

# Mount the "mainpart" as source and rsync it to new system
mount -o loop $MAINPART $TMPMOUNT

# Create target
dd if=/dev/zero of=$ROOTIMAGE bs=1 seek=2048M count=0
if [[ ! -d $MOUNTPOINT ]]; then
        mkdir $MOUNTPOINT
fi

mkfs -t ext4 -F $ROOTIMAGE
mount -o loop $ROOTIMAGE $MOUNTPOINT

echo "Now installing"
rsync -av $TMPMOUNT/ $MOUNTPOINT/

# Generate an fstab always using /dev/vda
echo "devpts /dev/pts devpts mode=0620,gid=5 0 0" > $MOUNTPOINT/etc/fstab
echo "proc   /proc    proc   defaults        0 0" >> $MOUNTPOINT/etc/fstab
echo "/dev/vda	 /	 ext4	 defaults,rw	 0	 0" >> $MOUNTPOINT/etc/fstab

# Very important, write out a systemd unit to talk back
# In future we need to get this source path at build time.
install -m 0700 /usr/share/vm-timing-report/vm-report.service $MOUNTPOINT/usr/lib/systemd/system/vm-report.service
install -m 0755 /usr/share/vm-timing-report/vm-report.sh $MOUNTPOINT/usr/bin/vm-report

chroot $MOUNTPOINT systemctl enable vm-report

sync
sync

umount $MOUNTPOINT

umount $TMPMOUNT
kpartx -dv "$RAWFS"
