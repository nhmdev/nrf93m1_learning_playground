#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <inttypes.h>

#define LED_NODE DT_ALIAS(led0)
#define BUTTON_NODE DT_ALIAS(sw0)

static const struct gpio_dt_spec led =
    GPIO_DT_SPEC_GET(LED_NODE, gpios);

static const struct gpio_dt_spec button =
    GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);


static struct k_work_delayable button_work;

static void button_work_handler(struct k_work *work){
    static int dbg_cnt=0;
    static int led_state = 0;
    static int btn_prev_state = 0;
    int btn_current_state;

    btn_current_state = gpio_pin_get_dt(&button);
    if (btn_current_state < 0) {
        printk("Error while reading button\n Error: %d",btn_current_state);
        return;
    }

    if(btn_current_state != btn_prev_state) {
        btn_prev_state = btn_current_state;
        if(btn_prev_state == 1) {
            printk("Button work executed: %d \n", dbg_cnt++);
            
            gpio_pin_set_dt(&led, led_state = !led_state);
            printk("Callback button pressed: Cnt: %d k_uptime: %" PRId64 "ms \n",dbg_cnt, k_uptime_get());
        }
    }
}

void button_pressed(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins) {
    k_work_reschedule(&button_work,K_MSEC(30));
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

    k_work_init_delayable(&button_work,button_work_handler);

    // Init button callback
    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));

    // Register callback
    ret = gpio_add_callback(button.port, &button_cb_data);
    if (ret < 0) {
        printk("Failed to register callback\n");
        return ret;
    }

    // Activate interrupt
    ret = gpio_pin_interrupt_configure_dt(&button,GPIO_INT_EDGE_BOTH);
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