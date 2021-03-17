#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

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

int findUserID(char *username, char usernames[MAX_DB_SIZE][MAX_NAME_LENGTH], int userCount)
{
    printf("%s %s\n", username, usernames[0]);
    for (int i = 0; i < userCount; i++)
    {
        if (strcmp(username, usernames[i]) == 0)
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

int main(int argc, char *argv[])
{
    char usernames[MAX_DB_SIZE][MAX_NAME_LENGTH];
    char passwords[MAX_DB_SIZE][MAX_PASSWORD_LENGTH];
    int loadedUsers = 0;
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

    char buffer[BUFFLEN];

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
    printf("hi\n");

    printf("%s %s\n", usernames[0], passwords[0]);
    printf("%s %s\n", usernames[1], passwords[1]);
    for (;;)
    {
        FD_ZERO(&read_set);
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
                memset(&clientaddr, 0, clientaddrlen);
                users[client_id].socket = accept(l_socket,
                                                 (struct sockaddr *)&clientaddr, &clientaddrlen);

                printf("Connected:  %s\n", inet_ntoa(clientaddr.sin_addr));

                int m_len = sprintf(buffer, "Username: \n");
                send(users[client_id].socket, buffer, m_len, 0);
            }
        }
        for (i = 0; i < MAXCLIENTS; i++)
        {
            if (users[i].socket != -1)
            {
                //int newUser = strcmp(users[i].username, "");
                if (FD_ISSET(users[i].socket, &read_set))
                {

                    if (users[i].isAuthenticated)
                    {
                        memset(&buffer, 0, BUFFLEN);
                        int r_len = recv(users[i].socket, &buffer, BUFFLEN, 0);

                        int j;
                        for (j = 0; j < MAXCLIENTS; j++)
                        {
                            //int newUser = strcmp(users[i].username, "");
                            if (users[j].socket != -1 && i != j) // && users[j].isAuthenticated
                            {
                                int w_len = send(users[j].socket, buffer, r_len, 0);
                                if (w_len <= 0)
                                {
                                    close(users[j].socket);
                                    users[j].socket = -1;
                                }
                            }
                        }
                    }
                    else if (!users[i].isAuthenticated && users[i].username == "")
                    {
                        memset(&buffer, 0, BUFFLEN);
                        int r_len = recv(users[client_id].socket, &buffer, BUFFLEN, 0);
                        //printf("%s\n", buffer);
                        buffer[strlen(buffer) - 1] = '\0';
                        int id = findUserID(buffer, usernames, loadedUsers);
                        printf("Returned id: %i\n", id);
                        if (id >= 0)
                        {
                            users[i].username = usernames[id];
                            //users[client_id].password = passwords[id];
                        }
                        else
                        {
                            loadedUsers++;
                            users[i].username = buffer;
                            strcpy(usernames[loadedUsers - 1], buffer);
                        }
                        int m_len = sprintf(buffer, "Password: \n");
                        send(users[i].socket, buffer, m_len, 0);
                    }
                    else
                    {
                        memset(&buffer, 0, BUFFLEN);
                        int r_len = recv(users[client_id].socket, &buffer, BUFFLEN, 0);
                        users[i].password = buffer;
                        users[i].isAuthenticated = true;
                        printf("password? %s\n", buffer);
                    }
                }
            }
        }
    }
    close(l_socket);
    return 0;
}
