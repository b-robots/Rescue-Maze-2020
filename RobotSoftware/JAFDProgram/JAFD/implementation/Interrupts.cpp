/*
In this part of the library are all interrupt handler
*/

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

#include "Interrupts_private.h"
#include "MotorControl_private.h"

void PIOA_Handler()
{
	JAFD::MotorControl::encoderInterrupt(JAFD::Interrupts::InterruptSource::pioA, PIOA->PIO_ISR);
}

void PIOB_Handler()
{
	JAFD::MotorControl::encoderInterrupt(JAFD::Interrupts::InterruptSource::pioB, PIOB->PIO_ISR);
}

void PIOC_Handler()
{
	JAFD::MotorControl::encoderInterrupt(JAFD::Interrupts::InterruptSource::pioC, PIOC->PIO_ISR);
}

void PIOD_Handler()
{
	JAFD::MotorControl::encoderInterrupt(JAFD::Interrupts::InterruptSource::pioD, PIOD->PIO_ISR);
}

// TC0 - TC2 are reserved for Arduino Framework

// 1kHz 
void TC3_Handler()
{
	TC1->TC_CHANNEL[0].TC_SR;
}

// 100Hz / 10Hz / 1Hz
void TC4_Handler()
{
	static uint8_t i = 0;

	TC1->TC_CHANNEL[1].TC_SR;

	i++;
	// 100Hz:

	if (i % 2 == 0)
	{
		// 50Hz:
	}

	if (i % 5 == 0)
	{
		// 20Hz:

		if (i % 10 == 0)
		{
			// 10Hz:

			if (i % 20 == 0)
			{
				// 5Hz:
				JAFD::MotorControl::calcMotorSpeed(5);

				if (i % 100 == 0)
				{
					i = 0;

					// 1Hz:
				}
			}
		}
	}
}