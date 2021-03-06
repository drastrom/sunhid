
#include <stdint.h>
#include <string.h>
#include <chopstx.h>

#include "config.h"
#include "board.h"

#include "stm32f103_local.h"
#include "usb_lld.h"
#include "usb_conf.h"
#include "usb_hid.h"

#include "serial.h"

#define USB_HID_REQ_GET_REPORT   1
#define USB_HID_REQ_GET_IDLE     2
#define USB_HID_REQ_GET_PROTOCOL 3
#define USB_HID_REQ_SET_REPORT   9
#define USB_HID_REQ_SET_IDLE     10
#define USB_HID_REQ_SET_PROTOCOL 11

static struct hid_info
{
	uint8_t hid_idle_rate;
	uint8_t hid_protocol;
} hid_info[2] = {{0, 1}, {0, 1}};

static struct hid_locks
{
	chopstx_mutex_t tx_mut;
} hid_locks[2];

static union keyb_hid_report
{
	uint64_t raw;
	struct {
		union {
			uint8_t modifiers;
			struct {
				uint8_t left_ctrl:1;
				uint8_t left_shift:1;
				uint8_t left_alt:1;
				uint8_t left_gui:1;
				uint8_t right_ctrl:1;
				uint8_t right_shift:1;
				uint8_t right_alt:1;
				uint8_t right_gui:1;
			};
		};
		uint8_t reserved;
		uint8_t keycodes[6];
	};
} keyb_hid_report;

static union keyb_output_report
{
	uint8_t raw;
	struct {
		uint8_t num_lock:1;
		uint8_t caps_lock:1;
		uint8_t scroll_lock:1;
		uint8_t compose:1;
		uint8_t kana:1;
		uint8_t reserved:3;
	};
} keyb_output_report;

static union mouse_hid_report
{
	uint64_t raw;
	struct {
		union {
			uint8_t raw_buttons;
			struct {
				uint8_t button1:1;
				uint8_t button2:1;
				uint8_t button3:1;
				uint8_t reserved1:5;
			};
			struct {
				uint8_t buttons:3;
				uint8_t reserved2:5;
			};
		};
		int8_t x;
		int8_t y;
#ifdef MOUSE_WHEEL
		int8_t wheel;
#endif
#ifdef MOUSE_PAN
		int8_t pan;
#endif
		uint8_t reserved[
#if defined(MOUSE_WHEEL) && defined(MOUSE_PAN)
			3
#elif defined(MOUSE_WHEEL) || defined(MOUSE_PAN)
			4
#else
			5
#endif
			];
	};
} mouse_hid_report;

static const struct endpoint_info
{
	uint8_t ep_num;
	uint16_t tx_addr;
} endpoint_info[2] = {{ENDP1, ENDP1_TXADDR}, {ENDP2, ENDP2_TXADDR}};

void hid_setup_endpoints(struct usb_dev *dev,
				uint16_t interface, int stop)
{
#if !defined(GNU_LINUX_EMULATION)
	(void)dev;
#endif

	if (!stop)
	{
#ifdef GNU_LINUX_EMULATION
		usb_lld_setup_endp (dev, endpoint_info[interface - HID_INTERFACE_0].ep_num, 0, 1);
#else
		usb_lld_setup_endpoint (endpoint_info[interface - HID_INTERFACE_0].ep_num, EP_INTERRUPT, 0, 0, endpoint_info[interface - HID_INTERFACE_0].tx_addr, 0);
#endif
		/* Spec says should default to report protocol (1) on intialization
		 * This seems like as good a place for intialization as any */
		hid_info[interface - HID_INTERFACE_0].hid_idle_rate = 0;
		hid_info[interface - HID_INTERFACE_0].hid_protocol = 1;
	}
	else
	{
		usb_lld_stall_tx (endpoint_info[interface - HID_INTERFACE_0].ep_num);
	}
}

void hid_tx_done(uint8_t ep_num, uint16_t len)
{
	(void)len;
}

static void hid_keyb_write(void)
{
#ifdef GNU_LINUX_EMULATION
	usb_lld_tx_enable_buf (ENDP1, &keyb_hid_report, sizeof(keyb_hid_report));
#else
	usb_lld_write (ENDP1, &keyb_hid_report, sizeof(keyb_hid_report));
#endif
}

int hid_key_pressed(uint8_t hidcode)
{
	int ret = 0;
	chopstx_mutex_lock(&hid_locks[0].tx_mut);
	if (hidcode >= 0xe0 && hidcode <= 0xe7)
	{
		uint8_t mask = 1 << (hidcode-0xe0);
		if (!(keyb_hid_report.modifiers & mask))
		{
			keyb_hid_report.modifiers |= mask;
			ret = 1;
		}
	}
	else
	{
		int slot = -1;
		int i;
		for (i = 0; i < 6; ++i)
		{
			if (keyb_hid_report.keycodes[i] == 0)
				slot = i;
			else if (keyb_hid_report.keycodes[i] == hidcode)
				break;
		}
		if (i == 6)
		{
			if (slot == -1)
			{
				ret = -1;
				/* TODO overflow - USB_ERR_OVF in all slots.
				 * But, if I do that, I lose all of the keys
				 * that are down.  I would have to track all
				 * key states elsewhere, so I could recover
				 * from USB_ERR_OVF properly.  See Appendix C
				 * of HID 1.11 spec.
				 */
			}
			else
			{
				keyb_hid_report.keycodes[slot] = hidcode;
				ret = 1;
			}
		}
	}

	if (ret == 1)
		hid_keyb_write();
	chopstx_mutex_unlock(&hid_locks[0].tx_mut);
	return ret;
}

int hid_key_released(uint8_t hidcode)
{
	int ret = 0;
	chopstx_mutex_lock(&hid_locks[0].tx_mut);
	if (hidcode >= 0xe0 && hidcode <= 0xe7)
	{
		uint8_t mask = 1 << (hidcode-0xe0);
		if (keyb_hid_report.modifiers & mask)
		{
			keyb_hid_report.modifiers &= ~mask;
			ret = 1;
		}
		else
		{
			ret = -1;
		}
	}
	else
	{
		int slot = -1;
		for (int i = 0; i < 6; ++i)
		{
			if (keyb_hid_report.keycodes[i] == hidcode)
			{
				slot = i;
				break;
			}
		}
		if (slot != -1)
		{
			keyb_hid_report.keycodes[slot] = 0;
			ret = 1;
		}
		else
		{
			ret = -1;
		}
	}

	if (ret == 1)
		hid_keyb_write();
	chopstx_mutex_unlock(&hid_locks[0].tx_mut);
	return ret;
}

int hid_key_releaseAll(void)
{
	int ret = 0;
	chopstx_mutex_lock(&hid_locks[0].tx_mut);
	if (keyb_hid_report.raw != 0)
	{
		ret = 1;
		keyb_hid_report.raw = 0;
		hid_keyb_write();
	}
	chopstx_mutex_unlock(&hid_locks[0].tx_mut);
	return ret;
}

static void hid_mouse_write(void)
{
#ifdef GNU_LINUX_EMULATION
	usb_lld_tx_enable_buf (ENDP2, &mouse_hid_report, sizeof(mouse_hid_report));
#else
	usb_lld_write (ENDP2, &mouse_hid_report, sizeof(mouse_hid_report));
#endif
}

int hid_mouse_move(int8_t x, int8_t y)
{
	int ret = 0;
	chopstx_mutex_lock(&hid_locks[1].tx_mut);
	mouse_hid_report.x = x;
	mouse_hid_report.y = y;
#if defined(MOUSE_WHEEL)
	mouse_hid_report.wheel = 0;
#endif
#if defined(MOUSE_PAN)
	mouse_hid_report.pan = 0;
#endif
	hid_mouse_write();
	ret = 1;
	chopstx_mutex_unlock(&hid_locks[1].tx_mut);
	return ret;
}

int hid_mouse_set_buttons(uint8_t buttons)
{
	int ret = 0;
	buttons &= 0x7;
	chopstx_mutex_lock(&hid_locks[1].tx_mut);
	if (mouse_hid_report.buttons != buttons)
	{
		mouse_hid_report.buttons = buttons;
		mouse_hid_report.x = 0;
		mouse_hid_report.y = 0;
#if defined(MOUSE_WHEEL)
		mouse_hid_report.wheel = 0;
#endif
#if defined(MOUSE_PAN)
		mouse_hid_report.pan = 0;
#endif
		//hid_mouse_write();
		ret = 1;
	}
	chopstx_mutex_unlock(&hid_locks[1].tx_mut);
	return ret;
}

int hid_mouse_button_press(uint8_t button)
{
	int ret = 0;
	uint8_t mask = 1 << button;
	chopstx_mutex_lock(&hid_locks[1].tx_mut);
	if (!(mouse_hid_report.buttons & mask))
	{
		mouse_hid_report.buttons |= mask;
		mouse_hid_report.x = 0;
		mouse_hid_report.y = 0;
#if defined(MOUSE_WHEEL)
		mouse_hid_report.wheel = 0;
#endif
#if defined(MOUSE_PAN)
		mouse_hid_report.pan = 0;
#endif
		hid_mouse_write();
		ret = 1;
	}
	chopstx_mutex_unlock(&hid_locks[1].tx_mut);
	return ret;
}

int hid_mouse_button_release(uint8_t button)
{
	int ret = 0;
	uint8_t mask = 1 << button;
	chopstx_mutex_lock(&hid_locks[1].tx_mut);
	if (mouse_hid_report.buttons & mask)
	{
		mouse_hid_report.buttons &= ~mask;
		mouse_hid_report.x = 0;
		mouse_hid_report.y = 0;
#if defined(MOUSE_WHEEL)
		mouse_hid_report.wheel = 0;
#endif
#if defined(MOUSE_PAN)
		mouse_hid_report.pan = 0;
#endif
		hid_mouse_write();
		ret = 1;
	}
	chopstx_mutex_unlock(&hid_locks[1].tx_mut);
	return ret;
}

int hid_data_setup(struct usb_dev *dev, uint16_t interface)
{
	switch (dev->dev_req.request)
	{
	case USB_HID_REQ_GET_IDLE:
		return usb_lld_ctrl_send (dev, &hid_info[interface - HID_INTERFACE_0].hid_idle_rate, 1);
	case USB_HID_REQ_SET_IDLE:
		hid_info[interface - HID_INTERFACE_0].hid_idle_rate = (dev->dev_req.value >> 8) & 0xff;
		return usb_lld_ctrl_ack (dev);

	case USB_HID_REQ_GET_REPORT:
		if (((dev->dev_req.value >> 8) & 0xFF) == 1)
		{
			int ret;
			chopstx_mutex_lock(&hid_locks[interface - HID_INTERFACE_0].tx_mut);
			if (interface == HID_INTERFACE_0)
				ret = usb_lld_ctrl_send (dev, &keyb_hid_report, sizeof(keyb_hid_report));
			else /*if (interface == HID_INTERFACE_1)*/
				ret = usb_lld_ctrl_send (dev, &mouse_hid_report, sizeof(mouse_hid_report));
			chopstx_mutex_unlock(&hid_locks[interface - HID_INTERFACE_0].tx_mut);
			return ret;
		}
		return -1;

	case USB_HID_REQ_SET_REPORT:
		if (((dev->dev_req.value >> 8) & 0xFF) == 2 && interface == HID_INTERFACE_0)
			return usb_lld_ctrl_recv (dev, &keyb_output_report, sizeof(keyb_output_report));
		return -1;

	case USB_HID_REQ_GET_PROTOCOL:
		return usb_lld_ctrl_send (dev, &hid_info[interface - HID_INTERFACE_0].hid_protocol, 1);
	case USB_HID_REQ_SET_PROTOCOL:
		hid_info[interface - HID_INTERFACE_0].hid_protocol = dev->dev_req.value & 0xff;
		return usb_lld_ctrl_ack (dev);

	default:
		return -1;
	}
}

void hid_ctrl_write_finish(struct usb_dev *dev, uint16_t interface)
{
	if (dev->dev_req.request == USB_HID_REQ_SET_REPORT)
	{
		if (((dev->dev_req.value >> 8) & 0xFF) == 2 && interface == HID_INTERFACE_0)
		{
			keyboard_set_leds(keyb_output_report.raw);
		}
	}
}

void hid_init(void)
{
	for (int i = 0; i < 2; ++i)
		chopstx_mutex_init(&hid_locks[i].tx_mut);
}
