#!/bin/bash

# GPIO pins to control
PINS=(122 123 124)

# Export the GPIO pins and set them as output
for PIN in "${PINS[@]}"; do
    echo $PIN > /sys/class/gpio/export
    echo out > /sys/class/gpio/gpio$PIN/direction
done

# Function to clean up GPIOs on exit
cleanup() {
    for PIN in "${PINS[@]}"; do
        echo 0 > /sys/class/gpio/gpio$PIN/value
        echo $PIN > /sys/class/gpio/unexport
    done
    echo "GPIO cleanup complete."
    exit 0
}

# Trap SIGINT (Ctrl+C) to clean up before exiting
trap cleanup SIGINT

echo "Starting LED blink test. Press Ctrl+C to stop."

# Blink the LEDs
while true; do
    for PIN in "${PINS[@]}"; do
        echo 1 > /sys/class/gpio/gpio$PIN/value
    sleep 0.5
    done
    sleep 0.5
    for PIN in "${PINS[@]}"; do
        echo 0 > /sys/class/gpio/gpio$PIN/value
    sleep 0.5
    done
    sleep 0.5
done

