## 第一阶段

---

### Redis数据结构方面

- 内存分配、

  ​	`zmalloc.h , zmalloc.c` 

- 动态字符串

  ​	`sds.h , sds.c`

- 双端链表

  ​	`adlist.h , adlist.c`

- 字典

  ​	`dict.h , dict.c`

- 跳跃表

  ​	`server.h` 中涉及`zskiplist`结构与`zskiplistNode`结构

  ​	`t_zest.c` 中所有 `zsl`开头的函数

- 日志类型

  ​	`hyperloglog.c` 中 `hllhdr`结构，以及所有`hll`开头的函数

---

## 第二阶段

---

### Redis的内存编码结构

- 整数集合数据结构

  ​	`intset.h , intset.c`

- 压缩列表数据结构

  ​	`ziplist.h , ziplist.c`

---

## 第三阶段

---

### Redis数据类型实现

- 对象系统

  ​	`object.c`

- 字符串键

  ​	`t_string.c`

- 列表键

  ​	`t_list.c`

- 散列键

  ​	`t_hash.c`

- 集合键

  ​	`t_set.c`

- 有序集合键

  ​	`t_zset.c` 中除`zsl`开头的所有函数

- `HyperLogLog`键

  ​	`hyperloglog.c`中所有以`pf`开头的函数

---

## 第四阶段

---

### Redis数据库的实现

- 数据库实现

  ​	`redis.h`中的 `redisDb`结构，以及 `db.c`

- 通知功能

  ​	`notify.c`

- RDB持久化

  ​	`rdb.c`

- AOF持久化

  ​	`aof.c`

- 发布与订阅

  ​	`redis.h`中的`pubsubPattern`结构，以及`pubsub.c`

- 事务

  ​	`redis.h`中的`multistate`结构，以及`multi.c`

---

## 第五阶段

---

### 客户端与服务器端代码实现

- 事件处理模块

  ​	`ae.c, ae_epoll.c, ae_evport.c, ae_kqueue.c, ae_select.c`

- 网络连接库

  ​	`anet.c, netowrking.c`

- 服务器端

  ​	`redis.c`

- 客户端

  ​	`redis-cli.c`

### 独立功能模块

- lua脚本

  ​	`scripting.c`

- 慢查询

  ​	`slowlog.c`

- 监视

  ​	`monitor.c`

---

## 第六阶段

---

### Redis多机部分

- 复制功能

  ​	`replication.c`

- Redis Sential

  ​	`sential.c`

- 集群

  ​	`cluster.c`

---

## 第七阶段

---

### 测试

- 内存检测

  ​	`memtest.c`

- Redis性能测试

  ​	`redis_benchmark.c`

- 更新日志检查

  ​	`redis_check_aof.c`

- 本地数据库检查

  ​	`redis_check_dump.c`

- C风格小型测试框架

  ​	`testhelp.c`

---

## 差缺补漏

---

### 工具类文件

- `bitops.c`

  ​	GETBIT, SETBIT等二进制操作命令的实现

- `debug.c`

  ​	用于调试

- `endianconv.c`

  ​	高低位转换，不同系统高低位不同

- `help.h`

  ​	辅助于命令的提示信息

- `lzf_c.c`

  ​	压缩算法

- `lzf_d.c`

  ​	压缩算法

- `rand.c`

  ​	随机数算法

- `release.c`

  ​	发布

- `sha1.c`

  ​	SHA加密算法

- `util.c`

  ​	通用工具方法

- `crc64.c`

  ​	循环冗余检验

- `sort.c`

  ​	SORT命令的实现

### 封装类文件

- `bio.c`

  ​	background I/O，开启后台线程

- `latency.c`

  ​	延迟类

- `migrate.c`

  ​	命令迁移类，包括命令的还原迁移

- `pqsort.c`

  ​	排序算法类

- `rio.c`

  ​	Redis定义的一个 I/O 类

- `syncio.c`

  ​	同步 socket和文件 I/O 操作

---

## 有用的链接

---

[redis源码解析](https://redissrc.readthedocs.io/en/latest/#id3)

[Redis源码剖析专栏](https://zhuanlan.zhihu.com/zeecoderRedis)

[Redis 设计与实现](https://redisbook.readthedocs.io/en/latest/index.html)