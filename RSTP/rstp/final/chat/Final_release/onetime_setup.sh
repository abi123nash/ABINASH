#!/bin/bash

# Ensure that both arguments are passed
if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <BOARD_NAME> <IP_ADDRESS>"
  exit 1
fi

# Assign input arguments to variables
BOARD_NAME=$1
IP_ADDRESS=$2

# Define file paths
DEV_FILE="/home/zumi/Abinash/am4376_board_bringup_new_project/rstp/rstp/final/chat/Final_release/dev.txt"       # Replace with the correct path to dev.txt
STP_FILE="/home/zumi/Abinash/am4376_board_bringup_new_project/rstp/rstp/final/chat/Final_release/STP_enable.sh" # Replace with the correct path to STP_enable.sh

# Debugging: Display content of dev.txt before modification
echo "Current content of dev.txt:"
cat "$DEV_FILE"
echo "------------------------------"

# Modify dev.txt file to replace 'IM am437x' with the BOARD_NAME (case-insensitive)
echo "Updating $DEV_FILE with BOARD_NAME = $BOARD_NAME ..."
echo $BOARD_NAME > $DEV_FILE

# Debugging: Display content of dev.txt after modification
echo "Updated content of dev.txt:"
cat "$DEV_FILE"
echo "------------------------------"

# Modify STP_enable.sh file to replace the IP_ADDRESS variable
echo "Updating $STP_FILE with IP_ADDRESS = $IP_ADDRESS ..."
sed -i "s/IP_ADDRESS=\"[^\"]*\"/IP_ADDRESS=\"$IP_ADDRESS\"/" "$STP_FILE"

echo "Modification complete!"

