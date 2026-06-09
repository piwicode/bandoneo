Boards can be programmed with [STLINK-V3MINIE](https://www.st.com/en/development-tools/stlink-v3minie.htm)

# Command library

I use [`just`](https://just.systems/) to run commands from a confiugration
file such as `code/main-g474/justfile`
```
cargo install just
```

# Connecting the debugger probe:

Plug STLink to a usb port and verify the device is detected:

```
$ lsusb | grep STLINK
Bus 001 Device 070: ID 0483:3754 STMicroelectronics STLINK-V3
```

Install [stlink-tools](https://github.com/stlink-org/stlink) version >= 1.8 with

```
apt install stlink-tools
```

or if your distrib version is too old:

```
sudo apt remove stlink-tools
sudo apt install build-essential cmake libusb-1.0-0-dev
git clone --depth 1 --branch testing https://github.com/stlink-org/stlink
cd stlink
git apply documentation/0001-fix-st-trace-fix-SWO-trace-on-STLINK-V3-HS-bulk-endp.patch
make release && sudo make install && sudo ldconfig
```

Once the stlink is plugged to the board:

```
$ st-info --probe
Found 1 stlink programmers
  version:    V3J15
  serial:     002F00413235510637333439
  flash:      131072 (pagesize: 2048)
  sram:       131072
  chipid:     0x469
  dev-type:   STM32G47x_G48x
```

# Build the code

Prerequisite:

```
sudo apt install gcc-arm-none-eabi ninja-build
```

```
cd code/main-g474
cmake --preset Debug
cmake --build --preset Debug
```

# Flash the code

```
arm-none-eabi-objcopy -O binary build/Debug/main-g474.elf build/Debug/main-g474.bin
st-flash write build/Debug/main-g474.bin 0x08000000
```

0x08000000 is the STM32G4's internal flash base address (matches STM32G474XX_FLASH.ld).

# Read SWO debug text stream


Characters written to `ITM_SendChar` show show up in a view names
`SWV ITM Data Console`, but the configuration button does not work, and that
falls short.

```
st-trace --clock=16m
```

Warning: This requires a patch (0001-fix-st-trace-fix-SWO-trace-on-STLINK-V3-HS-bulk-endp.patch) otherwise it will fail with `2026-06-08T23:00:36 ERROR usb.c: read_trace insufficient buffer length`


## OpenOCD Fails
```bash
$ openocd -f interface/stlink.cfg -f target/stm32g4x.cfg \
  -c "tpiu config internal swo.log uart off 16000000 2000000" \
  -c "init" -c "reset"
```

Fails with `Error: libusb_bulk_read error: LIBUSB_ERROR_OVERFLOW`

## In STM32CubeIde Fails

* In STM32Cube IDE debug configuration enable SWO and use the `To CPU1 FCLK`
  speed as Core Clock speed, that is to say 4MHz.
* Then add in "Windows" > Show View > SDW > SDW ITM Data Console
* Enable ITM Stimulus Port 0 in the parameter of this view, and finally press
  the red button.

Fails with `SWV info, Failed to read data!!!`

Reference:
- https://www.youtube.com/watch?v=j-GaEZKrkbQ

# Read UART debug console

Characters written with `printf` (via `_write` retargeted to `HAL_UART_Transmit`) are sent over USART to the STLink VCP bridge, which forwards them to a host `/dev/ttyACMx` device.

Use `tio` to read it — it auto-reconnects across resets, unlike `screen` or `cat`:

```bash
tio /dev/serial/by-id/usb-STMicroelectronics_STLINK-V3_*
```

The glob resolves to the correct `/dev/ttyACMx` regardless of what other USB serial devices are present. Or via the justfile recipe:

```bash
just console
```

Press ctrl-t q to quit.

# Development environement

I use VSCode with `clangd` rather than `intelliSenseEngine` because it reads `compile_commands.json` directly — correct ARM toolchain headers, exact build flags, no manual config. 

Make sure clangd is installed on your system:

```bash
sudo apt-get install -y clangd && clangd --version
```



