/*
This part of the Library is responsible for mapping the maze and finding the shortest paths.
*/

#pragma once

#if defined(ARDUINO) && ARDUINO >= 100
#include "arduino.h"
#else
#include "WProgram.h"
#endif

// The maximum/minimum coordinates that can fit in the RAM
#define MAX_X 31	// 0b111111 - 0b100000
#define MIN_X -32	// -(0b100000)
#define MAX_Y 31	// 0b111111 - 0b100000
#define MIN_Y -32	// -(0b100000)

#include "Helper.h"
#include "StaticQueue.h"

#include <SpiRAM.h>
#include <cstdint>

namespace JAFTD
{
	namespace internal
	{
		// Namespace for the MazeMapper
		namespace MazeMapping
		{
			// Settings
			typedef struct {
				byte ramSSPin;
			} MazeMapperSet;

			// Informations for one cell
			typedef struct
			{
				// Information about the entrances of the cell
				// From right to left:
				// 1.Bit: Entrance North
				// 2.Bit: Entrance East
				// 3.Bit: Entrance South
				// 4.Bit: Entrance West
				// 5.Bit: Is there a ramp?
				// 6. & 7.Bit: 00 = Ramp North; 01 = Ramp East; 10 = Ramp South; 11 = Ramp West
				// 8.Bit: Unused
				uint8_t cellConnections;

				// Information about Speed Bumps, Victims, Checkpoints...
				// From right to left:
				// 1.Bit: Is this Cell already visited?
				uint8_t cellState;
			} GridCell;

			// Coordinate on the map
			typedef struct
			{
				int8_t x;
				int8_t y;
				uint8_t floor;
			} MapCoordinate;

			// Namespace for the Breadth-First-Algorithm to find the shortest Path
			namespace BFAlgorithm
			{
				// Find the shortest known path from a to b
				ReturnCode findShortestPath(MapCoordinate a, MapCoordinate goal, Direction* directions, uint8_t maxPathLength);
			}

			// Setup the MazeMapper
			ReturnCode mazeMapperSetup(MazeMapperSet settings);

			// Set a grid cell in the RAM
			ReturnCode setGridCell(GridCell gridCell, MapCoordinate coor);
			ReturnCode setGridCell(uint16_t bfValue, MapCoordinate coor);
			ReturnCode setGridCell(GridCell gridCell, uint16_t bfValue, MapCoordinate coor);

			// Read a grid cell from the RAM
			ReturnCode getGridCell(GridCell* gridCell, MapCoordinate coor);
			ReturnCode getGridCell(uint16_t* bfValue, MapCoordinate coor);
			ReturnCode getGridCell(GridCell* gridCell, uint16_t* bfValue, MapCoordinate coor);
		}
	}
}