#pragma once
#include <stdio.h>
#include <tchar.h>
#include <iostream>
#include <functional>
#include <Windows.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <atlbase.h>
#include <wchar.h>

#include "helpers.h"
#include "message.h"
#include "server.h"
#include "xxhash.h"

#include <filesystem>
#include <iostream>
#include <fstream>


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
	unsigned int  type;
	unsigned int  code;
	unsigned char data[65];
};

enum class ClipboardStateStatus : unsigned int {
	OK,
	NotAvaible,
	Changed
};

enum ClipboardStateFormat : unsigned int {
	F_NONE,
	F_TEXT = CF_TEXT,
	F_UNICODE = CF_UNICODETEXT,
	F_BITMAP = CF_DIB
};

struct ClipboardState2
{
	ClipboardStateStatus status;
	ClipboardStateFormat format;

	unsigned int length;

	XXH_hash_t hash;

	LPVOID lpdata;
};




#pragma pack(2)
struct DIB
{
	std::uint32_t biSize;
	std::int32_t  biWidth;
	std::int32_t  biHeight;
	std::uint16_t biPlanes;
	std::uint16_t biBitCount;
	std::uint32_t biCompression;
	std::uint32_t biSizeImage;
	std::int32_t  biXPelsPerMeter;
	std::int32_t  biYPelsPerMeter;
	std::uint32_t biClrUsed;
	std::uint32_t biClrImportant;
};

struct HEADER
{
	std::uint16_t type;
	std::uint32_t bfSize;
	std::uint32_t reserved;
	std::uint32_t offset;
};

struct BMP
{
	HEADER header;
	DIB dib;
};

#pragma pack(pop) 

uint32_t GetClipboardDataLength(ClipboardStateFormat format, void* data, void* pszdata)
{
	switch (format)
	{
		case ClipboardStateFormat::F_TEXT:
		{
			return std::strlen(reinterpret_cast<const char*>(data)) + 1;
		};

		case ClipboardStateFormat::F_UNICODE:
		{
			return std::wcslen(reinterpret_cast<const wchar_t*>(data)) + 1;
		};

		case ClipboardStateFormat::F_BITMAP:
		{
			return reinterpret_cast<DIB*>(pszdata)->biSizeImage + sizeof(BMP);
		};
	}

	return NULL;
}

template<class T> inline void writebin(std::ofstream &file, T* data, std::streamsize size = sizeof(T))
{
	file.write(reinterpret_cast<char*>(data), size);
}

template<class T> inline void readbin(std::ifstream& file, T* data, std::streamsize size = sizeof(T))
{
	file.read(reinterpret_cast<char*>(data), size);
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	unsigned int numb  = 0; // number of image encoders
	unsigned int size = 0; // size of the image encoder array in bytes

	Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

	Gdiplus::GetImageEncodersSize(&numb, &size);

	pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	
	if (size == 0 || pImageCodecInfo == NULL)
	{
		return -1;
	}

	Gdiplus::GetImageEncoders(numb, size, pImageCodecInfo);

	int index;

	for (index = 0; index < numb; index++)
	{
		if (wcscmp(pImageCodecInfo[index].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[index].Clsid;
			
			goto out;
		}
	}

	index = -1;

out:
	free(pImageCodecInfo);
	return index;
}


ClipboardStateStatus GetCBData(ClipboardState2& state)
{
	HANDLE hData;
	LPVOID pszData;

	if (OpenClipboard(nullptr))
	{
		if (!IsClipboardFormatAvailable(state.format))
		{
			if (IsClipboardFormatAvailable(ClipboardStateFormat::F_UNICODE))
			{
				state.format = ClipboardStateFormat::F_UNICODE;
				state.status = ClipboardStateStatus::Changed;
			}
			else if(IsClipboardFormatAvailable(ClipboardStateFormat::F_TEXT))
			{
				state.format = ClipboardStateFormat::F_TEXT;
				state.status = ClipboardStateStatus::Changed;
			}
			else if (IsClipboardFormatAvailable(ClipboardStateFormat::F_BITMAP))
			{
				state.format = ClipboardStateFormat::F_BITMAP;
				state.status = ClipboardStateStatus::Changed;
			}
			else
			{
				state.format = ClipboardStateFormat::F_NONE;
				state.status = ClipboardStateStatus::NotAvaible;
			}
		}


		if (state.format != ClipboardStateFormat::F_NONE)
		{
			hData = GetClipboardData(state.format);
			if (hData != nullptr)
			{
				pszData = GlobalLock(hData);

				////////////////////////////////////////

				if (pszData != nullptr)
				{
					// == == == == == == == == == == ==

					uint32_t  length = GetClipboardDataLength(state.format, hData, pszData);
					XXH_hash_t lhash = XXH(hData, length, NULL);

					if (state.hash != lhash)
					{
						state.hash = lhash;

						delete[] state.lpdata;

						state.length = 0;

						switch (state.format)
						{
							case ClipboardStateFormat::F_TEXT:
							{
								state.length = length;
								state.lpdata = new char[state.length];

								std::copy(reinterpret_cast<char*>(pszData), reinterpret_cast<char*>(pszData) + state.length, reinterpret_cast<char*>(state.lpdata));
							};
							break;

							case ClipboardStateFormat::F_UNICODE:
							{
								state.length = length;
								state.lpdata = new wchar_t[state.length];

								std::copy(reinterpret_cast<wchar_t*>(pszData), reinterpret_cast<wchar_t*>(pszData) + state.length, reinterpret_cast<wchar_t*>(state.lpdata));
							};
							break;

							case ClipboardStateFormat::F_BITMAP:
							{
								DIB* dibinfo = reinterpret_cast<DIB*>(pszData);

								dibinfo->biCompression = 0;

								BMP bmp = {
									.header = {
										.type	= 0x4D42,
										.bfSize = length,
										.offset = sizeof(BMP)
									},
									.dib = *dibinfo
								};

								state.length = sizeof(BMP);
								state.lpdata = new BMP(bmp);

								wchar_t filename[256] = L"buffer";
								wchar_t filepath[256] = L"E:/";

								wchar_t filepath_png[256];

								unsigned int timeint = time(NULL);

								swprintf_s(filepath_png, L"%s%s-%d.png", filepath, filename, timeint);

								BYTE* buffer = new BYTE[length];

								BYTE* pBmpHeader = reinterpret_cast<BYTE*>(&bmp.header);
								BYTE* pDibinfo	 = reinterpret_cast<BYTE*>(dibinfo);

								long offset = sizeof(HEADER);
								std::copy(pBmpHeader, pBmpHeader + offset, buffer);
								std::copy(pDibinfo,   pDibinfo + (length - offset), buffer + offset);

								IStream* is = SHCreateMemStream(buffer, length);

								CLSID pngClsid;
								Gdiplus::Image image(is);

								GetEncoderClsid(L"image/png", &pngClsid);

								image.Save(filepath_png, &pngClsid);

								delete[] buffer;

								is->Release();
							};
							break;
						}


						state.status = ClipboardStateStatus::Changed;
					}
					else
					{
						state.status = ClipboardStateStatus::OK;
					}

					// == == == == == == == == == == ==
				}
				

				////////////////////////////////////////

				GlobalUnlock(hData);
			}
		}


		CloseClipboard();
	}
	else
	{
		state.status = ClipboardStateStatus::NotAvaible;
	}

	return state.status;
}


extern CMODE custom_mode_state;


void ClipboadWorker(SOCKET& tcp_connection)
{
	// Initialize GDI+.
	ULONG_PTR gdiplusToken;
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);


	ClipboardState2 state = {
		.status = ClipboardStateStatus::NotAvaible,
		.format = ClipboardStateFormat::F_NONE,
		.length = 0,
		.lpdata = nullptr,
	};

	while (true)
	{
		if (GetCBData(state) == ClipboardStateStatus::Changed)
		{
			if (state.format == ClipboardStateFormat::F_TEXT)
			{
				std::wcout << reinterpret_cast<char*>(state.lpdata) << std::endl;
			}
			
			if (state.format == ClipboardStateFormat::F_UNICODE)
			{
				std::wcout << reinterpret_cast<wchar_t*>(state.lpdata) << std::endl;
			}
		}

		Sleep(500);
	}

	Gdiplus::GdiplusShutdown(gdiplusToken);
}