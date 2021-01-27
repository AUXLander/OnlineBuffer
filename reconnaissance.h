#pragma once

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <winsock2.h>
#include <Windows.h>
#include "message.h"
#include <ctime>
#include <random>

#define RECCE_SENDING_INVITATIONS false
#define RECCE_WAIT_FOR_INVITE  true

#define RECCE_MODE_SERVER 0
#define RECCE_MODE_CLIENT 1
#define RECCE_MODE_SWTICHEBLE -1

#define RECCE_MESSAGE_BROADCAST_INVITE "Bradcast Invite\0"
#define RECCE_MESSAGE_BROADCAST_ACCEPT "Bradcast Accept\0"


typedef bool RECCE_CONNECTION_MODE;


extern bool ErrorFlag;

RECCE_CONNECTION_MODE recce_mode;

constexpr double RAND_RANGE_SECONDS = 10.0;
constexpr double RAND_MAX_DENOMINATOR = 1.0 / RAND_MAX;

int recvfromTimeOutUDP(SOCKET socket, long sec, long usec)
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(socket, &fds);

	timeval timeout = {
		.tv_sec  = sec,
		.tv_usec = usec
	};

	int __select = select(0, &fds, 0, 0, &timeout);

	// Return values:

	//  -1: error occurred
	//   0: timed out
	// > 0: data ready to be read

	if (__select == -1)
	{
		std::cout << "recvfromTimeOutUDP error!" << std::endl;
	}

	return __select;
}

CMODE reconnaissance(SOCKET_PACK* unpack)
{
	auto& [tcp_connection, tcp_addr, tcp_addr_broadcast, tcp_unused_flag, udp_connection, udp_addr, udp_addr_broadcast, udp_fixed_recce_mode] = *unpack;

	switch (udp_fixed_recce_mode)
	{
		case RECCE_MODE_SERVER:
		{
			recce_mode = RECCE_SENDING_INVITATIONS;
			break;
		};

		case RECCE_MODE_CLIENT:
		{
			__fallthrough;
		};
			 
		case RECCE_MODE_SWTICHEBLE:
		{
			recce_mode = RECCE_WAIT_FOR_INVITE;
			break;
		};
	}

	srand(static_cast<unsigned int>(time(NULL)));

	time_t timeout_start;
	double timeout_duration;
	double package_delay_sec;
	double package_delay_msec;

	while (true)
	{
		time(&timeout_start);

		if (recce_mode == RECCE_WAIT_FOR_INVITE)
		{
			timeout_duration = rand() * RAND_RANGE_SECONDS * RAND_MAX_DENOMINATOR;

			package_delay_sec  = 1.0;
			package_delay_msec = 1000.0 * package_delay_sec;

			while (std::abs(difftime(timeout_start, time(NULL))) < timeout_duration)
			{
				switch (recvfromTimeOutUDP(udp_connection, static_cast<long>(package_delay_sec), 0))
				{
					case -1: continue;
					case  0: continue;

					default:
					{
						const int bfSize = 20;
						char  cBuffer[bfSize];

						sockaddr_in addr_other;
						int addr_other_len = sizeof(addr_other);

						int length = recvfrom(udp_connection, cBuffer, bfSize, NULL, reinterpret_cast<SOCKADDR*>(&addr_other), &addr_other_len);
						if (length > 0 && strcmp(cBuffer, RECCE_MESSAGE_BROADCAST_INVITE) == 0)
						{
							std::cout << "Got broadcast!" << std::endl;

							tcp_addr.sin_addr.s_addr = addr_other.sin_addr.s_addr;

							int uncstcode = sendto(udp_connection, RECCE_MESSAGE_BROADCAST_ACCEPT, strlen(RECCE_MESSAGE_BROADCAST_ACCEPT) + 1, NULL, reinterpret_cast<SOCKADDR*>(&addr_other), addr_other_len);

							return CUSTOM_MODE_CLIENT;
						}
						/*else
						{
							std::cout << "Got message: " << buffer << std::endl;
						}*/
					}
				}
			}
		} 
		else if (recce_mode == RECCE_SENDING_INVITATIONS)
		{
			timeout_duration = rand()* RAND_RANGE_SECONDS* RAND_MAX_DENOMINATOR;

			package_delay_sec  = 0.5;
			package_delay_msec = 1000.0 * package_delay_sec;

			while (std::abs(difftime(timeout_start, time(NULL))) < timeout_duration)
			{
				int bcstcode = sendto(udp_connection, RECCE_MESSAGE_BROADCAST_INVITE, strlen(RECCE_MESSAGE_BROADCAST_INVITE) + 1, NULL, reinterpret_cast<SOCKADDR*>(&udp_addr_broadcast), sizeof(udp_addr_broadcast));

				if (bcstcode < 0)
				{
					std::cout << "Broadcast fault with error no: " << WSAGetLastError() << std::endl;
				}

				Sleep(static_cast<DWORD>(package_delay_msec));

				if (udp_fixed_recce_mode == RECCE_MODE_SWTICHEBLE)
				{
					switch (recvfromTimeOutUDP(udp_connection, static_cast<long>(package_delay_sec), 0))
					{
						case -1: continue;
						case  0: continue;

						default:
						{
							const int bfSize = 20;
							char  cBuffer[bfSize];

							int length = recv(udp_connection, cBuffer, bfSize, NULL);
							if (length > 0 && strcmp(cBuffer, RECCE_MESSAGE_BROADCAST_ACCEPT) == 0)
							{
								std::cout << "Go to server mode!" << std::endl;

								return CUSTOM_MODE_SERVER;
							}
							/*else
							{
								std::cout << "Got message: " << buffer << std::endl;
							}*/
						}
					}
				}
			}
		}

		if (udp_fixed_recce_mode == RECCE_MODE_SWTICHEBLE)
		{
			recce_mode = !recce_mode;
		}
	}
}