#include "DeviceListener.h"
#include "LGTVClient.h"
#include "StringUtil.h"
#include "Log.h"

#include <cxxopts.hpp>

namespace LGTVDeviceListener {
	namespace {

		struct Options final {
			std::optional<std::string> url;
			std::optional<std::string> clientKey;
			std::optional<std::string> deviceName;
			std::optional<std::string> addInput;
			std::optional<std::string> removeInput;
			int connectTimeoutSeconds = WebSocketClient::Options().connectTimeoutSeconds;
			int handshakeTimeoutSeconds = WebSocketClient::Options().handshakeTimeoutSeconds;
		};

		std::optional<Options> ParseCommandLine(int argc, char** argv) {
			::cxxopts::Options cxxoptsOptions("LGTVDeviceListener", "LGTV Device Listener");
			Options options;
			cxxoptsOptions.add_options()
				("url", "URL of the LGTV websocket. For example `ws://192.168.1.42:3000`. If not specified, log device events only", ::cxxopts::value(options.url))
				("client-key", "LGTV client key. For example `0123456789abcdef0123456789abcdef`. If not specified, will register a new key", ::cxxopts::value(options.clientKey))
				("device-name", R"(The name of the device to watch. Typically starts with `\\?\`. If not specified, log device events only)", ::cxxopts::value(options.deviceName))
				("add-input", "Which TV input to switch to when the device is added. For example `HDMI_1`. If not specified, does nothing on add", ::cxxopts::value(options.addInput))
				("remove-input", "Which TV input to switch to when the device is removed. For example `HDMI_2`. If not specified, does nothing on remove", ::cxxopts::value(options.removeInput))
				("connect-timeout-seconds", "How long to wait for the WebSocket connection to establish, in seconds", ::cxxopts::value(options.connectTimeoutSeconds))
				("handshake-timeout-seconds", "How long to wait for the WebSocket handshake to complete, in seconds", ::cxxopts::value(options.handshakeTimeoutSeconds));
			try {
				cxxoptsOptions.parse(argc, argv);
			}
			catch (const std::exception& exception) {
				Log() << "USAGE ERROR: " << ToWideString(exception.what(), CP_ACP) << "\r\n\r\n" << ToWideString(cxxoptsOptions.help(), CP_UTF8);
				return std::nullopt;
			}
			return options;
		}

		void Run(const Options& options) {
			const WebSocketClient::Options webSocketClientOptions = {
				.connectTimeoutSeconds = options.connectTimeoutSeconds,
				.handshakeTimeoutSeconds = options.handshakeTimeoutSeconds
			};

			std::optional<std::string> clientKey = *options.clientKey;
			if (!clientKey.has_value() && options.url.has_value()) {
				Log() << "Registering new client key with LGTV";
				LGTVClient::Run(
					*options.url, { .webSocketClientOptions = webSocketClientOptions },
					[&](LGTVClient& lgtvClient, std::string_view newClientKey) {
						Log() << "LGTV client key: " << ToWideString(newClientKey, CP_UTF8);
						clientKey = newClientKey;
						lgtvClient.Close();
					});
			}

			const auto expectedDeviceName = options.deviceName.has_value() ? std::optional<std::wstring>(ToWideString(*options.deviceName, CP_ACP)) : std::nullopt;
			ListenToDeviceEvents([&](DeviceEventType deviceEventType, std::wstring_view deviceName) {
				std::wstring_view deviceEventTypeString;
				switch (deviceEventType) {
				case DeviceEventType::ADDED:
					deviceEventTypeString = L"added";
					break;
				case DeviceEventType::REMOVED:
					deviceEventTypeString = L"removed";
					break;
				}
				Log() << "Device " << deviceEventTypeString << ": " << deviceName;

				if (!options.url.has_value() || deviceName != expectedDeviceName) return;

				const auto input = [&] {
					switch (deviceEventType) {
					case DeviceEventType::ADDED: return options.addInput;
					case DeviceEventType::REMOVED: return options.removeInput;
					}
					::abort();
				}();
				if (!input.has_value()) return;
				LGTVClient::Run(
					*options.url, { .clientKey = clientKey, .webSocketClientOptions = webSocketClientOptions },
					[&](LGTVClient& lgtvClient, std::string_view) {
						Log() << "Switching LGTV to input: " << ToWideString(*input, CP_UTF8);
						lgtvClient.SetInput(*input, [&] { lgtvClient.Close(); });
					});
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
