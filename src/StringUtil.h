#pragma once

#include <Windows.h>

#include <string_view>
#include <string>

namespace LGTVDeviceListener {

	std::wstring ToWideString(std::string_view input, UINT codePage);

}
