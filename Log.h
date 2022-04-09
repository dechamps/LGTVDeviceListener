#pragma once

#include <Windows.h>

#include <optional>
#include <sstream>

namespace LGTVDeviceListener {

	class Log final {
	public:
		enum class Channel { STDERR, WINDOWS_EVENT_LOG };

		struct Options final {
			bool verbose = false;
			Channel channel = Channel::STDERR;
		};
		static void Initialize(Options);

		enum class Level { NORMAL, VERBOSE };

		Log(Level);
		~Log();

		Log(const Log&) = delete;
		Log& operator=(const Log&) = delete;

		template <typename T> friend Log&& operator<<(Log&& lhs, T&& rhs) {
			auto& stream = lhs.stream;
			if (stream.has_value())
				(*stream) << std::forward<T>(rhs);
			return std::move(lhs);
		}

	private:
		struct State {
			bool verbose = false;
			HANDLE windowsEventLog = NULL;
		};
		static std::optional<State> state;

		std::optional<std::wstringstream> stream;
	};

}
