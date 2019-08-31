void hid_setup_endpoints(struct usb_dev *dev,
				uint16_t interface, int stop);
void hid_tx_done(uint8_t ep_num, uint16_t len);
int hid_data_setup(struct usb_dev *dev, uint16_t interface);
void hid_ctrl_write_finish(struct usb_dev *dev, uint16_t interface);
void hid_init(void);

