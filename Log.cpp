#include "Log.h"

#include <iostream>

namespace LGTVDeviceListener {
	
	Log::~Log() {
		std::wcerr << std::move(stream).str() << std::endl;
	}

}
