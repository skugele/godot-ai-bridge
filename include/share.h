#pragma once

#include <stdexcept>
#include <string>

namespace gab {
	class GodotAiBridgeException : public std::runtime_error{
	public:
		GodotAiBridgeException(const char* const what) : std::runtime_error(what) {}
		GodotAiBridgeException(const std::string& what): std::runtime_error(what.c_str()){}
	};
};
