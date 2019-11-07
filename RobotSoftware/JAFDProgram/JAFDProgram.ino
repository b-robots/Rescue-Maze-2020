/*
 Name:		MainProgram.ino
 Created:	07.08.2019 14:16:07
 Author:	B.Robots
*/

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

// RobotLibrary
#include "JAFD/JAFD.h"
#include "JAFD/implementation/MotorControl_private.h"
#include "JAFD/implementation/MazeMapping_private.h"
using namespace JAFD::MazeMapping;
using namespace JAFD::MotorControl;

// The setup function runs once when you press reset or power the board
void setup() {
	// For testing
	Serial.begin(115200);

	// If robot is completely stuck, just do nothing.
	if (JAFD::robotSetup() == JAFD::ReturnCode::fatalError)
	{
		while (true);
	}
	/*
	// Maze Mapping
	// Create test maze
	resetMap();

	// First row
	setGridCell({ 0b0110, CellState::visited }, { -2, 2, 0 });
	setGridCell({ 0b1110, CellState::visited }, { -1, 2, 0 });
	setGridCell({ 0b1010, CellState::visited }, { 0, 2, 0 });
	setGridCell({ 0b1100, CellState::visited }, { 1, 2, 0 });
	setGridCell({ 0b0000, CellState::none }, { 2, 2, 0 });

	// Second row
	setGridCell({ 0b0101, CellState::visited }, { -2, 1, 0 });
	setGridCell({ 0b0101, CellState::visited }, { -1, 1, 0 });
	setGridCell({ 0b0100, CellState::visited }, { 0, 1, 0 });
	setGridCell({ 0b0011, CellState::visited }, { 1, 1, 0 });
	setGridCell({ 0b1100, CellState::visited | CellState::blackTile }, { 2, 1, 0 });

	// Third row
	setGridCell({ 0b0111, CellState::visited }, { -2, 0, 0 });
	setGridCell({ 0b1011, CellState::visited }, { -1, 0, 0 });
	setGridCell({ 0b1111, CellState::visited | CellState::checkpoint }, { 0, 0, 0 });
	setGridCell({ 0b1010, CellState::visited }, { 1, 0, 0 });
	setGridCell({ 0b1001, CellState::visited }, { 2, 0, 0 });

	// Fourth row
	setGridCell({ 0b0011, CellState::visited }, { -2, -1, 0 });
	setGridCell({ 0b1010, CellState::visited }, { -1, -1, 0 });
	setGridCell({ 0b0011, CellState::visited }, { 0, -1, 0 });
	setGridCell({ 0b1000, CellState::visited }, { 1, -1, 0 });

	uint8_t directions[64] = { 0 };

	BFAlgorithm::findShortestPath({ -1, 0, 0 }, directions, 64, [](MapCoordinate coor, GridCell cell) -> bool {return cell.cellState & CellState::checkpoint; });

	for (int i = 0; directions[i] != 0; i++)
	{
		Serial.println(directions[i], BIN);
	}
	*/

	setSpeed(Motor::left, 0.3f);

	delay(500);
}

// The loop function runs over and over again until power down or reset
void loop() {
	// If robot is completely stuck, just do nothing.
	if (JAFD::robotLoop() == JAFD::ReturnCode::fatalError)
	{
		while (true);
	}

	auto a = micros();

	for (uint8_t i = 0; i < 100; i++)
	{
		setSpeed(Motor::left, 0.3f);
		volatile auto x = getCurrent(Motor::left);
	}

	Serial.println(micros() - a);

	delay(100);
}