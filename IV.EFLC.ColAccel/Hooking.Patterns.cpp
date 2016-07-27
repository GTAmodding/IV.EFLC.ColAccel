/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

 // client-side shared include file
#if defined(_MSC_VER)
#pragma warning(disable: 4251) // needs to have dll-interface to be used by clients
#pragma warning(disable: 4273) // inconsistent dll linkage
#pragma warning(disable: 4275) // non dll-interface class used as base
#pragma warning(disable: 4244) // possible loss of data
#pragma warning(disable: 4800) // forcing value to bool
#pragma warning(disable: 4290) // C++ exception specification ignored

 // MSVC odd defines
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_DEPRECATE
#endif

#if defined(_WIN32)
 // platform primary include
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <versionhelpers.h>

#ifdef GTA_NY
#include <d3d9.h>
#endif

#define DLL_IMPORT __declspec(dllimport)
#define DLL_EXPORT __declspec(dllexport)

#define __thread __declspec(thread)
#elif defined(__GNUC__)
#define DLL_IMPORT 
#define DLL_EXPORT __attribute__((visibility("default")))

#define FORCEINLINE __attribute__((always_inline))

#include <unistd.h>

 // compatibility
#define _stricmp strcasecmp
#define _strnicmp strncasecmp

#define _countof(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
#endif

#undef NDEBUG

 // C/C++ headers
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef NDEBUG
#undef NDEBUG
#include <assert.h>
#define NDEBUG
#else
#include <assert.h>
#endif

#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <list>
#include <atomic>
#include <locale>
#include <codecvt>
#include <thread>
#include <algorithm>

 // our common includes
#define COMPONENT_EXPORT

 // string types per-platform
#if defined(_WIN32)
class fwPlatformString : public std::wstring
{
private:
	inline std::wstring ConvertString(const char* narrowString)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
		return converter.from_bytes(narrowString);
	}

public:
	fwPlatformString()
		: std::wstring()
	{
	}

	fwPlatformString(const std::wstring& arg)
		: std::wstring(arg)
	{
	}

	fwPlatformString(const wchar_t* arg)
		: std::wstring(arg)
	{
	}

	inline fwPlatformString(const char* narrowString)
		: std::wstring(ConvertString(narrowString))
	{

	}
};
typedef wchar_t pchar_t;

#define _pfopen _wfopen
#define _P(x) L##x
#else
class fwPlatformString : public std::string
{
private:
	inline std::string ConvertString(const wchar_t* wideString)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
		return converter.to_bytes(wideString);
	}

public:
	fwPlatformString()
		: std::string()
	{
	}

	fwPlatformString(const std::string& arg)
		: std::string(arg)
	{
	}

	fwPlatformString(const char* arg)
		: std::string(arg)
	{
	}

	inline fwPlatformString(const wchar_t* wideString)
		: std::string(ConvertString(wideString))
	{

	}
};

typedef char pchar_t;

#define _pfopen fopen
#define _P(x) x
#endif

#include "Hooking.Patterns.h"
#include <cstdint>
#include <sstream>

#include <immintrin.h>

// from boost someplace
template <std::uint64_t FnvPrime, std::uint64_t OffsetBasis>
struct basic_fnv_1
{
	std::uint64_t operator()(std::string const& text) const
	{
		std::uint64_t hash = OffsetBasis;
		for (std::string::const_iterator it = text.begin(), end = text.end();
			 it != end; ++it)
		{
			hash *= FnvPrime;
			hash ^= *it;
		}

		return hash;
	}
};

const std::uint64_t fnv_prime = 1099511628211u;
const std::uint64_t fnv_offset_basis = 14695981039346656037u;

typedef basic_fnv_1<fnv_prime, fnv_offset_basis> fnv_1;

namespace hook
{
static std::multimap<uint64_t, uintptr_t> g_hints;

static void TransformPattern(const std::string& pattern, std::string& data, std::string& mask)
{
	std::stringstream dataStr;
	std::stringstream maskStr;

	uint8_t tempDigit = 0;
	bool tempFlag = false;

	for (auto& ch : pattern)
	{
		if (ch == ' ')
		{
			continue;
		}
		else if (ch == '?')
		{
			dataStr << '\x00';
			maskStr << '?';
		}
		else if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f'))
		{
			char str[] = { ch, 0 };
			int thisDigit = strtol(str, nullptr, 16);

			if (!tempFlag)
			{
				tempDigit = (thisDigit << 4);
				tempFlag = true;
			}
			else
			{
				tempDigit |= thisDigit;
				tempFlag = false;

				dataStr << tempDigit;
				maskStr << 'x';
			}
		}
	}

	data = dataStr.str();
	mask = maskStr.str();
}

class executable_meta
{
private:
	uintptr_t m_begin;
	uintptr_t m_end;

public:
	template<typename TReturn, typename TOffset>
	TReturn* getRVA(TOffset rva)
	{
		return (TReturn*)(m_begin + rva);
	}

	executable_meta(void* module)
		: m_begin((uintptr_t)module), m_end(0)
	{
		PIMAGE_DOS_HEADER dosHeader = getRVA<IMAGE_DOS_HEADER>(0);
		PIMAGE_NT_HEADERS ntHeader = getRVA<IMAGE_NT_HEADERS>(dosHeader->e_lfanew);

		m_end = m_begin + ntHeader->OptionalHeader.SizeOfCode;
	}

	inline uintptr_t begin() { return m_begin; }
	inline uintptr_t end()   { return m_end; }
	inline void set_begin(uintptr_t bgn) {  m_begin = bgn; }
	inline void set_end(uintptr_t end) {  m_end = end; }
};

void pattern::Initialize(const char* pattern, size_t length)
{
	// get the hash for the base pattern
	std::string baseString(pattern, length);
	m_hash = fnv_1()(baseString);

	m_matched = false;

	// transform the base pattern from IDA format to canonical format
	TransformPattern(baseString, m_bytes, m_mask);

	m_size = m_mask.size();

	// if there's hints, try those first
	auto range = g_hints.equal_range(m_hash);

	if (range.first != range.second)
	{
		std::for_each(range.first, range.second, [&] (const std::pair<uint64_t, uintptr_t>& hint)
		{
			ConsiderMatch(hint.second);
		});

		// if the hints succeeded, we don't need to do anything more
		if (m_matches.size() > 0)
		{
			m_matched = true;
			return;
		}
	}
}

void pattern::EnsureMatches(int maxCount)
{
	if (m_matched)
	{
		return;
	}

	// scan the executable for code
	executable_meta executable(m_module);

	if (range_start || range_end)
	{
		executable.set_begin(range_start);
		executable.set_end(range_end);
	}

	// check if SSE 4.2 is supported
	int cpuid[4];
	__cpuid(cpuid, 0);

	bool sse42 = false;

	if (m_mask.size() <= 16)
	{
		if (cpuid[0] >= 1)
		{
			__cpuidex(cpuid, 1, 0);

			sse42 = (cpuid[2] & (1 << 20));
		}
	}

	auto matchSuccess = [&] (uintptr_t address)
	{
#if 0
		Citizen_PatternSaveHint(m_hash, address);
#endif
		g_hints.insert(std::make_pair(m_hash, address));

		return (m_matches.size() == maxCount);
	};

	LARGE_INTEGER ticks;
	QueryPerformanceCounter(&ticks);

	uint64_t startTicksOld = ticks.QuadPart;

	if (!sse42)
	{
		for (uintptr_t i = executable.begin(); i <= executable.end(); i++)
		{
			if (ConsiderMatch(i))
			{
				if (matchSuccess(i))
				{
					break;
				}
			}
		}
	}
	else
	{
		__declspec(align(16)) char desiredMask[16] = { 0 };

		for (unsigned int i = 0; i < m_mask.size(); i++)
		{
			desiredMask[i / 8] |= ((m_mask[i] == '?') ? 0 : 1) << (i % 8);
		}

		__m128i mask = _mm_load_si128(reinterpret_cast<const __m128i*>(desiredMask));
		__m128i comparand = _mm_loadu_si128(reinterpret_cast<const __m128i*>(m_bytes.c_str()));

		for (uintptr_t i = executable.begin(); i <= executable.end(); i++)
		{
			__m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(i));
			__m128i result = _mm_cmpestrm(value, 16, comparand, m_bytes.size(), _SIDD_CMP_EQUAL_EACH);

			// as the result can match more bits than the mask contains
			__m128i matches = _mm_and_si128(mask, result);
			__m128i equivalence = _mm_xor_si128(mask, matches);

			if (_mm_test_all_zeros(equivalence, equivalence))
			{
				m_matches.push_back(pattern_match((void*)i));

				if (matchSuccess(i))
				{
					break;
				}
			}
		}
	}

	m_matched = true;
}

bool pattern::ConsiderMatch(uintptr_t offset)
{
	const char* pattern = m_bytes.c_str();
	const char* mask = m_mask.c_str();

	char* ptr = reinterpret_cast<char*>(offset);

	for (size_t i = 0; i < m_size; i++)
	{
		if (mask[i] == '?')
		{
			continue;
		}

		if (pattern[i] != ptr[i])
		{
			return false;
		}
	}

	m_matches.push_back(pattern_match(ptr));

	return true;
}

void pattern::hint(uint64_t hash, uintptr_t address)
{
	auto range = g_hints.equal_range(hash);

	for (auto it = range.first; it != range.second; it++)
	{
		if (it->second == address)
		{
			return;
		}
	}

	g_hints.insert(std::make_pair(hash, address));
}
}