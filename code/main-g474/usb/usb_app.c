#include "usb_app.h"

#include "tusb.h"
#include "stm32g4xx_hal.h"

void usb_app_init(void)
{
  /* The 48 MHz USB kernel clock (HSI48 + CRS, crystal-less) and the USB
   * peripheral clock are configured by the CubeMX-generated
   * SystemClock_Config() and MX_USB_PCD_Init(); enforced by
   * code/tests/test_usb_config.py against the .ioc. */

  /* Keep USB below the console UART so a CDC write burst cannot starve
   * character reception (HAL tick stays at 0, set by HAL_Init). */
  NVIC_SetPriority(USB_HP_IRQn, 6);
  NVIC_SetPriority(USB_LP_IRQn, 6);
  NVIC_SetPriority(USBWakeUp_IRQn, 6);

  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_FULL,
  };
  tusb_init(0, &dev_init);
}

void usb_app_task(void)
{
  tud_task();

  /* No MIDI input handling yet: drain the FIFO so it never sits full. */
  uint8_t packet[4];
  while (tud_midi_available()) {
    tud_midi_packet_read(packet);
  }
}

void usb_app_midi_test_note(uint8_t note)
{
  uint8_t const cable = 0, channel = 0;
  uint8_t note_on[3]  = { 0x90 | channel, note, 100 };
  uint8_t note_off[3] = { 0x80 | channel, note, 0 };
  tud_midi_stream_write(cable, note_on, 3);
  tud_midi_stream_write(cable, note_off, 3);
}

/* USB interrupt handlers (override the weak defaults from the startup file) */
void USB_HP_IRQHandler(void)
{
  tud_int_handler(0);
}

void USB_LP_IRQHandler(void)
{
  tud_int_handler(0);
}

void USBWakeUp_IRQHandler(void)
{
  tud_int_handler(0);
}
