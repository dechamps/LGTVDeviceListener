#include "DeviceListener.h"

#include <Windows.h>
#include <Dbt.h>

#include <system_error>
#include <iostream>

namespace LGTVDeviceListener {

	namespace {

		LRESULT DeviceListenerWindowProcedure(HWND window, UINT messageIdentifier, WPARAM wParam, LPARAM lParam) {
			if (messageIdentifier == WM_DEVICECHANGE) {
				std::wcout << L"WM_DEVICECHANGE " << wParam << std::endl;
				if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
					const auto& deviceEventHeader = *reinterpret_cast<const DEV_BROADCAST_HDR*>(lParam);
					if (deviceEventHeader.dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
						const auto& deviceInterfaceEvent = reinterpret_cast<const ::DEV_BROADCAST_DEVICEINTERFACE_W&>(deviceEventHeader);
						std::wcout << L"Device name: " << deviceInterfaceEvent.dbcc_name << std::endl;
					}
				}
			}
			return DefWindowProcW(window, messageIdentifier, wParam, lParam);
		}

		class Window final {
		public:
			Window() : windowHandle([&] {
				const auto windowHandle = ::CreateWindowW(windowClassName, L"LGTVDeviceListener DeviceListener", 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_MESSAGE, NULL, NULL, NULL);
				if (windowHandle == NULL)
					throw std::system_error(std::error_code(::GetLastError(), std::system_category()), "Unable to create DeviceListener window");

				return windowHandle;
			}()) {}

			Window(const Window&) = delete;
			Window& operator=(const Window&) = delete;

			~Window() {	DestroyWindow(windowHandle); }

			HWND GetWindowHandle() const { return windowHandle; }

		private:
			static constexpr LPCWSTR windowClassName = L"LGTVDeviceListener_DeviceListener";

			class WindowClassRegistration final {
			public:
				WindowClassRegistration() {
					const ::WNDCLASSW windowClass = {
						.style = 0,
						.lpfnWndProc = DeviceListenerWindowProcedure,
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

	void ListenToDeviceEvents() {
		Window window;
		DeviceNotificationRegistration deviceNotificationRegistration(window.GetWindowHandle());
		RunWindowMessageLoop(window.GetWindowHandle());
	}

}
