#include "DeviceListener.h"
#include "LGTVClient.h"
#include "StringUtil.h"
#include "Log.h"

#include <cxxopts.hpp>

#include <Shlobj.h>
#include <accctrl.h>
#include <aclapi.h>

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
			std::optional<std::string> clientKeyFile;
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
				("client-key-file", R"(Path to the file holding the LGTV client key. If the file doesn't exist, a new client key will be registered and written to the file (default: %ProgramData%\LGTVDeviceListener.client-key)", ::cxxopts::value(options.clientKeyFile))
				("device-name", R"(The name of the device to watch. Typically starts with `\\?\`. If not specified, log device events only)", ::cxxopts::value(options.deviceName))
				("add-input", "Which TV input to switch to when the device is added. For example `HDMI_1`. If not specified, does nothing on add", ::cxxopts::value(options.addInput))
				("remove-input", "Which TV input to switch to when the device is removed. For example `HDMI_2`. If not specified, does nothing on remove", ::cxxopts::value(options.removeInput))
				("create-service", "Create a Windows service that runs with the other provided arguments, then start it", ::cxxopts::value(options.createService))
				("verbose", "Enable verbose logging", ::cxxopts::value(options.verbose))
				("connect-timeout-seconds", "How long to wait for the WebSocket connection to establish, in seconds (default: " + std::to_string(Options().connectTimeoutSeconds) + ")", ::cxxopts::value(options.connectTimeoutSeconds))
				("handshake-timeout-seconds", "How long to wait for the WebSocket handshake to complete, in seconds (default: " + std::to_string(Options().handshakeTimeoutSeconds) + ")", ::cxxopts::value(options.handshakeTimeoutSeconds));
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

		struct HandleDeleter final {
			void operator()(HANDLE handle) const {
				if (handle == NULL || handle == INVALID_HANDLE_VALUE) return;
				CloseHandle(handle);
			}
		};
		using UniqueHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, HandleDeleter>;

		struct LocalHeapDeleter final {
			void operator()(HLOCAL hmem) const {
				if (LocalFree(hmem) != NULL)
					throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Unable to free local heap memory");
			}
		};
		template <typename T>
		using UniqueHeapPtr = std::unique_ptr<T, LocalHeapDeleter>;

		std::wstring GetClientKeyPath(const std::optional<std::string>& file) {
			if (file.has_value()) return ToWideString(*file, CP_ACP);
			PWSTR path = NULL;
			const auto hresult = ::SHGetKnownFolderPath(FOLDERID_ProgramData, 0, NULL, &path);
			if (hresult != S_OK) {
				::CoTaskMemFree(path);
				throw std::runtime_error("Unable to get ProgramData folder path [" + std::to_string(hresult) + "]");
			}
			auto pathString = std::wstring(path) + LR"(\LGTVDeviceListener.client-key)";
			::CoTaskMemFree(path);
			return pathString;
		}

		std::optional<std::string> ReadClientKey(const std::wstring& path) {
			UniqueHandle handle(::CreateFileW(
				/*lpFileName=*/path.c_str(),
				/*dwDesiredAccess=*/GENERIC_READ,
				/*dwShareMode=*/FILE_SHARE_READ,
				/*lpSecurityAttributes=*/NULL,
				/*dwCreationDisposition*/OPEN_EXISTING,
				/*dwFlagsAndAttributes=*/FILE_FLAG_SEQUENTIAL_SCAN,
				/*hTemplateFile*/NULL
			));
			if (handle.get() == INVALID_HANDLE_VALUE) {
				const auto error = ::GetLastError();
				if (error == ERROR_FILE_NOT_FOUND) return std::nullopt;
				throw std::system_error(std::error_code(error, std::system_category()), "Failed to open client key file");
			}

			char contents[4096];
			DWORD bytesRead;
			if (ReadFile(handle.get(), contents, sizeof(contents), &bytesRead, NULL) == 0)
				throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Failed to read client key file");
			if (bytesRead >= sizeof(contents))
				throw std::runtime_error("Client key file is too large");
			return std::string(contents, bytesRead);
		}

		UniqueHeapPtr<ACL> CreateAcl(ULONG explicitEntriesCount, const EXPLICIT_ACCESS_W* explicitEntries) {
			PACL acl;
			const auto setEntriesInAclError = ::SetEntriesInAclW(explicitEntriesCount, const_cast<EXPLICIT_ACCESS_W*>(explicitEntries), NULL, &acl);
			if (setEntriesInAclError != ERROR_SUCCESS)
				throw std::system_error(std::error_code(setEntriesInAclError, std::system_category()), "Failed to set ACL entries");
			return UniqueHeapPtr<ACL>(acl);
		}

		void WriteClientKey(const std::wstring& path, std::string_view clientKey) {
			const auto serviceTrusteeName = std::wstring(LR"(NT SERVICE\)") + SERVICE_NAME;
			EXPLICIT_ACCESS_W explicitEntries[] = {
				{
					.grfAccessPermissions = GENERIC_ALL,
					.grfAccessMode = GRANT_ACCESS,
					.grfInheritance = NO_INHERITANCE,
					.Trustee = {
						.pMultipleTrustee = NULL,
						.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE,
						.TrusteeForm = TRUSTEE_IS_NAME,
						.TrusteeType = TRUSTEE_IS_USER,
						.ptstrName = const_cast<wchar_t*>(L"CREATOR OWNER"),
					},
				},
				{
					.grfAccessPermissions = GENERIC_READ,
					.grfAccessMode = GRANT_ACCESS,
					.grfInheritance = NO_INHERITANCE,
					.Trustee = {
						.pMultipleTrustee = NULL,
						.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE,
						.TrusteeForm = TRUSTEE_IS_NAME,
						.TrusteeType = TRUSTEE_IS_USER,
						.ptstrName = const_cast<wchar_t*>(serviceTrusteeName.c_str()),
					},
				},
			};
			const auto acl = CreateAcl(sizeof(explicitEntries) / sizeof(*explicitEntries), explicitEntries);
			SECURITY_DESCRIPTOR securityDescriptor;
			if (::InitializeSecurityDescriptor(&securityDescriptor, SECURITY_DESCRIPTOR_REVISION) == 0)
				throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Failed to initialize security descriptor");
			if (::SetSecurityDescriptorDacl(&securityDescriptor, TRUE, acl.get(), FALSE) == 0)
				throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Failed to set DACL");
			if (::SetSecurityDescriptorControl(&securityDescriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED) == 0)
				throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Failed to set security control bits");

			SECURITY_ATTRIBUTES securityAttributes = {
				.nLength = sizeof(securityAttributes),
				.lpSecurityDescriptor = &securityDescriptor,
				.bInheritHandle = FALSE,
			};

			UniqueHandle handle(::CreateFileW(
				/*lpFileName=*/path.c_str(),
				/*dwDesiredAccess=*/GENERIC_WRITE,
				/*dwShareMode=*/0,
				/*lpSecurityAttributes=*/&securityAttributes,
				/*dwCreationDisposition*/CREATE_NEW,
				/*dwFlagsAndAttributes=*/FILE_ATTRIBUTE_NORMAL,
				/*hTemplateFile*/NULL
			));
			if (handle.get() == INVALID_HANDLE_VALUE)
				throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Failed to write client key file");

			DWORD bytesWritten;
			if (WriteFile(handle.get(), clientKey.data(), static_cast<DWORD>(clientKey.size()), &bytesWritten, NULL) == 0)
				throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Failed to read client key file");
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

			const UniqueServiceHandle service(CreateServiceW(
				/*hSCManager=*/serviceManager.get(),
				/*lpServiceName*/SERVICE_NAME,
				/*lpDisplayName=*/L"LGTVDeviceListener",
				/*dwDesiredAccess*/SERVICE_CHANGE_CONFIG | SERVICE_START,
				/*dwServiceType*/SERVICE_WIN32_OWN_PROCESS,
				/*dwStartType*/SERVICE_AUTO_START,
				/*dwErrorControl*/SERVICE_ERROR_NORMAL,
				/*lpBinaryPathName*/GetCommandLineW(),
				/*lpLoadOrderGroup*/NULL,
				/*lpdwTagId*/NULL,
				/*lpDependencies*/NULL,
				/*lpServiceStartName=*/LR"(NT AUTHORITY\LocalService)",
				/*lpPassword=*/NULL
			));
			if (service == NULL)
				throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Unable to create service");

			auto changeConfig = [&](DWORD infoLevel, LPVOID info) {
				if (ChangeServiceConfig2W(service.get(), infoLevel, info) == 0)
					throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Unable to change service configuration");
			};
			{
				SERVICE_DESCRIPTIONW description = { .lpDescription = const_cast<wchar_t*>(L"https://github.com/dechamps/LGTVDeviceListener") };
				changeConfig(SERVICE_CONFIG_DESCRIPTION, &description);
			}
			{
				SERVICE_REQUIRED_PRIVILEGES_INFOW requiredPrivileges = { .pmszRequiredPrivileges = const_cast<wchar_t*>(L"\0") };
				changeConfig(SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, &requiredPrivileges);
			}
			{
				// Ideally we'd use RESTRICTED here, but that makes the DeviceListener CreateWindow call fail with ERROR_ACCESS_DENIED for some reason.
				SERVICE_SID_INFO sid = { .dwServiceSidType = SERVICE_SID_TYPE_UNRESTRICTED };
				changeConfig(SERVICE_CONFIG_SERVICE_SID_INFO, &sid);
			}

			if (StartServiceW(service.get(), 0, NULL) == 0)
				throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Unable to start service");

			Log(Log::Level::INFO) << L"Service has been created and started. Any messages/errors from the service will be sent to the Windows Application Event Log.";
		}

		void RunDeviceListener(const Options& options, const std::function<void()>& onReady) {
			const WebSocketClient::Options webSocketClientOptions = {
				.connectTimeoutSeconds = options.connectTimeoutSeconds,
				.handshakeTimeoutSeconds = options.handshakeTimeoutSeconds
			};

			const auto clientKeyPath = GetClientKeyPath(options.clientKeyFile);
			Log(Log::Level::VERBOSE) << L"Using client key file: " << clientKeyPath;
			auto clientKey = ReadClientKey(clientKeyPath);
			if (clientKey.has_value())
				Log(Log::Level::VERBOSE) << L"Successfully loaded client key";
			else if (options.url.has_value()) {
				Log(Log::Level::INFO) << L"Client key file not found - registering new client key with LGTV";
				LGTVClient::Run(
					*options.url, { .webSocketClientOptions = webSocketClientOptions },
					[&](LGTVClient& lgtvClient, std::string_view newClientKey) {
						Log(Log::Level::INFO) << "New LGTV client key successfully obtained";
						clientKey = newClientKey;
						lgtvClient.Close();
					});
				WriteClientKey(clientKeyPath, *clientKey);
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
