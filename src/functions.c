#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h> // corrected
#include "../Include/functions.h"

#define TABLE_SIZE 100 // Adjust as needed

FILE *outputFile;

pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int lockCounter = 0;
int releaseCounter = 0;
long long timeStamp = 0.0;

typedef struct hash_struct
{
    uint32_t hash;
    char name[50];
    uint32_t salary;
    struct hash_struct *next;
} hashRecord;

int insertCompleted[TABLE_SIZE] = {0}; // Track completion for each index
hashRecord *hashTable[TABLE_SIZE] = {NULL};

long long current_timeStamp()
{
    struct timeval te;
    gettimeofday(&te, NULL);
    return (te.tv_sec * 1000000LL) + te.tv_usec;
}

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

void threads(int threadCount, char data[MAX_ROWS][MAX_WORDS_PER_ROW][MAX_LETTERS_PER_WORD])
{
    pthread_t *insertThreads = malloc(threadCount * sizeof(pthread_t));
    pthread_t *otherThreads = malloc(threadCount * sizeof(pthread_t));
    hashRecord **insertRecords = malloc(threadCount * sizeof(hashRecord *));
    hashRecord **otherRecords = malloc(threadCount * sizeof(hashRecord *));

    int insertCount = 0, otherCount = 0;

    outputFile = fopen("output.txt", "a");
    fprintf(outputFile, "Running %d threads\n", threadCount);

    // free(otherThreads);
    // free(insertRecords);
    // free(otherRecords);

    // Classify commands
    for (int i = 1; i <= threadCount; i++)
    {
        hashRecord *rec = malloc(sizeof(hashRecord));
        if (!rec)
            continue;

        void *(*routine)(void *) = NULL;
        if (strcmp(data[i][0], "insert") == 0)
        {
            strcpy(rec->name, data[i][1]);
            rec->salary = atoi(data[i][2]);
            routine = insert;
            insertRecords[insertCount] = rec;
            pthread_create(&insertThreads[insertCount], NULL, routine, rec);
            insertCount++;
        }
        else
        {
            strcpy(rec->name, data[i][1]);
            if (strcmp(data[i][0], "delete") == 0)
            {
                routine = delete;
            }
            else if (strcmp(data[i][0], "search") == 0)
            {
                routine = search;
            }
            otherRecords[otherCount] = rec;
            pthread_create(&otherThreads[otherCount], NULL, routine, rec);
            otherCount++;
        }
    }

    // Join insert threads
    for (int i = 0; i < insertCount; i++)
    {
        pthread_join(insertThreads[i], NULL);
        free(insertRecords[i]);
    }

    long long timeStamp = current_timeStamp();
    fprintf(outputFile, "%lld: WAITING ON INSERTS\n", timeStamp);
    fflush(outputFile);

    //Join delete and search threads
    for (int i = 0; i < otherCount; i++)
    {
        pthread_join(otherThreads[i], NULL);
        free(otherRecords[i]);
    }

    fprintf(outputFile, "Finished all threads.\n");
    fflush(outputFile);

    print(0, 0);

    free(insertThreads);
    free(otherThreads);
    free(insertRecords);
    free(otherRecords);
}

void *insert(void *data)
{

    hashRecord *record = (hashRecord *)data;
    uint32_t hash = jenkins_one_at_a_time_hash((const uint8_t *)record->name, strlen(record->name));
    uint32_t index = hash % TABLE_SIZE;

    long long timeStamp = current_timeStamp();
    fprintf(outputFile, "%lld: INSERT,%u,%s,%u\n", timeStamp, hash, record->name, record->salary);
    fflush(outputFile);

    fprintf(outputFile, "%lld: WRITE LOCK ACQUIRED\n", timeStamp);
    fflush(outputFile);
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
    fprintf(outputFile, "%lld: WRITE LOCK RELEASED\n", timeStamp);
    fflush(outputFile);
    releaseCounter++;

    pthread_rwlock_unlock(&rwlock);

    pthread_mutex_lock(&mutex);
    insertCompleted[index] = 1;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);

    return NULL;
}

void *delete(void *data)
{

    hashRecord *record = (hashRecord *)data;

    uint32_t hash = jenkins_one_at_a_time_hash((const uint8_t *)record->name, strlen(record->name));
    uint32_t index = hash % TABLE_SIZE;

    pthread_mutex_lock(&mutex);
    while (insertCompleted[index] == 0)
    {
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);
    
    long long timeStamp = current_timeStamp();
    fprintf(outputFile, "%lld: DELETE AWAKENED\n", timeStamp);

    timeStamp = current_timeStamp();
    fprintf(outputFile, "%lld: WRITE LOCK ACQUIRED\n", timeStamp);
    fflush(outputFile);
    lockCounter++;

    pthread_rwlock_wrlock(&rwlock);

    hashRecord *current = hashTable[index];
    hashRecord *prev = NULL;

    while (current != NULL)
    {
        if (current->hash == hash && strcmp(current->name, record->name) == 0)
        {
            if (prev == NULL)
                hashTable[index] = current->next;
            else
                prev->next = current->next;

            free(current);

            timeStamp = current_timeStamp();
            fprintf(outputFile, "%lld: DELETE,%s\n", timeStamp, record->name);

            timeStamp = current_timeStamp();
            fprintf(outputFile, "%lld: WRITE LOCK RELEASED\n", timeStamp);
            fflush(outputFile);
            releaseCounter++;

            pthread_rwlock_unlock(&rwlock);
            return NULL;
        }
        prev = current;
        current = current->next;
    }

    timeStamp = current_timeStamp();
    fprintf(outputFile, "%lld: WRITE LOCK RELEASED\n", timeStamp);
    fflush(outputFile);
    releaseCounter++;

    pthread_rwlock_unlock(&rwlock);
    timeStamp = current_timeStamp();
    fprintf(outputFile, "%lld: NOT FOUND,%s\n", timeStamp, record->name);
    fflush(outputFile);
    return NULL;
}

void *search(void *data)
{

    hashRecord *record = (hashRecord *)data;
    uint32_t index = jenkins_one_at_a_time_hash((const uint8_t *)record->name, strlen(record->name)) % TABLE_SIZE;

    pthread_rwlock_rdlock(&rwlock);
    lockCounter++;

    long long timeStamp = current_timeStamp();
    fprintf(outputFile, "%lld: READ LOCK ACQUIRED\n", timeStamp);
    fflush(outputFile);

    hashRecord *current = hashTable[index];

    while (current != NULL)
    {
        if (strcmp(current->name, record->name) == 0)
        {
            timeStamp = current_timeStamp();
            fprintf(outputFile, "%lld: SEARCH: FOUND %s, Salary: %u\n", timeStamp, current->name, current->salary);
            fprintf(outputFile, "%lld: READ LOCK RELEASED\n", timeStamp);
            fflush(outputFile);
            releaseCounter++;
            pthread_rwlock_unlock(&rwlock);
            return NULL;
        }
        current = current->next;
    }

    timeStamp = current_timeStamp();
    fprintf(outputFile, "%lld: SEARCH: NOT FOUND NOT FOUND\n", timeStamp);
    fprintf(outputFile, "%lld: READ LOCK RELEASED\n", timeStamp);
    fflush(outputFile);
    releaseCounter++;

    pthread_rwlock_unlock(&rwlock);
    return NULL;
}

void print(const char *unused1, int unused2)
{
    // Unused parameters
    (void)unused1;
    (void)unused2;

    FILE *output_file = fopen("output.txt", "a");
    hashRecord **records = NULL;
    int record_count = 0;

    fprintf(output_file, "Number of lock acquisitions: %d\n", lockCounter);
    fprintf(output_file, "Number of lock releases: %d\n", releaseCounter);
    fflush(output_file);

    if (!output_file)
    {
        fprintf(stderr, "Error opening output file for printing hash table\n");
        return;
    }

    // Acquire read lock before printing anything
    pthread_rwlock_rdlock(&rwlock);
    lockCounter++;

    timeStamp = current_timeStamp();
    fprintf(output_file, "%lld: READ LOCK ACQUIRED\n", timeStamp);
    fflush(outputFile);

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
    releaseCounter++;

    timeStamp = current_timeStamp();
    fprintf(output_file, "%lld: READ LOCK RELEASED\n", timeStamp);
    fflush(outputFile);
    fclose(output_file);
}
