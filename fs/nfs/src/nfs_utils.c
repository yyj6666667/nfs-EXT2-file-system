#include <stdlib.h>
#include <string.h>
#include "../include/nfs_utils.h"
//#define DEBUG_UTILS


//#define UGLY
//------------------------------------------/
//工具函数的工具函数
//------------------------------------------/

int inode_loc_in_disk(nfs_inode* inode) {
    int offset_bnum = super.super_bnum + super.bitmap_inode_bnum
                    + super.bitmap_data_bnum;
    int offset_1 = offset_bnum * BLK_SZ;
    int bias = (inode->ino) * sizeof(nfs_inode_d);
    int offset_final = offset_1 + bias;
    return offset_final;
}

int data_loc_in_disk(nfs_inode* inode) {
    return super.data_begin_loc + inode->ino * BLK_INODE * BLK_SZ;
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
    return NULL;
}

//********************************************** */
//正式开始
//************************************************* */


void super_init(struct nfs_super* super,int N, int k, int s, int nfs_magic) {
    
    super->magic = nfs_magic;
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
    super->bitmap_inode = calloc(1, super->bitmap_inode_bnum * BLK_SZ);
    super->bitmap_data  = calloc(1, super->bitmap_data_bnum * BLK_SZ);
    return;
}

struct nfs_dentry* new_dentry(char* filename, FILE_TYPE ftype) {
    nfs_dentry* new;
#ifdef UGLY
    new = (struct nfs_dentry*) malloc(sizeof(struct nfs_dentry));
    memcpy(new->name, filename, strlen(filename));
    //这段代码错在strcpy， 最后的/0没有写， 是malloc的随机垃圾值， 造成了相当严重的错误
#else
    int random = 1;
    switch(random) {
        case 1: {
            // calloc 比 malloc 更安全， 因为它会自动memset字段为0
            new = (nfs_dentry*) calloc(1, sizeof(nfs_dentry));
            memcpy(new->name, filename, strlen(filename));
            break;
        }
        case 2: {
            new = (nfs_dentry*) malloc(sizeof(nfs_dentry));
            memcpy(new->name, filename, strlen(filename));
            *(new->name + strlen(filename)) = '\0'; // or = (char) 0
            break;
        }
        case 3: {
            //modern c write style
            new = (nfs_dentry*) malloc(sizeof(nfs_dentry));
            strcpy(new->name, filename);
            break;
        }
    }  
#endif
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
    char* buf = (char*) calloc(1, size_for_ddriver);

    ddriver_seek(super.fd, offset_for_ddriver, 0);
    int   iter_num = size_for_ddriver / IO_SZ;
    for (int i = 0; i < iter_num; i++) {
        ddriver_read(super.fd, buf + i * IO_SZ, IO_SZ);
    }

    char* real_start = buf + bias;
    memcpy(out, real_start, size);
    free(buf);
}

int casual_write(int offset, char* input, int size) {
    if(input == NULL) {
        DBG("input指针为null，没法写");
        assert(input != NULL);
        return -1;
    }
    if (offset < 0 || size <= 0) {
        return -1;
    }
    int offset_for_ddriver = (offset / IO_SZ) * IO_SZ;
    int bias = offset - offset_for_ddriver;
    int size_for_ddriver   = ((size + bias + IO_SZ - 1) / IO_SZ) * IO_SZ;
    int   iter = size_for_ddriver / IO_SZ;
    char* input_dd         = (char*) calloc(1, size_for_ddriver);
    casual_read(offset_for_ddriver, input_dd, size_for_ddriver);
    memcpy(input_dd + bias, input, size);

    ddriver_seek(super.fd, offset_for_ddriver, 0);
    // if size is too large , print the process percent
    boolean print_flag = 0;
    int point[10];
    if (size >= BLK_SZ * 10) {
        print_flag = 1;
        if (print_flag) {
            for (int j = 0; j < 9; j++) {
                point[j] = iter / 10 * j;
            }
        }
    }
    printf("casual_write: writing data of size: %d\n", size);
    for(int i = 0; i < iter; i++) {
        ddriver_write(super.fd, input_dd + i * IO_SZ, IO_SZ);
        if (print_flag) {
            printf(". ");
        }
    }
    free(input_dd);
    return 0;
}

void insert_dentry (struct nfs_inode* inode, nfs_dentry** dentry, FILE_TYPE ftype) {
    //分配创建,不知道这种大包大揽好不好哈哈哈哈哈哈
    *dentry = (nfs_dentry*) calloc(1, sizeof(nfs_dentry));
    (*dentry)->ftype = ftype;
    (*dentry)->parent = inode->dentry_self;
    (*dentry)->brother = NULL;
    
    //附加到parent上
    (*dentry)->brother = inode->dentry_sons;
    inode->dentry_sons = *dentry;
    inode->child_count += 1;
}
/**
 * @param inode : dentry's parent inode
 */
void remove_dentry (nfs_inode* inode, nfs_dentry* dentry) {
    if (inode == NULL) {
        DBG("remove_dentry: parent inode not founded\n");
        return;
    }

    nfs_dentry* tem = inode->dentry_sons;
    nfs_dentry* prev = NULL;

    if (tem == NULL) {
        DBG("remove_dentry: supposed dentry not found\n");
        return;
    }
    for(; tem != NULL; prev = tem, tem = tem->brother) {
        if (tem == dentry) {
            if (prev == NULL) {
                //在头结点就找到了
                inode->dentry_sons = tem->brother;
                inode->child_count -= 1;
                break;
            } else {
                prev->brother = tem->brother;
                inode->child_count -= 1;
                break;
            }
        }
    }

    //one dentry refers to an inode, so inode bitmap change:
    int cursor = dentry->ino;
    int byte   = cursor / 8;
    uint8_t mask = 0b1 << (7 - cursor % 8);
    uint8_t origin  = super.bitmap_inode[byte];
    if ((mask & origin) == 0) {
        DBG("remove_dentry: 原始map inode有错\n");
        return;
    }
    uint8_t now = origin & (~mask);
    super.bitmap_inode[byte] = now;

    char string[100];
    strcpy(string, dentry->name);
    //free
    if (dentry->inode){
        free(dentry->inode);
        if (dentry->inode->data)
            free(dentry->inode->data);
    }
    free(dentry);
    printf("remove_dentry: ops is done， %s 已经成功释放\n", string);
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
            ino_cursor = i;
            break;
        }
    }
    if (is_empty && ino_cursor < super.inode_num) {
        nfs_inode* new = (nfs_inode*) calloc(1, sizeof(nfs_inode));
        new->ino = ino_cursor;
        dentry->ino = new->ino;
         printf("给新的dentry分配的ino号为：%d\n", dentry->ino);
        new->size = 0;
        dentry->inode = new;
        new->dentry_self = dentry;
        new->dentry_sons = NULL;
        new->child_count = 0;
        new->data = (uint8_t*) calloc(1, BLK_INODE * BLK_SZ);
        return new;
    } else {
        DBG("没有空闲inode，分配失败");
        return NULL;
    }
}


/**
 * @brief this func is both used in destroy() and rmdir(), it can change the bitmap, which may cause danger to bitmap_sync, but the code order in nfs.c avoid this danger
 */
void free_inode_recursively_ram(nfs_dentry* dentry) {
    
    nfs_inode* inode = dentry->inode;
    //检查是否到递归的最后一层    
    if(inode->dentry_sons != NULL) {
        //递归进入子目录
        nfs_dentry* iter = inode->dentry_sons;
        while (iter != NULL) {
            nfs_dentry* next = iter->brother;
            free_inode_recursively_ram(iter);
            iter = next;
        }
    }
    if (dentry->parent != NULL)
        remove_dentry(dentry->parent->inode, dentry);
    return;
}

void sync_inode_to_disk(nfs_inode *inode) {
    //安全检查
    if (inode == NULL) {
        return;
    }

    nfs_inode_d* inode_buf = (nfs_inode_d*) calloc(1, sizeof(nfs_inode_d));
    inode_buf->size = inode->size;
    inode_buf->ino  = inode->ino;
    inode_buf->child_count = inode->child_count;
 //  for (int i = inode->ino * BLK_INODE, j = 0; 
 //      j < BLK_INODE; j++, i++) {
 //          *(inode_buf->direct_data + j) = i;
 //      }
    int inode_loc_d = inode_loc_in_disk(inode);
    //写inode
    casual_write(inode_loc_d, (char*)inode_buf, sizeof(nfs_inode_d));
    //写数据
    int data_loc_disk = data_loc_in_disk(inode);
    printf("把数据写到地址: %d \n", data_loc_disk);
    #ifdef DEBUG_UTILS
        nfs_dentry_d* check_0 = (nfs_dentry_d*) inode->data;
        if (check_0->ftype == DIR) ;
    #endif
    casual_write(data_loc_disk, inode->data, inode->size);   

    #ifdef DEBUG_UTILS
        nfs_dentry_d* check_1 = NULL;
        char* temp_1 = (char*) calloc(1, BLK_SZ * BLK_INODE);
        casual_read(data_loc_disk, temp_1, BLK_SZ * BLK_INODE);
        check_1 = (nfs_dentry_d*) temp_1;
    #endif
    free(inode_buf);

    //好了，既然本体已经写入，那么开始愉快的递归吧, 我发现有的递归逻辑写在前面，有的写在后面
    FILE_TYPE type = inode->dentry_self->ftype;
    if(type == DIR) {
        nfs_dentry* sons = inode->dentry_sons;
        if (sons == NULL) return;
        for(nfs_dentry* cur = sons; cur; cur = cur->brother) {
            sync_inode_to_disk(cur->inode);
        }
    }
}

void sync_bitmap_to_disk(nfs_inode* inode) {
    #ifdef UGLY
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
        int start_bits = inode->ino * BLK_INODE;
        for (int i = 0; i < BLK_INODE; i++) {
            int cur_loc = loc + (start_bits + i) / 8;
            char tem;
            casual_read(cur_loc, &tem, 1);
            int resuial = (start_bits + i) % 8;
            uint8_t mask = 0b1 << (7 - resuial);
            tem |= mask;
            casual_write(cur_loc, &tem, 1);
        }
    }
    #else
    if(casual_write(super.bitmap_inode_loc_d, (char*)(super.bitmap_inode), super.bitmap_inode_bnum * BLK_SZ) != 0) {
        DBG("随机写错误");
    }
    if(casual_write(super.bitmap_data_loc_d, (char*)(super.bitmap_data), super.bitmap_data_bnum * BLK_SZ) != 0) {
        DBG("随机写错误");
    }
    #endif
}

void sync_super_to_disk() {
    casual_write(0, (char*)&super, sizeof(nfs_super)); //我还是想着nfs_super共用就算了
}

void free_super_ram() {
    free(super.bitmap_inode);
    free(super.bitmap_data);
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
 * 借鉴板
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

/**
 * @brief 根据ino， 读取disk中的data段到buf， 返回buf指针
 */
char* read_inode_data_disk(int ino){
    int loc_d = super.data_loc_d + ino * BLK_INODE * BLK_SZ;
    char* buf = (char*) calloc(BLK_INODE, BLK_SZ);

    ddriver_seek(super.fd, loc_d, 0);
    int read_times = BLK_INODE * BLK_SZ / IO_SZ;
    for (int i = 0; i < read_times; i++)
        ddriver_read(super.fd, buf + i * IO_SZ, IO_SZ);
    return buf;
}

/**
 * @param inode : parent inode, go on constructing from it
 */
int rebuilt_by_inode(nfs_inode* inode, nfs_super* super){
    switch(inode->dentry_self->ftype) {
        case DIR : {
            int check_num = inode->size / sizeof(nfs_dentry_d);
            if (check_num == inode->child_count || 1) {
                char* data = read_inode_data_disk(inode->ino);
                nfs_dentry_d* iter = (nfs_dentry_d*) data;
                for (int i = 0; i < inode->child_count; i++) {
                    nfs_dentry* dentry_to_add = new_dentry(iter->name, iter->ftype);
                    //debug
                    printf("重建时从磁盘中获取的名字：%s\n", iter->name);
                    dentry_to_add->parent = inode->dentry_self;
                    //头结点插入
                    dentry_to_add->brother = inode->dentry_sons;
                    inode->dentry_sons = dentry_to_add;
                    nfs_inode* inode_to_add = restore_inode(dentry_to_add, iter->ino);
                    //潜在的递归, 是DIR就继续往下走
                    if (iter->ftype == DIR && inode_to_add != NULL) {
                        rebuilt_by_inode(inode_to_add, super);
                    }
                    iter++;
                }
                //调用深了有堆溢出风险哈哈哈
                free(data);
            } else {
                DBG("inode's size don't match its child_count\n");
                return -1;
            }
            break;
        }
        case REG : {
            //我想实现懒汉式加载， 所以在重建期就不读数据到ram了
            break;
        }
        case SYM_LINK : {
            break;
        }
    }
    return 0;
}

int total_rebuilt_from_disk(nfs_super* super, nfs_super* super_disk, nfs_inode* root_inode) {
    //read 2 bitmap, 这个会把inode_size之外的碎片0也读进来
    casual_read(super->bitmap_inode_loc_d, (char*)(super->bitmap_inode), super->bitmap_inode_bnum * BLK_SZ);
    casual_read(super->bitmap_data_loc_d, (char*)(super->bitmap_data), super->bitmap_data_bnum * BLK_SZ);
    //read all dentry, construct trees in ram
    rebuilt_by_inode(root_inode, super);
    super->is_mounted = 1;
    return 0;
}

nfs_inode* restore_inode(nfs_dentry* dentry, int ino) {
    nfs_inode* inode = (nfs_inode*) calloc(1, sizeof(nfs_inode));
    inode->ino = ino;
    dentry->ino = ino;
    dentry->inode = inode;
    inode->dentry_self = dentry;
    nfs_inode_d inode_d;
    int loc = inode_loc_in_disk(inode);
    casual_read(loc, (char*)&inode_d, sizeof(nfs_inode_d));
    inode->size = inode_d.size;
    inode->child_count = inode_d.child_count;
    inode->data = (uint8_t*) calloc(1, BLK_INODE * BLK_SZ);
    char* disk_data = read_inode_data_disk(ino);
    memcpy(inode->data, disk_data, BLK_INODE * BLK_SZ);
    free(disk_data);
    return inode;
}

