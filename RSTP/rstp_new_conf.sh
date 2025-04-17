#!/bin/sh

### CONFIGURATION ###
BRIDGE="br0"
IFACES="eth0 eth1"
BRIDGE_IP="192.168.0.200"
NETMASK="255.255.255.0"
GATEWAY="192.168.0.1"

echo "🔧 Cleaning up previous bridge configuration..."
# Tear down existing bridge setup
for iface in $IFACES; do
  brctl delif $BRIDGE $iface 2>/dev/null
done
ifconfig $BRIDGE down 2>/dev/null
brctl delbr $BRIDGE 2>/dev/null

echo "🔗 Creating new bridge: $BRIDGE..."
brctl addbr $BRIDGE
for iface in $IFACES; do
  brctl addif $BRIDGE $iface
done
brctl stp $BRIDGE on
ifconfig $BRIDGE up

echo "🌐 Assigning static IP and setting default gateway..."
ifconfig $BRIDGE $BRIDGE_IP netmask $NETMASK up
route add default gw $GATEWAY

echo "🌀 Registering with mstpd and enabling RSTP..."
mstpctl addbridge $BRIDGE
mstpctl setforcevers $BRIDGE rstp

echo "⚙️ Tuning RSTP parameters for fast reconvergence..."
mstpctl sethello $BRIDGE 1         # Fast Hello
mstpctl setmaxage $BRIDGE 6        # Timeout window
mstpctl setfdelay $BRIDGE 4        # Fast forward delay
mstpctl settxholdcount $BRIDGE 3   # Control BPDU burst
mstpctl setageing $BRIDGE 60       # MAC aging

echo "🚀 Enabling fast edge detection and P2P auto-config..."
for iface in $IFACES; do
  mstpctl setportadminedge $BRIDGE $iface yes
  mstpctl setportautoedge $BRIDGE $iface yes
  mstpctl setportp2p $BRIDGE $iface auto
done

echo "✅ RSTP bridge $BRIDGE setup complete with optimized settings."
echo "🔍 Bridge Info:"
mstpctl showbridge $BRIDGE
for iface in $IFACES; do
  mstpctl showport $BRIDGE $iface
done

