#ifndef _UTILS_H_
#define _UTILS_H_

#include "../include/nfs.h"
#include "types.h"
#include <string.h>
#define  DBG(str)  do{printf("debugINFO: %s\n", str);}while(0)

//tools functions declaration
char* get_fname(const char* path);
int calc_path_level(const char* path);
void casual_read(int offset, char* out, int size);
void casual_write(int offset, char* input, int size);
void insert_dentry (struct nfs_inode* inode, nfs_dentry* dentry, FILE_TYPE ftype);
void remove_dentry (struct nfs_inode* inode, nfs_dentry* dentry);
nfs_inode* alloc_inode(nfs_dentry* dentry);
void free_inode(nfs_dentry* dentry);
void sync_inode_to_disk(nfs_inode *inode);
void sync_bitmap_to_disk(nfs_inode* inode);
void sync_super_to_disk();
nfs_dentry* general_find(const char* path, boolean* is_found);


#endif /* _UTILS_H_ */