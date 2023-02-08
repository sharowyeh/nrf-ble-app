#pragma once
// simple helper functions for generic usage

#include <string>
#include <vector>

static inline std::vector<std::string> split(std::string str, std::string delimiter) {
	std::vector<std::string> result;
	size_t pos = 0;
	while (pos <= str.size()) {
		auto token = str.substr(pos, str.find(delimiter, pos) - pos);
		result.push_back(token);
		pos += token.length() + delimiter.length();
	}
	return result;
}

// trim from start (in place)
static inline void ltrim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
		return !std::isspace(ch);
		}));
}

// trim from end (in place)
static inline void rtrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
		return !std::isspace(ch);
		}).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string& s) {
	rtrim(s);
	ltrim(s);
}