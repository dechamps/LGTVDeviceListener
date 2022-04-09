#include "Log.h"

#include <Windows.h>

#include <stdexcept>
#include <iostream>

namespace LGTVDeviceListener {

	void Log::Initialize(Options options) {
		if (state.has_value())
			throw std::logic_error("Logging initialized twice");

		state = {
			.verbose = options.verbose,
			.windowsEventLog = options.channel == Channel::WINDOWS_EVENT_LOG ? RegisterEventSourceW(NULL, L"LGTVDeviceListener") : NULL,
		};
	}

	std::optional<Log::State> Log::state;

	Log::Log(Log::Level level) {
		if (!state.has_value())
			throw std::logic_error("Attempted to log before logging is initialized");
		if (level != Level::VERBOSE || state->verbose)
			stream.emplace();
	}
	
	Log::~Log() {
		if (!stream.has_value()) return;
		auto str = std::move(stream)->str();

		const auto windowsEventLog = state->windowsEventLog;
		if (windowsEventLog != NULL) {
			auto cstr = str.c_str();
			if (ReportEventW(
				/*hEventLog=*/windowsEventLog,
				/*wType*/EVENTLOG_INFORMATION_TYPE,
				/*wCategory=*/0,
				/*dwEventID=*/0,
				/*lpUserSid=*/NULL,
				/*wNumStrings=*/1,
				/*dwDataSize=*/0,
				/*lpStrings=*/&cstr,
				/*lpRawData=*/NULL) != 0) return;
		}

		std::wcerr << std::move(str) << std::endl;
	}

}
