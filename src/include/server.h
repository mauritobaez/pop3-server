#ifndef SERVER_H
#define SERVER_H

typedef struct server_metrics {
    size_t total_pop3_connections;
    size_t max_concurrent_pop3_connections;
    size_t current_concurrent_pop3_connections;
    size_t sent_bytes;
    size_t received_bytes;
    size_t emails_read;
    size_t emails_removed;
} server_info;

typedef struct server_config {
    size_t max_connections;
    size_t polling_timeout;
} server_config;

#endif