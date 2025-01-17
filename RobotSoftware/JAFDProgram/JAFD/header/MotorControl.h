/*
This part of the Library is responsible for driving the motors by using the "Pololu Dual VNH5019 Motor Driver"
*/

#pragma once

#include "AllDatatypes.h"
#include "Interrupts.h"

namespace JAFD
{
	namespace MotorControl
	{
		// Setup the Motor-Shield
		ReturnCode setup();

		// Get distance measured by encoder
		float getDistance(const Motor motor);

		// Get measured velocity
		WheelSpeeds getSpeeds();

		// Get more accurate measured velocity
		FloatWheelSpeeds getFloatSpeeds();

		// Interrupthandler for speed calculation
		void calcMotorSpeed(const uint8_t freq);

		// Interrupthandler for speed PID-Loop
		void speedPID(const uint8_t freq);

		// Interrupthandler for Encoder
		void encoderInterrupt(const Interrupts::InterruptSource source, const uint32_t isr);

		// Set motor speed (cm/2)
		void setSpeeds(const WheelSpeeds wheelSpeeds);

		// Get the current
		float getCurrent(const Motor motor);
	}
}