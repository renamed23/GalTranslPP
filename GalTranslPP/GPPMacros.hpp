#ifndef GPPMACROS
#define GPPMACROS

#define _CRT_SECURE_NO_WARNINGS
#define _RANGES_
#define SPDLOG_WCHAR_FILENAMES

#ifdef PYBIND11_HEADERS 
#define PYBIND11_DETAILED_ERROR_MESSAGES
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/stl_bind.h>
#include <pybind11/embed.h>
#include <pybind11/subinterpreter.h>
#endif

#ifdef PCRE2_HEADERS
#include <jpcre2.hpp>
using jpc = jpcre2::select<char>;
#endif

#define NESTED_CVT(className, memberName) sol::property([](className& self) \
{ \
	return sol::nested<decltype(className::memberName)>(self.memberName); \
}, [](className& self, decltype(className::memberName) table) { self.memberName = std::move(table); }) 

#endif
