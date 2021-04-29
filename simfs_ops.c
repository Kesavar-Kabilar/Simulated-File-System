/* This file contains functions that are not part of the visible "interface".
 * They are essentially helper functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simfs.h"

/* Internal helper functions first.
 */

FILE *
openfs(char *filename, char *mode)
{
    FILE *fp;
    if((fp = fopen(filename, mode)) == NULL) {
        perror("openfs");
        exit(1);
    }
    return fp;
}

void
closefs(FILE *fp)
{
    if(fclose(fp) != 0) {
        perror("closefs");
        exit(1);
    }
}

/* File system operations: creating, deleting, reading, and writing to files.
 */
// Signatures omitted; design as you wish.

void update(char *filename, fentry files[], fnode fnodes[], char block[][BLOCKSIZE+1]) {
    int bytes_used = sizeof(fentry) * MAXFILES + sizeof(fnode) * MAXBLOCKS;
    FILE *fp = openfs(filename, "w");

    if(fwrite(files, sizeof(fentry), MAXFILES, fp) < MAXFILES) {
        fprintf(stderr, "Error: write failed on update\n");
        closefs(fp);
        exit(1);
    }

    if(fwrite(fnodes, sizeof(fnode), MAXBLOCKS, fp) < MAXBLOCKS) {
        fprintf(stderr, "Error: write failed on update\n");
        closefs(fp);
        exit(1);
    }
    
    int bytes_to_write = (BLOCKSIZE - (bytes_used % BLOCKSIZE)) % BLOCKSIZE;
    char zerobuf[BLOCKSIZE] = {0};
    if (bytes_to_write != 0  && fwrite(zerobuf, bytes_to_write, 1, fp) < 1) {
        fprintf(stderr, "Error: write failed on update\n");
        closefs(fp);
        exit(1);
    }

    int block_start = sizeof(fentry) * MAXFILES + sizeof(fnode) * MAXBLOCKS;
    int index = 1;
    while (block_start > BLOCKSIZE){
        block_start -= BLOCKSIZE;
        index++;
    }
    
    for (;index < MAXBLOCKS; index++){
        if(sizeof(block[index]) != 0){
            if(fwrite(block[index], BLOCKSIZE, 1, fp) < 1) {
                fprintf(stderr, "Error: write failed on update\n");
                closefs(fp);
                exit(1);
            }
        }
    }

    closefs(fp);
}


void retrieve(char *filename, fentry files[], fnode fnodes[], char block[][BLOCKSIZE+1]) {

    FILE *fp = openfs(filename, "r");

    if ((fread(files, sizeof(fentry), MAXFILES, fp)) == 0) {
        fprintf(stderr, "Error: could not read file entries\n");
        closefs(fp);
        exit(1);
    }
    if ((fread(fnodes, sizeof(fnode), MAXBLOCKS, fp)) == 0) {
        fprintf(stderr, "Error: could not read fnodes\n");
        closefs(fp);
        exit(1);
    }
    int bytes_used = sizeof(fentry) * MAXFILES + sizeof(fnode) * MAXBLOCKS;
    int bytes_to_read = (BLOCKSIZE - (bytes_used % BLOCKSIZE)) % BLOCKSIZE;
    if (bytes_to_read != 0  && fseek(fp, bytes_to_read, SEEK_CUR) != 0) {
        fprintf(stderr, "Error: seek failed during print\n");
        closefs(fp);
        exit(1);
    }
    for(int i = 0; i < MAXBLOCKS; i++){
        for(int j = 0; j < BLOCKSIZE+1; j++){\
            block[i][j] = '\0';
        }
    }
    int block_start = sizeof(fentry) * MAXFILES + sizeof(fnode) * MAXBLOCKS;
    int index = 1;
    while (block_start > BLOCKSIZE){
        block_start -= BLOCKSIZE;
        index++;
    }
    while (index < MAXBLOCKS && fread(block[index], 1, BLOCKSIZE, fp) == BLOCKSIZE) {
        index++;
    }
    if (ferror(fp)) {
        fprintf(stderr, "Error: could not read data block\n");
        closefs(fp);
        exit(1);
    }
    closefs(fp);
}

void zero_char(char a[]){
    for (int i = 0; i < BLOCKSIZE; i++){
        a[i] = '0';
    }
}

void createfile(char *filename, char *newfile){
    fentry files[MAXFILES];
    fnode fnodes[MAXBLOCKS];
    char block[MAXBLOCKS][BLOCKSIZE+1];
    retrieve(filename, files, fnodes, block);
    int success = 0;

    for (int i = 0; i < MAXFILES; i++){
        if (strcmp(files[i].name, "") == 0 && success == 0){
            strncpy(files[i].name, newfile, sizeof(files[i].name));
            files[i].name[sizeof(files[i].name)-1] = '\0';
            success = 1;
        }
    }
    if (success == 0){
        fprintf(stderr, "Error in createfile: not enough memory\n");
    }else{
        update(filename, files, fnodes, block);
    }
}


void deletefile(char *filename, char*newfile){
    fentry files[MAXFILES];
    fnode fnodes[MAXBLOCKS];
    char block[MAXBLOCKS][BLOCKSIZE + 1];
    retrieve(filename, files, fnodes, block);
    int success = 0;

    for (int i = 0; i < MAXFILES; i++){
        if (strcmp(files[i].name, newfile) == 0 && success == 0){
            strncpy(files[i].name, "", sizeof(files[i].name));
            files[i].name[sizeof(files[i].name)-1] = '\0';
            files[i].size = 0;
            success = 1;

            int block_number = files[i].firstblock, newblock = 0;
            while(block_number != -1){
                fnodes[block_number].blockindex *= -1;
                zero_char(block[block_number]);
                newblock = fnodes[block_number].nextblock;
                fnodes[block_number].nextblock = -1;
                block_number = newblock;
            }
            files[i].firstblock = -1;
        }
    }

    if (success == 0){
        fprintf(stderr, "Error in deletefile: file not in file system\n");
    }else{
        update(filename, files, fnodes, block);
    }
}

int nextfreeblock(fnode fnodes[]){
    for (int i = 0; i < MAXBLOCKS; i++){
        if (fnodes[i].blockindex < 0){
            return i;
        }
    }
    return -1;
}

int nextblock(int node, fnode fnodes[], char block[][BLOCKSIZE+1]){
    if(fnodes[node].nextblock > 0){
        return fnodes[node].nextblock;
    }else{
        fnodes[node].nextblock = nextfreeblock(fnodes);
        fnodes[fnodes[node].nextblock].blockindex *= -1;
        zero_char(block[fnodes[node].nextblock]);
        return fnodes[node].nextblock;
    }
}

void writefile(char *filename, char *file, int start, int length){
    fentry files[MAXFILES];
    fnode fnodes[MAXBLOCKS];
    char block[MAXBLOCKS][BLOCKSIZE + 1];
    retrieve(filename, files, fnodes, block);
    int success = 0;

    int curr_file;
    for(int i = 0; i < MAXFILES; i++){
        if (strcmp(files[i].name, file) == 0){
            curr_file = i;
            success += 1;
        }
    }
    if (success == 0){
        fprintf(stderr, "Error in writefile: file not in file system\n");
        return;
    }
    if (files[curr_file].size < start + length){
        files[curr_file].size = start + length;
    }

    int block_number, block_index=start;

    if (files[curr_file].firstblock < 0){
        files[curr_file].firstblock = nextfreeblock(fnodes);
        zero_char(block[files[curr_file].firstblock]);
        fnodes[files[curr_file].firstblock].blockindex *= -1;
    }
    block_number = files[curr_file].firstblock;
    while (block_index > BLOCKSIZE){
        block_index -= BLOCKSIZE;
        block_number = nextblock(block_number, fnodes, block);
        if (block_number < 0){
            fprintf(stderr, "Error in writefile: not enough memory");
            return;
        }
    }

    int input_len = 0;

    char input[length];
    fread(input, length, 1, stdin);
    if(strcmp(input, "") == 0){
        fprintf(stderr, "Error in writefile: did not recieve input\n");
        return;
    }

    while (input_len < length){
        block[block_number][block_index] = input[input_len];
        block_index++;
        input_len++;

        if (block_index == BLOCKSIZE){
            block_number = nextblock(block_number, fnodes, block);
            if (block_number < 0){
                fprintf(stderr, "Error in writefile: not enough memory");
                return;
            }
            block_index = 0;
        }
    }

    update(filename, files, fnodes, block);
}

void readfile(char *filename, char *file, int start, int length){
    fentry files[MAXFILES];
    fnode fnodes[MAXBLOCKS];
    char block[MAXBLOCKS][BLOCKSIZE + 1];
    retrieve(filename, files, fnodes, block);
    int success = 0;

    int curr_file;
    for(int i = 0; i < MAXFILES; i++){
        if (strcmp(files[i].name, file) == 0){
            curr_file = i;
            success += 1;
        }
    }
    if (success == 0){
        fprintf(stderr, "Error in readfile: file not in file system\n");
        return;
    }

    if (files[curr_file].size < start){
        fprintf(stderr, "Error in readfile: start position is larger than file\n");
        return;
    }

    int block_number, block_index=start;

    if (files[curr_file].firstblock < 0){
        files[curr_file].firstblock = nextfreeblock(fnodes);
        zero_char(block[files[curr_file].firstblock]);
        fnodes[files[curr_file].firstblock].blockindex *= -1;
    }
    block_number = files[curr_file].firstblock;
    while (block_index > BLOCKSIZE){
        block_index -= BLOCKSIZE;
        block_number = nextblock(block_number, fnodes, block);
        if (block_number < 0){
            fprintf(stderr, "Error in readfile: not enough memory");
            return;
        }
    }

    int output_len = 0;

    while (output_len < length){
        fprintf(stdout, "%c", block[block_number][block_index]);
        block_index++;
        output_len++;

        if (block_index == BLOCKSIZE){
            block_number = nextblock(block_number, fnodes, block);
            if (block_number < 0){
                fprintf(stderr, "Error in readfile: String goes out of bounds");
                return;
            }
            block_index = 0;
        }
    }

}
