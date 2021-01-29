#pragma once
#include <cstdint>

#include <Windows.h>

#undef min
#undef max

#include <algorithm>

#include "xxhash.h"

#pragma warning( disable : 26812 )

#pragma pack(push, 1)

struct Message2
{
	const static uint32_t MAX_SIZE = 1024;

	enum Data : unsigned char
	{
		TextANSI	= 0,
		TextUNICODE = 1,
		Image		= 2,
		File		= 3
	};

	enum Type : unsigned char
	{
		First = 0,
		Next  = 1
	};

	Data		 data;
	XXH32_hash_t hash;
	Type		 type;
	uint32_t	 total_length;
	uint32_t	 offset;
	uint32_t	 length;
};

#pragma pack(pop)

// return size of written bytes without header
uint32_t WritePayload2(void **pDest, void** pSource, Message2 &message)
{
	const uint32_t m_offset = std::min(message.offset, message.total_length);
		  uint32_t __offset = m_offset;
		  uint32_t w_offset = 0;

	const uint32_t sizeofheader = static_cast<uint32_t>(sizeof(Message2));

	if (__offset < message.total_length && w_offset < Message2::MAX_SIZE)
	{
		uint32_t __portion = std::min(message.total_length - message.offset, Message2::MAX_SIZE - sizeofheader);
		uint32_t w_portion = __portion + sizeofheader;
		
		*pDest = new BYTE[w_portion];

		Message2* const pHeader = reinterpret_cast<Message2*>(*pDest);
		BYTE*	  const pBody	= reinterpret_cast<BYTE*>(*pDest) + sizeofheader;

		pHeader->data		  = message.data;
		pHeader->hash		  = message.hash;
		pHeader->type		  = message.type;
		pHeader->total_length = message.total_length;
		pHeader->offset		  = m_offset;
		pHeader->length		  = __portion;

		std::copy(reinterpret_cast<BYTE*>(*pSource) + message.offset, reinterpret_cast<BYTE*>(*pSource) + message.offset + __portion, pBody);

		w_offset += w_portion;
		__offset += __portion;
	}

	message.offset += __offset - m_offset;

	return __offset - m_offset;
}



bool SendSequence(SOCKET& connection, void** pSource, Message2::Data data, uint32_t length)
{
	Message2 message = {
		.data		  = data,
		.hash		  = XXH32(*pSource, length, NULL),
		.type		  = Message2::Type::First,
		.total_length = length,
		.offset		  = 0
	};

	void* pBuffer;
	uint32_t paysize;
	int	sendcode = 0;
	bool delta = true;

	while (delta && message.offset < message.total_length && sendcode > SOCKET_ERROR)
	{
		paysize = WritePayload2(&pBuffer, pSource, message);

		sendcode = send(connection, reinterpret_cast<const char*>(pBuffer), paysize, NULL);
		
		message.type = Message2::Type::Next;

		delta = paysize > 0;

		delete[] pBuffer;

		Sleep(10);
	}

	return sendcode > SOCKET_ERROR;
}