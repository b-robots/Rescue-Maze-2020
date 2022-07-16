#include <Arduino.h>

#include "../SIAL.h"
#include "../SIALSettings.h"
#include "../header/DuePinMapping.h"
#include "../header/AllDatatypes.h"
#include "../header/Math.h"
#include "../header/Vector.h"
#include "../header/MotorControl.h"
#include "../header/HeatSensor.h"
#include "../header/I2CMultiplexer.h"
#include "../header/SmallThings.h"
#include "../header/DistanceSensors.h"
#include "../header/Gyro.h"
#include "../header/SpiNVSRAM.h"
#include "../header/ColorSensor.h"
#include "../header/MazeMapping.h"
#include "../header/SensorFusion.h"
#include "../header/SmoothDriving.h"
#include "../header/RobotLogic.h"
#include "../header/CamRec.h"
#include "../header/Dispenser.h"

#include <SPI.h>
#include <Wire.h>	// Changed static const timeout values

namespace SIAL {
	void robotSetup() {
		delay(500);

		// Setup the SPI-Bus
		SPI.begin();
		SPI.beginTransaction(SPISettings(10e+6, MSBFIRST, SPI_MODE0));

		// Setup I2C-Bus
		Wire.begin();
		Wire.setClock(400000);
		Wire.setTimeout(1);

		Wire1.begin();
		Wire1.setClock(400000);
		Wire1.setTimeout(1);

		// Start clock for PIO (debouncing)
		PMC->PMC_PCER0 = 1 << ID_PIOA | 1 << ID_PIOB | 1 << ID_PIOC | 1 << ID_PIOD;

		// Setup PWM - CLK A for motors - CLK B unused
		PMC->PMC_PCER1 = PMC_PCER1_PID36;
		PWM->PWM_CLK = PWM_CLK_PREB(0b111) | PWM_CLK_DIVB(1) | PWM_CLK_PREA(0) | PWM_CLK_DIVA(1);

		// Enable all Timer Counter
		PMC->PMC_PCER0 = PMC_PCER0_PID27 | PMC_PCER0_PID28 | PMC_PCER0_PID29 | PMC_PCER0_PID30 | PMC_PCER0_PID31;
		PMC->PMC_PCER1 = PMC_PCER1_PID32 | PMC_PCER1_PID33 | PMC_PCER1_PID34 | PMC_PCER1_PID35;

		// Setup interrupts for all ports 
		NVIC_EnableIRQ(PIOA_IRQn);
		NVIC_EnableIRQ(PIOB_IRQn);
		NVIC_EnableIRQ(PIOC_IRQn);
		NVIC_EnableIRQ(PIOD_IRQn);

		{
			volatile auto temp = PIOA->PIO_ISR;
			temp = PIOB->PIO_ISR;
			temp = PIOC->PIO_ISR;
			temp = PIOD->PIO_ISR;
		}

		// Initialise random number generator
		randomSeed(69420);

		SmoothDriving::stop();

		bool error = false;

		Switch::setup();
		PowerLEDs::setup();

		Bumper::setup();

		// Setup of SPI NVSRAM
		if (SpiNVSRAM::setup() != ReturnCode::ok)
		{
			Serial.println("Error SPI NVSRAM");
			error = true;
		}

		// Setup of I2C Multiplexer
		if (I2CMultiplexer::setup() != ReturnCode::ok)
		{
			Serial.println("Error I2CMultiplexer");
			error = true;
		}

		// Setup of Distance Sensors
		if (DistanceSensors::setup() != ReturnCode::ok)
		{
			Serial.println("Error DistanceSensors");
			error = true;
		}

		// TESTING
		DistanceSensors::resetHardCodedCalib();

		// Setup of Gyro
		if (Gyro::setup() != ReturnCode::ok)
		{
			Serial.println("Error BNO055");
			error = true;
		}

		// Setup of MotorControl
		if (MotorControl::setup() != ReturnCode::ok)
		{
			Serial.println("Error MotorControl!");
			error = true;
		}

		// Setup of HeatSensor
		if (HeatSensor::setup() != ReturnCode::ok)
		{
			Serial.println("Error HeatSensor!");
			error = true;
		}

		// Setup of ColorSensor
		if (ColorSensor::setup() != ReturnCode::ok)
		{
			Serial.println("Error ColorSensor!");
			error = true;
		}

		// Setup of MazeMapping
		if (MazeMapping::setup() != ReturnCode::ok)
		{
			Serial.println("Error MazeMapping!");
			error = true;
		}

		// Setup of Dispenser
		if (Dispenser::setup() != ReturnCode::ok)
		{
			Serial.println("Error Dispenser!");
			error = true;
		}

		Serial.println("Wait before CamRec init...");

		Serial.println("Wait for initial BNO055 calibration...");

		uint8_t bno_sys = 0;
		do {
			Gyro::updateValues();
			bno_sys = Gyro::getOverallCalibStatus();
			delay(100);
		} while (bno_sys < 3);

		Serial.println("BNO055 ready!");

		Gyro::tare();

		delay(100);

		while (!Switch::getState()) {
			SensorFusion::updateSensors();
			SensorFusion::sensorFusion();
			Serial.println(SensorFusion::getFusedData().robotState.globalHeading);
		}

		// Setup of CamRec
		if (CamRec::setup() != ReturnCode::ok)
		{
			Serial.println("Error CamRec!");
			error = true;
		}
		else {
			PowerLEDs::setBrightness(1.0f);
			Serial.println("CamRec ok");
		}

		while (Switch::getState());

		if (!error) {
			Serial.println("Finished setup!");
		}
		else {
			Serial.println("Error during setup!");
		}

		// Wait for all distance sensors to at least measure something once -> safety precaution
		uint8_t correctDistSens = 0b0;
		uint8_t cnt = 0;
		while (cnt < 3) {
			auto t = millis();
			SensorFusion::updateSensors();
			auto states = SensorFusion::getFusedData().distSensorState;
			auto updates = SensorFusion::getFusedData().distSensUpdates;

			if (correctDistSens == 0b1111111) {
				cnt++;
				correctDistSens = 0b0;
			}

			if (states.frontLeft == DistSensorStatus::ok && updates.frontLeft) {
				correctDistSens |= 1 << 0;
			}
			if (states.frontLong == DistSensorStatus::ok && updates.frontLong) {
				correctDistSens |= 1 << 1;
			}
			if (states.frontRight == DistSensorStatus::ok && updates.frontRight) {
				correctDistSens |= 1 << 2;
			}
			if (states.leftBack == DistSensorStatus::ok && updates.leftBack) {
				correctDistSens |= 1 << 3;
			}
			if (states.leftFront == DistSensorStatus::ok && updates.leftFront) {
				correctDistSens |= 1 << 4;
			}
			if (states.rightBack == DistSensorStatus::ok && updates.rightBack) {
				correctDistSens |= 1 << 5;
			}
			if (states.rightFront == DistSensorStatus::ok && updates.rightFront) {
				correctDistSens |= 1 << 6;
			}

			Serial.print(millis() - t);
			Serial.println("ms");
		}

		Serial.println("Checked all distance sensors!");
		Serial.println("Ready for run!");

		while (!Switch::getState()) {
			SensorFusion::updateSensors();
		}

		Serial.println("Start maze!!!");

		Gyro::tare();
	}

	void robotLoop() {
		static float fps = 100.0f;
		static uint32_t t = 0;
		if (t > 0) {
			fps = 1000.0f / (millis() - t) * 1.0f + fps * 0.0f;
		}
		t = millis();

		SensorFusion::updateSensors();
		SensorFusion::distSensFusion();
		SensorFusion::sensorFusion();

		SmoothDriving::updateSpeeds();

		CamRec::loop();

		RobotLogic::loop();

		auto data = SensorFusion::getFusedData();

		// Serial.println(MemWatcher::getDynamicRam());

		//Serial.print("fps: ");
		//Serial.println(fps);
		//Serial.print("(");
		//Serial.print(data.robotState.pitch);
		//Serial.println(")");

		//Serial.println("--");
		//Serial.println(data.robotState.globalHeading);

		//Serial.print("(");
		//Serial.print(data.robotState.position.x);
		//Serial.print("; ");
		//Serial.print(data.robotState.position.y);
		//Serial.println(")");

		const char* stateLookup[] = { "ok", "over", "under", "err" };
		//Serial.print("fl: ");
		//Serial.print(stateLookup[(int)data.distSensorState.frontLeft]);
		//Serial.print("; ");
		//Serial.println(data.distances.frontLeft);
		//Serial.print("fr: ");
		//Serial.print(stateLookup[(int)data.distSensorState.frontRight]);
		//Serial.print("; ");
		//Serial.println(data.distances.frontRight);
		//Serial.print("f: ");
		//Serial.print(stateLookup[(int)data.distSensorState.frontLong]);
		//Serial.print("; ");
		//Serial.println(data.distances.frontLong);
		//Serial.print("lf: ");
		//Serial.print(stateLookup[(int)data.distSensorState.leftFront]);
		//Serial.print("; ");
		//Serial.println(data.distances.leftFront);
		//Serial.print("lb: ");
		//Serial.print(stateLookup[(int)data.distSensorState.leftBack]);
		//Serial.print("; ");
		//Serial.println(data.distances.leftBack);
		//Serial.print("rf: ");
		//Serial.print(stateLookup[(int)data.distSensorState.rightFront]);
		//Serial.print("; ");
		//Serial.println(data.distances.rightFront);
		//Serial.print("rb: ");
		//Serial.print(stateLookup[(int)data.distSensorState.rightBack]);
		//Serial.print("; ");
		//Serial.println(data.distances.rightBack);
	}
}