#pragma once

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include <winsock2.h>
#include <Windows.h>
#include "message.h"
#include "message2.h"
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
		std::wcout << "recvfromTimeOutUDP error!" << std::endl;
	}

	return __select;
}

extern unsigned long inetaddr;

const int& toReference(int* pointer) {
	return *pointer;
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
						int addr_other_len = sizeof(sockaddr_in);
						sockaddr_in addr_other;

						BYTE* buffer;

						Message invite;
						Message accept = {
							.data = Message::Data::Control,
							.hash = XXHM(nullptr, 0, NULL),
							.type = Message::Type::BroadcastAccept,
							.total_length = 0,
							.offset = 0,
							.length = 0
						};

						ReadSingle(udp_connection, invite, buffer, reinterpret_cast<SOCKADDR*>(&addr_other), &addr_other_len);

						if(invite.data == Message::Data::Control && invite.type == Message::Type::BroadcastInvite && inetaddr != addr_other.sin_addr.s_addr)
						{
							std::wcout << "Got broadcast!" << std::endl;

							tcp_addr.sin_addr.s_addr = addr_other.sin_addr.s_addr;

							const void* ref = nullptr;

							bool sendcode = SendSingle(udp_connection, ref, accept, reinterpret_cast<SOCKADDR*>(&addr_other), sizeof(SOCKADDR));

							if (sendcode)
							{
								return CUSTOM_MODE_CLIENT;
							}
						}
					}
				}
			}
		} 
		else if (recce_mode == RECCE_SENDING_INVITATIONS)
		{
			timeout_duration = rand() * RAND_RANGE_SECONDS * RAND_MAX_DENOMINATOR;

			package_delay_sec  = 0.5;
			package_delay_msec = 1000.0 * package_delay_sec;

			Message invite = {
				.data = Message::Data::Control,
				.hash = XXHM(nullptr, 0, NULL),
				.type = Message::Type::BroadcastInvite,
				.total_length = 0,
				.offset = 0,
				.length = 0
			};

			while (std::abs(difftime(timeout_start, time(NULL))) < timeout_duration)
			{
				invite.type = Message::Type::BroadcastInvite;

				const void* ref = nullptr;

				SendSingle(udp_connection, ref, invite, reinterpret_cast<SOCKADDR*>(&udp_addr_broadcast), sizeof(udp_addr_broadcast));

				Sleep(static_cast<DWORD>(package_delay_msec));

				switch (recvfromTimeOutUDP(udp_connection, static_cast<long>(package_delay_sec), 0))
				{
					case -1: continue;
					case  0: continue;

					default:
					{
						BYTE* buffer;
						Message accept;

						ReadSingle(udp_connection, accept, buffer);

						if (accept.data == Message::Data::Control && accept.type == Message::Type::BroadcastAccept)
						{
							std::wcout << "Go to server mode!" << std::endl;

							return CUSTOM_MODE_SERVER;
						}
					}
				}
			}
		}
	}
}