#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <semaphore.h> 
#include "cache.h"

sem_t mutex, w;
int readcnt = 0;

typedef struct{
	char* id;
	time_t last_access;
	int size;
	char* data;
} cache_entry;

cache_entry** cache;
int max_object_size;
int max_cache_size;
int current_cache_size;
int cache_index;

int delete_oldest_cache(){
	if (cache_index < 1){
		return -1;//Cache empty, no delete
	}

	time_t oldest;
	time(&oldest);
	int oldest_index = 0;
	for (int i = 0; i < cache_index; i++){
		time_t tm = cache[i]->last_access;
		if (tm < oldest){
			oldest = tm;
			oldest_index = i;
		}
	}

	cache_entry* to_delete = cache[oldest_index];
	free(to_delete);
	for (int i = oldest_index+1; i < cache_index; i++){
		cache[i-1] = cache[i];
	}
	cache_index--;
	current_cache_size -= max_object_size;//TODO dynamic size
	free(cache[cache_index]);
	cache[cache_index] = NULL;
	return 0;//Success
}

int get_cache_index(char* id){
	for (int i = 0; i < cache_index; i++){
		if (strcmp(cache[i]->id, id) == 0){
			return i;
		}
	}
	return -1;//Not found
}

/***** Public funtions *****/

int init_cache(int cache_size, int max_obj_size){
	max_cache_size = cache_size;
	max_object_size = max_obj_size;
	current_cache_size = 0;
	cache_index = 0;
	cache = (cache_entry**) malloc(sizeof(cache_entry*)*11); //TODO max_cache_size/max_object_size instead of 11 hardcoded
	sem_init(&mutex, 0, 1);
	sem_init(&w, 0, 1);
}

int write_cache(char* id, char* data){
	if (max_cache_size == NULL || max_object_size == NULL){
		return -1; //Cache not initialized
	}
	//TODO make sure object is not bigger than max set
	//I think they are already limited by that size in proxy.c

	//Create new cache entry
	cache_entry* new_entry = (cache_entry*) malloc(max_object_size);
	//strcpy(new_entry->id, id);
	new_entry->id = id;
	time_t now;
	time(&now);
	new_entry->last_access = now;
	new_entry->size = max_object_size; //TODO size set to max for now
	//strncpy(new_entry->data, data, max_object_size);//memcpy?
	new_entry->data = data;
	
	P(&w);
	if ((current_cache_size + max_object_size) > max_cache_size){
		delete_oldest_cache();
	}
	//Write to next index
	cache[cache_index] = new_entry;
	cache_index++;
	current_cache_size += max_object_size;
	V(&w);

	return 1; //Successfully written to cache
}

char* read_cache(char* id){
	if (max_cache_size == NULL || max_object_size == NULL){
		return NULL; //Cache not initialized
	}

	P(&mutex);
	readcnt++;
	if (readcnt == 1){
		P(&w);
	}
	V(&mutex);

	int index = get_cache_index(id);
	char* data;
	if (index < 0){//Cache miss
		data = NULL;
	}
	else{//Cache hit
		time_t now;
		time(&now);
		cache[index]->last_access = now;
		data = cache[index]->data;
	}

	P(&mutex);
	readcnt--;
	if (readcnt == 0){
		V(&w);
	}
	V(&mutex);

	return data;
}

void free_cache(){
	max_cache_size = NULL;
	max_object_size = NULL;
	current_cache_size = NULL;
	cache_index = NULL;
	free(cache);
	cache = NULL;
	sem_destroy(&mutex);
	sem_destroy(&w);
}