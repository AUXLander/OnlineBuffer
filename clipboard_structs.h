#pragma once

#include <Windows.h>
#include "xxhash.h"
#include "images.h"

#ifdef _WIN64

typedef XXH64_hash_t XXH_hash_t;

XXH_hash_t inline XXH(const void* input, size_t size, XXH_hash_t seed)
{
	return XXH3_64bits(input, size);
}
#else

typedef XXH32_hash_t XXH_hash_t;

XXH_hash_t inline XXH(const void* input, size_t size, XXH_hash_t seed)
{
	return XXH32(input, size, seed);
}
#endif


struct ClipboardState
{
	enum class Status : unsigned int {
		OK,
		NotAvaible,
		Changed
	};

	enum class Format : unsigned int {
		F_NONE	  = CF_NULL,
		F_TEXT	  = CF_TEXT,
		F_UNICODE = CF_UNICODETEXT,
		F_BITMAP  = CF_DIB
	};

	static UINT uint(ClipboardState::Format __format) 
	{
		switch (__format)
		{
			case ClipboardState::Format::F_TEXT:
			{
				return CF_TEXT;
			};

			case ClipboardState::Format::F_UNICODE:
			{
				return CF_UNICODETEXT;
			};

			case ClipboardState::Format::F_BITMAP:
			{
				return CF_DIB;
			}
		}

		return NULL;
	}

	Status status;
	Format format;

	LPVOID lpdata;

	XXH_hash_t hash;

	unsigned int length;
};

uint32_t GetClipboardDataLength(ClipboardState::Format format, void* data, void* pszdata)
{
	

	switch (format)
	{
		case ClipboardState::Format::F_TEXT:
		{
			return std::strlen(reinterpret_cast<const char*>(data)) + 1;
		};

		case ClipboardState::Format::F_UNICODE:
		{
			return std::wcslen(reinterpret_cast<const wchar_t*>(data)) + 1;
		};

		case ClipboardState::Format::F_BITMAP:
		{
			return reinterpret_cast<DIB*>(pszdata)->biSizeImage + sizeof(BMP);
		};
	}

	return NULL;
}