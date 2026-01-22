#include <stdio.h>
#include "test_functs.h"

int main(void)
{
    int times = get_pos_int();
    meow(times);

}

int get_pos_int(void)
{
    int n;
    do
    {
        printf("Please enter a number: ");
        scanf(" %i", &n);
    }
    while (n < 1);
    return n;
}

void meow(int n)
{
    for (int i = 0; i < n; i++)
    {
        printf("meow\n");
    }
}

