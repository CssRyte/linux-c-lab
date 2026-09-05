#!/bin/bash

read -p "Enter directory: " DIR

echo "File count results for $DIR" > file-count-log.txt

echo "TXT files:" >> file-count-log.txt
find "$DIR" -type f -name "*.txt" | wc -l >> file-count-log.txt

echo "SH files:" >> file-count-log.txt
find "$DIR" -type f -name "*.sh" | wc -l >> file-count-log.txt

echo "C files:" >> file-count-log.txt
find "$DIR" -type f -name "*.c" | wc -l >> file-count-log.txt

cat file-count-log.txt
