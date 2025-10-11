#include <stdio.h>

void print_row(int row, int height);

int main(void)
{
    int height;
    printf("How many rows do you want: ");
    scanf(" %i", &height);
    for (int row = 1; row <= height; row++)
    {
        print_row(row, height);
    } 
}

void print_row(int row, int height)
{
    for (int spaces = 1; spaces <= height - row; spaces++)
    {
        printf(" ");
    }
    for (int block = 1; block <= row; block++)
    {
        printf("#");
    }
    printf("\n");
}
