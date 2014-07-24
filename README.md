vm-timing-report
======

Copyright (C) 2014 Intel Corporation
Author: Ikey Doherty <michael.i.doherty@intel.com>


To test this system, please point the script at a valid *raw file system*
that uses systemd.

Firstly, issue the following command:

    sudo spin-from-raw.sh $PATHTORAWFILESYSTEM

Once this is complete, you should have a new "rootfs.img" as a backing file
for your operating system images. Now you should create some test images:

    mkdir vms && cd vms

    for i in {0..20} ; do qemu-img create your-os-$i.img -b ../rootfs.img -f qcow2; done

And now, cd back to the directory that the rootfs.img lives in, and
execute according to the parameters required by your operating system:

    # Booting 8 vms (-n), 340M of memory each (-m), prefix of "your-os" (-p"
    # and suffix of ".img" (-s), using -k(ernel) and -i(nitrd) from the
    # BFILES directory (auto-saved) and using vms as the vmdir (-v)
    sudo vm-timing-report -n 8 -m 340 -p your-os -s .img -k BFILES/vmlinuz -i BFILES/initrd -v vms/

The output is quite verbose, in order to facilitate future integrations
via helpful reports.

The last of the output will be related to Virtual Machines reporting to
the vm-timing-report tool that they are "up" - these will poweroff 3 seconds
after this interval.

You will see an average of times and each VMs internal concept of boot time
by way of systemd-analyze output:

    Got a connection: 4
    Got a connection: 5
    Got a connection: 6
    Got a connection: 7
    Got a connection: 8
    Got a connection: 9
    Got a connection: 10
    Got a connection: 11
    VM 2 reported as booted
    VM 4 reported as booted
    VM 6 reported as booted
    VM 7 reported as booted
    VM 0 reported as booted
    VM 3 reported as booted
    VM 5 reported as booted
    VM 1 reported as booted
    VM 0 booted in: 13.64s: Startup finished in 988ms (kernel) + 1.167s (initrd) + 2.001s (userspace) = 4.158s
    VM 1 booted in: 14.48s: Startup finished in 1.785s (kernel) + 950ms (initrd) + 1.540s (userspace) = 4.276s
    VM 2 booted in: 8.385s: Startup finished in 898ms (kernel) + 1.518s (initrd) + 1.479s (userspace) = 3.896s
    VM 3 booted in: 13.97s: Startup finished in 1.200s (kernel) + 2.031s (initrd) + 966ms (userspace) = 4.199s
    VM 4 booted in: 10.3s: Startup finished in 900ms (kernel) + 1.217s (initrd) + 2.039s (userspace) = 4.157s
    VM 5 booted in: 14.39s: Startup finished in 1.773s (kernel) + 1.883s (initrd) + 1.504s (userspace) = 5.161s
    VM 6 booted in: 13.23s: Startup finished in 1.007s (kernel) + 1.536s (initrd) + 2.028s (userspace) = 4.572s
    VM 7 booted in: 13.62s: Startup finished in 1.579s (kernel) + 1.228s (initrd) + 1.723s (userspace) = 4.532s
    Average boot: 12.75s
    It took 14.48s to boot all VMs


Notes
-----
The spin script (spin-from-raw) requires sudo to do mount, rsync, etc, to
construct a new VM template.

You MIGHT not need sudo to run vm-timing-report itself, however please note
this is entirely distribution specific. Whilst some distributions automatically
give users access to KVM, you may have to either use sudo *or* add yourself
to the 'kvm' group. Please consult your Linux distribution's documentation
if you are unsure.
