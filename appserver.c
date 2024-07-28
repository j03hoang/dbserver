#include <winsock2.h>
#include <ws2tcpip.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 128

static volatile int server_running = 1;

typedef struct {
    SOCKET file_descriptor;
    char name[BUFFER_SIZE];
    int port;
} server_packet;

server_packet *dbserver;

void catch_ctrl_c_and_exit(int dummy) {
    server_running = 0;
}

int is_command_valid(char *command) {
    char command_copy[BUFFER_SIZE];
    strncpy(command_copy, command, BUFFER_SIZE);
    char *prompt = strtok(command_copy, " ");
    if (prompt == NULL) {
        return 0;
    } else if (strcmp(prompt, "list") == 0) {
        return 1;
    } else if (strcmp(prompt, "put") == 0) {
        char *key = strtok(NULL, " ");
        char *value = strtok(NULL, " ");
        char *extra = strtok(NULL, " ");
        if (key != NULL && value != NULL && extra == NULL) {
            return 1;
        }
    } else if (strcmp(prompt, "get") == 0) {
        char *key = strtok(NULL, " ");
        char *extra = strtok(NULL, " ");
        if (key != NULL && extra == NULL) {
            return 1;
        }
    } else if (strcmp(prompt, "remove") == 0) {
        char *key = strtok(NULL, " ");
        char *extra = strtok(NULL, " ");
        if (key != NULL && extra == NULL) {
            return 1;
        }
    }
    return 0;
}

void *handle_command(void *arg) {
    SOCKET client_socket = *(SOCKET *)arg;
    char command_buffer[BUFFER_SIZE];
    char name_buffer[BUFFER_SIZE];
    char print_buffer[BUFFER_SIZE];
    char db_response[BUFFER_SIZE];

    recv(client_socket, name_buffer, BUFFER_SIZE, 0);
    recv(client_socket, command_buffer, BUFFER_SIZE, 0);

    sprintf(print_buffer,"received command from '%s': '%s',", name_buffer, command_buffer);

    if (!is_command_valid(command_buffer)) {
        strcat(print_buffer, " but it was not valid\n");
        printf("%s", print_buffer);
        send(client_socket, "ERROR: invalid command", BUFFER_SIZE, 0);
    } else {
        strcat(print_buffer, " and sending to database server\n");
        printf("%s", print_buffer);
        send(dbserver->file_descriptor, name_buffer, strlen(name_buffer), 0);
        send(dbserver->file_descriptor, command_buffer, strlen(command_buffer), 0);
        recv(dbserver->file_descriptor, db_response, sizeof(db_response), 0);
        send(client_socket, db_response, strlen(db_response), 0);
    }


    closesocket(client_socket);
    pthread_detach(pthread_self());
    return NULL;
}

int is_args_valid(int argc, char **argv) {
    if (argc != 5) {
        printf("usage: ./appserver <listening_port> <nane> <host> <port> \n");
        return 0;
    }

    char *port_str = argv[4];
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
        printf("boom");
        return EXIT_FAILURE;
    }

    signal(SIGINT, catch_ctrl_c_and_exit);

    WSADATA wsaData; // needed for windows
    WSAStartup(0x202, &wsaData);

    char *ip = "127.0.0.1";
    int listening_port = atoi(argv[1]);
    char *name = argv[2];
    char *db_name = argv[3];
    int db_port = atoi(argv[4]);

    dbserver = (server_packet *)malloc(sizeof(server_packet ));

    SOCKET host_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in host_addr;
    host_addr.sin_family = AF_INET;
    host_addr.sin_addr.s_addr = inet_addr(ip);
    host_addr.sin_port = htons(db_port);

    int err = connect(host_socket, (struct sockaddr*)&host_addr, sizeof(host_addr));
    if (err == -1) {
        printf("ERROR: could not reach '%s' at port '%d'\n", db_name, db_port);
        closesocket(host_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    printf("connected to %s at %d\n", db_name, db_port);
    dbserver->file_descriptor = host_socket;
    strcpy(dbserver->name, db_name);
    dbserver->port = db_port;


    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(listening_port);

    if (bind(server_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("ERROR: the port %d is already in use\n", listening_port);
        return EXIT_FAILURE;
    }

    if (listen(server_socket, 10) < 0) {
        printf("ERROR: listen \n");
        return EXIT_FAILURE;
    }

    printf("waiting for incoming command...\n");
    pthread_t thread_id;

    while (server_running) {
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);

        int activity = select(server_socket, &read_fds, NULL, NULL, &timeout);

        if (activity > 0 && FD_ISSET(server_socket, &read_fds)) {
            struct sockaddr_in cli_addr;
            socklen_t cli_len = sizeof(cli_addr);
            SOCKET *client_socket = malloc(sizeof(SOCKET));
            *client_socket = accept(server_socket, (struct sockaddr*)&cli_addr, &cli_len);

            pthread_create(&thread_id, NULL, &handle_command, client_socket);
        }
    }

    closesocket(host_socket);
    closesocket(server_socket);
    WSACleanup();
    free(dbserver);
    printf("Server successfully shutdown\n");
    return EXIT_SUCCESS;

}