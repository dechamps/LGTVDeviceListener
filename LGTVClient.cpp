#include "LGTVClient.h"

#include <nlohmann/json.hpp>

#include <iostream>

namespace LGTVDeviceListener {

	namespace {

		nlohmann::json GetRegisterRequest(std::optional<std::string> clientKey) {
			return {
				{"type", "register"},
				{"payload", clientKey.has_value() ?
					nlohmann::json({{"client-key", *std::move(clientKey)}}) :
					// Shamelessly stolen from aiopylgtv
					nlohmann::json({{"manifest", {
						{"permissions", {"LAUNCH"}},
						{"signatures", {{
							{"signature",
								"eyJhbGdvcml0aG0iOiJSU0EtU0hBMjU2Iiwia2V5SWQiOiJ0ZXN0LXNpZ25pbm"
								"ctY2VydCIsInNpZ25hdHVyZVZlcnNpb24iOjF9.hrVRgjCwXVvE2OOSpDZ58hR"
								"+59aFNwYDyjQgKk3auukd7pcegmE2CzPCa0bJ0ZsRAcKkCTJrWo5iDzNhMBWRy"
								"aMOv5zWSrthlf7G128qvIlpMT0YNY+n/FaOHE73uLrS/g7swl3/qH/BGFG2Hu4"
								"RlL48eb3lLKqTt2xKHdCs6Cd4RMfJPYnzgvI4BNrFUKsjkcu+WD4OO2A27Pq1n"
								"50cMchmcaXadJhGrOqH5YmHdOCj5NSHzJYrsW0HPlpuAx/ECMeIZYDh6RMqaFM"
								"2DXzdKX9NmmyqzJ3o/0lkk/N97gfVRLW5hA29yeAwaCViZNCP8iC9aO0q9fQoj"
								"oa7NQnAtw=="
							},
							{"signatureVersion", 1}}}},
						{"signed", {
							{"appId", "com.lge.test"},
							{"created", "20140509"},
							{"localizedAppNames", {
								{"", "LG Remote App"},
								{"ko-KR", "리모컨 앱"},
								{"zxx-XX", "ЛГ Rэмotэ AПП"},
							}},
							{"localizedVendorNames", {{"", "LG Electronics"}}},
							{"permissions", {
								"TEST_SECURE",
								"CONTROL_INPUT_TEXT",
								"CONTROL_MOUSE_AND_KEYBOARD",
								"READ_INSTALLED_APPS",
								"READ_LGE_SDX",
								"READ_NOTIFICATIONS",
								"SEARCH",
								"WRITE_SETTINGS",
								"WRITE_NOTIFICATION_ALERT",
								"CONTROL_POWER",
								"READ_CURRENT_CHANNEL",
								"READ_RUNNING_APPS",
								"READ_UPDATE_INFO",
								"UPDATE_FROM_REMOTE_APP",
								"READ_LGE_TV_INPUT_EVENTS",
								"READ_TV_CURRENT_TIME",
							}},
							{"serial", "2f930e2d2cfe083771f68e4fe7bb07"},
							{"vendorId", "com.lge"}}}}}})}};
		}

	}

	void LGTVClient::Run(const std::string& url, const Options& options, const std::function<void(LGTVClient&, std::string_view clientKey)>& onRegistered) {
		std::optional<LGTVClient> lgtvClient;
		WebSocketClient::Run(
			url, options.webSocketClientOptions,
			[&](WebSocketClient& webSocketClient) {
				lgtvClient.emplace(ConstructorTag(), webSocketClient, std::move(options.clientKey), onRegistered);
				return [&lgtvClient = *lgtvClient](const std::string& message) {
					std::cerr << "MESSAGE: " << message << std::endl;

					auto json = nlohmann::json::parse(message);
					auto type = json.at("type");
					if (type == "error")
						throw std::runtime_error("Received error response from LGTV: " + message);
					
					auto& onMessage = lgtvClient.onMessage;
					if (onMessage) onMessage(std::move(type), std::move(json.at("payload")));
				};
			});;
	}

	LGTVClient::LGTVClient(ConstructorTag, WebSocketClient& webSocketClient, std::optional<std::string> clientKey, const std::function<OnRegistered>& onRegistered) :
		webSocketClient(webSocketClient),
		onMessage([&](std::string type, nlohmann::json payload) {
			if (type != "registered") return;
			onRegistered(*this, payload.at("client-key"));
			onMessage = nullptr;
		}) {
		Send(GetRegisterRequest(std::move(clientKey)));
	}

	void LGTVClient::Send(const nlohmann::json& message) {
		std::cout << "Sending: " << message << std::endl;
		webSocketClient.Send(message.dump());
	}

	void LGTVClient::SetInput(std::string input) {
		Send({
			{"type", "request"},
			{"uri", "ssap://tv/switchInput"},
			{"payload", {{"inputId", std::move(input)}}},
		});
	}

	void LGTVClient::Close() { webSocketClient.Close(); }

}
