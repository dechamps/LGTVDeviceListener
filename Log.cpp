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
				// We don't have a registered event source to pull messages from, so the EventId is meaningless.
				// In this situation, the Event Viewer will look up the EventID in the system message table, i.e. 0 translates to "The operation completed successfully".
				// Obviously this would be quite confusing so we use MAXDWORD to make sure that lookup will fail.
				// The result still isn't ideal as the Event Viewer complains that it can't find the message, but that's the best we can do without going through the tedious source registration mechanism.
				/*dwEventID=*/MAXDWORD,
				/*lpUserSid=*/NULL,
				/*wNumStrings=*/1,
				/*dwDataSize=*/0,
				/*lpStrings=*/&cstr,
				/*lpRawData=*/NULL) != 0) return;
		}

		std::wcerr << std::move(str) << std::endl;
	}

}
