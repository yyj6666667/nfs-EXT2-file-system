#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/nfs_utils.h"

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

char* split_path_from_left(char* path) {
    if(strcmp(path, "/") == 0) {
        return NULL;
    }
    int loc = 0;
    char* cursor = path + 1; 
    while(*cursor != '/') {
        loc++;
        cursor++;
    }
    char* split = (char*) malloc(loc * sizeof(char));
    memcpy(split, path, loc * sizeof(char));
    return split;
}

nfs_dentry* find_child_dentry(nfs_dentry* parent, const char* 
name) {
    if (parent->inode->dentry_sons == NULL || parent->ftype != DIR) {
        return NULL;
    }
    nfs_dentry* iter = parent->inode->dentry_sons;
    while(iter != NULL) {
        if(strcmp(iter->name, name) == 0) {
            return iter;
        }
        iter = iter->brother;
    }
    DBG("sorry, we didn't find any");
    return NULL;
}

//********************************************** */
//正式开始
//************************************************* */


void super_init(struct nfs_super* super,int N, int k, int s) {
    
    super->disk_size = N * 1024;
    super->is_mounted =  1;
    super->super_bnum = 1;
    double denominator = 1.0 + 1.0 / (8192 * k) + 
                         1.0 / 8192 + (double)s / (1024 * k);
    super->data_bnum  = (N - 1) / denominator - 10;//留点余地
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

void insert_dentry (struct nfs_inode* inode, nfs_dentry** dentry, FILE_TYPE ftype) {
    //分配创建,不知道这种大包大揽好不好哈哈哈哈哈哈
    *dentry = (nfs_dentry*) malloc(sizeof(nfs_dentry));
    (*dentry)->ftype = ftype;
    (*dentry)->parent = inode->dentry_self;
    (*dentry)->brother = NULL;
    
    //附加到parent上
    (*dentry)->brother = inode->dentry_sons;
    inode->dentry_sons = *dentry;
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
        int byte_num = i / 8;
        uint8_t mask = 0b1 << (7 - i % 8);
        uint8_t tem  = (super.bitmap_inode)[byte_num];
        if ((tem & mask) == 0) {
            tem |= mask;
            (super.bitmap_inode)[byte_num] = tem; //更新bitmap
            is_empty = 1; //标志位更新
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
    //安全检查
    if (inode == NULL) {
        return;
    }

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
        if (sons == NULL) return;
        do {
            sync_inode_to_disk(sons->inode); //问题： 如果有data怎么办？
        } while(sons->brother != NULL);
    }
     
    if(inode->data != NULL){
        int data_blk_id   = inode->ino * DATABLOCK_PER_INODE;
        int data_loc_disk = data_loc_in_disk(data_blk_id);
        casual_write(data_loc_disk, (char*)inode->data, inode->size);
    }

    //two bitmap
    sync_bitmap_to_disk(inode);
}

void sync_bitmap_to_disk(nfs_inode* inode) {
    //inode_map
    int loc = super.bitmap_inode_loc_d + inode->ino / 8;
    char tem;
    casual_read(loc, &tem, 1);
    int resuial = inode->ino % 8;
    uint8_t mask = 0b1 << (7 - resuial);
    uint8_t to_be_write = (uint8_t)tem | mask;
    casual_write(loc, (char*)(&to_be_write), 1);

    //data_map
    // 为了可读性和代码容易写，牺牲性能了
    if (inode->data != NULL) {
        int loc = super.bitmap_data_loc_d ;
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

void sync_super_to_disk() {
    casual_write(0, (char*)&super, sizeof(nfs_super)); //我还是想着nfs_super共用就算了
}

///**
// * @brief 多功能查找
// * 1. 如果查找到了，直接返回
// * 2. 如果没找到， 返回上级目录(inode or dentry?)
// */
//nfs_dentry* general_find (const char* path, boolean* is_found, const nfs_dentry* //cur_dentry) {
//   // if (calc_path_level(path) == 0) {
//   //     is_found = 1;
//   //     return (super.root_inode)->dentry_self;
//   // }
//    if (strcmp(path, cur_dentry->name)== 0) {
//        //当前比对上了
//        is_found = 1;
//        return cur_dentry;
//    }
//    nfs_inode* cur_inode = cur_dentry->inode;
//    nfs_dentry* iter = cur_inode->dentry_sons;
//    if (iter == NULL) {
//        //没有儿子了，而且没比对上
//        *is_found = 0;
//        return cur_dentry;
//    } else {
//        //有儿子，继续比对儿子
//        nfs_dentry* prev = iter;
//        while(iter != NULL) {
//            
//            iter = iter->brother;
//        }
//    }
//
//    
//}
//

/**
 * 抄袭板
 */
nfs_dentry* general_find (const char* path, boolean* is_found, nfs_dentry* root_dentry) {
    *is_found = 0;
    if (calc_path_level(path) == 0) {
        *is_found = 1;
        return root_dentry;
    }

    nfs_dentry* very_begin = root_dentry;
    char* path_copy = strdup(path);
    char* token = strtok(path_copy, "/");

    while(token != NULL) {
        nfs_dentry* deeper = find_child_dentry(very_begin, token);
        if (deeper == NULL) {
            DBG("往下找不到了");
            free(path_copy);
            return very_begin;
        } else {
            token = strtok(NULL, "/");
            very_begin = deeper;
        }
    }
    DBG("成功找到");
    free(path_copy);
    *is_found = 1;
    return very_begin;
}


