#include <winsock2.h>
#include <ws2tcpip.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "hashtable.h"

#define BUFFER_SIZE 128
#define NAME_SIZE 128

HashTable *kv_database;

static volatile int server_running = 1;

typedef struct {
    SOCKET file_descriptor;
    struct sockaddr_in address;
    char name[NAME_SIZE];
    char command[BUFFER_SIZE];
} client_packet;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void catch_ctrl_c_and_exit(int dummy) {
    server_running = 0;
}

void list_items(client_packet *client) {
    pthread_mutex_lock(&clients_mutex);
    char buffer[BUFFER_SIZE];
    sprintf(buffer, "passed: list\n");
    for (int i = 0; i < TABLE_SIZE; i++) {
        Node *current = kv_database->entries[i];
        while(current) {
            strcat(buffer, current->key);
            char value[12];
            sprintf(value, " %d\n", current->value);
            strcat(buffer, value);
            current = current->next;
        }
    }
    send(client->file_descriptor, buffer, strlen(buffer), 0);

    pthread_mutex_unlock(&clients_mutex);
}

void put_item(client_packet *client, char *key, char *value) {
    pthread_mutex_lock(&clients_mutex);
    char buffer[BUFFER_SIZE];

    if (put(kv_database, key, atoi(value)) != 0) {
        printf("database is full capacity\n");
        sprintf(buffer,
                "database is full capacity for the command from '%s' '%s'\n", client->name, client->command);
    } else {
        sprintf(buffer,
                "database server processed the command from '%s' '%s'\n", client->name, client->command);
    }
    send(client->file_descriptor, buffer, strlen(buffer), 0);
    pthread_mutex_unlock(&clients_mutex);
}

void get_item(client_packet *client, char *key) {
    pthread_mutex_lock(&clients_mutex);
    char buffer[BUFFER_SIZE];
    int value = get(kv_database, key);

    if (value == -1) {
        sprintf(buffer, "failed: get %s, '%s' is not in the database\n", key, key);
    } else {
        sprintf(buffer, "passed: get %s -> %d\n", key, value);
    }

    send(client->file_descriptor, buffer, strlen(buffer), 0);
    pthread_mutex_unlock(&clients_mutex);
}

void remove_item(client_packet *client, char *key) {
    pthread_mutex_lock(&clients_mutex);
    char buffer[BUFFER_SIZE];

    if (remove_entry(kv_database, key) == -1) {
        sprintf(buffer, "failed: remove %s, '%s' is not found in the database\n", key, key);
    } else {
        sprintf(buffer, "passed: remove %s\n", key);
    }

    send(client->file_descriptor, buffer, strlen(buffer), 0);
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_message(void *arg) {
    client_packet *client = (client_packet*) arg;
    char name_buffer[BUFFER_SIZE];
    char command_buffer[BUFFER_SIZE];
    int bytes_received;

    while (server_running) {
        bytes_received = recv(client->file_descriptor, name_buffer, sizeof(name_buffer), 0);
        if (bytes_received > 0) {
            name_buffer[bytes_received] = '\0';
            strtok(name_buffer, "\n");
            strcpy(client->name, name_buffer);
        }

        bytes_received = recv(client->file_descriptor, command_buffer, sizeof(command_buffer) - 1, 0);
        if (bytes_received > 0) {
            command_buffer[bytes_received] = '\0';
            strtok(command_buffer, "\n");
            strcpy(client->command, command_buffer);
        }

        printf("received command from %s: %s\n", name_buffer, command_buffer);

        char *prompt = strtok(command_buffer, " ");
        if (strcmp(prompt, "list") == 0) {
            list_items(client);
        } else if (strcmp(prompt, "put") == 0) {
            char *key = strtok(NULL, " ");
            char *value = strtok(NULL, "\n");
            put_item(client, key, value);
        } else if (strcmp(prompt, "get") == 0) {
            char *key = strtok(NULL, "\n");
            get_item(client, key);
        } else if (strcmp(prompt, "remove") == 0) {
            char *key = strtok(NULL, "\n");
            remove_item(client, key);
        } else {
            char error_message[BUFFER_SIZE];
            sprintf(error_message, "failed: the command '%s' was not found in the database\n", prompt);
            send(client->file_descriptor, error_message, strlen(error_message), 0);
        }
        memset(name_buffer, '\0', NAME_SIZE);
        memset(command_buffer, '\0', BUFFER_SIZE);
    }

    closesocket(client->file_descriptor);
    free(client);
    pthread_detach(pthread_self());
    return NULL;

}

int is_args_valid(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: ./server <port> \n");
        return 0;
    }

    char *port_str = argv[1];
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

    signal(SIGINT, catch_ctrl_c_and_exit);

    WSADATA wsaData; // needed for windows
    WSAStartup(0x202, &wsaData);

    char *ip = "127.0.0.1";
    int port = atoi(argv[1]);

    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("ERROR: the port %d is already in use\n", port);
        closesocket(server_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    if (listen(server_socket, 10) < 0) {
        printf("ERROR: listen \n");
        closesocket(server_socket);
        WSACleanup();
        return EXIT_FAILURE;
    }

    printf("waiting for incoming command...\n");
    pthread_t thread_id;
    kv_database = ht_create();

    while (server_running) {
        // https://www.mkssoftware.com/docs/man3/select.3.asp for file_descriptor-accept to properly interact with signal
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
            SOCKET client_socket = accept(server_socket, (struct sockaddr*)&cli_addr, &cli_len);
            client_packet *client = (client_packet *)malloc(sizeof(client_packet));
            client->file_descriptor = client_socket;
            client->address = cli_addr;

            pthread_create(&thread_id, NULL, &handle_message, client);
        }
    }

    ht_destroy(kv_database);
    closesocket(server_socket);
    WSACleanup();
    printf("Server successfully shutdown\n");
    return EXIT_SUCCESS;
}