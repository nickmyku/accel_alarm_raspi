# Accelerometer graph for the RP2040-Touch-LCD-1.28-B

Firmware for the [Waveshare RP2040-Touch-LCD-1.28-B](https://www.waveshare.com/rp2040-touch-lcd-1.28-b.htm). It reads the onboard QMI8658 accelerometer, draws the last five minutes of the X, Y, and Z axes on the round display, and pulses an expansion-connector pin when the shock crosses a limit you can change.

X is red, Y is green, Z is blue. All three share one plot. The right edge is now and the left edge is five minutes ago.

## What you see

The trace is dynamic acceleration in milli-g (1000 mg = 1 g), with gravity taken out. A raw plot would be dominated by the 1 g of gravity and a footstep would be a couple of pixels tall. Each 100 ms is stored as the largest swing in that slice, and each column of the graph draws the min-to-max of the slices it covers, so a footstep stays a visible spike instead of being averaged away.

The dashed yellow lines are the alarm limit, above and below zero. The big number is that limit in mg. `PK` is the largest dynamic magnitude in the last two seconds, which is the reading to use when you decide where the limit should sit.

For the first 1.5 s after power-up the screen says `SETTLING` and the alarm pin stays low while the baseline locks onto gravity.

## Alarm pin

`GPIO0` on the SH1.0 expansion connector. The pin idles low and goes high for 250 ms when the dynamic magnitude `sqrt(x²+y²+z²)` reaches the limit, then it goes low again. It will not fire a second time until the magnitude drops below the limit by 15 mg, so one shock is one pulse.

GPIO0 through GPIO5 are the six pins on that connector. The display, touch controller, and IMU already use the other GPIOs. Change the pin in `config.h`:

```c
#define ALARM_GPIO 0
#define ALARM_PULSE_MS 250
#define ALARM_LIMIT_MG_DEFAULT 50
```

The default limit is 50 mg. That is in the range of a footstep on the same table as the board. Raise it if walking nearby sets it off, or lower it if you want lighter steps. The accelerometer is set to ±2 g with its internal low-pass off, so those steps are not filtered out before the firmware sees them.

## Changing the limit

Tap the left side of the glass, or swipe left, to lower the limit by 5 mg. Tap or swipe right to raise it. The center of the glass does nothing, so you can rest a finger there. Hold a side and it keeps stepping. If left and right feel backwards, set `TOUCH_MIRROR_X` to `1` in `config.h` and rebuild.

Over USB serial (115200 baud is not required; it is a USB CDC port) the same control is:

```text
limit 80
status
```

The new value is written to flash two seconds after you stop changing it, and it is restored on the next boot.

## Build

Install the Arm embedded toolchain and the [Pico SDK](https://github.com/raspberrypi/pico-sdk), then:

```sh
export PICO_SDK_PATH=~/pico-sdk
cmake -S . -B build
cmake --build build
```

`build/accel_graph.uf2` is the file to copy to the board. Hold BOOT, tap RESET, and a `RPI-RP2` drive appears. Copy the UF2 onto it. The board reboots into the graph. It does not wait for a USB terminal, so it also runs from the battery connector.

Host-side checks, including a rendered preview of the screen, do not need the Pico SDK:

```sh
sh tests/run.sh
```

That compiles the filter, the five-minute history, the alarm pulse, and the screen drawing, and writes `build/preview.png`.
