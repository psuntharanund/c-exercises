// In case you want to implement the shared memory IPC as a library
// This is optional but may help with code reuse
//
#include "handle_with_cache.c"

#define MSG_SIZE 1028

typedef struct {
    char message[MSG_SIZE];
} shared_data_t;


