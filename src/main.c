#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define LED_NODE DT_ALIAS(led0)
#define BUTTON_NODE DT_ALIAS(sw0)

static const struct gpio_dt_spec led =
    GPIO_DT_SPEC_GET(LED_NODE, gpios);

static const struct gpio_dt_spec button =
    GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

void button_pressed(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins) {
    
    static int led_state = 0;
    static int64_t cb_last_run = 0;

    if((k_uptime_get() - cb_last_run) > 50) {
        gpio_pin_set_dt(&led, led_state = !led_state);
        printk("Callback button pressed\n");
        cb_last_run = k_uptime_get();
    }
}
static struct gpio_callback button_cb_data;

int main(void)
{
    int ret;

    if (!gpio_is_ready_dt(&led) ||
        !gpio_is_ready_dt(&button)) {
        printk("GPIO device not ready!\n");
        return 0;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        return 0;
    }

    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret < 0) {
        return 0;
    }

    // Init button callback
    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));

    // Register callback
    ret = gpio_add_callback(button.port, &button_cb_data);
    if (ret < 0) {
        printk("Failed to register callback\n");
        return ret;
    }

    // Activate interrupt
    ret = gpio_pin_interrupt_configure_dt(&button,GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        printk("Failed to activate interrupt\n");
        return ret;
    }

    printk("Button LED application started!\n");


    while (1) {

        
        

        k_msleep(1000);
    }

    return 0;
}