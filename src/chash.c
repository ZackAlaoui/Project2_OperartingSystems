#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../Include/functions.h"


int main()
{
  // Open the file for reading
  FILE *inputFile = fopen("./commands.txt", "r");

  //FILE *outputFile = fopen("../Output/output.txt", "a");


  char data[MAX_ROWS][MAX_WORDS_PER_ROW][MAX_LETTERS_PER_WORD];

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
        if (strlen(token) >= MAX_LETTERS_PER_WORD)
        {
          break; // Stop processing this line if token is too long
        }

        strncpy(data[row][col], token, MAX_LETTERS_PER_WORD - 1);      
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
