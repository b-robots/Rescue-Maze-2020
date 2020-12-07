#pragma once

#include "../../JAFDSettings.h"
#include "../header/AllDatatypes.h"

namespace JAFD
{
	namespace PowerLEDs
	{
		ReturnCode setup();
		void setBrightness(float perc);
	}

	namespace I2CBus
	{
		ReturnCode setup();
		ReturnCode resetBus();
	}
}