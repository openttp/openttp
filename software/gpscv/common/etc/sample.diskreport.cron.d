#
# Install this in /etc/cron.d/ 
# You may want to change the user.
#
# This will call the diskreport script every 15 minutes.
0-59/15 * * * * root /home/cvgps/bin/diskreport.py -c /home/cvgps/etc/diskreport.conf
