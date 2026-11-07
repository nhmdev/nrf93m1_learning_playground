# nRF93M1 Learning Playground

A personal playground for learning Nordic Semiconductor firmware development with the nRF93M1 Development Kit, nRF Connect SDK, and Zephyr RTOS.

The project starts with an extended version of Zephyr's Blinky sample and will grow through hands-on experiments with GPIO, interrupts, cellular connectivity, nRF Cloud, MQTT, and Bluetooth Low Energy (BLE).

The goal is to understand the logic, hardware interfaces, and debugging process behind each feature, one step at a time.

## Current functionality

The application currently toggles an LED when a button is pressed:

- The LED starts off.
- Both press and release edges trigger a GPIO interrupt and call a registered callback.
- The callback reschedules delayed work with a 30 ms delay to filter contact bounce.
- The work handler reads the button and compares it with the previously accepted state.
- A change from released to pressed gives a counting semaphore to signal the main thread.
- The main thread takes the semaphore and toggles the LED, which keeps its state after the button is released.
- Startup and accepted button events are reported through `printk()` over the UART console.
- The main thread waits for button signals without polling, while the system workqueue thread handles debouncing.
- After each LED update attempt, the main thread sleeps for one second to demonstrate how pending semaphore signals accumulate while button handling continues.
- GPIO write failures are logged, and the saved LED state is reverted before waiting for the next signal.

The application uses the board's `led0` and `sw0` devicetree aliases.

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

Example output for one accepted press:

```text
Button LED application started!
Before semaphore taken. Queue: 0
Button work executed: 0
Semaphore given for button_sem
Semaphore taken. k_uptime: 12511ms
Before semaphore taken. Queue: 0
```

The work handler prints the accepted press count and a message after giving the semaphore. The main thread prints the pending semaphore count before waiting and the system uptime in milliseconds after taking a signal. The uptime above is illustrative. The final line appears after the one-second sleep; its count can be higher if more presses were accepted during that time. Output order between the two threads can vary: the main thread may take the signal before the work handler prints its final message. Releases and unchanged button states do not give a semaphore signal or produce button-event messages.

## What I learned: debouncing and work scheduling

Mechanical buttons can produce several signal edges during a single press or release. The original approach ignored interrupts for a fixed time after an accepted press. This could still accept release bounce after a long press and toggle the LED a second time.

The current approach separates interrupt handling from button processing:

1. A `struct k_work_delayable` object represents work that can run after a delay.
2. `k_work_init_delayable()` connects the object to its handler before GPIO interrupts are enabled.
3. The GPIO callback calls `k_work_reschedule()` with `K_MSEC(30)`. It returns without sleeping or updating the LED.
4. Another edge during the delay restarts the 30 ms wait. Both edges are monitored with `GPIO_INT_EDGE_BOTH`, including changes during release.
5. When the delay expires, the work is submitted to Zephyr's system workqueue and its thread runs the handler when available.
6. The handler reads the button and records accepted state changes. It gives the semaphore only for a released-to-pressed transition.

This keeps the interrupt callback short and moves GPIO reads and console output into thread context. Local `static` variables preserve the previous button state and debug counter between handler calls. LED updates are handled separately by the main thread.

## What I learned: semaphores and communication between threads

The application uses two existing Zephyr threads: the main thread and the system workqueue thread. No additional application thread is needed for this exercise.

- A shared `struct k_sem` is initialized with `k_sem_init()` before interrupts are enabled. Its initial count is zero and its maximum count is ten.
- The work handler calls `k_sem_give()` when a new press is accepted. This records a pending signal; it does not wait for the main thread to finish processing it.
- The main loop calls `k_sem_take()` with `K_FOREVER`. If no signal is available, only the main thread waits; interrupts and the workqueue can continue running.
- A successful take consumes one signal. The main thread then attempts to toggle the LED, sleeps for one second, and returns to taking the next signal.
- Signals given before the main thread starts waiting remain pending, up to the semaphore's maximum count. Giving a full semaphore does not increase the count.

The processing chain is now:

```text
GPIO edge -> interrupt callback -> delayed work -> accepted press
         -> semaphore signal -> main thread -> LED update
```

A semaphore carries a count, not a job description or button identifier. If several sources give the same semaphore, the receiver cannot distinguish them. Message queues are a future option when events need to carry data.

The LED state belongs to the main thread and is kept in a local variable declared before the loop. If `gpio_pin_set_dt()` returns an error, the code logs it and restores the previous software state without ending the main loop. This does not retry the hardware write or verify the physical LED state.

During the exercise, a temporary sleep in the work handler demonstrated how the main thread could process a signal while the workqueue thread waited. That sleep has been removed so the shared system workqueue can process its next item promptly. Returning from a work handler ends that invocation, not the workqueue thread itself.

### Experiment: pending signals while the main thread sleeps

The main loop now uses `k_msleep(1000)` after each LED update attempt to simulate a slow task. Unlike sleeping in the shared workqueue handler, this pauses only the main thread and leaves the workqueue available to debounce further button events.

To observe the behavior, press and release the button several times faster than once per second. Accepted presses accumulate in the semaphore, and the main thread consumes them one at a time, with a one-second pause between LED update attempts. It can therefore continue toggling the LED after button activity has stopped.

The `Queue:` value printed through `k_sem_count_get()` is a snapshot of the pending signal count, not a queue containing individual job descriptions. The signal already taken by the main thread is excluded from that count. Up to ten signals can remain pending in addition to the task currently being processed; further gives while the semaphore is full are not retained. The count can change immediately after it is read.

The one-second delay is an intentional learning experiment, not part of the debounce timing.

### Current limitations

- A press or release shorter than the 30 ms settling interval may not be registered. This is a deliberate filtering tradeoff.
- The delay specifies when work becomes eligible for submission; execution can happen later if the system workqueue is busy.
- The button is initially assumed to be released. Startup behavior with the button already held has not been a focus of this exercise.

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
- [ ] Explore dedicated Zephyr threads, synchronization, and event handling further.
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
| `src/main.c` | Initialization, button debouncing, semaphore signaling, and LED handling in the main thread |
| `prj.conf` | Application Kconfig settings; currently enables GPIO |
| `CMakeLists.txt` | Zephyr application build definition, inherited from Blinky |

## References

- [nRF93M1 DK board documentation](https://docs.zephyrproject.org/latest/boards/nordic/nrf93m1dk/doc/index.html)
- [nRF Connect SDK documentation](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/index.html)
- [Zephyr GPIO API](https://docs.zephyrproject.org/latest/doxygen/html/group__gpio__interface.html)
- [Zephyr interrupt concepts](https://docs.zephyrproject.org/latest/kernel/services/interrupts.html)
- [Zephyr workqueue threads and delayed work](https://docs.zephyrproject.org/latest/kernel/services/threads/workqueue.html)
- [Zephyr semaphores](https://docs.zephyrproject.org/latest/kernel/services/synchronization/semaphores.html)
- [nRF Cloud documentation](https://docs.nordicsemi.com/bundle/nrf-cloud/page/index.html)

The project is based on Zephyr's Blinky sample. 
