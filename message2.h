#pragma once
#include <cstdint>
#include <string>
#include <Windows.h>
#include <vector>

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
		Control = 0,
		TextANSI,
		TextUNICODE,
		Image,
		File
	};

	enum Type : unsigned char
	{
		BroadcastInvite = 0,
		BroadcastAccept,
		First,
		Next
	};

	Data		 data;
	XXH32_hash_t hash;
	Type		 type;
	uint32_t	 total_length;
	uint32_t	 offset;
	uint32_t	 length;
};

#pragma pack(pop)

typedef std::pair<Message2, BYTE*> StorageCell;
typedef std::vector<StorageCell> Storage;

XXH32_hash_t inline XXHM(const void* input, size_t size, XXH32_hash_t seed)
{
	return XXH32(input, size, seed);
}

static const uint32_t BUFFER_SIZE = 2048;
static const uint32_t sizeofheader = static_cast<uint32_t>(sizeof(Message2));


// return size of written bytes without header
uint32_t WritePayload2(void** pDest, const void** pSource, Message2& message)
{
	const uint32_t m_offset = std::min(message.offset, message.total_length);
	uint32_t __offset = m_offset;
	uint32_t w_offset = 0;

	if ((__offset < message.total_length || __offset == 0 && message.total_length == 0) && w_offset < Message2::MAX_SIZE)
	{
		uint32_t __portion = std::min(message.total_length - message.offset, Message2::MAX_SIZE - sizeofheader);
		uint32_t w_portion = __portion + sizeofheader;

		*pDest = new BYTE[w_portion];

		Message2* const pHeader = reinterpret_cast<Message2*>(*pDest);
		BYTE* const pBody = reinterpret_cast<BYTE*>(*pDest) + sizeofheader;

		pHeader->data = message.data;
		pHeader->hash = message.hash;
		pHeader->type = message.type;
		pHeader->total_length = message.total_length;
		pHeader->offset = m_offset;
		pHeader->length = __portion;

		if (pSource != nullptr)
		{
			std::copy(reinterpret_cast<const BYTE*>(*pSource) + message.offset, reinterpret_cast<const BYTE*>(*pSource) + message.offset + __portion, pBody);
		}

		w_offset += w_portion;
		__offset += __portion;
	}

	message.length = __offset - m_offset;
	message.offset += message.length;

	return __offset - m_offset;
}


bool ReadSingle(SOCKET& connection, Message2& message, BYTE** buffer, SOCKADDR* from, int* fromlen)
{
	char pBuffer[BUFFER_SIZE];

	int recvval = recvfrom(connection, pBuffer, BUFFER_SIZE, NULL, from, fromlen);
	if (recvval >= 0)
	{
		Message2* const pMsg  = reinterpret_cast<Message2*>(pBuffer);
			BYTE* const pData = reinterpret_cast<BYTE*>(pBuffer) + sizeofheader;

		message.data = pMsg->data;
		message.hash = pMsg->hash;
		message.type = pMsg->type;
		message.total_length = pMsg->total_length;
		message.offset = pMsg->offset;
		message.length = pMsg->length;

		*buffer = pData;
	}

	return false;
}

bool ReadSingle(SOCKET& connection, Message2& message, BYTE** buffer)
{
	int addr_other_len;
	SOCKADDR addr_other;
	return ReadSingle(connection, message, buffer, &addr_other, &addr_other_len);
}



bool ReadSequence(SOCKET& connection, Storage& storage, Storage::iterator& iterator, SOCKADDR* from, int* fromlen)
{
	char buffer[BUFFER_SIZE];

	int recvval = recvfrom(connection, buffer, BUFFER_SIZE, NULL, from, fromlen);
	if (recvval >= 0)
	{
		Message2* msg = reinterpret_cast<Message2*>(buffer);

		Storage::iterator itPair;

		switch (msg->type)
		{
			case Message2::Type::First:
			{
				storage.emplace_back(StorageCell({ *msg, new BYTE[msg->total_length] }));
			};
			__fallthrough;

			case Message2::Type::Next:
			{
				itPair = std::find_if(std::begin(storage), std::end(storage), [&msg](auto& pair){return pair.first.hash == msg->hash;});

				if (itPair < storage.end())
				{
					BYTE* pData = reinterpret_cast<BYTE*>(buffer) + sizeofheader;

					std::copy(pData, pData + msg->length, itPair->second + msg->offset);
				}
			};
			break;

			default: throw new std::string("Unknown type!");
		}

		if (msg->total_length <= (msg->offset + msg->length) && itPair < storage.end())
		{
			iterator = itPair;

			return true;
		}
	}

	return false;
}

bool ReadSequence(SOCKET& connection, Storage& storage, Storage::iterator& iterator)
{
	int addr_other_len;
	SOCKADDR addr_other;
	return ReadSequence(connection, storage, iterator, &addr_other, &addr_other_len);
}



inline bool SendSingle(SOCKET& connection, const void** pSource, Message2& message, SOCKADDR* to = nullptr, int tolen = 0)
{
	void* pBuffer;
	uint32_t paysize;
	int	sendcode = 0;
	bool delta = true;

	while (delta && (message.offset < message.total_length || message.offset == 0 && message.total_length == 0) && sendcode > SOCKET_ERROR)
	{
		paysize = WritePayload2(&pBuffer, pSource, message);

		if (to == nullptr)
		{
			sendcode = send(connection, reinterpret_cast<const char*>(pBuffer), paysize + sizeofheader, NULL);
		}
		else
		{
			sendcode = sendto(connection, reinterpret_cast<const char*>(pBuffer), paysize + sizeofheader, NULL, to, tolen);
		}
		

		if (sendcode < 0)
		{
			int tt = WSAGetLastError();

			std::wcout << "SendSingle: " << "WSAGetLastError is " << tt << std::endl;
		}

		message.type = Message2::Type::Next;

		delta = paysize > 0;

		delete[] pBuffer;

		Sleep(10);
	}

	return sendcode > SOCKET_ERROR;
}

bool SendSequence(SOCKET& connection, const void** pSource, Message2::Data data, uint32_t length, SOCKADDR* to = nullptr, int tolen = 0)
{
	Message2 message = {
		.data = data,
		.hash = XXHM(*pSource, length, NULL),
		.type = Message2::Type::First,
		.total_length = length,
		.offset = 0
	};

	return SendSingle(connection, pSource, message, to, tolen);
}

void SendString2(SOCKET& connection, std::string str)
{
	const char* str_c = str.c_str();
	SendSequence(connection, reinterpret_cast<const void**>(&str_c), Message2::Data::TextUNICODE, str.size() + 1);
}