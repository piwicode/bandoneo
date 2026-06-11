/*
 * TinyUSB application glue for the Bandoneo main board.
 * Composite device: MIDI + CDC mirror of the microrl console.
 */

#ifndef USB_APP_H_
#define USB_APP_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Starts the TinyUSB device stack. Call after MX_USB_PCD_Init() so the USB
 * peripheral clock (48 MHz from PLLQ) is already configured. */
void usb_app_init(void);

/* Runs the TinyUSB device task and the CDC console bridge.
 * Call from the main loop on every iteration. */
void usb_app_task(void);

/* Sends a short note on/off pair on MIDI channel 1, for end-to-end testing. */
void usb_app_midi_test_note(uint8_t note);

/* Sends a Note On / Note Off on MIDI channel 1 (cable 0). */
void usb_app_midi_note_on(uint8_t note, uint8_t velocity);
void usb_app_midi_note_off(uint8_t note);

#ifdef __cplusplus
}
#endif

#endif /* USB_APP_H_ */
