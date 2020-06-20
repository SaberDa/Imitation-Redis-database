[Chinese Version](https://github.com/SaberDa/Imitation-Redis-database/blob/master/doc/plan-ch.md)

---

# PHASE 1

## Redis data structure

- Memory allocation

  ​	`zmalloc.h , zmalloc.c` 

- Dynamic string

  ​	`sds.h , sds.c`

- Linked list

  ​	`adlist.h , adlist.c`

- Dictionary

  ​	`dict.h , dict.c`

- Skip List

- Log

---

# PHASE 2

## Redis memory coding structure

- Integer collection data structure

- Compressed list data structure

---

# PHASE 3

## Redis data type implementation

- Object system

- String key

- List key

- Hash key

- Set key

- Ordered set key

- HyperLogLog key

---

# PHASE 4

## Redis database implementation

- Database implementation

- Notification function

- RDB persistence

- AOF persistence

- Publish and subscribe

- Business

---

# PHASE 5

## Client and server code implementation

- Event processing module

- Network connection library

- Service-Terminal

- Client

## Independent function module

- lua script

- Slow query

- Monitor

---

# PHASE 6

## Redis multi-machine part

- Copy function

- Redis Sential

- Cluster

---

# PHASE 7

## Test

- Memory detection

- Redis performance test

- Update log check

- Local database check

- C style small test framework

---

# Other Functions

## Tool files

## Package file