	#define _XOPEN_SOURCE 700

	#include "nfs.h"
	#include "nfs_utils.h"

	/******************************************************************************
	* SECTION: 宏定义
	*******************************************************************************/
	#define OPTION(t, p)        { t, offsetof(struct custom_options, p), 1 }
	#define BLOCK_SZ  1024
	//const int DATABLOCK_PER_INODE  = 6;  /* 每个inode指向的数据块个数 */
	/******************************************************************************
	* SECTION: 全局变量
	*******************************************************************************/
	static const struct fuse_opt option_spec[] = {		/* 用于FUSE文件系统解析参数 */
		OPTION("--device=%s", device),
		FUSE_OPT_END
	};

	struct custom_options nfs_options;			 /* 全局选项 */
	struct nfs_super super; 

	//为了方便， 把全局变量单独定义

	boolean  is_init;
	struct nfs_inode* root_inode;
	struct nfs_dentry* root_dentry;
	/******************************************************************************
	* SECTION: Macro Function
	*******************************************************************************/
	#define IS_DIR(wawawa)  (wawawa->dentry_self->ftype == DIR)
	#define IS_REG(wawa)    (wawa->dentry_self->ftype == REG)
	#define IS_SYM_LINK(wa) (wa->dentry_self->ftype == SYM_LINK)
	/******************************************************************************
	* SECTION: FUSE操作定义
	*******************************************************************************/
	static struct fuse_operations operations = {
		.init = nfs_init,						 /* mount文件系统 */		
		.destroy = nfs_destroy,				 /* umount文件系统 */
		.mkdir = nfs_mkdir,					 /* 建目录，mkdir */
		.getattr = nfs_getattr,				 /* 获取文件属性，类似stat，必须完成 */
		.readdir = nfs_readdir,				 /* 填充dentrys */
		.mknod = nfs_mknod,					 /* 创建文件，touch相关 */
		.write = NULL,								  	 /* 写入文件 */
		.read = NULL,								  	 /* 读文件 */
		.utimens = nfs_utimens,				 /* 修改时间，忽略，避免touch报错 */
		.truncate = NULL,						  		 /* 改变文件大小 */
		.unlink = NULL,							  		 /* 删除文件 */
		.rmdir	= NULL,							  		 /* 删除目录， rm -r */
		.rename = NULL,							  		 /* 重命名，mv */

		.open = NULL,							
		.opendir = NULL,
		.access = NULL
	};
	/*
	┌──────────────────────────────────────────────────┐
	│  Kernel Space (内核态)                           │
	│  ┌──────────────────────┐                       │
	│  │  FUSE 内核模块        │                       │
	│  │                      │                       │
	│  │  定义了这些接口:      │                       │
	│  │  .  init     ──────────┼───→  调用用户函数     │
	│  │  . destroy  ──────────┼───→  调用用户函数     │
	│  │  .readdir  ──────────┼───→  调用用户函数     │
	│  │  .mkdir    ──────────┼───→  调用用户函数     │
	│  └──────────────────────┘                       │
	├──────────────────────────────────────────────────┤
	│  User Space (用户态)                             │
	│  ┌──────────────────────┐                       │
	│  │  你的代码             │                       │
	│  │                      │                       │
	│  │  demo_mount()        │ ← .  init 调用这个      │
	│  │  demo_umount()       │ ← . destroy 调用这个   │
	│  │  demo_readdir()      │ ← . readdir 调用这个   │
	│  │  nfs_mkdir()         │ ← .mkdir 调用这个     │
	│  └──────────────────────┘                       │
	└──────────────────────────────────────────────────┘
	*/
	/******************************************************************************
	* SECTION: 必做函数实现
	*******************************************************************************/
	/**
	 * @brief 挂载（mount）文件系统
	 * 
	 * @param conn_info 可忽略，一些建立连接相关的信息 
	 * @return void*
	 */
	void* nfs_init(struct fuse_conn_info * conn_info) {
		/* TODO: 在这里进行挂载 */
		//拿到总大小，方便分配位图
		super.fd = ddriver_open(nfs_options.device);
		super.sz_io = 512;
		super.sz_blk = BLOCK_SZ;
		is_init = 0;

		root_dentry = new_dentry("/", DIR);

		//判断是否是首次挂载
		struct nfs_super super_disk ;
		super.super_loc_d = 0;
		super_disk.super_loc_d = 0;
		ddriver_read(super.fd, (char*)&super_disk, sizeof(struct nfs_super));
		if(NFS_MAGIC == super_disk.magic && super_disk.is_mounted == 0) {
			// reload
			//为了方便，就不区分定义了
			memcpy(&super, &super_disk, sizeof(struct nfs_super));
			super.is_mounted = 1;
		} else {
			//first load
			is_init = 1;
			int disk_size;
			ddriver_ioctl(super.fd, IOC_REQ_DEVICE_SIZE, &disk_size);// 太丑了
			super_init(&super, disk_size / 1024, DATABLOCK_PER_INODE, sizeof(struct nfs_inode));
			super.magic     = NFS_MAGIC;
			//写inode-map, datamap

		}
		
		//ram 分配，公共部分
		super.bitmap_data = (uint8_t*) malloc(super.bitmap_data_bnum * BLOCK_SZ);
		super.bitmap_inode = (uint8_t*) malloc(super.bitmap_inode_bnum * BLOCK_SZ);

		//todo 空间换时间，我们决定把inode table也读进来，大型系统中往往是按需读取
		//notice seek_offset is 2 * blk_offset
		ddriver_seek(super.fd, 0, 0);
		int total_blks = 1 + super.bitmap_data_bnum +
						super.bitmap_inode_bnum +
						super.inode_bnum;
		char* buf = calloc(total_blks, BLOCK_SZ);
		ddriver_read(super.fd, buf, total_blks * BLOCK_SZ);
		//buf 的布局和磁盘是一致的，所以super变量可以复用
		memcpy(super.bitmap_inode, buf + super.bitmap_inode_loc_d, super.bitmap_inode_bnum * BLOCK_SZ);
		memcpy(super.bitmap_data,  buf + super.bitmap_data_loc_d,  super.bitmap_data_bnum * BLOCK_SZ);
		memcpy(super.inode_table,  buf + super.inode_loc_d,        super.inode_bnum * BLOCK_SZ);
		free(buf);

		printf("\n--------------------------------------------------------------------------------\n\n");
		//nfs_dump_map();
		//这里直接拿到root_inode
		if (is_init) {
			root_inode = alloc_inode(root_dentry);
			sync_inode_to_disk(root_inode);  //to finish
			sync_bitmap_to_disk(root_inode);
		}  else {
			//to-do 这里少了一个读取所有信息的逻辑
			//危险
			root_inode = alloc_inode(root_dentry);
		}
		

		return NULL;
	}

	/**
	 * @brief 卸载（umount）文件系统
	 * 
	 * @param p 可忽略
	 * @return void
	 */
	void nfs_destroy(void* p) {
		/* TODO: 在这里进行卸载 */
		//oper for safe: last sync
		sync_inode_to_disk(root_inode); //包含了sync 2 bitmap to disk
		sync_super_to_disk();
		super.is_mounted = 0;
		free_inode(root_dentry);
		ddriver_close(super.fd);

		return;
	}

	/**
	 * @brief 创建目录,只能往下创建一层
	 * 
	 * @param path 相对于挂载点的路径
	 * @param mode 创建模式（只读？只写？），可忽略
	 * @return int 0成功，否则返回对应错误号
	 */
	int nfs_mkdir(const char* path, mode_t mode) {
		/* TODO: 解析路径，创建目录 */
		boolean is_found = 0;
		nfs_dentry* found = general_find(path, &is_found, root_dentry);
		if (is_found == 1) {
			return -1;
		} else {
			nfs_inode* parent= found->inode;
			nfs_dentry* new_null = NULL;
			//to-do
			insert_dentry(parent, &new_null, DIR);
			//debug 写名字
			//name split from right
			char* fname = get_fname(path);
			strcpy(new_null->name, fname);
			alloc_inode(new_null);
			//不知道够不够
		}
		return 0;
	}

	/**
	 * @brief 获取文件或目录的属性，该函数非常重要,宝宝不喜欢
	 * 
	 * @param path 相对于挂载点的路径
	 * @param nfs_stat 返回状态
	 * @return int 0成功，否则返回对应错误号
	 */
	int nfs_getattr(const char* path, struct stat * nfs_stat) {
		/* TODO: 解析路径，获取Inode，填充nfs_stat，可参考/fs/simplefs/nfs.c的nfs_getattr()函数实现 */
		boolean	is_find, is_root;
		struct nfs_dentry* dentry = general_find(path, &is_find, root_dentry);
		if (is_find == 0) {
			return -2;
		}
		is_root = (dentry == root_dentry) ? 1 : 0;

		if(dentry == NULL || dentry->inode == NULL) {
			DBG("nfs_getattr: dentry or inode is NULL");
			return -2;
		}
		if (IS_DIR(dentry->inode)) {
			nfs_stat->st_mode = S_IFDIR | NFS_DEFAULT_PERM;
			nfs_stat->st_size = dentry->inode->dir_count * sizeof( nfs_dentry);
		}
		else if (IS_REG(dentry->inode)) {
			nfs_stat->st_mode = S_IFREG | NFS_DEFAULT_PERM;
			nfs_stat->st_size = dentry->inode->size;
		}
		else if (IS_SYM_LINK(dentry->inode)) {
			nfs_stat->st_mode = S_IFLNK | NFS_DEFAULT_PERM;
			nfs_stat->st_size = dentry->inode->size;
		}

		nfs_stat->st_nlink = 1;
		nfs_stat->st_uid 	 = getuid();
		nfs_stat->st_gid 	 = getgid();
		nfs_stat->st_atime   = time(NULL);
		nfs_stat->st_mtime   = time(NULL);
		nfs_stat->st_blksize = BLOCK_SZ;

		if (is_root) {
			nfs_stat->st_size	= 0; //烦死了， 这个是已经使用的空间大小， 我又没有维护，要不先写0吧
			nfs_stat->st_blocks = super.disk_size / BLOCK_SZ;
			nfs_stat->st_nlink  = 2;		/* !特殊，根目录link数为2 */
		}
		return 0;
	}

	/**
	 * @brief 遍历目录项，填充至buf，并交给FUSE输出
	 * 
	 * @param path 相对于挂载点的路径
	 * @param buf 输出buffer
	 * @param filler 参数讲解:
	 * 
	 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
	 *				const struct stat *stbuf, off_t off)
	* buf: name会被复制到buf中
	* name: dentry名字
	* stbuf: 文件状态，可忽略
	* off: 下一次offset从哪里开始，这里可以理解为第几个dentry
	* 
	* @param offset 第几个目录项？
	* @param fi 可忽略
	* @return int 0成功，否则返回对应错误号
	*/
	int nfs_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset,
							struct fuse_file_info * fi) {
		/* TODO: 解析路径，获取目录的Inode，并读取目录项，利用filler填充到buf，可参考/fs/simplefs/nfs.c的nfs_readdir()函数实现 */
		boolean is_found = 0;
		nfs_dentry* potential_res = general_find(path, &is_found, root_dentry);
		if (is_found == 1 && potential_res != NULL) {
			/*
			实际系统中：
			filler(buf, ".", NULL, 0);
			filler(buf, "..", NULL, 0);
			*/
			//给出子目录
			nfs_inode* parent = potential_res->inode;
			nfs_dentry* iter = parent->dentry_sons;
			while(iter != NULL) {
				filler(buf, iter->name, NULL, 0);//据说++offset 是分页模式
				iter = iter->brother;
			}
			return 0;

		}
		DBG("坏了， 没有找到");
		return 2;
	}

	/**
	 * @brief 创建文件
	 * 
	 * @param path 相对于挂载点的路径
	 * @param mode 创建文件的模式，可忽略
	 * @param dev 设备类型，可忽略
	 * @return int 0成功，否则返回对应错误号
	 */
	int nfs_mknod(const char* path, mode_t mode, dev_t dev) {
		/* TODO: 解析路径，并创建相应的文件 */
		boolean is_found;
		nfs_dentry* begin = general_find(path, &is_found, root_dentry);
		if(is_found == 0 && begin != NULL) {
			nfs_inode* parent = begin->inode;
			char* filename = get_fname(path);
			switch (mode & __S_IFMT) {
				case __S_IFREG: {
					nfs_dentry* target = NULL;
					insert_dentry(parent, &target, REG);
					strcpy(target->name, filename);
					break;
				}
				case __S_IFDIR: {
					nfs_dentry* target = NULL;
					insert_dentry(parent, &target, DIR);
					strcpy(target->name, filename);
					break;
				}
				case __S_IFLNK: {
					nfs_dentry* target = NULL;
					insert_dentry(parent, &target, SYM_LINK);	
					strcpy(target->name, filename);
					break;		
					//写了也没用
				}
			}		
		}
		return 0;
	}
		


	/**
	 * @brief 修改时间，为了不让touch报错 
	 * 
	 * @param path 相对于挂载点的路径
	 * @param tv 实践
	 * @return int 0成功，否则返回对应错误号
	 */
	int nfs_utimens(const char* path, const struct timespec tv[2]) {
		(void)path;
		return 0;
	}
	/******************************************************************************
	* SECTION: 选做函数实现
	*******************************************************************************/
	/**
	 * @brief 写入文件
	 * 
	 * @param path 相对于挂载点的路径
	 * @param buf 写入的内容
	 * @param size 写入的字节数
	 * @param offset 相对文件的偏移
	 * @param fi 可忽略
	 * @return int 写入大小
	 */
	int nfs_write(const char* path, const char* buf, size_t size, off_t offset,
					struct fuse_file_info* fi) {
		/* 选做 */
		return size;
	}

	/**
	 * @brief 读取文件
	 * 
	 * @param path 相对于挂载点的路径
	 * @param buf 读取的内容
	 * @param size 读取的字节数
	 * @param offset 相对文件的偏移
	 * @param fi 可忽略
	 * @return int 读取大小
	 */
	int nfs_read(const char* path, char* buf, size_t size, off_t offset,
				struct fuse_file_info* fi) {
		/* 选做 */
		return size;			   
	}

	/**
	 * @brief 删除文件
	 * 
	 * @param path 相对于挂载点的路径
	 * @return int 0成功，否则返回对应错误号
	 */
	int nfs_unlink(const char* path) {
		/* 选做 */
		return 0;
	}

	/**
	 * @brief 删除目录
	 * 
	 * 一个可能的删除目录操作如下：
	 * rm ./tests/mnt/j/ -r
	 *  1) Step 1. rm ./tests/mnt/j/j
	 *  2) Step 2. rm ./tests/mnt/j
	 * 即，先删除最深层的文件，再删除目录文件本身
	 * 
	 * @param path 相对于挂载点的路径
	 * @return int 0成功，否则返回对应错误号
	 */
	int nfs_rmdir(const char* path) {
		/* 选做 */
		return 0;
	}

	/**
	 * @brief 重命名文件 
	 * 
	 * @param from 源文件路径
	 * @param to 目标文件路径
	 * @return int 0成功，否则返回对应错误号
	 */
	int nfs_rename(const char* from, const char* to) {
		/* 选做 */
		return 0;
	}

	/**
	 * @brief 打开文件，可以在这里维护fi的信息，例如，fi->fh可以理解为一个64位指针，可以把自己想保存的数据结构
	 * 保存在fh中
	 * 
	 * @param path 相对于挂载点的路径
	 * @param fi 文件信息
	 * @return int 0成功，否则返回对应错误号
	 */
	int nfs_open(const char* path, struct fuse_file_info* fi) {
		/* 选做 */
		return 0;
	}

	/**
	 * @brief 打开目录文件
	 * 
	 * @param path 相对于挂载点的路径
	 * @param fi 文件信息
	 * @return int 0成功，否则返回对应错误号
	 */
	int nfs_opendir(const char* path, struct fuse_file_info* fi) {
		/* 选做 */
		return 0;
	}

	/**
	 * @brief 改变文件大小
	 * 
	 * @param path 相对于挂载点的路径
	 * @param offset 改变后文件大小
	 * @return int 0成功，否则返回对应错误号
	 */
	int nfs_truncate(const char* path, off_t offset) {
		/* 选做 */
		return 0;
	}


	/**
	 * @brief 访问文件，因为读写文件时需要查看权限
	 * 
	 * @param path 相对于挂载点的路径
	 * @param type 访问类别
	 * R_OK: Test for read permission. 
	 * W_OK: Test for write permission.
	 * X_OK: Test for execute permission.
	 * F_OK: Test for existence. 
	 * 
	 * @return int 0成功，否则返回对应错误号
	 */
	int nfs_access(const char* path, int type) {
		/* 选做: 解析路径，判断是否存在 */
		return 0;
	}	
	/******************************************************************************
	* SECTION: FUSE入口
	*******************************************************************************/
	int main(int argc, char **argv)
	{
		int ret;
		struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

		nfs_options.device = strdup("/home/students/2023311E01/ddriver");

		if (fuse_opt_parse(&args, &nfs_options, option_spec, NULL) == -1)
			return -1;
		
		ret = fuse_main(args.argc, args.argv, &operations, NULL);
		fuse_opt_free_args(&args);
		return ret;
	}