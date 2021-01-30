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

static const int maxconnlen = 6;
static rbuffer<client> cpull(maxconnlen); 
static size_t lastindex = 0;


int SendEachClient(char *pSource)
{
	const auto pfirst = cpull.pFirst();

	char* pBuffer = nullptr;

	auto cnode = pfirst;
	auto sendcode = 0;

	if (pfirst != nullptr)
	{
		do
		{
			sendcode = SendData<DataType::CF__TEXT>(cnode->object->connection, pSource, &pBuffer, strlen(pSource) + 1);

			cnode = cnode->next;

		} while (cnode != pfirst && sendcode > 0);
	}

	return sendcode;
}


static DWORD receiver(client *object)
{
	int size;
	bool running = true;
	char* msg = new char[MaxMessageSize];
	
	std::cout << "Thread #" << object->index << " has been initialized." << std::endl;

	while (running)
	{
		size = recv(object->connection, msg, MaxMessageSize, NULL);

		running = size > 0;

		if (size > 0)
		{
			std::string ss = ReadPayload(&msg);

			char* mss = new char [ss.size()];

			std::cout << "Resend to other...   Sendcode is " << SendEachClient(mss) << std::endl;
			std::cout << "Msg from #" << object->index << ": " << mss << std::endl;
		}
	}

	delete[] msg;

	std::cout << "Thread #" << object->index << " closed connection with code: " << WSAGetLastError() << std::endl;

	return 0;
}


void server_start(SOCKET_PACK* unpack)
{
	SOCKET& tcp_connection = unpack->tcp_connection;
	SOCKADDR_IN& tcp_addr  = unpack->tcp_addr;

	listen(tcp_connection, maxconnlen);

	std::wcout << "Waiting for new connections..." << std::endl;

	int   addrlen  = sizeof(tcp_addr);
	char* pControlMsgBuffer = nullptr;

	while (true)
	{
		const auto pfirst = cpull.pFirst();
		const auto size	  = cpull.size();
		auto pnode		  = pfirst;
		
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
					std::cout << "Thread #" << pnode->object->index << " free." << std::endl;

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
			auto cnode = cpull.emplace(n_connection, lastindex++);

			if (cnode == nullptr)
			{
				std::string  text = "Error: failed to allocate new connection!";
				std::cout << text << std::endl;

				SendString(n_connection, text, &pControlMsgBuffer);

				closesocket(n_connection);
			}
			else if (cnode->object->connection == CONNECTION_CLOSED)
			{
				std::string  text = "Error: failed connection!";
				std::cout << text << std::endl;
				
				cpull.erase(cnode);
			}
			else if (cnode->object->connection == INVALID_SOCKET)
			{
				std::string  text = "Error: invalid socket! WSA last error: " + WSAGetLastError();
				std::cout << text << std::endl;

				cpull.erase(cnode);
			}
			else
			{
				std::string  text = "Successful connection!";
				std::cout << text << std::endl;
				
				SendString(cnode->object->connection, text, &pControlMsgBuffer);

				cnode->object->thread = CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(receiver), cnode->object, NULL, NULL);
			}
		}

		Sleep(500);
	}
}