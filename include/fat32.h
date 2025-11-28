#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH_LENGTH 260 
#define MAX_MODE_LENGTH 10 
#define MAX_FILENAME_LENGTH 12


typedef struct {
    int16_t bytesPerSector;
    int8_t sectorsPerCluster;
    int32_t FATSize32;
    int32_t rootClusterNum;
    int16_t reservedSectorCnt;
    int8_t FATnum;
    int32_t rootAddress;
    int32_t totalSectors;
    long fileSize;
} BootSectorData;

struct __attribute__((__packed__)) DirectoryEntry
{
    char name[11];
    uint8_t attributes;
    uint8_t PlaceHolder[8]; // Do not remove
    uint16_t firstClusterHigh;
    uint8_t PlaceHolder2[4]; // Do not remove
    uint16_t firstClusterLow;
    uint32_t fileSize;
};

typedef struct {
    char filename[MAX_FILENAME_LENGTH];
    char mode [4];
    int offset;
    int fileSize;
    char path[260];
} OpenFile;

typedef struct {
    char filename[256];
    char mode[10];      // -r, -w, etc
    int offset;
    int fileSize;
} FileRecord;


#endif