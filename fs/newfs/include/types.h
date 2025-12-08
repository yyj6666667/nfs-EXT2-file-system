#ifndef _TYPES_H_
#include <assert.h>
#include <string.h>
#include "nfs.h"
#define _TYPES_H_
#define BLOCK_SZ 1024

#define MAX_NAME_LEN    128     
//extern const int DATABLOCK_PER_INODE;

typedef enum {
    REG,
    DIR,
    SYM_LINK
} FILE_TYPE;

typedef int boolean;
/**
 * @param  N:磁盘总逻辑块数
 * @param  k: 每个inode指向的数据块个数
 * @param  s: 每个inode所占的字节数
 */

struct custom_options {
	const char*        device;
};

typedef struct nfs_super {
    uint32_t magic;
    int      fd;
    /* TODO: Define yourself */
    uint8_t* bitmap_inode;
    uint8_t* bitmap_data;
    uint8_t* inode_table;
    int     sz_io;
    int     sz_blk;
    int     disk_size;
    int     is_mounted;
    int     data_bnum; //数据块所占逻辑块块数
    int     inode_num;
    int     inode_bnum;
    int     super_bnum;
    int     bitmap_inode_bnum;
    int     bitmap_data_bnum;
    int     super_offset;
    int     bitmap_inode_offset;
    int     bitmap_data_offset;
    int     inode_offset;
    int     data_begin_loc;
    int super_loc_d ;// which is super_loc_begin_loc_in_disk
    int bitmap_inode_loc_d;
    int bitmap_data_loc_d;
    int inode_loc_d;
    int data_loc_d;
}nfs_super;

typedef struct nfs_inode {
    uint32_t ino;
    /* TODO: Define yourself */
    int      size;
    struct nfs_dentry* dentry_self;
    struct nfs_dentry* dentry_sons;
    int      dir_count;

    uint8_t* data;
}nfs_inode;

typedef struct nfs_dentry {
    char     name[MAX_NAME_LEN];
    uint32_t ino;
    /* TODO: Define yourself */
    struct nfs_dentry* parent;
    struct nfs_dentry* brother;
    struct nfs_inode*  inode;
    FILE_TYPE          ftype;
} nfs_dentry;

typedef struct {
    int magic;
    int disk_size;
    int ino_n;
    int inode_bnum;
    int bitmap_data_bnum;
    int bitmap_inode_bnum;
    int data_bnum;
    int data_start_offset;
}nfs_super_d;

typedef struct {
    int ino;
    int size;//file's byte size
    int dir_count;
    int direct_data[DATABLOCK_PER_INODE];//块号
}nfs_inode_d;

void super_init(struct nfs_super* super,int N, int k, int s) {
    
    super->disk_size = N * 1024;
    super->is_mounted =  1;
    super->super_bnum = 1;
    double denominator = 1.0 + 1.0 / (8192 * k) + 
                         1.0 / 8192 + (double)s / (1024 * k);
    super->data_bnum  = (N - 1) / denominator;
    super->inode_num = super->data_bnum / k ; 
    super->inode_bnum = (super->inode_num * s + 1023) / 1024;
    super->bitmap_inode_bnum = (super->inode_num + 8191) / 8192;
    super->bitmap_data_bnum  = (super->data_bnum + 8191) / 8192;

    //保险起见，验证合法性
    struct nfs_super tem = *super;
    int sum = tem.data_bnum + tem.inode_bnum 
              + tem.bitmap_inode_bnum + tem.bitmap_data_bnum + 1;
    assert(sum <= N);
    //计算各个磁盘中的offset
    int start = 0;
    start = 1024 * 1;
    super->bitmap_inode_offset = start;
    super->bitmap_inode_loc_d = start;
    start += super->bitmap_inode_bnum * 1024;
    super->bitmap_data_offset = start;
    super->bitmap_data_loc_d = start;
    start += super->bitmap_data_bnum * 1024;
    super->inode_offset = start;
    super->inode_loc_d = start;
    start += super->inode_bnum * 1024;
    super->data_loc_d = start;
    super->data_begin_loc = start;
    super->bitmap_inode = malloc(super->bitmap_inode_bnum * BLOCK_SZ);
    super->bitmap_data  = malloc(super->bitmap_data_bnum * BLOCK_SZ);
    super->inode_table  = malloc(super->inode_bnum * BLOCK_SZ);
    return;
}

struct nfs_dentry* new_dentry(char* filename, FILE_TYPE ftype) {
    struct nfs_dentry* new = (struct nfs_dentry*) malloc(sizeof(struct nfs_dentry));
    memcpy(new->name, filename, strlen(filename));
    new->ftype = ftype;
    new->ino   = -1;
    new->inode = NULL;
    new->parent = NULL;
    new->brother = NULL;
    return new;
}
/*
偏移量(字节)    区域
─────────────────────────────
0              ┌─────────────┐
               │   超级块     │ 1 块
1024           ├─────────────┤
               │ inode位图   │ bitmap_inode_bnum 块
               ├─────────────┤
               │ 数据块位图   │ bitmap_data_bnum 块
               ├─────────────┤
               │  inode区    │ inode_bnum 块
               ├─────────────┤
               │  数据块区    │ data_bnum 块
               └─────────────┘

*/
#endif /* _TYPES_H_ */