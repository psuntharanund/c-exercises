#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>

void startProgram();
void chooseOperation(int choice);
void createUser(char *name, int *ID);
void readUser(char *name, int *ID);
void updateUser(char *name, int *ID);
void deleteUser(char *name, int *ID);
