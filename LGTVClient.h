#pragma once

#include "WebSocketClient.h"

#include <nlohmann/json.hpp>

#include <string>

namespace LGTVDeviceListener {

	class LGTVClient final {
	private:
		struct ConstructorTag final {};

	public:
		struct Options final {
			std::optional<std::string> clientKey;
			WebSocketClient::Options webSocketClientOptions;
		};

		LGTVClient(const LGTVClient&) = delete;
		LGTVClient& operator=(const LGTVClient&) = delete;

		using OnRegistered = void(LGTVClient&, std::string_view clientKey);

		static void Run(const std::string& url, const Options& options, const std::function<OnRegistered>& onRegistered);

		LGTVClient(ConstructorTag, WebSocketClient& webSocketClient, std::optional<std::string> clientKey, const std::function<OnRegistered>& onRegistered);

		void SetInput(std::string input);
		void Close();

	private:
		void Send(const nlohmann::json& message);

		WebSocketClient& webSocketClient;
		std::function<void(std::string type, nlohmann::json payload)> onMessage;
	};

}
