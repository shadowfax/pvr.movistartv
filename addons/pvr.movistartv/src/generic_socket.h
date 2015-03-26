#pragma once

#if defined(TARGET_WINDOWS)
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <mswsock.h>

typedef unsigned long ipaddr_t;
typedef unsigned short port_t;
typedef int socklen_t;

#define u_int32 UINT32  // Unix uses u_int32

#else /* !TARGET_WINDOWS */
// ----------------------------------------
// common unix includes / defines
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

// Windows uses a special binary type 'SOCKET' to reference the socket
// Linux uses an integer to represent this
typedef int SOCKET;

#define closesocket close

// Redefine windows binary to Linux integer equivalent
#define INVALID_SOCKET - 1
#define SOCKET_ERROR - 1

#ifndef INADDR_NONE
#define INADDR_NONE((unsigned long)-1)
#endif // INADDR_NONE

#endif