/*
 * StepperDriver.cpp
 *
 *  Created on: 29 Sep 2017
 *      Author: Eibl-PC
 */

#include "StepperDriver.h"

xSemaphoreHandle sbRIT;
static bool RIT_running;
DigitalIoPin *xLimit1 = new DigitalIoPin(1, 9, DigitalIoPin::pullup, true);
DigitalIoPin *xLimit2 = new DigitalIoPin(1, 10, DigitalIoPin::pullup, true);
DigitalIoPin *xStep = new DigitalIoPin(0, 9, DigitalIoPin::output, false);
DigitalIoPin *xDir = new DigitalIoPin(0, 29, DigitalIoPin::output, false);

DigitalIoPin *yLimit1 = new DigitalIoPin(0, 27, DigitalIoPin::pullup, true);
DigitalIoPin *yLimit2 = new DigitalIoPin(0, 28, DigitalIoPin::pullup, true);
DigitalIoPin *yStep = new DigitalIoPin(0, 24, DigitalIoPin::output, false);
DigitalIoPin *yDir = new DigitalIoPin(1, 0, DigitalIoPin::output, false);

bool isCalibrating;
bool isRunning;
bool xStepperDir;
bool yStepperDir;
bool stepperPulse;

static int initTime;
static int xTotalSteps;
static int yTotalSteps;
int xLimitsHit;
int yLimitsHit;
int xStepCount;
int yStepCount;
int x;
int y;

/* Start timer */
void RIT_start(int count, int us)
{
	uint64_t cmp_value;
	// Determine approximate compare value based on clock rate and passed interval
	cmp_value = (uint64_t) Chip_Clock_GetSystemClockRate() * (uint64_t) us / 1000000;
	// disable timer during configuration
	Chip_RIT_Disable(LPC_RITIMER);
	RIT_running = true;
	// enable automatic clear on when compare value==timer value
	// this makes interrupts trigger periodically
	Chip_RIT_EnableCompClear(LPC_RITIMER);
	// reset the counter
	Chip_RIT_SetCounter(LPC_RITIMER, 0);
	Chip_RIT_SetCompareValue(LPC_RITIMER, cmp_value);
	// start counting
	Chip_RIT_Enable(LPC_RITIMER);
	// Enable the interrupt signal in NVIC (the interrupt controller)
	NVIC_EnableIRQ(RITIMER_IRQn);
	// wait for ISR to tell that we're done
	if(xSemaphoreTake(sbRIT, portMAX_DELAY) == pdTRUE) {
		// Disable the interrupt signal in NVIC (the interrupt controller)
		NVIC_DisableIRQ(RITIMER_IRQn);
	}
	else {
		// unexpected error
	}
}

/* Update timer */
void RIT_update(int us) {

	uint64_t cmp_value = (uint64_t) Chip_Clock_GetSystemClockRate() * (uint64_t) us / 1000000; // Calculate compare value
	Chip_RIT_Disable(LPC_RITIMER);						// Disable timer
	Chip_RIT_EnableCompClear(LPC_RITIMER);				// Set clear on comparison match
	Chip_RIT_SetCounter(LPC_RITIMER, 0);				// Reset counter
	Chip_RIT_SetCompareValue(LPC_RITIMER, cmp_value);	// Give timer a new compare value
	Chip_RIT_Enable(LPC_RITIMER);						// Enable timer
}

/* RIT IRQ Handler */
extern "C" {
void RIT_IRQHandler(void)
{
	// This used to check if a context switch is required
	portBASE_TYPE xHigherPriorityWoken = pdFALSE;
	// Tell timer that we have processed the interrupt.
	// Timer then removes the IRQ until next match occurs
	Chip_RIT_ClearIntStatus(LPC_RITIMER); // clear IRQ flag

	if(RIT_running) {

		//		/* Running */
		//		if(isRunning) {
		//
		//			// Speed up when limit is reached
		//			if((yLimit1->read() && (stepperDirection == true)) || (yLimit2->read() && (stepperDirection == false))) {
		//
		//				// Switch direction
		//				if(yLimit1->read()) stepperDirection = false;
		//				if(yLimit2->read()) stepperDirection = true;
		//
		//				// Increase speed
		//				initTime /= 1.05;
		//				time = initTime;
		//
		//				// Update timer
		//				RIT_update(initTime);
		//
		//				// Reset
		//				stepCount = 0;
		//				RIT_count = totalSteps;
		//			}
		//
		//			/* Acceleration */
		//
		//			// Accelerate until mid point is reached
		//			if(stepCount < (totalSteps * 0.5)) {
		//				if(timeCounter > 75) {
		//					time -= 1;
		//					RIT_update(time);
		//					timeCounter = 0;
		//				}
		//			}
		//
		//			// Decelerate until min speed or limit reached
		//			else if(stepCount >= (totalSteps * 0.5)){
		//
		//				// Until tolerables speed reached
		//				if(time <= initTime) {
		//
		//					if(timeCounter > 75) {
		//						time += 1;
		//						RIT_update(time);
		//						timeCounter = 0;
		//					}
		//				}
		//			}
		//		}

		/* Calibration */
		if(isCalibrating) {

			/* X-AXIS */

			if(xLimitsHit < 2) {

				// Run
				xDir->write(xStepperDir);
				xStep->write(stepperPulse);

				// Run to both ends
				if(xLimit1->read() && (xStepperDir == true)){
					xTotalSteps = xStepCount;
					xStepCount = 0;
					xLimitsHit++;
					xStepperDir = false;
				}
				else if(xLimit2->read() && (xStepperDir == false)) {
					xTotalSteps = xStepCount;
					xStepCount = 0;
					xLimitsHit++;
					xStepperDir = true;
				}

			}

			else {

			}

			/* Y-AXIS */

			if(yLimitsHit < 2) {

				// Run
				yDir->write(yStepperDir);
				yStep->write(stepperPulse);

				// Run to both ends
				if(yLimit1->read() && (yStepperDir == true)){
					yTotalSteps = yStepCount;
					yStepCount = 0;
					yLimitsHit++;
					yStepperDir = false;
				}
				else if(yLimit2->read() && (yStepperDir == false)) {
					yTotalSteps = yStepCount;
					yStepCount = 0;
					yLimitsHit++;
					yStepperDir = true;
				}
			}

			// Finish calibration
			if(xLimitsHit >= 2 && yLimitsHit >= 2) {
				RIT_running = false;
			}

		}

		// Toggle pulse
		stepperPulse = !stepperPulse;

	}
	else {
		Chip_RIT_Disable(LPC_RITIMER); // disable timer
		// Give semaphore and set context switch flag if a higher priority task was woken up
		xSemaphoreGiveFromISR(sbRIT, &xHigherPriorityWoken);
	}
	// End the ISR and (possibly) do a context switch
	portEND_SWITCHING_ISR(xHigherPriorityWoken);
}
}

/* Constructor */
StepperDriver::StepperDriver() {

	isCalibrating = true;
	isRunning = false;
	xStepperDir = true;
	yStepperDir = true;
	stepperPulse = true;

	initTime = 1000000 / (configTICK_RATE_HZ * 2);
	xTotalSteps = 0;
	yTotalSteps = 0;
	xLimitsHit = 0;
	yLimitsHit = 0;
	xStepCount = 0;
	yStepCount = 0;
	x = 0;
	y = 0;

}

/* Destructor */
StepperDriver::~StepperDriver() {
	// TODO Auto-generated destructor stub
}

/* Plot */
void StepperDriver::plot(Point point) {
	x = point.getX;
	y = point.getY;
	RIT_start(10000, initTime);
}

/* Calibrate plotter */
void StepperDriver::calibrate() {
	isCalibrating = true;
	isRunning = false;

	RIT_start(10000, initTime);

	isCalibrating = false;
	isRunning = true;
}