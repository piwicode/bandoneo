#ifndef SPI_LINK_H_
#define SPI_LINK_H_

/* Wing -> main keyboard frame, sent over the SPI link described in
 * design_review/hardware.md (wing = SPI master, main = SPI slave, hardware
 * CRC-16 enabled on both ends).
 *
 * Fixed-size so the SPI slave can post a single HAL_SPI_Receive of known
 * length: word 0 is the sending wing's id, words 1.. are the raw hall
 * measurement for each physical key (one per key, in flattened key order).
 * The frame carries the full absolute keyboard state every cycle (not a
 * delta), so a dropped (CRC-failed) frame just leaves the receiver's view at
 * its previous state until the next one arrives.
 */

/* Physical keys per wing (5 hall ADCs x 4 mux selects x 2 ranks). */
#define SPI_LINK_NUM_KEYS 40

#define SPI_LINK_FRAME_WORDS (1 + SPI_LINK_NUM_KEYS)

#endif /* SPI_LINK_H_ */
