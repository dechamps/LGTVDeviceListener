cmake_minimum_required(VERSION 3.21)
project(LGTVDeviceListener)

find_package(cxxopts REQUIRED)

set(CMAKE_CXX_STANDARD 20)
add_compile_options(
	/WX /W4 /permissive- /analyze

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

add_executable(LGTVDeviceListener LGTVDeviceListener.cpp)
target_link_libraries(LGTVDeviceListener PRIVATE cxxopts::cxxopts)
install(TARGETS LGTVDeviceListener RUNTIME DESTINATION bin)
