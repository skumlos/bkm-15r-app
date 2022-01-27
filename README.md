BKM-15R emulator written in C for Linux terminal

Crude implementation of the BKM-15R protocol as described here: https://immerhax.com/?p=797

Knobs need work, right now only supports step of 1 up or down, and essentially input should be moved out of the default loop for responsiveness.

Ensure machines LAN port is configured to 192.168.0.100 and monitor is set in peer-to-peer mode.

Building:
gcc remote.c -o remoteapp

Running:
./remoteapp

See LICENSE.txt

(2022) Martin Hejnfelt (martin@hejnfelt.com)
www.immerhax.com
