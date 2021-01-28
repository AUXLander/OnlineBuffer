#pragma once

#include <Windows.h>

#include "clipboard_structs.h"
#include "xxhash.h"

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

	enum Format : unsigned int {
		F_NONE,
		F_TEXT		= CF_TEXT,
		F_UNICODE	= CF_UNICODETEXT,
		F_BITMAP	= CF_DIB
	};

	Status status;
	Format format;

	unsigned int length;

	XXH_hash_t hash;

	LPVOID lpdata;
};