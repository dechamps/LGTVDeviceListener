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

	void WebSocketClient::Run(const std::function<void()>& onOpen, const std::function<void(const std::string&)>& onMessage) {
		webSocket.setOnMessageCallback([&](const ix::WebSocketMessagePtr& webSocketMessage) {
			std::cerr << "OnMessageCallback(" << std::to_string(static_cast<int>(webSocketMessage->type)) << ")" << std::endl;

			using Type = ix::WebSocketMessageType;
			switch (webSocketMessage->type) {
			case Type::Message: {
				onMessage(webSocketMessage->str);
			} break;
			case Type::Open: {
				onOpen();
			} break;
			case Type::Error: {
				const auto& errorInfo = webSocketMessage->errorInfo;
				throw std::runtime_error(std::string("WebSocket client error: ") + errorInfo.reason +
					" (retries: " + std::to_string(errorInfo.retries) +
					", wait time: " + std::to_string(errorInfo.wait_time) +
					" ms, HTTP status: " + std::to_string(errorInfo.http_status) +
					(errorInfo.decompressionError ? ", decompression error" : "") +
					")");
			} break;
			}
		});

		IxNetSystemInitializer ixNetSystemInitializer;
		webSocket.connect(connectTimeoutSeconds);
		webSocket.run();
	}

	void WebSocketClient::Send(const std::string& data) {
		webSocket.send(data);
	}

}
