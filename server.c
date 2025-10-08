
 /*
 *   Socket path: /tmp/kvstore.sock
 */

#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define SOCKET_PATH "/tmp/kvstore.sock" // Unix socket path
#define BACKLOG 10                      // Max pending connections
#define BUF_SIZE 256                    // Buffer size for read/write
#define MAX_ENTRIES 100                 // Max key-value entries

// Structure for storing one key-value pair
typedef struct
{
    char key[BUF_SIZE];
    char value[BUF_SIZE];
} keyvalue;

// In-memory key-value store
static keyvalue store[MAX_ENTRIES];
static int store_count = 0;

/* --------------------- Key-Value Store Functions --------------------- */

// Find value by key
const char *kv_get(const char *key)
{
    for (int i = 0; i < store_count; i++)
    {
        if (strcmp(store[i].key, key) == 0)
            return store[i].value;
    }
    return NULL; // Not found
}

// Set value by key (updates if key exists, adds new otherwise)
void kv_set(const char *key, const char *value)
{
    for (int i = 0; i < store_count; i++)
    {
        if (strcmp(store[i].key, key) == 0)
        {
            strncpy(store[i].value, value, BUF_SIZE - 1);
            store[i].value[BUF_SIZE - 1] = '\0';
            return;
        }
    }

    // If key not found, create new entry
    if (store_count < MAX_ENTRIES)
    {
        strncpy(store[store_count].key, key, BUF_SIZE - 1);
        strncpy(store[store_count].value, value, BUF_SIZE - 1);
        store[store_count].key[BUF_SIZE - 1] = '\0';
        store[store_count].value[BUF_SIZE - 1] = '\0';
        store_count++;
    }
}

// Exit with error and cleanup socket
static void die(const char *msg)
{
    perror(msg);
    unlink(SOCKET_PATH);
    exit(EXIT_FAILURE);
}

/* --------------------- Main Server Logic --------------------- */

int main(void)
{
    int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd == -1){
        die("socket");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    // Remove old socket if exists
    unlink(SOCKET_PATH);

    // Bind socket to file path
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1){
        die("bind");
    }

    // Start listening for connections
    if (listen(listen_fd, BACKLOG) == -1){
        die("listen");
    }

    printf("KV Store server listening on %s\n", SOCKET_PATH);

    while (1)
    {
        // Accept a new client
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd == -1)
        {
            if (errno == EINTR)
                continue; // Retry if interrupted
            die("accept");
        }

        // Read client message
        char buf[BUF_SIZE];
        ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
        if (n > 0)
        {
            buf[n] = '\0';
            char key[BUF_SIZE], value[BUF_SIZE];

            // Handle SET command
            if (sscanf(buf, "SET %s %[^\n]", key, value) == 2)
            {
                kv_set(key, value);
                write(client_fd, "OK\n", 3);
            }
            // Handle GET command
            else if (sscanf(buf, "GET %s", key) == 1)
            {
                const char *val = kv_get(key);
                if (val)
                    dprintf(client_fd, "%s\n", val);
                else
                    write(client_fd, "NOT_FOUND\n", 10);
            }
            else
            {
                write(client_fd, "ERROR\n", 6);
            }
        }

        close(client_fd); // Close connection after each command
    }

    close(listen_fd);
    unlink(SOCKET_PATH);
    return 0;
}
