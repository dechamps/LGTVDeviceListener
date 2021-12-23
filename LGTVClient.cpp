#include "LGTVClient.h"

namespace LGTVDeviceListener {

	void ConnectToLGTV(const std::string& url, const WebSocketClient::Options& options) {
		WebSocketClient webSocketClient(url, options);
		webSocketClient.Run([&] { webSocketClient.Send("foo"); });
	}

}
