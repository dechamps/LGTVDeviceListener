#include "DeviceListener.h"
#include "LGTVClient.h"

#include <cxxopts.hpp>

namespace LGTVDeviceListener {
	namespace {

		struct Options final {
			std::optional<std::string> url;
			std::optional<std::string> clientKey;
			int connectTimeoutSeconds = WebSocketClient::Options().connectTimeoutSeconds;
			int handshakeTimeoutSeconds = WebSocketClient::Options().handshakeTimeoutSeconds;
		};

		std::optional<Options> ParseCommandLine(int argc, char** argv) {
			::cxxopts::Options cxxoptsOptions("LGTVDeviceListener", "LGTV Device Listener");
			Options options;
			cxxoptsOptions.add_options()
				("url", "URL of the LGTV websocket. For example `ws://192.168.1.42:3000`. If not specified, log device events only", ::cxxopts::value(options.url))
				("client-key", "LGTV client key. For example `0123456789abcdef0123456789abcdef`. If not specified, will register a new key", ::cxxopts::value(options.clientKey))
				("connect-timeout-seconds", "How long to wait for the WebSocket connection to establish, in seconds", ::cxxopts::value(options.connectTimeoutSeconds))
				("handshake-timeout-seconds", "How long to wait for the WebSocket handshake to complete, in seconds", ::cxxopts::value(options.handshakeTimeoutSeconds));
			try {
				cxxoptsOptions.parse(argc, argv);
			}
			catch (const std::exception& exception) {
				std::cerr << "USAGE ERROR: " << exception.what() << std::endl;
				std::cerr << std::endl;
				std::cerr << cxxoptsOptions.help() << std::endl;
				return std::nullopt;
			}
			return options;
		}

		void Run(const Options& options) {
			if (!options.url.has_value()) {
				ListenToDeviceEvents([&](DeviceEventType deviceEventType, std::wstring_view deviceName) {
					std::wstring_view deviceEventTypeString;
					switch (deviceEventType) {
					case DeviceEventType::ADDED:
						deviceEventTypeString = L"ADDED";
						break;
					case DeviceEventType::REMOVED:
						deviceEventTypeString = L"REMOVED";
						break;
					}
					std::wcout << deviceEventTypeString << ": " << deviceName << std::endl;
				});
				return;
			}

			LGTVClient::Run(
				*options.url,
				{
					.clientKey = options.clientKey, .webSocketClientOptions = {
						.connectTimeoutSeconds = options.connectTimeoutSeconds,
						.handshakeTimeoutSeconds = options.handshakeTimeoutSeconds }
				},
				[&](LGTVClient& lgtvClient, std::string_view clientKey) {
					std::cout << clientKey << std::endl;
					
					lgtvClient.SetInput("HDMI_2", [&] { lgtvClient.Close(); });
				});
		}

	}
}

int main(int argc, char** argv) {
	try {
		const auto options = ::LGTVDeviceListener::ParseCommandLine(argc, argv);
		if (!options.has_value()) return EXIT_FAILURE;
		::LGTVDeviceListener::Run(*options);
	}
	catch (const std::exception& exception) {
		std::cerr << "ERROR: " << exception.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
