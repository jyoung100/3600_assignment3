#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/stat.h>
#include <string.h>
#include "Practical.h"

#define IMAGE_CHUNK_SIZE 8192

int main(int argc, char *argv[]) {
  if (argc != 3)
    DieWithUserMessage("Parameter(s)", "<Server Address/Name> <Server Port/Service>");

  char *server = argv[1];
  char *servPort = argv[2];

  struct addrinfo addrCriteria;
  memset(&addrCriteria, 0, sizeof(addrCriteria));
  addrCriteria.ai_family = AF_UNSPEC;
  addrCriteria.ai_socktype = SOCK_STREAM;
  addrCriteria.ai_protocol = IPPROTO_TCP;

  struct addrinfo *servAddr;
  int rtnVal = getaddrinfo(server, servPort, &addrCriteria, &servAddr);
  if (rtnVal != 0)
    DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));

  int sock = socket(servAddr->ai_family, servAddr->ai_socktype, servAddr->ai_protocol);
  if (sock < 0)
    DieWithSystemMessage("socket() failed");

  if (connect(sock, servAddr->ai_addr, servAddr->ai_addrlen) < 0)
    DieWithSystemMessage("connect() failed");

  freeaddrinfo(servAddr);

  printf("********************************************\n");
  printf("Welcome to Simple File Server\n");
  printf("********************************************\n");

  char filename[BUFSIZE];
  char buffer[BUFSIZE];

  while (1) {
    printf("\nSelect an action from the menu:\n");
    printf("1. Download text file\n");
    printf("2. Download image file\n");
    printf("3. Exit\n");

    int choice;
    if (scanf("%d", &choice) != 1) {
      printf("Invalid input. Please enter a number.\n");
      while (getchar() != '\n'); 
      continue;
    }
    getchar();

    if (choice == 1) {
      while (1) {
        printf("Enter filename to download:\n");
        if (fgets(filename, sizeof(filename), stdin) == NULL) {
          break;
        }
        size_t len = strlen(filename);
        if (len > 0 && filename[len - 1] == '\n')
          filename[len - 1] = '\0';

        if (strlen(filename) == 0) {
          printf("Please enter a valid filename.\n");
          continue;
        }

        char request[BUFSIZE];
        snprintf(request, BUFSIZE, "TEXT:%s", filename);
        if (send(sock, request, strlen(request), 0) < 0)
          DieWithSystemMessage("send() failed");

        ssize_t numBytes = recv(sock, buffer, BUFSIZE - 1, 0);
        if (numBytes < 0)
          DieWithSystemMessage("recv() failed");
        buffer[numBytes] = '\0';

        if (strncmp(buffer, "ERROR:", 6) == 0) {
          printf("Sorry, file %s does not exist on the server, please enter filename to download:\n", filename);
          continue;
        }

        if (strcmp(buffer, "OK") == 0) {
          printf("Downloading %s\n", filename);
          FILE *outFile = fopen("downloaded_text.txt", "w");
          if (outFile == NULL) {
            DieWithSystemMessage("fopen() failed for downloaded_text.txt");
          }

          size_t totalBytes = 0;
          char lineBuffer[BUFSIZE];
          size_t lineBufferPos = 0;
          
          while (1) {
            numBytes = recv(sock, buffer, BUFSIZE - 1, 0);
            
            if (numBytes == 0)
              break;

            buffer[numBytes] = '\0';

            char *fileEndPtr = strstr(buffer, "FILE_END");
            if (fileEndPtr != NULL) {
              size_t bytesBeforeEnd = fileEndPtr - buffer;
              if (bytesBeforeEnd > 0) {
                if (lineBufferPos > 0) {
                  memcpy(lineBuffer + lineBufferPos, buffer, bytesBeforeEnd);
                  lineBuffer[lineBufferPos + bytesBeforeEnd] = '\0';
                  printf("%s", lineBuffer);
                  fputs(lineBuffer, outFile);
                  totalBytes += (lineBufferPos + bytesBeforeEnd);
                  lineBufferPos = 0;
                  usleep(500000);
                } else {
                  printf("%.*s", (int)bytesBeforeEnd, buffer);
                  fwrite(buffer, 1, bytesBeforeEnd, outFile);
                  totalBytes += bytesBeforeEnd;
                  usleep(500000);
                }
              }
              break;
            }

            char *lineStart = buffer;
            char *newline;
            while ((newline = strchr(lineStart, '\n')) != NULL) {
              size_t lineLen = newline - lineStart + 1;
              
              if (lineBufferPos > 0) {
                memcpy(lineBuffer + lineBufferPos, lineStart, lineLen);
                lineBuffer[lineBufferPos + lineLen] = '\0';
                printf("%s", lineBuffer);
                fputs(lineBuffer, outFile);
                totalBytes += (lineBufferPos + lineLen);
                lineBufferPos = 0;
                usleep(500000);
              } else {
                printf("%.*s", (int)lineLen, lineStart);
                fwrite(lineStart, 1, lineLen, outFile);
                totalBytes += lineLen;
                usleep(500000);
              }
              
              lineStart = newline + 1;
            }
            
            size_t remaining = strlen(lineStart);
            if (remaining > 0) {
              if (lineBufferPos + remaining < BUFSIZE) {
                memcpy(lineBuffer + lineBufferPos, lineStart, remaining);
                lineBufferPos += remaining;
                lineBuffer[lineBufferPos] = '\0';
              }
            }
          }
          
          if (lineBufferPos > 0) {
            printf("%s", lineBuffer);
            fputs(lineBuffer, outFile);
            totalBytes += lineBufferPos;
          }

          fclose(outFile);
          printf("\nDownloaded %zu bytes\n", totalBytes);
          printf("Done\n");
          break;
        }
      }
    }
    else if (choice == 2) {
      while (1) {
        printf("Enter filename to download:\n");
        if (fgets(filename, sizeof(filename), stdin) == NULL) {
          break;
        }
        size_t len = strlen(filename);
        if (len > 0 && filename[len - 1] == '\n')
          filename[len - 1] = '\0';

        if (strlen(filename) == 0) {
          printf("Please enter a valid filename.\n");
          continue;
        }

        // Send request to server
        char request[BUFSIZE];
        snprintf(request, BUFSIZE, "IMAGE:%s", filename);
        if (send(sock, request, strlen(request), 0) < 0)

        ssize_t numBytes = recv(sock, buffer, BUFSIZE - 1, 0);
        
        buffer[numBytes] = '\0';

        if (strncmp(buffer, "ERROR:", 6) == 0) {
          printf("Sorry, file %s does not exist on the server, please enter filename to download:\n", filename);
          continue;
        }

        if (strncmp(buffer, "OK:", 3) == 0) {
          size_t fileSize = 0;
          sscanf(buffer + 3, "%zu", &fileSize);

          printf("Downloading %s\n", filename);
          FILE *outFile = fopen("downloaded_image.ppm", "wb");

          size_t totalBytes = 0;
          char recvBuffer[IMAGE_CHUNK_SIZE + 8]; // Extra space for FILE_END marker
          size_t bufferPos = 0;
          
          while (1) {
            size_t maxRecv = IMAGE_CHUNK_SIZE - bufferPos;
            if (maxRecv == 0) {
              fwrite(recvBuffer, 1, IMAGE_CHUNK_SIZE, outFile);
              totalBytes += IMAGE_CHUNK_SIZE;
              bufferPos = 0;
              maxRecv = IMAGE_CHUNK_SIZE;
            }
            
            numBytes = recv(sock, recvBuffer + bufferPos, maxRecv, 0);
            if (numBytes < 0)
              DieWithSystemMessage("recv() failed");
            if (numBytes == 0)
              break;

            bufferPos += numBytes;

            char *fileEndPtr = NULL;
            if (bufferPos >= 8) {
              for (size_t i = 0; i <= bufferPos - 8; i++) {
                if (memcmp(recvBuffer + i, "FILE_END", 8) == 0) {
                  fileEndPtr = recvBuffer + i;
                  break;
                }
              }
            }
            if (fileEndPtr != NULL) {
              size_t bytesBeforeEnd = fileEndPtr - recvBuffer;
              if (bytesBeforeEnd > 0) {
                fwrite(recvBuffer, 1, bytesBeforeEnd, outFile);
                totalBytes += bytesBeforeEnd;
                
                int progress = fileSize > 0 ? (int)((totalBytes * 100) / fileSize) : 0;
                if (progress > 100) progress = 100;
                printf("\rProgress: [");
                int position = (progress * 20) / 100;
                for (int i = 0; i < 20; i++) {
                  if (i < position)
                    printf("=");
                  else if (i == position)
                    printf(">");
                  else
                    printf("-");
                }
                printf("] %d%% (%zu/%zu bytes)", progress, totalBytes, fileSize);
                fflush(stdout);
              }
              break;
            }

            if (bufferPos >= IMAGE_CHUNK_SIZE) {
              fwrite(recvBuffer, 1, IMAGE_CHUNK_SIZE, outFile);
              totalBytes += IMAGE_CHUNK_SIZE;
              
              size_t remaining = bufferPos - IMAGE_CHUNK_SIZE;
              if (remaining > 0) {
                memmove(recvBuffer, recvBuffer + IMAGE_CHUNK_SIZE, remaining);
              }
              bufferPos = remaining;

              int progress = fileSize > 0 ? (int)((totalBytes * 100) / fileSize) : 0;
              if (progress > 100) progress = 100;

              printf("\rProgress: [");
              int position = (progress * 20) / 100;
              for (int i = 0; i < 20; i++) {
                if (i < position)
                  printf("=");
                else if (i == position)
                  printf(">");
                else
                  printf("-");
              }
              printf("] %d%% (%zu/%zu bytes)", progress, totalBytes, fileSize);
              fflush(stdout);

              usleep(500000); 
            }
          }
          
          if (bufferPos > 0) {
            fwrite(recvBuffer, 1, bufferPos, outFile);
            totalBytes += bufferPos;
          }

          fclose(outFile);
          printf("\rProgress: [====================] 100%% (%zu/%zu bytes)\n", totalBytes, fileSize);
          
          if (totalBytes < 1024) {
            printf("Downloaded %zu bytes\n", totalBytes);
          } else if (totalBytes < 1024 * 1024) {
            printf("Downloaded %zu kilo bytes\n", totalBytes / 1024);
          } else {
            printf("Downloaded %zu mega bytes\n", totalBytes / (1024 * 1024));
          }
          printf("Done\n");
          break;
        }
      }
    }
    else if (choice == 3) {
      if (send(sock, "EXIT", 4, 0) < 0)
        DieWithSystemMessage("send() failed");
      
      recv(sock, buffer, BUFSIZE - 1, 0);
      
      printf("Goodbye!!!\n");
      printf("***************************\n");
      break;
    }
    else {
      printf("Invalid choice. Please select 1, 2, or 3.\n");
    }
  }

  close(sock);
  return 0;
}

