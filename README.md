# embedded-courses

Hands-on courses in Embedded and Low-Level Engineering. Every lab is a plain
`arm-none-eabi-gcc` project, and the same binary runs in three places:

- **in the browser**: the site compiles the learner's code and streams an emulated board (LED, button, serial console) to the page
- **in QEMU** on the learner's machine: `make qemu`
- **on real hardware**: a WeAct Black Pill (STM32F411), flashed over USB

The first course, **Build a Mini OS on STM32**, follows
[itamar93/stm32-mini-os](https://github.com/itamar93/stm32-mini-os). Modules 1
(blinky) and 2 (UART) are free; the rest of the course will be paid.

## Repository layout

| Path | What it is |
| --- | --- |
| `courses/<course>/course.yaml` | Course title, board and module list (free or paid) |
| `courses/<course>/<module>/lesson.mdx` | The lesson text |
| `courses/<course>/<module>/lab/` | Starter project the learner edits (Makefile, linker script, startup, HAL, app) |
| `courses/<course>/<module>/solution/` | Files that replace the starter's TODOs with a working answer |
| `courses/<course>/<module>/lab.yaml` | Editable files and the automatic checks |
| `emulator/qemu/` | The `blackpill-f411` QEMU machine: GPIO models and a pin monitor, as a patch on QEMU 10.2.4 |
| `services/runner/` | Go service: builds labs with GCC, runs them in QEMU, streams UART and pin events, grades checks |
| `site/` | Astro website: home, course pages, lessons with the lab workbench |

## Run it locally

You need `arm-none-eabi-gcc`, `make`, Go 1.24, Node 22 and QEMU's build dependencies
(`ninja-build`, `libglib2.0-dev`, `libpixman-1-dev`, `python3-venv`, `flex`, `bison`).

```bash
# 1. Build QEMU with the Black Pill machine (once, about 5 minutes)
emulator/qemu/build.sh

# 2. Start the build-and-run service on :8080
cd services/runner && go run . &

# 3. Start the site on :4321
cd site && npm install && npm run dev
```

Open http://localhost:4321/courses/mini-os/01-blinky and click **Run**.

## The `blackpill-f411` machine

```bash
qemu-system-arm -M blackpill-f411 -display none -kernel build/kernel.elf \
  -serial null -serial stdio \
  -chardev file,id=pins,path=/dev/stderr
```

- USART2 is QEMU's second serial port.
- Every change of a driven output pin is written to the chardev named `pins` as a JSON line: `{"t":502968183,"pin":"PC13","v":0}` (`t` is virtual time in ns).
- Writing `PA0=0` to the same chardev drives an input pin; the KEY button is PA0.
- The board runs at the 16 MHz HSI reset clock. RCC enables are stored but clock configuration is not modelled.

## Lab checks

`lab.yaml` checks are graded by the runner after running the firmware for `run_ms`:

```yaml
checks:
  - name: PC13 is an output
    hint: Write 01 into bits 27:26 of GPIOC->MODER.
    expect: { reg: GPIOC_MODER, mask: 0x0C000000, equals: 0x04000000 }
  - name: LED blinks every 500 ms
    expect: { pin: PC13, toggles: { min: 5, max: 7 } }
  - name: Board says hello
    expect: { uart_contains: "Hello, World!" }
```

## Analytics

Set these in `site/.env` (see `site/.env.example`); both are off when empty:

- `PUBLIC_CF_ANALYTICS_TOKEN`: Cloudflare Web Analytics, for visits, top pages, referrers and countries, with no cookies
- `PUBLIC_POSTHOG_KEY`: PostHog, for the course funnel (`lab_run`, `lab_passed`, `lab_check_failed`, `lab_build_failed`)

## Deploying

- **Site**: static (`npm run build` → `site/dist`), for example on Cloudflare Pages. Set `PUBLIC_RUNNER_URL` to the runner's public URL.
- **Runner**: `docker build -f services/runner/Dockerfile .` builds QEMU, the service and the toolchain into one image. Learner code is untrusted: run it under gVisor (`--runtime=runsc`) with no network, and set `-origin` to the site's URL.
