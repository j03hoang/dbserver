#include "hashtable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int hash(const char *key) {
    unsigned long hash = 5381;
    unsigned char c;

    while ((c = *key++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash % TABLE_SIZE;
}

HashTable *ht_create() {
    HashTable *hashTable = malloc(sizeof(HashTable));

    for (int i = 0; i < TABLE_SIZE; i++)
        hashTable->entries[i] = NULL;

    hashTable->capacity = TABLE_SIZE;
    hashTable->length = 0;

    return hashTable;
}

void ht_destroy(HashTable *hashTable) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        Node *current = hashTable->entries[i];
        while (current) {
            Node *temp = current;
            current = current->next;
            free((void*)temp->key);
            free(temp);
        }
    }
    free(hashTable);
}

int put(HashTable *hashTable, const char *key, int value) {
    if (hashTable->length == hashTable->capacity)
        return -1;

    unsigned int index = hash(key);

    // check if key already exists and update the value if so
    Node *current = hashTable->entries[index];
    while (current) {
        if (strcmp(current->key, key) == 0) {
            current->value = value;
            return 0;
        }
        current = current->next;
    }

    Node *newNode = malloc(sizeof(Node)); // to be freed later
    newNode->key = strdup(key);
    newNode->value = value;
    newNode->next = hashTable->entries[index];
    hashTable->entries[index] = newNode;

    hashTable->length++;
    return 0;
}

int get(HashTable *hashTable, const char *key) {
    unsigned int index = hash(key);
    Node *current = hashTable->entries[index];
    while (current) {
        if (strcmp(current->key, key) == 0)
            return current->value;
        current = current->next;
    }
    return -1;
}

int remove_entry(HashTable *hashTable, const char *key) {
    unsigned int index = hash(key);
    Node *head = hashTable->entries[index];

    if (!head)
        return -1;


    if (strcmp(head->key, key) == 0) {
        Node *temp = head;
        hashTable->entries[index] = head->next;
        free((void*)temp->key);
        free(temp);
        return 0;
    }

    Node *prev = NULL;
    Node *current = head;

    while (current && strcmp(current->key, key) != 0) {
        prev = current;
        current = current->next;
    }

    if (!current)
        return -1;

    prev->next = current->next;
    free((void*)current->key);
    free(current);
    hashTable->length--;
    return 0;
}

