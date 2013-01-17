/*
 * ÉäÉXÉgÉRÉìÉeÉiÉâÉCÉuÉâÉä
 */

#ifndef XYZSH_LIST_H
#define XYZSH_LIST_H

#define BOOL int
#define TRUE 1
#define FALSE 0

///////////////////////////////////////////////////////////////////////
// ä÷êîêÈåæ
///////////////////////////////////////////////////////////////////////
// èâä˙âª
#ifdef MDEBUG
sObject* list_new_debug_on_malloc(const char* fname, int line, const char* func_name);
#define LIST_NEW_MALLOC() list_new_debug_on_malloc(__FILE__, __LINE__, __FUNCTION__)
#else
sObject* list_new_on_malloc();
#define LIST_NEW_MALLOC() list_new_on_malloc()
#endif

sObject* list_new_on_gc(BOOL user_object);
#define LIST_NEW_GC(user_object) list_new_on_gc(user_object)

sObject* list_new_on_stack();
#define LIST_NEW_STACK() list_new_on_stack()

void list_delete_on_malloc(sObject* self);
void list_delete_on_stack(sObject* self);
void list_delete_on_gc(sObject* self);

int list_count(sObject* self);                      // ÉTÉCÉY
BOOL list_empty(sObject* self);                    // ãÛÇ©Ç«Ç§Ç©
int list_index_of(sObject* self, void* item);      // ÉAÉCÉeÉÄÇÃÉCÉìÉfÉbÉNÉX
void list_push_front(sObject* self, void* item);   // ÉAÉCÉeÉÄÇëOÇ©ÇÁí«â¡
void list_push_back(sObject* self, void* item);    // ÉAÉCÉeÉÄÇå„ÇÎÇ©ÇÁí«â¡
void* list_pop_front(sObject* self);               // ëOÇ©ÇÁÉAÉCÉeÉÄÇéÊÇËèoÇ∑
void* list_pop_back(sObject* self);                // å„ÇÎÇ©ÇÁÉAÉCÉeÉÄÇéÊÇËèoÇ∑
void list_clear(sObject* self);                    // ÉAÉCÉeÉÄÇÉNÉäÉA

list_it* list_begin(sObject* self);                // êÊì™ÇÃÉäÉXÉgÉCÉeÉåÅ[É^Å[ÇéÊÇËèoÇ∑
list_it* list_last(sObject* self);                 // ç≈å„ÇÃÉäÉXÉgÉCÉeÉåÅ[É^Å[ÇéÊÇËèoÇ∑
list_it* list_at(sObject* self, int index);        // îCà”ÇÃèÍèäÇÃÉäÉXÉgÉCÉeÉåÅ[É^Å[ÇéÊÇËÇæÇ∑
list_it* list_find(sObject* self, void* item);     // à¯êîÇÃÉAÉCÉeÉÄÇÃÉCÉeÉåÅ[É^Å[ÇéÊÇËèoÇ∑
void* list_item(list_it* self);                     // ÉCÉeÉåÅ[É^Å[Ç©ÇÁÉAÉCÉeÉÄÇéÊÇËèoÇ∑
list_it* list_next(list_it* self);                  // ÉCÉeÉåÅ[É^Å[Ç©ÇÁéüÇÃÉCÉeÉåÅ[É^Å[ÇéÊÇËèoÇ∑
list_it* list_prev(list_it* self);                  // ÉCÉeÉåÅ[É^Å[Ç©ÇÁëOÇÃÉCÉeÉåÅ[É^Å[ÇéÊÇËèoÇ∑
void list_replace(list_it* self, void* item);       // ÉCÉeÉåÅ[É^Å[ÇÃÉAÉCÉeÉÄÇíuÇ´ä∑Ç¶ÇÈ
list_it* list_erase(list_it* self, sObject* owner);    // ÉCÉeÉåÅ[É^Å[ÇÃÉAÉCÉeÉÄÇçÌèúÇ∑ÇÈÅBéüÇÃITÇï‘Ç∑
void list_insert_front(list_it* self, void* item, sObject* owner);     // ÉCÉeÉåÅ[É^Å[ÇÃëOÇ…ÉAÉCÉeÉÄÇí«â¡Ç∑ÇÈ
void list_insert_back(list_it* self, void* item, sObject* owner);      // ÉCÉeÉåÅ[É^Å[ÇÃå„ÇÎÇ…ÉAÉCÉeÉÄÇí«â¡Ç∑ÇÈ

/*
    ëSëñç∏ó·
    
    for(list_it* i=list_begin(gList); i; i=list_next(i)) {
        sItem* item = (sItem*)list_item(i);
    }
*/

int list_gc_children_mark(sObject* self);
unsigned int list_size(sObject* self);

#endif
