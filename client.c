/*
 * Chat client
 * 
 * Author: Ričardas Čubukinas
 * Description: A chat client to connect to the server
 */

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#define BUFFER_LENGTH 1024
#define MAX_NAME_LENGTH 21
#define MAX_PASSWORD_LENGTH 64

int main(int argc, char *argv[])
{
    unsigned int clientPort;
    int serverSocket, i;
    struct sockaddr_in serverAddress;
    fd_set readSet;

    char inBuffer[BUFFER_LENGTH], outBuffer[BUFFER_LENGTH];

    // Expectcs a correct amount of arguments
    if (argc != 3)
    {
        fprintf(stderr, "USAGE: %s <ip> <port>\n", argv[0]);
        exit(1);
    }

    clientPort = atoi(argv[2]);

    if ((clientPort < 1) || (clientPort > 65535))
    {
        printf("ERROR: invalid port specified.\n");
        exit(1);
    }

    // Server socket - up
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "ERROR: cannot create socket.\n");
        exit(1);
    }

    // Clear and set server structure
    memset(&serverAddress, 0, sizeof(serverAddress));
    // Setting protocol(IP) and port(cPort)
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(clientPort);

    // Convert IPv4 address into binary form
    if (inet_aton(argv[1], &serverAddress.sin_addr) <= 0)
    {
        fprintf(stderr, "ERROR: Invalid remote IP address.\n");
        exit(1);
    }

    // If all good, connect
    if (connect(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        fprintf(stderr, "ERROR: error in connect().\n");
        exit(1);
    }

    bool isAuthenticated = false;
    char tempName[MAX_NAME_LENGTH];
    char tempPassword[MAX_PASSWORD_LENGTH];

    memset(&outBuffer, 0, BUFFER_LENGTH);
    // Block stdin until the user authenticates
    fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) & ~O_NONBLOCK);

    // Start authentication loop
    while (!isAuthenticated)
    {
        memset(&tempName, '\0', MAX_NAME_LENGTH);
        memset(&tempPassword, '\0', MAX_PASSWORD_LENGTH);

        printf("Username: ");
        fgets(tempName, MAX_NAME_LENGTH, stdin);

        if (strcmp(tempName, "/exit\n\0") == 0)
        {
            break;
        }

        printf("Password: ");
        fgets(tempPassword, MAX_PASSWORD_LENGTH, stdin);

        if (strcmp(tempPassword, "/exit\n\0") == 0)
        {
            break;
        }

        memset(&outBuffer, 0, BUFFER_LENGTH);

        strcpy(outBuffer, tempName);
        strcpy(outBuffer + MAX_NAME_LENGTH, tempPassword);
        write(serverSocket, outBuffer, MAX_NAME_LENGTH + MAX_PASSWORD_LENGTH);

        // Wait for an authentication response from server, break if all good
        while (true)
        {
            FD_ZERO(&readSet);
            FD_SET(serverSocket, &readSet);
            select(serverSocket + 1, &readSet, NULL, NULL, NULL);

            if (FD_ISSET(serverSocket, &readSet))
            {
                memset(&inBuffer, 0, BUFFER_LENGTH);
                i = read(serverSocket, &inBuffer, BUFFER_LENGTH);
                if (i <= 0)
                {
                    printf("Connection lost\n");
                    isAuthenticated = false;
                    break;
                }
                else
                {
                    if (strcmp(inBuffer, "0\n\0") == 0)
                    {
                        printf("Wrong username or account already online!\n");
                        break;
                    }
                    else
                    {
                        isAuthenticated = true;
                        printf("%s", inBuffer);
                        break;
                    }
                }
            }
        }
    }

    // Reset stdin blocking, let the  chatting begin
    fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) | O_NONBLOCK);

    while (isAuthenticated)
    {
        // Clear waiting, then set it for server and stdin
        FD_ZERO(&readSet);
        FD_SET(serverSocket, &readSet);
        FD_SET(0, &readSet);

        select(serverSocket + 1, &readSet, NULL, NULL, NULL);

        // If received a message
        if (FD_ISSET(serverSocket, &readSet))
        {
            memset(&inBuffer, 0, BUFFER_LENGTH);
            i = read(serverSocket, &inBuffer, BUFFER_LENGTH);
            // If received size <= 0, connection was probably lost
            if (i <= 0)
            {
                printf("Connection lost\n");
                break;
            }
            else
            {
                printf("%s", inBuffer);
            }
        }
        // If sent a message
        else if (FD_ISSET(0, &readSet))
        {
            memset(&outBuffer, '\0', BUFFER_LENGTH);
            i = read(0, &outBuffer, BUFFER_LENGTH);
            if (i > 0)
            {
                if (strcmp(outBuffer, "/exit\n\0") == 0)
                {
                    isAuthenticated = false;
                }
                else
                {
                    write(serverSocket, outBuffer, i);
                }
            }
        }
    }

    close(serverSocket);

    return 0;
}
