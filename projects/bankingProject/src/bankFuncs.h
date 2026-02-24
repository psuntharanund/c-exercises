#include "headers.h"

typedef struct operation_t operation_t;

void startProgram();
void chooseOperation(int choice);
void createUser(char *name, int *ID);
void readUser(char *name, int *ID);
void updateUser(char *name, int *ID);
void deleteUser(char *name, int *ID);
