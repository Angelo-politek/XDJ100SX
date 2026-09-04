#include "pico/stdlib.h"

#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

// Bring-up firmware: heartbeat blink only, used to validate the toolchain
// and flashing workflow before any XDJ100SX-specific code is written.

static void led_init(void) {
#ifdef CYW43_WL_GPIO_LED_PIN
    cyw43_arch_init();
#else
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
}

static void led_set(bool on) {
#ifdef CYW43_WL_GPIO_LED_PIN
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
#else
    gpio_put(PICO_DEFAULT_LED_PIN, on);
#endif
}

int main(void) {
    stdio_init_all();
    led_init();

    while (true) {
        led_set(true);
        sleep_ms(250);
        led_set(false);
        sleep_ms(750);
    }
}
