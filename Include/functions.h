#include <stdint.h>

// functions.h
#ifndef FUNCTIONS_H
#define FUNCTIONS_H
#define MAX_ROWS 11         // Number of rows in the file
#define MAX_WORDS_PER_ROW 3 // Words per row
#define MAX_LETTERS_PER_WORD 50
#endif


typedef struct hash_struct hashRecord;

// Insert(key, value)
void insert(void *data);

//This function is responsible from removing a name from the list
void deleteRecord(void * name);

//Seaches for a key data pair in the hash table and returns the value if it exists.
void search(const void *key);

//Hash function
uint32_t jenkins_one_at_a_time_hash(const uint8_t *key, size_t length);

//Print function
void print();

//Thread function
void threads(int threadCount, char data[MAX_ROWS][MAX_WORDS_PER_ROW][MAX_LETTERS_PER_WORD]);

//Get current time function
long long current_timestamp();
