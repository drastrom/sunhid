
#include <stdint.h>
#include <string.h>
#include <chopstx.h>

#include "config.h"

uint8_t hid2sun_leds(uint8_t hid_leds)
{
	return (hid_leds & 0x05) |
	       ((hid_leds & 0x02) << 2) |
	       ((hid_leds & 0x08) >> 2);
}


