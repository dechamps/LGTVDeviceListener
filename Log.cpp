#include "Log.h"

#include <stdexcept>
#include <iostream>

namespace LGTVDeviceListener {

	void Log::Initialize(Options newOptions) {
		if (options.has_value())
			throw std::logic_error("Logging initialized twice");
		options = newOptions;
	}

	std::optional<Log::Options> Log::options;

	Log::Log(Log::Level level) {
		if (!options.has_value())
			throw std::logic_error("Attempted to log before logging is initialized");
		if (level != Level::VERBOSE || options->verbose)
			stream.emplace();
	}
	
	Log::~Log() {
		if (!stream.has_value()) return;
		std::wcerr << std::move(stream)->str() << std::endl;
	}

}
