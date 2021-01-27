#pragma once
#include <cstdint>
#include <string>
#include "helpers.h"

enum class DataType : uint8_t
{
	CF__TEXT		= 1,
	CF__BITMAP		= 2,
	CF__RAWFILE		= 3
};

enum class MessageType : uint8_t
{
	Announce = 1,
	Transfer = 2
};

const uint16_t MaxMessageSize = 1024U;

#pragma pack(push, 1)

struct MetaMessage
{
	DataType	 data;
	MessageType  message;

	uint16_t	 reserved_1;
	uint16_t	 length;
};

template<MessageType MT, DataType DT> struct Message
{
	uint8_t  data	 = static_cast<uint8_t>(DT);
	uint8_t  message = static_cast<uint8_t>(MT);

	uint16_t reserved_1;
	uint16_t length;
};

template<DataType DT> struct Message<MessageType::Announce, DT>
{
	uint8_t  data	 = static_cast<uint8_t>(DT);
	uint8_t  message = static_cast<uint8_t>(MessageType::Announce);

	uint16_t size;
	uint16_t length;
};

template<DataType DT> struct Message<MessageType::Transfer, DT>
{
	uint8_t  data	 = static_cast<uint8_t>(DT);
	uint8_t  message = static_cast<uint8_t>(MessageType::Transfer);

	uint16_t offset;
	uint16_t length;
};

#pragma pack(pop)

size_t WritePayload(char** pDest, std::string& message, uint16_t length = 65535, int offset = 0)
{
	DataType	dtype = DataType::CF__TEXT;
	MessageType mtype = offset > 0 ? MessageType::Transfer : MessageType::Announce;

	int		 tsize = static_cast<int>(message.size()); // total size of message
	uint16_t hsize = sizeof(Message<MessageType::Announce, DataType::CF__TEXT>); // // size of payload header part (1/2)
	uint16_t msize = MIN(static_cast<uint16_t>(std::max(tsize - offset, 0)), MaxMessageSize - hsize, length); // size of payload message part (2/2)
	uint16_t size  = msize + hsize; // total size of payload (header + message)

	if (msize > 1)
	{
		*pDest = new char[size];

		char* pPayload = *pDest + hsize;

		if (mtype == MessageType::Announce)
		{
			auto pAMsg		= reinterpret_cast<Message<MessageType::Announce, DataType::CF__TEXT>*>(*pDest);

			pAMsg->data		= static_cast<uint8_t>(dtype);
			pAMsg->message  = static_cast<uint8_t>(mtype);
			pAMsg->size		= static_cast<uint16_t>(tsize);
			pAMsg->length	= static_cast<uint16_t>(msize);
		}
		else if (mtype == MessageType::Transfer)
		{
			auto pAMsg		= reinterpret_cast<Message<MessageType::Transfer, DataType::CF__TEXT>*>(*pDest);

			pAMsg->data		= static_cast<uint8_t>(dtype);
			pAMsg->message	= static_cast<uint8_t>(mtype);
			pAMsg->offset	= offset;
			pAMsg->length	= msize;
		}

		std::copy(message.c_str() + offset, message.c_str() + msize + offset, pPayload);

		return size;
	}

	return 0;
}


std::string ReadPayload(char** ptr)
{
	DataType   DataType = reinterpret_cast<MetaMessage*>(*ptr)->data;
	MessageType MsgType = reinterpret_cast<MetaMessage*>(*ptr)->message;

	if (DataType == DataType::CF__TEXT && MsgType == MessageType::Announce)
	{
		Message<MessageType::Announce, DataType::CF__TEXT>* pMessage = reinterpret_cast<Message<MessageType::Announce, DataType::CF__TEXT>*>(*ptr);

		char* pPayload = *ptr + sizeof(Message<MessageType::Announce, DataType::CF__TEXT>);

		size_t size   = pMessage->size;
		size_t length = pMessage->length;

		return std::string(pPayload, pPayload + length);
	}

	return std::string();
}


int SendString(SOCKET& connection, std::string message, char** buffer)
{
	int offset = 0;
	int	sendcode = 0;

	size_t message_size = message.size();
	size_t payload_size = WritePayload(buffer, message, MaxMessageSize, offset);

	while (payload_size > 0 && sendcode >= 0)
	{
		sendcode = send(connection, *buffer, payload_size, NULL);

		Sleep(10);

		delete[] * buffer;

		offset += sendcode;

		payload_size = WritePayload(buffer, message, MaxMessageSize, offset);
	}

	return sendcode;
}


template<DataType DTYPE> 
size_t WritePayload(char* pSource, char** pDest, size_t length, int offset)
{
	MessageType mtype = offset > 0 ? MessageType::Transfer : MessageType::Announce;

	int		 tsize = static_cast<int>(length); // total size of data
	uint16_t hsize = sizeof(Message<MessageType::Announce, DTYPE>); // size of payload header part (1/2)
	uint16_t msize = MIN(static_cast<uint16_t>(std::max(tsize - offset, 0)), MaxMessageSize - hsize, length); // size of payload message part (2/2)
	uint16_t size  = msize + hsize; // total size of payload (header + message)

	if (msize > 1)
	{
		*pDest = new char[size];

		char* pPayload = *pDest + hsize;

		if (mtype == MessageType::Announce)
		{
			auto pAMsg = reinterpret_cast<Message<MessageType::Announce, DTYPE>*>(*pDest);

			pAMsg->data		= static_cast<uint8_t>(DTYPE);
			pAMsg->message	= static_cast<uint8_t>(mtype);
			pAMsg->size		= static_cast<uint16_t>(tsize);
			pAMsg->length	= static_cast<uint16_t>(msize);
		}
		else if (mtype == MessageType::Transfer)
		{
			auto pAMsg = reinterpret_cast<Message<MessageType::Transfer, DTYPE>*>(*pDest);

			pAMsg->data		= static_cast<uint8_t>(DTYPE);
			pAMsg->message	= static_cast<uint8_t>(mtype);
			pAMsg->offset	= static_cast<uint16_t>(offset);
			pAMsg->length	= static_cast<uint16_t>(msize);
		}

		std::copy(pSource + offset, pSource + msize + offset, pPayload);

		return size;
	}

	return 0;
}


template<DataType DTYPE>
int SendData(SOCKET& connection, char* pSource, char** pBuffer, size_t size)
{
	int offset = 0;
	int	sendcode = 0;

	size_t payload_size;

	do
	{
		payload_size = WritePayload<DTYPE>(pSource, pBuffer, MIN(size, size, MaxMessageSize), offset);

		sendcode = send(connection, *pBuffer, payload_size, NULL);

		delete[] *pBuffer;

		offset  += sendcode;

		Sleep(10);

	} while ((sendcode > SOCKET_ERROR) && (size + sizeof(Message<MessageType::Announce, DTYPE>) - payload_size) > 0);

	return sendcode;
}