# 你的README文档
* 重建期， 读进来的第一个文件夹s0, 应该是ino = 1, name = s0, 现在ino = 1050, name 也不对
* 两种可能： 数据成功写入磁盘， 读取时出错  或者  数据在last mount期间 sync to disk 错误
* 找到了， 是root_dentry->data 写入dentry_d 时错误
* 破案了， super_init 分配bnum 不合理导致数据覆写 

* 解耦的痛苦：inode size 增加时，  mkdir 导致的增加和 write regular file 导致的增加不一样， 可能在维护上带来麻烦