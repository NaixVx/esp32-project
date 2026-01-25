# ESP32 ESP-IDF C++ Base Project Template

This repository is a reusable **ESP-IDF (v5.5+) C++ project template** for ESP32.
It is intended to be used as a base for multiple projects by adding new components.

## Features

- ESP-IDF 5.5+ compatible
- C++ project structure
- Wi-Fi support:
  - Access Point (AP)
  - Station (STA)
- Non-Volatile Storage (NVS) for persistent settings
- Simple HTTP API (GET / POST)
- Web-based configuration UI
  - Root endpoint (`/`)
  - HTML, CSS, and JavaScript stored in LittleFS
- Custom partition table with LittleFS support
- Modular, component-based design

## Project Structure

- `main/` – Application entry point
- `components/` – Reusable components
- `partitions.csv` – Custom partition table
- `.gitmodules` – External dependencies (e.g. LittleFS)

## Requirements

- ESP-IDF **5.5 or newer**
- ESP32-compatible board

## Getting Started

### 1. Install ESP-IDF 5.5+

Follow the official ESP-IDF installation guide.

### 2. Clone the repository

```bash
git clone <repo-url>
cd <repo-name>
```

### 3. Initialize submodules

```bash
git submodule update --init --recursive
```

### 4. Export ESP-IDF environment

```bash
. $IDF_PATH/export.sh
```

### 5. Configure the project

```bash
idf.py menuconfig
```

Set Partition Table → Custom partition table
Ensure LittleFS support is enabled

### 6. Build and flash

```bash
idf.py build
idf.py flash monitor
```

## Usage

- On first boot, the device starts in Access Point (AP) mode
- Connect to the AP and open the root endpoint (`192.168.4.1/`) in a browser
- Use the web UI to configure Wi-Fi and system settings

## Extending the Template

- Add new functionality by creating additional components
- Modify or extend the web UI via the www/ directory
- Reuse this repository as a base for multiple ESP32 projects

## License

MIT License
