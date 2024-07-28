#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define NAME_SIZE 32
#define BUFFER_SIZE 1024

typedef struct {
    SOCKET file_descriptor;
    char host_name[NAME_SIZE];
    char client_name[NAME_SIZE];
    int port;
    char command[BUFFER_SIZE];
} server_packet;

void *handle_command(void *arg) {
    server_packet *server = (server_packet*) arg;

    char buffer[BUFFER_SIZE];
    if (send(server->file_descriptor, server->client_name, strlen(server->client_name) + 1, 0) == SOCKET_ERROR) {
        printf("ERROR: Failed to send client name\n");
        closesocket(server->file_descriptor);
        free(server);
        return NULL;
    }

    if (send(server->file_descriptor, server->command, strlen(server->command) + 1, 0) == SOCKET_ERROR) {
        printf("ERROR: Failed to send command\n");
        closesocket(server->file_descriptor);
        free(server);
        return NULL;
    }

    int bytes_received = recv(server->file_descriptor, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("%s", buffer);
    } else {
        printf("ERROR: Failed to receive response\n");
    }

    closesocket(server->file_descriptor);
    free(server);
    pthread_exit(NULL);
}

int is_args_valid(int argc, char **argv) {
    if (argc != 5) {
        printf("usage: ./client <host_name> <host> <port> <command> \n");
        return 0;
    }

    char *port_str = argv[3];
    for (int i = 0; port_str[i] != '\0'; i++) {
        if (!isdigit(port_str[i])) {
            printf("%s is not a valid port", port_str);
            return 0;
        }
    }

    return 1;
}

int main(int argc, char **argv) {
    if (!is_args_valid(argc, argv)) {
        return EXIT_FAILURE;
    }

    char *ip = "127.0.0.1";
    char *name = argv[1];
    char *host = argv[2];
    int port = atoi(argv[3]);
    char *command = argv[4];

    WSADATA wsaData;
    WSAStartup(0x202, &wsaData);

    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    int err = connect(server_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (err == -1) {
        printf("ERROR: could not reach '%s' at port '%s'\n", host, port);
        closesocket(server_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    pthread_t thread_id;
    server_packet *server = (server_packet *)malloc(sizeof(server_packet));
    server->file_descriptor = server_socket;
    strcpy(server->host_name, host);
    strcpy(server->client_name, name);
    strcpy(server->command, command);
    server->port = port;
    if (pthread_create(&thread_id, NULL, &handle_command, server) != 0) {
        printf("ERROR: failed to create thread\n");
        closesocket(server_socket);
        free(server);
        WSACleanup();
        return EXIT_FAILURE;
    }

    pthread_join(thread_id, NULL);

    WSACleanup();
    return EXIT_SUCCESS;
}

