#include <stdio.h>

int main(void)
{
    char answer[50];

    printf("Enter your name: ");
    scanf("%49s", answer);

    printf("Hello, %s", answer);
    
}
