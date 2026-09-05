#!/bin/bash

echo "Compiling contacts program..."

gcc ../c-programs/contacts.c -o contacts

echo "Running contacts program..."

./contacts

echo "Creating backup..."

tar -czf contacts-backup.tar.gz ../c-programs/*

echo "Output logged at $(date)" >> project-log.txt

echo "Project complete."
