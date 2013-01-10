/*
 * hash table container library
 */

#ifndef XYZSH_HASH_H
#define XYZSH_HASH_H

#define HASH_KEYSIZE_PER_ONE_IT 5

///////////////////////////////////////////////////////////////////////
// function definition
///////////////////////////////////////////////////////////////////////

#ifndef MDEBUG
sObject* hash_new_from_malloc(int size);
#define HASH_NEW_MALLOC(size) hash_new_from_malloc(size)
#else
sObject* hash_new_from_malloc_debug(int size, const char* fname, int line, const char* func_name);
#define HASH_NEW_MALLOC(size) hash_new_from_malloc_debug(size, __FILE__, __LINE__, __FUNCTION__)
#endif

sObject* hash_new_from_gc(int size, BOOL user_object);
sObject* hash_new_from_stack(int size);

#define HASH_NEW_GC(size, user_object) hash_new_from_gc(size, user_object)
#define HASH_NEW_STACK(size) hash_new_from_stack(size)

void hash_delete_gc(sObject* self);
void hash_delete_stack(sObject* self);
void hash_delete_malloc(sObject* self);

void hash_put(sObject* self, char* key, void* item);
BOOL hash_erase(sObject* self, char* key);
void hash_clear(sObject* self);
void hash_replace(sObject* self, char* key, void* item);

void hash_show(sObject* self, char* fname);

void* hash_item(sObject* self, char* key);
void* hash_item_addr(sObject* self, char* key);
char* hash_key(sObject* self, void* item);
int hash_count(sObject* self);


hash_it* hash_loop_begin(sObject* self);
void* hash_loop_item(hash_it* it);
char* hash_loop_key(hash_it* it);
hash_it* hash_loop_next(hash_it* it);

/*
 * access all items
 *
 * hash_it* i = hash_loop_begin(hash);
 * while(it) {
 *     void* obj = hash_loop_item(it);
 *
 *     // using obj
 *
 *     it = hash_loop_next(it);
 * }
 *
 */

int hash_gc_children_mark(sObject* self);
void hash_it_release(hash_it* it, sObject* hash);
hash_it* hash_it_new(char* key, void* item, hash_it* coll_it, hash_it* next_it, sObject* hash) ;

unsigned int hash_size(sObject* self);

#endif
