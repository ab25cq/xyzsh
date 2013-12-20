#ifndef XYZSH_H
#define XYZSH_H

#include <termios.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdarg.h>
#include <oniguruma.h>

#include "debug.h"
#include "kanji.h"
#include "xfunc.h"

///////////////////////////////////////////////////////////////////////////////
// global definition
///////////////////////////////////////////////////////////////////////////////
#ifndef BOOL
#define BOOL int
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef ALLOC
#define ALLOC                   // indicate that a memory which should be freed after using
#endif

#ifndef MANAGED
#define MANAGED                 // indicate that a memory which is managed inside the function or object
#endif

#define kLF 0x01
#define kCRLF 0x02
#define kCR 0x03
#define kBel 0x04

typedef char eLineField;
extern eLineField gLineField;

extern enum eKanjiCode gKanjiCode;

enum eAppType { kATOptC, kATXApp, kATConsoleApp }; // Application kind
extern enum eAppType gAppType;

struct _sObject;

extern struct _sObject* gErrMsg;               // error message

extern struct _sObject* gStdin;
extern struct _sObject* gStdout;
extern struct _sObject* gStderr;

void xyzsh_init(enum eAppType app_type, BOOL no_runtime_script);
void xyzsh_final();

///////////////////////////////////////////////////////////////////////////////
// Base
///////////////////////////////////////////////////////////////////////////////
typedef struct _list_it {
   void* mItem;
   
   struct _list_it* mPrevIt;
   struct _list_it* mNextIt;
} list_it;

typedef struct {
   list_it* mEntryIt;
   list_it* mLastIt;
   int mCount;
} list_obj;

typedef struct _hash_it {
   char* mKey;
   
   void* mItem;
   struct _hash_it* mCollisionIt;

   struct _hash_it* mNextIt;
} hash_it;

typedef struct {
   hash_it** mTable;
   int mTableSize;

   hash_it* mEntryIt;

   int mCounter;
} hash_obj;

typedef struct
{
    char* mStr;
    int mLen;

    int mMallocLen;
} string_obj;

typedef struct {
   void** mTable;
   int mTableSize;
   
   int mCount;
} vector_obj;

typedef struct {
    char* mBuf;
    int mBufSize;
    int mBufLen;

    struct _sObject* mLines;
    eLineField mLinesLineField;
    unsigned int mReadedLineNumber;
} fd_obj;

typedef struct {
    uint mFD;
} fd2_obj;

typedef struct {
    char* mPath;
} external_prog_obj;

struct sStatment;

struct block_obj;
struct _sObject;

typedef struct _option_hash_it {
    char* mKey;
    char* mArg;
} option_hash_it;

#define ENV_FLAGS_KIND_ENV 0x01
#define ENV_FLAGS_KIND_BLOCK 0x02
#define ENV_FLAGS_KEY_ENV 0x10
#define ENV_FLAGS_KEY_QUOTED_STRING 0x20

typedef unsigned char eEnvKind;

typedef struct {
    int mFlags;

    union {
        struct {
            char* mName;
            char* mInitValue;
            char* mKey;
        };
        struct {
            struct _sObject* mBlock;
            eLineField mLineField;
        };
    };
} sEnv;

#define REDIRECT_KIND 0xff
#define REDIRECT_IN 0x1
#define REDIRECT_APPEND 0x2
#define REDIRECT_OUT 0x4
#define REDIRECT_ERROR_APPEND 0x8
#define REDIRECT_ERROR_OUT 0x10
#define REDIRECT_ENV 0x100
#define REDIRECT_GLOB 0x200
#define REDIRECT_QUOTED 0x400

#define XYZSH_ARGUMENT_ENV 0x01
#define XYZSH_ARGUMENT_GLOB 0x02
#define XYZSH_ARGUMENT_OPTION 0x04
#define XYZSH_ARGUMENT_QUOTED 0x08
#define XYZSH_ARGUMENT_QUOTED_HEAD 0x10

#define XYZSH_ARG_SIZE_MAX 65536*3
#define XYZSH_REDIRECT_SIZE_MAX 72
#define XYZSH_BLOCK_SIZE_MAX 72
#define XYZSH_MESSAGE_SIZE_MAX 72
#define XYZSH_ENV_SIZE_MAX 72

typedef struct {
    char** mArgs;
    int* mArgsFlags;    // This index is the same as mArgs
    int mArgsNum;
    int mArgsSize;

    sEnv* mEnvs;
    unsigned char mEnvsNum;
    unsigned char mEnvsSize;

    struct _sObject** mBlocks;     // block_obj**
    unsigned char mBlocksNum;
    unsigned char mBlocksSize;

    char** mRedirectsFileNames;
    int* mRedirects;
    unsigned char mRedirectsNum;
    unsigned char mRedirectsSize;

    char** mMessages;
    unsigned char mMessagesNum;
    unsigned char mMessagesSize;
} sCommand;

#define STATMENT_FLAGS_REVERSE 0x010000
#define STATMENT_FLAGS_BACKGROUND 0x020000
#define STATMENT_FLAGS_GLOBAL_PIPE_IN 0x040000
#define STATMENT_FLAGS_GLOBAL_PIPE_OUT 0x080000
#define STATMENT_FLAGS_GLOBAL_PIPE_APPEND 0x1000000
#define STATMENT_FLAGS_CONTEXT_PIPE 0x100000
#define STATMENT_FLAGS_NORMAL 0x200000
#define STATMENT_FLAGS_OROR 0x400000
#define STATMENT_FLAGS_ANDAND 0x800000
#define STATMENT_FLAGS_KIND_NODETREE 0x1000000
#define STATMENT_FLAGS_NODETREE_OUTPUT 0x2000000
#define STATMENT_FLAGS_NODETREE_PASTE 0x4000000
#define STATMENT_FLAGS_CONTEXT_PIPE_NUMBER 0xFFFF

#define STATMENT_COMMANDS_MAX 12
#define STATMENT_NODE_MAX 12
#define XYZSH_OPTION_MAX 16

struct _sNodeTree;

typedef struct _sNodeTree sNodeTree;

typedef struct {
    uint mFlags;

    union {
        struct {
            sCommand mCommands[STATMENT_COMMANDS_MAX];
            unsigned char mCommandsNum;
        };
        struct {
            struct _sNodeTree* mNodeTree[STATMENT_NODE_MAX];
            unsigned char mNodeTreeNum;
        };
    };

    char* mFName;
    ushort mLine;
} sStatment;

#define COMPLETION_FLAGS_BLOCK_OR_ENV_NUM 0xff
#define COMPLETION_FLAGS_INPUTING_COMMAND_NAME 0x0100
#define COMPLETION_FLAGS_COMMAND_END 0x0200
#define COMPLETION_FLAGS_ENV 0x0800
#define COMPLETION_FLAGS_ENV_BLOCK 0x1000
#define COMPLETION_FLAGS_BLOCK 0x2000
#define COMPLETION_FLAGS_STATMENT_END 0x8000
#define COMPLETION_FLAGS_STATMENT_HEAD 0x10000
#define COMPLETION_FLAGS_AFTER_REDIRECT 0x20000
#define COMPLETION_FLAGS_AFTER_EQUAL 0x40000
#define COMPLETION_FLAGS_HERE_DOCUMENT 0x80000
#define COMPLETION_FLAGS_AFTER_SPACE 0x100000

struct block_obj
{
    sStatment* mStatments;
    int mStatmentsNum;
    int mStatmentsSize;
    char* mSource;
    uint mCompletionFlags;
};

struct _sObject;
struct _sRunInfo;

#define FUN_FLAGS_NO_STACKFRAME 0x01

typedef struct
{
    struct _sObject* mBlock;
    struct _sObject* mParent;

    struct _sObject* mLocalObjects;
    struct _sObject* mArgBlocks;
    option_hash_it* mOptions;

    unsigned char mFlags;
} fun_obj;

typedef fun_obj class_obj;
typedef fun_obj alias_obj;

typedef BOOL (*fXyzshNativeFun)(struct _sObject*, struct _sObject*, struct _sRunInfo*);

typedef struct 
{
    fXyzshNativeFun mNativeFun;
    struct _sObject* mParent;

    option_hash_it* mOptions;
} nfun_obj;

typedef struct {
    char* mName;                // the job name
    pid_t mPGroup;              // process group id
    struct termios* mTty;       // a terminal status
    BOOL mSuspended;
} job_obj;

typedef struct _uobject_it {
   char* mKey;
   
   void* mItem;
   struct _uobject_it* mCollisionIt;

   struct _uobject_it* mNextIt;
} uobject_it;

typedef struct {
   uobject_it** mTable;
   int mTableSize;

   uobject_it* mEntryIt;

   int mCounter;

   char* mName;

   struct _sObject* mParent;
} uobject_obj;

struct _sRunInfo;

typedef int (*fExtObjMarkFun)(struct _sObject* self);
typedef void (*fExtObjFreeFun)(void* self);
typedef BOOL (*fExtObjMainFun)(void* self, struct _sObject* nextin, struct _sObject* nextout, struct _sRunInfo* runinfo);

typedef struct {
    void* mObject;

    fExtObjMarkFun mMarkFun;
    fExtObjFreeFun mFreeFun;
    fExtObjMainFun mMainFun;
} external_obj;

typedef struct {
    struct _sObject* mBlock;
} completion_obj;

#define GC_MARK 0x8000

#define SOBJ_USER_OBJECT 0x100
#define IS_USER_OBJECT(o) ((o)->mFlags & SOBJ_USER_OBJECT)
#define SET_USER_OBJECT(o) ((o)->mFlags |= SOBJ_USER_OBJECT)

#define IS_MARKED(o) ((o)->mFlags & GC_MARK)
#define SET_MARK(o) ((o)->mFlags |= GC_MARK)

#define STYPE(o) ((o)->mFlags & 0xFF)
#define CLEAR_MARK(o) ((o)->mFlags &= ~GC_MARK)

#define T_STRING 1
#define T_VECTOR 2
#define T_HASH 3
#define T_LIST 4
#define T_NFUN 5
#define T_BLOCK 6
#define T_FD 7
#define T_FD2 8
#define T_JOB 9
#define T_UOBJECT 10
#define T_FUN 11
#define T_CLASS 12
#define T_EXTPROG 13
#define T_COMPLETION 14
#define T_EXTOBJ 15
#define T_ALIAS 16
#define T_INT 17
#define T_TYPE_MAX 18

typedef struct _sObject {
    int mFlags;         // contains a kind of the above and a mark flag, user object flag
    struct _sObject* mNextFreeObject;

    union {
        string_obj uString;
        vector_obj uVector;
        hash_obj uHash;
        list_obj uList;
        fd_obj uFd;
        fd2_obj uFd2;
        job_obj uJob;
        struct block_obj uBlock;

        uobject_obj uUObject;
        nfun_obj uNativeFun;
        fun_obj uFunction;
        class_obj uClass;
        completion_obj uCompletion;
        external_prog_obj uExternalProg;
        external_obj uExternalObj;
        alias_obj uAlias;
        int uInt;
    };
} sObject;

#define SSTRING(o) (o)->uString
#define SVECTOR(o) (o)->uVector
#define SHASH(o) (o)->uHash
#define SLIST(o) (o)->uList
#define SBLOCK(o) (o)->uBlock
#define SNFUN(o) (o)->uNativeFun
#define SFD(o) (o)->uFd
#define SFD2(o) (o)->uFd2
#define SJOB(o) (o)->uJob
#define SUOBJECT(o) (o)->uUObject
#define SFUN(o) (o)->uFunction
#define SCLASS(o) (o)->uClass
#define SEXTPROG(o) (o)->uExternalProg
#define SEXTOBJ(o) (o)->uExternalObj
#define SCOMPLETION(o) (o)->uCompletion
#define SALIAS(o) (o)->uAlias
#define SINT(o) (o)->uInt

void gc_init(int pool_size);
void gc_final();
sObject* gc_get_free_object(int kind, BOOL user_object);

void stack_init(int stack_page_size);
void stack_final();
void stack_start_stack();
void stack_end_stack();
sObject* stack_get_free_object(int kind);
int stack_slot_size();
int stack_page_size();
BOOL stack_valid_object(sObject* object);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// block
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

BOOL sCommand_add_arg(sCommand* self, MANAGED char* buf, int env, int quoted_string, int quoted_head, int glob, int option, char* sname, int sline);
BOOL sCommand_add_block(sCommand* self, sObject* block, char* sname, int sline);
BOOL sCommand_add_message(sCommand* self, MANAGED char* message, char* sname, int sline);
BOOL sCommand_add_env(sCommand* self, MANAGED char* name, MANAGED char* init_value, MANAGED char* key, BOOL key_env, BOOL key_quoted, char* sname, int sline);
BOOL sCommand_add_env_block(sCommand* self, sObject* block, eLineField lf, char* sname, int sline);
BOOL sCommand_add_redirect(sCommand* self, MANAGED char* name, BOOL env, BOOL quoted, BOOL glob, int redirect, char* sname, int sline);
BOOL sCommand_add_command_without_command_name(sCommand* self, sCommand* command, char* sname, int sline);

BOOL sCommand_expand_env(sCommand* command, sObject* object, sObject* nextin, struct _sRunInfo* runinfo);

void sRunInfo_command_new(struct _sRunInfo* runinfo);
void sRunInfo_command_delete(struct _sRunInfo* runinfo);
BOOL sRunInfo_option(struct _sRunInfo* self, char* key);
char* sRunInfo_option_with_argument(struct _sRunInfo* self, char* key);
BOOL sRunInfo_put_option(struct _sRunInfo* self, MANAGED char* key);
BOOL sRunInfo_put_option_with_argument(struct _sRunInfo* self, MANAGED char* key, MANAGED char* argument);
void sRunInfo_add_arg(struct _sRunInfo* self, MANAGED char* buf);
void sRunInfo_init_redirect_file_name(struct _sRunInfo* self, int redirect_number);
void sRunInfo_add_redirect_file_name(struct _sRunInfo* self, int redirect_index, MANAGED char* fname);

sObject* fun_clone_on_gc_from_stack_block(sObject* block, BOOL user_object, sObject* parent, BOOL no_stackframe);
sObject* fun_clone_on_stack_from_stack_block(sObject* block, sObject* parent, BOOL no_stackframe);

sObject* class_clone_on_gc_from_stack_block(sObject* block, BOOL user_object, sObject* parent, BOOL no_stackframe);
sObject* class_clone_on_stack_from_stack_block(sObject* block, sObject* parent, BOOL no_stackframe);

/////////////////////////////////////////////////////////////////////
// functions
/////////////////////////////////////////////////////////////////////
void mcurses_init();
void mcurses_final();

void mclear();
void mclear_immediately();
void mclear_online(int y);

void mmove_immediately(int y, int x);

int mgetmaxx();
int mgetmaxy();

int mis_raw_mode();
void mreset_tty();
void msave_screen();
void mrestore_screen();
void mrestore_ttysettings();
void msave_ttysettings();

///////////////////////////////////////////////////////////////////////////////
// vector
///////////////////////////////////////////////////////////////////////////////

#ifndef MDEBUG
sObject* vector_new_on_malloc(int first_size);
#else
sObject* vector_new_on_malloc_debug(int first_size, const char* fname, int line, const char* func_name);
#endif
sObject* vector_new_on_gc(int first_size, BOOL user_object);
sObject* vector_new_on_stack(int first_size);

#ifndef MDEBUG
#define VECTOR_NEW_MALLOC(size) vector_new_on_malloc(size)
#else
#define VECTOR_NEW_MALLOC(size) vector_new_on_malloc_debug(size, __FILE__, __LINE__, __FUNCTION__)
#endif
#define VECTOR_NEW_STACK(size) vector_new_on_stack(size)
#define VECTOR_NEW_GC(size, user_object) vector_new_on_gc(size, user_object)

sObject* vector_clone(sObject* v);                        // clone

#define vector_item(o, i) SVECTOR(o).mTable[i]
#define vector_count(o) SVECTOR(o).mCount

void vector_delete_on_malloc(sObject* self);
void vector_delete_on_stack(sObject* self);
void vector_delete_on_gc(sObject* self);

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

///////////////////////////////////////////////////////////////////////////////
// string
///////////////////////////////////////////////////////////////////////////////
#ifdef MDEBUG
    sObject* string_new_on_malloc_debug(char* str, const char* fname, int line, const char* func_name);
#else
    sObject* string_new_on_malloc(char* str);
#endif
sObject* string_new_on_gc(char* str, BOOL user_object);
sObject* string_new_on_gc3(char* str, int size, BOOL user_object);
sObject* string_new_on_stack(char* str);

#ifdef MDEBUG
    #define STRING_NEW_MALLOC(str) string_new_on_malloc_debug(str, __FILE__, __LINE__, __FUNCTION__)
#else
    #define STRING_NEW_MALLOC(str) string_new_on_malloc(str)
#endif

#define STRING_NEW_GC(str, user_object) string_new_on_gc(str, user_object)
#define STRING_NEW_GC3(str, size, user_object) string_new_on_gc3(str, size, user_object)
#define STRING_NEW_STACK(str) string_new_on_stack(str)

void string_delete_on_malloc(sObject* self);
void string_delete_on_gc(sObject* self);
void string_delete_on_stack(sObject* self);

#define string_length(o) SSTRING(o).mLen

#define string_c_str(o) SSTRING(o).mStr

void string_push_back(sObject* self, char* key);
void string_push_back2(sObject* self, char key);
void string_push_back3(sObject* self, char* str, int n);
void string_put(sObject* self, char* str);

void string_put2(sObject* self, char c);

void string_trunc(sObject* self, int n);

void string_insert(sObject* obj, int pos, char* str);
void string_erase(sObject* obj, int pos, int len);

void string_toupper(sObject* self, enum eKanjiCode code);
void string_tolower(sObject* self, enum eKanjiCode code);

BOOL string_pomch(sObject* str, eLineField lf);
BOOL string_chomp(sObject* str);
void string_quote(sObject* self, enum eKanjiCode code);

unsigned int string_size(sObject* self);

///////////////////////////////////////////////////////////////////////////////
// list
///////////////////////////////////////////////////////////////////////////////
#define BOOL int
#define TRUE 1
#define FALSE 0

///////////////////////////////////////////////////////////////////////
// list
///////////////////////////////////////////////////////////////////////
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

int list_count(sObject* self);
BOOL list_empty(sObject* self);
int list_index_of(sObject* self, void* item);
void list_push_front(sObject* self, void* item);
void list_push_back(sObject* self, void* item);
void* list_pop_front(sObject* self);
void* list_pop_back(sObject* self);
void list_clear(sObject* self);

list_it* list_begin(sObject* self);
list_it* list_last(sObject* self);
list_it* list_at(sObject* self, int index);
list_it* list_find(sObject* self, void* item);
void* list_item(list_it* self);
list_it* list_next(list_it* self);
list_it* list_prev(list_it* self);
void list_replace(list_it* self, void* item);
list_it* list_erase(list_it* self, sObject* owner);
void list_insert_front(list_it* self, void* item, sObject* owner);
void list_insert_back(list_it* self, void* item, sObject* owner);

/*
    for(list_it* i=list_begin(gList); i; i=list_next(i)) {
        sItem* item = (sItem*)list_item(i);
    }
*/

int list_gc_children_mark(sObject* self);
unsigned int list_size(sObject* self);

///////////////////////////////////////////////////////////////////////////////
// hash
///////////////////////////////////////////////////////////////////////////////
#define HASH_KEYSIZE_PER_ONE_IT 5

#ifndef MDEBUG
sObject* hash_new_on_malloc(int size);
#define HASH_NEW_MALLOC(size) hash_new_on_malloc(size)
#else
sObject* hash_new_on_malloc_debug(int size, const char* fname, int line, const char* func_name);
#define HASH_NEW_MALLOC(size) hash_new_on_malloc_debug(size, __FILE__, __LINE__, __FUNCTION__)
#endif

sObject* hash_new_on_gc(int size, BOOL user_object);
sObject* hash_new_on_stack(int size);

#define HASH_NEW_GC(size, user_object) hash_new_on_gc(size, user_object)
#define HASH_NEW_STACK(size) hash_new_on_stack(size)

void hash_delete_on_gc(sObject* self);
void hash_delete_on_stack(sObject* self);
void hash_delete_on_malloc(sObject* self);

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

sObject* extobj_new_on_gc(void* object, fExtObjMarkFun mark_fun, fExtObjFreeFun free_fun, fExtObjMainFun main_fun, BOOL user_object);
#define EXTOBJ_NEW_GC(o, o2, o3, o4, o5) extobj_new_on_gc(o, o2, o3, o4, o5)

#define UOBJECT_NEW_GC(o, o2, o3, o4) uobject_new_on_gc(o, o2, o3, o4)
#define UOBJECT_NEW_STACK(o, o2, o3) uobject_new_on_stack(o, o2, o3)

void uobject_delete_on_gc(sObject* self);
void uobject_delete_on_stack(sObject* self);
int uobject_gc_children_mark(sObject* self);
BOOL uobject_erase(sObject* self, char* key);

sObject* uobject_new_on_gc(int size, sObject* parent, char* name, BOOL user_object);
sObject* uobject_new_on_stack(int size, sObject* object, char* name);
void uobject_put(sObject* self, char* key, void* item);
void uobject_init(sObject* self);
void uobject_root_init(sObject* self);
void editline_object_init(sObject* self);
void curses_object_init(sObject* self);
void xyzsh_object_init(sObject* self);
void uobject_put(sObject* self, char* key, void* item);
void* uobject_item(sObject* self, char* key);
void uobject_clear(sObject* self);
uobject_it* uobject_loop_begin(sObject* self);
void* uobject_loop_item(uobject_it* it);
char* uobject_loop_key(uobject_it* it);
uobject_it* uobject_loop_next(uobject_it* it);
int uobject_count(sObject* self);

sObject* external_prog_new_on_gc(char* path, BOOL user_object);
void external_prog_delete_on_gc();

#define EXTPROG_NEW_GC(o, o2) external_prog_new_on_gc(o, o2)

sObject* completion_new_on_gc(sObject* block, BOOL user_object);
int completion_gc_children_mark(sObject* self);

#define COMPLETION_NEW_GC(o, o2) completion_new_on_gc(o, o2)

sObject* nfun_new_on_gc(fXyzshNativeFun fun, sObject* parent, BOOL user_object);
#define NFUN_NEW_GC(o, o2, o3) nfun_new_on_gc(o, o2, o3)
int nfun_gc_children_mark(sObject* self);
void nfun_delete_on_gc(sObject* self);
BOOL nfun_put_option_with_argument(sObject* self, MANAGED char* key);
BOOL nfun_option_with_argument_item(sObject* self, char* key);

sObject* fun_new_on_gc(sObject* parent, BOOL user_object, BOOL no_stackframe);
#define FUN_NEW_GC(o, o2, o3) fun_new_on_gc(o, o2, o3)
void fun_delete_on_gc(sObject* self);
int fun_gc_children_mark(sObject* self);
#define FUN_NEW_STACK(o) fun_new_on_stack(o)
sObject* fun_new_on_stack(sObject* parent);
void fun_delete_on_stack(sObject* self);
BOOL fun_put_option_with_argument(sObject* self, MANAGED char* key);
BOOL fun_option_with_argument_item(sObject* self, char* key);

sObject* class_new_on_gc(sObject* parent, BOOL user_object, BOOL no_stackframe);
#define CLASS_NEW_GC(o, o2, o3) class_fun_new_on_gc(o, o2, o3)
void class_delete_on_gc(sObject* self);
void class_delete_on_stack(sObject* self);
BOOL class_option_with_argument_item(sObject* self, char* key);
BOOL class_put_option_with_argument(sObject* self, MANAGED char* key);
int class_gc_children_mark(sObject* self);

sObject* fd_new_on_stack();
void fd_delete_on_stack(sObject* self);
#define FD_NEW_STACK() fd_new_on_stack()
BOOL fd_write(sObject* self, char* str, int size);
BOOL fd_writec(sObject* self, char c);

void fd_split(sObject* self, eLineField lf, BOOL pomch_to_last_line, BOOL chomp_before_split, BOOL split_with_all_item_chompped);
BOOL fd_guess_lf(sObject* self, eLineField* lf);
void fd_put(sObject* self, MANAGED char* buf, int buf_size, int buf_len);
void fd_trunc(sObject* self, int index);
BOOL fd_chomp(sObject* self);

sObject* fd2_new_on_stack(uint fd);
#define FD2_NEW_STACK(o) fd2_new_on_stack(o)

sObject* job_new_on_gc(char* name, pid_t pgroup, struct termios tty);
#define JOB_NEW_GC(o, o2, o3) job_new_on_gc(o, o2, o3)
void job_delete_on_gc(sObject* self);
void job_push_back_child_pid(sObject* self, pid_t pid);

sObject* alias_new_on_gc(sObject* block, BOOL user_object);
#define ALIAS_NEW_GC(o, o2) alias_new_on_gc(o, o2)
int alias_gc_children_mark(sObject* self);

sObject* int_new_on_stack(int value);
#define INT_NEW_STACK(o) int_new_on_stack(o);

/// structure used from run function, which contains run time info
typedef struct _sRunInfo {
    char* mSName;
    int mSLine;
    int mRCode;
    sCommand* mCommand;
    sStatment* mStatment;
    BOOL mLastProgram;
    int mNestLevel;
    BOOL mFilter;
    sObject* mCurrentObject;
    sObject* mRunningObject;
    sObject* mRecieverObject;

    option_hash_it mOptions[XYZSH_OPTION_MAX];  // open adressing hash
    
    char** mArgs;
    int mArgsNum;

    char** mArgsRuntime;
    int mArgsNumRuntime;
    int mArgsSizeRuntime;

    char** mRedirectsFileNamesRuntime;
    int mRedirectsNumRuntime;

    struct _sObject** mBlocks;     // block_obj**
    int mBlocksNum;
} sRunInfo;

BOOL xyzsh_rehash(char* sname, int sline);

typedef BOOL (*fInternalCommand)(sObject*, sObject*, sRunInfo*);

BOOL cmd_typeof(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_alias(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_expand_tilda(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_umask(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_succ(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_tr(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_squeeze(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_delete(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_readline(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_count(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_objinfo(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_kanjicode(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_defined(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_funinfo(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_fselector(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_readline_delete_text(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_mshow(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_mrun(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_errmsg(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_prompt(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_rprompt(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_completion(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_try(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_rehash(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_sort(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_unset(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_p(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_selector(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_plusplus(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_minusminus(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_plus(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_minus(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_mult(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_div(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_mod(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_pow(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_abs(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_add(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_del(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_stackframe(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_jobs(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_gcinfo(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_stackinfo(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_fg(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_exit(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_sweep(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_subshell(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_print(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_if(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_for(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_break(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_while(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_times(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_combine(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_return(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_rehash(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_true(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_false(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_def(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_var(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_write(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_quote(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_load(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_each(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_inherit(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_eval(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_object(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_pwo(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_ref(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_class(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_ary(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_hash(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_length(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_export(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_x(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_msleep(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_raise(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_cd(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_popd(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_pushd(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_ablock(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_index(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_rindex(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_uc(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_lc(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_chomp(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_chop(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_pomch(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_printf(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_join(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_lines(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_rows(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_scan(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_split(sObject* nextin, sObject* nextout, sRunInfo* runinfo);

BOOL cmd_condition_n(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_z(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_b(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_c(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_d(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_f(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_h(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_l(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_p(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_t(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_s2(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_g(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_k(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_u(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_r(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_w(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_x(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_o(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_g2(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_e(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_s(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_eq(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_neq(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_slt(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_sgt(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_sle(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_sge(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_eq2(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_ne(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_lt(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_le(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_gt(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_ge(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_nt(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_ot(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_ef(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_condition_re(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_sub(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_time(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_strip(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_lstrip(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_rstrip(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_substr(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL cmd_substr_replace(sObject* nextin, sObject* nextout, sRunInfo* runinfo);

#if defined(HAVE_MIGEMO_H)
BOOL cmd_migemo_match(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
#endif

BOOL run(sObject* block, sObject* pipein, sObject* pipeout, int* rcode, sObject* current_object, sObject* running_object);

extern volatile BOOL gXyzshSigInt;
extern volatile BOOL gXyzshSigUser;

void xyzsh_restore_signal_default();
void xyzsh_set_signal();
void xyzsh_set_signal_optc();
void sigchld_block(int block);

extern void (*xyzsh_set_signal_other)();
typedef void (*fXyzshJobDone)(int job_num, char* job_title);
extern fXyzshJobDone xyzsh_job_done;

BOOL forground_job(int num);
BOOL background_job(int num);
void xyzsh_wait_background_job();

void xyzsh_editline_init();
void xyzsh_editline_final();

extern sObject* gJobs;

int gc_free_objects_num();
int gc_slot_size();
int gc_pool_size();
int gc_used_objects_num();
int gc_sweep();
BOOL gc_valid_object(sObject* object);

BOOL parse(char* p, char* sname, int* sline, sObject* block, sObject** current_object);

#define RCODE_BREAK 0x100
#define RCODE_RETURN 0x200
#define RCODE_EXIT 0x400
#define RCODE_SIGNAL_INTERRUPT 0x800
#define RCODE_NFUN_FALSE 0x1000
#define RCODE_NFUN_INVALID_USSING 0x2000
#define RCODE_NFUN_NULL_INPUT 0x4000
//#define RCODE_COMMAND_NOT_FOUND 262

void xyzsh_kill_all_jobs();

extern sObject* gRootObject;
extern sObject* gCompletionObject;
extern sObject* gXyzshObject;
extern sObject* gCurrentObject;
extern sObject* gStackFrames;
extern sObject* gMemChecker;
extern sObject* gGlobalPipe;
extern sObject* gInheritMethod;
extern sObject* gInheritMethod2;
extern sRunInfo* gRunInfoOfRunningObject;

BOOL bufsiz_write(int fd, char* buf, int buf_len);
BOOL bufsiz_read(int fd, ALLOC char** result, int* result_len, int* result_size);

BOOL load_file(char* fname, sObject* nextin, sObject* nextout, sRunInfo* runinfo, char** argv, int argc);
BOOL load_so_file(char* fname, sObject* nextin, sObject* nextout, sRunInfo* runinfo);

int object_gc_children_mark(sObject* self);

BOOL sCommand_option_item(sCommand* self, char* key);
char* sCommand_option_with_argument_item(sCommand* self, char* key);
BOOL sCommand_put_option(sCommand* self, MANAGED char* key);
BOOL sCommand_put_option_with_argument(sCommand* self, MANAGED char* key, MANAGED char* argument);
void sCommand_sweep_runtime_info(sCommand* command);
void sStatment_sweep_runtime_info(sStatment* self);

BOOL run_function(sObject* fun, sObject* nextin, sObject* nextout, sRunInfo* runinfo, char** argv, int argc, sObject** blocks, int blocks_num);
BOOL run_completion(sObject* compl, sObject* nextin, sObject* nextout, sRunInfo* runinfo);

#define PARSER_MAGIC_NUMBER_ENV 3

int get_onig_regex(regex_t** reg, sRunInfo* runinfo, char* regex);

void clear_matching_info_variable();
void xyzsh_read_rc_core(char* path);

#define XYZSH_OBJECTS_IN_PIPE_MAX 128

void run_init(enum eAppType app_type);
void run_final();

extern sObject* gPrompt;
extern sObject* gRPrompt;
extern sObject* gDirStack;

BOOL contained_in_pipe(sObject* object);
BOOL get_object_from_argument(sObject** object, char* str, sObject* current_object, sObject* running_object, sRunInfo* runinfo) ;
BOOL get_object_prefix_and_name_from_argument(sObject** object, sObject* name, char* str, sObject* current_object, sObject* running_object, sRunInfo* runinfo) ;
BOOL get_object_from_command_name(sCommand* command, sObject* current_object, sObject** object, sObject** reciever, sRunInfo* runinfo);

enum eOperand { 
    kOpAdd, kOpSub, kOpMult, kOpDiv, kOpEqual, kOpEqualPlus, kOpEqualMinus, kOpEqualMult, kOpEqualDiv, kOpEqualMod, kOpEqualShiftLeft, kOpEqualShiftRight, kOpEqualAnd, kOpEqualOr, kOpEqualXor, kOpAndAnd, kOpOrOr, kOpEqEq, kOpNotEqEq, kOpGt, kOpGte, kOpLt, kOpLte, kOpQuestion, kOpOr, kOpXor, kOpAnd, kOpLeftShift, kOpRightShift, kOpMod, kOpPlusPlus, kOpMinusMinus, kOpTilda, kOpExclamation, kOpPlusPlus2, kOpMinusMinus2, kOpStrAdd, kOpStrEqEq, kOpStrNotEqEq, kOpStrCmp, kOpStrLt, kOpStrGt, kOpStrLe, kOpStrGe
};

struct _sNodeTree {
    unsigned char mType;   // 0: operand 1: value 2: local variable 3: global variable 4:string value 5:function name

    union {
        enum eOperand mOperand;
        int mValue;
        sObject* mVarName;
        sObject* mStringValue;
        struct {
            char* mFunName;
            char* mFunArg;
        };
    };

    struct _sNodeTree* mLeft;
    struct _sNodeTree* mRight;
    struct _sNodeTree* mMiddle;
};

sNodeTree* sNodeTree_create_operand(enum eOperand operand, sNodeTree* left, sNodeTree* right, sNodeTree* middle);
sNodeTree* sNodeTree_create_value(int value, sNodeTree* left, sNodeTree* right, sNodeTree* middle);
sNodeTree* sNodeTree_create_var(char* var_name, sNodeTree* left, sNodeTree* right, sNodeTree* middle);
sNodeTree* sNodeTree_clone(sNodeTree* source);
void sNodeTree_free(sNodeTree* self);
BOOL node_tree(sStatment* statment, sObject* nextin, sObject* nextout, sRunInfo* runinfo, sObject* current_object);

BOOL node_expression(ALLOC sNodeTree** node, char** p, char* sname, int* sline);

extern int gTtyFD;

//////////////////////////////////////////////////////////////////////
// xyzsh API
//////////////////////////////////////////////////////////////////////
BOOL xyzsh_load_file(char* fname, char** argv, int argc, sObject* current_object);
void xyzsh_opt_c(char* cmd, char** argv, int argc);
void xyzsh_editline_interface(char* cmdline, int cursor_point, char** argv, int argc, BOOL exit_in_spite_ofjob_exist, BOOL welcome_msg);

void xyzsh_add_inner_command(sObject* object, char* name, fXyzshNativeFun command, int arg_num, ...);

BOOL xyzsh_editline_interface_onetime(int* rcode, char* cmdline, int cursor_point, char* source_name, char** argv, int argc, fXyzshJobDone xyzsh_job_done_);
char* xyzsh_job_title(int n);
int xyzsh_job_num();
BOOL xyzsh_run(int* rcode, sObject* block, char* source_name, fXyzshJobDone xyzsh_job_done_, sObject* nextin, sObject* nextout, int argc, char** argv, sObject* current_object);
BOOL xyzsh_eval(int* rcode, char* cmd, char* source_name, fXyzshJobDone xyzsh_job_done_, sObject* nextin, sObject* nextout, int argc, char** argv, sObject* current_object);

void err_msg(char* msg, char* sname, int line);
void err_msg_adding(char* msg, char* sname, int line);

sObject* access_object(char* name, sObject** current, sObject* running_object);
sObject* access_object2(char* name, sObject* current, sObject* running_object);
sObject* access_object3(char* name, sObject** current);

ALLOC char* editline(char* prompt, char* rprompt, char* text, int cursor_pos);
ALLOC char* editline_user_castamized(char* prompt, char* rprompt, char* text, int cursor_pos, sObject* block);
ALLOC char* editline_no_completion(char* prompt, char* rprompt, char* text, int cursor_pos);

BOOL nfun_option_with_argument(sObject* self, char* key);
BOOL class_option_with_argument(sObject* self, char* key);
BOOL fun_option_with_argument(sObject* self, char* key);
void fd_clear(sObject* self);
BOOL fd_flash(sObject* self, int fd);
BOOL run_object(sObject* object, sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL add_object_to_objects_in_pipe(sObject* object, sRunInfo* runinfo, sCommand* command);
BOOL xyzsh_readline_interface_onetime(int* rcode, char* cmdline, int cursor_pos, char* source_name, char** argv, int argc, fXyzshJobDone xyzsh_job_done_);
void xyzsh_readline_interface(char* cmdline, int cursor_point, char** argv, int argc, BOOL exit_in_spite_ofjob_exist, BOOL welcome_msg);
void xyzsh_readline_interface_on_curses(char* cmdline, int cursor_point, char** argv, int argc, BOOL exit_in_spite_ofjob_exist, BOOL welcome_msg);

void load_init();
void load_final();

#endif

