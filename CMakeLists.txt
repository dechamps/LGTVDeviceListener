cmake_minimum_required(VERSION 3.21)
project(LGTVDeviceListener)

find_package(cxxopts REQUIRED)
find_package(ixwebsocket REQUIRED)
find_package(nlohmann_json REQUIRED)

set(CMAKE_CXX_STANDARD 20)
add_compile_options(
	/Zc:__cplusplus /utf-8

	/permissive-
	/WX /W4 /external:anglebrackets /external:W0
	/analyze /analyze:external-

	# Suppress warnings about shadowing declarations.
	#
	# In most cases, this happens when a lambda is used to initialize some
	# variable, and the lambda declares a local variable with the same name as the
	# variable it's tasked with initializing. In such cases the shadowing is
	# actually desirable, because it prevents one from accidentally using the (not
	# yet initialized) outer variable instead of the (valid) local variable within
	# the lambda.
	/wd4458 /wd4456
)

add_library(StringUtil StringUtil.cpp)

add_library(Log Log.cpp)

add_library(DeviceListener DeviceListener.cpp)
target_link_libraries(DeviceListener
	PRIVATE StringUtil
	PRIVATE Log
)

add_library(WebSocketClient WebSocketClient.cpp)
target_link_libraries(WebSocketClient
	PUBLIC ixwebsocket::ixwebsocket
)

add_library(LGTVClient LGTVClient.cpp)
target_link_libraries(LGTVClient
	PRIVATE StringUtil
	PRIVATE Log
	PRIVATE WebSocketClient
	PRIVATE nlohmann_json
)

add_executable(LGTVDeviceListener LGTVDeviceListener.cpp)
target_link_libraries(LGTVDeviceListener
	PRIVATE StringUtil
	PRIVATE Log
	PRIVATE DeviceListener
	PRIVATE LGTVClient
	PRIVATE cxxopts::cxxopts
	PRIVATE ws2_32
)
install(TARGETS LGTVDeviceListener)
