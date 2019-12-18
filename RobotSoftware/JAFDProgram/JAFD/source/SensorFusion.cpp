/*
This file of the library is responsible for the sensor fusion
*/

#pragma once

#include "../header/AllDatatypes.h"
#include "../header/SensorFusion.h"
#include "../header/MotorControl.h"

namespace JAFD
{
	namespace SensorFusion
	{
		namespace
		{
			RobotState _robotState; // Current state of robot
		}

		void updateSensorValues(const uint8_t freq)
		{
			_robotState.wheelSpeeds = MotorControl::getFloatSpeeds();
			_robotState.position += Vec3f((_robotState.wheelSpeeds.left + _robotState.wheelSpeeds.left) / (float)freq / 2.0f, 0.0f, 0.0f);
		}

		RobotState getRobotState()
		{
			return _robotState;
		}
	}
}