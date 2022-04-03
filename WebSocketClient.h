#pragma once

#include <ixwebsocket/IXWebSocket.h>

#include <string>

namespace LGTVDeviceListener {

	class WebSocketClient final {
	public:
		struct Options final {
			int connectTimeoutSeconds = 5;
			int handshakeTimeoutSeconds = 5;
		};

		WebSocketClient(const WebSocketClient&) = delete;
		WebSocketClient& operator=(const WebSocketClient&) = delete;

		static void Run(
			const std::string& url, const Options& options,
			std::function<void(WebSocketClient&)> onOpen,
			std::function<void(WebSocketClient&, const std::string&)> onMessage);

		void Send(const std::string& data);
		void Close();
		
	private:
		WebSocketClient() = default;

		ix::WebSocket webSocket;
	};

}