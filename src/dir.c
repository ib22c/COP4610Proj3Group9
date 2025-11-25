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
#include <stdlib.h>
#include <stdint.h>


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
// --- Part 3: mkdir / creat / cp ---

// helper: build a short-name entry name buffer
static bool build_83_name(const char *input, char out[11])
{
    return format_name_83(input, out);
}

bool dir_mkdir(const char *name)
{
    if (!name || !*name) {
        printf("mkdir: invalid name\n");
        return false;
    }

    // disallow duplicates in current directory
    if (find_dir_entry(cwd_cluster, name, NULL, NULL)) {
        printf("mkdir: already exists: %s\n", name);
        return false;
    }

    char short_name[11];
    if (!build_83_name(name, short_name)) {
        printf("mkdir: invalid 8.3 name: %s\n", name);
        return false;
    }

    // allocate a new cluster for the directory
    uint32_t new_cluster = fat_find_free_cluster();
    if (!new_cluster) {
        printf("mkdir: no free clusters\n");
        return false;
    }

    // mark this cluster as end-of-chain
    fat_set_entry(new_cluster, FAT32_EOC);

    // build "." and ".." entries inside the new directory cluster
    uint8_t *buf = calloc(1, cluster_size);
    if (!buf) {
        printf("mkdir: OOM\n");
        return false;
    }

    DirEntry *dot = (DirEntry *)(buf + 0 * 32);
    DirEntry *dotdot = (DirEntry *)(buf + 1 * 32);

    // "." entry
    memset(dot, 0, sizeof(DirEntry));
    for (int i = 0; i < 11; ++i) dot->DIR_Name[i] = ' ';
    dot->DIR_Name[0] = '.';
    dot->DIR_Attr = ATTR_DIRECTORY;
    dot->DIR_FstClusHigh    = (uint16_t)(new_cluster >> 16);
    dot->DIR_FirstClusterLow = (uint16_t)(new_cluster & 0xFFFF);
    dot->DIR_FileSize = 0;

    // ".." entry
    memset(dotdot, 0, sizeof(DirEntry));
    for (int i = 0; i < 11; ++i) dotdot->DIR_Name[i] = ' ';
    dotdot->DIR_Name[0] = '.';
    dotdot->DIR_Name[1] = '.';
    dotdot->DIR_Attr = ATTR_DIRECTORY;
    dotdot->DIR_FstClusHigh    = (uint16_t)(cwd_cluster >> 16);
    dotdot->DIR_FirstClusterLow = (uint16_t)(cwd_cluster & 0xFFFF);
    dotdot->DIR_FileSize = 0;

    // remaining entries already zeroed by calloc => first 0x00 marks end of dir

    if (write_cluster_bytes(new_cluster, buf) != 0) {
        printf("mkdir: failed to write new directory cluster\n");
        free(buf);
        return false;
    }
    free(buf);

    // now create the short-name directory entry in the *parent* directory
    DirEntry new_entry;
    memset(&new_entry, 0, sizeof(new_entry));
    memcpy(new_entry.DIR_Name, short_name, 11);
    new_entry.DIR_Attr = ATTR_DIRECTORY;
    new_entry.DIR_FstClusHigh    = (uint16_t)(new_cluster >> 16);
    new_entry.DIR_FirstClusterLow = (uint16_t)(new_cluster & 0xFFFF);
    new_entry.DIR_FileSize = 0;

    if (!create_dir_entry(cwd_cluster, &new_entry)) {
        printf("mkdir: failed to create dir entry in parent\n");
        return false;
    }

    return true;
}

bool dir_creat(const char *name)
{
    if (!name || !*name) {
        printf("creat: invalid name\n");
        return false;
    }

    if (find_dir_entry(cwd_cluster, name, NULL, NULL)) {
        printf("creat: already exists: %s\n", name);
        return false;
    }

    char short_name[11];
    if (!build_83_name(name, short_name)) {
        printf("creat: invalid 8.3 name: %s\n", name);
        return false;
    }

    DirEntry new_entry;
    memset(&new_entry, 0, sizeof(new_entry));
    memcpy(new_entry.DIR_Name, short_name, 11);

    new_entry.DIR_Attr = ATTR_ARCHIVE;       // regular file
    new_entry.DIR_FstClusHigh    = 0;        // no data cluster yet
    new_entry.DIR_FirstClusterLow = 0;
    new_entry.DIR_FileSize = 0;             // empty file

    if (!create_dir_entry(cwd_cluster, &new_entry)) {
        printf("creat: failed to create file entry\n");
        return false;
    }

    return true;
}

bool dir_cp(const char *src, const char *dst)
{
    if (!src || !dst || !*src || !*dst) {
        printf("cp: invalid name\n");
        return false;
    }

    // source must exist and be a file
    DirEntry src_entry;
    if (!find_dir_entry(cwd_cluster, src, &src_entry, NULL)) {
        printf("cp: source not found: %s\n", src);
        return false;
    }
    if (src_entry.DIR_Attr & ATTR_DIRECTORY) {
        printf("cp: source is a directory (not supported)\n");
        return false;
    }

    // destination must not exist
    if (find_dir_entry(cwd_cluster, dst, NULL, NULL)) {
        printf("cp: destination already exists: %s\n", dst);
        return false;
    }

    char dst_short_name[11];
    if (!build_83_name(dst, dst_short_name)) {
        printf("cp: invalid destination 8.3 name: %s\n", dst);
        return false;
    }

    // create a placeholder entry for the destination
    DirEntry dst_entry;
    memset(&dst_entry, 0, sizeof(dst_entry));
    memcpy(dst_entry.DIR_Name, dst_short_name, 11);
    dst_entry.DIR_Attr = ATTR_ARCHIVE;
    dst_entry.DIR_FstClusHigh    = 0;
    dst_entry.DIR_FirstClusterLow = 0;
    dst_entry.DIR_FileSize = 0;

    if (!create_dir_entry(cwd_cluster, &dst_entry)) {
        printf("cp: failed to create dest entry\n");
        return false;
    }

    // zero-length source: nothing more to do
    uint32_t src_first_cluster = first_cluster_from_entry(&src_entry);
    uint32_t src_size = src_entry.DIR_FileSize;
    if (src_first_cluster == 0 || src_size == 0) {
        // entry already exists as empty file
        return true;
    }

    // get the source cluster chain
    size_t chain_len = 0;
    uint32_t *src_chain = fat_get_chain(src_first_cluster, &chain_len);
    if (!src_chain || chain_len == 0) {
        printf("cp: failed to build source cluster chain\n");
        free(src_chain);
        return false;
    }

    uint8_t *buf = malloc(cluster_size);
    if (!buf) {
        printf("cp: OOM\n");
        free(src_chain);
        return false;
    }

    uint32_t first_dst_cluster = 0;
    uint32_t prev_dst_cluster = 0;

    for (size_t i = 0; i < chain_len; ++i) {
        uint32_t scluster = src_chain[i];

        if (read_cluster_bytes(scluster, buf) != 0) {
            printf("cp: failed to read source cluster\n");
            free(buf);
            free(src_chain);
            return false;
        }

        uint32_t dcluster = fat_find_free_cluster();
        if (!dcluster) {
            printf("cp: no free cluster while copying\n");
            free(buf);
            free(src_chain);
            return false;
        }

        // link into dest chain
        if (first_dst_cluster == 0)
            first_dst_cluster = dcluster;
        if (prev_dst_cluster != 0)
            fat_set_entry(prev_dst_cluster, dcluster);

        // mark new dest cluster as EOC for now
        fat_set_entry(dcluster, FAT32_EOC);

        if (write_cluster_bytes(dcluster, buf) != 0) {
            printf("cp: failed to write dest cluster\n");
            free(buf);
            free(src_chain);
            return false;
        }

        prev_dst_cluster = dcluster;
    }

    free(buf);
    free(src_chain);

    // update destination directory entry with cluster and size
    uint32_t dst_offset = 0;
    if (!find_dir_entry(cwd_cluster, dst, &dst_entry, &dst_offset)) {
        printf("cp: internal error: dest entry vanished\n");
        return false;
    }

    dst_entry.DIR_FstClusHigh    = (uint16_t)(first_dst_cluster >> 16);
    dst_entry.DIR_FirstClusterLow = (uint16_t)(first_dst_cluster & 0xFFFF);
    dst_entry.DIR_FileSize = src_size;

    write_dir_entry(cwd_cluster, dst_offset, &dst_entry);

    return true;
}
