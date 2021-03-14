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

	recce_mode = udp_fixed_recce_mode;

	long sec = static_cast<long>(2);

	if (recce_mode == RECCE_WAIT_FOR_INVITE)
	{
		while (true)
		{
			Sleep(2000);

			switch (recvfromTimeOutUDP(udp_connection, sec, 0))
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

					if (invite.data == Message::Data::Control && invite.type == Message::Type::BroadcastInvite && inetaddr != addr_other.sin_addr.s_addr)
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
		while (true)
		{
			Message invite = {
				.data = Message::Data::Control,
				.hash = XXHM(nullptr, 0, NULL),
				.type = Message::Type::BroadcastInvite,
				.total_length = 0,
				.offset = 0,
				.length = 0
			};

			invite.type = Message::Type::BroadcastInvite;

			const void* ref = nullptr;

			SendSingle(udp_connection, ref, invite, reinterpret_cast<SOCKADDR*>(&udp_addr_broadcast), sizeof(udp_addr_broadcast));

			Sleep(2000);


			switch (recvfromTimeOutUDP(udp_connection, sec, 0))
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