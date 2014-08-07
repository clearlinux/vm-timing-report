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

OURID=""
for i in $(cat /proc/cmdline) ; do
        case $i in 
                VMREPORTID*)
                	OURID=$i
			;;
        esac
done

while :
do
        if [[ -e /usr/lib/upstart || ! -e /dev/virtio-ports/serial0 ]]; then
            ANALYZE="`uptime`"
        else
            ANALYZE="`systemd-analyze`"
        fi
        if [[ $? -ne 0 ]]; then
                # Must wait for systemd-analyze to report a boot time,
                # otherwise this is not considered "booted"
                sleep 0.2
                continue
        fi
        echo "$OURID|||$ANALYZE" > /dev/virtio-ports/serial0
        break
done
sync

sleep 3
poweroff
