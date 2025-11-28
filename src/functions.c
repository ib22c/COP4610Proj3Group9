#include "functions.h"
#include "fat32.h"

int LBAToOffset(int sector, BootSectorData bs)
{
    return ((sector - 2) * bs.bytesPerSector) +
           (bs.bytesPerSector * bs.reservedSectorCnt) +
           (bs.FATnum * bs.FATSize32 * bs.bytesPerSector);
}

int compare(char imgName[], char input[])
{
    char expanded_name[12];
    memset(expanded_name, ' ', 12);

    char temp[12];

    strcpy(temp, input);

    char *token = strtok(temp, ".");

    if (token == NULL)
    {
        strncpy(expanded_name, "..", strlen(".."));
    }
    else
    {
        strncpy(expanded_name, token, strlen(token));
    }

    token = strtok(NULL, ".");

    if (token)
    {
        strncpy((char *)(expanded_name + 8), token, strlen(token));
    }

    expanded_name[11] = '\0';

    int i;
    for (i = 0; i < 11; i++)
    {
        expanded_name[i] = toupper(expanded_name[i]);
    }


    if (strncmp(expanded_name, imgName, 11) == 0)
    {
        return 1;
    }

    return 0;
}

int match(struct DirectoryEntry dir[], char token[])
{
    int index = 0;
    while (index < MAX_FILE_SIZE)
    {
        if ((dir[index].name[0] != 0xffffffe5) &&
            (compare(dir[index].name, token)) &&
            (dir[index].attributes == 0x01 ||
             dir[index].attributes == 0x10 ||
             dir[index].attributes == 0x20 ||
             dir[index].name[0] == 0x2e))
        {
            return index;
        }
        index++;
    }
    return -2;
}


char *custom_strsep(char **stringp, const char *delim) {
    if (*stringp == NULL) { 
        return NULL;
    }

    char *start = *stringp;
    char *end;

    if ((end = strpbrk(start, delim)) != NULL) {
        *end = '\0';  // Replace the delimiter with null terminator
        *stringp = end + 1;
    } else {
        *stringp = NULL;
    }

    return start;
}

char *custom_strdup(const char *s, size_t n) {
    size_t len = strnlen(s, n);
    char *new_str = (char *)malloc(len + 1);

    if (new_str == NULL) {
        return NULL;
    }

    new_str[len] = '\0';
    return (char *)memcpy(new_str, s, len);
}

void truncateToLastSlash(char* path) {
    if (path == NULL || strcmp(path, "/") == 0) {
        return;
    }

    char* lastSlash = strrchr(path, '/');
    if (lastSlash != NULL) {
        if (lastSlash == path) {
            // If the only slash is at the beginning, it's the root directory
            *(lastSlash + 1) = '\0'; // Keep the root slash
        } else {
            *lastSlash = '\0';
        }
    }
}

void appendToPath(char* path, const char* toAppend, int maxLen) {
    if (path == NULL || toAppend == NULL) return;

    // Calculate the remaining space in the path
    int spaceLeft = maxLen - strlen(path) - 1; // -1 for the null terminator

    if (spaceLeft > 1) {
        // Append '/' only if the current path doesn't already end with one
        if (path[strlen(path) - 1] != '/') {
            strncat(path, "/", 1);
            spaceLeft--;
        }
        strncat(path, toAppend, spaceLeft);
    }
}
void writeDirectoryToCluster(FILE* imageFile, BootSectorData bs, 
                             int cluster, struct DirectoryEntry dir[]) {
    int offset = LBAToOffset(cluster, bs);
    fseek(imageFile, offset, SEEK_SET);
    
    for (int i = 0; i < 16; i++) {
        fwrite(&dir[i], sizeof(struct DirectoryEntry), 1, imageFile);
    }
    
    fflush(imageFile); // Force write to disk
}

int findFreeDirectoryEntry(struct DirectoryEntry dir[]) {
    for (int i = 0; i < 16; i++) {
        // 0x00 means never used, 0xE5 means deleted
        if (dir[i].name[0] == 0x00 || dir[i].name[0] == (signed char)0xE5) {
            return i;
        }
    }
    return -1; // No free entry
}
int entryExists(struct DirectoryEntry dir[], const char* name) {
    // Use your existing match() function
    int result = match(dir, (char*)name);
    return (result != -2); // -2 means not found
}
void nameToFAT32Format(const char* input, char* output) {
    // Initialize with spaces
    memset(output, ' ', 11);
    
    char temp[256];
    strncpy(temp, input, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    // Split by dot
    char* dot = strchr(temp, '.');
    
    if (dot != NULL) {
        // Has extension
        *dot = '\0';
        char* name = temp;
        char* ext = dot + 1;
        
        // Copy name (up to 8 chars)
        int nameLen = strlen(name);
        if (nameLen > 8) nameLen = 8;
        for (int i = 0; i < nameLen; i++) {
            output[i] = toupper(name[i]);
        }
        
        // Copy extension (up to 3 chars)
        int extLen = strlen(ext);
        if (extLen > 3) extLen = 3;
        for (int i = 0; i < extLen; i++) {
            output[8 + i] = toupper(ext[i]);
        }
    } else {
        // No extension (like directory names)
        int nameLen = strlen(temp);
        if (nameLen > 11) nameLen = 11;
        for (int i = 0; i < nameLen; i++) {
            output[i] = toupper(temp[i]);
        }
    }
}
int findFreeCluster(FILE* imageFile, BootSectorData bs) {
    // FAT starts at byte offset: reservedSectorCnt * bytesPerSector
    int fatStart = bs.reservedSectorCnt * bs.bytesPerSector;
    
    // Calculate number of clusters in FAT
    int numClusters = (bs.FATSize32 * bs.bytesPerSector) / 4;
    
    uint32_t fatEntry;
    
    // Start from cluster 2 (0 and 1 are reserved)
    for (int cluster = 2; cluster < numClusters; cluster++) {
        fseek(imageFile, fatStart + (cluster * 4), SEEK_SET);
        fread(&fatEntry, 4, 1, imageFile);
        
        if (fatEntry == 0x00000000) {
            return cluster; // Found free cluster
        }
    }
    
    return -1; // No free cluster
}
void markClusterEOC(FILE* imageFile, BootSectorData bs, int cluster) {
    int fatStart = bs.reservedSectorCnt * bs.bytesPerSector;
    uint32_t eocMarker = 0x0FFFFFFF; // End of chain marker
    
    // Write to both FATs (usually there are 2)
    for (int fatNum = 0; fatNum < bs.FATnum; fatNum++) {
        int fatOffset = fatStart + (fatNum * bs.FATSize32 * bs.bytesPerSector);
        fseek(imageFile, fatOffset + (cluster * 4), SEEK_SET);
        fwrite(&eocMarker, 4, 1, imageFile);
    }
}
void writeDirectoryEntries(FILE* imageFile, BootSectorData bs, int cluster, 
                           struct DirectoryEntry entries[]) {
    int offset = LBAToOffset(cluster, bs);
    fseek(imageFile, offset, SEEK_SET);
    
    for (int i = 0; i < 16; i++) {
        fwrite(&entries[i], sizeof(struct DirectoryEntry), 1, imageFile);
    }
    
    fflush(imageFile); // Ensure data is written
}