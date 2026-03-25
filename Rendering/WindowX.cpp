#include <windows.h>
#include "Window.h"
#if 0
#ifdef _WIN32
// Registry support methods
std::string Window::getRegistryKeyPath() const {
	return "SOFTWARE\\Multi-Viewport3DWindow";
}

bool writeRegistryValue(HKEY hKey, const std::string& valueName, DWORD value) {
	LSTATUS result = RegSetValueExA(hKey, valueName.c_str(), 0, REG_DWORD,
		reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
	return result == ERROR_SUCCESS;
}

bool readRegistryValue(HKEY hKey, const std::string& valueName, DWORD& value) {
	DWORD dataSize = sizeof(DWORD);
	DWORD dataType = REG_DWORD;
	LSTATUS result = RegQueryValueExA(hKey, valueName.c_str(), nullptr, &dataType,
		reinterpret_cast<BYTE*>(&value), &dataSize);
	return result == ERROR_SUCCESS && dataType == REG_DWORD;
}

void Window::saveWindowStateToRegistry() {
	if (!m_config.saveWindowState || !m_window) {
		return;
	}

	HKEY hKey;
	std::string keyPath = getRegistryKeyPath();

	// Create or open the registry key
	LSTATUS result = RegCreateKeyExA(HKEY_CURRENT_USER, keyPath.c_str(), 0, nullptr,
		REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);

	if (result != ERROR_SUCCESS) {
		std::cerr << "Failed to create/open registry key: " << keyPath << std::endl;
		return;
	}

	// Get current window position and size
	int x, y, width, height;
	glfwGetWindowPos(m_window, &x, &y);
	glfwGetWindowSize(m_window, &width, &height);

	// Save values to registry
	bool success = true;
	success &= writeRegistryValue(hKey, "WindowX", static_cast<DWORD>(x));
	success &= writeRegistryValue(hKey, "WindowY", static_cast<DWORD>(y));
	success &= writeRegistryValue(hKey, "WindowWidth", static_cast<DWORD>(width));
	success &= writeRegistryValue(hKey, "WindowHeight", static_cast<DWORD>(height));

	// Check if window is maximized
	if (glfwGetWindowAttrib(m_window, GLFW_MAXIMIZED)) {
		writeRegistryValue(hKey, "WindowMaximized", 1);
	}
	else {
		writeRegistryValue(hKey, "WindowMaximized", 0);
	}

	RegCloseKey(hKey);

	if (success) {
		std::cout << "Window state saved to registry: " << x << "x" << y << " " << width << "x" << height << std::endl;
	}
	else {
		std::cerr << "Failed to save some window state values to registry" << std::endl;
	}
}

void Window::loadWindowStateFromRegistry() {
	if (!m_config.saveWindowState) {
		return;
	}

	HKEY hKey;
	std::string keyPath = getRegistryKeyPath();

	// Open the registry key
	LSTATUS result = RegOpenKeyExA(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_READ, &hKey);

	if (result != ERROR_SUCCESS) {
		std::cout << "No saved window state found in registry" << std::endl;
		return;
	}

	// Read values from registry
	DWORD x, y, width, height, maximized;
	bool hasValidData = true;

	hasValidData &= readRegistryValue(hKey, "WindowX", x);
	hasValidData &= readRegistryValue(hKey, "WindowY", y);
	hasValidData &= readRegistryValue(hKey, "WindowWidth", width);
	hasValidData &= readRegistryValue(hKey, "WindowHeight", height);

	// Maximized state is optional
	if (!readRegistryValue(hKey, "WindowMaximized", maximized)) {
		maximized = 0;
	}

	RegCloseKey(hKey);

	if (hasValidData) {
		// Validate the values are reasonable
		if (width >= 100 && height >= 100 && width <= 10000 && height <= 10000) {
			m_config.width = static_cast<int>(width);
			m_config.height = static_cast<int>(height);

			std::cout << "Restored window state from registry: " << x << "x" << y << " "
				<< width << "x" << height << (maximized ? " (maximized)" : "") << std::endl;
		}
		else {
			std::cerr << "Invalid window dimensions in registry, using defaults" << std::endl;
		}
	}
	else {
		std::cout << "Incomplete window state data in registry, using defaults" << std::endl;
	}
}


void Window::restoreWindowStateFromRegistry(Window* window)
{
	// Restore window position if we have saved state
	if (m_config.saveWindowState) {
		HKEY hKey;
		std::string keyPath = getRegistryKeyPath();

		if (RegOpenKeyExA(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
			DWORD x, y, maximized;

			if (readRegistryValue(hKey, "WindowX", x) && readRegistryValue(hKey, "WindowY", y)) {
				glfwSetWindowPos(m_window, static_cast<int>(x), static_cast<int>(y));
			}

			if (readRegistryValue(hKey, "WindowMaximized", maximized) && maximized == 1) {
				glfwMaximizeWindow(m_window);
			}

			RegCloseKey(hKey);
		}
	}
}


#endif
#endif