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

static void cs_select (uint cs_pin) {
    gpio_put(cs_pin, false);
}

static void cs_deselect (uint cs_pin) {
    gpio_put(cs_pin, true);
}

static void set_speed (uint16_t speed, uint8_t direction, uint cs_pin) {
    uint8_t buf[4];
    uint8_t reg = 0xFD;  //defined speed register for slave
    buf[0] = reg & 0x7F; //sets first bit as 0 to indicate write
    buf[1] = (uint8_t) ((speed >> 8) & 0xFF);
    buf[2] = (uint8_t) (speed & 0xFF);
    buf[3] = direction;
    printf("Setting speed: %u\n", speed);
    //cs_select(cs_pin);
    spi_write_blocking(spi0, buf, 4);
    //cs_deselect(cs_pin);
    sleep_ms(10);
}

static void set_direction (uint8_t direction, uint cs_pin) {
    uint8_t buf[2];
    uint8_t reg = 0xFC;  //defined direction register for slave
    buf[0] = reg & 0x7F; //sets first bit as 0 to indicate write
    buf[1] = direction;
    //buf[2] = 0x0;
    //cs_select(cs_pin);
    spi_write_blocking(spi0, buf, 2);
    //cs_deselect(cs_pin);
    sleep_ms(10);
}

static void set_mode (uint8_t mode, uint cs_pin) {
    uint8_t buf[2];
    uint8_t reg = 0xFB;   //defined mode register for slave
    buf[0] = reg & 0x7F;  //sets first bit as 0 to indicate write
    buf[1] = mode;
    //buf[2] = 0x0;
    //cs_select(cs_pin);
    spi_write_blocking(spi0, buf, 2);
    //cs_deselect(cs_pin);
    sleep_ms(10);
}

int main () {
    //initialize for printout over serial usb
    stdio_init_all();

    const uint led[4] = {10, 11, 12, 13};
    const uint but1 = 16;
    const uint but2 = 17;
    const uint toggle = 18;
    const uint speed = 26;
    const uint ind = PICO_DEFAULT_LED_PIN;
    const uint spi_clk = 6;
    const uint spi_mosi = 7;
    const uint cs_pin = 5;
    const uint spi_miso = 4;

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

    //set function for spi pins
    gpio_set_function(spi_mosi, GPIO_FUNC_SPI);
    gpio_set_function(spi_miso, GPIO_FUNC_SPI);
    gpio_set_function(spi_clk, GPIO_FUNC_SPI);
    gpio_set_function(cs_pin, GPIO_FUNC_SPI);
    spi_init(spi0, 500000);
    
    //initialize GPIO and ADC
    gpio_init(but1);
    gpio_init(but2);
    gpio_init(ind);
    //gpio_init(cs_pin);
    adc_init();

    gpio_set_dir(but1,   GPIO_IN);
    gpio_set_dir(but2,   GPIO_IN);
    gpio_set_dir(ind,    GPIO_OUT);
    //gpio_set_dir(cs_pin, GPIO_OUT);
    adc_gpio_init(speed);

    //select input 0, GPIO pin 26
    adc_select_input(0);

    gpio_put(ind, false);
    gpio_put(cs_pin, true);
    
    int old_state = 1;
    enum mode curr_mode = brake;
    while (true) {
        
        uint16_t speed = adc_read();
        printf("Speed: %d\n", speed);
        //printf("Raw value: 0x%03x, voltage: %f V\n", set_speed, set_speed * conversion_factor);
        //uint16_t speed = MAX_16/8;
        sleep_ms(1000);
        if (gpio_get(but1)) {
            gpio_put(ind, true);
            
            pwm_set_enabled(slice_1, false);
            pwm_set_enabled(slice_2, false);

            pwm_init(slice_1, &switch_phase, false);
            pwm_set_both_levels(slice_1, speed << 4, speed << 4);

            pwm_init(slice_2, &const_phase,  false);
            pwm_set_both_levels(slice_2, 0, MAX_16);

            pwm_set_enabled(slice_2, true);
            pwm_set_enabled(slice_1, true);
            
            set_speed(speed, FORWARD, cs_pin);
            set_direction(0x01, cs_pin);
        } else if (gpio_get(but2)) {
            gpio_put(ind, true);
            
            pwm_set_enabled(slice_1, false);
            pwm_set_enabled(slice_2, false);

            pwm_init(slice_2, &switch_phase, false);
            pwm_set_both_levels(slice_2, speed << 4, speed << 4);

            pwm_init(slice_1, &const_phase,  false);
            pwm_set_both_levels(slice_1, 0, MAX_16);

            pwm_set_enabled(slice_1, true);
            pwm_set_enabled(slice_2, true);
            
            set_speed(speed, REVERSE, cs_pin);
            set_direction(0x00, cs_pin);
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
            
            set_speed(0x0, 0x02, cs_pin);
        }

        int curr_state = gpio_get(toggle);
        if(!old_state && (curr_state)) {
            gpio_put(ind, true);
            if (curr_mode == brake) {
                curr_mode = coast;
                set_mode(COAST, cs_pin);
            } else {
                curr_mode = brake;
                set_mode(BRAKE, cs_pin);
            }
        }

        old_state = curr_state;
    }
}