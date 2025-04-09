#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "sys/time.h"
#include "../include/functions.h"

#define TABLE_SIZE 100 // Adjust as needed

pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;

hashRecord *hashTable[TABLE_SIZE] = {NULL};

long long current_timestamp()
{
    struct timeval te;
    gettimeofday(&te, NULL);                                     // get current time
    long long microseconds = (te.tv_sec * 1000000) + te.tv_usec; // calculate milliseconds
    return microseconds;
}

// Jenkins hash function
uint32_t jenkins_one_at_a_time_hash(const uint8_t *key, size_t length)
{
    size_t i = 0;
    uint32_t hash = 0;
    while (i != length)
    {
        hash += key[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

typedef struct hash_struct
{
    uint32_t hash;
    char name[50];
    uint32_t salary;
    struct hash_struct *next;
} hashRecord;

void threads(int threadCount, char data[MAX_ROWS][MAX_WORDS_PER_ROW][MAX_LETTERS_PER_WORD])
{
    pthread_t *threadArray = (pthread_t *)malloc(threadCount * sizeof(pthread_t));
    hashRecord **record = (hashRecord **)malloc(threadCount * sizeof(hashRecord *)); // Array of pointers

    if (threadArray == NULL || record == NULL)
    {
        perror("Failed to allocate memory.");
        free(threadArray);
        free(record);
        return;
    }
    else
    {
        for (int i = 1; i <= threadCount; i++)
        {
            if (data[i][0] == NULL || data[i][1] == NULL || data[i][2] == NULL)
            {
                perror("Invalid data format");
                continue;
            }

            record[i] = malloc(sizeof(hashRecord));

            if (record[i] == NULL)
            {
                perror("Failed to allocate memory for record");
                continue;
            }

            void *routine; // Declaring a function pointer here

            if (strcmp(data[i][1], "insert") == 0)
            {
                strcpy(record[i]->name, data[i][1]);
                record[i]->salary = atoi(data[i][2]); // This converts the salary string into an integer
                routine = insert;
            }
            else if (strcmp(data[i][1], "search") == 0)
            {
                strcpy(record[i]->name, data[i][1]);
                routine = search;
            }
            else if (strcmp(data[i][1], "delete") == 0)
            {
                strcpy(record[i]->name, data[i][1]);
                routine = deleteRecord;
            }
            else
            {
                continue;
            }

            if (pthread_create(&threadArray[i], NULL, routine, record[i]) != 0)
            {
                perror("Failed to create thread");
                continue;
            }
        }

        for (int i = 1; i <= threadCount; i++)
        {
            pthread_join(threadArray[i], NULL);
            free(record[i]);
        }

        free(threadArray);
        //print();
    }
}

// Insert(key, value)
void insert(void *data)
{
    hashRecord *record = (hashRecord *)data;

    uint32_t hash = jenkins_one_at_a_time_hash((const uint8_t *)record->name, strlen(record->name));
    uint32_t index = hash % TABLE_SIZE;

    if (pthread_rwlock_wrlock(&rwlock) == 0) // acquire write lock
    {
        hashRecord *newRecord = malloc(sizeof(hashRecord));
        newRecord->hash = hash;
        strncpy(newRecord->name, record->name, sizeof(newRecord->name));
        newRecord->name[sizeof(newRecord->name) - 1] = '\0'; // ensure null-termination
        newRecord->salary = record->salary;
        newRecord->next = NULL;
        hashTable[index] = newRecord;

        pthread_rwlock_unlock(&rwlock); // release lock
    }
    else
    {
        perror("Write lock cannot be acquired!");
        return;
    }
}

// Thread-safe delete function
void deleteRecord(void *data)
{
    //(void)unused; // Explicitly ignore the second parameter
    hashRecord *record = (hashRecord *)data;

    uint32_t hash = jenkins_one_at_a_time_hash((const uint8_t *)record->name, strlen(record->name));
    size_t index = hash % TABLE_SIZE; // Get bucket index

    // Acquire writer lock before modifying the table
    pthread_rwlock_wrlock(&rwlock);

    hashRecord *current = hashTable[index];
    hashRecord *prev = NULL;

    while (current != NULL)
    {
        if (current->hash == hash && strcmp(current->name, record->name) == 0)
        {
            // Found the record, remove it
            if (prev == NULL)
            {
                hashTable[index] = current->next; // Removing head
            }
            else
            {
                prev->next = current->next; // Removing middle/end node
            }
            free(current);
            printf("Deleted record with key: %s\n", record->name);

            // Release write lock and return
            pthread_rwlock_unlock(&rwlock);
            return;
        }
        prev = current;
        current = current->next;
    }

    // Release write lock if key was not found
    pthread_rwlock_unlock(&rwlock);
    printf("Key not found: %s\n", record->name);
}

void search(const void *key)
{
    hashRecord *record = (hashRecord *)key;

    uint32_t index = jenkins_one_at_a_time_hash(record->name, strlen(record->name)); // Compute the hash value for the key, key is the param <name>
    pthread_rwlock_rdlock(&rwlock);                                                  // Acquire the reader lock

    hashRecord *current = hashTable[index]; // Traverse the linked list

    while (current != NULL)
    {
        if (strcmp(current->name, record->name) == 0)
        {                                   // If the key matches
            pthread_rwlock_unlock(&rwlock); // Release the lock
            print();
            // return current->salary;                 // Return the associated value
        }
        current = current->next; // Move to the next record
    }

    pthread_rwlock_unlock(&rwlock); // Release the lock if key is not found                              // Return NULL if the key is not found
}

void print()
{

    FILE *output_file = fopen("./output.txt", "a");
    if (output_file == NULL)
    {
        fprintf(stderr, "Error opening output file for printing hash table\n");
        return;
    }

    // Get current timestamp
    long long timestamp = current_timestamp();
    fprintf(output_file, "%lld: PRINT\n", timestamp);

    // Create an array to store all records for sorting
    hashRecord **records = NULL;
    int record_count = 0;

    // Acquire read lock to read the hash table safely
    timestamp = current_timestamp();
    fprintf(output_file, "%lld: READ LOCK ACQUIRED\n", timestamp);
    pthread_rwlock_rdlock(&rwlock);

    // First pass: count records to allocate array
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        hashRecord *current = hashTable[i];
        while (current != NULL)
        {
            record_count++;
            current = current->next;
        }
    }

    if (record_count > 0)
    {
        // Allocate memory for pointers to all records
        records = (hashRecord **)malloc(record_count * sizeof(hashRecord *));
        if (records == NULL)
        {
            fprintf(stderr, "Memory allocation failed in print function\n");
            pthread_rwlock_unlock(&rwlock);
            fclose(output_file);
            return;
        }

        // Second pass: fill the array with pointers to all records
        int index = 0;
        for (int i = 0; i < TABLE_SIZE; i++)
        {
            hashRecord *current = hashTable[i];
            while (current != NULL)
            {
                records[index++] = current;
                current = current->next;
            }
        }

        // Sort records by hash value
        for (int i = 0; i < record_count - 1; i++)
        {
            for (int j = 0; j < record_count - i - 1; j++)
            {
                if (records[j]->hash > records[j + 1]->hash)
                {
                    // Swap pointers
                    hashRecord *temp = records[j];
                    records[j] = records[j + 1];
                    records[j + 1] = temp;
                }
            }
        }

        // Print sorted records
        for (int i = 0; i < record_count; i++)
        {
            fprintf(output_file, "%u,%s,%u\n",
                    records[i]->hash,
                    records[i]->name,
                    records[i]->salary);
        }

        // Free the temporary array
        free(records);
    }
    else
    {
        // No records to print
        fprintf(output_file, "Hash table is empty\n");
    }

    // Release read lock
    pthread_rwlock_unlock(&rwlock);
    timestamp = current_timestamp();
    fprintf(output_file, "%lld: READ LOCK RELEASED\n", timestamp);

    fclose(output_file);
}
