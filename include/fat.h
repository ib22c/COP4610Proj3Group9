#ifndef FAT_H
#define FAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
//Hello there!

#define FAT32_EOC      0x0FFFFFFF
#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN    0x02
#define ATTR_SYSTEM    0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE   0x20

// BPB (BIOS Parameter Block) - fields read from the boot sector
// This is a packed, fixed-layout representation of the on-disk BPB values
// used by FAT32. Keep this packed so the offsets match the disk layout.
#pragma pack(push, 1)
typedef struct
{
    uint16_t bytes_per_sector;    // Number of bytes per logical sector (usually 512)
    uint8_t  sectors_per_cluster; // Number of sectors in a cluster (power-of-two)
    uint16_t reserved_sectors;    // Number of reserved sectors before FAT (includes boot sector)
    uint8_t  num_fats;             // Number of FAT tables (usually 2)
    uint32_t fat_size;            // Size of each FAT in sectors
    uint32_t root_cluster;        // Cluster number of the root directory (usually 2)
    uint32_t total_sectors;       // Total sectors on the volume
    uint32_t image_size;          // Size of the image (in bytes) - derived/optional
    int32_t  rootAddr;            // Additional helper field used by this implementation
} BootInfo;
#pragma pack(pop)

/* Compile-time sanity check: ensure the packed BootInfo size matches
   the expected on-disk layout for the fields we defined. If this fails
   your compiler may not be using C11 or packing is wrong for the target. */
_Static_assert(sizeof(BootInfo) == 26, "BootInfo must be 26 bytes when packed");


// Directory entry (on-disk) - 32 bytes per entry in FAT directories
//got rid of created time, created date, last access date, write time, write date
#pragma pack(push, 1)
typedef struct 
{
    char     DIR_Name[11];      // 0-10
    uint8_t  DIR_Attr;            // File attributes bitmask (readonly, hidden, system, volume, dir, archive), 11
    uint8_t  DIR_NTRes;           // Reserved for NT (usually 0), 12, DO NOT USE
    uint8_t  DIR_CrtTimeTenth;    // 13 Do NOT USE
    uint16_t DIR_CrtTime;         // 14..15 Do NOT USE
    uint16_t DIR_CrtDate;         // 16..17 Do NOT USE
    uint16_t DIR_LastAccDate;     // 18..19 DO NOT USE
    uint16_t DIR_FstClusHigh;     // High 16 bits of first cluster (FAT32), 20..21
    uint16_t DIR_WrtTime;         // 22..23 DO NOT USE
    uint16_t DIR_WrtDate;         // 24..25 DO NOT USE
    uint16_t DIR_FirstClusterLow; // Low 16 bits of first cluster
    uint32_t DIR_FileSize;        // File size in bytes (for files); 0 for directories
} DirEntry;
#pragma pack(pop)


//GLobal State for Loaded FAT32 Image
extern FILE *fat_img;

extern BootInfo bpb;    //initalizes bpb structure

extern uint32_t first_data_sector;
extern uint32_t first_fat_sector;
extern uint32_t cluster_size;

uint32_t first_cluster_from_entry(const DirEntry *entry);

//Initialization & Shutdown
/*returns a bool if properly initalized or not*/
bool fat32_init(const char *img_path);

/* Close the opened FAT image and release any resources held by the
 * FAT32 subsystem (close file handles, free caches, etc.). Safe to call
 * even if `fat32_init` previously failed.*/
void fat32_close();

// Cluster <-> Byte offset functions
/* Convert a cluster number to a byte offset within the image file.
 * The returned offset points to the first byte of the given cluster's
 * data area (i.e., start of that cluster in the image file). */
uint32_t cluster_to_offset(uint32_t cluster);

/* Read the raw bytes of `cluster` into `buffer` which must be at least
 * `cluster_size` bytes long. Returns 0 on success, non-zero on error.
 */
int read_cluster_bytes(uint32_t cluster, uint8_t *buffer);


// FAT table access
/* Read the FAT entry for `cluster` and return its value.
 * The returned value is the next cluster in the chain, or a special
 * end-of-chain marker. Caller must ensure FAT is initialized.*/
uint32_t fat_get_entry(uint32_t cluster);

/* Write `value` into the FAT entry for `cluster`.
 * Use this to create/extend/truncate cluster chains. Returns nothing
 * but callers should ensure proper synchronization if needed.*/
void fat_set_entry(uint32_t cluster, uint32_t value);

// Cluster chain utilities
/* Find a free cluster in the FAT and return its cluster number.
 * Returns 0 on failure (no free clusters) or the cluster index (>0).*/
uint32_t fat_find_free_cluster();

/* Build and return the cluster chain starting at `start_cluster`.
 * Allocates and returns an array of cluster numbers; the number of
 * entries is stored in `*count`. Caller is responsible for freeing
 * the returned array (if non-NULL).*/
uint32_t *fat_get_chain(uint32_t start_cluster, size_t *count);

/* Extend the chain that begins at `start_cluster` by allocating
 * `additional_clusters_needed` free clusters and linking them.
 * Returns true on success, false on failure (insufficient free clusters).*/
bool fat_extend_chain(uint32_t start_cluster, size_t additional_clusters_needed);

/* Free (release) all clusters in the chain starting at `start_cluster`.
 * Marks each cluster in the chain as free in the FAT. Returns true on
 * success, false on failure.*/
bool fat_free_chain(uint32_t start_cluster);


// Directory reading/writing utilities
/* Read directory entries from the directory starting at `cluster`.
 * Parameters:
 *  - cluster: cluster number of the directory to read
 *  - entries: caller-provided buffer to hold DirEntry items
 *  - max_entries: capacity of the `entries` buffer
 *  - out_count: pointer to size_t to receive the number of entries read
 * Returns true on success, false on error. */
bool read_directory_cluster(uint32_t cluster, DirEntry *entries, size_t max_entries, size_t *out_count);

/* Search a directory for an entry matching `name` (8.3 format or plain
 * user input — implementation may call `format_name_83`). If found,
 * copy the DirEntry into `out_entry` and write the entry byte-offset
 * (relative to the start of the directory cluster area) into `entry_offset`.
 * Returns true if found, false if not found or on error.*/
bool find_dir_entry(uint32_t cluster, const char *name, DirEntry *out_entry, uint32_t *entry_offset);

/* Overwrite an existing directory entry at `entry_offset` (within the
 * given directory cluster) with the data from `entry`. Returns true on
 * success.*/
bool write_dir_entry(uint32_t cluster, uint32_t entry_offset, const DirEntry *entry);

/* Create a new directory entry `new_entry` inside the directory at
 * `cluster`. Finds a free slot and writes the entry. Returns true on success.*/
bool create_dir_entry(uint32_t cluster, const DirEntry *new_entry);

// Name handling helpers (8.3 filename support)
/* Convert a user-supplied filename to the FAT 8.3 on-disk format.
 * Writes exactly 11 bytes into `out` (no NUL). Returns true on success
 * or false for invalid input.*/
bool format_name_83(const char *input, char out[11]);

/* Compare an on-disk 11-byte 8.3 name (`entry_name`) with a user
 * string `input` (case-insensitive per FAT rules). Returns true if they
 * match.*/
bool compare_name_83(const char entry_name[11], const char *input);

/* Return true if `entry` is the end-of-directory marker (first byte == 0x00).
 * According to FAT spec, a directory entry whose first name byte is 0x00
 * indicates there are no further entries in this directory.
 */
bool is_end_of_dir(const DirEntry *entry);

/* Format an on-disk 11-byte 8.3 name into a human-readable string.
 * Writes a nul-terminated string into `out` (up to `out_size` bytes).
 * Example: {"FOO     ","TXT"} -> "FOO.TXT" (no quotes)., CHAT
 */
void format_short_name(const char entry_name[11], char *out, size_t out_size);

int write_cluster_bytes(uint32_t cluster, const uint8_t *buffer); //part 3 Ivan


#endif //FAT_H
