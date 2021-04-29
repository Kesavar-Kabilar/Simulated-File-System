#include <stdio.h>
#include <stdlib.h>

int main(void){
    char a[10];
    fgets(a, 10, stdin);
    printf("%s\n", a);
}