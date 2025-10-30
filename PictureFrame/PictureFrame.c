#include <stdio.h>
#include "pico/stdlib.h"
#include "SPI/RP2040_ILI9341.h"

void spi_screen_init () {

}

int main() {
    stdio_init_all();

    ILI9341_init(spi0, 10);
    
    gpio_init(1);
    gpio_init(2);

    gpio_set_dir(1, GPIO_OUT);
    gpio_set_dir(2, GPIO_OUT);

    while (true) {
        gpio_put(1,1);
        sleep_ms(200);
        gpio_put(1,0);
        sleep_ms(800);
    }
}
