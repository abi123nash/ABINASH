#!/bin/sh

# Function to check MDIO status
check_mdio_status() {
    if dmesg | grep -q "phy\[0\]: device 54432400.mdio:00"; then
        echo "MDIO initialized successfully."
        return 0
    else
        echo "MDIO initialization failed. Rebooting the board..."
        return 1
    fi
}



# Load kernel modules with delay
insmod /lib/modules/hsr.ko
sleep 1                       

insmod /lib/modules/pruss.ko
sleep 3

# Verify MDIO status
check_mdio_status
if [ $? -ne 0 ]; then
        echo "Exiting script due to MDIO initialization failure."
#        reboot
else

        # Load additional kernel modules
        insmod /lib/modules/pru_rproc.ko
        sleep 1                        
                               
        insmod /lib/modules/icss_iep.ko
        sleep 1                      
                             
        insmod /lib/modules/irq-pruss-intc.ko
        sleep 1

        insmod /lib/modules/prueth.ko
        sleep 1                                             

        insmod /lib/modules/llc.ko
        sleep 1

        insmod /lib/modules/stp.ko
        sleep 1

        insmod /lib/modules/bridge.ko
        sleep 1

        # Bring up Ethernet interfaces
        ifconfig eth0 up
        sleep 1         
     
        ifconfig eth1 up
        sleep 1                                                                
                                         
        echo "Kernel module loading and Ethernet setup completed successfully."

fi

