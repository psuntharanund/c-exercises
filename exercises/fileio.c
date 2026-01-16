#include <stdio.h>

int main(void){
    FILE *out = fopen("filetest.txt","w");
    
    if (!out) {
        return 1;
    }

    fprintf(out, "This is a test.\n");
    fprintf(out, "You can do this man! I believe!");

    fclose(out);
}