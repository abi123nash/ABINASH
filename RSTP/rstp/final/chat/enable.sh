#!/bin/bash

echo "Checking Network Interfaces ....."
sudo ifconfig
sleep 1

echo "Adding Bridge as br0 ....."
sudo brctl addbr br0
sleep 1

echo "Making Bridge up ....."
sudo ifconfig br0 up
sleep 1

echo "Adding Network Interfaces To the Bridge ....."
sudo brctl addif br0 enx30de4b21a192 
sleep 1

sudo brctl addif br0 enx30de4b49d0b7
sleep 1

echo "Enabling STP on Bridge br0 ....."
sudo brctl stp br0 on
sleep 1


echo "Verifying ....."
sudo brctl show
sleep 1

echo "check the states of links ....."
sudo brctl showstp br0
sleep 1

echo "Assigning IP Address for bridge br0 as 192.168.0.12 ....."
sudo ifconfig br0 192.168.0.12
sleep 1

echo "Verifying IP Address for br0 ....."
ifconfig

echo "Adding bridge for rstp br0 ..."
mstpctl addbridge br0
sleep 1

echo "Displaying bridge configuration for br0 ..."
mstpctl showbridge br0
sleep 1

