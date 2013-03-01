#ifndef XYZSH_BLOCK_H
#define XYZSH_BLOCK_H

sObject* block_new_on_gc(BOOL user_object);
#define BLOCK_NEW_GC(o) block_new_on_gc(o)

sObject* block_new_on_stack();
#define BLOCK_NEW_STACK() block_new_on_stack()

sObject* block_clone_on_gc(sObject* source, int type, BOOL user_object);
sObject* block_clone_on_stack(sObject* source, int type);
int block_gc_children_mark(sObject* self);

void block_delete_on_stack(sObject* self);
void block_delete_on_gc(sObject* self);

sCommand* sCommand_new(sStatment* statment);
void sStatment_delete(sStatment* self);
sStatment* sStatment_new(sObject* block, int sline, char* sname);
void sCommand_delete(sCommand* self);

BOOL sCommand_add_arg(sCommand* self, MANAGED char* buf, int env, int glob, char* sname, int sline);
BOOL sCommand_add_block(sCommand* self, sObject* block, char* sname, int sline);
BOOL sCommand_add_message(sCommand* self, MANAGED char* message, char* sname, int sline);
BOOL sCommand_add_env(sCommand* self, MANAGED char* name, MANAGED char* key, BOOL key_env, BOOL double_dollar, BOOL option, char* sname, int sline);
BOOL sCommand_add_env_block(sCommand* self, sObject* block, BOOL double_dollar, BOOL option, eLineField lf, char* sname, int sline);
BOOL sCommand_add_redirect(sCommand* self, MANAGED char* name, BOOL env, BOOL glob, int redirect, char* sname, int sline);

BOOL sCommand_expand_env(sCommand* command, sObject* object, sObject* nextin, sObject* nextout, struct _sRunInfo* runinfo);
BOOL sCommand_expand_env_of_redirect(sCommand* command, sObject* nextin, struct _sRunInfo* runinfo);

void sRunInfo_command_new(struct _sRunInfo* runinfo);
void sRunInfo_command_delete(struct _sRunInfo* runinfo);
BOOL sRunInfo_option(struct _sRunInfo* self, char* key);
char* sRunInfo_option_with_argument(struct _sRunInfo* self, char* key);

sObject* fun_clone_on_gc_from_stack_block(sObject* block, BOOL user_object, sObject* parent, BOOL no_stackframe);
sObject* fun_clone_on_stack_from_stack_block(sObject* block, sObject* parent, BOOL no_stackframe);

sObject* class_clone_on_gc_from_stack_block(sObject* block, BOOL user_object, sObject* parent, BOOL no_stackframe);
sObject* class_clone_on_stack_from_stack_block(sObject* block, sObject* parent, BOOL no_stackframe);

#endif

