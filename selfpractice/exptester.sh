#!/bin/bash
# Usage: ./tester.sh <src_path>

# Check if argument is provided
if [ $# -lt 1 ]; then
    echo "Usage: $0 <source_file>"
    exit 1
fi

SRC_PATH=$1              # The source file path passed as argument
OUT_FILE=tester  # Output binary same name as source

# Call make with SRC variable
make fclean
make SRCS_PATH="$SRC_PATH" APPNAME="$OUT_FILE"