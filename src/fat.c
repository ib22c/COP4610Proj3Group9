//Reading FAT entries
//Allocating/freeing clusters
//Converting cluster ↔ sector
//Reading/writing raw sectors

//Boot sector parsing (for part 1)

#include "fat.h"


//Hello there!
//Load FAT image and parse BPB
bool fat32_init(const char *img_path)
{
    fat_img = fopen(img_path, "rb+");
    if(!fat_img)
    {
        return false;
    }

    fseek(fat_img, 11, SEEK_SET);
    fread(&bpb.bytes_per_sector, 2, 1, fat_img);
    fread(&bpb.sectors_per_cluster, 1, 1, fat_img);
    fread(&bpb.reserved_sectors, 2, 1, fat_img);
    fread(&bpb.num_fats, 1, 1, fat_img);

    fseek(fat_img, 36, SEEK_SET);
    fread(&bpb.fat_size, 4, 1, fat_img);
    fread(&bpb.root_cluster, 4, 1, fat_img);

    fseek(fat_img, 32, SEEK_SET);
    fread(&bpb.total_sectors, 4, 1, fat_img);

    first_fat_sector = bpb.reserved_sectors;
    first_data_sector = bpb.reserved_sectors + bpb.num_fats * bpb.fat_size;

    cluster_size = bpb.bytes_per_sector * bpb.sectors_per_cluster;

    return true;
}

//Convert Cluster -> Byte Offset
uint32_t cluster_to_offset(uint32_t cluster)
{
    return(first_data_sector + (cluster - 2) * bpb.sectors_per_cluster)
            * bpb.bytes_per_sector;
}

//Get FAT Entry
uint32_t fat_get_entry(uint32_t cluster)
{
    uint32_t fat_offset = first_fat_sector * bpb.bytes_per_sector + cluster * 4;

    fseek(fat_img, fat_offset, SEEK_SET);

    uint32_t value;
    fread(&value, 4, 1, fat_img);

    return value & 0x0FFFFFFF;
}

//Set FAT Entry
void fat_set_entry(uint32_t cluster, uint32_t value)
{
    uint32_t fat_offset = first_fat_sector * bpb.bytes_per_sector + cluster * 4;

    fseek(fat_img, fat_offset, SEEK_SET);
    fwrite(&value, 4, 1, fat_img);

    for(int i = 1; i < bpb.num_fats; i++)
    {
        uint32_t mirror_offset = (first_fat_sector + i * bpb.fat_size)
                                    * bpb.bytes_per_sector + cluster * 4;
        fseek(fat_img, mirror_offset, SEEK_SET);
        fwrite(&value, 4, 1, fat_img);
    }
}

//Find Free CLuster
uint32_t fat_find_free_cluster()
{
    uint32_t total_clusters = (bpb.total_sectors - first_data_sector) / bpb.sectors_per_cluster;

    for(uint32_t c = 2; c < total_clusters; c++)
    {
        if(fat_get_entry(c) == 0)
        {
            return c;
        }
    }

    return 0;

}

//Build CLuster Chain
uint32_t *fat_get_chain(uint32_t start, size_t *count_out)
{
    size_t capacity = 16;
    uint32_t *chain = malloc(sizeof(uint32_t) * capacity);
    size_t count = 0;

    uint32_t cur = start;
    while(cur < 0x0FFFFFF8)
    {
        if(count >= capacity)
        {
            capacity *= 2;
            chain = realloc(chain, sizeof(uint32_t) * capacity);
        }
        chain[count++] = cur;
        cur = fat_get_entry(cur);
    }

    *count_out = count;
    return chain;
}

//Free Cluster Chain
bool fat_free_chain(uint32_t start)
{
    uint32_t cur = start;
    while(cur < 0x0FFFFFF8)
    {
        uint32_t next = fat_get_entry(cur);
        fat_set_entry(cur, 0);
        cur = next;
    }
    return true;
}

//Read All Directory Entries in a CLuster
bool read_directory_cluster(uint32_t cluster, DirEntry *entries, size_t max, size_t *count_out)
{
    uint32_t offset = cluster_to_offset(cluster);
    size_t bytes = cluster_size;

    fseek(fat_img, offset, SEEK_SET);

    size_t read = fread(entries, sizeof(DirEntry), bytes / 32, fat_img);
    *count_out = read;

    return (read > 0);
}

//Find Specific Directory Entry
bool find_dir_entry(uint32_t cluster, const char *name, DirEntry *out_entry, uint32_t *entry_offset)
{
    DirEntry entries[256];
    size_t count;

    size_t entries_per_cluster = cluster_size / 32;

    while(1)
    {
        read_directory_cluster(cluster, entries, 256, &count);

        for(size_t i = 0; i < entries_per_cluster; i++)
        {
            if(entries[i].DIR_Name[0] == 0x00)
            {
                return false;
            }
            if(entries[i].DIR_Name[0] == 0xE5)
            {
                continue;
            }
            if(entries[i].DIR_Name[0] == 0x0F)
            {
                continue;
            }

            if(compare_name_83(entries[i].DIR_Name, name))
            {
                if(out_entry) *out_entry = entries[i];

                if(entry_offset)
                {
                    *entry_offset = cluster_to_offset(cluster) + i * 32;
                }
                return true;
            }
        }

        uint32_t next = fat_get_entry(cluster);
        if(next >= 0x0FFFFFF8)
        {
            return false;
        }
        cluster = next;
    }
}

//Write Directory Entry at Offset
bool write_dir_entry(uint32_t cluster, uint32_t entry_offset, const DirEntry *entry)
{
    fseek(fat_img, entry_offset, SEEK_SET);
    fwrite(entry, sizeof(DirEntry), 1, fat_img);
    return true;
}

//Create Directory Entry (FInd free slot)
bool create_dir_entry(uint32_t cluster, const DirEntry * new_entry)
{
    DirEntry entries[256];
    size_t count;

    while(1)
    {
        read_directory_cluster(cluster, entries, 256, &count);

        for(size_t i = 0; i < count; i++)
        {
            if(entries[i].DIR_Name[0] == 0x00 ||entries[i].DIR_Name[0] == 0xE5)
            {
                uint32_t off = cluster_to_offset(cluster) + i * 32;
                //Might need to move this out of the if statement.
                return write_dir_entry(cluster, off, new_entry);
            }


        }

        uint32_t next = fat_get_entry(cluster);
        if(next >= 0x0FFFFFF8)
        {
            uint32_t newc = fat_find_free_cluster();
            if(!newc)
            {
                return false;
            }

            fat_set_entry(cluster, newc);
            fat_set_entry(newc, 0x0FFFFFFF);

            uint32_t off = cluster_to_offset(newc);
            uint8_t zero[4096] = {0};
            fseek(fat_img, off, SEEK_SET);
            fwrite(zero, 1, cluster_size, fat_img);

            cluster = newc;
        }
        else
        {
            cluster = next;
        }
    }
}