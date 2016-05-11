
#include <cmsis_os.h>																			// CMSIS RTOS header file
#include "stm32f429i_discovery.h"

static void LedTimer_Callback(void const *arg);           // prototype for timer callback function

static osTimerId idLed;                                   // timer id
static osTimerDef(LedTimer, LedTimer_Callback);                                      
 
// Periodic Timer
static void LedTimer_Callback  (void const *arg)  {
	BSP_LED_Toggle(LED3);
}	


void Init_Led (void) {

	BSP_LED_Init(LED3);
	
  // Create periodic timer
  idLed = osTimerCreate (osTimer(LedTimer), osTimerPeriodic, NULL);
  if (idLed != NULL)  {     // Periodic timer created
    // start timer with periodic 1000ms interval
    if (osTimerStart (idLed, 1000) != osOK)  {
      // Timer could not be started
    }
  }
}

