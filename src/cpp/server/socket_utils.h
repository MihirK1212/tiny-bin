#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <cstdint>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

int create_socket_and_bind(uint16_t port);

void set_nonblocking(int fd);

/*
Private helpers
*/

int get_new_socket_id();

void set_socket_options(int fd);

sockaddr_in get_socket_addr(uint16_t port);

void bind_socket_to_addr(int fd, const sockaddr_in& addr);

#endif