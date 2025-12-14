# user-land-filesystem, a simple implement of EXT2 filesystem
## 世界破破烂烂，小猫缝缝补补

debug log:

---
12.12
* is_mounted 没有即时刷新到磁盘，造成下次加载remount分支无法进入
* super->inode_table 没有必要维护， 属于开发遗留
---
12.13
* 需要解决一个全新挂载的问题， 新增维护nfs_dentry_d (on disk)
* 注意，把磁盘全部读进来很不经济， general_find是在ram端全部重建完成的前提下写的， 如果要实现按需读取目录，需要重写面向磁盘的general_find
* 注意 alloc_inode 过后没有传buf往里面写数据
---
12.14
* root_dentry 存到super里面， 指向第一个inode， 是最经济的
* 有疑问， rebuilt_by_inode(), nfs_inode* super按值传递调用了很深的栈，会有问题吗？