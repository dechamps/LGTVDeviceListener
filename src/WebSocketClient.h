#pragma once

#include <IXWebSocket.h>

#include <string>

namespace LGTVDeviceListener {

	class WebSocketClient final {
	public:
		struct Options final {
			int connectTimeoutSeconds = 5;
			int handshakeTimeoutSeconds = 5;
			ix::SocketTLSOptions tlsOptions;
		};

		WebSocketClient(const WebSocketClient&) = delete;
		WebSocketClient& operator=(const WebSocketClient&) = delete;

		using OnMessage = void(const std::string&);
		using OnOpen = std::function<OnMessage>(WebSocketClient&);
		
		static void Run(const std::string& url, const Options& options, const std::function<OnOpen>& onOpen);

		void Send(const std::string& data);
		void Close();
		
	private:
		WebSocketClient() = default;
		~WebSocketClient() = default;

		ix::WebSocket webSocket;
	};

}