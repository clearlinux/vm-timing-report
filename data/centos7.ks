install
lang en_GB.UTF-8
keyboard gb
timezone Europe/London
auth --useshadow --enablemd5
selinux --disabled
firewall --disabled
eula --agreed
ignoredisk --only-use=vda
poweroff
	 
bootloader --location=mbr
zerombr
clearpart --all --initlabel
part / --fstype ext4 --size=2048
rootpw --plaintext vmreportdemo
repo --name=base --baseurl=http://mirror.cogentco.com/pub/linux/centos/7/os/x86_64/
url --url="http://mirror.cogentco.com/pub/linux/centos/7/os/x86_64/"

%packages --nobase --ignoremissing
@core
%end
