#include "DeviceListener.h"
#include "LGTVClient.h"
#include "StringUtil.h"
#include "Log.h"

#include <cxxopts.hpp>

namespace LGTVDeviceListener {
	namespace {

		constexpr auto SERVICE_NAME = L"LGTVDeviceListener";

		enum class RunMode { CONSOLE, SERVICE };

		// Made global because otherwise it's difficult to get to while running as a service.
		int argc;
		const char* const* argv;

		void InitializeLog(RunMode runMode, bool verbose = false) {
			Log::Initialize({
				.verbose = verbose,
				.channel = runMode == RunMode::SERVICE ? Log::Channel::WINDOWS_EVENT_LOG : Log::Channel::STDERR,
			});
		}

		struct Options final {
			std::optional<std::string> url;
			std::optional<std::string> clientKey;
			std::optional<std::string> deviceName;
			std::optional<std::string> addInput;
			std::optional<std::string> removeInput;
			bool createService = false;
			bool verbose = false;
			int connectTimeoutSeconds = WebSocketClient::Options().connectTimeoutSeconds;
			int handshakeTimeoutSeconds = WebSocketClient::Options().handshakeTimeoutSeconds;
		};

		std::optional<Options> ParseCommandLine(RunMode runMode) {
			::cxxopts::Options cxxoptsOptions("LGTVDeviceListener", "LGTV Device Listener");
			Options options;
			cxxoptsOptions.add_options()
				("url", "URL of the LGTV websocket. For example `ws://192.168.1.42:3000`. If not specified, log device events only", ::cxxopts::value(options.url))
				("client-key", "LGTV client key. For example `0123456789abcdef0123456789abcdef`. If not specified, will register a new key", ::cxxopts::value(options.clientKey))
				("device-name", R"(The name of the device to watch. Typically starts with `\\?\`. If not specified, log device events only)", ::cxxopts::value(options.deviceName))
				("add-input", "Which TV input to switch to when the device is added. For example `HDMI_1`. If not specified, does nothing on add", ::cxxopts::value(options.addInput))
				("remove-input", "Which TV input to switch to when the device is removed. For example `HDMI_2`. If not specified, does nothing on remove", ::cxxopts::value(options.removeInput))
				("create-service", "Create a Windows service that runs with the other provided arguments", ::cxxopts::value(options.createService))
				("verbose", "Enable verbose logging", ::cxxopts::value(options.verbose))
				("connect-timeout-seconds", "How long to wait for the WebSocket connection to establish, in seconds", ::cxxopts::value(options.connectTimeoutSeconds))
				("handshake-timeout-seconds", "How long to wait for the WebSocket handshake to complete, in seconds", ::cxxopts::value(options.handshakeTimeoutSeconds));
			try {
				cxxoptsOptions.parse(argc, argv);
			}
			catch (const std::exception& exception) {
				InitializeLog(runMode);
				Log(Log::Level::ERR) << L"Wrong usage: " << ToWideString(exception.what(), CP_ACP) << L"\r\n\r\n" << ToWideString(cxxoptsOptions.help(), CP_UTF8);
				return std::nullopt;
			}
			return options;
		}

		struct ServiceHandleDeleter final {
			void operator()(SC_HANDLE handle) const {
				if (handle == NULL) return;
				CloseServiceHandle(handle);
			}
		};
		using UniqueServiceHandle = std::unique_ptr<std::remove_pointer_t<SC_HANDLE>, ServiceHandleDeleter>;

		void CreateService() {
			const UniqueServiceHandle serviceManager(OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE));
			if (serviceManager == NULL)
				throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Unable to open service manager");

			if (UniqueServiceHandle(CreateServiceW(
				/*hSCManager=*/serviceManager.get(),
				/*lpServiceName*/SERVICE_NAME,
				/*lpDisplayName=*/L"LGTVDeviceListener",
				/*dwDesiredAccess*/0,
				/*dwServiceType*/SERVICE_WIN32_OWN_PROCESS,
				/*dwStartType*/SERVICE_DEMAND_START,
				/*dwErrorControl*/SERVICE_ERROR_NORMAL,
				/*lpBinaryPathName*/GetCommandLineW(),
				/*lpLoadOrderGroup*/NULL,
				/*lpdwTagId*/NULL,
				/*lpDependencies*/NULL,
				/*lpServiceStartName=*/LR"(NT AUTHORITY\LocalService)",
				/*lpPassword=*/NULL
			)) == NULL)
				throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Unable to create service");
		}

		void RunDeviceListener(const Options& options, const std::function<void()>& onReady) {
			const WebSocketClient::Options webSocketClientOptions = {
				.connectTimeoutSeconds = options.connectTimeoutSeconds,
				.handshakeTimeoutSeconds = options.handshakeTimeoutSeconds
			};

			std::optional<std::string> clientKey = options.clientKey;
			if (!clientKey.has_value() && options.url.has_value()) {
				Log(Log::Level::INFO) << L"Registering new client key with LGTV";
				LGTVClient::Run(
					*options.url, { .webSocketClientOptions = webSocketClientOptions },
					[&](LGTVClient& lgtvClient, std::string_view newClientKey) {
						Log(Log::Level::INFO) << "LGTV client key: " << ToWideString(newClientKey, CP_UTF8);
						clientKey = newClientKey;
						lgtvClient.Close();
					});
			}

			const auto expectedDeviceName = options.deviceName.has_value() ? std::optional<std::wstring>(ToWideString(*options.deviceName, CP_ACP)) : std::nullopt;
			ListenToDeviceEvents(
				[&]{
					Log(Log::Level::INFO) << L"Listening for device events";
					onReady();
				},
				[&](DeviceEventType deviceEventType, std::wstring_view deviceName) {
				std::wstring_view deviceEventTypeString;

				const bool loggingOnly = !options.url.has_value();
				Log(loggingOnly ? Log::Level::INFO : Log::Level::VERBOSE) << L"Device " << [&] {
					switch (deviceEventType) {
					case DeviceEventType::ADDED: return L"added";
					case DeviceEventType::REMOVED: return L"removed";
					}
					::abort();
				}() << L": " << deviceName;

				if (loggingOnly || deviceName != expectedDeviceName) return;

				const auto input = [&] {
					switch (deviceEventType) {
					case DeviceEventType::ADDED: return options.addInput;
					case DeviceEventType::REMOVED: return options.removeInput;
					}
					::abort();
				}();
				if (!input.has_value()) return;
				Log(Log::Level::INFO) << L"Switching LGTV to input: " << ToWideString(*input, CP_UTF8);
				LGTVClient::Run(
					*options.url, { .clientKey = clientKey, .webSocketClientOptions = webSocketClientOptions },
					[&](LGTVClient& lgtvClient, std::string_view) {
						lgtvClient.SetInput(*input, [&] { lgtvClient.Close(); });
					});
			});
		}

		int Run(RunMode runMode, const std::function<void()>& onReady = [] {}) {
			const auto options = ::LGTVDeviceListener::ParseCommandLine(runMode);
			if (!options.has_value()) return EXIT_FAILURE;

			InitializeLog(runMode, options->verbose);
			try {
				if (options->createService && runMode != RunMode::SERVICE)
					CreateService();
				else
					RunDeviceListener(*options, onReady);
			}
			catch (const std::exception& exception) {
				Log(Log::Level::ERR) << "FATAL: " << ToWideString(exception.what(), CP_ACP);
				return EXIT_FAILURE;
			}
			return EXIT_SUCCESS;
		}

		class ServiceControlHandler final {
		public:
			ServiceControlHandler() {
				if (serviceStatusHandle == NULL)
					throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Unable to register service control handler");
				SetStatus(SERVICE_START_PENDING, SERVICE_ACCEPT_STOP);
			}

			~ServiceControlHandler() {
				// This means the service thread is unwinding, which currently always means an error occurred.
				// In any case, we *have* to send SERVICE_STOPPED because any further control requests will hit a destroyed ServiceControlHandler object.
				SetStatus(SERVICE_STOPPED, 0, true);
			}

			void OnReady() {
				SetStatus(SERVICE_RUNNING, SERVICE_ACCEPT_STOP);
			}

		private:
			static DWORD WINAPI Handler(DWORD control, DWORD eventType, LPVOID eventData, LPVOID context) {
				return static_cast<ServiceControlHandler*>(context)->Handle(control, eventType, eventData);
			}

			DWORD WINAPI Handle(DWORD control, DWORD, LPVOID) {
				switch (control) {
				case SERVICE_CONTROL_INTERROGATE: return NO_ERROR;
				case SERVICE_CONTROL_STOP:
					SetStatus(SERVICE_STOPPED, 0);
					return NO_ERROR;
				default: return ERROR_CALL_NOT_IMPLEMENTED;
				}
			}

			void SetStatus(DWORD currentState, DWORD controlsAccepted, bool error = false) {
				SERVICE_STATUS serviceStatus = {
					.dwServiceType = SERVICE_USER_OWN_PROCESS,
					.dwCurrentState = currentState,
					.dwControlsAccepted = controlsAccepted,
					// It looks like no matter what we do, the services UI will try to interpret the exit code as a win32 system error code (yes, even dwServiceSpecificExitCode).
					// Therefore zero would show as "This operation completed successfully". There doesn't seem be any "generic" or "unknown" win32 system error code so we just keep repeating ERROR_SERVICE_SPECIFIC_ERROR.
					.dwWin32ExitCode = error ? DWORD(ERROR_SERVICE_SPECIFIC_ERROR) : NO_ERROR,
					.dwServiceSpecificExitCode = error ? DWORD(ERROR_SERVICE_SPECIFIC_ERROR) : 0,
					.dwCheckPoint = 0,
					.dwWaitHint = 0,
				};
				if (SetServiceStatus(serviceStatusHandle, &serviceStatus) == 0)
					throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Unable to set service status");
			}

			const SERVICE_STATUS_HANDLE serviceStatusHandle = RegisterServiceCtrlHandlerExW(SERVICE_NAME, Handler, this);
		};

		VOID WINAPI RunService(DWORD, LPTSTR*) {
			try {
				ServiceControlHandler serviceControlHandler;
				Run(RunMode::SERVICE, [&] { serviceControlHandler.OnReady(); });
			}
			catch (const std::exception& exception) {
				InitializeLog(RunMode::SERVICE);
				Log(Log::Level::ERR) << "FATAL: " << ToWideString(exception.what(), CP_ACP);
			}
		}

	}
}

int main(int argc, const char* const* argv) {
	::LGTVDeviceListener::argc = argc;
	::LGTVDeviceListener::argv = argv;

	try {
		const SERVICE_TABLE_ENTRYA serviceTableEntries[] = {
			{.lpServiceName = const_cast<LPSTR>(""), .lpServiceProc = ::LGTVDeviceListener::RunService },
			{.lpServiceName = NULL, .lpServiceProc = NULL},
		};
		if (StartServiceCtrlDispatcherA(serviceTableEntries) != 0) return EXIT_SUCCESS;
		const auto serviceDispatcherError = GetLastError();
		if (serviceDispatcherError != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
			throw std::system_error(std::error_code(serviceDispatcherError, std::system_category()), "Unable to start service dispatcher");

		return ::LGTVDeviceListener::Run(::LGTVDeviceListener::RunMode::CONSOLE);
	}
	catch (const std::exception& exception) {
		std::cerr << "FATAL ERROR: " << exception.what() << std::endl;
		return EXIT_FAILURE;
	}
}
