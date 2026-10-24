#include "socket_utils.h"

#include <asm-generic/socket.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <system_error>
#include <cstring>
#include <netinet/tcp.h> 
#include <fcntl.h>


int create_socket_and_bind(uint16_t port)
{
    int fd = get_new_socket_id();
    set_socket_options(fd);
    set_nonblocking(fd);

    sockaddr_in server_addr = get_socket_addr(port);

    bind_socket_to_addr(fd, server_addr);

    return fd;
}

int get_new_socket_id() 
{
    /*
        AFINET is for IPv4. Use AF_INET6 for IPv6
        SOCK_STREAM is for TCP. Use SOCK_DGRAM for UDP
    */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("Error creating new socket socket()");
    }
    return fd;
}

void set_socket_options(int fd)
{
    /*
        combination of the 2nd & 3rd arguments specifies which option to set
        SO_REUSEADDR is set to 1 => server program binds to the same IP:port it was using after a restart
        SO_REUSEPORT is set to 1 => all sockets bind to same port. kernel takes care of load balancing. this is required for multithread
        TCP_NODELAY disables agle algoritm. forces tcp to transmit data packets immediately over the network
    */
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));  // uses same port
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));  // disable nagle
}

void set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);  // get the flags
    flags |= O_NONBLOCK;                // modify the flags
    fcntl(fd, F_SETFL, flags); 
}

sockaddr_in get_socket_addr(uint16_t port)
{
    /*
        sockaddr_in means "socket internet address"
        sockaddr_in -> IPv4 internet address
        sockaddr_in6 -> IPv6 internet address

        sockaddr can also be used as a generic interface (IPv4 + IPv6 support)

        sin_ prefix stands for socket internet (IPv4)

        sin6_ prefix is used for IPv6 i.e. socket internet IPv6
    */
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    return addr;
}

void bind_socket_to_addr(int fd, const sockaddr_in& addr)
{
    if (bind(fd, (const sockaddr *)&addr, sizeof(addr)) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "bind");
    }

    if (listen(fd, SOMAXCONN) == -1)
    {
        throw std::system_error(errno, std::generic_category(), "listen");
    }
}