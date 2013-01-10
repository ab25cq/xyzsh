/*
 * リストコンテナライブラリ
 */

#ifndef XYZSH_LIST_H
#define XYZSH_LIST_H

#define BOOL int
#define TRUE 1
#define FALSE 0

///////////////////////////////////////////////////////////////////////
// 関数宣言
///////////////////////////////////////////////////////////////////////
// 初期化
#ifdef MDEBUG
sObject* list_new_debug_from_malloc(const char* fname, int line, const char* func_name);
#define LIST_NEW_MALLOC() list_new_debug_from_malloc(__FILE__, __LINE__, __FUNCTION__)
#else
sObject* list_new_from_malloc();
#define LIST_NEW_MALLOC() list_new_from_malloc()
#endif

sObject* list_new_from_gc(BOOL user_object);
#define LIST_NEW_GC(user_object) list_new_from_gc(user_object)

sObject* list_new_from_stack();
#define LIST_NEW_STACK() list_new_from_stack()

void list_delete_malloc(sObject* self);
void list_delete_stack(sObject* self);
void list_delete_gc(sObject* self);

int list_count(sObject* self);                      // サイズ
BOOL list_empty(sObject* self);                    // 空かどうか
int list_index_of(sObject* self, void* item);      // アイテムのインデックス
void list_push_front(sObject* self, void* item);   // アイテムを前から追加
void list_push_back(sObject* self, void* item);    // アイテムを後ろから追加
void* list_pop_front(sObject* self);               // 前からアイテムを取り出す
void* list_pop_back(sObject* self);                // 後ろからアイテムを取り出す
void list_clear(sObject* self);                    // アイテムをクリア

list_it* list_begin(sObject* self);                // 先頭のリストイテレーターを取り出す
list_it* list_last(sObject* self);                 // 最後のリストイテレーターを取り出す
list_it* list_at(sObject* self, int index);        // 任意の場所のリストイテレーターを取りだす
list_it* list_find(sObject* self, void* item);     // 引数のアイテムのイテレーターを取り出す
void* list_item(list_it* self);                     // イテレーターからアイテムを取り出す
list_it* list_next(list_it* self);                  // イテレーターから次のイテレーターを取り出す
list_it* list_prev(list_it* self);                  // イテレーターから前のイテレーターを取り出す
void list_replace(list_it* self, void* item);       // イテレーターのアイテムを置き換える
list_it* list_erase(list_it* self, sObject* owner);    // イテレーターのアイテムを削除する。次のITを返す
void list_insert_front(list_it* self, void* item, sObject* owner);     // イテレーターの前にアイテムを追加する
void list_insert_back(list_it* self, void* item, sObject* owner);      // イテレーターの後ろにアイテムを追加する

/*
    全走査例
    
    for(list_it* i=list_begin(gList); i; i=list_next(i)) {
        sItem* item = (sItem*)list_item(i);
    }
*/

int list_gc_children_mark(sObject* self);
unsigned int list_size(sObject* self);

#endif
