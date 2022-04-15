#include <stdio.h>
#include <stdlib.h>

int main() {

    char *ptr = malloc(32);

    scanf("%s", ptr);

    printf("%s", ptr);

    return 0;
}