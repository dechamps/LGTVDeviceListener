#include "DeviceListener.h"

#include "Log.h"

#include <Windows.h>
#include <Dbt.h>

#include <system_error>
#include <iostream>

namespace LGTVDeviceListener {

	namespace {

		class Window final {
		public:
			using OnMessage = void(HWND window, UINT messageIdentifier, WPARAM wParam, LPARAM lParam);

			Window(std::function<OnMessage> onMessage) : onMessage(std::move(onMessage)), windowHandle([&] {
				const auto windowHandle = ::CreateWindowW(windowClassName, L"LGTVDeviceListener DeviceListener", 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_MESSAGE, NULL, NULL, this);
				if (windowHandle == NULL)
					throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Unable to create DeviceListener window");

				return windowHandle;
			}()) {}

			Window(const Window&) = delete;
			Window& operator=(const Window&) = delete;

			~Window() {	DestroyWindow(windowHandle); }

			HWND GetWindowHandle() const { return windowHandle; }

		private:
			static void SetWindowUserDataOnCreate(HWND window, LPARAM lParam) {
				::SetLastError(NO_ERROR);
				::SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)((::CREATESTRUCT*)lParam)->lpCreateParams);
				const DWORD setWindowLongError = ::GetLastError();
				if (setWindowLongError != NO_ERROR)
					throw std::system_error(std::error_code(setWindowLongError, std::system_category()), "Unable to set GWLP_USERDATA");
			}

			static LONG_PTR GetWindowUserData(HWND window) {
				::SetLastError(NO_ERROR);
				const LONG_PTR windowUserData = ::GetWindowLongPtrW(window, GWLP_USERDATA);
				const DWORD getWindowLongError = ::GetLastError();
				if (getWindowLongError != NO_ERROR)
					throw std::system_error(std::error_code(getWindowLongError, std::system_category()), "Unable to set GWLP_USERDATA");
				return windowUserData;
			}

			static LRESULT WindowProcedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
				if (uMsg == WM_NCCREATE) SetWindowUserDataOnCreate(hwnd, lParam);

				auto window = reinterpret_cast<Window*>(GetWindowUserData(hwnd));
				if (window != NULL) window->onMessage(hwnd, uMsg, wParam, lParam);

				return DefWindowProcW(hwnd, uMsg, wParam, lParam);
			}

			static constexpr LPCWSTR windowClassName = L"LGTVDeviceListener_DeviceListener";

			class WindowClassRegistration final {
			public:
				WindowClassRegistration() {
					const ::WNDCLASSW windowClass = {
						.style = 0,
						.lpfnWndProc = WindowProcedure,
						.cbClsExtra = 0,
						.cbWndExtra = 0,
						.hInstance = NULL,
						.hIcon = NULL,
						.hCursor = NULL,
						.hbrBackground = NULL,
						.lpszMenuName = NULL,
						.lpszClassName = windowClassName,
					};
					if (::RegisterClassW(&windowClass) == 0)
						throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Unable to create DeviceListener window class");
				}

				WindowClassRegistration(const WindowClassRegistration&) = delete;
				WindowClassRegistration& operator=(const WindowClassRegistration&) = delete;

				~WindowClassRegistration() { UnregisterClassW(windowClassName, NULL); }
			};

			const std::function<OnMessage> onMessage;
			WindowClassRegistration windowClassRegistration;
			const HWND windowHandle;
		};

		class DeviceNotificationRegistration final {
		public:
			DeviceNotificationRegistration(HWND window) : deviceNotificationHandle([&] {
				::DEV_BROADCAST_DEVICEINTERFACE_W notificationFilter = {
					.dbcc_size = sizeof(notificationFilter),
					.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE,
				};
				const auto deviceNotificationHandle = RegisterDeviceNotificationW(window, &notificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES);
				if (deviceNotificationHandle == NULL)
					throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Unable to register device notification");
				return deviceNotificationHandle;
			}()) {}

			DeviceNotificationRegistration(const DeviceNotificationRegistration&) = delete;
			DeviceNotificationRegistration& operator=(const DeviceNotificationRegistration&) = delete;

			~DeviceNotificationRegistration() {	UnregisterDeviceNotification(deviceNotificationHandle);	}

		private:
			const HDEVNOTIFY deviceNotificationHandle;
		};

		void RunWindowMessageLoop(HWND window) {
			for (;;) {
				::MSG message;
				switch (::GetMessage(&message, window, 0, 0)) {
				case -1:
					throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Unable to get DeviceListener window message");
				case 0:
					break;
				default:
					::TranslateMessage(&message);
					::DispatchMessage(&message);
				}
			}
		}

	}

	void ListenToDeviceEvents(const std::function<void(DeviceEventType, std::wstring_view deviceName)>& onEvent) {
		Window window([&](HWND, UINT messageIdentifier, WPARAM wParam, LPARAM lParam) {
			if (messageIdentifier != WM_DEVICECHANGE) return;

			DeviceEventType deviceEventType;
			switch (wParam) {
			case DBT_DEVICEARRIVAL: deviceEventType = DeviceEventType::ADDED; break;
			case DBT_DEVICEREMOVECOMPLETE: deviceEventType = DeviceEventType::REMOVED; break;
			default: return;
			}

			const auto& deviceEventHeader = *reinterpret_cast<const DEV_BROADCAST_HDR*>(lParam);
			if (deviceEventHeader.dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE) return;

			const auto& deviceInterfaceEvent = reinterpret_cast<const ::DEV_BROADCAST_DEVICEINTERFACE_W&>(deviceEventHeader);
			onEvent(deviceEventType, deviceInterfaceEvent.dbcc_name);
		});
		DeviceNotificationRegistration deviceNotificationRegistration(window.GetWindowHandle());
		Log() << "Listening for device events";
		RunWindowMessageLoop(window.GetWindowHandle());
	}

}
