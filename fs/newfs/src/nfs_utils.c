#include "../include/nfs.h"
#include <string.h>
#define  DBG(str)  do{printf("debugINFO: %s\n", str);}while(0)

char* get_fname(const char* path) {
    char tem = '/';
    if (strrchr(path, tem) != NULL) {
        char* fname = strrchr(path, tem) + 1;
        return fname;
    } else
        return path;
}

int calc_path_level(const char* path) {
    if (strcmp(path, "/") == 0) {
        return 0;
    }
    char* tem = path;
    int   level = 0;
    while(*tem != '\0' && *tem != NULL) {
        if (*tem == '/') {
            level++;
        }
        tem++;
    }
    return level;
}

void casual_read(int offset, char* out, int size) {
    int offset_for_ddriver = (offset / IO_SZ) * IO_SZ;
    int bias = offset - offset_for_ddriver;
    int size_for_ddriver   = ((size + bias + IO_SZ - 1) / IO_SZ) * IO_SZ;
    char* buf = (char*) malloc(size_for_ddriver);

    ddriver_seek(super.fd, offset_for_ddriver, 0);
    int   iter_num = size_for_ddriver / IO_SZ;
    for (int i = 0; i < iter_num; i++) {
        ddriver_read(super.fd, buf + i * IO_SZ, IO_SZ);
    }

    char* real_start = buf + bias;
    memcpy(out, real_start, size);
    free(buf);
}

void casual_write(int offset, char* input, int size) {
    int offset_for_ddriver = (offset / IO_SZ) * IO_SZ;
    int bias = offset - offset_for_ddriver;
    int size_for_ddriver   = ((size + bias + IO_SZ - 1) / IO_SZ) * IO_SZ;
    int   iter = size_for_ddriver / IO_SZ;
    char* input_dd         = (char*) malloc(size_for_ddriver);
    casual_read(offset_for_ddriver, input_dd, size_for_ddriver);
    memcpy(input_dd + bias, input, size);

    ddriver_seek(super.fd, offset_for_ddriver, 0);
    for(int i = 0; i < iter; i++) {
        ddriver_write(super.fd, input_dd + i * IO_SZ, IO_SZ);
    }

    free(input_dd);
}

void insert_dentry (struct nfs_inode* inode, nfs_dentry* dentry) {
    dentry->brother = inode->dentry_sons;
    inode->dentry_sons = dentry;
    inode->dir_count += 1;
}

void remove_dentry (nfs_inode* inode, nfs_dentry* dentry) {
    nfs_dentry* tem = inode->dentry_sons;
    if (tem == NULL) return;
    if (tem == dentry) {
        inode->dentry_sons = tem->brother;
        inode->dir_count -= 1;
        return;
    }
    if (tem != NULL && tem != dentry) {
        while (tem->brother != NULL) {
            if(tem->brother == dentry) {
                tem->brother = dentry->brother;
                inode->dir_count -= 1;
                return;
            }
            tem = tem->brother;
        }
    }
    DBG("根本没找到，remove_dentry 失败");
}

nfs_inode* alloc_inode(nfs_dentry* dentry) {
    //检查bitmap是否有空位
    boolean is_empty = 0;
    int     ino_cursor = -1;
    for (int i = 0; i < super.inode_num; i++) {
        if((super.bitmap_inode + i)[0] == 0) {
            is_empty = 1;
            ino_cursor = i;
        }
    }
    if (is_empty || ino_cursor < super.inode_num) {
        nfs_inode* new = (nfs_inode*) malloc(sizeof(nfs_inode));
        new->ino = ino_cursor;
        dentry->ino = new->ino;
        new->size = 0;
        dentry->inode = new;
        new->dentry_self = dentry;
        new->dentry_sons = NULL;
        new->dir_count = 0;
        super.bitmap_inode[ino_cursor] = 1;
        if(dentry->ftype == REG) {
            new->data = (uint8_t*) malloc(DATABLOCK_PER_INODE * BLOCK_SZ);
        }
        return new;
    } else {
        DBG("没有空闲inode，分配失败");
        return NULL;
    }
}


//唉， 还是remove inode / dentry 分开写吧
void remove_inode(nfs_dentry* dentry) {
    nfs_inode* tem = dentry->inode;
    //检查是否到递归的最后一层    
    if(tem->dentry_sons != NULL) {
        //递归进入子目录
        nfs_dentry* iter = tem->dentry_sons;
        while (iter != NULL) {
            remove_inode(iter);
            iter = iter->brother;
        }
    }
    //最后
    free(tem->data);
    free(tem);
    //remove_dentry(tem->dentry_self); 这个很麻烦， 因为要维护串联dentrybrother， 还要维护parent_inode 的 dentry_sons
    return;
    
}

void remove_dentry()
