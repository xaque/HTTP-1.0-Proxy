#ifndef __CACHE_H__
#define __CACHE_H__

int init_cache(int cache_size, int max_obj_size);
int write_cache(char* id, char* data);
char* read_cache(char* id);
void free_cache();

#endif /* __CACHE_H__ */