#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <functions.h>

#define MAX_ROWS 11         // Number of rows in the file
#define MAX_WORDS_PER_ROW 3 // Words per row

typedef struct hash_struct
{
  uint32_t hash;
  char name[50];
  uint32_t salary;
  struct hash_struct *next;
} hashRecord;

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

int main()
{
  // Open the file for reading
  FILE *inputFile = fopen("commands.txt", "r");

  char data[MAX_ROWS][MAX_WORDS_PER_ROW];

  char line[256]; // Buffer to hold a single line

  if (!inputFile)
  {
    printf("The input file was unabled to be opened.\n");
    return 1;
  }
  else
  {
    int row = 0;

    // Have a while loop that keeps reading a line from the file until we reach the end of a file
    while (fgets(line, sizeof(line), inputFile) != NULL && row < MAX_ROWS)
    {
      line[strcspn(line, "\n")] = '\0';
      char *token = strtok(line, ",");
      int col = 0;

      while (token != NULL && col < MAX_WORDS_PER_ROW)
      {
        strcpy(data[row][col], token);
        token = strtok(NULL, ",");
        col++;
      }

      row++;
    }
  }

  int threadCount = atoi(data[0][1]);
  threads(threadCount, data);

  fclose(inputFile);

  return 0;
}