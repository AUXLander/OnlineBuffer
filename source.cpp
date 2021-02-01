#include <stdio.h>
#include <tchar.h>
#include <iostream>
#include <fcntl.h>
#include <io.h>
#include <functional>

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

#include <Windows.h>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

#undef min
#undef max

#include "helpers.h"
#include "error.h"
#include "server.h"
#include "client.h"
#include "reconnaissance.h"
#include "rbuffer.h"
#include "clipboard.h"
#include "xxhash.h"

CMODE custom_mode_state = CUSTOM_MODE_UNDEFINED;

unsigned long inetaddr;

std::function<bool(SOCKET& connection, const void** pSource, Message2::Data data, uint32_t length)> Send = [](SOCKET& connection, const void** pSource, Message2::Data data, uint32_t length)
{
	return false;
};

int main(int argc, char* argv[])
{
	//setlocale(LC_ALL, "Russian");

	int setmode = _setmode(_fileno(stdout), _O_U16TEXT);

	inetaddr = inet_addr("192.168.0.10");

	WSAData wsaData;
	int libstat = WSAStartup(MAKEWORD(2, 1), &wsaData);

	if (libstat != 0)
	{
		return RT_LIBRARY_LOAD_FAIL;
	}

	int reccemode;

	custom_mode_state = CUSTOM_MODE_SERVER;
	
	
	if (argc > 1)
	{
		if (strcmp(argv[1], "server") == 0)
		{
			custom_mode_state = CUSTOM_MODE_SERVER;
			reccemode = RECCE_MODE_SERVER;

			std::wcout << L"Custom mode: server" << std::endl;
		}
		else if (strcmp(argv[1], "client") == 0)
		{
			custom_mode_state = CUSTOM_MODE_CLIENT;
			reccemode = RECCE_MODE_CLIENT;

			std::wcout << L"Custom mode: client" << std::endl;
		}
	}

	if (argc > 2)
	{
		inetaddr = inet_addr(argv[2]);
	}

	SOCKADDR_IN tcp_addr = {
		.sin_family = AF_INET,
		.sin_port	= htons(static_cast<u_short>(1111))
	};

	SOCKADDR_IN udp_addr = {
		.sin_family = AF_INET,
		.sin_port	= htons(static_cast<u_short>(1112))
	};

	SOCKADDR_IN tcp_broadcast_addr = {
		.sin_family = NULL,
		.sin_port	= NULL
	};

	SOCKADDR_IN udp_broadcast_addr = {
		.sin_family = AF_INET,
		.sin_port	= htons(static_cast<u_short>(1112))
	};

	tcp_addr.sin_addr.s_addr = inetaddr; // inet_addr("192.168.0.10");
	udp_addr.sin_addr.s_addr = inetaddr; // inet_addr("192.168.0.10");

	tcp_broadcast_addr.sin_addr.s_addr = inet_addr("192.168.255.255");
	udp_broadcast_addr.sin_addr.s_addr = inet_addr("192.168.255.255");

	SOCKET tcp_connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	SOCKET udp_connection = socket(AF_INET, SOCK_DGRAM,  IPPROTO_UDP);

	const char trueflag = 1;
	if (setsockopt(udp_connection, SOL_SOCKET, SO_BROADCAST, &trueflag, sizeof trueflag) < 0)
	{
		return RT_SOCKETOPT_FAILURE;
	}

	HANDLE TCPThread;
	HANDLE UDPThread;

	SOCKET_PACK unpack = { tcp_connection, tcp_addr, tcp_broadcast_addr, NULL, udp_connection, udp_addr, udp_broadcast_addr, reccemode };

	int udp_bindcode = bind(udp_connection, reinterpret_cast<SOCKADDR*>(&udp_addr), sizeof(udp_addr));

	if (udp_bindcode == -1)
	{
		return RT_CANNOT_BIND_UDP_SOCKET;
	}

	custom_mode_state = reconnaissance(&unpack);

	if (custom_mode_state == CUSTOM_MODE_SERVER)
	{
		int tcp_bindcode = bind(tcp_connection, reinterpret_cast<SOCKADDR*>(&tcp_addr), sizeof(tcp_addr));

		if (tcp_bindcode == -1)
		{
			return RT_CANNOT_BIND_TCP_SOCKET;
		}

		TCPThread = CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(server_start), &unpack, NULL, NULL);

		//unpack.udp_flag = RECCE_MODE_SERVER;

		//UDPThread = CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(reconnaissance), &unpack, NULL, NULL);
	}
	else if (custom_mode_state == CUSTOM_MODE_CLIENT)
	{
		TCPThread = CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(client_start), &unpack, NULL, NULL);
	}


	ClipboadWorker(tcp_connection);


	system("pause");

	return RT_SUCCESS;
}