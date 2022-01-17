#!/bin/sh
cp bootmsg.txt /usr/local/etc
cp email-on-boot.sh /usr/local/bin
cp email-on-boot.service /lib/systemd/system
systemctl enable email-on-boot
echo "You may need to customize /usr/local/etc/bootmsg.txt"
