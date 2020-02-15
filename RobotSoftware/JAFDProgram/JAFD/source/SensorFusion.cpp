/*
This file of the library is responsible for the sensor fusion
*/

#pragma once

#include "../header/MazeMapping.h"
#include "../header/SensorFusion.h"
#include "../header/MotorControl.h"
#include "../header/DistanceSensors.h"
#include "../../JAFDSettings.h"
#include <math.h>

namespace JAFD
{
	namespace SensorFusion
	{
		namespace
		{
			volatile FusedData _fusedData;	// Fused data
		}

		void sensorFiltering(const uint8_t freq)
		{
			_fusedData.robotState.wheelSpeeds = MotorControl::getFloatSpeeds();
			_fusedData.robotState.angularVel = Vec3f((_fusedData.robotState.wheelSpeeds.right - _fusedData.robotState.wheelSpeeds.left) / JAFDSettings::Mechanics::wheelDistance, 0.0f, 0.0f) / 1.21f;
			_fusedData.robotState.rotation = Vec3f((MotorControl::getDistance(Motor::right) - MotorControl::getDistance(Motor::left)) / JAFDSettings::Mechanics::wheelDistance, 0.0f, 0.0f) / 1.21f;
			_fusedData.robotState.forwardVel = (_fusedData.robotState.wheelSpeeds.left + _fusedData.robotState.wheelSpeeds.right) / 2.0f;
			_fusedData.robotState.position += Vec3f(cosf(_fusedData.robotState.rotation.x), sinf(_fusedData.robotState.rotation.x), 0.0f) * (_fusedData.robotState.forwardVel / freq);
			_fusedData.robotState.mapCoordinate.x = roundf(_fusedData.robotState.position.x / JAFDSettings::Field::cellWidth);
			_fusedData.robotState.mapCoordinate.y = roundf(_fusedData.robotState.position.y / JAFDSettings::Field::cellWidth);
			_fusedData.robotState.mapCoordinate.floor = 0;

			float positiveAngle = RAD_TO_DEG * _fusedData.robotState.rotation.x;

			while (positiveAngle < 0.0f) positiveAngle += 360.0f;
			while (positiveAngle > 360.0f) positiveAngle -= 360.0f;

			if (positiveAngle > 315.0f || positiveAngle < 45.0f) _fusedData.heading = AbsoluteDir::north;
			else if (positiveAngle > 45.0f && positiveAngle < 135.0f) _fusedData.heading = AbsoluteDir::west;
			else if (positiveAngle > 135.0f && positiveAngle < 225.0f) _fusedData.heading = AbsoluteDir::south;
			else _fusedData.heading = AbsoluteDir::east;
		}

		void untimedFusion()
		{
			MazeMapping::updateCurrentCell(_fusedData.gridCellCertainty, _fusedData.gridCell);
		}

		void updateDistSensor()
		{
			_fusedData.distances.backLong = DistanceSensors::backLong.getDistance();
			_fusedData.distances.frontLeft = DistanceSensors::frontLeft.getDistance();
			_fusedData.distances.frontLong = DistanceSensors::frontLong.getDistance();
			_fusedData.distances.frontRight = DistanceSensors::frontRight.getDistance();
			_fusedData.distances.leftBack = DistanceSensors::leftBack.getDistance();
			_fusedData.distances.leftFront = DistanceSensors::leftFront.getDistance();
			_fusedData.distances.rightBack = DistanceSensors::rightBack.getDistance();
			_fusedData.distances.rightFront = DistanceSensors::rightFront.getDistance();
		}

		const volatile FusedData& getFusedData()
		{
			return _fusedData;
		}
	}
}