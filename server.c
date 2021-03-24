/*
 * Chat server
 * 
 * Author: Ričardas Čubukinas
 * Description: IRC's little brother
 */

#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>

#define BUFFER_LENGTH 1024
#define MAX_CLIENTS 10
#define MAX_NAME_LENGTH 21
#define MAX_PASSWORD_LENGTH 64
#define MAX_DB_SIZE 10000

typedef struct
{
    char *username;
    char *password;
    int socket;
    bool isAuthenticated;
} User;

// Find an empty slot in connected user array
int findEmptySlot(User *users)
{
    int i;
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (users[i].socket == -1)
        {
            return i;
        }
    }
    return -1;
}

// Check if entered username and password is correct, if yes which pos
int loginUser(char *username, char *password, char usernames[MAX_DB_SIZE][MAX_NAME_LENGTH], char passwords[MAX_DB_SIZE][MAX_PASSWORD_LENGTH], int userCount)
{
    //printf("%s %s\n", username, usernames[0]);
    for (int i = 0; i < userCount; i++)
    {
        if (strcmp(username, usernames[i]) == 0 && strcmp(password, passwords[i]) == 0)
        {
            return i;
        }
    }
    return -1;
}

// Preload all user names and passwords
void loadDatabase(char usernames[MAX_DB_SIZE][MAX_NAME_LENGTH], char passwords[MAX_DB_SIZE][MAX_PASSWORD_LENGTH], int *userCount)
{
    FILE *dbPtr = fopen("db.txt", "r");
    int read = fscanf(dbPtr, "%s%s", &usernames[0], &passwords[0]);
    int i;
    for (i = 1; read == 2 && !feof(dbPtr); i++)
    {
        read = fscanf(dbPtr, "%s%s", &usernames[i], &passwords[i]);
    }
    *userCount = i;
    fclose(dbPtr);
}

// Send message to all users except the sender
void sendAll(User users[], char buffer[], int excludeID)
{
    // Uncomment to see user messages in server's console
    //printf("%s", buffer);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (users[i].socket != -1)
        {
            // Don't send the same message to the sender
            if (users[i].isAuthenticated && i != excludeID)
            {
                int writeLength = send(users[i].socket, buffer, strlen(buffer), 0);
                // If user didn't receive the message probably lost connection
                if (writeLength <= 0)
                {
                    printf("Disconnected: %d, %s\n", users[i].socket, users[i].username);
                    users[i].socket = -1;
                    users[i].isAuthenticated = false;
                    users[i].username = "";
                    users[i].password = "";

                    close(users[i].socket);
                }
            }
        }
    }
}

// Registering not implemented yet
void registerUser(char *username, char *password)
{
    FILE *wrPtr = fopen("db.txt", "a");
    fprintf(wrPtr, "\n%s\n%s", username, password);
    fclose(wrPtr);
}

int main(int argc, char *argv[])
{
    char usernames[MAX_DB_SIZE][MAX_NAME_LENGTH], passwords[MAX_DB_SIZE][MAX_PASSWORD_LENGTH];
    int loadedUsers = 0, maxfd = 0, i, localSocket;
    User users[MAX_CLIENTS];
    unsigned int serverPort, clientAddressLength;
    fd_set readSet;

    struct sockaddr_in serverAddress, clientAddress;

    char inBuffer[BUFFER_LENGTH], outBuffer[BUFFER_LENGTH + MAX_NAME_LENGTH * 2];

    if (argc != 2)
    {
        fprintf(stderr, "USAGE: %s <port>\n", argv[0]);
        return -1;
    }

    serverPort = atoi(argv[1]);
    // TCP packet format only likes 2^16-1
    if ((serverPort < 1) || (serverPort > 65535))
    {
        fprintf(stderr, "ERROR: invalid port specified.\n");
        return -1;
    }

    if ((localSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "ERROR: cannot create listening socket.\n");
        return -1;
    }

    // We're using the IP protocol, so clear the structure and set the port
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(serverPort);

    // Don't use a taken port, thanks
    if (bind(localSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        fprintf(stderr, "ERROR: bind listening socket.\n");
        return -1;
    }

    if (listen(localSocket, 5) < 0)
    {
        fprintf(stderr, "ERROR: error in listen() can't hear properly.\n");
        return -1;
    }

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        // RESET THE DATABASE
        users[i].socket = -1;
        users[i].username = malloc(MAX_NAME_LENGTH * sizeof(char));
        users[i].password = malloc(MAX_PASSWORD_LENGTH * sizeof(char));
        users[i].username = "";
        users[i].password = "";
        users[i].isAuthenticated = false;
    }
    loadDatabase(usernames, passwords, &loadedUsers);

    // I'll need to read stdin in case of exit message
    fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) | O_NONBLOCK);

    while (true)
    {
        // Clear reading, read stdin
        FD_ZERO(&readSet);
        FD_SET(0, &readSet);
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            if (users[i].socket != -1)
            {
                FD_SET(users[i].socket, &readSet);
                // Let's get the correct file descriptor
                if (users[i].socket > maxfd)
                {
                    maxfd = users[i].socket;
                }
            }
        }

        // Get user input too
        FD_SET(localSocket, &readSet);
        // Highest socket is the server one
        if (localSocket > maxfd)
        {
            maxfd = localSocket;
        }

        select(maxfd + 1, &readSet, NULL, NULL, NULL);

        int clientID;
        if (FD_ISSET(localSocket, &readSet))
        {
            clientID = findEmptySlot(users);
            if (clientID != -1)
            {
                clientAddressLength = sizeof(clientAddress);
                memset(&clientAddress, '\0', clientAddressLength);
                users[clientID].socket = accept(localSocket, (struct sockaddr *)&clientAddress, &clientAddressLength);
                printf("\033[33mConnected:  %s\033[0m\n", inet_ntoa(clientAddress.sin_addr));
            }
            else
            {
                printf("Why is the server full?\n");
            }
        }

        if (FD_ISSET(0, &readSet))
        {
            memset(&inBuffer, 0, BUFFER_LENGTH);
            int i = read(0, &inBuffer, BUFFER_LENGTH);
            if (i > 0)
            {
                // Maybe the server wants to quit his job too
                if (strcmp(inBuffer, "/exit\n\0") == 0)
                {
                    break;
                }
                else
                {
                    memset(&outBuffer, '\0', BUFFER_LENGTH + MAX_NAME_LENGTH * 2);
                    sprintf(outBuffer, "\033[31;1mServer\033[0m: %s", inBuffer);
                    sendAll(users, outBuffer, -2);
                }
            }
        }

        for (i = 0; i < MAX_CLIENTS; i++)
        {
            // We only care about connected users
            if (users[i].socket != -1)
            {
                if (FD_ISSET(users[i].socket, &readSet))
                {
                    memset(&inBuffer, 0, BUFFER_LENGTH);
                    int recvLength = recv(users[i].socket, &inBuffer, BUFFER_LENGTH, 0);
                    // If received a negative length message, user is down, free his slot
                    if (recvLength <= 0)
                    {
                        printf("\033[33mDisconnected: %i %s\033[0m\n", users[i].socket, users[i].username);
                        users[i].socket = -1;
                        users[i].isAuthenticated = false;
                        users[i].username = "";
                        users[i].password = "";

                        close(users[i].socket);
                    }
                    // Only authenticated users can see messages
                    else if (users[i].isAuthenticated)
                    {
                        memset(&outBuffer, '\0', BUFFER_LENGTH + MAX_NAME_LENGTH);
                        sprintf(outBuffer, "\033[32m%s\033[0m: %s", users[i].username, inBuffer);

                        sendAll(users, outBuffer, i);
                    }
                    // Not authenticated - then do it
                    else if (!users[i].isAuthenticated)
                    {
                        char tempUsername[MAX_NAME_LENGTH];
                        char tempPassword[MAX_PASSWORD_LENGTH];
                        strcpy(tempUsername, inBuffer);
                        strcpy(tempPassword, inBuffer + MAX_NAME_LENGTH);
                        tempUsername[strlen(tempUsername) - 1] = '\0';
                        tempPassword[strlen(tempPassword) - 1] = '\0';

                        int id = loginUser(tempUsername, tempPassword, usernames, passwords, loadedUsers);
                        // Uncomment to see if user authenticates successfully
                        //printf("Returned id: %i\n", id);
                        if (id >= 0)
                        {
                            users[i].username = usernames[id];
                            users[i].password = passwords[id];
                            memset(&inBuffer, '\0', BUFFER_LENGTH);
                            sprintf(inBuffer, "Welcome to the server %s\n", users[i].username);
                            users[i].isAuthenticated = true;
                            sendAll(users, inBuffer, -2);
                        }
                        else
                        {
                            int w_len = send(users[i].socket, "0\n\0", 3, 0);
                            if (w_len <= 0)
                            {
                                printf("Disconnected: %i\n", users[i].socket);
                                close(users[i].socket = -1);
                                users[i].isAuthenticated = false;
                                users[i].username = "";
                            }
                            //TODO: implement automatic registration
                        }
                    }
                }
            }
        }
    }

    // If server is going offline, be kind disconnect the users as well
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (users[i].socket != -1)
        {
            close(users[i].socket);
        }
    }

    close(localSocket);

    return 0;
}
