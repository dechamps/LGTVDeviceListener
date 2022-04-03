#pragma once

#include "WebSocketClient.h"

#include <string>

namespace LGTVDeviceListener {

	class LGTVClient final {
	public:
		struct Options final {
			std::optional<std::string> clientKey;
			WebSocketClient::Options webSocketClientOptions;
		};

		LGTVClient(const LGTVClient&) = delete;
		LGTVClient& operator=(const LGTVClient&) = delete;

		static void Run(const std::string& url, const Options& options, const std::function<void(LGTVClient&, std::string_view clientKey)>& onRegistered);

		void Close();

	private:
		LGTVClient(WebSocketClient& webSocketClient) : webSocketClient(webSocketClient) {}
		~LGTVClient() = default;

		WebSocketClient& webSocketClient;
	};

}
