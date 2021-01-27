#pragma once

#define MIN(a,b,c) (((a < b) ? (a < c ? a : c) : (b < c ? b : c)))


//template<class T> T min(T a, T b) {
//	return a > b ? b : a;
//}

//template<class T> T max(T a, T b) {
//	return a > b ? a : b;
//}

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