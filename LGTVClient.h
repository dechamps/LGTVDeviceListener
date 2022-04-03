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

		void SetInput(std::string input, std::function<void()> onDone);
		void Close();

	private:
		using OnResponse = void(std::string type, nlohmann::json payload);

		void IssueRequest(nlohmann::json request, std::function<OnResponse> onResponse);
		void OnMessage(const nlohmann::json& message);

		WebSocketClient& webSocketClient;
		uint32_t lastRequestId = 0;
		std::unordered_map<uint32_t, std::function<OnResponse>> inflightRequests;
	};

}
