/* forward declare to shut up warnings */
struct usb_dev;

void hid_setup_endpoints(struct usb_dev *dev,
				uint16_t interface, int stop);
void hid_tx_done(uint8_t ep_num, uint16_t len);
int hid_data_setup(struct usb_dev *dev, uint16_t interface);
void hid_ctrl_write_finish(struct usb_dev *dev, uint16_t interface);
void hid_init(void);
int hid_key_pressed(uint8_t hidcode);
int hid_key_released(uint8_t hidcode);
int hid_key_releaseAll(void);
int hid_mouse_move(int8_t x, int8_t y);
/* XXX does not send, assume buttons are follwed immediately by move */
int hid_mouse_set_buttons(uint8_t buttons);
int hid_mouse_button_press(uint8_t button);
int hid_mouse_button_release(uint8_t button);

