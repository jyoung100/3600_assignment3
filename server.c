#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "Practical.h"

#define MAXPENDING 5

int main(int argc, char *argv[]) {
  if (argc != 2)
    DieWithUserMessage("Parameter(s)", "<Server Port/Service>");

  char *service = argv[1];

  // Construct the server address structure
  struct addrinfo addrCriteria;
  memset(&addrCriteria, 0, sizeof(addrCriteria));
  addrCriteria.ai_family = AF_UNSPEC;
  addrCriteria.ai_flags = AI_PASSIVE;
  addrCriteria.ai_socktype = SOCK_STREAM;
  addrCriteria.ai_protocol = IPPROTO_TCP;

  struct addrinfo *servAddr;
  int rtnVal = getaddrinfo(NULL, service, &addrCriteria, &servAddr);
  if (rtnVal != 0)
    DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));

  // Create socket for incoming connections
  int servSock = socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol);
  if (servSock < 0)
    DieWithSystemMessage("socket() failed");

  // Bind to the local address
  if (bind(servSock, servAddr->ai_addr, servAddr->ai_addrlen) < 0)
    DieWithSystemMessage("bind() failed");

  // Mark the socket so it will listen for incoming connections
  if (listen(servSock, MAXPENDING) < 0)
    DieWithSystemMessage("listen() failed");

  // Free address list allocated by getaddrinfo()
  freeaddrinfo(servAddr);

  printf("********************************************\n");
  printf("Welcome to Simple File Server\n");
  printf("********************************************\n");

  for (;;) {
    struct sockaddr_storage clntAddr;
    socklen_t clntAddrLen = sizeof(clntAddr);

    // Wait for a client to connect
    int clntSock = accept(servSock, (struct sockaddr *) &clntAddr, &clntAddrLen);
    if (clntSock < 0)
      DieWithSystemMessage("accept() failed");

    // Print client connection info
    char ipStr[INET6_ADDRSTRLEN];
    if (clntAddr.ss_family == AF_INET) {
      struct sockaddr_in *ipv4 = (struct sockaddr_in *)&clntAddr;
      inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, INET6_ADDRSTRLEN);
    } else if (clntAddr.ss_family == AF_INET6) {
      struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)&clntAddr;
      inet_ntop(AF_INET6, &(ipv6->sin6_addr), ipStr, INET6_ADDRSTRLEN);
    } else {
      strcpy(ipStr, "unknown");
    }
    printf("Connected to IP: %s\n", ipStr);

    // Handle client requests
    char buffer[BUFSIZE];
    ssize_t numBytesRcvd;

    while ((numBytesRcvd = recv(clntSock, buffer, BUFSIZE - 1, 0)) > 0) {
      buffer[numBytesRcvd] = '\0';

      if (strncmp(buffer, "TEXT:", 5) == 0) {
        char *filename = buffer + 5;
        printf("-- client requested to download file %s\n", filename);

        FILE *file = fopen(filename, "r");
        if (file == NULL) {
          char errorMsg[BUFSIZE];
          snprintf(errorMsg, BUFSIZE, "ERROR:File not found");
          send(clntSock, errorMsg, strlen(errorMsg), 0);
          continue;
        }

        // Send OK confirmation
        send(clntSock, "OK", 2, 0);

        // Send file line by line
        char line[BUFSIZE];
        size_t totalBytes = 0;
        while (fgets(line, sizeof(line), file) != NULL) {
          size_t lineLen = strlen(line);
          ssize_t bytesSent = send(clntSock, line, lineLen, 0);
          if (bytesSent < 0)
            DieWithSystemMessage("send() failed");
          totalBytes += lineLen;
          usleep(500000); // 0.5 second delay between lines
        }

        fclose(file);
        send(clntSock, "FILE_END", 8, 0);
        printf("Sent %s (%zu bytes) to the client\n", filename, totalBytes);
      }
      else if (strncmp(buffer, "IMAGE:", 6) == 0) {
        char *filename = buffer + 6;
        printf("-- client requested to download file %s\n", filename);

        FILE *file = fopen(filename, "rb");
        if (file == NULL) {
          char errorMsg[BUFSIZE];
          snprintf(errorMsg, BUFSIZE, "ERROR:File not found");
          send(clntSock, errorMsg, strlen(errorMsg), 0);
          continue;
        }

        // Get file size
        struct stat fileStat;
        if (stat(filename, &fileStat) < 0) {
          char errorMsg[BUFSIZE];
          snprintf(errorMsg, BUFSIZE, "ERROR:File not found");
          send(clntSock, errorMsg, strlen(errorMsg), 0);
          fclose(file);
          continue;
        }

        // Send OK with file size
        char okMsg[BUFSIZE];
        snprintf(okMsg, BUFSIZE, "OK:%zu", (size_t)fileStat.st_size);
        send(clntSock, okMsg, strlen(okMsg), 0);

        // Send file in chunks
        char chunk[BUFSIZE];
        size_t totalBytes = 0;
        size_t bytesRead;
        while ((bytesRead = fread(chunk, 1, BUFSIZE, file)) > 0) {
          ssize_t bytesSent = send(clntSock, chunk, bytesRead, 0);
          if (bytesSent < 0)
            DieWithSystemMessage("send() failed");
          totalBytes += bytesRead;
          usleep(500000); // 0.5 second delay between chunks
        }

        fclose(file);
        send(clntSock, "FILE_END", 8, 0);
        
        // Format file size for display
        if (totalBytes < 1024) {
          printf("Sent %s (%zu bytes) to the client\n", filename, totalBytes);
        } else if (totalBytes < 1024 * 1024) {
          printf("Sent %s (%zu kilo bytes) to the client\n", filename, totalBytes / 1024);
        } else {
          printf("Sent %s (%zu mega bytes) to the client\n", filename, totalBytes / (1024 * 1024));
        }
      }
      else if (strncmp(buffer, "EXIT", 4) == 0) {
        send(clntSock, "BYE", 3, 0);
        break;
      }
    }

    close(clntSock);
    printf("Client ");
    PrintSocketAddress((struct sockaddr *) &clntAddr, stdout);
    printf(" left\n");
  }

  close(servSock);
  return 0;
}
