#pragma once

#include <optional>
#include <sstream>

namespace LGTVDeviceListener {

	class Log final {
	public:
		struct Options final {
			bool verbose = false;
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
		static std::optional<Options> options;

		std::optional<std::wstringstream> stream;
	};

}
