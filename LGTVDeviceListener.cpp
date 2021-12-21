#include <cxxopts.hpp>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

namespace LGTVDeviceListener {
	namespace {

		class IxNetSystemInitializer final {
		public:
			IxNetSystemInitializer() {
				if (!ix::initNetSystem()) throw std::runtime_error("Unable to initialize WebSocket net system");
			}
			~IxNetSystemInitializer() {
				ix::uninitNetSystem();
			}
		};

		struct Options final {
			std::string url;
			int connectTimeoutSeconds = 5;
			int handshakeTimeoutSeconds = 5;
		};

		std::optional<Options> ParseCommandLine(int argc, char** argv) {
			::cxxopts::Options cxxoptsOptions("LGTVDeviceListener", "LGTV Device Listener");
			Options options;
			cxxoptsOptions.add_options()
				("url", "URL of the LGTV websocket. For example `ws://192.168.1.42:3000`", ::cxxopts::value(options.url))
				("connect-timeout-seconds", "How long to wait for the WebSocket connection to establish, in seconds", ::cxxopts::value(options.connectTimeoutSeconds))
				("handshake-timeout-seconds", "How long to wait for the WebSocket handshake to complete, in seconds", ::cxxopts::value(options.handshakeTimeoutSeconds));
			try {
				cxxoptsOptions.parse(argc, argv);
				if (options.url.empty()) throw std::runtime_error("`url` option must be specified");
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
			IxNetSystemInitializer ixNetSystemInitializer;
			ix::WebSocket webSocket;

			webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr& webSocketMessage) {
				std::cerr << "OnMessageCallback(" << std::to_string(static_cast<int>(webSocketMessage->type)) << ")" << std::endl;

				if (webSocketMessage->type == ix::WebSocketMessageType::Error) {
					std::cerr << "WebSocket error: " << webSocketMessage->errorInfo.reason << std::endl;
				}
			});

			webSocket.setUrl(options.url);
			webSocket.setHandshakeTimeout(options.handshakeTimeoutSeconds);
			webSocket.disableAutomaticReconnection();
			webSocket.connect(options.connectTimeoutSeconds);
			webSocket.run();
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
