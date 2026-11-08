# nRF93M1 Learning Playground

A personal playground for learning Nordic Semiconductor firmware development with the nRF93M1 Development Kit, nRF Connect SDK, and Zephyr RTOS.

The project starts with an extended version of Zephyr's Blinky sample and will grow through hands-on experiments with GPIO, interrupts, cellular connectivity, nRF Cloud, MQTT, and Bluetooth Low Energy (BLE).

The goal is to understand the logic, hardware interfaces, and debugging process behind each feature, one step at a time.

## Current functionality

The application uses two buttons to select or turn off the green and red LEDs:

- Both LEDs start off.
- Both press and release edges trigger a GPIO interrupt and call a registered callback.
- The callback reschedules delayed work with a 30 ms delay to filter contact bounce.
- Each button has its own callback object, delayed work object, and saved button state.
- Each work handler reads its button and compares it with the previously accepted state.
- A change from released to pressed sends the button ID through a shared message queue.
- A dedicated LED thread receives messages and selects the next LED mode: off, green, or red.
- Startup and accepted button events are reported through `printk()` over the UART console.
- The LED thread waits without polling, while the system workqueue thread handles debouncing for both buttons.
- GPIO write failures are logged. The saved LED mode is updated only when both LED writes succeed.
- The main thread initializes the application, starts the LED thread, and returns.

| Current mode | Button 1 | Button 2 |
| --- | --- | --- |
| Off | Green | Red |
| Green | Off | Red |
| Red | Green | Off |

The selected mode remains after release. Pressing the button for the active color turns it off; pressing the other button switches colors.

| Function | Devicetree alias | GPIO pin |
| --- | --- | --- |
| Green LED | `led2` | P2.10 |
| Red LED | `led1` | P2.8 |
| Button 1 | `sw0` | P0.4 |
| Button 2 | `sw1` | P1.9 |

The board's devicetree supplies port, pin, and polarity information through `DT_ALIAS()` and `GPIO_DT_SPEC_GET()`. Both buttons use pull-ups and active-low logic.

## Hardware and software

| Component | Current setup |
| --- | --- |
| Development board | Nordic Semiconductor nRF93M1 DK |
| Board revision | 0.4.0, as selected in the existing build |
| Application target | `nrf93m1dk@0.4.0/nrf54l15/cpuapp` |
| SDK | nRF Connect SDK development checkout (`main`), currently reporting version `3.4.99` |
| RTOS | Zephyr, supplied by nRF Connect SDK |
| Console | UART, 115200 baud, 8 data bits, no parity, 1 stop bit |

The firmware runs on the DK's **nRF54L15 host MCU**. The **nRF93M1 cellular modem** is a separate module connected to the host over UART. Future BLE experiments will use the nRF54L15.

The SDK checkout is not pinned to a specific commit yet. The version above identifies the current development setup rather than a released SDK compatibility requirement.

## Building and flashing

Install nRF Connect SDK with support for the nRF93M1 DK and its matching toolchain. Connect the board through its programming/debug USB connection.

### nRF Connect for VS Code

1. Open this repository as an application in nRF Connect for VS Code.
2. Create a build configuration for `nrf93m1dk/nrf54l15/cpuapp`, selecting board revision `0.4.0` to match the current setup.
3. Build the application.
4. Flash it to the connected board.

### Command line

From the repository root, in a terminal configured for the selected nRF Connect SDK and toolchain:

```sh
west build -p always -b nrf93m1dk@0.4.0/nrf54l15/cpuapp --sysbuild .
west flash -d build
```

Use a board revision that matches your hardware. These commands target the current setup; other SDK versions or board revisions may need adjustments.

After moving or renaming the repository, create a pristine build or a new build configuration. Existing CMake build files can retain the old absolute source path.

## Viewing console output

In nRF Connect for VS Code:

1. Find the board under **Connected Devices** and expand its entry.
2. Select **Connect to Serial Port** next to the appropriate serial port.
3. Choose the **115200 8n1** preset, with hardware flow control disabled.
4. Reset the board to see the startup message.

Illustrative output for one accepted press (line breaks added for readability):

```text
Button LED application started!
LED Thread entry started
Button 1 work executed: 0
Message has been sent to queue
Message received from queue. k_uptime: 12511ms
```

Each button work handler prints its own accepted press count and reports successful queue submission. The LED thread prints the system uptime when it receives a message. Output order between threads can vary, and some messages currently lack a trailing newline. Releases and unchanged button states do not send messages.

## What I learned: debouncing and work scheduling

Mechanical buttons can produce several signal edges during a single press or release. The original approach ignored interrupts for a fixed time after an accepted press. This could still accept release bounce after a long press and toggle the LED a second time.

The current approach separates interrupt handling from button processing:

1. A `struct k_work_delayable` object represents work that can run after a delay.
2. `k_work_init_delayable()` connects the object to its handler before GPIO interrupts are enabled.
3. The GPIO callback calls `k_work_reschedule()` with `K_MSEC(30)`. It returns without sleeping or updating the LED.
4. Another edge during the delay restarts the 30 ms wait. Both edges are monitored with `GPIO_INT_EDGE_BOTH`, including changes during release.
5. When the delay expires, the work is submitted to Zephyr's system workqueue and its thread runs the handler when available.
6. The handler reads its button and records accepted state changes. It sends the button ID only for a released-to-pressed transition.

This keeps the interrupt callbacks short and moves GPIO reads and console output into thread context. Local `static` variables preserve each button's previous state and debug counter between handler calls. The two work objects share the system workqueue thread; they do not create separate threads. LED updates are handled by the dedicated LED thread.

## What I learned: semaphores and communication between threads

An earlier exercise used a counting semaphore to signal accepted presses from the system workqueue to the main thread. It started at zero and retained up to ten pending signals. The main thread waited with `k_sem_take()` and updated the LED after taking a signal.

A one-second sleep simulated slow processing while button handling continued. Logging `k_sem_count_get()` demonstrated pending signals, consumption, and the count limit. Giving a semaphore did not wait for the receiver to finish its work.

The semaphore and artificial delay have now been replaced by a message queue and a dedicated LED thread. The queue carries the button identity as well as recording that an event occurred.

## What I learned: creating a dedicated thread

- `struct k_thread` stores the thread's control information.
- `K_THREAD_STACK_DEFINE()` reserves a stack; the LED thread requests 1024 bytes.
- The entry function accepts three `void *` arguments, unused in this exercise.
- `k_thread_create()` starts the thread after initialization, with priority `5`, no special options, and `K_NO_WAIT` as its start delay.
- The LED thread keeps its state in local variables and waits for messages instead of polling.
- Returning from `main()` ends the main thread while the LED thread and system workqueue continue running.

Lower numeric priorities mean higher priority in Zephyr. Waiting allows other ready threads to run. Stack usage should be measured as the thread's responsibilities grow.

## What I learned: message queues and event handling

`K_MSGQ_DEFINE()` defines and initializes a queue holding up to ten `int` messages. Button 1 sends `1`; button 2 sends `2`.

The work handlers use `k_msgq_put()` with `K_NO_WAIT`, so a full queue produces an error rather than blocking the shared system workqueue. The message value is copied, allowing each sender to use a normal local variable. The LED thread receives messages in queue order using `k_msgq_get()` with `K_FOREVER`.

```text
Button edge -> GPIO callback -> per-button delayed work
            -> button ID in message queue -> LED thread -> LED mode
```

An `enum led_mode` names the alternatives `LED_OFF`, `LED_GREEN`, and `LED_RED`. The received button ID and current mode determine the desired mode. A `switch` applies it to both outputs, switching off the other color before turning on the selected color.

Only the LED thread modifies the saved mode. Both GPIO return values are checked separately; the saved mode changes only if both writes succeed. Invalid button IDs and invalid desired modes are logged and skipped.

### Current limitations

- A press or release shorter than the 30 ms settling interval may not be registered. This is a deliberate filtering tradeoff.
- The delay specifies when work becomes eligible for submission; execution can happen later if the system workqueue is busy.
- Both buttons are initially assumed to be released; startup with a button held has not been a focus.
- A full message queue rejects a new message, which is logged without retry.
- LED writes are separate operations. Partial failure can leave physical outputs different from the saved mode; there is no automatic rollback or hardware-state verification.

## Learning roadmap

- [x] Start from the Blinky sample.
- [x] Read a button through GPIO polling.
- [x] Preserve LED state and detect new button presses.
- [x] Switch to GPIO interrupts and callbacks.
- [x] Improve debouncing and diagnose short button presses.
- [x] Learn delayed work scheduling and move button processing to the system workqueue.
- [x] Use a counting semaphore to signal the main thread and move LED updates there.
- [x] Check GPIO write errors and restore the saved LED state on failure.
- [x] Simulate slow processing in the main thread and observe pending semaphore signals and their count limit.
- [x] Explore dedicated Zephyr threads, synchronization, and event handling further.
- [ ] Power on and communicate with the nRF93M1 modem.
- [ ] Establish a cellular network connection.
- [ ] Connect to nRF Cloud and send initial messages.
- [ ] Learn MQTT connections, topics, publishing, and subscriptions.
- [ ] Explore additional nRF Cloud features and device interactions.
- [ ] Experiment with BLE advertising, connections, and GATT on the nRF54L15.

Cellular connectivity, nRF Cloud, MQTT, and BLE are planned learning topics and are not implemented in the current application.

## Project layout

| Path | Purpose |
| --- | --- |
| `src/main.c` | Initialization, two-button debounce, message passing, and LED thread mode logic |
| `prj.conf` | Application Kconfig settings; currently enables GPIO |
| `CMakeLists.txt` | Zephyr application build definition, inherited from Blinky |

## References

- [nRF93M1 DK board documentation](https://docs.zephyrproject.org/latest/boards/nordic/nrf93m1dk/doc/index.html)
- [nRF Connect SDK documentation](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/index.html)
- [Zephyr GPIO API](https://docs.zephyrproject.org/latest/doxygen/html/group__gpio__interface.html)
- [Zephyr interrupt concepts](https://docs.zephyrproject.org/latest/kernel/services/interrupts.html)
- [Zephyr workqueue threads and delayed work](https://docs.zephyrproject.org/latest/kernel/services/threads/workqueue.html)
- [Zephyr threads](https://docs.zephyrproject.org/latest/kernel/services/threads/index.html)
- [Zephyr message queues](https://docs.zephyrproject.org/latest/kernel/services/data_passing/message_queues.html)
- [Zephyr devicetree access from C](https://docs.zephyrproject.org/latest/build/dts/api-usage.html)
- [Zephyr semaphores](https://docs.zephyrproject.org/latest/kernel/services/synchronization/semaphores.html)
- [nRF Cloud documentation](https://docs.nordicsemi.com/bundle/nrf-cloud/page/index.html)

The project is based on Zephyr's Blinky sample. 
