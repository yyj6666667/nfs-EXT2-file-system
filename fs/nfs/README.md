# 你的README文档
* 重建期， 读进来的第一个文件夹s0, 应该是ino = 1, name = s0, 现在ino = 1050, name 也不对
* 两种可能： 数据成功写入磁盘， 读取时出错  或者  数据在last mount期间 sync to disk 错误
* 找到了， 是root_dentry->data 写入dentry_d 时错误
* 破案了， sizeof( a struct contains pointer ) , 自己品吧， 改了3个小时， 这绝对是我改过的最笨的bug！