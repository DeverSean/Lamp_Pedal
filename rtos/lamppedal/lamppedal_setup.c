/*
 * This module supports the lamppedal project. 
 *
 */

/* Includes */
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/adc.h>

#include "lamppedal_setup.h"
/* Defines */

/* Function Definitions */

/* Global Variables */
//uint16_t exti_direction = FALLING;

/* Set STM32 to 72 MHz. */
void clock_setup(void)
{
  rcc_clock_setup_in_hse_8mhz_out_72mhz();
}


/* Setup external interrupt */
void exti_setup(void)
{
  // uint16_t exti_direction = FALLING;
  /* Enable GPIOA clock. */
  rcc_periph_clock_enable(RCC_GPIOA);

  /* Enable AFIO clock. */
  rcc_periph_clock_enable(RCC_AFIO);

  /* Enable EXTI0 interrupt. */
  nvic_enable_irq(NVIC_EXTI0_IRQ);

  /* Set GPIO0 (in GPIO port A) to 'input open-drain'. */
  gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT\
		, GPIO0);

  /* Configure the EXTI subsystem. */
  exti_select_source(EXTI0, GPIOA);
  //exti_direction = FALLING;
  exti_set_trigger(EXTI0, EXTI_TRIGGER_FALLING);
  exti_enable_request(EXTI0);
}
