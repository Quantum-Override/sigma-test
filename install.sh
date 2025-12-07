#!/bin/bash

# Install script for sigma-test (stest)
# This script installs the shared library and headers to standard system locations

set -e

echo "Installing sigma-test..."

# Check if running as root or with sudo
if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root or with sudo"
    exit 1
fi

# Copy the shared library to /usr/lib/
if [[ -f "bin/lib/libstest.so" ]]; then
    cp bin/lib/libstest.so /usr/lib/
    echo "Installed libstest.so to /usr/lib/"
else
    echo "Error: bin/lib/libstest.so not found"
    exit 1
fi

# Copy headers to /usr/include/
if [[ -d "include" ]]; then
    mkdir -p /usr/include/sigtest
    cp -r include/* /usr/include/sigtest/
    echo "Installed headers to /usr/include/sigtest/"
else
    echo "Error: include directory not found"
    exit 1
fi

# Update library cache
ldconfig

echo "Installation complete!"
echo "You may need to restart applications that use sigma-test."