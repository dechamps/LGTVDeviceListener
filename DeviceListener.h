#pragma once

#include <functional>
#include <string_view>

namespace LGTVDeviceListener {

	enum class DeviceEventType { REMOVED, ADDED };

	void ListenToDeviceEvents(
		const std::function<void()>& onReady,
		const std::function<void(DeviceEventType, std::wstring_view deviceName)>& onEvent);

}
