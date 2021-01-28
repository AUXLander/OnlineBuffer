#pragma once
#include <stdint.h>
#include <Windows.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <atlbase.h>

#include "clipboard_structs.h"


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
