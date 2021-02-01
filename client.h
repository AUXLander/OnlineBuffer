#pragma once

#include <iostream>
#include <Windows.h>
#include "message2.h"

extern std::function<bool(SOCKET& connection, const void** pSource, Message2::Data data, uint32_t length)> Send;


SOCKADDR_IN client_tcp_addr;

void client_start(SOCKET_PACK* unpack)
{
	SOCKET& tcp_connection = unpack->tcp_connection;
	SOCKADDR_IN& tcp_addr  = unpack->tcp_addr;

	client_tcp_addr = tcp_addr;

	Send = [](SOCKET& connection, const void** pSource, Message2::Data data, uint32_t length)
	{
		return SendSequence(connection, pSource, data, length, reinterpret_cast<SOCKADDR*>(&client_tcp_addr), sizeof(client_tcp_addr));
	};

	int status = connect(tcp_connection, reinterpret_cast<SOCKADDR*>(&tcp_addr), sizeof(tcp_addr));
	if (status != 0)
	{
		wprintf(L"Error: failed connect to server!");

		system("pause");
	}
	else
	{
		bool isNoError = true;
		Storage storage;
		
		while (true)
		{
			Storage::iterator iterator;
			
			isNoError = ReadSequence(tcp_connection, storage, iterator);

			if (isNoError)
			{
				StorageCell& cell = *iterator;

				if (cell.first.type == Message2::Data::TextUNICODE)
				{
					std::wcout << reinterpret_cast<wchar_t*>(cell.second) << std::endl;
				}
				else if (cell.first.type == Message2::Data::TextANSI)
				{
					std::wcout << reinterpret_cast<char*>(cell.second) << std::endl;
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
			else
			{
				break;
			}
		}

		std::wcout << "Close client side!" << std::endl;
	}
}