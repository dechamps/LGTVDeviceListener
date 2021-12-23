#pragma once

#include "WebSocketClient.h"

#include <string>

namespace LGTVDeviceListener {

	void ConnectToLGTV(const std::string& url, const WebSocketClient::Options& options);

}
