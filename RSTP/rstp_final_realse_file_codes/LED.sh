#!/bin/bash

# GPIO pin to control
PIN=124

# Export the GPIO pin and set it as output
echo $PIN > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio$PIN/direction

echo 0 > /sys/class/gpio/gpio$PIN/value

echo "System power on."

