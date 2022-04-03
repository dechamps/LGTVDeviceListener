#include "LGTVClient.h"

#include <nlohmann/json.hpp>

#include <iostream>

namespace LGTVDeviceListener {

	namespace {

		nlohmann::json GetRegisterRequest() {
			// Shamelessly stolen from aiopylgtv
			return {
				{"type", "register"},
				{"payload", {
					{"manifest", {
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
							{"vendorId", "com.lge"},
						}}}},
			}} };
		}

	}

	std::string RegisterWithLGTV(const std::string& url, const WebSocketClient::Options& options) {
		std::string clientKey;
		WebSocketClient::Run(
			url, options,
			[&](WebSocketClient& webSocketClient) {
				webSocketClient.Send(GetRegisterRequest().dump());

				return [&](const std::string& message) {
					std::cerr << "MESSAGE: " << message << std::endl;

					auto json = nlohmann::json::parse(message);
					const auto type = json.at("type");
					if (type == "error")
						throw std::runtime_error("Received error response from LGTV: " + message);

					if (type != "registered") return;
					clientKey = json.at("payload").at("client-key");
					webSocketClient.Close();
				};
			});;
		if (clientKey.empty())
			throw std::runtime_error("LGTV did not provide any client key");
		return clientKey;
	}

}
