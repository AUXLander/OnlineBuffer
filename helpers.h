#pragma once

#define MIN(a,b,c) (((a < b) ? (a < c ? a : c) : (b < c ? b : c)))

struct SOCKET_PACK
{
	SOCKET& tcp_connection;
	SOCKADDR_IN& tcp_addr;
	SOCKADDR_IN& tcp_addr_broadcast;

	int tcp_flag;


	SOCKET& udp_connection;
	SOCKADDR_IN& udp_addr;
	SOCKADDR_IN& udp_addr_broadcast;

	int udp_flag;
};

typedef int CMODE;

#define CUSTOM_MODE_CLIENT 0
#define CUSTOM_MODE_SERVER 1
#define CUSTOM_MODE_UNDEFINED -1