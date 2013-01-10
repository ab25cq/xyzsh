#ifndef XYZSH_BLOCK_H
#define XYZSH_BLOCK_H

sObject* block_new_from_gc(BOOL user_object);
#define BLOCK_NEW_GC(o) block_new_from_gc(o)

sObject* block_new_from_stack();
#define BLOCK_NEW_STACK() block_new_from_stack()

sObject* block_clone_gc(sObject* source, int type, BOOL user_object);
sObject* block_clone_stack(sObject* source, int type);
int block_gc_children_mark(sObject* self);

void block_delete_stack(sObject* self);
void block_delete_gc(sObject* self);

sCommand* sCommand_new(sStatment* statment);
void sStatment_delete(sStatment* self);
sStatment* sStatment_new(sObject* block, int sline, char* sname);
void sCommand_delete(sCommand* self);

void sCommand_add_arg_to_command(sCommand* self, MANAGED char* buf, int env, int glob);
BOOL sCommand_add_message(sCommand* self, MANAGED char* message);
void sCommand_add_env_block(sCommand* self, sObject* block, BOOL double_dollar, eLineField lf);
void sCommand_add_env(sCommand* self, MANAGED char* name, MANAGED char* key, BOOL key_env, BOOL double_dollar);
void sCommand_add_redirect_to_command(sCommand* self, MANAGED char* name, BOOL env, BOOL glob, int redirect);

sObject* fun_clone_from_stack_block_to_gc(sObject* block, BOOL user_object, sObject* parent, BOOL no_stackframe);
sObject* fun_clone_from_stack_block_to_stack(sObject* block, sObject* parent, BOOL no_stackframe);

sObject* class_clone_from_stack_block_to_gc(sObject* block, BOOL user_object, sObject* parent, BOOL no_stackframe);
sObject* class_clone_from_stack_block_to_stack(sObject* block, sObject* parent, BOOL no_stackframe);

#endif

