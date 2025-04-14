
chmod 777 /home/root/onetime_setup.sh

chmod 777 /lib/modules/STP/*



/lib/modules/STP/LED.sh
sleep 1
/lib/modules/STP/Ethernet_bringup.sh
sleep 1
/lib/modules/STP/STP_enable.sh
sleep 1
/lib/modules/STP/client > /dev/null 2>&1 &
sleep 1


