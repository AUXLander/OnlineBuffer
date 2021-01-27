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

//#pragma pack(pop) 


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
					
					if (state.format == ClipboardStateFormat::F_TEXT)
					{
						unsigned int length = std::strlen(reinterpret_cast<const char*>(hData)) + 1;

						XXH_hash_t lhash = XXH(hData, sizeof(char) * length, NULL);

						if (state.hash != lhash)
						{
							delete[] state.lpdata;

							state.hash = lhash;
							state.length = length;
							state.lpdata = new char[state.length];

							std::copy(reinterpret_cast<char*>(pszData), reinterpret_cast<char*>(pszData) + state.length, reinterpret_cast<char*>(state.lpdata));

							state.status = ClipboardStateStatus::Changed;
						}
						else
						{
							state.status = ClipboardStateStatus::OK;
						}
					}
					else if (state.format == ClipboardStateFormat::F_UNICODE)
					{
						unsigned int length = std::wcslen(reinterpret_cast<const wchar_t*>(hData)) + 1;

						XXH_hash_t lhash = XXH(hData, sizeof(wchar_t) * length, NULL);

						if (state.hash != lhash)
						{
							delete[] state.lpdata;

							state.hash = lhash;
							state.length = length;
							state.lpdata = new wchar_t[state.length];

							std::copy(reinterpret_cast<wchar_t*>(pszData), reinterpret_cast<wchar_t*>(pszData) + state.length, reinterpret_cast<wchar_t*>(state.lpdata));

							state.status = ClipboardStateStatus::Changed;
						}
						else
						{
							state.status = ClipboardStateStatus::OK;
						}
					}
					else if (state.format == ClipboardStateFormat::F_BITMAP)
					{
						DIB* dibinfo = reinterpret_cast<DIB*>(pszData);

						BMP bmp = {
							.header = {
								.type	= 0x4D42,
								.bfSize = sizeof(BMP) + dibinfo->biSizeImage,
								.offset = sizeof(BMP)
							},
							.dib = *dibinfo
						};

						XXH_hash_t lhash = XXH(hData, bmp.header.bfSize, NULL);

						if (state.hash != lhash)
						{
							delete[] state.lpdata;

							state.hash = lhash;
							state.length = sizeof(BMP);
							state.lpdata = new BMP(bmp);

							/*
							std::wcout << "Type: " << std::hex << bmp.header.type << "\n";
							std::wcout << "bfSize: " << std::dec << bmp.header.bfSize << "\n";
							std::wcout << "Reserved: " << bmp.header.reserved << "\n";
							std::wcout << "Offset: " << bmp.header.offset << "\n";
							std::wcout << "biSize: " << bmp.dib.biSize << "\n";
							std::wcout << "Width: " << bmp.dib.biWidth << "\n";
							std::wcout << "Height: " << bmp.dib.biHeight << "\n";
							std::wcout << "Planes: " << bmp.dib.biPlanes << "\n";
							std::wcout << "Bits: " << bmp.dib.biBitCount << "\n";
							std::wcout << "Compression: " << bmp.dib.biCompression << "\n";
							std::wcout << "Size: " << bmp.dib.biSizeImage << "\n";
							std::wcout << "X-res: " << bmp.dib.biXPelsPerMeter << "\n";
							std::wcout << "Y-res: " << bmp.dib.biYPelsPerMeter << "\n";
							std::wcout << "ClrUsed: " << bmp.dib.biClrUsed << "\n";
							std::wcout << "ClrImportant: " << bmp.dib.biClrImportant << "\n";
							*/
							
							wchar_t filename[256] = L"buffer";
							wchar_t filepath[256] = L"E:/";
							
							wchar_t filepath_bmp[256];
							wchar_t filepath_png[256];

							unsigned int timeint = time(NULL);

							swprintf_s(filepath_bmp, L"%s%s-%d.bmp", filepath, filename, timeint);
							swprintf_s(filepath_png, L"%s%s-%d.png", filepath, filename, timeint);


							std::ofstream file(filepath_bmp, std::ios::out | std::ios::binary);
							

							if (file)
							{
								bmp.dib.biCompression = 0;

								writebin(file, &bmp.header.type); 
								writebin(file, &bmp.header.bfSize);
								writebin(file, &bmp.header.reserved);
								writebin(file, &bmp.header.offset);
								writebin(file, &bmp.dib.biSize); 
								writebin(file, &bmp.dib.biWidth);
								writebin(file, &bmp.dib.biHeight); 
								writebin(file, &bmp.dib.biPlanes); 
								writebin(file, &bmp.dib.biBitCount); 
								writebin(file, &bmp.dib.biCompression);
								writebin(file, &bmp.dib.biSizeImage); 
								writebin(file, &bmp.dib.biXPelsPerMeter);
								writebin(file, &bmp.dib.biYPelsPerMeter);
								writebin(file, &bmp.dib.biClrUsed);
								writebin(file, &bmp.dib.biClrImportant);
								writebin(file, dibinfo + 1, bmp.dib.biSizeImage);

								

								file.close();
							}

							std::ifstream in(filepath_bmp, std::ifstream::ate | std::ifstream::binary);
							
							DWORD siz = in.tellg();
							BYTE* buf = new BYTE[siz];

							in.seekg(0, std::ios::beg);
							in.read((char*)buf, siz);

							in.close();

							IStream* is = SHCreateMemStream(buf, siz);

							CLSID pngClsid;
							Gdiplus::Image image(is);

							GetEncoderClsid(L"image/png", &pngClsid);

							image.Save(filepath_png, &pngClsid);

							delete[] buf;

							is->Release();

							try
							{
								std::filesystem::remove(filepath_bmp);
							}
							catch (const std::filesystem::filesystem_error& err) {
								std::string sss(err.what());
								std::wcout << "filesystem error: " << sss.c_str() << '\n';
							}


							state.status = ClipboardStateStatus::Changed;
						}
						else
						{
							state.status = ClipboardStateStatus::OK;
						}
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


unsigned int GetClipboardText(ClipboardState& state)
{
	HANDLE hData;
	unsigned char* pszData;

	if (!OpenClipboard(nullptr))
	{
		return state.code = 0x2;
	}

	if (!IsClipboardFormatAvailable(state.type))
	{
		state.type = IsClipboardFormatAvailable(CF_BITMAP) ? CF_BITMAP : state.type;
		state.type = IsClipboardFormatAvailable(CF_TEXT) ? CF_TEXT : state.type;

		state.code = 0x1;
	}
	else
	{
		state.code = 0x0;
	}

	hData = GetClipboardData(state.type);
	if (hData == nullptr)
	{
		return state.code = 0x3;
	}

	pszData = static_cast<unsigned char*>(GlobalLock(hData));
	if (pszData == nullptr)
	{
		return state.code = 0x4;
	}

	if (state.type == CF_TEXT)
	{
		bool cverify = true;
		unsigned int index = 0;
		unsigned int index_l = 0;
		unsigned int index_r = 0;

		// имменно такие условия в цикле
		while (cverify && index < 64 && index == index_l && index_l == index_r)
		{
			cverify = static_cast<bool>(state.data[index_l] == pszData[index_r]);

			index_l += static_cast<unsigned int>(state.data[index_l] != '\0');
			index_r += static_cast<unsigned int>(pszData[index_r] != '\0');

			index++;
		}

		if (index_r != index_l || !cverify)
		{
			state.code = 0x1;
		}
	}
	else if (state.type == CF_BITMAP)
	{

	}

	std::copy(pszData, pszData + 64, state.data);

	GlobalUnlock(hData);
	CloseClipboard();

	return state.code;
}


void ClipboadWorker_old(SOCKET& tcp_connection)
{
	ClipboardState state = { 0, 0, {} };

	std::function<int()> sendstring = []() { return 0; };

	if (custom_mode_state == CUSTOM_MODE_SERVER)
	{
		sendstring = [&state]()
		{
			return SendEachClient(reinterpret_cast<char*>(state.data));
		};
	}
	else if (custom_mode_state == CUSTOM_MODE_CLIENT)
	{
		sendstring = [&state, &tcp_connection]()
		{
			char* pBuffer = nullptr;
			//char* data = new char[65];

			//const char* tdata = "GOgLESS";

			//std::copy(tdata, tdata + strlen(tdata) + 1, data);
			

			//return SendData<DataType::CF__TEXT>(tcp_connection, reinterpret_cast<char**>(&data), &pBuffer, 65);
			return SendData<DataType::CF__TEXT>(tcp_connection, reinterpret_cast<char*>(state.data), &pBuffer, strlen(reinterpret_cast<char*>(state.data)) + 1);
		};
	}


	while (state.code == 0x3 || state.code < 0x2)
	{
		if (GetClipboardText(state) == 0x1)
		{
			sendstring();

			std::cout << reinterpret_cast<char*>(state.data) << std::endl;
		}

		Sleep(500);
	}
}

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

		if (state.format == ClipboardStateFormat::F_BITMAP)
		{
			//Sleep(1000);
			Sleep(500);
		}
		else
		{
			Sleep(500);
		}
	}

	Gdiplus::GdiplusShutdown(gdiplusToken);
}