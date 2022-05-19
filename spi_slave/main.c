#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"

#define FORWARD 0x01
#define REVERSE 0x00
#define BRAKE   0x00
#define COAST   0x01
#define MAX_16 65535

enum mode {brake, coast};

int main () {
    //initialize for printout over serial usb
    stdio_init_all();

    //pin number constants
    const uint led[4] = {10, 11, 12, 13};
    const uint ind = PICO_DEFAULT_LED_PIN;
    const uint spi_mosi = 16;
    const uint spi_cs   = 17;
    const uint spi_clk  = 18;
    const uint spi_miso = 19;

    //value constants
    const float conversion_factor = 3.3f / (1 << 12);

    //set function for PWM pins
    gpio_set_function(led[0], GPIO_FUNC_PWM);
    gpio_set_function(led[1], GPIO_FUNC_PWM);
    gpio_set_function(led[2], GPIO_FUNC_PWM);
    gpio_set_function(led[3], GPIO_FUNC_PWM);

    //initialized PWM output
    uint slice_1 = pwm_gpio_to_slice_num(led[0]);
    uint slice_2 = pwm_gpio_to_slice_num(led[2]);
    
    pwm_config switch_phase = pwm_get_default_config();
    pwm_config_set_output_polarity(&switch_phase, false, true);
    pwm_config_set_wrap(&switch_phase, MAX_16);

    pwm_config const_phase = pwm_get_default_config();
    pwm_config_set_wrap(&const_phase, MAX_16);

    pwm_set_both_levels(slice_1, 0, 0);
    pwm_set_both_levels(slice_2, 0, 0);
    pwm_set_enabled(slice_1, false);
    pwm_set_enabled(slice_2, false);
    

    //initialize SPI
    spi_init(spi0, 500000);
    spi_set_slave(spi0, true);
    gpio_set_function(spi_mosi, GPIO_FUNC_SPI);
    gpio_set_function(spi_miso, GPIO_FUNC_SPI);
    gpio_set_function(spi_clk,  GPIO_FUNC_SPI);
    gpio_set_function(spi_cs,   GPIO_FUNC_SPI);

    gpio_init(ind);

    gpio_set_dir(ind, GPIO_OUT);

    //select input 0, GPIO pin 26
    adc_select_input(0);

    gpio_put(ind, true);
    
    uint8_t direction = 0;
    uint16_t speed = 0;
    enum mode curr_mode = brake;
    uint8_t out_buf[3] = {0};
    while (true) {
        uint8_t reg;

        spi_read_blocking(spi0, 0x0, &reg, 1);
        //printf("reg:    0x%x\n", in_buf[0]);
        //printf("buf[1]: 0x%x\n", in_buf[1]);
        //printf("buf[2]: 0x%x\n", in_buf[2]);

        if (reg == 0x7D) {
            uint8_t buf[3];
            spi_read_blocking(spi0, 0x0, buf, 3);
            speed = (((uint16_t) buf[0]) << 8) | ((uint16_t) (buf[1] & 0xFF));
            printf("Speed: %d\n", speed);
            direction = buf[2];
        } else if (reg == 0x7C) {
            uint8_t buf[1];
            spi_read_blocking(spi0, 0x0, buf, 1);
            direction = buf[0];
        } else if (reg == 0x7B) {
            uint8_t buf[1];
            spi_read_blocking(spi0, 0x0, buf, 1);
            curr_mode = buf[0];
        }

        if (direction == 0x01) {
            gpio_put(ind, true);
            
            pwm_set_enabled(slice_1, false);
            pwm_set_enabled(slice_2, false);

            pwm_init(slice_1, &switch_phase, false);
            pwm_set_both_levels(slice_1, speed << 4, speed << 4);

            pwm_init(slice_2, &const_phase,  false);
            pwm_set_both_levels(slice_2, 0, MAX_16);

            pwm_set_enabled(slice_2, true);
            pwm_set_enabled(slice_1, true);
            
        } else if (!direction) {
            gpio_put(ind, true);
            
            pwm_set_enabled(slice_1, false);
            pwm_set_enabled(slice_2, false);

            pwm_init(slice_2, &switch_phase, false);
            pwm_set_both_levels(slice_2, speed << 4, speed << 4);

            pwm_init(slice_1, &const_phase,  false);
            pwm_set_both_levels(slice_1, 0, MAX_16);

            pwm_set_enabled(slice_1, true);
            pwm_set_enabled(slice_2, true);
            
        } else {
            gpio_put(ind, false);
            
            pwm_set_enabled(slice_1, false);
            pwm_set_enabled(slice_2, false);

            pwm_init(slice_1, &const_phase, false);
            pwm_init(slice_2, &const_phase, false);

            if (curr_mode == coast) {
                pwm_set_both_levels(slice_1, 0, 0);
                pwm_set_both_levels(slice_2, 0, 0);
            } else {
                pwm_set_both_levels(slice_1, 0, MAX_16);
                pwm_set_both_levels(slice_2, 0, MAX_16);
            }

            pwm_set_enabled(slice_1, true);
            pwm_set_enabled(slice_2, true);
            
        }

    }
}