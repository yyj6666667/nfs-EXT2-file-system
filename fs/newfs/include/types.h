#ifndef _TYPES_H_
#include <assert.h>
#define _TYPES_H_

#define MAX_NAME_LEN    128     
/**
 * @param  N:磁盘总逻辑块数
 * @param  k: 每个inode指向的数据块个数
 * @param  s: 每个inode所占的字节数
 */
extern void super_init(struct nfs_super* super,int N, int k, int s);

struct custom_options {
	const char*        device;
};

struct nfs_super {
    uint32_t magic;
    int      fd;
    /* TODO: Define yourself */
    int     disk_size;
    int     is_mounted;
    int     data_bnum; //数据块所占逻辑块块数
    int     inode_num;
    int     inode_bnum;
    int     super_bnum;
    int     bitmap_inode_bnum;
    int     bitmap_data_bnum;
    int     super_offset = 0;
    int     bitmap_inode_offset;
    int     bitmap_data_offset;
    int     inode_offset;
    int     data_offset;

};

struct nfs_inode {
    uint32_t ino;
    /* TODO: Define yourself */
};

struct nfs_dentry {
    char     name[MAX_NAME_LEN];
    uint32_t ino;
    /* TODO: Define yourself */
};


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
    start += super->bitmap_inode_bnum * 1024;
    super->bitmap_data_offset = start;
    start += super->bitmap_data_bnum * 1024;
    super->inode_offset = start;
    start += super->inode_bnum * 1024;
    super->data_offset = start;
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