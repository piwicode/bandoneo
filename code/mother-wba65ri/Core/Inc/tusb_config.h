#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#define CFG_TUSB_DEBUG    2
#define CFG_TUSB_MCU                OPT_MCU_STM32WBA  // Use OPT_MCU_STM32WBA for WBA
#define CFG_TUD_ENABLED             1
#define CFG_TUD_MIDI                1    // <--- This enables Native MIDI!
#define CFG_TUD_MIDI_RX_BUFSIZE     64
#define CFG_TUD_MIDI_TX_BUFSIZE     64
// Define the Root Hub Port 0 mode
// For STM32WB55, this is the internal Full Speed PHY
#define CFG_TUSB_RHPORT0_MODE    (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#endif
