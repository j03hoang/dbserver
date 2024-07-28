#include <stdio.h>
#include "hashtable.h"
#include <string.h>

int main(void) {
    printf("Hello, World!\n");


    HashTable *table = ht_create();
    put(table, "apple", 5);
    put(table, "pie", 5);
    put(table, "carrot", 5);
    printf("%d\n", get(table, "carrot"));
    ht_destroy(table);

    return 0;
}
