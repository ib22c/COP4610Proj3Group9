#ifndef FAT_H
#define FAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

//BPB Structure (Boot Sector Fields we'll need)
#pragma pack(push, 1)
typedef struct
{
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint32_t fat_size; //Sectors per FAT
    uint32_t root_cluster;
    uint32_t total_sectors;
    uint32_t image_size;
} BootInfo;
#pragma pack(pop)


//Directory Entry Structure
#pragma pack(push, 1)
typedef struct 
{
    char DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t DIR_NTRes;
    uint8_t DIR_CrtTimeTenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LstAccDate;
    uint16_t DIR_FstClusHigh;
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint16_t DIR_FirstClusterLow;
    uint32_t DIR_FileSize;
} DirEntry;
#pragma pack(pop)


//GLobal State for Loaded FAT32 Image
extern FILE *fat_img;

extern BootInfo bpb;

extern uint32_t first_data_sector;
extern uint32_t first_fat_sector;
extern uint32_t cluster_size;


//Initialization & Shutdown
bool fat32_init(const char *img_path);
void fat32_close();


//CLuster <-> Byte Offset FUnctions
uint32_t cluster_to_offset(uint32_t cluster);


//FAT Table Access
uint32_t fat_get_entry(uint32_t cluster);

void fat_set_entry(uint32_t cluster, uint32_t value);

//Cluster Chain Utilities
uint32_t fat_find_free_cluster();

uint32_t *fat_get_chain(uint32_t start_cluster, size_t *count);

bool fat_extend_chain(uint32_t start_cluster, size_t additional_clusters_needed);

bool fat_free_chain(uint32_t start_cluster);


//Directory Reading Utilities
bool read_directory_cluster(uint32_t cluster, DirEntry *entries, size_t max_entries, size_t *out_count);

bool find_dir_entry(uint32_t cluster, const char *name, DirEntry *out_entry, uint32_t *entry_offset);

bool write_dir_entry(uint32_t cluster, uint32_t entry_offset, const DirEntry *entry);

bool create_dir_entry(uint32_t cluster, const DirEntry *new_entry);

//Name Handling

#endif //FAT_H