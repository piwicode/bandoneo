# USB stack (TinyUSB)

The main board enumerates as a full-speed MIDI USB device built on
[TinyUSB](../../third_party/tinyusb) (git submodule, `code/third_party/tinyusb`):

| | |
|---|---|
| VID / PID | `0xCafe` / `0x4001` (TinyUSB convention: `0x4000 \| interface bitmap`) |
| Manufacturer | L'Atelier du bandonéon libre |
| Product | Bandoneo |
| Serial | 96-bit chip UID, 24 hex digits |
| Interfaces | MIDI only |

## Files

- `tusb_config.h` — TinyUSB feature configuration (device-only, CDC + MIDI).
- `usb_descriptors.c` — device/configuration/string descriptors. Strings are
  UTF-8 and converted to UTF-16 on the fly, so accented names work.
- `usb_app.c/.h` — glue: `usb_app_init()` starts the stack (called from
  `main()` after `MX_USB_PCD_Init()`), `usb_app_task()` runs `tud_task()` from
  the main loop, and the three USB IRQ handlers (which override the weak
  symbols in `startup_stm32g474xx.s`).

The TinyUSB sources, include paths, and the `CFG_TUSB_MCU=OPT_MCU_STM32G4`
define are listed in the user sections of [CMakeLists.txt](../CMakeLists.txt).

## How it coexists with CubeMX

CubeMX still generates `MX_USB_PCD_Init()` (HAL PCD), which enables the USB
peripheral clock via `HAL_PCD_MspInit()`. TinyUSB's `dcd_init()` then resets
and takes over the peripheral; `HAL_PCD_Start()` is never called, so the HAL
never enables the DP pull-up or the USB interrupts itself. Don't remove the
USB peripheral from the `.ioc`.

**USB clock**: the board is crystal-less, so the `.ioc` selects HSI48 as USB
kernel clock, trimmed by the CRS against USB SOF (the PLL alternative is fed
by HSI16, ±1%, outside the USB FS ±0.25% budget). All of this lives in the
generated `SystemClock_Config()`. These `.ioc` invariants — HSI48 + CRS, USB
device FS enabled, USB interrupts *not* enabled in the NVIC tab (TinyUSB owns
the handlers) — are checked by `code/tests/test_usb_config.py` (`just test`
from `code/tests`).


## Testing

```sh
lsusb -d cafe:4001       # device present
amidi -l                 # MIDI port listed as "Bandoneo"
aseqdump -p Bandoneo &   # then run `midi [note]` in the console to test
```

The `midi [note]` console command (via USART1/ST-Link) sends a note-on/note-off
pair (default 69 = A4) for end-to-end checking over USB MIDI.
