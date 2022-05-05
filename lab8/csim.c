// name: Wang Shao
// ID: 520021911427
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "cachelab.h"
#define MAX_LEN 512

bool verbose = false;           // Details print or not
int hit_count = 0;              // cache hit
int miss_count = 0;             // cache miss
int eviction_count = 0;         // clean some memory region
int index_bits;                 // Set index
int ass_num;                    // how many lines per set
int block_bits;                 // Block offset

typedef struct line
{
    int valid;
    int tag;
    int block;
    int size;
} line_t;

typedef struct set
{
    line_t *lineGrp;
    int *used;                  
} set_t;

set_t *cache;

void build_cache()
{
    int set_num = 2 << index_bits;
    int ass_num = ass_num;

    cache = (set_t *) malloc(sizeof(line_t) * set_num);
    for (int i = 0; i < set_num; ++i) {
        cache[i].used = (int *) malloc(sizeof(int) * ass_num);
        cache[i].lineGrp = (line_t *) malloc(sizeof(line_t) * ass_num);
        for (int j = 0; j < ass_num; ++j) {
            cache[i].used[j] = 0;
            cache[i].lineGrp[j].valid = 0;
            cache[i].lineGrp[j].tag = 0;
            cache[i].lineGrp[j].block = 0;
            cache[i].lineGrp[j].size = 0;
        }
    }
   
}

int getTag(long addr)
{
    return (addr >> (index_bits + block_bits));
}

int getSet(long addr)
{
    int mask = 0;
    for (int i = 0; i < index_bits; ++i)
        mask |= (0x1 << i);
    return (addr >> block_bits) & mask;
}

int getBlock(long addr)
{
    int mask = 0;
    for (int i = 0; i < block_bits; ++i)
        mask |= (0x1 << i);
    return addr & mask;
}

void load(long addr, int size)
{
    int ass_num = ass_num;
    int tag = getTag(addr);
    int set_index = getSet(addr);
    int block_offset = getBlock(addr);

    int index = 0;
    bool isHit = false;
    bool hasEmptyLine = false;
    for (int i = 0; i < ass_num; ++i) {
        line_t line = cache[set_index].lineGrp[i];
        index = (cache[set_index].used[i] >= cache[set_index].used[index]) ? i : index; //LRU index
        if (line.valid == 0) hasEmptyLine = true;       //check if exists empty line
        
        /* memory hit */
        if (line.valid == 1 &&  line.tag == tag) {
            isHit = true;
            hit_count++;
            cache[set_index].used[i] = 0;               //if accessed, used[i] set to 0
            if (verbose) {
                printf("L %lx,%d hit\n", addr, size);
            }
        }
        /* not hit */
        else {
            cache[set_index].used[i]++;                 //else ++used[i]
        }
    }

    if (isHit) return;
    else if (hasEmptyLine) {
        cache[set_index].lineGrp[index].valid = 1;
        cache[set_index].lineGrp[index].tag = tag;
        cache[set_index].lineGrp[index].block = block_offset;
        cache[set_index].lineGrp[index].size = size;
        cache[set_index].used[index] = 0;
        ++miss_count;
        if (verbose) printf("L %lx,%d miss\n", addr, size);
    }
    else {
        cache[set_index].lineGrp[index].valid = 1;
        cache[set_index].lineGrp[index].tag = tag;
        cache[set_index].lineGrp[index].block = block_offset;
        cache[set_index].lineGrp[index].size = size;
        cache[set_index].used[index] = 0;
        ++miss_count;
        ++eviction_count;
        if (verbose) printf("L %lx,%d miss eviction\n", addr, size);
    }


}

void store(long addr, int size)
{
    int ass_num = ass_num;
    int tag = getTag(addr);
    int set_index = getSet(addr);
    int block_offset = getBlock(addr);

    int index = 0;
    bool isHit = false;
    bool hasEmptyLine = false;
    for (int i = 0; i < ass_num; ++i) {
        line_t line = cache[set_index].lineGrp[i];
        index = (cache[set_index].used[i] >= cache[set_index].used[index]) ? i : index; //LRU index
        if (line.valid == 0) hasEmptyLine = true;       //check if exists empty line
        
        /* memory hit */
        if (line.valid == 1 &&  line.tag == tag) {
            isHit = true;
            hit_count++;
            cache[set_index].used[i] = 0;               //if accessed, used[i] set to 0
            cache[set_index].lineGrp[i].block = block_offset;   //update block_offset
            cache[set_index].lineGrp[i].size = size;            //update size
            if (verbose) {
                printf("S %lx,%d hit\n", addr, size);
            }
        }
        /* not hit */
        else {
            cache[set_index].used[i]++;                 //else ++used[i]
        }
    }

    if (isHit) return;
    else if (hasEmptyLine) {
        cache[set_index].lineGrp[index].valid = 1;
        cache[set_index].lineGrp[index].tag = tag;
        cache[set_index].lineGrp[index].block = block_offset;
        cache[set_index].lineGrp[index].size = size;
        cache[set_index].used[index] = 0;
        ++miss_count;
        if (verbose) printf("S %lx,%d miss\n", addr, size);
    }
    else {
        cache[set_index].lineGrp[index].valid = 1;
        cache[set_index].lineGrp[index].tag = tag;
        cache[set_index].lineGrp[index].block = block_offset;
        cache[set_index].lineGrp[index].size = size;
        cache[set_index].used[index] = 0;
        ++miss_count;
        ++eviction_count;
        if (verbose) printf("S %lx,%d miss eviction\n", addr, size);
    }
}

void modify(long addr, int size)
{
    printf("M: \n");
    load(addr, size);
    store(addr, size);
}

int main(int argc, char *argv[])
{
    int o;
    char input_path[20];
    const char *optstring = "vs:E:b:t:";
    while ((o = getopt(argc, argv, optstring)) != -1)
    {
        switch (o)
        {
        case 'v':
            verbose = true;
            break;
        case 's':
            index_bits = atoi(optarg);
            printf("index_bits: %d\n", index_bits);
            break;
        case 'E':
            ass_num = atoi(optarg);
            printf("ass_num: %d\n", ass_num);
            break;
        case 'b':
            block_bits = atoi(optarg);
            printf("block_bits: %d\n", block_bits);
            break;
        case 't':
            strcpy(input_path, optarg);
            //printf("input_path: %s\n", input_path);
            break;
        }
    }

    build_cache();
    
    FILE *in;
    int slen;
    static char buf[MAX_LEN];
    in = fopen(input_path, "r");
    if (!in) {
        printf("Can't open input file '%s'\n", input_path);
        exit(1);
    }

    while (fgets(buf, MAX_LEN, in) != NULL) {
        slen = strlen(buf);
        char op = buf[1];               //operation code
        int pos;
        if (buf[0] == 'I') continue;    //ignore 'I'
        for (int i = 0; i < slen; ++i)
            if(buf[i] == ',') {
                pos = i;
                buf[i] = '\0';
                break;
            }
        long address = strtol(buf + 3, NULL, 16);
        int size = atoi(buf + pos + 1);

        switch(op) {
            case 'L':
                load(address, size);
                break;
            case 'S':
                store(address, size);
                break;
            case 'M':
                modify(address, size);
                break;
        }
    }
    

    printSummary(index_bits, miss_count, block_bits);
    return 0;
}
