#pragma once

#include <sstream>

namespace LGTVDeviceListener {

	class Log final {
	public:
		Log() = default;
		~Log();

		Log(const Log&) = delete;
		Log& operator=(const Log&) = delete;

		template <typename T> friend Log&& operator<<(Log&& lhs, T&& rhs) {
			lhs.stream << std::forward<T>(rhs);
			return std::move(lhs);
		}

	private:
		std::wstringstream stream;
	};

}
