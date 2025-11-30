#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include "fat32.h"
#include "commands.h"
#include <ctype.h>
#define MAX_FILE_SIZE 16


int LBAToOffset(int sector, BootSectorData bs);
int compare(char imgName[], char input[]);
int match(struct DirectoryEntry dir[], char token[]);
char *custom_strsep(char **stringp, const char *delim);
char *custom_strdup(const char *s, size_t n);
void truncateToLastSlash(char* path);
void appendToPath(char* path, const char* toAppend, int maxLen);

//added, mkdir creat, 
void writeDirectoryToCluster(FILE* imageFile, BootSectorData bs, int cluster, struct DirectoryEntry dir[]);
int findFreeDirectoryEntry(struct DirectoryEntry dir[]);
int entryExists(struct DirectoryEntry dir[], const char* name);
void nameToFAT32Format(const char* input, char* output);
int findFreeCluster(FILE* imageFile, BootSectorData bs);
void markClusterEOC(FILE* imageFile, BootSectorData bs, int cluster);
void writeDirectoryEntries(FILE* imageFile, BootSectorData bs, int cluster, struct DirectoryEntry entries[]);
#endif