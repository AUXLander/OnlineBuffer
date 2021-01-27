#pragma once

#include <iostream>
#include <Windows.h>
#include "message.h"


void client_start(SOCKET_PACK* unpack)
{
	SOCKET& tcp_connection = unpack->tcp_connection;
	SOCKADDR_IN& tcp_addr  = unpack->tcp_addr;

	int status = connect(tcp_connection, reinterpret_cast<SOCKADDR*>(&tcp_addr), sizeof(tcp_addr));
	if (status != 0)
	{
		wprintf(L"Error: failed connect to server!");

		system("pause");
	}
	else
	{
		int recvcode = 0;
		while (recvcode > SOCKET_ERROR)
		{
			char* msg = new char[MaxMessageSize];

			recvcode = recv(tcp_connection, msg, MaxMessageSize, NULL);

			//wprintf

			std::cout << ReadPayload(&msg) << std::endl;

			delete[] msg;
		}
	}
}