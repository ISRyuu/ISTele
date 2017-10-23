#include "rm_net.h"
#include <netdb.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int
tcp_server(const char *host, const char* service, int* socklen)
{
  struct addrinfo hint, *adif, *head;
  int error;
  int sockfd;
  int reuse = 1;

  bzero(&hint, sizeof(hint));
  hint.ai_flags    = AI_PASSIVE;
  hint.ai_socktype = SOCK_STREAM;

  if ( (error = getaddrinfo(host, service, &hint, &adif)) != 0 ) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(error));
    return -1;
  }

  head = adif;
  while ( adif != NULL ) {
    
    if ( (sockfd = socket(adif->ai_family, adif->ai_socktype, adif->ai_protocol)) >= 0 ) {

      setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

      if ( bind(sockfd, adif->ai_addr, adif->ai_addrlen) == 0 )
          break;
      else close(sockfd);
    }
      
    adif = adif->ai_next;
  }

  if ( adif == NULL ) {
    freeaddrinfo(head);
    return -1;
  } 

  listen(sockfd, LISTENQ);

  if ( socklen )
    *socklen = adif->ai_addrlen;

  freeaddrinfo(head);
  
  return sockfd;
}

int
udp_server(const char *host, const char* service, int* socklen)
{
  struct addrinfo hint, *adif, *head;
  int error;
  int sockfd;

  bzero(&hint, sizeof(hint));
  
  hint.ai_flags    = AI_PASSIVE;
  hint.ai_socktype = SOCK_DGRAM;

  if ( (error = getaddrinfo(host, service, &hint, &adif)) != 0 ) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(error));
    return -1;
  }

  head = adif;
  
  while ( adif != NULL ) {
    if ( (sockfd = socket(adif->ai_family, adif->ai_socktype, adif->ai_protocol)) < 0 )
      continue;

    if ( bind(sockfd, adif->ai_addr, adif->ai_addrlen) == 0 )
      break;

    close(sockfd);
    
    adif = adif->ai_next;
  }

  if ( adif == NULL ) {
    freeaddrinfo(head);
    return -1;
  }

  if ( socklen )
    *socklen = adif->ai_addrlen;

  freeaddrinfo(head);
  
  return sockfd;
}

int
tcp_connect(const char* host, const char* service)
{
  int sockfd;
  struct addrinfo hint, *adif, *head;
  int error;

  bzero(&hint, sizeof(hint));

  hint.ai_family   = AF_UNSPEC; 
  hint.ai_socktype = SOCK_STREAM;
  hint.ai_flags    = AI_CANONNAME;	

  if ( (error = getaddrinfo(host, service, &hint, &adif) ) != 0 ) {
    printf("getaddrinfo error: %s\n", gai_strerror(error));
    return -1;
  }

  head = adif;
  
  while ( adif != NULL ) {
    if ( (sockfd = socket(adif->ai_family, adif->ai_socktype, adif->ai_protocol)) < 0 )
      continue;
    
    if ( connect(sockfd, adif->ai_addr, adif->ai_addrlen) == 0 )
      break;

    close(sockfd);
    
    adif = adif->ai_next;
  }

  if ( adif == NULL ) {
    freeaddrinfo(head);
    return -1;
  }

   //  printf("connnected to host: %s\n", adif->ai_canonname);
  
  freeaddrinfo(head);

  return sockfd;
}

int
udp_connect(const char* host, const char* service, int* len)
{
  int sockfd;
  struct addrinfo hint, *adif, *head;
  int error;

  bzero(&hint, sizeof(hint));

  hint.ai_family   = AF_UNSPEC; 
  hint.ai_socktype = SOCK_DGRAM;
  hint.ai_flags    = AI_CANONNAME;	

  if ( (error = getaddrinfo(host, service, &hint, &adif) ) != 0 ) {
    printf("getaddrinfo error: %s\n", gai_strerror(error));
    return -1;
  }

  head = adif;
  
  while ( adif != NULL ) {
    if ( (sockfd = socket(adif->ai_family, adif->ai_socktype, adif->ai_protocol)) < 0 )
      continue;

    if ( connect(sockfd, adif->ai_addr, adif->ai_addrlen) == 0 )
      break;

    close(sockfd);
    adif = adif->ai_next;
  }

  if ( adif == NULL ) {
    freeaddrinfo(head);
    return -1;
  }
  //  printf("connnected to host: %s\n", adif->ai_canonname);
  if ( len )
    *len = adif->ai_addrlen;
    
  freeaddrinfo(head);

  return sockfd;
}


int
udp_client(const char* host, const char* service, struct sockaddr** addr, socklen_t* len)
{
  int sockfd;
  struct addrinfo hint, *adif, *head;
  int error;

  bzero(&hint, sizeof(hint));

  hint.ai_family   = AF_UNSPEC; 
  hint.ai_socktype = SOCK_DGRAM;
  hint.ai_flags    = AI_CANONNAME;	

  if ( (error = getaddrinfo(host, service, &hint, &adif) ) != 0 ) {
    printf("getaddrinfo error: %s\n", gai_strerror(error));
    return -1;
  }

  head = adif;
  
  while ( adif != NULL ) {
    if ( (sockfd = socket(adif->ai_family, adif->ai_socktype, adif->ai_protocol)) >= 0 )
        break;
     
    adif = adif->ai_next;
  }

  if ( adif == NULL ) {
    freeaddrinfo(head);
    return -1;
  }

    if ( bind(sockfd, adif->ai_addr, adif->ai_addrlen) != 0 ) {
        freeaddrinfo(head);
        return -1;
    }

    if ( getsockname(sockfd, adif->ai_addr, &adif->ai_addrlen) != 0 ) {
        freeaddrinfo(head);
        return -1;
    }

  *addr = (struct sockaddr*)malloc(adif->ai_addrlen);
  if ( *addr == NULL ) {
    perror("malloc error");
    close(sockfd);
    sockfd = -1;
  } else {
    memcpy(*addr, adif->ai_addr, adif->ai_addrlen);
    *len = adif->ai_addrlen;
  }
  freeaddrinfo(head);

  return sockfd;
}
