#pragma once

#include <exception>
#include <string>

namespace gab {
	class GodotAiBridgeException : public std::exception{
	public:
		GodotAiBridgeException(const char* what) : std::exception(what) {}
		GodotAiBridgeException(const std::string& what): std::exception(what.c_str()){}
	};
};