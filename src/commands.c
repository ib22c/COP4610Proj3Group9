#include "fat32.h"
#include "functions.h"
#define MAX_OPEN_FILES 10 // Maximum number of files that can be open at once
#define MAX_FILENAME_LENGTH 12


void printFileSystemInfo(BootSectorData bs) {
    printf("Bytes per Sector: %d\n", bs.bytesPerSector);
    printf("Sectors per Cluster: %d\n", bs.sectorsPerCluster);
    printf("Total clusters in Data Region: %d\n", (bs.totalSectors - (bs.reservedSectorCnt +
      (bs.FATnum * bs.FATSize32))/ bs.sectorsPerCluster));
    printf("# of entries in one FAT: %f\n", ((bs.FATSize32 * bs.bytesPerSector) / 4.));
    printf("Size of Image (bytes): %ld\n",bs.fileSize);
    printf("Root Cluster: %d\n", bs.rootClusterNum);
    printf("First Data Sector: %d\n",bs.reservedSectorCnt + (bs.FATnum * bs.FATSize32));
    printf("Total Sectors: %d\n",bs.totalSectors);
}

void lsfunction(struct DirectoryEntry dir[])
{
    int i = 0;
    while (i < 16)
    {
        char word[12];
        memset(&word, 0, 12);
        //Checks if the files are read only, subdirectories, 0X30.
        //Does not print the deleted file (signed char)0Xe5.
        if ((dir[i].attributes == 0x01 ||
             dir[i].attributes == 0x10 ||
             dir[i].attributes == 0x20 ||
             dir[i].attributes == 0x30) &&
            dir[i].name[0] != (signed char)0xe5)
        {
            strncpy(word, dir[i].name, 11);
            //added for color purposes, blue is a directory!
            if(dir[i].attributes==0x10){
                printf("\033[34m%s\033[0m\n",word); // Set text color to blue
            }
            else{
                printf("%s\n", word);
            }
        }
        i++;
    }
}

void ReadDirEntries(struct DirectoryEntry dir[], int counter, FILE *imageFile, BootSectorData bs)
{
    fseek(imageFile, LBAToOffset(dir[counter].firstClusterLow, bs), SEEK_SET);
    int i = 0;
    for (i = 0; i < 16; i++)
    {
        fread(&dir[i], sizeof(dir[i]), 1, imageFile);
    }
}

int openFile(struct DirectoryEntry dir[], const char* filename, const char* mode, 
int openFilesCount,OpenFile openFiles[MAX_OPEN_FILES], char* currentPath) {
    // Check if the file is already open
    for (int i = 0; i < openFilesCount; i++) {
        if (strcmp(openFiles[i].filename, filename) == 0) {
            printf("Error: %s is already opened.\n", filename);
            return -1;
        }
    }

    // Validate the mode
    if (strcmp(mode, "-r") != 0 && strcmp(mode, "-w") != 0 &&
        strcmp(mode, "-rw") != 0 && strcmp(mode, "-wr") != 0) {
        printf("Error: Invalid mode.\n");
        return -1;
    }

    if (openFilesCount < MAX_OPEN_FILES) {
        // Possibly need a check for it match doesn't work
        openFiles[openFilesCount].fileSize = dir[match(dir, filename)].fileSize;

        strncpy(openFiles[openFilesCount].filename, filename, MAX_FILENAME_LENGTH - 1);

        openFiles[openFilesCount].filename[FILENAME_MAX - 1] = '\0'; // Ensure null-termination

        strncpy(openFiles[openFilesCount].mode, mode, 3);
        openFiles[openFilesCount].mode[3] = '\0';

        openFiles[openFilesCount].offset = 0;
        
        strncpy(openFiles[openFilesCount].path, currentPath, 11); 

        return 0; // Success
    } else {
        printf("Error: Too many open files.\n");
        return -1;
    }
}

int closeFile(const char* filename, int openFilesCount,OpenFile openFiles[MAX_OPEN_FILES]) {
    // Check if file is open
        for (int i = 0; i < openFilesCount; i++) {
            if (strcmp(openFiles[i].filename, filename) == 0) {
                // Shift all elements after the found element one position back
                for (int j = i; j < openFilesCount - 1; j++) {
                    openFiles[j] = openFiles[j + 1];
                }
                openFilesCount--;
                openFiles[openFilesCount].filename[0] = '\0';
                openFiles[openFilesCount].mode[0] = '\0';
                openFiles[openFilesCount].offset = '\0';
                openFiles[openFilesCount].fileSize = '\0';
                return 0;
            }
        }
        return -1;
}



void lsoffunction(OpenFile openFiles[MAX_OPEN_FILES], char* img_mounted_name)
{
    if (openFiles[0].filename[0] == '\0')
    {
        printf("No files currently open.\n");
    }
    else
    {       //print header 
            printf("%-10s %-15s %-10s %-10s %s\n", "INDEX", "NAME", "MODE", "OFFSET", "PATH");

        int i=0;
        while ((i < MAX_OPEN_FILES) && (openFiles[i].filename[0] != '\0'))
        {

            printf("%-10d %-15s %-10s %-10d %s%s\n", i, openFiles[i].filename,
             openFiles[i].mode, openFiles[i].offset, img_mounted_name,openFiles[i].path);
            i++;
        }
    }
}


void lseekfunction(OpenFile openFiles[], const char *filename, char *offset) {
    int fileFound = -1;
    if (filename == NULL || offset == NULL) {
        printf("Usage: lseek [FILENAME] [OFFSET]\n");
    } else {
        for (int i = 0; i < 10; i++) // assuming max 10 open files
        {
            if (strcmp(openFiles[i].filename, filename) == 0 
             && openFiles[i].filename[0] != '\0') {
                fileFound = i;
            }
        }

        if (fileFound != -1) {
            int newOffset = atoi(offset);

            if (newOffset > openFiles[fileFound].fileSize) {
                printf("Error: Reached end of file.\n");
                return;
            }

            openFiles[fileFound].offset += newOffset;
            return;

        }
        else
        {
            printf("File must be opened first\n");
        }

    if (!fileFound) {
        printf("Error: File '%s' is not opened or does not exist.\n", filename);
    }
}
}

//part3 ADDED
int mkdirFunction(struct DirectoryEntry dir[], const char* dirname, 
                  FILE* imageFile, BootSectorData bs, int parentCluster) {
    
    // Step 1: Check if name already exists
    if (entryExists(dir, dirname)) {
        printf("Error: %s already exists.\n", dirname);
        return -1;
    }
    
    // Step 2: Find free entry in current directory
    int freeEntry = findFreeDirectoryEntry(dir);
    if (freeEntry == -1) {
        printf("Error: Directory is full.\n");
        return -1;
    }
    
    // Step 3: Find a free cluster for the new directory
    int newCluster = findFreeCluster(imageFile, bs);
    if (newCluster == -1) {
        printf("Error: No free clusters available.\n");
        return -1;
    }
    
    // Step 4: Create the directory entry
    struct DirectoryEntry newDir;
    memset(&newDir, 0, sizeof(struct DirectoryEntry));
    
    // Set name in FAT32 format
    nameToFAT32Format(dirname, newDir.name);
    
    // Set attributes (0x10 = directory)
    newDir.attributes = 0x10;
    
    // Set cluster number
    newDir.firstClusterLow = (uint16_t)(newCluster & 0xFFFF);
    newDir.firstClusterHigh = (uint16_t)((newCluster >> 16) & 0xFFFF);
    
    // Set size (directories have size 0)
    newDir.fileSize = 0;
    
    // Step 5: Add entry to parent directory
    dir[freeEntry] = newDir;
    
    // Step 6: Mark cluster as EOC in FAT
    markClusterEOC(imageFile, bs, newCluster);
    
    // Step 7: Initialize new directory with . and .. entries
    struct DirectoryEntry newDirEntries[16];
    memset(newDirEntries, 0, sizeof(newDirEntries));
    
    // Create "." entry (points to itself)
    nameToFAT32Format(".", newDirEntries[0].name);
    newDirEntries[0].attributes = 0x10;
    newDirEntries[0].firstClusterLow = (uint16_t)(newCluster & 0xFFFF);
    newDirEntries[0].firstClusterHigh = (uint16_t)((newCluster >> 16) & 0xFFFF);
    newDirEntries[0].fileSize = 0;
    
    // Create ".." entry (points to parent)
    nameToFAT32Format("..", newDirEntries[1].name);
    newDirEntries[1].attributes = 0x10;
    newDirEntries[1].firstClusterLow = (uint16_t)(parentCluster & 0xFFFF);
    newDirEntries[1].firstClusterHigh = (uint16_t)((parentCluster >> 16) & 0xFFFF);
    newDirEntries[1].fileSize = 0;
    
    // Mark rest as unused
    for (int i = 2; i < 16; i++) {
        newDirEntries[i].name[0] = 0x00;
    }
    
    // Write the new directory's entries to disk
    writeDirectoryEntries(imageFile, bs, newCluster, newDirEntries);
    
    // Step 8: Write updated parent directory to disk
    // You'll need to know the parent's cluster to write it back
    // For now, we'll update the in-memory dir[] array
    // The actual write-back will depend on your current implementation
    
    printf("Directory %s created successfully.\n", dirname);
    return 0;
}
int creatFunction(struct DirectoryEntry dir[], const char* filename, 
                  FILE* imageFile, BootSectorData bs) {
    
    // Step 1: Check if name already exists
    if (entryExists(dir, filename)) {
        printf("Error: %s already exists.\n", filename);
        return -1;
    }
    
    // Step 2: Find free entry in current directory
    int freeEntry = findFreeDirectoryEntry(dir);
    if (freeEntry == -1) {
        printf("Error: Directory is full.\n");
        return -1;
    }
    
    //Step 3: Create the file entry
    struct DirectoryEntry newFile;
    memset(&newFile, 0, sizeof(struct DirectoryEntry));
    
    //Set name in FAT32 format,issue with .txts, idk if need to change that or what
    nameToFAT32Format(filename, newFile.name);
    
    //Set attributes (0x20 = archive/file)
    newFile.attributes = 0x20;
    
    //Set cluster to 0 (no data allocated yet for 0-byte file)
    newFile.firstClusterLow = 0;
    newFile.firstClusterHigh = 0;
    
    //Set size to 0
    newFile.fileSize = 0;
    
    //Step 4: Add entry to directory
    dir[freeEntry] = newFile;
    
    printf("File %s created successfully.\n", filename);
    return 0;
}

