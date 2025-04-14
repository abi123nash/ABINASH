#!/bin/bash

echo "Checking Network Interfaces ....."
# Define variables
IP_ADDRESS="192.168.10.100"  # IP address for the bridge

INTERFACE1="eth0"  # First network interface

INTERFACE2="eth1"  # Second network interface

echo "Checking Network Interfaces ....."
ifconfig
sleep 1

echo "Adding Bridge as br0 ....."
brctl addbr br0
sleep 1

echo "Making Bridge up ....."
ifconfig br0 up
sleep 1

echo "Adding Network Interfaces To the Bridge ....."
brctl addif br0 $INTERFACE1
sleep 1

brctl addif br0 $INTERFACE2
sleep 1

echo "Enabling STP on Bridge br0 ....."
brctl stp br0 on
sleep 1

echo "Verifying ....."
brctl show
sleep 1

echo "Check the states of links ....."
brctl showstp br0
sleep 1

echo "Assigning IP Address for bridge br0 as $IP_ADDRESS ....."
ifconfig br0 $IP_ADDRESS
sleep 1

echo "Verifying IP Address for br0 ....."
ifconfig

echo "Adding bridge for RSTP br0 ..."
mstpctl addbridge br0
sleep 1

echo "Displaying bridge configuration for br0 ..."
mstpctl showbridge br0
sleep 1

