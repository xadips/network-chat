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

#define BUFFLEN 1024
#define MAXCLIENTS 10
#define MAX_NAME_LENGTH 21
#define MAX_PASSWORD_LENGTH 64
#define MAX_DB_SIZE 1000

typedef struct
{
    char *username;
    char *password;
    int socket;
    bool isAuthenticated;
} User;

int findemptyuser(User *users)
{
    int i;
    for (i = 0; i < MAXCLIENTS; i++)
    {
        if (users[i].socket == -1)
        {
            return i;
        }
    }
    return -1;
}

int findUserID(char *username, char *password, char usernames[MAX_DB_SIZE][MAX_NAME_LENGTH], char passwords[MAX_DB_SIZE][MAX_PASSWORD_LENGTH], int userCount)
{
    printf("%s %s\n", username, usernames[0]);
    for (int i = 0; i < userCount; i++)
    {
        if (strcmp(username, usernames[i]) == 0 && strcmp(password, passwords[i]) == 0)
        {
            return i;
        }
    }
    return -1;
}

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

void sendAll(User users[], char buffer[], int excludeSocket)
{ // sends a message in buffer to all logged in users
    printf("%s\n", buffer);
    for (int i = 0; i < MAXCLIENTS; i++)
    {
        if (users[i].socket != -1)
        {
            if (users[i].isAuthenticated)
            {
                int writeLength = send(users[i].socket, buffer, strlen(buffer), 0);
                if (writeLength <= 0)
                {
                    printf("Disconnected: %d, %s\n", users[i].socket, users[i].username);
                    users[i].socket = -1;
                    users[i].isAuthenticated = false;
                    strcpy(users[i].username, "");
                    close(users[i].socket);
                }
            }
        }
    }
}

void registerUser(char *username, char *password)
{
    FILE *wrPtr = fopen("db.txt", "a");
    fprintf(wrPtr, "\n%s\n%s", username, password);
    fclose(wrPtr);
}

int main(int argc, char *argv[])
{
    char usernames[MAX_DB_SIZE][MAX_NAME_LENGTH];
    char passwords[MAX_DB_SIZE][MAX_PASSWORD_LENGTH];
    int oldCount = 0, loadedUsers = 0;
    User users[MAXCLIENTS];
    unsigned int port;
    unsigned int clientaddrlen;
    int l_socket;
    //int c_sockets[MAXCLIENTS];
    fd_set read_set;

    struct sockaddr_in servaddr;
    struct sockaddr_in clientaddr;

    int maxfd = 0;
    int i;

    char buffer[BUFFLEN], buffer2[BUFFLEN + MAX_NAME_LENGTH + 2];

    if (argc != 2)
    {
        fprintf(stderr, "USAGE: %s <port>\n", argv[0]);
        return -1;
    }
    port = atoi(argv[1]);
    if ((port < 1) || (port > 65535))
    {
        fprintf(stderr, "ERROR #1: invalid port specified.\n");
        return -1;
    }

    if ((l_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "ERROR #2: cannot create listening socket.\n");
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(l_socket, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        fprintf(stderr, "ERROR #3: bind listening socket.\n");
        return -1;
    }

    if (listen(l_socket, 5) < 0)
    {
        fprintf(stderr, "ERROR #4: error in listen().\n");
        return -1;
    }

    for (i = 0; i < MAXCLIENTS; i++)
    {
        users[i].socket = -1;
        users[i].username = malloc(MAX_NAME_LENGTH * sizeof(char));
        users[i].password = malloc(MAX_PASSWORD_LENGTH * sizeof(char));
        users[i].username = "";
        users[i].password = "";
        users[i].isAuthenticated = false;
    }
    loadDatabase(usernames, passwords, &loadedUsers);

    printf("%s %s\n", usernames[0], passwords[0]);
    printf("%s %s\n", usernames[1], passwords[1]);

    fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) | O_NONBLOCK); // making stdin socked non-blocking

    for (;;)
    {
        FD_ZERO(&read_set);
        FD_SET(0, &read_set);
        for (i = 0; i < MAXCLIENTS; i++)
        {
            if (users[i].socket != -1)
            {
                FD_SET(users[i].socket, &read_set);
                if (users[i].socket > maxfd)
                {
                    maxfd = users[i].socket;
                }
            }
        }

        FD_SET(l_socket, &read_set);
        if (l_socket > maxfd)
        {
            maxfd = l_socket;
        }

        select(maxfd + 1, &read_set, NULL, NULL, NULL);
        int client_id;
        if (FD_ISSET(l_socket, &read_set))
        {
            client_id = findemptyuser(users);
            if (client_id != -1)
            {
                clientaddrlen = sizeof(clientaddr);
                memset(&clientaddr, '\0', clientaddrlen);
                users[client_id].socket = accept(l_socket, (struct sockaddr *)&clientaddr, &clientaddrlen);
                printf("Connected:  %s\n", inet_ntoa(clientaddr.sin_addr));

                //int m_len = sprintf(buffer, "Username: \n");
                //send(users[client_id].socket, buffer, m_len, 0);
            }
        }

        if (FD_ISSET(0, &read_set))
        { // stdin -> might want to close server with /q
            memset(&buffer, 0, BUFFLEN);
            int i = read(0, &buffer, BUFFLEN);
            if (i > 0)
            {
                printf("%s\n", buffer);
                if (strcmp(buffer, "exit\n\0") == 0)
                {
                    break;
                }
                else
                {
                    memset(&buffer2, '\0', BUFFLEN + MAX_NAME_LENGTH + 2);
                    sprintf(buffer2, "Server: %s", buffer);
                    sendAll(users, buffer2, -2);
                }
            }
        }

        for (i = 0; i < MAXCLIENTS; i++)
        {
            if (users[i].socket != -1)
            {
                //int newUser = strcmp(users[i].username, "");
                if (FD_ISSET(users[i].socket, &read_set))
                {
                    memset(&buffer, 0, BUFFLEN);
                    int r_len = recv(users[i].socket, &buffer, BUFFLEN, 0);
                    if (r_len <= 0)
                    {
                        printf("Disconnected: %i", users[i].socket);
                        users[i].socket = -1;
                        users[i].isAuthenticated = false;
                        users[i].username = "";
                        users[i].password = "";

                        close(users[i].socket);
                    }
                    else if (users[i].isAuthenticated)
                    {
                        memset(&buffer2, '\0', BUFFLEN + MAX_NAME_LENGTH);
                        sprintf(buffer2, "%s: %s", users[i].username, buffer);

                        sendAll(users, buffer2, users[i].socket);
                    }
                    else if (!users[i].isAuthenticated)
                    {
                        char tempUsername[MAX_NAME_LENGTH];
                        char tempPassword[MAX_PASSWORD_LENGTH];
                        strcpy(tempUsername, buffer);
                        strcpy(tempPassword, buffer + MAX_NAME_LENGTH);
                        tempUsername[strlen(tempUsername) - 1] = '\0';
                        tempPassword[strlen(tempPassword) - 1] = '\0';
                        //printf("%s\n", buffer);

                        int id = findUserID(tempUsername, tempPassword, usernames, passwords, loadedUsers);
                        printf("Returned id: %i\n", id);
                        if (id >= 0)
                        {
                            users[i].username = usernames[id];
                            users[i].password = passwords[id];
                            memset(&buffer, '\0', BUFFLEN);
                            sprintf(buffer, "Welcome to the server %s\n", users[i].username);
                            users[i].isAuthenticated = true;
                            sendAll(users, buffer, -2);
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
                            //loadedUsers++;
                            //users[i].username = tempUsername;
                            //registerUser(tempUsername, tempPassword);
                            //strcpy(usernames[loadedUsers - 1], buffer);

                            //memset(&buffer, '\0', BUFFLEN);
                            //sprintf(buffer, "A new user registered here %s\n", users[i].username);

                            //sendAll(users, buffer, -2);
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i < MAXCLIENTS; ++i)
    {
        if (users[i].socket != -1)
        {
            close(users[i].socket);
        }
    }

    close(l_socket);

    return 0;
}
