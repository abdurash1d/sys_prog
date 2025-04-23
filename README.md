# CPU and Memory Usage Tracker with Kill Switch

A graphical application showing CPU/memory usage per process, with termination options.

## Project Overview

This utility provides real-time monitoring of system processes, showing CPU and memory usage with the ability to terminate resource-heavy processes directly through the interface.

## Features

- Real-time display of CPU and memory usage per process
- Process information including PID, name, and user
- Sort functionality (click column headers to sort)
- Adjustable refresh rate (1, 2, 5, or 10 seconds)
- Kill switch to terminate selected processes
- User-friendly GTK-based interface

## Dependencies

- GTK3 development libraries
- C compiler (gcc recommended)
- pthread library
- Linux system (relies on /proc filesystem)

## Installation

### Install Dependencies

On Debian/Ubuntu:
```bash
sudo apt-get install build-essential libgtk-3-dev