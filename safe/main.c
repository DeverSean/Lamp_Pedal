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

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/timer.h>

#define FALLING 0
#define RISING 1

uint16_t exti_direction = FALLING;

int myTicks = 0;

/* Set STM32 to 72 MHz. */
static void clock_setup(void)
{
  rcc_clock_setup_in_hse_8mhz_out_72mhz();
}

static void gpio_setup(void)
{
  /* Enable GPIOC clock. */
  rcc_periph_clock_enable(RCC_GPIOC);

  /* Set GPIO12 (in GPIO port C) to 'output push-pull'. */
  gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ,
		GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
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
  /* Enable TIM2 clock. */
  rcc_periph_clock_enable(RCC_TIM2);

  /* Enable TIM2 interrupt. */
  nvic_enable_irq(NVIC_TIM2_IRQ);

  /* Reset TIM2 peripheral to defaults. */
  rcc_periph_reset_pulse(RST_TIM2);

  /* Timer global mode:                                                                 
   * - No divider                                                                       
   * - Alignment edge                                                                   
   * - Direction up                                                                     
   * (These are actually default values after reset above, so this call                 
   * is strictly unnecessary, but demos the api for alternative settings)               
   */
  timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT,
                 TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);

  /*                                                                                    
   * Please take note that the clock source for STM32 timers                            
   * might not be the raw APB1/APB2 clocks.  In various conditions they                 
   * are doubled.  See the Reference Manual for full details!                           
   * In our case, TIM2 on APB1 is running at double frequency, so this                  
   * sets the prescaler to have the timer run at 5kHz                                   
   */
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
   * Increment 10 times before triggering an interrupt */ 
  uint16_t fire_delay = 10;
  uint16_t new_time = compare_time + 1;
  timer_set_oc_value(TIM2, TIM_OC1, new_time);
}

void tim2_isr(void)
{
  if (timer_get_flag(TIM2, TIM_SR_CC1IF)) {

    /* Clear compare interrupt flag. */
    timer_clear_flag(TIM2, TIM_SR_CC1IF);
    /* LED should toggle 120 times per second, 1ms after the zero-crossing. */
    gpio_toggle(GPIOC, GPIO13);
    /* TODO: fire TRIAC using GPIO pulse */
  }
}

int main(void)
{
  clock_setup();
  gpio_setup();
  tim_setup();
  exti_setup();
  while (1)
    __asm("nop");

  return 0;
}
