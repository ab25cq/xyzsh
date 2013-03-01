#ifndef XYZSH_H
#define XYZSH_H

#include <termios.h>
#include <stdio.h>
#include <limits.h>

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

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned short ushort;

#include <unistd.h>

enum eCommandKind { 
    kJobs, 
    kFg,
    kExit,
    kGCInfo,
    kStackInfo,
    kSweep,
    kSubshell,
    kIf,
    kWhile,
    kBreak,
    kReturn,
    kPrint,
    kRehash,
    kTrue,
    kFalse,
    kDef,
    kVar,
    kWrite,
    kQuote,
    kLoad,
    kEach,
    kInherit,
    kEval,
    kObject,
    kPwo,
    kCo,
    kRef,
    kClass,
    kAry,
    kHash,
    kLength,
    kExport,
    kX,

    kInnerCommand,
    kCommand,
    kCommandMax
};

#define kLF 0x01
#define kCRLF 0x02
#define kCR 0x03
#define kBel 0x04

typedef char eLineField;

extern eLineField gLineField;
extern enum eKanjiCode gKanjiCode;

enum eAppType { kATOptC, kATXApp, kATConsoleApp };
    // Application kind

extern enum eAppType gAppType;

struct _sObject;

extern struct _sObject* gErrMsg;               // error message

extern struct _sObject* gStdin;
extern struct _sObject* gStdout;
extern struct _sObject* gStderr;

void xyzsh_init(enum eAppType app_type, BOOL no_runtime_script);
void xyzsh_final();

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
#define ENV_FLAGS_DOUBLE_DOLLAR 0x10
#define ENV_FLAGS_OPTION 0x20

typedef unsigned char eEnvKind;

typedef struct {
    int mFlags;

    union {
        struct {
            char* mName;
            char* mKey;
            uchar mKeyEnv;
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

#define XYZSH_ARGUMENT_ENV 0x01
#define XYZSH_ARGUMENT_GLOB 0x02

#define XYZSH_ARG_SIZE_MAX 65536*3
#define XYZSH_REDIRECT_SIZE_MAX 72
#define XYZSH_BLOCK_SIZE_MAX 72
#define XYZSH_MESSAGE_SIZE_MAX 72
#define XYZSH_ENV_SIZE_MAX 72

typedef struct {
    enum eCommandKind mKind;

    char** mArgs;
    int* mArgsFlags;    // This index is the same as mArgs
    int mArgsNum;
    int mArgsSize;

    sEnv* mEnvs;
    uchar mEnvsNum;
    uchar mEnvsSize;

    struct _sObject** mBlocks;     // block_obj**
    uchar mBlocksNum;
    uchar mBlocksSize;

    char** mRedirectsFileNames;
    int* mRedirects;
    uchar mRedirectsNum;
    uchar mRedirectsSize;

    char** mMessages;
    uchar mMessagesNum;
    uchar mMessagesSize;
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
#define STATMENT_FLAGS_CONTEXT_PIPE_NUMBER 0xFFFF

#define STATMENT_COMMANDS_MAX 12
#define XYZSH_OPTION_MAX 16

typedef struct {
    sCommand mCommands[STATMENT_COMMANDS_MAX];
    uchar mCommandsNum;

    char* mFName;
    ushort mLine;

    uint mFlags;
} sStatment;

#define COMPLETION_FLAGS_BLOCK_OR_ENV_NUM 0xff
#define COMPLETION_FLAGS_INPUTING_COMMAND_NAME 0x0100
#define COMPLETION_FLAGS_COMMAND_END 0x0200
#define COMPLETION_FLAGS_ENV 0x0800
#define COMPLETION_FLAGS_ENV_BLOCK 0x1000
#define COMPLETION_FLAGS_BLOCK 0x2000
#define COMPLETION_FLAGS_TILDA 0x4000
#define COMPLETION_FLAGS_STATMENT_END 0x8000
#define COMPLETION_FLAGS_STATMENT_HEAD 0x10000
#define COMPLETION_FLAGS_AFTER_REDIRECT 0x20000
#define COMPLETION_FLAGS_AFTER_EQUAL 0x40000
#define COMPLETION_FLAGS_HERE_DOCUMENT 0x80000

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

    uchar mFlags;
} fun_obj;

typedef fun_obj class_obj;

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

#define TYPE(o) ((o)->mFlags & 0xFF)
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
#define T_TYPE_MAX 16

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

#include <xyzsh/debug.h>
#include <xyzsh/hash.h>
#include <xyzsh/kanji.h>
#include <xyzsh/string.h>
#include <xyzsh/vector.h>
#include <xyzsh/list.h>
#include <xyzsh/block.h>
#include <xyzsh/curses.h>

sObject* extobj_new_on_gc(void* object, fExtObjMarkFun mark_fun, fExtObjFreeFun free_fun, fExtObjMainFun main_fun, BOOL user_object);
#define EXTOBJ_NEW_GC(o, o2, o3, o4, o5) extobj_new_on_gc(o, o2, o3, o4, o5)

#define UOBJECT_NEW_GC(o, o2, o3, o4) uobject_new_on_gc(o, o2, o3, o4)
#define UOBJECT_NEW_STACK(o, o2, o3) uobject_new_on_stack(o, o2, o3)

void uobject_delete_on_gc(sObject* self);
void uobject_delete_on_stack(sObject* self);
int uobject_gc_children_mark(sObject* self);

sObject* uobject_new_on_gc(int size, sObject* parent, char* name, BOOL user_object);
sObject* uobject_new_on_stack(int size, sObject* object, char* name);
void uobject_put(sObject* self, char* key, void* item);
void uobject_init(sObject* self);
void uobject_root_init(sObject* self);
void readline_object_init(sObject* self);
void curses_object_init(sObject* self);
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

void fd_split(sObject* self, eLineField lf);
BOOL fd_guess_lf(sObject* self, eLineField* lf);
void fd_put(sObject* self, MANAGED char* buf, int buf_size, int buf_len);
void fd_trunc(sObject* self, int index);

sObject* fd2_new_on_stack(uint fd);
#define FD2_NEW_STACK(o) fd2_new_on_stack(o)

sObject* job_new_on_gc(char* name, pid_t pgroup, struct termios tty);
#define JOB_NEW_GC(o, o2, o3) job_new_on_gc(o, o2, o3)
void job_delete_on_gc(sObject* self);
void job_push_back_child_pid(sObject* self, pid_t pid);

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

extern fInternalCommand kInternalCommands[kInnerCommand];
extern char * gCommandKindStrs[kCommandMax];

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
BOOL cmd_co(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
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
BOOL cmd_block(sObject* nextin, sObject* nextout, sRunInfo* runinfo);
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

void xyzsh_readline_init(BOOL runtime_script);
void xyzsh_readline_final();

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
extern sObject* gInheritObject;
extern sRunInfo* gRunInfoOfRunningObject;

BOOL bufsiz_write(int fd, char* buf, int buf_len);

BOOL load_file(char* fname, sObject* nextin, sObject* nextout, sRunInfo* runinfo, char** argv, int argc);
BOOL load_so_file(char* fname, sObject* nextin, sObject* nextout, sRunInfo* runinfo);

int object_gc_children_mark(sObject* self);

BOOL sCommand_option_item(sCommand* self, char* key);
char* sCommand_option_with_argument_item(sCommand* self, char* key);
BOOL sCommand_put_option(sCommand* self, MANAGED char* key);
BOOL sCommand_put_option_with_argument(sCommand* self, MANAGED char* key, MANAGED char* argument);
BOOL sCommand_expand_env(sCommand* command, sObject* object, sObject* nextin, sObject* nextout, sRunInfo* runinfo);
BOOL sCommand_expand_env_redirect(sCommand* command, sObject* nextin, sRunInfo* runinfo);
void sCommand_sweep_runtime_info(sCommand* command);
void sStatment_sweep_runtime_info(sStatment* self);

BOOL run_function(sObject* fun, sObject* nextin, sObject* nextout, sRunInfo* runinfo, char** argv, int argc, sObject** blocks, int blocks_num);
BOOL run_completion(sObject* compl, sObject* nextin, sObject* nextout, sRunInfo* runinfo);

#define PARSER_MAGIC_NUMBER_ENV 3
#define PARSER_MAGIC_NUMBER_OPTION 5

#include <oniguruma.h>
int get_onig_regex(regex_t** reg, sRunInfo* runinfo, char* regex);

int readline_signal();
void clear_matching_info_variable();
#include <stdarg.h>
void xyzsh_read_rc_core(char* path);

#define XYZSH_OBJECTS_IN_PIPE_MAX 128

void run_init(enum eAppType app_type);
void run_final();

extern sObject* gPrompt;
extern sObject* gDirStack;

void readline_write_history();
void readline_read_history();
void readline_no_completion();
void readline_completion();

char* xstrncpy(char* src, char* des, int size);
char* xstrncat(char* src, char* des, int size);

BOOL contained_in_pipe(sObject* object);
BOOL get_object_from_str(sObject** object, char* str, sObject* current_object, sObject* running_object, sRunInfo* runinfo) ;
void split_prefix_of_object_and_name(sObject** object, sObject* prefix, sObject* name, char* str);
void split_prefix_of_object_and_name2(sObject** object, sObject* prefix, sObject* name, char* str, sObject* current_object);

//////////////////////////////////////////////////////////////////////
// xyzsh API
//////////////////////////////////////////////////////////////////////
BOOL xyzsh_load_file(char* fname, char** argv, int argc, sObject* current_object);
void xyzsh_opt_c(char* cmd, char** argv, int argc);
void xyzsh_readline_interface(char* cmdline, int cursor_point, char** argv, int argc, BOOL exit_in_spite_ofjob_exist);

void xyzsh_add_inner_command(sObject* object, char* name, fXyzshNativeFun command, int arg_num, ...);

BOOL xyzsh_readline_interface_onetime(int* rcode, char* cmdline, int cursor_point, char* source_name, char** argv, int argc, fXyzshJobDone xyzsh_job_done_);
char* xyzsh_job_title(int n);
int xyzsh_job_num();
BOOL xyzsh_run(int* rcode, sObject* block, char* source_name, fXyzshJobDone xyzsh_job_done_, sObject* nextin, sObject* nextout, int argc, char** argv, sObject* current_object);
BOOL xyzsh_eval(int* rcode, char* cmd, char* source_name, fXyzshJobDone xyzsh_job_done_, sObject* nextin, sObject* nextout, int argc, char** argv, sObject* current_object);

void err_msg(char* msg, char* sname, int line, char* command);
void err_msg_adding(char* msg, char* sname, int line, char* command);

sObject* access_object(char* name, sObject** current, sObject* running_object);
sObject* access_object2(char* name, sObject* current, sObject* running_object);
sObject* access_object3(char* name, sObject** current);

#endif

