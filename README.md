# CPU and Memory Usage Tracker

![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)
![Platform](https://img.shields.io/badge/platform-linux%20%7C%20windows%20%7C%20macos-lightgrey.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

A lightweight, cross-platform system utility that monitors CPU and memory usage of all running processes with an integrated process termination capability.

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Screenshots](#screenshots)
- [Requirements](#requirements)
- [Installation](#installation)
- [Usage](#usage)
- [Technical Implementation](#technical-implementation)
- [Development](#development)
- [Future Plans](#future-plans)
- [Contributors](#contributors)

## 🔍 Overview

This application provides a real-time, graphical interface for monitoring system resource usage at the process level. It allows users to identify resource-intensive applications and terminate them when necessary, all through an intuitive GTK-based interface.

## ✨ Features

- **Real-time Monitoring**: View CPU and memory usage for all system processes
- **Process Management**: Safely terminate resource-heavy processes with confirmation
- **Customizable Updates**: Adjust refresh rates from 500ms to 10s
- **Cross-Platform**: Works on Linux, Windows, and macOS with native APIs
- **Resource Efficient**: Lightweight design with minimal system impact
- **User-Friendly**: Clear, intuitive interface with sortable process list

## 📸 Screenshots

*[Screenshots will be added upon completion of the UI design]*

## 📋 Requirements

### System Requirements

- **Memory**: 50MB minimum
- **Storage**: 10MB of free space
- **Display**: 800×600 minimum resolution

### Dependencies

- C Compiler (GCC recommended)
- GTK 3.0+
- POSIX Threads (pthreads)
- Platform-specific requirements:
  - **Linux**: proc filesystem
  - **Windows**: PSAPI libraries
  - **macOS**: Mach kernel APIs

## 🚀 Installation

### Linux

```bash
# Install dependencies
## Debian/Ubuntu
sudo apt-get install build-essential libgtk-3-dev

## Fedora
sudo dnf install gcc gtk3-devel

## Arch Linux
sudo pacman -S base-devel gtk3

# Build and install
git clone https://github.com/username/cpu-memory-tracker.git
cd cpu-memory-tracker
make
sudo make install  # Optional: installs to /usr/local/bin
```

### macOS

```bash
# Install dependencies with Homebrew
brew install gtk+3

# Build and run
git clone https://github.com/username/cpu-memory-tracker.git
cd cpu-memory-tracker
make
```

### Windows

```bash
# Install MSYS2 from https://www.msys2.org/
# Open MSYS2 terminal and run:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-gtk3 make

# Build and run
git clone https://github.com/username/cpu-memory-tracker.git
cd cpu-memory-tracker
make
```

## 📖 Usage

### Basic Usage

1. Launch the application:
   ```bash
   ./cpu_memory_tracker
   ```

2. The main window shows all running processes with their resource usage
3. Select a process and click "Terminate Process" to end it
4. Adjust the refresh interval using the spin button

### Interface Guide

- **Process List**: Displays PID, name, CPU%, and memory% for each process
- **Refresh Now**: Force an immediate update of the process list
- **Refresh Interval**: Set how frequently the data updates (in milliseconds)
- **Terminate Process**: End the selected process (requires confirmation)

## 🔧 Technical Implementation

### Architecture

The application follows a multi-threaded architecture:
- Main thread: Handles UI and user interaction
- Background thread: Collects process data at regular intervals

### Platform-Specific APIs

- **Linux**: Uses `/proc` filesystem to gather process information
- **Windows**: Utilizes Windows Management Instrumentation and PSAPI
- **macOS**: Leverages Mach kernel APIs and BSD process information

### Key Components

```
├── main.c             # Main application code
├── Makefile           # Build configuration
└── README.md          # Documentation
```

## 👨‍💻 Development

### Building from Source

```bash
# Clone repository
git clone https://github.com/username/cpu-memory-tracker.git
cd cpu-memory-tracker

# Standard build
make

# Clean build files
make clean
```

### Extending the Application

To add new features:
1. Modify `main.c` to implement additional functionality
2. Update the GTK interface as needed
3. Maintain platform-specific implementations for cross-platform compatibility

## 🚀 Future Plans

- Process search and filtering capabilities
- Historical resource usage graphs
- System resource threshold notifications
- Additional memory metrics (shared, private, swap)
- Dark mode and UI customization options
- Export process data to CSV/JSON

## 👥 Contributors

- **Abzalbek Ibrokhimov** (ID: 220587)
- **Abdurashid Djumabaev** (ID: 210004)

---

*This project was developed as part of the System Programming course at [University Name].*