#pragma once

#include "WebSocketClient.h"

#include <string>

namespace LGTVDeviceListener {

	std::string RegisterWithLGTV(const std::string& url, const WebSocketClient::Options& options);

}
