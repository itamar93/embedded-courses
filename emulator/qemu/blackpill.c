/*
 * WeAct "Black Pill" STM32F411 board for embedded-courses
 *
 * The STM32F411 shares its memory map and core peripherals with the
 * STM32F405, so this board reuses the F405 SoC model, runs it at the
 * 16 MHz HSI reset clock, and adds what the courses need on top:
 *
 *  - an STM32F4 GPIO model for ports A-E and H (MODER, ODR, IDR, BSRR, ...)
 *  - a pin monitor on the chardev with id "pins": every change of a driven
 *    output pin is reported as one JSON line, and lines such as "PA0=1"
 *    written to the chardev drive input pins (the KEY button is PA0)
 *
 * The on-board LED is PC13 (active low).
 *
 * SPDX-License-Identifier: MIT
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/timer.h"
#include "hw/boards.h"
#include "hw/irq.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "hw/qdev-clock.h"
#include "chardev/char.h"
#include "chardev/char-fe.h"
#include "hw/arm/stm32f405_soc.h"
#include "hw/arm/boot.h"
#include "hw/arm/machines-qom.h"

/* The STM32F411 resets onto the 16 MHz HSI oscillator */
#define BLACKPILL_SYSCLK_HZ 16000000ULL
#define BLACKPILL_FLASH_SIZE (512 * 1024)

#define STM32F4_GPIO_NPINS 16

/* ------------------------------------------------------------------------ */
/* STM32F4 GPIO port                                                        */
/* ------------------------------------------------------------------------ */

#define TYPE_STM32F4_GPIO "stm32f4-gpio"
OBJECT_DECLARE_SIMPLE_TYPE(STM32F4GpioState, STM32F4_GPIO)

#define GPIO_MODER   0x00
#define GPIO_OTYPER  0x04
#define GPIO_OSPEEDR 0x08
#define GPIO_PUPDR   0x0c
#define GPIO_IDR     0x10
#define GPIO_ODR     0x14
#define GPIO_BSRR    0x18
#define GPIO_LCKR    0x1c
#define GPIO_AFRL    0x20
#define GPIO_AFRH    0x24

struct STM32F4GpioState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    qemu_irq out[STM32F4_GPIO_NPINS];

    uint32_t moder_reset;
    uint32_t pupdr_reset;

    uint32_t moder;
    uint32_t otyper;
    uint32_t ospeedr;
    uint32_t pupdr;
    uint32_t odr;
    uint32_t lckr;
    uint32_t afr[2];

    /* Level driven onto each pin from outside (button, wire) */
    uint32_t ext_level;
    uint32_t ext_driven;

    /* Last level reported per output pin, -1 when not driven */
    int8_t reported[STM32F4_GPIO_NPINS];
};

static unsigned stm32f4_gpio_mode(STM32F4GpioState *s, int pin)
{
    return extract32(s->moder, pin * 2, 2);
}

static uint32_t stm32f4_gpio_idr(STM32F4GpioState *s)
{
    uint32_t idr = 0;

    for (int pin = 0; pin < STM32F4_GPIO_NPINS; pin++) {
        bool level;

        if (stm32f4_gpio_mode(s, pin) == 1) {
            level = extract32(s->odr, pin, 1);
        } else if (extract32(s->ext_driven, pin, 1)) {
            level = extract32(s->ext_level, pin, 1);
        } else {
            /* Floating input reads its pull: pull-up 01 -> 1, else 0 */
            level = extract32(s->pupdr, pin * 2, 2) == 1;
        }
        idr = deposit32(idr, pin, 1, level);
    }
    return idr;
}

/* Report every output pin whose driven level changed */
static void stm32f4_gpio_update(STM32F4GpioState *s)
{
    for (int pin = 0; pin < STM32F4_GPIO_NPINS; pin++) {
        int level = -1;

        if (stm32f4_gpio_mode(s, pin) == 1) {
            level = extract32(s->odr, pin, 1);
        }
        if (level != s->reported[pin]) {
            s->reported[pin] = level;
            if (level >= 0) {
                qemu_set_irq(s->out[pin], level);
            }
        }
    }
}

static uint64_t stm32f4_gpio_read(void *opaque, hwaddr addr, unsigned size)
{
    STM32F4GpioState *s = opaque;

    switch (addr) {
    case GPIO_MODER:
        return s->moder;
    case GPIO_OTYPER:
        return s->otyper;
    case GPIO_OSPEEDR:
        return s->ospeedr;
    case GPIO_PUPDR:
        return s->pupdr;
    case GPIO_IDR:
        return stm32f4_gpio_idr(s);
    case GPIO_ODR:
        return s->odr;
    case GPIO_BSRR:
        return 0;
    case GPIO_LCKR:
        return s->lckr;
    case GPIO_AFRL:
        return s->afr[0];
    case GPIO_AFRH:
        return s->afr[1];
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: bad offset 0x%" HWADDR_PRIx "\n",
                      __func__, addr);
        return 0;
    }
}

static void stm32f4_gpio_write(void *opaque, hwaddr addr, uint64_t val64,
                               unsigned size)
{
    STM32F4GpioState *s = opaque;
    uint32_t val = val64;

    switch (addr) {
    case GPIO_MODER:
        s->moder = val;
        break;
    case GPIO_OTYPER:
        s->otyper = val & 0xffff;
        break;
    case GPIO_OSPEEDR:
        s->ospeedr = val;
        break;
    case GPIO_PUPDR:
        s->pupdr = val;
        break;
    case GPIO_IDR:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: IDR is read-only\n", __func__);
        return;
    case GPIO_ODR:
        s->odr = val & 0xffff;
        break;
    case GPIO_BSRR:
        /* Reset bits first: when both are set, set wins */
        s->odr &= ~(val >> 16);
        s->odr |= val & 0xffff;
        break;
    case GPIO_LCKR:
        s->lckr = val & 0x1ffff;
        return;
    case GPIO_AFRL:
        s->afr[0] = val;
        return;
    case GPIO_AFRH:
        s->afr[1] = val;
        return;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: bad offset 0x%" HWADDR_PRIx "\n",
                      __func__, addr);
        return;
    }
    stm32f4_gpio_update(s);
}

static const MemoryRegionOps stm32f4_gpio_ops = {
    .read = stm32f4_gpio_read,
    .write = stm32f4_gpio_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
};

/* An external level driven onto an input pin */
static void stm32f4_gpio_set_input(void *opaque, int pin, int level)
{
    STM32F4GpioState *s = opaque;

    s->ext_driven = deposit32(s->ext_driven, pin, 1, 1);
    s->ext_level = deposit32(s->ext_level, pin, 1, !!level);
}

static void stm32f4_gpio_reset(DeviceState *dev)
{
    STM32F4GpioState *s = STM32F4_GPIO(dev);

    s->moder = s->moder_reset;
    s->otyper = 0;
    s->ospeedr = 0;
    s->pupdr = s->pupdr_reset;
    s->odr = 0;
    s->lckr = 0;
    s->afr[0] = s->afr[1] = 0;
    for (int pin = 0; pin < STM32F4_GPIO_NPINS; pin++) {
        s->reported[pin] = -1;
    }
}

static void stm32f4_gpio_init(Object *obj)
{
    STM32F4GpioState *s = STM32F4_GPIO(obj);

    memory_region_init_io(&s->mmio, obj, &stm32f4_gpio_ops, s,
                          TYPE_STM32F4_GPIO, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
    qdev_init_gpio_out(DEVICE(obj), s->out, STM32F4_GPIO_NPINS);
    qdev_init_gpio_in(DEVICE(obj), stm32f4_gpio_set_input,
                      STM32F4_GPIO_NPINS);
}

static const Property stm32f4_gpio_properties[] = {
    DEFINE_PROP_UINT32("moder-reset", STM32F4GpioState, moder_reset, 0),
    DEFINE_PROP_UINT32("pupdr-reset", STM32F4GpioState, pupdr_reset, 0),
};

static void stm32f4_gpio_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, stm32f4_gpio_reset);
    device_class_set_props(dc, stm32f4_gpio_properties);
}

static const TypeInfo stm32f4_gpio_info = {
    .name          = TYPE_STM32F4_GPIO,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32F4GpioState),
    .instance_init = stm32f4_gpio_init,
    .class_init    = stm32f4_gpio_class_init,
};

/* ------------------------------------------------------------------------ */
/* Pin monitor: GPIO events <-> JSON lines on a chardev                     */
/* ------------------------------------------------------------------------ */

#define BLACKPILL_NPORTS 6
static const char blackpill_port_names[BLACKPILL_NPORTS] = "ABCDEH";
static const hwaddr blackpill_port_addr[BLACKPILL_NPORTS] = {
    0x40020000, 0x40020400, 0x40020800, 0x40020c00, 0x40021000, 0x40021c00,
};

#define TYPE_BLACKPILL_PINMON "blackpill-pinmon"
OBJECT_DECLARE_SIMPLE_TYPE(BlackpillPinmonState, BLACKPILL_PINMON)

struct BlackpillPinmonState {
    DeviceState parent_obj;

    CharFrontend chr;
    qemu_irq drive[BLACKPILL_NPORTS * STM32F4_GPIO_NPINS];
    char line[32];
    int line_len;
};

static void blackpill_pinmon_event(void *opaque, int n, int level)
{
    BlackpillPinmonState *s = opaque;
    char buf[96];
    int len;

    len = snprintf(buf, sizeof(buf),
                   "{\"t\":%" PRId64 ",\"pin\":\"P%c%d\",\"v\":%d}\n",
                   qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL),
                   blackpill_port_names[n / STM32F4_GPIO_NPINS],
                   n % STM32F4_GPIO_NPINS, level);
    qemu_chr_fe_write_all(&s->chr, (const uint8_t *)buf, len);
}

/* Parse "PA0=1" */
static void blackpill_pinmon_command(BlackpillPinmonState *s)
{
    char port;
    int pin, level;
    const char *p;

    if (sscanf(s->line, "P%c%d=%d", &port, &pin, &level) != 3 ||
        pin < 0 || pin >= STM32F4_GPIO_NPINS) {
        return;
    }
    p = memchr(blackpill_port_names, port, BLACKPILL_NPORTS);
    if (!p) {
        return;
    }
    qemu_set_irq(s->drive[(p - blackpill_port_names) * STM32F4_GPIO_NPINS
                          + pin], level);
}

static int blackpill_pinmon_can_receive(void *opaque)
{
    return 32;
}

static void blackpill_pinmon_receive(void *opaque, const uint8_t *buf,
                                     int size)
{
    BlackpillPinmonState *s = opaque;

    for (int i = 0; i < size; i++) {
        if (buf[i] == '\n' || buf[i] == '\r') {
            s->line[s->line_len] = '\0';
            if (s->line_len) {
                blackpill_pinmon_command(s);
            }
            s->line_len = 0;
        } else if (s->line_len < (int)sizeof(s->line) - 1) {
            s->line[s->line_len++] = buf[i];
        }
    }
}

static void blackpill_pinmon_realize(DeviceState *dev, Error **errp)
{
    BlackpillPinmonState *s = BLACKPILL_PINMON(dev);

    qdev_init_gpio_in(dev, blackpill_pinmon_event,
                      BLACKPILL_NPORTS * STM32F4_GPIO_NPINS);
    qdev_init_gpio_out(dev, s->drive, BLACKPILL_NPORTS * STM32F4_GPIO_NPINS);
    qemu_chr_fe_set_handlers(&s->chr, blackpill_pinmon_can_receive,
                             blackpill_pinmon_receive, NULL, NULL,
                             s, NULL, true);
}

static const Property blackpill_pinmon_properties[] = {
    DEFINE_PROP_CHR("chardev", BlackpillPinmonState, chr),
};

static void blackpill_pinmon_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = blackpill_pinmon_realize;
    device_class_set_props(dc, blackpill_pinmon_properties);
}

static const TypeInfo blackpill_pinmon_info = {
    .name          = TYPE_BLACKPILL_PINMON,
    .parent        = TYPE_DEVICE,
    .instance_size = sizeof(BlackpillPinmonState),
    .class_init    = blackpill_pinmon_class_init,
};

/* ------------------------------------------------------------------------ */
/* Board                                                                    */
/* ------------------------------------------------------------------------ */

static void blackpill_init(MachineState *machine)
{
    DeviceState *soc, *pinmon;
    Chardev *pins_chr;
    Clock *sysclk;

    sysclk = clock_new(OBJECT(machine), "SYSCLK");
    clock_set_hz(sysclk, BLACKPILL_SYSCLK_HZ);

    soc = qdev_new(TYPE_STM32F405_SOC);
    object_property_add_child(OBJECT(machine), "soc", OBJECT(soc));
    qdev_connect_clock_in(soc, "sysclk", sysclk);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(soc), &error_fatal);

    pinmon = qdev_new(TYPE_BLACKPILL_PINMON);
    object_property_add_child(OBJECT(machine), "pinmon", OBJECT(pinmon));
    pins_chr = qemu_chr_find("pins");
    if (pins_chr) {
        qdev_prop_set_chr(pinmon, "chardev", pins_chr);
    }
    qdev_realize_and_unref(pinmon, NULL, &error_fatal);

    for (int port = 0; port < BLACKPILL_NPORTS; port++) {
        DeviceState *gpio = qdev_new(TYPE_STM32F4_GPIO);
        g_autofree char *name = g_strdup_printf("gpio%c",
                                                blackpill_port_names[port]);

        /* Ports A and B reset with the debug (SWD/JTAG) pins configured */
        if (port == 0) {
            qdev_prop_set_uint32(gpio, "moder-reset", 0xa8000000);
            qdev_prop_set_uint32(gpio, "pupdr-reset", 0x64000000);
        } else if (port == 1) {
            qdev_prop_set_uint32(gpio, "moder-reset", 0x00000280);
            qdev_prop_set_uint32(gpio, "pupdr-reset", 0x00000100);
        }
        object_property_add_child(OBJECT(machine), name, OBJECT(gpio));
        sysbus_realize_and_unref(SYS_BUS_DEVICE(gpio), &error_fatal);
        /* Overlaps the SoC's unimplemented-device placeholder */
        sysbus_mmio_map_overlap(SYS_BUS_DEVICE(gpio), 0,
                                blackpill_port_addr[port], 1);

        for (int pin = 0; pin < STM32F4_GPIO_NPINS; pin++) {
            int n = port * STM32F4_GPIO_NPINS + pin;

            qdev_connect_gpio_out(gpio, pin, qdev_get_gpio_in(pinmon, n));
            qdev_connect_gpio_out(pinmon, n, qdev_get_gpio_in(gpio, pin));
        }
    }

    armv7m_load_kernel(STM32F405_SOC(soc)->armv7m.cpu,
                       machine->kernel_filename, 0, BLACKPILL_FLASH_SIZE);
}

static void blackpill_machine_init(MachineClass *mc)
{
    static const char * const valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-m4"),
        NULL
    };

    mc->desc = "WeAct Black Pill STM32F411 (Cortex-M4, 16 MHz HSI)";
    mc->init = blackpill_init;
    mc->valid_cpu_types = valid_cpu_types;
}

DEFINE_MACHINE_ARM("blackpill-f411", blackpill_machine_init)

static void blackpill_register_types(void)
{
    type_register_static(&stm32f4_gpio_info);
    type_register_static(&blackpill_pinmon_info);
}

type_init(blackpill_register_types)
