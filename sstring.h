
#pragma once
//#define _HAS_CXX17 1
#include <windows.h>
#include <string>

class sstring : public std::string
{
public:
	sstring() : std::string() {}
	sstring(const char *s) : std::string(s) {}
	sstring(const std::string &s) : std::string(s) {}
	sstring(std::string &&s) noexcept : std::string(std::move(s)) {}
	sstring(const sstring &s) : std::string(s) {}
	sstring(sstring &&s) noexcept : std::string(std::move(s)) {}
	sstring(std::string::size_type len, char c) : std::string(len, c) {}
    virtual ~sstring() {
		clear();
    }
    sstring& operator=(const char *s) 
	{
		clear();
	    std::string::operator=(s);
	    return *this;
	}
    sstring& operator=(const std::string& s) 
	{
		clear();
	    std::string::operator=(s);
	    return *this;
	}
	sstring& operator=(sstring&& s)
	{
		clear();
		std::string::operator=(s);
		return *this;
	}
	void clear()
	{
		const size_t size = length();
		if (size > 0) {
			char *p = &std::string::operator[](0);
//			char *p = data();
			SecureZeroMemory(p, size);
		}
		std::string::clear();
	}
};


// Local Variables:
// mode: c++
// coding: utf-8-with-signature
// tab-width: 4
// End:

