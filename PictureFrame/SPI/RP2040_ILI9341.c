#include "RP2040_ILI9341.h"

/*
 * Basic functions to run commands and data through SPI
 * ILI9341_sendCommand -> sends `command` to `port`
 */
static void ILI9341_sendCommand (spi_inst_t *port, uint8_t command) {
    gpio_put(PIN_CSN, 0);
    gpio_put(PIN_DC, 0);
    spi_write_blocking(port, &command, 1);
    gpio_put(PIN_CSN, 1);
}

/*
 * Basic functions to run commands and data through SPI
 * ILI9341_sendCommand -> sends `data` of length `len` to `port`
 */
static void ILI9341_sendData (spi_inst_t *port, uint8_t *data, int len) {
    gpio_put(PIN_CSN, 0);
    gpio_put(PIN_DC, 1);
    spi_write_blocking(port, data, len);
    gpio_put(PIN_CSN, 1);
}

/*
 * Basic functions to run commands and data through SPI
 * ILI9341_reset -> runs reset sequence for display
 */
static void ILI9341_hwreset () {
    gpio_put(PIN_RST, 0);
    sleep_ms(50);
    gpio_put(PIN_RST, 1);
    sleep_ms(120);
}

struct ILI9341_Command bootupSequence[] = {
    {0xEF, {0x03, 0x80, 0x02}, 3},
    {0xCF, {0x00, 0xC1, 0x30}, 3},
    {0xED, {0x64, 0x03, 0x12, 0x81}, 4},
    {0xE8, {0x85, 0x00, 0x78}, 3},
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    {0xF7, {0x20}, 1},
    {0xEA, {0x00, 0x00}, 2},
    {ILI9341_PWCTR1, {0x23}, 1},
    {ILI9341_PWCTR2, {0x10}, 1},
    {ILI9341_VMCTR1, {0x3e, 0x28}, 2},
    {ILI9341_VMCTR2, {0x86}, 1},
    {ILI9341_MADCTL, {0x48}, 1},
    {ILI9341_VSCRSADD,  {0x00}, 1},
    {ILI9341_PIXFMT, {0x55}, 1},
    {ILI9341_FRMCTR1, {0x00, 0x18}, 2},
    {ILI9341_DFUNCTR, {0x08, 0x82, 0x27}, 3},
    {0xF2, {0x00}, 1},
    {ILI9341_GAMMASET, {0x01}, 1},
    {ILI9341_GMCTRP1, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 
        0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00}, 15},
    {ILI9341_GMCTRN1, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 
        0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}, 15}
};

/* 
 * Initalize the SPI Screen to pins specified in RP2040_ILI9341.h
 * Initalize `port_input` with `frequency` (MHz)
 * 
 * Sequence of operations: 
 * - get port instance ptr 
 * - init spi interface with baud rate 
 * - init gpio pins
 * - reset hardware and software and set display off
 * - Run sequence of 'begin' instructions based on Adafruit driver
 * - Turn off sleep mode
 * - Turn on display
 */
void ILI9341_init (spi_inst_t *port_input, int frequency) {
    spi_inst_t *port = port_input;

    spi_init(port, frequency * 1000000);

    gpio_init(PIN_DC);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_init(PIN_CSN);
    gpio_set_dir(PIN_CSN, GPIO_OUT);
    gpio_init(PIN_RST);
    gpio_set_dir(PIN_RST, GPIO_OUT);
    gpio_put(PIN_CSN, 1); 
    gpio_put(PIN_RST, 1);

    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_DC, GPIO_FUNC_SIO);
    gpio_set_function(PIN_CSN, GPIO_FUNC_SIO);
    gpio_set_function(PIN_RST, GPIO_FUNC_SIO);
    spi_set_format(port, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    ILI9341_hwreset();
    ILI9341_sendCommand(port, ILI9341_SWRESET);
    sleep_ms(5);
    ILI9341_sendCommand(port, ILI9341_DISPOFF);

    for (size_t i = 0; i < sizeof(bootupSequence) / sizeof(bootupSequence[0]); i++) {
        ILI9341_sendCommand(port, bootupSequence[i].cmd);
        ILI9341_sendData(port, bootupSequence[i].data, bootupSequence[i].len);
    }

    ILI9341_sendCommand(port, ILI9341_SLPOUT);
    sleep_ms(150);
    ILI9341_sendCommand(port, ILI9341_DISPON);
    sleep_ms(150);

    uint16_t x = 160;
    uint16_t y = 120;

    uint8_t x_data[] = {0x00, 0xA0, 0x00, 0xA0};
    uint8_t y_data[] = {0x00, 0x78, 0x00, 0x78};

    // --- Step 1 & 2: Set Address Window (Column and Row) ---
    // The format is Start_High, Start_Low, End_High, End_Low
    
    // Set Column Address (X-axis) - both start and end are 'x'
    ILI9341_sendCommand(port, ILI9341_CASET); 
    ILI9341_sendData(port, x_data, 4);

    // Set Page Address (Y-axis) - both start and end are 'y'
    ILI9341_sendCommand(port, ILI9341_PASET); 
    ILI9341_sendData(port, y_data, 4);

    // --- Step 3: Write Color Data (16-bit RGB565) ---
    ILI9341_sendCommand(port, ILI9341_RAMWR); 
    uint8_t red[] = {0xF8, 0x00};
    ILI9341_sendData(port, red, 2);

}