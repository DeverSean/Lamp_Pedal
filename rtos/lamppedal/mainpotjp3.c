/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2009 Uwe Hermann <uwe@hermann-uwe.de>,
 * Copyright (C) 2010 Piotr Esden-Tempski <piotr@esden.net>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */


/* Includes*/
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/adc.h>

/* Defines */
#define FALLING 0
#define RISING 1

/* Global Variables */
//uint16_t exti_direction = FALLING;

int myTicks = 0;

/* Function prototypes */
static uint16_t read_adc(uint8_t channel);


/* Set STM32 to 72 MHz. */
static void clock_setup(void)
{
  rcc_clock_setup_in_hse_8mhz_out_72mhz();
}


static void gpio_setup(void)
{
  /* Enable GPIOC clock. */
  rcc_periph_clock_enable(RCC_GPIOB);

  /* Set GPIO12 (in GPIO port C) to 'output push-pull'. */
  gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL, GPIO5);
}

static void exti_setup(void)
{
  /* Enable GPIOA clock. */
  rcc_periph_clock_enable(RCC_GPIOA);

  /* Enable AFIO clock. */
  rcc_periph_clock_enable(RCC_AFIO);

  /* Enable EXTI0 interrupt. */
  nvic_enable_irq(NVIC_EXTI0_IRQ);

  /* Set GPIO0 (in GPIO port A) to 'input open-drain'. */
  gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO0);

  /* Configure the EXTI subsystem. */
  exti_select_source(EXTI0, GPIOA);
  exti_direction = FALLING;
  exti_set_trigger(EXTI0, EXTI_TRIGGER_FALLING);
  exti_enable_request(EXTI0);
}

static void tim_setup(void)
{
  uint16_t exti_direction = FALLING;

  /* Enable TIM2 clock. */
  rcc_periph_clock_enable(RCC_TIM2);

  /* Enable TIM2 interrupt. */
  nvic_enable_irq(NVIC_TIM2_IRQ);

  /* Reset TIM2 peripheral to defaults. */
  rcc_periph_reset_pulse(RST_TIM2);

  /* Timer global mode:                                                              
   * - No divider                                                                    
   * - Alignment edge                                                                
   * - Direction up  */                                 
  timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT,
                 TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);

  /* Set prescaler */                       
  timer_set_prescaler(TIM2, 7200); //Ignore above comment, timer res = 100us.

  /* Disable preload. */
  timer_disable_preload(TIM2);
  timer_continuous_mode(TIM2);

  /* count full range, as we'll update compare value continuously */
  timer_set_period(TIM2, 65535);

  /* Set the initial output compare value for OC1. */
  timer_set_oc_value(TIM2, TIM_OC1, 65534);

  /* Counter enable. */
  timer_enable_counter(TIM2);

  /* Enable Channel 1 compare interrupt to recalculate compare values */
  timer_enable_irq(TIM2, TIM_DIER_CC1IE);
}



static void adc_setup(void){
  /* Set GPIOA/GPIO1 to ADC input */

  gpio_set_mode(GPIOA,
                GPIO_MODE_INPUT,
                GPIO_CNF_INPUT_ANALOG,
                GPIO1);  

  rcc_peripheral_enable_clock(&RCC_APB2ENR,RCC_APB2ENR_ADC1EN);
  adc_power_off(ADC1);
  rcc_peripheral_reset(&RCC_APB2RSTR,RCC_APB2RSTR_ADC1RST);
  rcc_peripheral_clear_reset(&RCC_APB2RSTR,RCC_APB2RSTR_ADC1RST);
  rcc_set_adcpre(RCC_CFGR_ADCPRE_PCLK2_DIV6);     // Set. 12MHz, Max. 14MHz       
  adc_set_dual_mode(ADC_CR1_DUALMOD_IND);         // Independent mode             
  adc_disable_scan_mode(ADC1);
  adc_set_right_aligned(ADC1);
  adc_set_single_conversion_mode(ADC1);
  adc_set_sample_time(ADC1,ADC_CHANNEL_TEMP,ADC_SMPR_SMP_239DOT5CYC);
  adc_set_sample_time(ADC1,ADC_CHANNEL_VREF,ADC_SMPR_SMP_239DOT5CYC);
  adc_enable_temperature_sensor();
  adc_power_on(ADC1);
  adc_reset_calibration(ADC1);
  adc_calibrate_async(ADC1);
  while ( adc_is_calibrating(ADC1) );

}



void exti0_isr(void)
{
  exti_reset_request(EXTI0);

  /*  myTicks++;
  if(myTicks>=60){
    gpio_toggle(GPIOC,GPIO13);
    myTicks = 0;
  }else{}
  */
  /* Get current timer value to calculate next
   * compare register value */
  uint16_t compare_time = timer_get_counter(TIM2);
  /* Calculate and set the next compare value. */
  /* TODO: make fire_delay dependent on magnitude of ADC input. 
   * for debugging, fire_delay is set to 1ms
   * since the timer res is 100us, the ARR value should
   * Increment "fire_delay" times before triggering an interrupt */
  
  uint16_t fire_delay = read_adc(3)/55;
  uint16_t new_time = compare_time + fire_delay + 8;
  timer_set_oc_value(TIM2, TIM_OC1, new_time);
}

void tim2_isr(void)
{
  if (timer_get_flag(TIM2, TIM_SR_CC1IF))
 {
    /* Clear compare interrupt flag. */
    timer_clear_flag(TIM2, TIM_SR_CC1IF);
    /* LED should toggle 120 times per second, 1ms after the zero-crossing. */
    //    gpio_toggle(GPIOC, GPIO13);
    /* TODO: fire TRIAC using GPIO pulse */
    gpio_clear(GPIOB,GPIO5);
    for(int x = 0;x<100;x++)
    {
      __asm("nop");
    }
    gpio_set(GPIOB,GPIO5);
  }
}

/***************************************************************                  
 * Read ADC Channel                                                           
 ***************************************************************/
static uint16_t
read_adc(uint8_t channel) {

  adc_set_sample_time(ADC1,channel,ADC_SMPR_SMP_239DOT5CYC);
  adc_set_regular_sequence(ADC1,1,&channel);
  adc_start_conversion_direct(ADC1);
  while ( !adc_eoc(ADC1) ){}
  //taskYIELD();
  return adc_read_regular(ADC1);
}

int main(void)
{
  clock_setup();
  gpio_setup();
  tim_setup();
  exti_setup();
  adc_setup();
  gpio_set(GPIOB,GPIO5);
  while (1)
    __asm("nop");

  return 0;
}
