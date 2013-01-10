/*
 * ���X�g�R���e�i���C�u����
 */

#ifndef XYZSH_LIST_H
#define XYZSH_LIST_H

#define BOOL int
#define TRUE 1
#define FALSE 0

///////////////////////////////////////////////////////////////////////
// �֐��錾
///////////////////////////////////////////////////////////////////////
// ������
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

int list_count(sObject* self);                      // �T�C�Y
BOOL list_empty(sObject* self);                    // �󂩂ǂ���
int list_index_of(sObject* self, void* item);      // �A�C�e���̃C���f�b�N�X
void list_push_front(sObject* self, void* item);   // �A�C�e����O����ǉ�
void list_push_back(sObject* self, void* item);    // �A�C�e������납��ǉ�
void* list_pop_front(sObject* self);               // �O����A�C�e�������o��
void* list_pop_back(sObject* self);                // ��납��A�C�e�������o��
void list_clear(sObject* self);                    // �A�C�e�����N���A

list_it* list_begin(sObject* self);                // �擪�̃��X�g�C�e���[�^�[�����o��
list_it* list_last(sObject* self);                 // �Ō�̃��X�g�C�e���[�^�[�����o��
list_it* list_at(sObject* self, int index);        // �C�ӂ̏ꏊ�̃��X�g�C�e���[�^�[����肾��
list_it* list_find(sObject* self, void* item);     // �����̃A�C�e���̃C�e���[�^�[�����o��
void* list_item(list_it* self);                     // �C�e���[�^�[����A�C�e�������o��
list_it* list_next(list_it* self);                  // �C�e���[�^�[���玟�̃C�e���[�^�[�����o��
list_it* list_prev(list_it* self);                  // �C�e���[�^�[����O�̃C�e���[�^�[�����o��
void list_replace(list_it* self, void* item);       // �C�e���[�^�[�̃A�C�e����u��������
list_it* list_erase(list_it* self, sObject* owner);    // �C�e���[�^�[�̃A�C�e�����폜����B����IT��Ԃ�
void list_insert_front(list_it* self, void* item, sObject* owner);     // �C�e���[�^�[�̑O�ɃA�C�e����ǉ�����
void list_insert_back(list_it* self, void* item, sObject* owner);      // �C�e���[�^�[�̌��ɃA�C�e����ǉ�����

/*
    �S������
    
    for(list_it* i=list_begin(gList); i; i=list_next(i)) {
        sItem* item = (sItem*)list_item(i);
    }
*/

int list_gc_children_mark(sObject* self);
unsigned int list_size(sObject* self);

#endif
