/**************************************************************************//**
 * @main.c
 * @brief This project demonstrates the use of the TIMER module for interrupt
 * based input capture. After button 0 is pressed, the PRS routes this signal
 * to the timer to indicate that an input capture needs to occur. An interrupt
 * then occurs because the Compare/Capture channel interrupt flag is set.
 * The captured timer value is then stored in the user's buffer.
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
#include "em_prs.h"
#include "bsp.h"

// Buffer size
#define BUFFER_SIZE 8

// Buffer to hold input capture values
// Note: needs to be volatile or else the compiler will optimize it out
static volatile uint32_t buffer[BUFFER_SIZE];

/**************************************************************************//**
 * @brief
 *    GPIO initialization
 *
 * @details
 *    Since the external interrupts are asynchronous, they are sensitive to
 *    noise. As a result, the filter option is enabled for the button 0 GPIO
 *    pin.
 *
 * @note
 *    Furthermore, the button needs to be pulled up to prevent unintended
 *    triggers to the PRS (i.e. not pulling up the button can result in
 *    capture events before the button is even pressed).
 *
 * @note
 *    We need to configure an external interrupt using GPIO_ExtIntConfig()
 *    since the GPIO_EXTIPSEL[3:0] bits tell the PRS which port to listen to.
 *    PRS_SourceSignalSet() tells the PRS it needs to route an incoming GPIO
 *    signal on a specific pin, but it doesn't tell the PRS which port to listen
 *    to. Thus, we use GPIO_ExtIntConfig() to trigger an interrupt flag, but we
 *    don't actually need an interrupt handler so we won't enable GPIO
 *    interrupts and won't enable either edge.
 *****************************************************************************/
void initGpio(void)
{
  // Enable GPIO clock
  CMU_ClockEnable(cmuClock_GPIO, true);

  // Configure button 0 as input
  GPIO_PinModeSet(BSP_GPIO_PB0_PORT, BSP_GPIO_PB0_PIN, gpioModeInputPullFilter, 1);

  // Select button 0 as the interrupt source (configure as disabled since using PRS)
  GPIO_ExtIntConfig(BSP_GPIO_PB0_PORT, BSP_GPIO_PB0_PIN, BSP_GPIO_PB0_PIN, false, false, false);
}

/**************************************************************************//**
 * @brief
 *    PRS initialization
 *
 * @details
 *    prsEdgeOff is chosen because GPIO produces a level signal and the timer
 *    CC0 input can accept either a pulse or level. Thus, we do not need the PRS
 *    module to generate a pulse from the GPIO level signal (we can just leave
 *    it as it is). See the PRS chapter in the reference manual for further
 *    details on Producers and Consumers.
 *****************************************************************************/
void initPrs(void)
{
  // Enable PRS clock
  CMU_ClockEnable(cmuClock_PRS, true);

  // Select GPIO as source and button 0 GPIO pin as signal for PRS channel 0
  // Note that the PRS Channel Source Select splits LOWER (0-7) and HIGHER (8-15) GPIO pins
  if (BSP_GPIO_PB0_PIN < 8) {
    PRS_SourceSignalSet(0, PRS_CH_CTRL_SOURCESEL_GPIOL, BSP_GPIO_PB0_PIN, prsEdgeOff);
  } else {
    PRS_SourceSignalSet(0, PRS_CH_CTRL_SOURCESEL_GPIOH, (uint32_t)(BSP_GPIO_PB0_PIN - 8), prsEdgeOff);
  }
}

/**************************************************************************//**
 * @brief
 *    TIMER initialization
 *
 * @note
 *    Making the timer prescale value this large is unnecessary. The clock is
 *    prescaled by 1024 (each clock tick is slowed down by a factor of 1024) to
 *    more easily show the user that the difference between timer capture values
 *    is reasonably correct.
 *****************************************************************************/
void initTimer(void)
{
  // Enable clock for TIMER0 module
  CMU_ClockEnable(cmuClock_TIMER0, true);

  // Configure TIMER0 Compare/Capture for input capture of PRS channel 0
  TIMER_InitCC_TypeDef timerCCInit = TIMER_INITCC_DEFAULT;
  timerCCInit.prsSel = timerPRSSELCh0;
  timerCCInit.edge = timerEdgeFalling;
  timerCCInit.mode = timerCCModeCapture;
  timerCCInit.prsInput = true;
  TIMER_InitCC(TIMER0, 0, &timerCCInit);

  // Initialize timer with largest prescale setting
  TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;
  timerInit.prescale = timerPrescale1024;
  TIMER_Init(TIMER0, &timerInit);

  // Enable TIMER0 interrupts for Capture/Compare on channel 0
  TIMER_IntEnable(TIMER0, TIMER_IEN_CC0);
  NVIC_EnableIRQ(TIMER0_IRQn);
}

/**************************************************************************//**
 * @brief
 *    TIMER 0 handler
 *****************************************************************************/
void TIMER0_IRQHandler(void)
{
  static uint32_t i = 0;

  // Acknowledge the interrupt
  uint32_t flags = TIMER_IntGet(TIMER0);
  TIMER_IntClear(TIMER0, flags);

  // Check for capture event on channel 0
  if (flags & TIMER_IF_CC0) {

    // Record input capture value
    buffer[i] = TIMER_CaptureGet(TIMER0, 0);

    // Increment index and have it wrap around
    i = (i + 1) % BUFFER_SIZE;
  }
}

/**************************************************************************//**
 * @brief
 *    Main function
 *****************************************************************************/
int main(void)
{
  // Chip errata
  CHIP_Init();

#ifdef EMU_DCDCINIT_DEFAULT
  // Init DCDC regulator with kit specific parameters
  EMU_DCDCInit_TypeDef dcdcInit = EMU_DCDCINIT_DEFAULT;
  EMU_DCDCInit(&dcdcInit);
#endif

  // Initializations
  initGpio();
  initPrs();
  initTimer();

  while (1) {
    EMU_EnterEM1(); // Enter EM1 (won't exit)
  }
}

