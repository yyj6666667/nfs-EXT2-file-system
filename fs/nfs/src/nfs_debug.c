#include "../include/nfs.h"


void nfs_dump_bitmap() {
    int byte_cursor = 0;
    int bit_cursor = 0;

    for (byte_cursor = 0; byte_cursor < BLOCK_SZ; 
         byte_cursor+=4)
    {
        for (bit_cursor = 0; bit_cursor < 8; bit_cursor++) {
            printf("%d ", (super.bitmap_inode[byte_cursor] & (0x1 << bit_cursor)) >> bit_cursor);   
        }
        printf("\t");

        for (bit_cursor = 0; bit_cursor < 8; bit_cursor++) {
            printf("%d ", (super.bitmap_inode[byte_cursor + 1] & (0x1 << bit_cursor)) >> bit_cursor);   
        }
        printf("\t");
        
        for (bit_cursor = 0; bit_cursor < 8; bit_cursor++) {
            printf("%d ", (super.bitmap_inode[byte_cursor + 2] & (0x1 << bit_cursor)) >> bit_cursor);   
        }
        printf("\t");
        
        for (bit_cursor = 0; bit_cursor < 8; bit_cursor++) {
            printf("%d ", (super.bitmap_inode[byte_cursor + 3] & (0x1 << bit_cursor)) >> bit_cursor);   
        }
        printf("\n");
    }
}