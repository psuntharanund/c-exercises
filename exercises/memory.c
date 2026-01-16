#include <stdio.h>
#include <stdlib.h>

int* generateArray(int size){
    int* array= malloc(sizeof(int) * size);
    for (int i = 0; i < size; i++){
        array[i] = i;
    }
    return array;
}
int main(){
    //size --> What do we put?
    //void* --> How can we get a stronger type?
    int count = 0;
    int size = 10;
    int* array = generateArray(size);

    if (array == NULL){
        printf("Memory allocation failed.\n");
        return 1;
    }
    
    size*=2;
    int* array2 = realloc(array, sizeof(int) * size);
    
    if (array2 == NULL){
        printf("New Memory allocation failed...\n");
        return 1;
    } else {
        array = array2;
    }

    for (int i = 0; i < size / 2; i++){
        array[i] = i;
        count++;
    }

    for (int i = 0; i < count; i++){
        printf("%i ", array[i]);
    }

    printf("\n");

    printf("size: %i, count %i\n", size, count);

    free(array);

    return 0;
}