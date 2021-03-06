/*
 * smart_dimmer.c
 *
 *  Created on: Sep 18, 2020
 *      Author: lari
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/hw_timer.h"
#include "driver/adc.h"

#include "esp_log.h"
#include "esp_system.h"

// Minimum us delay from ZCI to triac trigger, used to compensate for ZC circuit trigger point
#define MIN_TRIG_DELAY 1000

// Amount of microseconds the gate is kept HIGH after trigger activation
#define TURN_OFF_DELAY 500

// Triac conduction state, used for triac triggering sequence
volatile bool triac_triggered = false;

// Delay between ZCI and triggering the triac into conduction
volatile int trig_delay = MIN_TRIG_DELAY;

// ADC raw value
uint16_t adc_data[100];




// Triac Pin
#define GPIO_OUTPUT_IO_0    5
// LED Pin
#define GPIO_OUTPUT_IO_1    16
// Output pin mask
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1))

// Zerocross interrupt pin
#define GPIO_INPUT_IO_0     4
#define GPIO_INPUT_PIN_SEL  1ULL<<GPIO_INPUT_IO_0

/* ISR for zero cross interrupt (ZCI)
 *
 */
static void zerocross_interrupt(){

	// Set timer for gate delay
	hw_timer_alarm_us(trig_delay, false);

}

/* Initializing GPIO
 *
 */
void init_gpio(){

	gpio_config_t io_conf;
	//disable interrupt
	io_conf.intr_type = GPIO_INTR_DISABLE;
	//set as output mode
	io_conf.mode = GPIO_MODE_OUTPUT;
	//bit mask of the pins that you want to set,e.g.GPIO15/16
	io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	//disable pull-down mode
	io_conf.pull_down_en = 0;
	//disable pull-up mode
	io_conf.pull_up_en = 0;
	//configure GPIO with the given settings
	gpio_config(&io_conf);

	//interrupt of rising edge
	io_conf.intr_type = GPIO_INTR_POSEDGE;
	//bit mask of the pins, use GPIO4/5 here
	io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
	//set as input mode
	io_conf.mode = GPIO_MODE_INPUT;
	//enable pull-down mode
	io_conf.pull_down_en = 1;
	//disable pull-up mode
	io_conf.pull_up_en = 0;
	gpio_config(&io_conf);


	 //install gpio isr service
	gpio_install_isr_service(0);
	//hook isr handler for specific gpio pin
	gpio_isr_handler_add(GPIO_INPUT_IO_0, zerocross_interrupt, (void *) GPIO_INPUT_IO_0);


}

/* Timer callback function
 *
 */
void timer_callback(){

	if(triac_triggered){
		// Triac is conducting and gate is HIGH => Change gate to low & reset sequence

		// Gate to LOW
		gpio_set_level(GPIO_OUTPUT_IO_0, 0);

		// Reset sequence
		triac_triggered = false;


	}
	else{
		// Triac is not conducting and dt from ZCI is equal to trig_delay
		// => Change gate to HIGH & set timer for TURN_OFF_DELAY

		// Gate to HIGH
		gpio_set_level(GPIO_OUTPUT_IO_0, 1);

		// Set timer for TURN_OFF_DELAY
		hw_timer_alarm_us(TURN_OFF_DELAY, false);

		// Triac is now triggered
		triac_triggered = true;

	}


}

/* Initializing timer
 *
 */
void init_timer(){

	// Init timer
	hw_timer_init(timer_callback, NULL);

}



static void adc_task()
{


    while (1) {
       adc_read(&adc_data[0]);

       if(*adc_data < 255){
    	   trig_delay = MIN_TRIG_DELAY*8;
       }else if(*adc_data < 512){
    	   trig_delay = MIN_TRIG_DELAY*5;
       }
       else if(*adc_data < 767){
           trig_delay = MIN_TRIG_DELAY*3;
       }else{
    	   trig_delay = MIN_TRIG_DELAY;
       }

       vTaskDelay(200 / portTICK_RATE_MS);
    }
}

void init_adc(){
	 // 1. init adc
	adc_config_t adc_config;

	// Depend on menuconfig->Component config->PHY->vdd33_const value
	// When measuring system voltage(ADC_READ_VDD_MODE), vdd33_const must be set to 255.
	adc_config.mode = ADC_READ_TOUT_MODE;
	adc_config.clk_div = 8; // ADC sample collection clock = 80MHz/clk_div = 10MHz
	ESP_ERROR_CHECK(adc_init(&adc_config));

}



void app_main()
{
    printf("Hail Saaatan!\n");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("Archangelooo!!\n");


    init_timer();
    init_gpio();
    init_adc();

    gpio_set_level(GPIO_OUTPUT_IO_0, 0);

    int cnt = 0;

    xTaskCreate(adc_task, "adc_task", 1024, NULL, 5, NULL);

    while (1) {
    	//printf("tik\n");
		vTaskDelay(1000 / portTICK_RATE_MS);
		gpio_set_level(GPIO_OUTPUT_IO_1, cnt % 2);
		cnt++;
	}

}
