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

echo "$OURID|||`systemd-analyze`" > /dev/virtio-ports/serial0
sync

sleep 3
poweroff
