
#include <stdint.h>
#include <string.h>
#include <chopstx.h>

#include "config.h"
#include "board.h"

#include "stm32f103_local.h"
#include "usb_lld.h"
#include "usb_conf.h"
#include "usb_hid.h"

#define USB_HID_REQ_GET_REPORT   1
#define USB_HID_REQ_GET_IDLE     2
#define USB_HID_REQ_GET_PROTOCOL 3
#define USB_HID_REQ_SET_REPORT   9
#define USB_HID_REQ_SET_IDLE     10
#define USB_HID_REQ_SET_PROTOCOL 11

static uint8_t hid_idle_rate;	/* in 4ms */
static uint8_t hid_protocol = 1;
static uint64_t hid_report;
static uint8_t hid_report_output;

void hid_setup_endpoints(struct usb_dev *dev,
				uint16_t interface, int stop)
{
#if !defined(GNU_LINUX_EMULATION)
	(void)dev;
#endif

	if (!stop)
#ifdef GNU_LINUX_EMULATION
		usb_lld_setup_endp (dev, interface == HID_INTERFACE_0 ? ENDP1 : ENDP2, 0, 1);
#else
		usb_lld_setup_endpoint (interface == HID_INTERFACE_0 ? ENDP1 : ENDP2, EP_INTERRUPT, 0, 0, interface == HID_INTERFACE_0 ? ENDP1_TXADDR : ENDP2_TXADDR, 0);
#endif
	else
		usb_lld_stall_tx (interface == HID_INTERFACE_0 ? ENDP1 : ENDP2);
}

int hid_data_setup(struct usb_dev *dev, uint16_t interface)
{
	switch (dev->dev_req.request)
	{
	case USB_HID_REQ_GET_IDLE:
		return usb_lld_ctrl_send (dev, &hid_idle_rate, 1);
	case USB_HID_REQ_SET_IDLE:
		/* XXX XXX
		return usb_lld_ctrl_recv (dev, &hid_idle_rate, 1);
		*/
		hid_idle_rate = (dev->dev_req.value >> 8) & 0xff;
		return usb_lld_ctrl_ack (dev);

	case USB_HID_REQ_GET_REPORT:
		if (((dev->dev_req.value >> 8) & 0xFF) == 1)
			return usb_lld_ctrl_send (dev, &hid_report, sizeof(hid_report));
		return -1;

	case USB_HID_REQ_SET_REPORT:
		if (((dev->dev_req.value >> 8) & 0xFF) == 2)
			return usb_lld_ctrl_recv (dev, &hid_report_output, sizeof(hid_report_output));
		return -1;

	case USB_HID_REQ_GET_PROTOCOL:
		return usb_lld_ctrl_send (dev, &hid_protocol, 1);
	case USB_HID_REQ_SET_PROTOCOL:
		/* XXX XXX
		return usb_lld_ctrl_recv (dev, &hid_protocol, 1);
		*/
		hid_protocol = dev->dev_req.value & 0xff;
		return usb_lld_ctrl_ack (dev);

	default:
		return -1;
	}
}
