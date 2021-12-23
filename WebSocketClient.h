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

		WebSocketClient(const std::string& url, const Options& options);

		void Run(const std::function<void()>& onOpen, const std::function<void(const std::string&)>& onMessage);

		void Send(const std::string& data);
		
	private:
		const int connectTimeoutSeconds;
		ix::WebSocket webSocket;
	};

}