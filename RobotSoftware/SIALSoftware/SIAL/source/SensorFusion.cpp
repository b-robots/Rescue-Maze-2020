#include "../header/MazeMapping.h"
#include "../header/Math.h"
#include "../header/SensorFusion.h"
#include "../header/SmoothDriving.h"
#include "../header/MotorControl.h"
#include "../header/DistanceSensors.h"
#include "../header/Gyro.h"
#include "../header/ColorSensor.h"
//#include "../header/RobotLogic.h"
#include "../header/SmallThings.h"
#include "../SIALSettings.h"

#include <cmath>
#include <limits>

namespace SIAL
{
	namespace SensorFusion
	{
		namespace
		{
			FusedData fusedData;				// Fused data
			float totalHeadingOff = 0.0f;		// Total heading offset

			NewForcedFusionValues correctedState;
		}

		bool forceAnglePosReset = false;

		void setCorrectedState(NewForcedFusionValues newValues) {
			correctedState = newValues;
		}

		void sensorFusion()
		{
			static bool first = true;
			static uint32_t lastTime = millis();
			if (first) {
				first = false;
				lastTime = millis();
				return;
			}

			const float dt = (millis() - lastTime) / 1000.0f;

			if (1.0f / dt > SIALSettings::SensorFusion::maxFreq) {
				return;
			}

			lastTime = millis();

			RobotState robotState = fusedData.robotState;

			float correctedDistSensAngleTrust = /* TODO SmoothDriving::isDrivingStraight()*/ true ? fusedData.fusedDistSens.distSensAngleTrust : 0.0f;

			robotState.wheelSpeeds = MotorControl::getFloatSpeeds();

			// Rotation
			// We don't handle rotation of robot on ramp (pitch != 0�) completely correct! But it shouldn't matter.
			static float lastBnoHeading = 0.0f;
			const auto lastHeading = robotState.globalHeading;
			const auto lastAngularVel = robotState.angularVel;
			static float lastBnoPitch = 0.0f;
			float currHeading = 0.0f;

			const auto bnoForwardVec = Gyro::getForwardVec();
			auto bnoHeading = getGlobalHeading(bnoForwardVec);

			auto excpectedHeading = fitAngleToInterval(lastHeading + lastAngularVel.x * dt * 0.8);

			float bnoPitch = getPitch(bnoForwardVec);
			float lastPitch = robotState.pitch;

			static bool lastBnoPitchErr = false;
			static uint8_t consecutivePitchError = 0;

			robotState.pitch = bnoPitch * SIALSettings::SensorFusion::pitchIIRFactor + robotState.pitch * (1.0f - SIALSettings::SensorFusion::pitchIIRFactor);
			lastBnoPitch = bnoPitch;
			consecutivePitchError = 0;

			robotState.angularVel.z = (robotState.pitch - lastPitch) / dt;

			float encoderYawVel = (robotState.wheelSpeeds.right - robotState.wheelSpeeds.left) / (SIALSettings::Mechanics::wheelDistToMiddle * 2.0f * SIALSettings::SensorFusion::chi);

			//if (fabsf(encoderYawVel) - fabsf(fitAngleToInterval(bnoHeading - lastBnoHeading) * freq) > 0.2f) {
			//	consecutiveRotStuck++;
			//}
			//else {
			//	consecutiveRotStuck = 0;
			//}

			//if (consecutiveRotStuck > 10) {
			//	trustYawVel = 0.3f;
			//	tempRobotState.angularVel.x = fitAngleToInterval(bnoHeading - lastBnoHeading) * freq;
			//}
			//else {
			//	tempRobotState.angularVel.x = fitAngleToInterval(bnoHeading - lastBnoHeading) * freq * SIALSettings::SensorFusion::bno055DiffPortion + encoderYawVel * (1.0f - SIALSettings::SensorFusion::bno055DiffPortion);
			//}

			currHeading = (MotorControl::getDistance(Motor::right) - MotorControl::getDistance(Motor::left)) / (SIALSettings::Mechanics::wheelDistToMiddle * 2.0f * SIALSettings::SensorFusion::chi);
			currHeading -= totalHeadingOff;

			currHeading = interpolateAngle(fitAngleToInterval(currHeading), bnoHeading, SIALSettings::SensorFusion::bno055RotPortion);

			currHeading = interpolateAngle(currHeading, fitAngleToInterval(fusedData.fusedDistSens.distSensAngle), correctedDistSensAngleTrust * SIALSettings::SensorFusion::distAngularPortion);

			// mix asolute and relative heading
			currHeading = interpolateAngle(currHeading, fitAngleToInterval(lastHeading + robotState.angularVel.x * dt), SIALSettings::SensorFusion::angleDiffPortion);

			robotState.angularVel.y = 0.0f;

			robotState.angularVel = robotState.angularVel * SIALSettings::SensorFusion::angularVelIIRFactor + lastAngularVel * (1.0f - SIALSettings::SensorFusion::angularVelIIRFactor);

			if (correctedState.newHeading) {
				currHeading = correctedState.heading;
				correctedState.newHeading = false;
			}

			if (correctedState.zeroPitch) {
				robotState.pitch = 0.0f;
				robotState.angularVel.z = 0.0f;
				correctedState.zeroPitch = false;
			}

			robotState.globalHeading = makeRotationCoherent(lastHeading, fitAngleToInterval(currHeading));

			robotState.forwardVec = toForwardVec(robotState.globalHeading, robotState.pitch);		// Calculate forward vector

			// Linear velocitys
			robotState.forwardVel = (robotState.wheelSpeeds.left + robotState.wheelSpeeds.right) / 2.0f * SIALSettings::SensorFusion::speedIIRFac + robotState.forwardVel * (1.0f - SIALSettings::SensorFusion::speedIIRFac);

			// Position
			robotState.position += robotState.forwardVec * (robotState.forwardVel * dt);

			if (correctedState.newX) {
				robotState.position.x = correctedState.x;
				correctedState.newX = false;
			}

			if (correctedState.newY) {
				robotState.position.y = correctedState.y;
				correctedState.newY = false;
			}

			// Ramp and speed bump detection
			static uint8_t consecutiveRamp = 0;
			if (fabsf(robotState.pitch) > SIALSettings::MazeMapping::minRampAngle * (consecutiveRamp > 0 ? 0.8f : 1.0f)) {
				consecutiveRamp++;
			}
			else {
				consecutiveRamp = 0;
			}

			if (consecutiveRamp >= 8) {
				fusedData.gridCell.cellState |= CellState::ramp;
			}

			// Map absolute heading & position
			currHeading = fitAngleToInterval(robotState.globalHeading);
			if (currHeading > M_PI_4 && currHeading < M_3PI_4) {
				robotState.heading = AbsoluteDir::west;
			}
			else if (currHeading < -M_PI_4 && currHeading > -M_3PI_4) {
				robotState.heading = AbsoluteDir::east;
			}
			else if (currHeading < M_PI_4 && currHeading > -M_PI_4) {
				robotState.heading = AbsoluteDir::north;
			}
			else {
				robotState.heading = AbsoluteDir::south;
			}

			auto prevCoord = robotState.mapCoordinate;

			robotState.mapCoordinate.x = (int8_t)roundf(robotState.position.x / 30.0f);
			robotState.mapCoordinate.y = (int8_t)roundf(robotState.position.y / 30.0f);

			if (correctedState.clearCell) {
				correctedState.clearCell = false;
				fusedData.gridCell = GridCell();
			}

			if (robotState.mapCoordinate != prevCoord) {
				fusedData.gridCell = GridCell();
			}

			fusedData.robotState = robotState;

			lastBnoHeading = bnoHeading;
		}

		//bool scanSurrounding(uint8_t& outCumSureWalls) {
		//	uint8_t frontWallsDetected = 0;		// How many times did a wall in front of us get detected
		//	uint8_t leftWallsDetected = 0;		// How many times did a wall left of us get detected
		//	uint8_t rightWallsDetected = 0;		// How many times did a wall right of us get detected

		//	static MapCoordinate lastCoordinate;

		//	auto tempFusedData = fusedData;

		//	// positive value -> too far in the given direction
		//	float centerOffsetToLeft = 0.0f;
		//	float centerOffsetToFront = 0.0f;

		//	switch (tempFusedData.robotState.heading)
		//	{
		//	case AbsoluteDir::north:
		//		centerOffsetToFront = tempFusedData.robotState.position.x - roundf(tempFusedData.robotState.position.x / 30.0f) * 30.0f;
		//		centerOffsetToLeft = tempFusedData.robotState.position.y - roundf(tempFusedData.robotState.position.y / 30.0f) * 30.0f;
		//		break;
		//	case AbsoluteDir::east:
		//		centerOffsetToFront = roundf(tempFusedData.robotState.position.y / 30.0f) * 30.0f - tempFusedData.robotState.position.y;
		//		centerOffsetToLeft = tempFusedData.robotState.position.x - roundf(tempFusedData.robotState.position.x / 30.0f) * 30.0f;
		//		break;
		//	case AbsoluteDir::south:
		//		centerOffsetToFront = roundf(tempFusedData.robotState.position.x / 30.0f) * 30.0f - tempFusedData.robotState.position.x;
		//		centerOffsetToLeft = roundf(tempFusedData.robotState.position.y / 30.0f) * 30.0f - tempFusedData.robotState.position.y;
		//		break;
		//	case AbsoluteDir::west:
		//		centerOffsetToFront = tempFusedData.robotState.position.y - roundf(tempFusedData.robotState.position.y / 30.0f) * 30.0f;
		//		centerOffsetToLeft = roundf(tempFusedData.robotState.position.x / 30.0f) * 30.0f - tempFusedData.robotState.position.x;
		//		break;
		//	default:
		//		break;
		//	}

		//	if (tempFusedData.distSensorState.frontLeft == DistSensorStatus::ok)
		//	{
		//		if ((tempFusedData.distances.frontLeft / 10.0f + centerOffsetToFront) < SIALSettings::MazeMapping::distLongerThanBorder + 30.0f / 2.0f - SIALSettings::Mechanics::distSensFrontBackDist / 2.0f) {
		//			frontWallsDetected++;
		//		}
		//	}
		//	else if (tempFusedData.distSensorState.frontLeft == DistSensorStatus::underflow)
		//	{
		//		frontWallsDetected++;
		//	}

		//	if (tempFusedData.distSensorState.frontRight == DistSensorStatus::ok)
		//	{
		//		if ((tempFusedData.distances.frontRight / 10.0f + centerOffsetToFront) < SIALSettings::MazeMapping::distLongerThanBorder + 30.0f / 2.0f - SIALSettings::Mechanics::distSensFrontBackDist / 2.0f) {
		//			frontWallsDetected++;
		//		}
		//	}
		//	else if (tempFusedData.distSensorState.frontRight == DistSensorStatus::underflow)
		//	{
		//		frontWallsDetected++;
		//	}

		//	if (tempFusedData.distSensorState.leftFront == DistSensorStatus::ok)
		//	{
		//		if ((tempFusedData.distances.leftFront / 10.0f + centerOffsetToLeft) < SIALSettings::MazeMapping::distLongerThanBorder + 30.0f / 2.0f - SIALSettings::Mechanics::distSensLeftRightDist / 2.0f) {
		//			leftWallsDetected++;
		//		}
		//	}
		//	else if (tempFusedData.distSensorState.leftFront == DistSensorStatus::underflow)
		//	{
		//		leftWallsDetected++;
		//	}

		//	if (tempFusedData.distSensorState.leftBack == DistSensorStatus::ok)
		//	{
		//		if ((tempFusedData.distances.leftBack / 10.0f + centerOffsetToLeft) < SIALSettings::MazeMapping::distLongerThanBorder + 30.0f / 2.0f - SIALSettings::Mechanics::distSensLeftRightDist / 2.0f) {
		//			leftWallsDetected++;
		//		}
		//	}
		//	else if (tempFusedData.distSensorState.leftBack == DistSensorStatus::underflow)
		//	{
		//		leftWallsDetected++;
		//	}

		//	if (tempFusedData.distSensorState.rightFront == DistSensorStatus::ok)
		//	{
		//		if ((tempFusedData.distances.rightFront / 10.0f - centerOffsetToLeft) < SIALSettings::MazeMapping::distLongerThanBorder + 30.0f / 2.0f - SIALSettings::Mechanics::distSensLeftRightDist / 2.0f) {
		//			rightWallsDetected++;
		//		}
		//	}
		//	else if (tempFusedData.distSensorState.rightFront == DistSensorStatus::underflow)
		//	{
		//		rightWallsDetected++;
		//	}

		//	if (tempFusedData.distSensorState.rightBack == DistSensorStatus::ok)
		//	{
		//		if ((tempFusedData.distances.rightBack / 10.0f - centerOffsetToLeft) < SIALSettings::MazeMapping::distLongerThanBorder + 30.0f / 2.0f - SIALSettings::Mechanics::distSensLeftRightDist / 2.0f) {
		//			rightWallsDetected++;
		//		}
		//	}
		//	else if (tempFusedData.distSensorState.rightBack == DistSensorStatus::underflow)
		//	{
		//		rightWallsDetected++;
		//	}

		//	GridCell newCell;
		//	uint8_t sureWalls;
		//	static int consecutiveOk = 0;

		//	bool isOk = MazeMapping::manageDetectedWalls(frontWallsDetected, leftWallsDetected, rightWallsDetected, tempFusedData, newCell, sureWalls);
		//	tempFusedData.gridCell.cellConnections = newCell.cellConnections;

		//	if (isOk) {
		//		consecutiveOk++;
		//	}
		//	else {
		//		consecutiveOk = 0;
		//	}

		//	if (consecutiveOk >= 3) {
		//		Serial.print("successful scan: ");
		//		Serial.print(sureWalls, BIN);
		//		Serial.print(", at: ");
		//		Serial.print(tempFusedData.robotState.mapCoordinate.x);
		//		Serial.print(", ");
		//		Serial.println(tempFusedData.robotState.mapCoordinate.y);

		//		outCumSureWalls |= sureWalls;
		//		tempFusedData.gridCell.cellConnections = ~((~tempFusedData.gridCell.cellConnections) | outCumSureWalls);
		//	}

		//	fusedData = tempFusedData;

		//	lastCoordinate = tempFusedData.robotState.mapCoordinate;

		//	return consecutiveOk >= 3;
		//}

		void calcOffsetAngleFromDistSens() {
			struct {
				bool lf = false;
				bool lb = false;
				bool rf = false;
				bool rb = false;
				bool fl = false;
				bool fr = false;
			} usableData;

			struct {
				int lf = 0;
				int lb = 0;
				int rf = 0;
				int rb = 0;
				int fl = 0;
				int fr = 0;
			} distances;

			if ((fusedData.distSensorState.leftFront == DistSensorStatus::ok &&
				fusedData.distances.leftFront < 300 - SIALSettings::Mechanics::distSensLeftRightDist * 10 * 0.9) ||
				fusedData.distSensorState.leftFront == DistSensorStatus::underflow) {
				usableData.lf = true;
				distances.lf = fusedData.distSensorState.leftFront == DistSensorStatus::underflow ? DistanceSensors::VL6180::minDist : fusedData.distances.leftFront;
			}

			if ((fusedData.distSensorState.leftBack == DistSensorStatus::ok &&
				fusedData.distances.leftBack < 300 - SIALSettings::Mechanics::distSensLeftRightDist * 10 * 0.9) ||
				fusedData.distSensorState.leftBack == DistSensorStatus::underflow) {
				usableData.lb = true;
				distances.lb = fusedData.distSensorState.leftBack == DistSensorStatus::underflow ? DistanceSensors::VL6180::minDist : fusedData.distances.leftBack;
			}

			if ((fusedData.distSensorState.rightFront == DistSensorStatus::ok &&
				fusedData.distances.rightFront < 300 - SIALSettings::Mechanics::distSensLeftRightDist * 10 * 0.9) ||
				fusedData.distSensorState.rightFront == DistSensorStatus::underflow) {
				usableData.rf = true;
				distances.rf = fusedData.distSensorState.rightFront == DistSensorStatus::underflow ? DistanceSensors::VL6180::minDist : fusedData.distances.rightFront;
			}

			if ((fusedData.distSensorState.rightBack == DistSensorStatus::ok &&
				fusedData.distances.rightBack < 300 - SIALSettings::Mechanics::distSensLeftRightDist * 10 * 0.9) ||
				fusedData.distSensorState.rightBack == DistSensorStatus::underflow) {
				usableData.rb = true;
				distances.rb = fusedData.distSensorState.rightBack == DistSensorStatus::underflow ? DistanceSensors::VL6180::minDist : fusedData.distances.rightBack;
			}

			if ((fusedData.distSensorState.frontLeft == DistSensorStatus::ok &&
				fusedData.distances.frontLeft < 300 - SIALSettings::Mechanics::distSensFrontBackDist * 10 * 0.9) ||
				fusedData.distSensorState.frontLeft == DistSensorStatus::underflow) {
				usableData.fl = true;
				distances.fl = fusedData.distSensorState.frontLeft == DistSensorStatus::underflow ? DistanceSensors::VL53L0::minDist : fusedData.distances.frontLeft;
			}

			if ((fusedData.distSensorState.frontRight == DistSensorStatus::ok &&
				fusedData.distances.frontRight < 300 - SIALSettings::Mechanics::distSensFrontBackDist * 10 * 0.9) ||
				fusedData.distSensorState.frontRight == DistSensorStatus::underflow) {
				usableData.fr = true;
				distances.fr = fusedData.distSensorState.frontRight == DistSensorStatus::underflow ? DistanceSensors::VL53L0::minDist : fusedData.distances.frontRight;
			}

			float angleL = 0.0f;
			float angleR = 0.0f;
			float angleF = 0.0f;

			float distToWallL = -1.0f;
			float distToWallR = -1.0f;
			float distToWallF = -1.0f;

			int numData = 0;

			if (usableData.lf && usableData.lb) {
				calcAngleWallOffsetFromTwoDistances(&angleL, &distToWallL, distances.lf, distances.lb, SIALSettings::Mechanics::distSensLRSpacing, SIALSettings::Mechanics::distSensLeftRightDist);
				angleL *= -1.0f;

				numData++;
			}

			if (usableData.rf && usableData.rb) {
				calcAngleWallOffsetFromTwoDistances(&angleR, &distToWallR, distances.rf, distances.rb, SIALSettings::Mechanics::distSensLRSpacing, SIALSettings::Mechanics::distSensLeftRightDist);

				numData++;
			}

			if (usableData.fl && usableData.fr) {
				calcAngleWallOffsetFromTwoDistances(&angleF, &distToWallF, distances.fl, distances.fr, SIALSettings::Mechanics::distSensFrontSpacing, SIALSettings::Mechanics::distSensFrontBackDist);
				numData++;
			}

			fusedData.fusedDistSens.distToWalls.l = distToWallL;
			fusedData.fusedDistSens.distToWalls.r = distToWallR;
			fusedData.fusedDistSens.distToWalls.f = distToWallF;

			if (numData > 0) {
				float angle = (angleL + angleR + angleF) / numData;

				float wallAngle = 0.0f;
				switch (fusedData.robotState.heading)
				{
				case AbsoluteDir::north:
					wallAngle = 0.0f;
					break;
				case AbsoluteDir::east:
					wallAngle = -M_PI_2;
					break;
				case AbsoluteDir::south:
					wallAngle = M_PI;
					break;
				case AbsoluteDir::west:
					wallAngle = M_PI_2;
					break;
				default:
					break;
				}
				fusedData.fusedDistSens.distSensAngle = fitAngleToInterval(angle + wallAngle);
				fusedData.fusedDistSens.distSensAngleTrust = (numData + 2.0f) / 5.0f;
			}
			else {
				fusedData.fusedDistSens.distSensAngleTrust = 0.0f;
			}
		}

		void distSensFusion()
		{
			/*
			* TODO Edge-Detection
			* nur dort wo sicher ist, dass es eine Wand ist
			* �bergang overflow -> underflow/ok
			* 
			static bool firstEdgeDetection = true;

			if (!( TODO SmoothDriving::isDrivingStraight() &&
				MotorControl::getSpeeds().left > 1 &&
				MotorControl::getSpeeds().right > 1 &&
				fusedData.robotState.pitch < SIALSettings::SensorFusion::maxPitchForDistSensor) ||
				fusedData.gridCell.cellState && CellState::ramp) {
				firstEdgeDetection = true;
			}
			else {
				if (firstEdgeDetection) {
					firstEdgeDetection = false;
				}
				else {
					float x = NAN;
					float y = NAN;

					if ((fabsf(lastDistances.lf - distances.lf) > 50 && lastDistances.lf * distances.lf > 0.1f) || (fabsf(lastDistances.rf - distances.rf) > 50 && lastDistances.rf * distances.rf > 0.1f)) {
						switch (fusedData.robotState.heading)
						{
						case AbsoluteDir::north:
						{
							float edgeX = roundf((fusedData.robotState.position.x + SIALSettings::Mechanics::distSensLRSpacing / 2.0f - 15.0f) / 30.0f) * 30.0f + 15.0f;
							x = edgeX - SIALSettings::Mechanics::distSensLRSpacing / 2.0f;
							break;
						}
						case AbsoluteDir::east:
						{
							float edgeY = roundf((fusedData.robotState.position.y - SIALSettings::Mechanics::distSensLRSpacing / 2.0f - 15.0f) / 30.0f) * 30.0f + 15.0f;
							y = edgeY + SIALSettings::Mechanics::distSensLRSpacing / 2.0f;
							break;
						}
						case AbsoluteDir::south:
						{
							float edgeX = roundf((fusedData.robotState.position.x - SIALSettings::Mechanics::distSensLRSpacing / 2.0f - 15.0f) / 30.0f) * 30.0f + 15.0f;
							x = edgeX + SIALSettings::Mechanics::distSensLRSpacing / 2.0f;
							break;
						}
						case AbsoluteDir::west:
						{
							float edgeY = roundf((fusedData.robotState.position.y + SIALSettings::Mechanics::distSensLRSpacing / 2.0f - 15.0f) / 30.0f) * 30.0f + 15.0f;
							y = edgeY - SIALSettings::Mechanics::distSensLRSpacing / 2.0f;
							break;
						}
						default:
							break;
						}
					}

					if ((fabsf(lastDistances.lb - distances.lb) > 50 && lastDistances.lb * distances.lb > 0.1f) || (fabsf(lastDistances.rb - distances.rb) > 50 && lastDistances.rb * distances.rb > 0.1f)) {
						switch (fusedData.robotState.heading)
						{
						case AbsoluteDir::north:
						{
							float edgeX = roundf((fusedData.robotState.position.x - SIALSettings::Mechanics::distSensLRSpacing / 2.0f - 15.0f) / 30.0f) * 30.0f + 15.0f;
							x = edgeX + SIALSettings::Mechanics::distSensLRSpacing / 2.0f;
							break;
						}
						case AbsoluteDir::east:
						{
							float edgeY = roundf((fusedData.robotState.position.y + SIALSettings::Mechanics::distSensLRSpacing / 2.0f - 15.0f) / 30.0f) * 30.0f + 15.0f;
							y = edgeY - SIALSettings::Mechanics::distSensLRSpacing / 2.0f;
							break;
						}
						case AbsoluteDir::south:
						{
							float edgeX = roundf((fusedData.robotState.position.x + SIALSettings::Mechanics::distSensLRSpacing / 2.0f - 15.0f) / 30.0f) * 30.0f + 15.0f;
							x = edgeX - SIALSettings::Mechanics::distSensLRSpacing / 2.0f;
							break;
						}
						case AbsoluteDir::west:
						{
							float edgeY = roundf((fusedData.robotState.position.y - SIALSettings::Mechanics::distSensLRSpacing / 2.0f - 15.0f) / 30.0f) * 30.0f + 15.0f;
							y = edgeY + SIALSettings::Mechanics::distSensLRSpacing / 2.0f;
							break;
						}
						default:
							break;
						}
					}

					if (!std::isnan(x)) {
						Serial.print("new edge x: ");
						Serial.print(x);
						Serial.print(", pos.x: ");
						Serial.println(fusedData.robotState.position.x);
						correctedState.x = x * 0.8f + fusedData.robotState.position.x * 0.2f;
						correctedState.newX = true;
					}
					else if (!std::isnan(y)) {
						Serial.print("new edge y: ");
						Serial.print(y);
						Serial.print(", pos.y: ");
						Serial.println(fusedData.robotState.position.y);
						correctedState.y = y * 0.8f + fusedData.robotState.position.y * 0.2f;
						correctedState.newY = true;
					}
				}
			}
			*/

			calcOffsetAngleFromDistSens();

			if (forceAnglePosReset) {
				forceAnglePosReset = false;
				updatePosAndRotFromDist();
			}
		}

		void updatePosAndRotFromDist() {
			float totAngleWeight = 0.0f;
			float avgCos = 0.0f;
			float avgSin = 0.0f;
			float avgX = 0.0f;
			int totXNum = 0.0f;
			float avgY = 0.0f;
			int totYNum = 0.0f;

			const RobotState robotState = fusedData.robotState;

			for (int i = 0; i < 10; i++) {
				DistanceSensors::updateDistSensors();
				calcOffsetAngleFromDistSens();

				if (fusedData.fusedDistSens.distSensAngleTrust > 0.01f) {
					totAngleWeight += fusedData.fusedDistSens.distSensAngleTrust;
					avgCos += cosf(fusedData.fusedDistSens.distSensAngle) * fusedData.fusedDistSens.distSensAngleTrust;
					avgSin += sinf(fusedData.fusedDistSens.distSensAngle) * fusedData.fusedDistSens.distSensAngleTrust;
				}

				switch (robotState.heading)
				{
				case AbsoluteDir::north:
					if (fusedData.fusedDistSens.distToWalls.l > 0.0f) {
						avgY += (robotState.mapCoordinate.y + 0.5f) * 30.0f - fusedData.fusedDistSens.distToWalls.l;
						totYNum++;
					}

					if (fusedData.fusedDistSens.distToWalls.r > 0.0f) {
						avgY += (robotState.mapCoordinate.y - 0.5f) * 30.0f + fusedData.fusedDistSens.distToWalls.r;
						totYNum++;
					}

					if (fusedData.fusedDistSens.distToWalls.f > 0.0f) {
						avgX += (robotState.mapCoordinate.x + 0.5f) * 30.0f - fusedData.fusedDistSens.distToWalls.f;
						totXNum++;
					}
					break;
				case AbsoluteDir::east:
					if (fusedData.fusedDistSens.distToWalls.l > 0.0f) {
						avgX += (robotState.mapCoordinate.x + 0.5f) * 30.0f - fusedData.fusedDistSens.distToWalls.l;
						totXNum++;
					}

					if (fusedData.fusedDistSens.distToWalls.r > 0.0f) {
						avgX += (robotState.mapCoordinate.x - 0.5f) * 30.0f + fusedData.fusedDistSens.distToWalls.r;
						totXNum++;
					}

					if (fusedData.fusedDistSens.distToWalls.f > 0.0f) {
						avgY += (robotState.mapCoordinate.y - 0.5f) * 30.0f + fusedData.fusedDistSens.distToWalls.f;
						totYNum++;
					}
					break;
				case AbsoluteDir::south:
					if (fusedData.fusedDistSens.distToWalls.l > 0.0f) {
						avgY += (robotState.mapCoordinate.y - 0.5f) * 30.0f + fusedData.fusedDistSens.distToWalls.l;
						totYNum++;
					}

					if (fusedData.fusedDistSens.distToWalls.r > 0.0f) {
						avgY += (robotState.mapCoordinate.y + 0.5f) * 30.0f - fusedData.fusedDistSens.distToWalls.r;
						totYNum++;
					}

					if (fusedData.fusedDistSens.distToWalls.f > 0.0f) {
						avgX += (robotState.mapCoordinate.x - 0.5f) * 30.0f + fusedData.fusedDistSens.distToWalls.f;
						totXNum++;
					}
					break;
				case AbsoluteDir::west:
					if (fusedData.fusedDistSens.distToWalls.l > 0.0f) {
						avgX += (robotState.mapCoordinate.x - 0.5f) * 30.0f + fusedData.fusedDistSens.distToWalls.l;
						totXNum++;
					}

					if (fusedData.fusedDistSens.distToWalls.r > 0.0f) {
						avgX += (robotState.mapCoordinate.x + 0.5f) * 30.0f - fusedData.fusedDistSens.distToWalls.r;
						totXNum++;
					}

					if (fusedData.fusedDistSens.distToWalls.f > 0.0f) {
						avgY += (robotState.mapCoordinate.y + 0.5f) * 30.0f - fusedData.fusedDistSens.distToWalls.f;
						totYNum++;
					}
					break;
				default:
					break;
				}
			}

			correctedState.zeroPitch = true;

			if (totXNum > 0) {
				avgX /= totXNum;
				correctedState.x = avgX;
				correctedState.newX = true;
			}

			if (totYNum > 0) {
				avgY /= totYNum;
				correctedState.y = avgY;
				correctedState.newY = true;
			}

			float avgAngle = 0.0f;
			if (totAngleWeight > 0.01f) {
				avgCos /= totAngleWeight;
				avgSin /= totAngleWeight;
				avgAngle = atan2f(avgSin, avgCos);

				correctedState.heading = makeRotationCoherent(robotState.globalHeading, avgAngle);
				correctedState.newHeading = true;

				float currentRotEncAngle = (MotorControl::getDistance(Motor::right) - MotorControl::getDistance(Motor::left)) / (SIALSettings::Mechanics::wheelDistToMiddle * 2.0f * SIALSettings::SensorFusion::chi);
				totalHeadingOff = fitAngleToInterval(currentRotEncAngle - avgAngle);
			}

			if (totAngleWeight > 0.01f) {
				Gyro::tare(avgAngle);
			}
		}

		FusedData getFusedData()
		{
			return fusedData;
		}

		void updateSensors()
		{
			if (ColorSensor::dataIsReady())
			{
				uint16_t colorTemp = 0;
				uint16_t lux = 0;
				ColorSensor::getData(&colorTemp, &lux);

				fusedData.colorSensData.colorTemp = colorTemp;
				fusedData.colorSensData.lux = lux;
			}
			DistanceSensors::updateDistSensors();
			Gyro::updateValues();
		}

		void setDistances(Distances distances)
		{
			fusedData.distances = distances;
		}

		void setDistSensStates(DistSensorStates distSensorStates)
		{
			fusedData.distSensorState = distSensorStates;
		}
	}
}