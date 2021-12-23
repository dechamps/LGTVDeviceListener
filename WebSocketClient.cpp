#include "WebSocketClient.h"

#include <ixwebsocket/IXNetSystem.h>

#include <iostream>

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

	}

	WebSocketClient::WebSocketClient(const std::string& url, const Options& options) :
		connectTimeoutSeconds(options.connectTimeoutSeconds) {
		webSocket.setUrl(url);
		webSocket.setHandshakeTimeout(options.handshakeTimeoutSeconds);
		webSocket.disableAutomaticReconnection();
	}

	void WebSocketClient::Run() {
		webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr& webSocketMessage) {
			std::cerr << "OnMessageCallback(" << std::to_string(static_cast<int>(webSocketMessage->type)) << ")" << std::endl;

			if (webSocketMessage->type == ix::WebSocketMessageType::Error) {
				std::cerr << "WebSocket error: " << webSocketMessage->errorInfo.reason << std::endl;
			}
		});

		IxNetSystemInitializer ixNetSystemInitializer;
		webSocket.connect(connectTimeoutSeconds);
		webSocket.run();
	}

}
