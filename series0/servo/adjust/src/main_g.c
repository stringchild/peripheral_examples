/**************************************************************************//**
 * @main_g.c
 * @brief This project demonstrates controlling servo motors using pulse width modulation generated by the TIMER
 * module. The GPIO pin specified in the readme.txt is configured for output and
 * outputs a 1kHz, 30% duty cycle signal. The duty cycle can be adjusted by
 * writing to the CCVB or changing the global dutyCyclePercent variable.
 * @version 0.0.2
 ******************************************************************************
 * @section License
 * <b>Copyright 2018 Silicon Labs, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#include "em_device.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_chip.h"
#include "em_gpio.h"
#include "em_timer.h"
#include "bsp.h"

// Note: change this to set the desired output frequency in Hz
#define PWM_FREQ 1000

// Note: change this to set the desired duty cycle (used to update CCVB value)
static volatile int dutyCyclePercent = 30;

/**************************************************************************//**
 * @brief
 *    Interrupt handler for TIMER0 that changes the duty cycle
 *
 * @note
 *    This handler doesn't actually dynamically change the duty cycle. Instead,
 *    it acts as a template for doing so. Simply change the dutyCyclePercent
 *    global variable here to dynamically change the duty cycle.
 *****************************************************************************/
void TIMER0_IRQHandler(void)
{
  // Acknowledge the interrupt
  uint32_t flags = TIMER_IntGet(TIMER0);
  TIMER_IntClear(TIMER0, flags);

  // Update CCVB to alter duty cycle starting next period
  TIMER_CompareBufSet(TIMER0, 0, (TIMER_TopGet(TIMER0) * dutyCyclePercent) / 100);
}

/**************************************************************************//**
 * @brief GPIO Even IRQ for pushbuttons on even-numbered pins, decreases the duty cycle
 *****************************************************************************/
void GPIO_ODD_IRQHandler(void)
{
  // Clear all even pin interrupt flags
  GPIO_IntClear(0xAAAA);

  dutyCyclePercent -= 1;
  if(dutyCyclePercent < 0) {dutyCyclePercent = 0;}
}

/**************************************************************************//**
 * @brief GPIO Odd IRQ for pushbuttons on odd-numbered pins, increases the duty cycle
 *****************************************************************************/
void GPIO_EVEN_IRQHandler(void)
{
  // Clear all odd pin interrupt flags
  GPIO_IntClear(0x5555);

  dutyCyclePercent += 1;
  if(dutyCyclePercent > 20) {dutyCyclePercent = 20;}
}

/**************************************************************************//**
 * @brief
 *    GPIO initialization
 *****************************************************************************/
void initGpio(void)
{
  // Enable GPIO and clock
  CMU_ClockEnable(cmuClock_GPIO, true);

  // Configure PD1 as output for our PWM signal
  GPIO_PinModeSet(gpioPortD, 1, gpioModePushPull, 0);

  // Configure PB0 and PB1 as input with glitch filter enabled
  GPIO_PinModeSet(BSP_GPIO_PB0_PORT, BSP_GPIO_PB0_PIN, gpioModeInputPullFilter, 1);
  GPIO_PinModeSet(BSP_GPIO_PB1_PORT, BSP_GPIO_PB1_PIN, gpioModeInputPullFilter, 1);

  // Enable IRQ for even numbered GPIO pins
  NVIC_EnableIRQ(GPIO_EVEN_IRQn);

  // Enable IRQ for odd numbered GPIO pins
  NVIC_EnableIRQ(GPIO_ODD_IRQn);

  // Enable falling-edge interrupts for PB pins
  GPIO_IntConfig(BSP_GPIO_PB0_PORT, BSP_GPIO_PB0_PIN, 0, 1, true);
  GPIO_IntConfig(BSP_GPIO_PB1_PORT, BSP_GPIO_PB1_PIN, 0, 1, true);
}

/**************************************************************************//**
 * @brief
 *    TIMER initialization
 *****************************************************************************/
void initTimer(void)
{
  // Enable clock for TIMER0 module
  CMU_ClockEnable(cmuClock_TIMER0, true);

  // Configure TIMER0 Compare/Capture for output compare
  // Use PWM mode, which sets output on overflow and clears on compare events
  TIMER_InitCC_TypeDef timerCCInit = TIMER_INITCC_DEFAULT;
  timerCCInit.mode = timerCCModePWM;
  TIMER_InitCC(TIMER0, 0, &timerCCInit);

  // Route TIMER0 CC0 to location 3 and enable CC0 route pin
  // TIM0_CC0 #3 is GPIO Pin PD1
  TIMER0->ROUTE |= (TIMER_ROUTE_CC0PEN | TIMER_ROUTE_LOCATION_LOC3);

  // Set top value to overflow at the desired PWM_FREQ frequency
  TIMER_TopSet(TIMER0, CMU_ClockFreqGet(cmuClock_TIMER0) / PWM_FREQ);

  // Set compare value for initial duty cycle
  TIMER_CompareSet(TIMER0, 0, (TIMER_TopGet(TIMER0) * dutyCyclePercent) / 100);

  // Initialize the timer
  TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;
  TIMER_Init(TIMER0, &timerInit);

  // Enable TIMER0 compare event interrupts to update the duty cycle
  TIMER_IntEnable(TIMER0, TIMER_IEN_CC0);
  NVIC_EnableIRQ(TIMER0_IRQn);
}

/**************************************************************************//**
 * @brief
 *    Main function
 *****************************************************************************/
int main(void)
{
  // Chip errata
  CHIP_Init();

  // Initializations
  initGpio();
  initTimer();

  while (1) {
    EMU_EnterEM1(); // Enter EM1 (won't exit)
  }
}

