#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "commands.h"
#include "fat32.h"
#include "functions.h"

#define WHITESPACE " \t\n"
#define MAX_FILENAME_LENGTH 12 // Given max filename length
#define MAX_CMD_SIZE 255 // The maximum command-line size
#define MAX_ARG_NUM 5 // Assumption based on implemented cmds
#define MAX_FILE_SIZE 16

// take out image file
// margins

int img_mounted = 0;
char img_mounted_name[50];

int main(int argc, char *argv[]) {

    //Initialize Directory struct
    struct DirectoryEntry dir[16];

    // Initialize boot sector data struct
    BootSectorData bs;

    if (argc != 2) {
        // Wrong Usage
        printf("Usage: ./filesys [FAT32 ISO]\n");
        return 1;
    }

    FILE *imageFile = fopen(argv[1], "rb+");    //needs read and write permissions
    strcpy(img_mounted_name,argv[1]);
    if (!imageFile) {
        printf("Error: File does not exist.\n");
        return 1;
    }
    else
    {
        //Read in from the file
        fseek(imageFile, 11, SEEK_SET);
        fread(&bs.bytesPerSector, 2, 1, imageFile);

        fseek(imageFile, 13, SEEK_SET);
        fread(&bs.sectorsPerCluster, 1, 1, imageFile);

        fseek(imageFile, 14, SEEK_SET);
        fread(&bs.reservedSectorCnt, 1, 2, imageFile);

        fseek(imageFile, 16, SEEK_SET);
        fread(&bs.FATnum, 1, 1, imageFile);

        fseek(imageFile, 36, SEEK_SET);
        fread(&bs.FATSize32, 1, 4, imageFile);

        bs.rootAddress = (bs.bytesPerSector * bs.reservedSectorCnt) +
                       (bs.FATnum * bs.FATSize32 * bs.bytesPerSector);

        fseek(imageFile, bs.rootAddress, SEEK_SET);

        // Saving curr pos of file ptr cus idk
        long current_position = ftell(imageFile); 

        fseek(imageFile, 44, SEEK_SET);
        fread(&bs.rootClusterNum, 4, 1, imageFile);

        fseek(imageFile, 32, SEEK_SET);
        fread(&bs.totalSectors, 4, 1, imageFile);

        fseek(imageFile, 0, SEEK_END);
        long fileSize = ftell(imageFile);
        bs.fileSize = fileSize;

        fseek(imageFile, current_position, SEEK_SET);

        for (int i = 0; i < 16; i++)
        {
            fread(&dir[i], sizeof(dir[i]), 1, imageFile);
        }

        img_mounted = 1;
    }

    // Initalize current path
    char currentPath[MAX_CMD_SIZE] = "/"; 

    int currentDirCluster = bs.rootClusterNum; // Track which cluster we're in (start at root), was 2??, ADDED

    OpenFile openFiles[MAX_OPEN_FILES];


      // Initializing elements of openFiles
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        openFiles[i].filename[0] = '\0';
        openFiles[i].mode[0] = '\0';
        openFiles[i].offset = 0;
    }

    int openFilesCount = 0; // Current number of open files

    char *commands = (char *)malloc(MAX_CMD_SIZE);
    if (!commands) {
        printf("Error: Memory allocation failed.\n");
        fclose(imageFile);
        return 1;
    }

    // Big loop start
    while (1) {
        // Print statement
        printf("%s%s> ", argv[1], currentPath);

        if (!fgets(commands, MAX_CMD_SIZE, stdin)) {
            // EOF
            break;
        }

        char *token[MAX_ARG_NUM];
        int token_count = 0;
        char *arg_ptr;
        char *working_str = custom_strdup(commands, MAX_CMD_SIZE);

        if (!working_str) {
            // Memory allocation failure
            break;
        }

        char *working_root = working_str;

        while (((arg_ptr = custom_strsep(&working_str, WHITESPACE)) != NULL) &&
               (token_count < MAX_ARG_NUM)) {
            if (*arg_ptr != '\0') {  // Check if token is not empty
                token[token_count] = custom_strdup(arg_ptr, MAX_CMD_SIZE);
                if (token[token_count] == NULL) {
                    // Memory allocation failure
                    break;
                }
                token_count++;
            }
            else{
                for(int i = token_count; i<MAX_ARG_NUM;i++){
                    token[i] = NULL;
                }
            }
        }

        if (token[0] == NULL)
        {
            // No command entered yet
            continue;
        }
        else if (strcmp("exit", token[0]) == 0)
        {
            // Exit command entered
            if (img_mounted == 1)
            {
                // Close image first
                fclose(imageFile);
                imageFile = NULL;
            }
            // Free memory and exit
            for (int i = 0; i < token_count; i++) {
                free(token[i]);
            }
            free(working_root);
            free(commands);
            break;
        }
        else if (((strcmp("info", token[0]) == 0) || (strcmp("ls", token[0]) == 0) ||
                  (strcmp("cd", token[0]) == 0) || (strcmp("open", token[0]) == 0) ||
                  (strcmp("close", token[0]) == 0) || (strcmp("lsof", token[0]) == 0) ||
                  (strcmp("lseek", token[0]) == 0) || (strcmp("read", token[0]) == 0) ||
                  (strcmp("mkdir", token[0]) == 0) || (strcmp("creat", token[0]) == 0))     //added
                  && (img_mounted == 0))
        {
            printf("Error: Image must be mounted first. Critical program error :( .\n");
        }
        else if (((strcmp("info", token[0]) == 0) || (strcmp("ls", token[0]) == 0) ||
                  (strcmp("cd", token[0]) == 0) || (strcmp("open", token[0]) == 0) ||
                  (strcmp("close", token[0]) == 0) || (strcmp("lsof", token[0]) == 0) ||
                 (strcmp("lseek", token[0]) == 0) || (strcmp("read", token[0]) == 0) ||
                 (strcmp("mkdir", token[0]) == 0) || (strcmp("creat", token[0]) == 0))     //added
                 && (img_mounted == 1))
        {
            // INFO
            // Prints out all the information about the fat32 image file.
            if (strcmp("info", token[0]) == 0)
            {
                printFileSystemInfo(bs);
                continue;
            }
            
            // LS
            // List downs the files from a current directory when fat32 image file is open.
            else if (strcmp("ls", token[0]) == 0)
            {
                lsfunction(dir);
                continue;
            }

            // CD
            // Change Directory Command
            else if (strcmp("cd", token[0]) == 0)
            {
                // Set find bool originally to false
                int find = 0;
                if (token[1] == NULL)
                {
                    // No directory entered
                    printf("Error: Please enter the name of the directory.\n");
                }
                else
                {
                    int counter = 0;
                    if (!strcmp(token[1], "..") || !strcmp(token[1], "."))
                    {
                        if(!strcmp(token[1], "..")){
                            truncateToLastSlash(currentPath);
                            
                        }
                        // Directory is ".." or "."
                        while (counter < 16)
                        {
                            if (strstr(dir[counter].name, token[1]) != NULL)
                            {
                                if (dir[counter].firstClusterLow == 0)
                                {
                                    dir[counter].firstClusterLow = 2;
                                }
                                ReadDirEntries(dir, counter, imageFile, bs);
                                break;
                            }
                            counter++;
                        }
                    }
                    else
                    {
                        // Directory entered is not ".." or "."
                        while (counter < 16)
                        {
                            char word[100];
                            memset(word, 0, sizeof(word));
                            strncpy(word, token[1], strlen(token[1]));

                            if (dir[counter].attributes != 0x20 && compare(dir[counter].name,
                             word))
                            {
                                // Directory is found
                                ReadDirEntries(dir, counter, imageFile, bs);
                                find = 1;
                                appendToPath(currentPath, token[1], MAX_CMD_SIZE);
                                break;
                            }
                            counter++;
                        }
                        
                        if (find == 0)
                        {
                            // Directory entered does not exist
                            printf("Error: %s Does not exist.\n", token[1]);
                        }
                    }
                }
                continue;
            }

            // OPEN
            else if (strcmp("open", token[0]) == 0)
            {
                //Check if there is no file name
                if (token[1] == NULL || token[2] == NULL)
                {
                    printf("Error - Usage: open [FILENAME] [FLAGS]\n");
                    continue;
                }
                else
                {
                    // Check if file exists in directory
                    if (match(dir, token[1]) == -2)
                    {
                        printf("File %s does not exist.\n", token[1]);
                    }
                    else
                    {
                        int result = openFile(dir, token[1],token[2],openFilesCount,
                        openFiles,currentPath);
                        if (result == 0) { // Assuming 0 is the success code
                            // Check if the file is in the openFiles array
                            openFilesCount++;
                            for (int i = 0; i < openFilesCount; i++) {
                                if (strcmp(openFiles[i].filename, token[1]) == 0) {
                                    // File was opened successfully
                                    printf("Opened %s.\n", token[1]);
                                    break;
                                }
                            }

                        } else {
                            printf("Failed to open file.\n");
                        }
                    }
                }
            }

            // CLOSE
            else if (strcmp("close", token[0]) == 0)
            {
                // Cmd use check
                if(token[1] == NULL ){
                    printf("Error - Usage: close [FILENAME]\n");
                    continue;
                }
                else{
                    if(closeFile(token[1], openFilesCount, openFiles) == 0){
                        // Successfully closed file
                        printf("Closed %s\n", token[1]);
                    }
                    else{
                        // File needs to be opened before closing
                        printf("Error: %s is not open.\n", token[1]);
                    }
                }
            }

            // LSOF
            else if (strcmp("lsof", token[0]) == 0)
            {
                lsoffunction(openFiles, img_mounted_name);

            }

            // LSEEK
            else if (strcmp("lseek", token[0]) == 0) {

                lseekfunction(openFiles, token[1], token[2]);

            }

            // READ
            else if (strcmp("read", token[0]) == 0)
            {
                // Readu use check
                if (token[1] == NULL || token[2] == NULL )
                {
                    printf("Please enter the file to read in the following format: ");
                    printf("read <filename> <size(number of bytes)>.\n");
                }
                else
                {
                    // Check if file is in directory
                    int index_counter= match(dir,token[1]);
                    int filepos = -1;
                    int i=0;
                    while(openFiles[i].filename[0] != '\0')
                    {
                        // Check if file is opened
                        if (strcmp(token[1],openFiles[i].filename) == 0)
                        {
                            filepos = i;
                        }
                        i++;
                    }
                    if(index_counter==-2)
                    {
                        // File is not in directory
                        printf("Error: %s not found \n", token[1]);

                    }
                    else if (filepos != -1)
                    {
                        // Check if file has read permissions before reading it
                        if ((strcmp(openFiles[filepos].mode, "-r") == 0) || 
                         (strcmp(openFiles[filepos].mode, "-rw") == 0)
                         || (strcmp(openFiles[filepos].mode, "-wr") == 0))
                        {
                            int bytesNum= atoi(token[2]);

                            int cluster = dir[index_counter].firstClusterLow;

                            fseek(imageFile,
                             openFiles[filepos].offset + LBAToOffset(cluster, bs), SEEK_SET);

                            char *temp_str = malloc(bytesNum);

                            fread(temp_str,bytesNum,1,imageFile);

                            printf("%s\n",temp_str);

                            free(temp_str);
                            // We added this since the instructions said too
                            openFiles[filepos].offset = openFiles[filepos].offset + atoi(token[2]);

                        }
                        else
                        {
                            printf("File must have read permissions\n");
                        }

                    }
                    else
                    {
                        printf("File must be opened first\n");
                    }
                }
            }
        
            // MKDIR, ADDED
            else if (strcmp("mkdir", token[0]) == 0)
            {
                if (token[1] == NULL) {
                    printf("Error - Usage: mkdir [DIRNAME]\n");
                } 
                else 
                {
                    int result = mkdirFunction(dir, token[1], imageFile, bs, currentDirCluster);
                    if (result == 0) {
                    // Write updated directory back to disk
                        writeDirectoryToCluster(imageFile, bs, currentDirCluster, dir);
                    }
                }
                continue;
            }
            // CREAT,ADDED
            else if (strcmp("creat", token[0]) == 0)
            {
                if (token[1] == NULL) {
                    printf("Error - Usage: creat [FILENAME]\n");
                } 
                else 
                {
                    int result = creatFunction(dir, token[1], imageFile, bs);
                    if (result == 0) {
                        // Write updated directory back to disk
                        writeDirectoryToCluster(imageFile, bs, currentDirCluster, dir);
                    }
                }
                continue;
            }
        
            else
            {
                printf("Error: Invalid Command.\n");
                continue;
            } 
        }  
    }

    // Check if file was already closed from exiting
    if(imageFile!= NULL){
        fclose(imageFile);
    }

    return 0;
}

