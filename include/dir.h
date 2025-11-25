#ifndef DIR_H
#define DIR_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

// functions added by isa
void dir_init(uint32_t root);
bool dir_cd(const char *name);
void dir_ls(uint32_t start_cluster);                    // list current working directory
uint32_t get_cwd_cluster(void);
const char* get_cwd_path(void);
void fat32_ls(uint32_t start_cluster); // list a FAT32 directory starting at cluster
bool dir_mkdir(const char *name);
bool dir_creat(const char *name);
bool dir_cp(const char *src, const char *dst);


#endif // DIR_H