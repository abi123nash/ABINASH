#!/bin/bash

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
brctl addif br0 eth0 
sleep 1

brctl addif br0 eth1
sleep 1

echo "Enabling STP on Bridge br0 ....."
brctl stp br0 on
sleep 1


echo "Verifying ....."
brctl show
sleep 1

echo "check the states of links ....."
brctl showstp br0
sleep 1

echo "Assigning IP Address for bridge br0 as 192.168.0.12 ....."
ifconfig br0 192.168.0.12
sleep 1

echo "Verifying IP Address for br0 ....."
ifconfig

echo "Adding bridge for rstp br0 ..."
mstpctl addbridge br0
sleep 1

echo "Displaying bridge configuration for br0 ..."
mstpctl showbridge br0
sleep 1

