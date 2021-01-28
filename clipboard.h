#pragma once
#include <stdio.h>
#include <tchar.h>
#include <Windows.h>
#include <wchar.h>

#include "clipboard_structs.h"
#include "helpers.h"
#include "message.h"
#include "server.h"
#include "images.h"


int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	int index = -1;

	unsigned int numb = 0; // number of image encoders
	unsigned int size = 0; // size of the image encoder array in bytes

	Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

	Gdiplus::GetImageEncodersSize(&numb, &size);

	pImageCodecInfo = reinterpret_cast<Gdiplus::ImageCodecInfo*>(malloc(size));
	
	if (size == 0 || pImageCodecInfo == NULL)
	{
		goto GetEncoderClsidOut;
	}

	Gdiplus::GetImageEncoders(numb, size, pImageCodecInfo);

	for (index = 0; index < numb; index++)
	{
		if (wcscmp(pImageCodecInfo[index].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[index].Clsid;
			
			goto GetEncoderClsidOut;
		}
	}

	index = -1;

GetEncoderClsidOut:
	free(pImageCodecInfo);
	return index;
}


ClipboardState::Status GetCBData(ClipboardState& state)
{
	HANDLE hData;
	LPVOID pszData;

	if (OpenClipboard(nullptr))
	{
		if (!IsClipboardFormatAvailable(state.format))
		{
			if (IsClipboardFormatAvailable(ClipboardState::Format::F_UNICODE))
			{
				state.format = ClipboardState::Format::F_UNICODE;
				state.status = ClipboardState::Status::Changed;
			}
			else if(IsClipboardFormatAvailable(ClipboardState::Format::F_TEXT))
			{
				state.format = ClipboardState::Format::F_TEXT;
				state.status = ClipboardState::Status::Changed;
			}
			else if (IsClipboardFormatAvailable(ClipboardState::Format::F_BITMAP))
			{
				state.format = ClipboardState::Format::F_BITMAP;
				state.status = ClipboardState::Status::Changed;
			}
			else
			{
				state.format = ClipboardState::Format::F_NONE;
				state.status = ClipboardState::Status::NotAvaible;
			}
		}


		if (state.format != ClipboardState::Format::F_NONE)
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
							case ClipboardState::Format::F_TEXT:
							{
								state.length = length;
								state.lpdata = new char[state.length];

								std::copy(reinterpret_cast<char*>(pszData), reinterpret_cast<char*>(pszData) + state.length, reinterpret_cast<char*>(state.lpdata));
							};
							break;

							case ClipboardState::Format::F_UNICODE:
							{
								state.length = length;
								state.lpdata = new wchar_t[state.length];

								std::copy(reinterpret_cast<wchar_t*>(pszData), reinterpret_cast<wchar_t*>(pszData) + state.length, reinterpret_cast<wchar_t*>(state.lpdata));
							};
							break;

							case ClipboardState::Format::F_BITMAP:
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

								wchar_t filename[] = L"bufferimage";
								wchar_t filepath[] = L"E:/";

								wchar_t filepath_png[256];


								time_t timeint = time(NULL);

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


						state.status = ClipboardState::Status::Changed;
					}
					else
					{
						state.status = ClipboardState::Status::OK;
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
		state.status = ClipboardState::Status::NotAvaible;
	}

	return state.status;
}



void ClipboadWorker(SOCKET& tcp_connection)
{
	// Initialize GDI+.
	ULONG_PTR gdiplusToken;
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);


	ClipboardState state = {
		.status = ClipboardState::Status::NotAvaible,
		.format = ClipboardState::Format::F_NONE,
		.length = 0,
		.lpdata = nullptr,
	};

	while (true)
	{
		if (GetCBData(state) == ClipboardState::Status::Changed)
		{
			if (state.format == ClipboardState::Format::F_TEXT)
			{
				std::wcout << reinterpret_cast<char*>(state.lpdata) << std::endl;
			}
			
			if (state.format == ClipboardState::Format::F_UNICODE)
			{
				std::wcout << reinterpret_cast<wchar_t*>(state.lpdata) << std::endl;
			}
		}

		Sleep(500);
	}

	Gdiplus::GdiplusShutdown(gdiplusToken);
}