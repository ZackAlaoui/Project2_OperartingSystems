#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h> // corrected
#include "../include/functions.h"

#define TABLE_SIZE 100 // Adjust as needed

FILE * outputFile;

int lockCounter = 0;
int releaseCounter = 0;
long long timeStamp = 0.0;

typedef struct hash_struct {
    uint32_t hash;
    char name[50];
    uint32_t salary;
    struct hash_struct *next;
} hashRecord;

pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
hashRecord *hashTable[TABLE_SIZE] = {NULL};

long long current_timeStamp() {
    struct timeval te;
    gettimeofday(&te, NULL);
    return (te.tv_sec * 1000000LL) + te.tv_usec;
}

// Function to log messages to a file
// void log_lock_action(const char *action, const char *lock_type) {
//     long long timeStamp = current_timeStamp();
//     fprintf(logfile, "%lld: %s - %s\n", timeStamp, action, lock_type);
//     fflush(logfile);  // Make sure the message is written immediately to the file
// }

uint32_t jenkins_one_at_a_time_hash(const uint8_t *key, size_t length) {
    size_t i = 0;
    uint32_t hash = 0;
    while (i != length) {
        hash += key[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

void threads(int threadCount, char data[MAX_ROWS][MAX_WORDS_PER_ROW][MAX_LETTERS_PER_WORD]) {
    pthread_t *threadArray = malloc(threadCount * sizeof(pthread_t));
    hashRecord **record = malloc(threadCount * sizeof(hashRecord *));

    outputFile = fopen("output.txt", "a");

    if (!threadArray || !record) {
        perror("Memory allocation failed");
        free(threadArray);
        free(record);
        return;
    }

    for (int i = 0; i < threadCount + 1; i++) {
        //printf("%s\n", data[i+1][0]);
        record[i] = malloc(sizeof(hashRecord));
        if (!record[i]) {
            perror("Failed to allocate memory for record");
            continue;
        }

        void *(*routine)(void *) = NULL;

        if (strcmp(data[i+1][0], "insert") == 0) {
            strcpy(record[i]->name, data[i+1][1]);
            record[i]->salary = atoi(data[i+1][2]);
            routine = insert;
        } else if (strcmp(data[i+1][0], "search") == 0) {
            strcpy(record[i]->name, data[i+1][1]);
            routine = search;
        } else if (strcmp(data[i+1][0], "delete") == 0) {
            strcpy(record[i]->name, data[i+1][1]);
            routine = delete;
        } else {
            continue;
        }

        if (pthread_create(&threadArray[i], NULL, routine, record[i]) != 0) {
            perror("Failed to create thread");
            free(record[i]);
        }
    }

    long long timeStamp = current_timeStamp();
    printf("%lld: WAITING ON INSERTS\n", timeStamp);

    for (int i = 0; i < threadCount + 1; i++) {
        pthread_join(threadArray[i], NULL);
        free(record[i]);
    }

    fprintf(stdout, "Finished all threads.\n");

    print(0 ,0);

    free(threadArray);
    free(record);
}

void *insert(void *data) {
    //printf("Inside insert\n");

    hashRecord *record = (hashRecord *)data;
    uint32_t hash = jenkins_one_at_a_time_hash((const uint8_t *)record->name, strlen(record->name));
    uint32_t index = hash % TABLE_SIZE;

    long long timeStamp = current_timeStamp();
    fprintf(stdout, "%lld: INSERT,%u,%s,%u\n", timeStamp, hash, record->name, record->salary);
    fprintf(stdout, "%lld: WRITE LOCK ACQUIRED\n", timeStamp);
    lockCounter++;

    pthread_rwlock_wrlock(&rwlock);

    hashRecord *newRecord = malloc(sizeof(hashRecord));
    newRecord->hash = hash;
    strncpy(newRecord->name, record->name, sizeof(newRecord->name));
    newRecord->name[sizeof(newRecord->name) - 1] = '\0';
    newRecord->salary = record->salary;
    newRecord->next = hashTable[index]; // Chain to existing
    hashTable[index] = newRecord;

    timeStamp = current_timeStamp();
    fprintf(stdout, "%lld: WRITE LOCK RELEASED\n", timeStamp);
    releaseCounter++;

    pthread_rwlock_unlock(&rwlock);
    return NULL;
}

void *delete(void *data) {

    hashRecord *record = (hashRecord *)data;

    uint32_t hash = jenkins_one_at_a_time_hash((const uint8_t *)record->name, strlen(record->name));
    uint32_t index = hash % TABLE_SIZE;

    long long timeStamp = current_timeStamp();
    fprintf(stdout, "%lld: DELETE AWAKENED\n", timeStamp);

    timeStamp = current_timeStamp();
    fprintf(stdout, "%lld: WRITE LOCK ACQUIRED\n", timeStamp);
    lockCounter++;

    pthread_rwlock_wrlock(&rwlock);

    hashRecord *current = hashTable[index];
    hashRecord *prev = NULL;

    while (current != NULL) {
        if (current->hash == hash && strcmp(current->name, record->name) == 0) {
            if (prev == NULL)
                hashTable[index] = current->next;
            else
                prev->next = current->next;

            free(current);

            timeStamp = current_timeStamp();
            fprintf(stdout, "%lld: DELETE,%s\n", timeStamp, record->name);

            timeStamp = current_timeStamp();
            fprintf(stdout, "%lld: WRITE LOCK RELEASED\n", timeStamp);
            releaseCounter++;

            pthread_rwlock_unlock(&rwlock);
            return NULL;
        }
        prev = current;
        current = current->next;
    }

    timeStamp = current_timeStamp();
    fprintf(stdout, "%lld: WRITE LOCK RELEASED\n", timeStamp);
    releaseCounter++;

    pthread_rwlock_unlock(&rwlock);
    timeStamp = current_timeStamp();
    fprintf(stdout, "%lld: NOT FOUND,%s\n", timeStamp, record->name);
    return NULL;
}

void *search(void *data) {

    hashRecord *record = (hashRecord *)data;
    uint32_t index = jenkins_one_at_a_time_hash((const uint8_t *)record->name, strlen(record->name)) % TABLE_SIZE;

    long long timeStamp = current_timeStamp();
    fprintf(stdout, "%lld: READ LOCK ACQUIRED\n", timeStamp);
    pthread_rwlock_rdlock(&rwlock); //Acquire the reader lock
    lockCounter++;

    hashRecord *current = hashTable[index];

    while (current != NULL) {
        if (strcmp(current->name, record->name) == 0) {
            timeStamp = current_timeStamp();
            fprintf(stdout, "%lld: SEARCH: FOUND %s, Salary: %u\n", timeStamp, current->name, current->salary);
            fprintf(stdout, "%lld: READ LOCK RELEASED\n", timeStamp);
            releaseCounter++;
            pthread_rwlock_unlock(&rwlock);
            return NULL;
        }
        current = current->next;
    }

    timeStamp = current_timeStamp();
    fprintf(stdout, "%lld: SEARCH: NOT FOUND %s\n", timeStamp, record->name);
    fprintf(stdout, "%lld: READ LOCK RELEASED\n", timeStamp);
    releaseCounter++;

    pthread_rwlock_unlock(&rwlock);
    return NULL;
}


void print(const char* unused1, int unused2) {
    // Unused parameters
    (void)unused1;
    (void)unused2;
    
    FILE* output_file = fopen("output.txt", "a");

    fprintf(output_file, "Number of lock acquisitions: %d\n", lockCounter);
    fprintf(output_file, "Number of lock releases: %d\n", releaseCounter);


    if (output_file == NULL) {
        fprintf(stderr, "Error opening output file for printing hash table\n");
        return;
    }
    
    // Create an array to store all records for sorting
    hashRecord** records = NULL;
    int record_count = 0;
    
    // Acquire read lock to read the hash table safely
    timeStamp = current_timeStamp();
    //fprintf(output_file, "%lld: READ LOCK ACQUIRED\n", timeStamp);

    pthread_rwlock_rdlock(&rwlock);
    
    // First pass: count records to allocate array
    for (int i = 0; i < TABLE_SIZE; i++) {
        hashRecord* current = hashTable[i];
        while (current != NULL) {
            record_count++;
            current = current->next;
        }
    }
    
    if (record_count > 0) {
        // Allocate memory for pointers to all records
        records = (hashRecord**)malloc(record_count * sizeof(hashRecord*));
        if (records == NULL) {
            fprintf(stderr, "Memory allocation failed in print function\n");
            pthread_rwlock_unlock(&rwlock);
            fclose(output_file);
            return;
        }
        
        // Second pass: fill the array with pointers to all records
        int index = 0;
        for (int i = 0; i < TABLE_SIZE; i++) {
            hashRecord* current = hashTable[i];
            while (current != NULL) {
                records[index++] = current;
                current = current->next;
            }
        }
        
        // Sort records by hash value
        for (int i = 0; i < record_count - 1; i++) {
            for (int j = 0; j < record_count - i - 1; j++) {
                if (records[j]->hash > records[j + 1]->hash) {
                    // Swap pointers
                    hashRecord* temp = records[j];
                    records[j] = records[j + 1];
                    records[j + 1] = temp;
                }
            }
        }
        
        // Print sorted records
        for (int i = 0; i < record_count; i++) {
            fprintf(output_file, "%u,%s,%u\n", 
                    records[i]->hash, 
                    records[i]->name, 
                    records[i]->salary);
        }
        
        // Free the temporary array
        free(records);
    } else {
        // No records to print
        fprintf(output_file, "Hash table is empty\n");
    }
    
    // Release read lock
    pthread_rwlock_unlock(&rwlock);
    timeStamp = current_timeStamp();
    //fprintf(output_file, "%lld: READ LOCK RELEASED\n", timeStamp);
    
    fclose(output_file);
}
