#ifndef ___LORA_GPIO_H___
#define ___LORA_GPIO_H___ 1

#define RFM95_DIO0                  26
#define RFM95_RESET                 45

int gpio_init(void);
void gpio_deinit(void);
int gpio_reset_rfm95(void);
int gpio_dio0_callback(void);

#endif /* ___LORA_GPIO_H___ */
