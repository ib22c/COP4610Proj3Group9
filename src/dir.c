/*
-ls
-cd
-parsing directory entries
-creating new entries (mkdir, creat)
-deleting entries (rm, rmdir)
-checking if directory is empty*/
#include "dir.h"
#include "fat.h"
#include <string.h>

uint32_t cwd_cluster;  // the first cluster of the current working directory, so we know where we are
static char cwd_path[256];     //string path for shell prompt

void dir_init(uint32_t root) {
    cwd_cluster = root;        //usually root_cluster from fat32_init()
    strcpy(cwd_path, "/");
}

//helpers
uint32_t get_cwd_cluster(void) { return cwd_cluster; }
const char* get_cwd_path(void) { return cwd_path; }

bool dir_cd(const char *name) {
    DirEntry e;
    if (find_dir_entry(cwd_cluster, name, &e, NULL) && (e.DIR_Attr & 0x10)) {
        cwd_cluster = first_cluster_from_entry(&e); //update cwd_path string for prompt
        strcat(cwd_path, name);
        return true;
    }
    return false;
}

void fat32_ls(uint32_t start_cluster)
{
    if (start_cluster < 2) 
        start_cluster = bpb.root_cluster; // safety: use root cluster from global BootInfo `bpb`

    const size_t entries_per_cluster = cluster_size / 32;
    if (entries_per_cluster == 0) 
        return;

    uint8_t *cluster_buf = malloc(cluster_size);
    if (!cluster_buf) 
        return;

    uint32_t cluster = start_cluster;
    bool is_root = (start_cluster == bpb.root_cluster);

    while (1) {
        //read cluster bytes
        if (read_cluster_bytes(cluster, cluster_buf) != 0) 
            break;

        //iterate directory entries in this cluster
        for (size_t i = 0; i < entries_per_cluster; ++i) {
            DirEntry *e = (DirEntry *)(cluster_buf + i * 32);

            if (is_end_of_dir(e)) { // 0x00 -> no more entries in this dir
                free(cluster_buf);
                return;
            }

            // Format the short 8.3 name
            char namebuf[64];
            format_short_name(e->DIR_Name, namebuf, sizeof namebuf);

            //indicate directory vs file
            bool is_dir = (e->DIR_Attr & 0x10) != 0;

            if (is_dir) 
                printf("%s/\t", namebuf);
            else         
                printf("%s\t", namebuf);
        }

        //follow cluster chain
        uint32_t next = fat_get_entry(cluster) & 0x0FFFFFFF; // mask to 28 bits for FAT32
        if (next >= 0x0FFFFFF8 || next == 0x0) 
            break; // EOC or invalid
        cluster = next;
    }

    free(cluster_buf);
}

void dir_ls(uint32_t start_cluster) {
    fat32_ls(cwd_cluster);  // ls uses cwd_cluster internally
}