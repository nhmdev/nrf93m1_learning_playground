#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <inttypes.h>

// Load GPIO Device Tree LED and Button
#define LED_NODE_GREEN DT_ALIAS(led2)
#define LED_NODE_RED DT_ALIAS(led1)
#define BUTTON_NODE_1 DT_ALIAS(sw0)
#define BUTTON_NODE_2 DT_ALIAS(sw1)


static const struct gpio_dt_spec led_green =
    GPIO_DT_SPEC_GET(LED_NODE_GREEN, gpios);
    
static const struct gpio_dt_spec led_red =
    GPIO_DT_SPEC_GET(LED_NODE_RED, gpios);

static const struct gpio_dt_spec button_1 =
    GPIO_DT_SPEC_GET(BUTTON_NODE_1, gpios);

static const struct gpio_dt_spec button_2 =
    GPIO_DT_SPEC_GET(BUTTON_NODE_2, gpios);

// Add Delayed work schedulings
static struct k_work_delayable button1_work;
static struct k_work_delayable button2_work;

// Add semaphores

// Add Callbacks
static struct gpio_callback button1_cb_data;
static struct gpio_callback button2_cb_data;

// Add Threads and stacks
static struct k_thread led_thread_data;
K_THREAD_STACK_DEFINE(led_thread_stack, 1024);

// Add Message Queues
K_MSGQ_DEFINE(button_msgq,sizeof(int),10,1);

// Add Enum's
enum led_mode {
    LED_OFF,
    LED_GREEN,
    LED_RED
};

static void button1_work_handler(struct k_work *work){
    static int dbg_cnt=0;
    static int btn_prev_state = 0;
    int btn_current_state;
    int button_id = 1;
    int ret;

    btn_current_state = gpio_pin_get_dt(&button_1);
    if (btn_current_state < 0) {
        printk("Error while reading button 1\n Error: %d",btn_current_state);
        return;
    }

    if(btn_current_state != btn_prev_state) {
        btn_prev_state = btn_current_state;
        if(btn_prev_state == 1) {
            printk("Button 1 work executed: %d \n", dbg_cnt++);
            ret = k_msgq_put(&button_msgq, &button_id, K_NO_WAIT);
            if (ret < 0) {
                printk("Error sending to message queue. Ret: %d",ret);
            }
            if(ret == 0) printk("Message has been sent to queue");
        }
    }
}

static void button2_work_handler(struct k_work *work){
    static int dbg_cnt=0;
    static int btn_prev_state = 0;
    int btn_current_state;
    int button_id = 2;
    int ret;

    btn_current_state = gpio_pin_get_dt(&button_2);
    if (btn_current_state < 0) {
        printk("Error while reading button 2\n Error: %d",btn_current_state);
        return;
    }

    if(btn_current_state != btn_prev_state) {
        btn_prev_state = btn_current_state;
        if(btn_prev_state == 1) {
            printk("Button 2 work executed: %d \n", dbg_cnt++);
            ret = k_msgq_put(&button_msgq, &button_id, K_NO_WAIT);
            if (ret < 0) {
                printk("Error sending to message queue. Ret: %d",ret);
            }
            if(ret == 0) printk("Message has been sent to queue");
        }
    }
}

void button1_pressed(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins) {
    k_work_reschedule(&button1_work,K_MSEC(30));
}

void button2_pressed(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins) {
    k_work_reschedule(&button2_work,K_MSEC(30));
}

static void led_thread_entry(void *p1, void *p2, void *p3){
    int ret;
    int button_id;
    enum led_mode current_led_mode = LED_OFF;
    enum led_mode next_led_mode;
    
    printk("LED Thread entry started\n");

    while (1) {
        ret = k_msgq_get(&button_msgq,&button_id,K_FOREVER);
        if(ret == 0){
           // We have received the message correctly
           printk("Message received from queue. k_uptime: %" PRId64 "ms\n",k_uptime_get());

           if(button_id == 1) {
                if(current_led_mode == LED_GREEN) {
                    next_led_mode = LED_OFF;
                } else {
                    next_led_mode = LED_GREEN;
                }
           } else if(button_id == 2) {
                if(current_led_mode == LED_RED) {
                    next_led_mode = LED_OFF;
                } else {
                    next_led_mode = LED_RED;
                }
           } else {
            printk("Invalid button id. %d",button_id);
            continue;
           }
           int ret_r;
           int ret_g;

           switch (next_led_mode)
           {
           case LED_OFF:
            ret_r = gpio_pin_set_dt(&led_red, 0);
                if (ret_r < 0) {
                    printk("Error turning off red led. Ret: %d",ret_r);
                }
            ret_g = gpio_pin_set_dt(&led_green, 0);
            if (ret_g < 0) {
                printk("Error turning off green led. Ret: %d",ret_g);
            }
            break;
           case LED_GREEN:
            ret_r = gpio_pin_set_dt(&led_red, 0);
                if (ret_r < 0) {
                    printk("Error turning off red led. Ret: %d",ret_r);
                }
            ret_g = gpio_pin_set_dt(&led_green, 1);
            if (ret_g < 0) {
                printk("Error turning on green led. Ret: %d",ret_g);
            }
            break;
            case LED_RED:
            ret_g = gpio_pin_set_dt(&led_green, 0);
                if (ret_g < 0) {
                    printk("Error turning off green led. Ret: %d",ret_g);
                }
            ret_r = gpio_pin_set_dt(&led_red, 1);
            if (ret_r < 0) {
                printk("Error turning on red led. Ret: %d",ret_r);
            }
            break;
           default:
                printk("Error. Invalid next_led_mode");
                continue;
           }
           if(ret_r == 0 && ret_g == 0) {
                current_led_mode = next_led_mode;
            }
        }
    }
}

int main(void)
{
    int ret;

    // Check GPIO is ready
    if (!gpio_is_ready_dt(&led_green) ||
        !gpio_is_ready_dt(&led_red) ||
        !gpio_is_ready_dt(&button_1) ||
        !gpio_is_ready_dt(&button_2)) {
            printk("GPIO device not ready!\n");
            return 0;
    }

    // Configure LEDs
    ret = gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error for GPIO pin configure for Green LED. Ret: %d",ret);
        return ret;
    }

    ret = gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Error for GPIO pin configure for Red LED. Ret: %d",ret);
        return ret;
    }

    // Configure Buttons
    ret = gpio_pin_configure_dt(&button_1, GPIO_INPUT);
    if (ret < 0) {
        printk("Error for GPIO pin configure for button. Ret: %d",ret);
        return ret;
    }

    ret = gpio_pin_configure_dt(&button_2, GPIO_INPUT);
    if (ret < 0) {
        printk("Error for GPIO pin configure for button 2. Ret: %d",ret);
        return ret;
    }

    // Initiate delayed Work Thread
    k_work_init_delayable(&button1_work,button1_work_handler);
    k_work_init_delayable(&button2_work,button2_work_handler);

    // Init button callback
    gpio_init_callback(&button1_cb_data, button1_pressed, BIT(button_1.pin));
    gpio_init_callback(&button2_cb_data, button2_pressed, BIT(button_2.pin));

    // Register callback
    ret = gpio_add_callback(button_1.port, &button1_cb_data);
    if (ret < 0) {
        printk("Failed to register callback for button 1. Ret: %d\n",ret);
        return ret;
    }

    ret = gpio_add_callback(button_2.port, &button2_cb_data);
    if (ret < 0) {
        printk("Failed to register callback for button 2. Ret: %d\n",ret);
        return ret;
    }

    // Activate interrupt
    ret = gpio_pin_interrupt_configure_dt(&button_1,GPIO_INT_EDGE_BOTH);
    if (ret < 0) {
        printk("Failed to activate interrupt for button 1. Ret: %d\n",ret);
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(&button_2,GPIO_INT_EDGE_BOTH);
    if (ret < 0) {
        printk("Failed to activate interrupt for button 2. Ret: %d\n",ret);
        return ret;
    }

    // Application is ready... 
    printk("Button LED application started!\n");

    // Start new thread for LEDs
    k_tid_t tid_led = k_thread_create(&led_thread_data, 
        led_thread_stack, 
        K_THREAD_STACK_SIZEOF(led_thread_stack),
        led_thread_entry, 
        NULL, 
        NULL, 
        NULL, 
        5, 
        0, 
        K_NO_WAIT);
   

    return 0;
}