#include<cstdint>


// Insert(key, value)
void insert(const char* name, uint32_t salary);

//This function is responsible from removing a name from the list
void deleteRecord(char * name, int unused);

//Seaches for a key data pair in the hash table and returns the value if it exists.
void* search(const char *key);

//Print function
void print(const char * unused1, int unused2);

//Thread function
void threads(int threadCount, char ** data);
