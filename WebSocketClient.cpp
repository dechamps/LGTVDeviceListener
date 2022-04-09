#include "WebSocketClient.h"

#include "Log.h"

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

	void WebSocketClient::Run(const std::string& url, const Options& options, const std::function<OnOpen>& onOpen) {
		WebSocketClient webSocketClient;
		auto& webSocket = webSocketClient.webSocket;

		webSocket.setUrl(url);
		webSocket.setHandshakeTimeout(options.handshakeTimeoutSeconds);
		webSocket.disableAutomaticReconnection();

		std::function<OnMessage> onMessage;
		webSocket.setOnMessageCallback([&](const ix::WebSocketMessagePtr& webSocketMessage) {
			using Type = ix::WebSocketMessageType;
			switch (webSocketMessage->type) {
			case Type::Message: {
				if (!onMessage) throw std::runtime_error("Unexpected ix::WebSocket message callback");
				onMessage(webSocketMessage->str);
			} break;
			case Type::Open: {
				if (onMessage) throw std::runtime_error("ix::WebSocket delivered Open message twice");
				onMessage = onOpen(webSocketClient);
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
		webSocket.connect(options.connectTimeoutSeconds);
		webSocket.run();
	}

	void WebSocketClient::Send(const std::string& data) {
		webSocket.send(data);
	}

	void WebSocketClient::Close() {
		webSocket.close();
	}

}
