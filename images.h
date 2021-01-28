#pragma once
#include <stdint.h>
#include <Windows.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <atlbase.h>


#pragma pack(2)
struct DIB
{
	uint32_t biSize;
	int32_t  biWidth;
	int32_t  biHeight;
	uint16_t biPlanes;
	uint16_t biBitCount;
	uint32_t biCompression;
	uint32_t biSizeImage;
	int32_t  biXPelsPerMeter;
	int32_t  biYPelsPerMeter;
	uint32_t biClrUsed;
	uint32_t biClrImportant;
};

struct HEADER
{
	uint16_t type;
	uint32_t bfSize;
	uint32_t reserved;
	uint32_t offset;
};

struct BMP
{
	HEADER header;
	DIB dib;
};
#pragma pack(pop) 


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