#include <stddef.h>
#include "cache.h"


int init_cache(int cache_size, int max_obj_size){
	max_cache_size = cache_size;
	max_object_size = max_obj_size;
}

int write_cache(char* id, char* data){
	if (max_cache_size == NULL || max_object_size == NULL){
		return -1; //Cache not initialized
	}
	return 0; //Not written to cache
}

char* read_cache(char* id){
	if (max_cache_size == NULL || max_object_size == NULL){
		return NULL; //Cache not initialized
	}
	return NULL; //Cache miss
}