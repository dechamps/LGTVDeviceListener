#include <cxxopts.hpp>

int main(int argc, char** argv) {
	::cxxopts::Options options("LGTVDeviceListener", "LGTV Device Listener");
	try {
		options.parse(argc, argv);
	}
	catch (const std::exception& exception) {
		std::cerr << "USAGE ERROR: " << exception.what() << std::endl;
		std::cerr << std::endl;
		std::cerr << options.help() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
