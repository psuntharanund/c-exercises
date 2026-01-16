#include <stdio.h>

void print_row(int row);

int main(void)
{
    int rows;
    printf("Rows: ");
    scanf(" %i", &rows);
    for (int i = 0; i < rows; i++)
    {
        print_row(i + 1);
    }
}

void print_row(int n)
{
    for (int i = 0; i < n; i++)
    {
        printf("#");
    }
    printf("\n");
}
