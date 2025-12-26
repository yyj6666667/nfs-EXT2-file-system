#ifndef _UTILS_H_
#define _UTILS_H_

#include "../include/nfs.h"
#include <string.h>
#include <assert.h>
#define  DBG(str)  do{printf("debugINFO: %s\n", str);}while(0)

//tools functions declaration
void super_init(struct nfs_super* super, int N, int k, int s, int nfs_magic);
struct nfs_dentry* new_dentry(char* filename, int ino, FILE_TYPE ftype);
char* get_fname(const char* path);
int calc_path_level(const char* path);
void casual_read(int offset, char* out, int size);
int casual_write(int offset, char* input, int size);
void insert_dentry (struct nfs_inode* inode, nfs_dentry** dentry, FILE_TYPE ftype);
void remove_dentry (struct nfs_inode* inode, nfs_dentry* dentry);
nfs_inode* alloc_inode(nfs_dentry* dentry);
void free_inode_recursively_ram(nfs_dentry* dentry);
void sync_inode_to_disk(nfs_inode *inode);
void sync_bitmap_to_disk(nfs_inode* inode);
void sync_super_to_disk();
void free_super_ram();
nfs_dentry* general_find(const char* path, boolean* is_found, nfs_dentry* root_dentry);
int total_rebuilt_from_disk(nfs_super* super_ram, nfs_super* super_disk, nfs_dentry* root_dentry, nfs_inode** root_inode);
nfs_inode* restore_inode(nfs_dentry* dentry);
char* read_inode_data_disk(nfs_inode* inode);
int   write_inode_data_disk(nfs_inode* inode);

int ram_and_disk_trans(boolean flag, nfs_dentry* dentry, nfs_inode* inode);


#endif /* _UTILS_H_ */