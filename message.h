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

struct Message
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

struct StorageMessage : public Message
{
	time_t timestamp;

	StorageMessage(Message* pObject, time_t time) : Message(*pObject), timestamp(time) {}
};

#pragma pack(pop)

typedef std::pair<StorageMessage, const BYTE*> StorageCell;
typedef std::vector<StorageCell> Storage;

XXH32_hash_t __forceinline XXHM(const void* input, size_t size, XXH32_hash_t seed)
{
	return XXH32(input, size, seed);
}

static const uint32_t BUFFER_SIZE = 2048;
static const uint32_t sizeofheader = static_cast<uint32_t>(sizeof(Message));
static const time_t   timeout_secs = 30;


// return size of written bytes without header
uint32_t WritePayload(void*& pDest, const void*& pSource, Message& message)
{
	const uint32_t m_offset = std::min(message.offset, message.total_length);

	uint32_t __offset = m_offset;
	uint32_t w_offset = 0;

	const bool isSeekEnd = (__offset < message.total_length);
	const bool isOverflowed = (message.length + sizeofheader > Message::MAX_SIZE);

	const bool isBroadcastException = (__offset == 0 && message.total_length == 0);

	if (isBroadcastException || isSeekEnd && !isOverflowed)
	{
		uint32_t __portion = std::min(message.total_length - message.offset, Message::MAX_SIZE - sizeofheader);
		uint32_t w_portion = __portion + sizeofheader;

		pDest = new BYTE[w_portion];

		Message* const pDstHeader = reinterpret_cast<Message*>(pDest);
		BYTE*	 const pDstBody	  = reinterpret_cast<BYTE*>(pDest) + sizeofheader;

		std::copy(&message, &message + 1, pDstHeader);

		pDstHeader->offset = m_offset;
		pDstHeader->length = __portion;

		if (pSource != nullptr && message.total_length > 0 && message.length > 0)
		{
			const BYTE* pSrcBody = reinterpret_cast<const BYTE*>(pSource) + message.offset;
			
			std::copy(pSrcBody, pSrcBody + __portion, pDstBody);
		}

		w_offset += w_portion;
		__offset += __portion;
	}

	message.length = __offset - m_offset;
	message.offset += message.length;	

	return __offset - m_offset;
}


bool ReadSingle(SOCKET& connection, Message& message, BYTE*& buffer, SOCKADDR* from, int* fromlen)
{
	char pBuffer[BUFFER_SIZE];

	int recvval = recvfrom(connection, pBuffer, BUFFER_SIZE, NULL, from, fromlen);
	if (recvval >= static_cast<int>(sizeofheader))
	{
		Message* const pMsg  = reinterpret_cast<Message*>(pBuffer);

		std::copy(pMsg, pMsg + 1, &message);

		buffer = reinterpret_cast<BYTE*>(pBuffer) + sizeofheader;
	}

	return recvval >= static_cast<int>(sizeofheader);
}

bool ReadSingle(SOCKET& connection, Message& message, BYTE*& buffer)
{
	int addr_other_len;
	SOCKADDR addr_other;
	return ReadSingle(connection, message, buffer, &addr_other, &addr_other_len);
}



bool ReadSequence(SOCKET& connection, Storage& storage, Storage::iterator& iterator, SOCKADDR* from, int* fromlen)
{
	char buffer[BUFFER_SIZE];

	int recvval = recvfrom(connection, buffer, BUFFER_SIZE, NULL, from, fromlen);
	if (recvval >= sizeofheader)
	{
		Message* message = reinterpret_cast<Message*>(buffer);

		Storage::iterator pivot;

		switch (message->type)
		{
			case Message::Type::First:
			{
				storage.emplace_back(StorageCell({ {message, time(NULL)}, new BYTE[message->total_length] }));
			};
			__fallthrough;

			case Message::Type::Next:
			{
				time_t __time = time(NULL);
				pivot = std::begin(storage);

				auto __filter = [&](auto& pair)
				{
					if (pair.first.hash != message->hash)
					{
						const bool verifyObject = (pivot->first.hash == pair.first.hash);
						const bool isTimeouted  = (pair.first.timestamp - __time) > timeout_secs;

						if (isTimeouted && verifyObject)
						{
							storage.erase(pivot);
						}
						else
						{
							++pivot;
						}

						return false;
					}

					return true;
				};

				pivot = std::find_if(std::begin(storage), std::end(storage), __filter);

				if (pivot < storage.end())
				{
					BYTE* const pSource = reinterpret_cast<BYTE*>(buffer) + sizeofheader;
					BYTE* const pDest	= const_cast<BYTE*>(pivot->second + message->offset);

					std::copy(pSource, pSource + message->length, pDest);
				}
			};
			break;

			default: throw new std::string("Unknown type!");
		}

		if (message->total_length <= (message->offset + message->length))
		{
			if (pivot < storage.end())
			{
				const XXH32_hash_t lhash = XXHM(pivot->second, pivot->first.total_length, NULL);

				if (pivot->first.hash == lhash)
				{
					iterator = pivot;

					return true;
				}
				else
				{
					std::wcout << "Bad hash!" << std::endl;

					storage.erase(pivot);
				}
			}
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



inline bool SendSingle(SOCKET& connection, const void*& pSource, Message& message, SOCKADDR* to = nullptr, int tolen = 0)
{
	void* pBuffer;
	int	sendcode = 0;
	uint32_t paysize;

	const bool isBroadcastException = (message.offset == 0 && message.total_length == 0);
	auto isOverflowed = [&message]() -> bool { return message.offset > message.total_length; };

	if (!isOverflowed()) do
	{
		paysize = WritePayload(pBuffer, pSource, message);

		if (paysize > 0 || isBroadcastException)
		{
			if (to == nullptr)
			{
				sendcode = send(connection, reinterpret_cast<const char*>(pBuffer), paysize + sizeofheader, NULL);
			}
			else
			{
				sendcode = sendto(connection, reinterpret_cast<const char*>(pBuffer), paysize + sizeofheader, NULL, to, tolen);
			}

			if (sendcode == SOCKET_ERROR)
			{
				std::wcout << "SendSingle: WSAGetLastError is " << WSAGetLastError() << std::endl;
			}
			else
			{
				message.type = Message::Type::Next;
			}
		}

		delete[] pBuffer;

		Sleep(10);
	}
	while (paysize > 0 && !isOverflowed() && sendcode > SOCKET_ERROR);

	return sendcode > SOCKET_ERROR;
}

bool SendSequence(SOCKET& connection, const void*& pSource, Message::Data data, uint32_t length, SOCKADDR* to = nullptr, int tolen = 0)
{
	Message message = {
		.data = data,
		.hash = XXHM(pSource, length, NULL),
		.type = Message::Type::First,
		.total_length = length,
		.offset = 0
	};

	return SendSingle(connection, pSource, message, to, tolen);
}

void SendString(SOCKET& connection, std::string str)
{
	const char* str_c = str.c_str();
	SendSequence(connection, reinterpret_cast<const void*&>(str_c), Message::Data::TextUNICODE, str.size() + 1);
}