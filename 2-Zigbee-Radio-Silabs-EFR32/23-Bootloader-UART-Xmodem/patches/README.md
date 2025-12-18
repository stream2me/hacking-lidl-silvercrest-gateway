# Patches for Bootloader-UART-Xmodem

Ready-to-use project files for building the UART XMODEM bootloader for EFR32MG1B232F256GM48 (Lidl Silvercrest Gateway).

## Files

| File | Purpose |
|------|---------|
| `bootloader-uart-xmodem.slcp` | Project config with components and settings |
| `btl_uart_driver_cfg.h` | UART pin configuration (USART0 PA0/PA1, RTS/CTS PA4/PA5) |
| `btl_gpio_activation_cfg.h` | Button pin configuration (PB11) |

## Build Process

```
1. Copy slcp from patches/
        ↓
2. slc generate
        ↓
3. Copy config headers from patches/
        ↓
4. make -Oz
```

---

## Pin Configuration

### UART (USART0)

| Signal | Port | Pin | Location |
|--------|------|-----|----------|
| TX | PA0 | 0 | 0 |
| RX | PA1 | 1 | 0 |
| RTS | PA4 | 4 | 0 |
| CTS | PA5 | 5 | 0 |

### GPIO Activation

| Signal | Port | Pin |
|--------|------|-----|
| Button | PB11 | 11 |

---

## Bootloader Size

The bootloader occupies the first 16 KB of flash:
- First stage: 0x0000 - 0x0800 (2 KB)
- Main stage: 0x0800 - 0x4000 (14 KB)

Applications start at 0x4000.

Typical compiled size: ~12.6 KB (fits within 14 KB limit)
