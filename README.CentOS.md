Creating a suitable VM template for Cent OS 7
===

Firstly you'll need to ensure you have "virt-install" available for use.
On many distributions this is packaged as "virtinst" - yours may differ.

Initial VM template:
---

Firstly we'll create a direct VM image as a raw filesystem suitable for
conversion:

    virt-install --name "centos7x-vm-gpt" --ram 2048 --nographics --os-type=linux \
    --os-variant=rhel7 --location=http://mirrors.clouvider.net/CentOS/7/os/x86_64 \
    --extra-args="ks=https://raw.githubusercontent.com/ikeydoherty/vm-timing-report/master/data/centos7.ks text console=tty0\ utf8 console=ttyS0,115200" \
    --disk /var/lib/libvirt/images/centos7.img,device=disk,sparse=true,size=10,bus=virtio,format=raw

This will create a virtual machine using the kickstart template provided in the
vm-timing-report git repository. Note this may take some time to complete!

Conversion to template:
---

Now we must convert the image to a format suitable for usage with vm-timing-report:

    sudo spin-from-raw.sh /var/lib/libvirt/images/centos7.img

Now you may follow the instructions detailed in README.md, as this format
is now compatible with vm-timing-report, and file names are also identical.


Author
===

Copyright (C) 2014 Intel Corporation

Author: Ikey Doherty <michael.i.doherty@intel.com>
