# Simplicity Studio V5


## Silicon Labs Tools Overview

Silicon Labs provides several tools for EFR32 development. See the [full list](https://www.silabs.com/software-and-tools/simplicity-studio/developer-tools).

| Tool | Description |
|------|-------------|
| **Simplicity Studio** | Full IDE with GUI for project creation, configuration, and flashing |
| **slc** | Command-line tool to create and configure projects |
| **Commander** | Flash, debug, and secure devices via GUI or CLI |
| **Network Analyzer** | Capture and analyze wireless network traffic |

**Simplicity Studio V5** is Silicon Labs' official IDE for developing and flashing firmware onto EFR32MG1B chips. It is an alternative to the CLI-based approach used in this project (see [24-NCP-UART-HW](../24-NCP-UART-HW/), [25-RCP-UART-HW](../25-RCP-UART-HW/), etc.) which uses `slc`.

**Note:** The EFR32MG1B is a Series 1 chip. Simplicity Studio V5 is the latest Silabs IDE supporting Series 1. The newer Simplicity Studio V6 only supports Series 2 chips.

## Download

Download from the official Silicon Labs website:

https://www.silabs.com/software-and-tools/simplicity-studio/simplicity-studio-version-5

A free Silicon Labs account may be required later to download SDKs.

## Notes

- **Debugger (J-Link, etc.)**: Required to flash firmware via SWD. See
  [22-Backup-Flash-Restore](../22-Backup-Flash-Restore/) for hardware setup.
- **Administrator Privileges**: On some Linux systems, you may need to run
  Simplicity Studio as root.
  ```sh
  sudo ./studio.sh
  ```
- **udev Rules for Debugger Access**: If your debugger is not recognized,
  create a udev rule:
  ```sh
  echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="1366", MODE="0666"' | sudo tee /etc/udev/rules.d/99-segger.rules
  sudo udevadm control --reload-rules
  ```
- **Java Dependencies**: If Simplicity Studio fails to start, ensure you
  have Java installed:
  ```sh
  sudo apt install default-jre
  ```
