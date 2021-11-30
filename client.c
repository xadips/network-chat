
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
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#define BUFFER_LENGTH 1024
#define MAX_NAME_LENGTH 21
#define MAX_PASSWORD_LENGTH 64

int main(int argc, char *argv[])
{
    unsigned int clientPort;
    int serverSocket, i, s;
    fd_set readSet;

    char buffer[BUFFER_LENGTH];

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


    struct addrinfo hints;
    struct addrinfo *result, *rp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP pls
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          // Any protocol

    s = getaddrinfo(argv[1], argv[2], &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        serverSocket = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (serverSocket == -1)
            continue;

        if (connect(serverSocket, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  // Success

        close(serverSocket);
    }

    freeaddrinfo(result);           /* No longer needed */

    if (rp == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }


    bool isAuthenticated = false;
    char tempName[MAX_NAME_LENGTH];
    char tempPassword[MAX_PASSWORD_LENGTH];

    memset(&buffer, 0, BUFFER_LENGTH);
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

        memset(&buffer, 0, BUFFER_LENGTH);

        strcpy(buffer, tempName);
        strcpy(buffer + MAX_NAME_LENGTH, tempPassword);
        write(serverSocket, buffer, MAX_NAME_LENGTH + MAX_PASSWORD_LENGTH);

        // Wait for an authentication response from server, break if all good
        while (true)
        {
            FD_ZERO(&readSet);
            FD_SET(serverSocket, &readSet);
            select(serverSocket + 1, &readSet, NULL, NULL, NULL);

            if (FD_ISSET(serverSocket, &readSet))
            {
                memset(&buffer, 0, BUFFER_LENGTH);
                i = read(serverSocket, &buffer, BUFFER_LENGTH);
                if (i <= 0)
                {
                    printf("Connection lost\n");
                    isAuthenticated = false;
                    break;
                }
                else
                {
                    if (strcmp(buffer, "0\n\0") == 0)
                    {
                        printf("Wrong username or account already online!\n");
                        break;
                    }
                    else
                    {
                        isAuthenticated = true;
                        printf("%s", buffer);
                        break;
                    }
                }
            }
        }
    }

    // Reset stdin blocking, let the  chatting begin
    fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) | O_NONBLOCK);

    // Once authenticated read messages
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
            memset(&buffer, 0, BUFFER_LENGTH);
            i = read(serverSocket, &buffer, BUFFER_LENGTH);
            // If received size <= 0, connection was probably lost
            if (i <= 0)
            {
                printf("Connection lost\n");
                break;
            }
            else
            {
                printf("%s", buffer);
            }
        }
        // If sent a message
        else if (FD_ISSET(0, &readSet))
        {
            memset(&buffer, '\0', BUFFER_LENGTH);
            i = read(0, &buffer, BUFFER_LENGTH);
            if (i > 0)
            {
                if (strcmp(buffer, "/exit\n\0") == 0)
                {
                    isAuthenticated = false;
                }
                else
                {
                    write(serverSocket, buffer, i);
                }
            }
        }
    }

    close(serverSocket);

    return 0;
}

