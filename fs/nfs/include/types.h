#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdint.h>

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

typedef struct {
    char    name[MAX_NAME_LEN];
    uint32_t ino;
    FILE_TYPE ftype;
}nfs_dentry_d;

typedef struct nfs_super {
    uint32_t magic;
    int      fd;
    /* TODO: Define yourself */
    uint8_t* bitmap_inode;
    uint8_t* bitmap_data;
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
    nfs_dentry_d root_dentry_d;
}nfs_super;

typedef struct nfs_inode {
    uint32_t ino;
    /* TODO: Define yourself */
    int      size;
    struct nfs_dentry* dentry_self;
    struct nfs_dentry* dentry_sons;
    int      child_count;

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
    int ino;
    int size;//file's byte size
    int child_count;
    int direct_data[DATABLOCK_PER_INODE];//块号
}nfs_inode_d;


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