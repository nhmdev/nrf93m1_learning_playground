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

static struct k_sem button_sem;

static void button_work_handler(struct k_work *work){
    static int dbg_cnt=0;
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
            k_sem_give(&button_sem);
            printk("Semaphore given for button_sem\n");
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
    
    ret = k_sem_init(&button_sem,0,10);
    if (ret < 0) {
        printk("Error init semaphore for button. Ret: %d",ret);
        return ret;
    }

    if (!gpio_is_ready_dt(&led) ||
        !gpio_is_ready_dt(&button)) {
        printk("GPIO device not ready!\n");
        return 0;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error for GPIO pin configure for LED. Ret: %d",ret);
        return ret;
    }

    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret < 0) {
        printk("Error for GPIO pin configure for button. Ret: %d",ret);
        return ret;
    }

    k_work_init_delayable(&button_work,button_work_handler);

    // Init button callback
    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));

    // Register callback
    ret = gpio_add_callback(button.port, &button_cb_data);
    if (ret < 0) {
        printk("Failed to register callback. Ret: %d\n",ret);
        return ret;
    }

    // Activate interrupt
    ret = gpio_pin_interrupt_configure_dt(&button,GPIO_INT_EDGE_BOTH);
    if (ret < 0) {
        printk("Failed to activate interrupt. Ret: %d\n",ret);
        return ret;
    }

    printk("Button LED application started!\n");

    int led_state = 0;

    while (1) {
        printk("Before semaphore taken. Queue: %d\n",k_sem_count_get(&button_sem));
        ret = k_sem_take(&button_sem,K_FOREVER);
        if(ret == 0){
           // We have taken the job.
           printk("Semaphore taken. k_uptime: %" PRId64 "ms\n",k_uptime_get());
           ret = gpio_pin_set_dt(&led, led_state = !led_state);
           if (ret < 0) {
                printk("Error chaning led. Ret: %d",ret);
                led_state = !led_state;
            }
            k_msleep(1000);
        }
    }

    return 0;
}