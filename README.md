# nRF93M1 Learning Playground

A personal playground for learning Nordic Semiconductor firmware development with the nRF93M1 Development Kit, nRF Connect SDK, and Zephyr RTOS.

The project starts with an extended version of Zephyr's Blinky sample and will grow through hands-on experiments with GPIO, interrupts, cellular connectivity, nRF Cloud, MQTT, and Bluetooth Low Energy (BLE).

The goal is to understand the logic, hardware interfaces, and debugging process behind each feature, one step at a time.

## Current functionality

The application currently toggles an LED when a button is pressed:

- The LED starts off.
- A button press triggers a GPIO interrupt and calls a registered callback.
- The callback toggles the LED, which keeps its state after the button is released.
- A simple time filter rejects events occurring within 50 ms of the last accepted event.
- Startup and accepted button events are reported through `printk()` over the UART console.
- The main thread sleeps while the GPIO callback handles button events.

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

Expected output:

```text
Button LED application started!
Callback button pressed
```

Each accepted button event prints another `Callback button pressed` line. Events rejected by the time filter do not produce a message.

## Known issues and ongoing experiments

- Short button presses sometimes behave inconsistently. Debouncing and event handling are still being investigated.
- The current 50 ms filter limits how frequently events can be accepted; it does not verify that the button has reached a stable pressed or released state.
- Console output currently runs inside the GPIO callback. As the project grows, deferred work and keeping interrupt handlers short will be explored.

## Learning roadmap

- [x] Start from the Blinky sample.
- [x] Read a button through GPIO polling.
- [x] Preserve LED state and detect new button presses.
- [x] Switch to GPIO interrupts and callbacks.
- [ ] Improve debouncing and diagnose short button presses.
- [ ] Explore Zephyr threads, work queues, and event handling.
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
| `src/main.c` | Application initialization and button/LED logic |
| `prj.conf` | Application Kconfig settings; currently enables GPIO |
| `CMakeLists.txt` | Zephyr application build definition, inherited from Blinky |

## References

- [nRF93M1 DK board documentation](https://docs.zephyrproject.org/latest/boards/nordic/nrf93m1dk/doc/index.html)
- [nRF Connect SDK documentation](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/index.html)
- [Zephyr GPIO API](https://docs.zephyrproject.org/latest/doxygen/html/group__gpio__interface.html)
- [Zephyr interrupt concepts](https://docs.zephyrproject.org/latest/kernel/services/interrupts.html)
- [nRF Cloud documentation](https://docs.nordicsemi.com/bundle/nrf-cloud/page/index.html)

The project is based on Zephyr's Blinky sample. 
