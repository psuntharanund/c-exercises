#include <stdio.h>

void print_left(int row, int height);
void print_right(int row, int height);

int main(void)
{
    int height;
    do
    {
        printf("How many tall would you like to make the pyramid? (1 - 8): ");
        scanf(" %i", &height);
    } while (height <= 1 || height >= 8);
    
    for (int row = 1; row <= height; row++)
    {
        print_left(row, height);
        print_right(row, height);
    } 
}

void print_left(int row, int height)
{
    for (int spaces_left = 1; spaces_left <= height - row; spaces_left++)
    {
        printf(" ");
    }
    for (int blocks_left = 1; blocks_left <= row; blocks_left++)
    {
        printf("#");
    }
    for (int space_middle = 0; space_middle < 2; space_middle++)
    {
        printf(" ");
    }
}

void print_right(int row, int height)
{
    for (int block_right = 1; block_right <= row; block_right++)
    {
        printf("#");
    }
    for (int spaces_right = 1; spaces_right <= height - row; spaces_right++)
    {
        printf(" ");
    }
    printf("\n");

}

