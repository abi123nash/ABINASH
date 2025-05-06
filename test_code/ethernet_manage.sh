#!/bin/sh

# Debug control: Set to 1 to enable debug prints, 0 to disable
DEBUG=1

STATUS_FILE="/tmp/iface_status"
INTERFACES="eth0 eth1"
SLEEP_INTERVAL=2

# Ensure status file exists
touch "$STATUS_FILE"

log_msg() {
    [ "$DEBUG" -eq 1 ] && echo "$(date '+%Y-%m-%d %H:%M:%S') - $1"
}

initialize_interfaces() {
    for iface in $INTERFACES; do
        if ! ip link show "$iface" > /dev/null 2>&1; then
            log_msg "Interface $iface not found during initialization"
            continue
        fi

        STATE=$(cat "/sys/class/net/$iface/operstate")
        if [ "$STATE" != "up" ]; then
            ip link set "$iface" up
            log_msg "Brought up interface $iface"
        fi

        echo "$iface=down" >> "$STATUS_FILE"
    done
}

check_and_configure_interface() {
    ETH_IF=$1
    LINK_PATH="/sys/class/net/$ETH_IF/carrier"

    if [ ! -e "$LINK_PATH" ]; then
        log_msg "Interface $ETH_IF not found"
        return
    fi

    LINK_STATUS=$(cat "$LINK_PATH")

    if [ "$LINK_STATUS" -eq 1 ]; then
        if ! ip addr show "$ETH_IF" | grep -q "inet "; then
            log_msg "$ETH_IF is up, acquiring IP via udhcpc"
            pkill -f "udhcpc.*$ETH_IF" 2>/dev/null
            udhcpc -i "$ETH_IF" >/dev/null 2>&1 &
        fi
    else
        log_msg "$ETH_IF is down, flushing IP addresses"
        ip addr flush dev "$ETH_IF"
        pkill -f "udhcpc.*$ETH_IF" 2>/dev/null
    fi
}

trap 'log_msg "Script interrupted"; exit 0' INT TERM

log_msg "Starting eth_monitor script"
initialize_interfaces

while true; do
    for IFACE in $INTERFACES; do
        check_and_configure_interface "$IFACE"
    done
    sleep "$SLEEP_INTERVAL"
done
