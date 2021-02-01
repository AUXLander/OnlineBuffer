#pragma once

#include <vector>
#include <iostream>
#include <Windows.h>
#include "message.h"
#include "rbuffer.h"
#include "message2.h"

#define THREAD_CLOSED WAIT_OBJECT_0
#define CONNECTION_CLOSED 0

struct client
{
	SOCKET connection = NULL;
	size_t index	  = (2 << 16) - 1;
	HANDLE thread 	  = nullptr;

	client() : connection(NULL) {}
	client(SOCKET __connection, size_t __index) : connection(__connection), index(__index) {}
};

static size_t lastindex = 0;
static const int maxconnlen = 6;
static rbuffer<client> cpull(maxconnlen);

extern std::function<bool(SOCKET& connection, const void** pSource, Message2::Data data, uint32_t length)> Send;


bool SendSequenceEachClient(SOCKET& connection, const void** pSource, Message2::Data data, uint32_t length)
{
	const auto pfirst = cpull.pFirst();
		  auto cnode  = pfirst;

	bool sendstatus = (pfirst != nullptr);

	if (pfirst != nullptr)
	{
		do
		{
			if (cnode->object->connection != connection)
			{
				sendstatus &= SendSequence(cnode->object->connection, pSource, data, length);
			}

			cnode = cnode->next;
		}
		while (cnode != pfirst);
	}

	// TO DO list of failed transactions 

	return sendstatus;
}


static DWORD receiver(client *object)
{
	int size;
	bool running = true;
	//char* msg = new char[MaxMessageSize];
	
	std::wcout << "Thread #" << object->index << " has been initialized." << std::endl;

	Storage storage;

	while (true)
	{
		Storage::iterator iterator;
		ReadSequence(object->connection, storage, iterator);

		StorageCell& cell = *iterator;

		SendSequenceEachClient(object->connection, const_cast<const void**>(reinterpret_cast<void**>(&cell.second)), cell.first.data, cell.first.total_length);
		
		if (cell.first.type == Message2::Data::TextUNICODE)
		{
			wchar_t* unicode_str = reinterpret_cast<wchar_t*>(cell.second);

			std::wcout << unicode_str << std::endl;
		}
		else if (cell.first.type == Message2::Data::TextANSI)
		{
			std::wcout << reinterpret_cast<char *>(cell.second) << std::endl;
		}
		else if (cell.first.type == Message2::Data::Image)
		{
			std::wcout << "Image!" << std::endl;
		}
		else if (cell.first.type == Message2::Data::File)
		{
			std::wcout << "File!" << std::endl;
		}
	}

	std::wcout << "Thread #" << object->index << " closed connection with code: " << WSAGetLastError() << std::endl;

	return 0;
}


void server_start(SOCKET_PACK* unpack)
{
	SOCKET& tcp_connection = unpack->tcp_connection;
	SOCKADDR_IN& tcp_addr  = unpack->tcp_addr;

	Send = [](SOCKET& connection, const void** pSource, Message2::Data data, uint32_t length)
	{
		return SendSequenceEachClient(connection, pSource, data, length);
	};

	listen(tcp_connection, maxconnlen);

	std::wcout << "Waiting for new connections..." << std::endl;

	int   addrlen  = sizeof(tcp_addr);
	char* pControlMsgBuffer = nullptr;

	while (true)
	{
		const auto pfirst = cpull.pFirst();
		const auto size	  = cpull.size();
			  auto pnode  = pfirst;
		
		for (size_t i = 0; i < size; i++)
		{
			if (pnode == nullptr)
			{
				break;
			}
			else
			{
				DWORD ThreadStatus = WaitForSingleObject(pnode->object->thread, 0);

				if (ThreadStatus == THREAD_CLOSED || pnode->object->connection == CONNECTION_CLOSED)
				{
					std::wcout << "Thread #" << pnode->object->index << " free." << std::endl;

					closesocket(pnode->object->connection);

					pnode = pnode->back;

					cpull.erase(pnode->next);
				}
				else
				{
					pnode = pnode->next;
				}
			}

			if (pnode == pfirst)
			{
				break;
			}
		}

		while (cpull.size() < maxconnlen)
		{
			SOCKET n_connection = accept(tcp_connection, reinterpret_cast<SOCKADDR*>(&tcp_addr), &addrlen);
			auto cnode = cpull.emplace(n_connection, lastindex);

			lastindex++;

			if (cnode == nullptr)
			{
				std::string text = "Error: failed to allocate new connection!";
				std::wcout << text.c_str() << std::endl;

				SendString2(n_connection, text);

				closesocket(n_connection);
			}
			else if (cnode->object->connection == CONNECTION_CLOSED)
			{
				std::string  text = "Error: failed connection!";
				std::wcout << text.c_str() << std::endl;
				
				cpull.erase(cnode);
			}
			else if (cnode->object->connection == INVALID_SOCKET)
			{
				std::string  text = "Error: invalid socket! WSA last error: " + WSAGetLastError();
				std::wcout << text.c_str() << std::endl;

				cpull.erase(cnode);
			}
			else
			{
				std::string  text = "Successful connection!";
				std::wcout << text.c_str() << std::endl;

				SendString2(cnode->object->connection, text);

				cnode->object->thread = CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(receiver), cnode->object, NULL, NULL);
			}
		}

		Sleep(500);
	}
}