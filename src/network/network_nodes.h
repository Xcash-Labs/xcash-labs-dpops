#ifndef NETWORK_NODES_H
#define NETWORK_NODES_H

// Define a struct to store network node data
typedef struct {
    const char *public_address;
    const char *ip_address;
    const char *public_key;
} NetworkNode;

// Declare an external array of nodes (variable size, terminated with NULL)
extern const NetworkNode network_nodes[];

#endif // NETWORK_NODES_H