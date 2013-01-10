/*
 * dynamic array library
 */

#ifndef XYZSH_VECTOR_H
#define XYZSH_VECTOR_H

#ifndef MDEBUG
sObject* vector_new_from_malloc(int first_size);
#else
sObject* vector_new_from_malloc_debug(int first_size, const char* fname, int line, const char* func_name);
#endif
sObject* vector_new_from_gc(int first_size, BOOL user_object);
sObject* vector_new_from_stack(int first_size);

#ifndef MDEBUG
#define VECTOR_NEW_MALLOC(size) vector_new_from_malloc(size)
#else
#define VECTOR_NEW_MALLOC(size) vector_new_from_malloc_debug(size, __FILE__, __LINE__, __FUNCTION__)
#endif
#define VECTOR_NEW_STACK(size) vector_new_from_stack(size)
#define VECTOR_NEW_GC(size, user_object) vector_new_from_gc(size, user_object)

sObject* vector_clone(sObject* v);                        // clone

#define vector_item(o, i) SVECTOR(o).mTable[i]
#define vector_count(o) SVECTOR(o).mCount

void vector_delete_malloc(sObject* self);
void vector_delete_stack(sObject* self);
void vector_delete_gc(sObject* self);

int vector_index(sObject* self, void* item);                 // return item index

void vector_exchange(sObject* self, int n, void* item);      // replace item at index position
void vector_add(sObject* self, void* item);                  // add item at last position
void vector_insert(sObject* self, int n, void* item);        // insert item to index position
void vector_mass_insert(sObject* self, int n, void** items, int items_size);
void* vector_erase(sObject* self, int n);                    // delete item at the position
void* vector_pop_back(sObject* self);                        // pop last item
void* vector_pop_front(sObject* self);                       // pop first item
void vector_clear(sObject* self);                            // clear all items

typedef int (*sort_if)(void* left, void* right);
void vector_sort(sObject* self, sort_if fun);

void vector_add_test(sObject* self, void* item, void* t);
int vector_gc_children_mark(sObject* self);

unsigned int vector_size(sObject* self);

#endif
