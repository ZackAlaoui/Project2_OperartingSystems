#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <chash.c>
#include <cstdio>


pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
#define TABLE_SIZE 100  // Adjust as needed

#define TABLE_SIZE 100  // Adjust as needed


//TO DO : Create a wrapper function that takes in the data and calls the corresponding function with the correct parameters

void 

void threads(int threadCount, char **data)
{
    pthread_t *threadArray = (pthread_t *)malloc(threadCount * sizeof(pthread_t));
    hashRecord *records[threadCount]; // Array to store record pointers

    if(threadArray == NULL)
    {
        perror("Failed to allocate memory for threadArray");
        return;
    }
    else
    {
        for (int i = 1; i <= threadCount; i++)
        {
            if(data[i][0] == NULL || data[i][1] == NULL || data[i][2] == NULL)
            {
                perror("Invalid data format");
                continue;
            }

            records[i] = malloc(sizeof(hashRecord));

            if(records[i] == NULL)
            {
                perror("Failed to allocate memory for record");
                continue;
            }

            strcpy(records[i]->name, data[i][1]);
            records[i]->salary = atoi(data[i][2]); //This converts the salary string into an integer

            //Determine thread routine
            void *routine;
            if (strcmp(data[i][1], "insert") == 0)
            {
                routine = insert;
            }
            else if (strcmp(data[i][1], "search") == 0)
            {
                routine = search;
            }
            else if (strcmp(data[i][1], "delete") == 0)
            {
                routine = deleteRecord;
            }
            else
            {
                continue;
            }

            if(pthread_create(&threadArray[i], NULL, routine, records[i]) != 0)
            {
                perror("Failed to create thread");
                continue;
            }
        }

        for(int i = 1; i <= threadCount; i++)
        {
            pthread_join(threadArray[i], NULL);
            free(records[i]);
        }

        free(threadArray);
    }
}

// Insert(key, value)
void insert(const char *name, uint32_t salary)
{
    uint32_t hash = jenkins_one_at_a_time_hash((const uint8_t *)name, strlen(name));
    uint32_t index = hash % TABLE_SIZE;
    Bucket *bucket = &table.buckets[index];

    pthread_rwlock_wrlock(&bucket->lock); // acquire write lock

    if (bucket->head != NULL && strcmp(bucket->head->name, name) == 0)
    {
        // Update existing
        bucket->head->salary = salary;
    }
    else
    {
        // Overwrite or insert new (no collision handling)
        if (bucket->head != NULL)
        {
            free(bucket->head);
        }
        hashRecord *newRecord = malloc(sizeof(hashRecord));
        newRecord->hash = hash;
        strncpy(newRecord->name, name, sizeof(newRecord->name));
        newRecord->name[sizeof(newRecord->name) - 1] = '\0'; // ensure null-termination
        newRecord->salary = salary;
        newRecord->next = NULL;
        bucket->head = newRecord;
    }

    pthread_rwlock_unlock(&bucket->lock); // release lock
}


// Thread-safe delete function
void delete_record(const char *name, int unused) {
    (void)unused;  // Explicitly ignore the second parameter


    uint32_t hash = jenkins_one_at_a_time_hash((const uint8_t *)name, strlen(name));
    size_t index = hash % TABLE_SIZE;  // Get bucket index


    // Acquire writer lock before modifying the table
    pthread_rwlock_wrlock(&rwlock);


    hashRecord *current = hashTable[index];
    hashRecord *prev = NULL;


    while (current != NULL) {
        if (current->hash == hash && strcmp(current->name, name) == 0) {
            // Found the record, remove it
            if (prev == NULL) {
                hashTable[index] = current->next;  // Removing head
            } else {
                prev->next = current->next;  // Removing middle/end node
            }
            free(current);
            printf("Deleted record with key: %s\n", name);


            // Release write lock and return
            pthread_rwlock_unlock(&rwlock);
            return;
        }
        prev = current;
        current = current->next;
    }


    // Release write lock if key was not found
    pthread_rwlock_unlock(&rwlock);
    printf("Key not found: %s\n", name);
}


void* search(const char *key) {
	int unusedParam;
	if (unusedParam != 0) {
    	return NULL; // Return NULL if the unused parameter is not 0
	}
    
	uint32_t index = jenkins_one_at_a_time_hash(key, strlen(key));     // Compute the hash value for the key, key is the param <name>
	pthread_rwlock_rdlock(&hashLocks[index]);  						   // Acquire the reader lock

	hashRecord *current = hashTable[index];        						   // Traverse the linked list
	while (current != NULL) {
    	if (strcmp(current->key, key) == 0) {  					   	   // If the key matches
        	pthread_rwlock_unlock(&hashLocks[index]); 				   // Release the lock
        	return current->value;             						   // Return the associated value
    	}
    	current = current->next;               						   // Move to the next record
	}


	
	pthread_rwlock_unlock(&hashLocks[index]);  						   // Release the lock if key is not found
	return NULL;                               						   // Return NULL if the key is not found
}

void print(const char* unused1, int unused2) {
    // Unused parameters
    (void)unused1;
    (void)unused2;
    
    FILE* output_file = fopen("output.txt", "a");
    if (output_file == NULL) {
        fprintf(stderr, "Error opening output file for printing hash table\n");
        return;
    }
    
    // Get current timestamp
    long long timestamp = current_timestamp();
    fprintf(output_file, "%lld: PRINT\n", timestamp);
    
    // Create an array to store all records for sorting
    hashRecord** records = NULL;
    int record_count = 0;
    
    // Acquire read lock to read the hash table safely
    timestamp = current_timestamp();
    fprintf(output_file, "%lld: READ LOCK ACQUIRED\n", timestamp);
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
    timestamp = current_timestamp();
    fprintf(output_file, "%lld: READ LOCK RELEASED\n", timestamp);
    
    fclose(output_file);
}

