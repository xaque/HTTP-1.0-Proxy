#ifndef __CACHE_H__
#define __CACHE_H__

int max_cache_size;
int max_object_size;

int init_cache(int cache_size, int max_obj_size);
int write_cache(char* id, char* data);
char* read_cache(char* id);

#endif /* __CACHE_H__ */