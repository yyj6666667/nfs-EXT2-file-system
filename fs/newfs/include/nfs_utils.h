#ifndef _UTILS_H_
#define _UTILS_H_

#include "../include/nfs.h"
#include "types.h"
#include <string.h>
#define  DBG(str)  do{printf("debugINFO: %s\n", str);}while(0)

//tools functions declaration
void super_init(struct nfs_super* super, int N, int k, int s);
struct nfs_dentry* new_dentry(char* filename, FILE_TYPE ftype);
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
int inode_loc_in_disk(nfs_inode* inode);
int data_loc_in_disk(int block_id);
nfs_dentry* find_child_dentry(nfs_dentry* parent, const char* name);


#endif /* _UTILS_H_ */