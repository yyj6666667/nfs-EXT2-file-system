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


//香香了
void free_inode(nfs_dentry* dentry) {
    nfs_inode* inode = dentry->inode;
    //检查是否到递归的最后一层    
    if(inode->dentry_sons != NULL) {
        //递归进入子目录
        nfs_dentry* iter = inode->dentry_sons;
        while (iter != NULL) {
            nfs_dentry* next = iter->brother;
            free_inode(iter);
            iter = next;
        }
    }
    //最后
    super.bitmap_inode[inode->ino] = 0;
    for (int i = 0; i < DATABLOCK_PER_INODE; i++) {
        super.bitmap_data[inode->ino * DATABLOCK_PER_INODE + i] = 0;
    }
    remove_dentry(dentry->parent, inode->dentry_self);
    free(dentry);
    free(inode->data);
    free(inode);
    return;
}

void sync_inode_to_disk(nfs_inode *inode) {
    nfs_inode_d* buf_tem = (nfs_inode_d*) malloc(sizeof(nfs_inode_d));
    buf_tem->size = inode->size;
    buf_tem->ino  = inode->ino;
    buf_tem->dir_count = inode->dir_count;
    for (int i = inode->ino * DATABLOCK_PER_INODE, j = 0; 
        j < DATABLOCK_PER_INODE; j++, i++) {
            *(buf_tem->direct_data + j) = i;
        }
    int loc_in_disk = inode_loc_in_disk(inode);
    casual_write(loc_in_disk, (char*)buf_tem, sizeof(nfs_inode_d));
    free(buf_tem);

    //好了，既然本体已经写入，那么开始愉快的递归吧, 我发现有的递归逻辑写在前面，有的写在后面
    FILE_TYPE type = inode->dentry_self->ftype;
    if(type == DIR) {
        nfs_dentry* sons = inode->dentry_sons;
        do {
            sync_inode_to_disk(sons->inode); //问题： 如果有data怎么办？
        } while(sons->brother != NULL);
    }
     
    if(inode->data != NULL){
        int data_blk_id   = inode->ino * DATABLOCK_PER_INODE;
        int data_loc_disk = data_loc_in_disk(data_blk_id);
        casual_write(data_loc_disk, (char*)inode->data, inode->size);
    }
}

void sync_bitmap_to_disk(nfs_inode* inode) {
    //inode_map
    int loc = super.bitmap_inode_begin_loc_in_disk + inode->ino / 8;
    char tem;
    casual_read(loc, &tem, 1);
    int resuial = inode->ino % 8;
    uint8_t mask = 0b1 << (7 - resuial);
    uint8_t to_be_write = (uint8_t)tem | mask;
    casual_write(loc, (char*)(&to_be_write), 1);

    //data_map
    // 为了可读性和代码容易写，牺牲性能了
    if (inode->data != NULL) {
        int loc = super.bitmap_data_begin_loc_in_disk ;
        int start_bits = inode->ino * DATABLOCK_PER_INODE;
        for (int i = 0; i < DATABLOCK_PER_INODE; i++) {
            int cur_loc = loc + (start_bits + i) / 8;
            char tem;
            casual_read(cur_loc, &tem, 1);
            int resuial = (start_bits + i) % 8;
            uint8_t mask = 0b1 << (7 - resuial);
            tem |= mask;
            casual_write(cur_loc, &tem, 1);
        }
    }
}



//------------------------------------------/
//工具函数的工具函数
//------------------------------------------/

int inode_loc_in_disk(nfs_inode* inode) {
    int offset_bnum = super.super_bnum + super.bitmap_inode_bnum
                    + super.bitmap_data_bnum;
    int offset_1 = offset_bnum * BLOCK_SZ;
    int bias = (inode->ino) * sizeof(nfs_inode_d);
    int offset_final = offset_1 + bias;
    return offset_final;
}

int data_loc_in_disk(int block_id) {
    return super.data_begin_loc + block_id * BLOCK_SZ;
}