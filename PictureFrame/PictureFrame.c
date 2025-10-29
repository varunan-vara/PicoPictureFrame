#include <stdio.h>
#include "pico/stdlib.h"

void spi_screen_init () {

}

int main() {
    stdio_init_all();
    
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
