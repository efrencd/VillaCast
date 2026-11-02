// SPDX-License-Identifier: GPL-2.0-or-later
#include "mdns-socket.hpp"
#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

bool MdnsSocket::setIpv4Ttl(qintptr descriptor, int ttl)
{
#ifdef Q_OS_WIN
	return setsockopt(static_cast<SOCKET>(descriptor), IPPROTO_IP, IP_TTL, reinterpret_cast<const char *>(&ttl),
			  sizeof(ttl)) == 0;
#else
	return setsockopt(static_cast<int>(descriptor), IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) == 0;
#endif
}
