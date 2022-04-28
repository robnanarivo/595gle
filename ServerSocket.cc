/*
 * Copyright ©2022 Travis McGaha.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Pennsylvania
 * CIT 595 for use solely during Spring Semester 2022 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <cstdio>       // for snprintf()
#include <unistd.h>      // for close(), fcntl()
#include <sys/types.h>   // for socket(), getaddrinfo(), etc.
#include <sys/socket.h>  // for socket(), getaddrinfo(), etc.
#include <arpa/inet.h>   // for inet_ntop()
#include <netdb.h>       // for getaddrinfo()
#include <errno.h>       // for errno, used by strerror()
#include <cstring>      // for memset, strerror()
#include <iostream>      // for std::cerr, etc.

#include "./ServerSocket.h"

namespace searchserver {

ServerSocket::ServerSocket(uint16_t port) {
  port_ = port;
  listen_sock_fd_ = -1;
}

ServerSocket::~ServerSocket() {
  // Close the listening socket if it's not zero.  The rest of this
  // class will make sure to zero out the socket if it is closed
  // elsewhere.
  if (listen_sock_fd_ != -1)
    close(listen_sock_fd_);
  listen_sock_fd_ = -1;
}

bool ServerSocket::bind_and_listen(int ai_family, int *listen_fd) {
  // Use "getaddrinfo," "socket," "bind," and "listen" to
  // create a listening socket on port port_.  Return the
  // listening socket through the output parameter "listen_fd"
  // and set the ServerSocket data member "listen_sock_fd_"

  // populate hints
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = ai_family;      // ai_family from input
  hints.ai_socktype = SOCK_STREAM;  // stream
  hints.ai_flags = AI_PASSIVE;      // use wildcard "in6addr_any" address
  hints.ai_flags |= AI_V4MAPPED;    // use v4-mapped v6 if no v6 found
  hints.ai_protocol = IPPROTO_TCP;  // tcp protocol
  hints.ai_canonname = nullptr;
  hints.ai_addr = nullptr;
  hints.ai_next = nullptr;

  // get address info
  char portStr[6];
  sprintf(portStr, "%u", port_);
  struct addrinfo *result;
  int res = getaddrinfo(nullptr, portStr, &hints, &result);
  if (res != 0) {
    std::cerr << "getaddrinfo() failed: ";
    std::cerr << gai_strerror(res) << std::endl;
    return -1;
  }

  // create socket and bind
  for (struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
    listen_sock_fd_ = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (listen_sock_fd_ == -1) {
      std::cerr << "socket() failed: " << strerror(errno) << std::endl;
      listen_sock_fd_ = -1;
      continue;
    }

    // configure the socket; make port available immediately after exit
    int optval = 1;
    setsockopt(listen_sock_fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // bind socket to port
    if (bind(listen_sock_fd_, rp->ai_addr, rp->ai_addrlen) == 0) {
      // bind success
      sock_family_ = rp->ai_family;
      break;
    }

    // bind failed; loop back and try another address
    close(listen_sock_fd_);
    listen_sock_fd_ = -1;
  }

  // free the structure returned by getaddrinfo().
  freeaddrinfo(result);

  // socket/bind failed
  if (listen_sock_fd_ == -1)
    return false;

  // listen for connections
  if (listen(listen_sock_fd_, SOMAXCONN) != 0) {
    std::cerr << "listen() failed: " << strerror(errno) << std::endl;
    close(listen_sock_fd_);
    listen_sock_fd_ = -1;
    return false;
  }

  // listen scuccessful; return the listening socket
  *listen_fd = listen_sock_fd_;
  return true;
}

bool ServerSocket::accept_client(int *accepted_fd,
                          std::string *client_addr,
                          uint16_t *client_port,
                          std::string *client_dns_name,
                          std::string *server_addr,
                          std::string *server_dns_name) const {
  // Accept a new connection on the listening socket listen_sock_fd_.
  // (Block until a new connection arrives.)  Return the newly accepted
  // socket, as well as information about both ends of the new connection,
  // through the various output parameters.

  while (true) {
    // accept a new connection
    struct sockaddr_storage client_addr_storage;
    socklen_t client_addr_storage_len = sizeof(client_addr_storage);
    int client_fd = accept(listen_sock_fd_,
                           reinterpret_cast<struct sockaddr *> (&client_addr_storage),
                           &client_addr_storage_len);
    if (client_fd < 0) {
      if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
        continue;
      std::cerr << "Failure on accept: " << strerror(errno) << std::endl;
      close(listen_sock_fd_);
      return false;
    }
    // accept successful
    else {
      // return the accepted socket
      *accepted_fd = client_fd;

      // get client address and port
      if (client_addr_storage.ss_family == AF_INET) {
        // ipv4
        char client_addr_str[INET_ADDRSTRLEN];
        struct sockaddr_in *s = reinterpret_cast<struct sockaddr_in *> (&client_addr_storage);
        inet_ntop(AF_INET, &s->sin_addr, client_addr_str, INET_ADDRSTRLEN);
        *client_port = ntohs(s->sin_port);
        *client_addr = client_addr_str;
      } else {
        // ipv6
        char client_addr_str[INET6_ADDRSTRLEN];
        struct sockaddr_in6 *s = reinterpret_cast<struct sockaddr_in6 *> (&client_addr_storage);
        inet_ntop(AF_INET6, &s->sin6_addr, client_addr_str, INET6_ADDRSTRLEN);
        *client_port = ntohs(s->sin6_port);
        *client_addr = client_addr_str;
      }

      // get client dns name
      char client_dns_name_str[NI_MAXHOST];
      int res = getnameinfo(reinterpret_cast<struct sockaddr*>(&client_addr_storage),
                            client_addr_storage_len,
                            client_dns_name_str,
                            NI_MAXHOST,
                            nullptr,
                            0,
                            0);
      if (res != 0) {
        sprintf(client_dns_name_str, "[reverse DNS failed]");
      }
      *client_dns_name = client_dns_name_str;

      // get server address and port
      char hname[1024];
      hname[0] = '\0';

      if (sock_family_ == AF_INET) {
        // The server is using an IPv4 address.
        struct sockaddr_in srvr;
        socklen_t srvrlen = sizeof(srvr);
        char addrbuf[INET_ADDRSTRLEN];
        getsockname(client_fd, (struct sockaddr *) &srvr, &srvrlen);
        inet_ntop(AF_INET, &srvr.sin_addr, addrbuf, INET_ADDRSTRLEN);
        *server_addr = std::string(addrbuf);
        // Get the server's dns name, or return it's IP address as
        // a substitute if the dns lookup fails.
        getnameinfo((const struct sockaddr *) &srvr,
                    srvrlen, hname, 1024, nullptr, 0, 0);
        *server_dns_name = std::string(hname);
      } else {
        // The server is using an IPv6 address.
        struct sockaddr_in6 srvr;
        socklen_t srvrlen = sizeof(srvr);
        char addrbuf[INET6_ADDRSTRLEN];
        getsockname(client_fd, (struct sockaddr *) &srvr, &srvrlen);
        inet_ntop(AF_INET6, &srvr.sin6_addr, addrbuf, INET6_ADDRSTRLEN);
        *server_addr = std::string(addrbuf);
        // Get the server's dns name, or return it's IP address as
        // a substitute if the dns lookup fails.
        getnameinfo((const struct sockaddr *) &srvr,
                    srvrlen, hname, 1024, nullptr, 0, 0);
        *server_dns_name = std::string(hname);
      }
      
      break;
    }
  }
  return true;
}

}  // namespace searchserver
