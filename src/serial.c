
#include <stdint.h>
#include <string.h>
#include <chopstx.h>

#include "config.h"
#include "board.h"
#include "usart.h"

#include "stm32f103_local.h"

#include "sun_xlate.h"
#include "usb_hid.h"

extern void _write (const char *s, int len);
#ifdef DEBUG
extern void put_byte_with_no_nl(uint8_t);
extern void put_int(uint32_t);
extern void put_short(uint16_t);
extern void put_binary (const char *, int);
#endif

#define STACK_PROCESS_2
#define STACK_PROCESS_3
#define STACK_PROCESS_4
#include "stack-def.h"
#define STACK_ADDR_USART ((uintptr_t)process2_base)
#define STACK_SIZE_USART (sizeof process2_base)
#define STACK_ADDR_KEYBOARD ((uintptr_t)process3_base)
#define STACK_SIZE_KEYBOARD (sizeof process3_base)
#define STACK_ADDR_MOUSE ((uintptr_t)process4_base)
#define STACK_SIZE_MOUSE (sizeof process4_base)

#define PRIO_USART 5
#define PRIO_KEYBOARD 4
#define PRIO_MOUSE 3

static int my_callback (uint8_t dev_no, uint16_t notify_bits)
{
	(void)dev_no;
	(void)notify_bits;
	return 0;
}

static void *
mouse_main(void *arg)
{
	uint8_t read_byte;
	(void)arg;
	chopstx_usec_wait(250*1000);
	while (usart_read(2, (char *)&read_byte, 1))
	{
#ifdef DEBUG
		//put_byte_with_no_nl(read_byte);
#endif
		/* TODO */
	}
	return NULL;
}

static void *
keyboard_main(void *arg)
{
	uint8_t read_byte;
	(void)arg;
	chopstx_usec_wait(250*1000);
	/* chances are we missed POST, so send a reset command */
	read_byte = 0x01;
	usart_write(3, (char *)&read_byte, 1);
	while (usart_read(3, (char *)&read_byte, 1))
	{
		uint8_t hidcode;
#ifdef DEBUG
		put_byte_with_no_nl(read_byte);
#endif
		switch (read_byte)
		{
		case 0xff: /* reset response */
			usart_read(3, (char *)&read_byte, 1);
			/* better be 4 */
			//assert(read_byte == 0x04);
			break;
		case 0xfe: /* Layout request reponse */
			usart_read(3, (char *)&read_byte, 1);
			/* layout dip switches */
			break;
		case 0x7e: /* Failed self-test */
			usart_read(3, (char *)&read_byte, 1);
			/* better be 1 */
			//assert(read_byte == 0x01);
			break;
		case 0x7f: /* Idle */
			hid_key_releaseAll();
			break;
		default:
			hidcode = sun2hid_keycode(read_byte);
			if (hidcode != 0)
			{
				if (read_byte & 0x80)
					hid_key_released(hidcode);
				else
					hid_key_pressed(hidcode);
			}
		}
	}
	return NULL;
}

void keyboard_set_leds(uint8_t hid_leds)
{
	uint8_t set_leds_command[2] = {0x0E, hid2sun_leds(hid_leds)};
	usart_write(3, (char *)set_leds_command, 2);
}

void serial_init(void)
{
	/* blue pill board.h initialized GPIOA and GPIOC, but we need to do GPIOB */
	RCC->APB2RSTR = RCC_APB2RSTR_IOPBRST;
	RCC->APB2RSTR = 0;
	RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

	/* PB11 input pull-up, PB10 alt function push-pull 2MHz
	 * Everything else input pull-up */
	GPIOB->ODR = 0xFFFFFFFF;
	GPIOB->CRL = 0x88888888;
	GPIOB->CRH = 0x88888A88;

	usart_init(PRIO_USART, STACK_ADDR_USART, STACK_SIZE_USART, my_callback);
	chopstx_create(PRIO_KEYBOARD, STACK_ADDR_KEYBOARD, STACK_SIZE_KEYBOARD, keyboard_main, NULL);
	chopstx_create(PRIO_MOUSE, STACK_ADDR_MOUSE, STACK_SIZE_MOUSE, mouse_main, NULL);
}

