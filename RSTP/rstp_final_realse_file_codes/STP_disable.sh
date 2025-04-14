#!/bin/bash

echo "Making Bridge br0 down ....."
ifconfig br0 down
sleep 1

echo "Disabling STP on Bridge br0 ....."
brctl stp br0 off
sleep 1

echo "removing the network interfaces ....."
brctl delif br0 enp9s0
sleep 1

brctl delif br0 enx30de4b49af5e
sleep 1

echo "Deleting the Bridge br0 ....."
brctl delbr br0
sleep 1

echo "Varify Bridge is Removed ....."
ifconfig
