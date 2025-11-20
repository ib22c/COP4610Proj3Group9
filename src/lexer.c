#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "fat.h"
//Info command (for part 1)

void info()
{
	uint32_t data_secs = bpb.total_sectors - (bpb.reserved_sectors + bpb.num_fats * bpb.fat_size);
	uint32_t total_clusters = data_secs / bpb.sectors_per_cluster;

	printf("Root cluster: %u\n", bpb.root_cluster);
	printf("Bytes per sector: %u\n", bpb.bytes_per_sector);
	printf("Sectors per cluster: %u\n", bpb.sectors_per_cluster);
	printf("Total clusters: %u\n", total_clusters);
	printf("FAT entries: %u\n", bpb.fat_size * (bpb.bytes_per_sector / 4));
	printf("Image size: %u bytes\n", bpb.total_sectors * bpb.bytes_per_sector);
	
}

int main()
{
	while (1) {
		printf("> ");

		/* input contains the whole command
		 * tokens contains substrings from input split by spaces
		 */

		char *input = get_input();
		printf("whole input: %s\n", input);

		tokenlist *tokens = get_tokens(input);
		for (int i = 0; i < tokens->size; i++) {
			printf("token %d: (%s)\n", i, tokens->items[i]);
		}

				if(strcmp(input, "exit") == 0)
		{
			fat32_close();
			return 0;
		}
		else if(strcmp(input, "info") == 0)
		{
			info();
		}
		else if(strlen(input) == 0)
		{
			continue;
		}
		else
		{
			printf("Unknown command: %s\n", input);
		}
		
		free(input);
		free_tokens(tokens);
	}
	fat32_close();
	return 0;
}

char *get_input(void) {
	char *buffer = NULL;
	int bufsize = 0;
	char line[5];
	while (fgets(line, 5, stdin) != NULL)
	{
		int addby = 0;
		char *newln = strchr(line, '\n');
		if (newln != NULL)
			addby = newln - line;
		else
			addby = 5 - 1;
		buffer = (char *)realloc(buffer, bufsize + addby);
		memcpy(&buffer[bufsize], line, addby);
		bufsize += addby;
		if (newln != NULL)
			break;
	}
	buffer = (char *)realloc(buffer, bufsize + 1);
	buffer[bufsize] = 0;
	return buffer;
}

tokenlist *new_tokenlist(void) {
	tokenlist *tokens = (tokenlist *)malloc(sizeof(tokenlist));
	tokens->size = 0;
	tokens->items = (char **)malloc(sizeof(char *));
	tokens->items[0] = NULL; /* make NULL terminated */
	return tokens;
}

void add_token(tokenlist *tokens, char *item) {
	int i = tokens->size;

	tokens->items = (char **)realloc(tokens->items, (i + 2) * sizeof(char *));
	tokens->items[i] = (char *)malloc(strlen(item) + 1);
	tokens->items[i + 1] = NULL;
	strcpy(tokens->items[i], item);

	tokens->size += 1;
}

tokenlist *get_tokens(char *input) {
	char *buf = (char *)malloc(strlen(input) + 1);
	strcpy(buf, input);
	tokenlist *tokens = new_tokenlist();
	char *tok = strtok(buf, " ");
	while (tok != NULL)
	{
		add_token(tokens, tok);
		tok = strtok(NULL, " ");
	}
	free(buf);
	return tokens;
}

void free_tokens(tokenlist *tokens) {
	for (int i = 0; i < tokens->size; i++)
		free(tokens->items[i]);
	free(tokens->items);
	free(tokens);
}
