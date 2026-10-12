#!/bin/sh
/usr/bin/systemctl stop lcdmonitor
sleep 3
# reset the lcd
/usr/local/bin/lcdcmd -r
# wait a bit - docs say 3 s to reboot
sleep 5
/usr/bin/systemctl start lcdmonitor
