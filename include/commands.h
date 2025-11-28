#ifndef COMMANDS_H
#define COMMANDS_H

#include "fat32.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_OPEN_FILES 10 // Maximum number of files that can be open at once

void printFileSystemInfo(BootSectorData bs);
void lsfunction(struct DirectoryEntry dir[]);
void ReadDirEntries(struct DirectoryEntry dir[], int counter, 
  FILE *imageFile, BootSectorData bs);
int openFile(struct DirectoryEntry dir[], const char* filename, const char* mode, 
  int openFilesCount,OpenFile openFiles[MAX_OPEN_FILES], char * currPath);
int closeFile(const char* filename, int openFilesCount,OpenFile openFiles[MAX_OPEN_FILES]);
void lsoffunction(OpenFile openFiles[MAX_OPEN_FILES], char img_mounted_name[50]);
void lseekfunction(OpenFile openFiles[], const char *filename, char *token);

//part3 ADDED
int mkdirFunction(struct DirectoryEntry dir[], const char* dirname, FILE* imageFile, BootSectorData bs, int parentCluster);
int creatFunction(struct DirectoryEntry dir[], const char* filename, FILE* imageFile, BootSectorData bs);


#endif