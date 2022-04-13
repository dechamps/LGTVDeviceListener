#include "StringUtil.h"

#include <system_error>

namespace LGTVDeviceListener {

	std::wstring ToWideString(std::string_view input, UINT codePage) {
		if (input.size() == 0) return {};

		const auto size = ::MultiByteToWideChar(codePage, 0, input.data(), int(input.size()), NULL, 0);
		if (size <= 0) throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Unable to get size for conversion to wide string");

		std::wstring result(size, 0);
		if (::MultiByteToWideChar(codePage, 0, input.data(), int(input.size()), result.data(), int(result.size())) != int(result.size()))
			throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Unable to convert to wide string");
		return result;
	}

}
