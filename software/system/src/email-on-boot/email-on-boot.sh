cat /usr/local/etc/bootmsg.txt | /usr/bin/mail -r time@measurement.gov.au -s "$HOSTNAME has rebooted" time@measurement.gov.au
