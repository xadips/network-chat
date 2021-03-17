#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <string.h>
#include <unistd.h>

#define BUFFLEN 1024
#define MAXCLIENTS 10
#define MAX_NAME_LENGTH 21
#define MAX_PASSWORD_LENGTH 64
#define MAX_DB_SIZE 1000

struct User
{
    char username[MAX_NAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
    int userSocket;
} user;

int findemptyuser(int c_sockets[])
{
    int i;
    for (i = 0; i < MAXCLIENTS; i++)
    {
        if (c_sockets[i] == -1)
        {
            return i;
        }
    }
    return -1;
}

void loadDatabase(char **usernames, char **passwords, int *userCount)
{
    FILE *dbPtr = fopen("db.txt", "r");
    int i = 0;
    int read = fscanf(dbPtr, "%s\n%s", usernames[i], passwords[i]);
    for (i = 1; read == 2 && read != EOF; i++)
    {
        read = fscanf(dbPtr, "%s\n%s", usernames[i], passwords[i]);
        printf("%s %s", usernames[i], passwords[i]);
    }

    fclose(dbPtr);
}

int main(int argc, char *argv[])
{
    char(*usernames)[MAX_NAME_LENGTH];
    char(*passwords)[MAX_PASSWORD_LENGTH];
    int loadedUsers = 0;
    struct User users[MAXCLIENTS];
    unsigned int port;
    unsigned int clientaddrlen;
    int l_socket;
    int c_sockets[MAXCLIENTS];
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
        c_sockets[i] = -1;
        usernames = malloc(MAX_DB_SIZE)
    }

    loadDatabase(usernames, passwords);

    for (;;)
    {
        FD_ZERO(&read_set);
        for (i = 0; i < MAXCLIENTS; i++)
        {
            if (c_sockets[i] != -1)
            {
                FD_SET(c_sockets[i], &read_set);
                if (c_sockets[i] > maxfd)
                {
                    maxfd = c_sockets[i];
                }
            }
        }

        FD_SET(l_socket, &read_set);
        if (l_socket > maxfd)
        {
            maxfd = l_socket;
        }

        select(maxfd + 1, &read_set, NULL, NULL, NULL);

        if (FD_ISSET(l_socket, &read_set))
        {
            int client_id = findemptyuser(c_sockets);
            if (client_id != -1)
            {
                clientaddrlen = sizeof(clientaddr);
                memset(&clientaddr, 0, clientaddrlen);
                c_sockets[client_id] = accept(l_socket,
                                              (struct sockaddr *)&clientaddr, &clientaddrlen);
                printf("Connected:  %s\n", inet_ntoa(clientaddr.sin_addr));
            }
        }
        for (i = 0; i < MAXCLIENTS; i++)
        {
            if (c_sockets[i] != -1)
            {
                if (FD_ISSET(c_sockets[i], &read_set))
                {
                    memset(&buffer, 0, BUFFLEN);
                    int r_len = recv(c_sockets[i], &buffer, BUFFLEN, 0);

                    int j;
                    for (j = 0; j < MAXCLIENTS; j++)
                    {
                        if (c_sockets[j] != -1 && i != j)
                        {
                            int w_len = send(c_sockets[j], buffer, r_len, 0);
                            if (w_len <= 0)
                            {
                                close(c_sockets[j]);
                                c_sockets[j] = -1;
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}
