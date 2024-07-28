#ifndef HASHTABLE_H
#define HASHTABLE_H

#define TABLE_SIZE 20 // assignment specification

typedef struct Node {
    const char *key;
    int value;
    struct Node *next;
} Node;

typedef struct HashTable {
    Node *entries[TABLE_SIZE];
    unsigned int capacity;
    unsigned int length;
} HashTable;

unsigned int hash(const char *key);
HashTable *ht_create();
int put(HashTable *hashTable, const char *key, int value);
int get(HashTable *hashTable, const char *key);
int remove_entry(HashTable *hashTable, const char *key);
void ht_destroy(HashTable *hashTable);

#endif
