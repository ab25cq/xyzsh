#include "config.h"
#include "xyzsh.h"
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <pwd.h>
#include <limits.h>

// skip spaces
static void skip_spaces(char** p)
{
    while(**p == ' ' || **p == '\t') {
        (*p)++;
    }
}

static void skip_to_nextline(char** p, int* sline)
{
    while(**p) {
        if(**p == '\n') {
            (*p)++;
            (*sline)++;
            break;
        }
        else {
            (*p)++;
        }
    }
}

#define XYZSH_ENV_MAX 32

typedef struct {
    char* mBuf;
    int mLen;
    int mSize;

    int mRedirect;

    BOOL mEnv;
    BOOL mGlob;
    BOOL mTilda;
    BOOL mQuotedString;
    BOOL mMessagePassing;
    BOOL mOption;
    BOOL mQuotedHead;
} sBuf;

static void add_char_to_buf(sBuf* buf, char c)
{
    if(buf->mSize <= buf->mLen + 1) {
        buf->mSize *= 1.8;
        buf->mBuf = REALLOC(buf->mBuf, buf->mSize);
    }
    (buf->mBuf)[buf->mLen] = c;
    (buf->mBuf)[buf->mLen+1] = 0;
    buf->mLen++;
}

static void add_str_to_buf(sBuf* buf, char* str)
{
    const int len = strlen(str);
    if(buf->mSize <= buf->mLen + len) {
        buf->mSize = (buf->mSize + len) * 1.8;
        buf->mBuf = REALLOC(buf->mBuf, buf->mSize);
    }
    memcpy(buf->mBuf + buf->mLen, str, len+1);
    buf->mLen += len;
}

static BOOL read_backslash(char**p, sBuf* buf, char* sname, int* sline)
{
    (*p)++;

    switch(**p) {
        case 0:
            err_msg("unexpected end. can't quote null.", sname, *sline);
            return FALSE;

        case '\n':
            //add_char_to_buf(buf, '\n');
            (*sline)++;
            break;

        case 't':
            add_char_to_buf(buf, '\t');
            break;

        case 'n':
            add_char_to_buf(buf, '\n');
            break;

        case 'r':
            add_char_to_buf(buf, '\r');
            break;

        case 'a':
            add_char_to_buf(buf, '\a');
            
            break;

        case '\\':
            add_char_to_buf(buf, '\\');
            break;

        default:
            if(buf->mLen == 0) {
                buf->mQuotedHead = TRUE;
            }
            add_char_to_buf(buf, **p);
    }

    (*p)++;

    return TRUE;
}

static BOOL block_parse(char** p, char* sname, int* sline, sObject* block, sObject** current_object);
static BOOL read_one_argument(char** p, sBuf* buf, char* sname, int* sline, sCommand* command, sObject* block, sObject** current_object, BOOL read_until_bracket_close, BOOL read_until_backquote);

#define XYZSH_VARIABLE_OBJECT_MAX 8

static BOOL read_dollar(char** p, sBuf* buf, char* sname, int* sline, sCommand* command, sObject* block, sObject** current_object)
{
    (*p)++;

    eLineField lf = kLF;

    if(**p == '-') {
        (*p)++;
        buf->mOption = TRUE;
    }

    if(*(*p+1) == '(') {
        if(**p == 'a') {
            (*p)++;
            lf = kBel;
        }
        else if(**p == 'm') {
            (*p)++;
            lf = kCR;
        }
        else if(**p == 'w') {
            (*p)++;
            lf = kCRLF;
        }
        else if(**p == 'u') {
            (*p)++;
            lf = kLF;
        }
    }

    if(**p == '(') {
        (*p)++;

        sObject* block2 = BLOCK_NEW_STACK();
        if(!block_parse(p, sname, sline, block2, current_object)) {
            (void)sCommand_add_env_block(command, block2, lf, sname, *sline);
            SBLOCK(block).mCompletionFlags |= COMPLETION_FLAGS_ENV_BLOCK;
            SBLOCK(block).mCompletionFlags |= command->mEnvsNum & COMPLETION_FLAGS_BLOCK_OR_ENV_NUM;
            return FALSE;
        }

        buf->mEnv = TRUE;

        add_char_to_buf(buf, PARSER_MAGIC_NUMBER_ENV);
        char buf2[128];
        snprintf(buf2, 128, "%d", command->mEnvsNum);
        add_str_to_buf(buf, buf2);
        add_char_to_buf(buf, PARSER_MAGIC_NUMBER_ENV);

        if(!sCommand_add_env_block(command, block2, lf, sname, *sline)) {
            return FALSE;
        }
    }
    else {
        SBLOCK(block).mCompletionFlags |= COMPLETION_FLAGS_ENV;

        sBuf name;
        memset(&name, 0, sizeof(sBuf));
        name.mBuf = MALLOC(64);
        name.mBuf[0] = 0;
        name.mSize = 64;

        sBuf key;
        memset(&key, 0, sizeof(sBuf));
        key.mBuf = MALLOC(64);
        key.mBuf[0] = 0;
        key.mSize = 64;

        sBuf init_value;
        memset(&init_value, 0, sizeof(sBuf));
        init_value.mBuf = MALLOC(64);
        init_value.mBuf[0] = 0;
        init_value.mSize = 64;

        if(**p == '{') {
            (*p)++;

            while(1) {
                if(**p == 0) {
                    err_msg("close } which is used by expand variable", sname, *sline);
                    FREE(name.mBuf);
                    FREE(key.mBuf);
                    FREE(init_value.mBuf);
                    return FALSE;
                }
                else if(**p == '}') {
                    (*p)++;
                    break;
                }
                else if(**p == '=') {
                    (*p)++;

                    while(**p != 0) {
                        if(**p == '}') {
                            (*p)++;
                            break;
                        }
                        else {
                            add_char_to_buf(&init_value, **p);
                            (*p)++;
                        }
                    }
                    break;
                }
                else {
                    add_char_to_buf(&name, **p);
                    (*p)++;
                }
            }
        }
        else {
            while(**p >= 'a' && **p <= 'z' || **p >= 'A' && **p <= 'Z' || **p == '_' || **p >= '0' && **p <= '9' || **p == ':')
            {
                add_char_to_buf(&name, **p);
                (*p)++;
            }
        }

        if(**p == '[') {
            (*p)++;

            if(!read_one_argument(p, &key, sname, sline, command, block, current_object, TRUE, FALSE)) 
            {
                FREE(name.mBuf);
                if(key.mEnv) {  // for readline
                    (void)sCommand_add_arg(command, MANAGED key.mBuf, key.mEnv, key.mQuotedString, key.mQuotedHead, key.mGlob, key.mOption, sname, *sline);
                }
                else {
                    FREE(key.mBuf);
                }
                FREE(init_value.mBuf);
                return FALSE;
            }

            skip_spaces(p);
            if(**p != ']') {
                err_msg("require ] character", sname, *sline);
                FREE(name.mBuf);
                FREE(key.mBuf);
                FREE(init_value.mBuf);
                return FALSE;
            }
            (*p)++;
        }

        buf->mEnv = TRUE;

        add_char_to_buf(buf, PARSER_MAGIC_NUMBER_ENV);
        char buf2[128];
        snprintf(buf2, 128, "%d", command->mEnvsNum);
        add_str_to_buf(buf, buf2);
        add_char_to_buf(buf, PARSER_MAGIC_NUMBER_ENV);

        if(!sCommand_add_env(command, MANAGED name.mBuf, MANAGED init_value.mBuf, MANAGED key.mBuf, key.mEnv, key.mQuotedString, sname, *sline)) {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL block_parse_until_backquote(char** p, char* sname, int* sline, sObject* block, sObject** current_object);

static BOOL read_backquote(char** p, sBuf* buf, char* sname, int* sline, sCommand* command, sObject* block, sObject** current_object)
{
    (*p)++;

    eLineField lf = kLF;

    sObject* block2 = BLOCK_NEW_STACK();
    if(!block_parse_until_backquote(p, sname, sline, block2, current_object)) {
        (void)sCommand_add_env_block(command, block2, lf, sname, *sline);
        SBLOCK(block).mCompletionFlags |= COMPLETION_FLAGS_ENV_BLOCK;
        SBLOCK(block).mCompletionFlags |= command->mEnvsNum & COMPLETION_FLAGS_BLOCK_OR_ENV_NUM;
        return FALSE;
    }

    buf->mEnv = TRUE;

    add_char_to_buf(buf, PARSER_MAGIC_NUMBER_ENV);
    char buf2[128];
    snprintf(buf2, 128, "%d", command->mEnvsNum);
    add_str_to_buf(buf, buf2);
    add_char_to_buf(buf, PARSER_MAGIC_NUMBER_ENV);

    if(!sCommand_add_env_block(command, block2, lf, sname, *sline)) {
        return FALSE;
    }

    return TRUE;
}

static BOOL colon_statment(char** p, sStatment* statment, char* sname, int* sline)
{
    statment->mNodeTreeNum = 0;
    while(**p) {
        if(**p == ')' || **p == ']' || **p == '`' || **p == '}') {
            break;
        }
        else if(**p == '\n') {
            (*sline)++;
            (*p)++;
            break;
        }
        else if(**p == ';') {
            (*p)++;
            break;
        }
        else {
            sNodeTree* node;
            if(!node_expression(ALLOC &node, p, sname, sline)) {
                sNodeTree_free(node);
                return FALSE;
            }
            statment->mNodeTree[statment->mNodeTreeNum++] = MANAGED node;

            if(statment->mNodeTreeNum == STATMENT_NODE_MAX) {
                err_msg("overflow number of node tree", sname, *sline);
                return FALSE;
            }

            skip_spaces(p);

            if(**p == ',') {
                (*p)++;
                skip_spaces(p);
            }
            else {
                break;
            }
        }
    }

    return TRUE;
}

static BOOL add_argument_to_command(MANAGED sBuf* buf, sCommand* command, char* sname, int* sline);

static BOOL read_one_argument(char** p, sBuf* buf, char* sname, int* sline, sCommand* command, sObject* block, sObject** current_object, BOOL read_until_bracket_close, BOOL read_until_backquote)
{
    BOOL squote = FALSE;
    BOOL dquote = FALSE;
    while(**p) {
        if(!dquote && **p == '\'') {
            (*p)++;

            buf->mQuotedString = TRUE;
            squote = !squote;
        }
        else if(!squote && **p == '"') {
            (*p)++;

            buf->mQuotedString = TRUE;
            dquote = !dquote;
        }
        else if(squote || dquote) {
            if(**p == 0) {
                err_msg("close single quote or double quote", sname, *sline);
                return FALSE;
            }
            else if(dquote) {
                if(**p == '\\') {
                    if(!read_backslash(p, buf, sname, sline)) {
                        return FALSE;
                    }
                }
                else if(**p == '$') {
                    if(!read_dollar(p, buf, sname,sline, command, block, current_object)) {
                        return FALSE;
                    }
                }
                else if(**p == '`') {
                    if(!read_backquote(p, buf, sname,sline, command, block, current_object)) {
                        return FALSE;
                    }
                }
                else {
                    if(**p == '\n') (*sline)++;

                    add_char_to_buf(buf, **p);
                    (*p)++;
                }
            }
            else {
                if(**p == '\n') (*sline)++;

                add_char_to_buf(buf, **p);
                (*p)++;
            }
        }
        /// %q<...> or %Q<...> quote
        else if(**p == '%' && (*(*p+1) == 'q' || *(*p+1) == 'Q') && !isalnum(*(*p+2)) && *(*p+2) >= ' ' && *(*p+2) <= 126)
        {
            BOOL dquote2 = *(*p+1) == 'Q';
            BOOL delimiter = *(*p+2);
            (*p) += 3;

            if(delimiter == '(') {
                delimiter = ')';
            } else if(delimiter == '<') {
                delimiter = '>';
            } else if(delimiter == '[') {
                delimiter = ']';
            } else if(delimiter == '{') {
                delimiter = '}';
            }

            buf->mQuotedString = TRUE;

            if(dquote2) {
                while(1) {
                    if(**p == delimiter) {
                        (*p)++;
                        break;
                    }
                    else if(**p == 0) {
                        err_msg("reqire to close %q quote",  sname, *sline);
                        return FALSE;
                    }
                    else if(**p == '\\') {
                        if(!read_backslash(p, buf, sname, sline)) {
                            return FALSE;
                        }
                    }
                    else if(**p == '$') {
                        if(!read_dollar(p, buf, sname,sline, command, block, current_object)) {
                            return FALSE;
                        }
                    }
                    else {
                        if(**p == '\n') (*sline) ++;

                        add_char_to_buf(buf, **p);
                        (*p)++;
                    }
                }
            }
            else {
                while(1) {
                    if(**p == delimiter) {
                        (*p)++;
                        break;
                    }
                    else if(**p == 0) {
                        err_msg("reqire to close %q quote",  sname, *sline);
                        return FALSE;
                    }
                    else {
                        if(**p == '\n') (*sline) ++;

                        add_char_to_buf(buf, **p);
                        (*p)++;
                    }
                }
            }
        }
        /// quote ///
        else if(**p == '\\') {
            if(!read_backslash(p, buf, sname, sline)) {
                return FALSE;
            }
        }
        // this must be previous position to glob 
        else if(**p == '\n' || **p ==' ' || **p == '#' || **p == '&' || **p == '(' || **p == ')' || **p == ';' || **p =='|' || read_until_bracket_close && **p == ']') 
        {
            break;
        }
        /// read backquote
        else if(**p == '`') {
            if(read_until_backquote) {
                break;
            }
            else {
                if(!read_backquote(p, buf, sname,sline, command, block, current_object)) {
                    return FALSE;
                }
            }
        }
        /// read dollar
        else if(**p == '$') {
            if(!read_dollar(p, buf, sname,sline, command, block, current_object)) {
                return FALSE;
            }
        }
        /// here document
        else if(**p == '<' && *(*p+1) == '<' && *(*p+2) == '<') {
            (*p)+=3;

            skip_spaces(p);

            BOOL squote;
            if(**p == '\'') {
                squote = TRUE;
                (*p)++;
            }
            else {
                squote = FALSE;
            }

            skip_spaces(p);

            sObject* name = STRING_NEW_STACK("");
            while(**p) {
                if(**p >='A' && ** p <= 'Z' || **p >='a' && **p <='z' || **p == '_' || **p >= '0' && **p <= '9')
                {
                    string_push_back2(name, **p);
                    (*p)++;
                }
                else {
                    break;
                }
            }

            SBLOCK(block).mCompletionFlags |= COMPLETION_FLAGS_HERE_DOCUMENT;

            skip_spaces(p);

            if(squote) {
                if(**p == '\'') {
                    (*p)++;
                }
                else {
                    err_msg("invalid here document name", sname, *sline);
                    return FALSE;
                }
            }

            skip_spaces(p);

            if(**p == '\n') {
                (*sline)++;
                (*p)++;
            }
            else {
                err_msg("invalid here document name", sname, *sline);
                return FALSE;
            }

            if(string_c_str(name)[0] == 0) {
                err_msg("invalid here document name", sname, *sline);
                return FALSE;
            }

            sObject* line = STRING_NEW_STACK("");
            while(1) {
                if(**p == 0) {
                    err_msg("require to close Here Document", sname, *sline);
                    return FALSE;
                }
                /// env ///
                else if(!squote && **p == '$') {
                    if(!read_dollar(p, buf, sname,sline, command, block, current_object)) {
                        return FALSE;
                    }
                }
                else if(gXyzshSigInt) {
                    err_msg("signal interrupt", sname, *sline);
                    return FALSE;
                }
                else if(**p == '\n') {
                    add_char_to_buf(buf, **p);
                    (*p)++;
                    (*sline)++;

                    sObject* line = STRING_NEW_STACK("");
                    while(**p) {
                        if(**p >='A' && ** p <= 'Z' || **p >='a' && **p <='z' || **p == '_' || **p >= '0' && **p <= '9')
                        {
                            string_push_back2(line, **p);
                            (*p)++;
                        }
                        else {
                            break;
                        }
                    }

                    if(strcmp(string_c_str(line), string_c_str(name)) == 0) {
                        break;
                    }
                    else {
                        add_str_to_buf(buf, string_c_str(line));
                    }
                }
                else {
                    add_char_to_buf(buf, **p);
                    (*p)++;
                }
            }

            SBLOCK(block).mCompletionFlags &= ~COMPLETION_FLAGS_HERE_DOCUMENT;
        }
        /// tilda ///
        else if(buf->mLen == 0 && **p == '~') {
            add_char_to_buf(buf, **p);
            (*p)++;

            buf->mTilda = TRUE;
        }
        /// equal for completion ///
        else if(**p == '=') {
            add_char_to_buf(buf, **p);
            (*p)++;

            SBLOCK(block).mCompletionFlags |= COMPLETION_FLAGS_AFTER_EQUAL;
        }
        /// redirect out ///
        else if(**p == '>') {
            if(buf->mBuf[0] != 0) {
                /// message passing ///
                if(buf->mMessagePassing) {
                    if(!sCommand_add_message(command, MANAGED buf->mBuf, sname, *sline)) {
                        return FALSE;
                    }
                }
                else if(!add_argument_to_command(MANAGED buf, command, sname, sline)) {
                    return FALSE;
                }

                memset(buf, 0, sizeof(sBuf));
                buf->mBuf = MALLOC(64);
                buf->mBuf[0] = 0;
                buf->mSize = 64;
            }

            (*p)++;

            if(**p == '>') {
                buf->mRedirect = REDIRECT_APPEND;
                (*p)++;
            }
            else {
                buf->mRedirect = REDIRECT_OUT;
            }

            SBLOCK(block).mCompletionFlags |= COMPLETION_FLAGS_AFTER_REDIRECT;

            skip_spaces(p);
        }
        /// redirect in ///
        else if(**p == '<') {
            if(buf->mBuf[0] != 0) {
                /// message passing ///
                if(buf->mMessagePassing) {
                    if(!sCommand_add_message(command, MANAGED buf->mBuf, sname, *sline)) {
                        return FALSE;
                    }
                }
                else if(!add_argument_to_command(MANAGED buf, command, sname, sline)) {
                    return FALSE;
                }

                memset(buf, 0, sizeof(sBuf));
                buf->mBuf = MALLOC(64);
                buf->mBuf[0] = 0;
                buf->mSize = 64;
            }

            (*p)++;

            buf->mRedirect = REDIRECT_IN;

            SBLOCK(block).mCompletionFlags |= COMPLETION_FLAGS_AFTER_REDIRECT;

            skip_spaces(p);
        }
        /// glob ///
        else if(command->mArgsNum > 0 && (**p == '[' || **p == ']' || **p == '?' || **p == '*')) {
            add_char_to_buf(buf, **p);
            (*p)++;

            buf->mGlob = TRUE;

            if(buf->mOption) {
                err_msg("can't take option with glob", sname, *sline);
                return FALSE;
            }
        }
        /// option ///
        else if(command->mArgsNum > 0 && buf->mLen == 0 && **p == '-' && (isalpha(*(*p+1)) || *(*p+1) == '-' || *(*p+1) == '_'))
        {
            add_char_to_buf(buf, **p);
            (*p)++;

            buf->mOption = TRUE;

            if(buf->mGlob) {
                err_msg("can't take option with glob", sname, *sline);
                return FALSE;
            }
        }
        /// message passing ///
        else if(command->mArgsNum == 0 && **p == ':' && *(*p+1) == ':') {
            (*p)+=2;
            buf->mMessagePassing = TRUE;
            break;
        }
        /// utf-8 character
        else if(((unsigned char)**p) > 127) {
            add_char_to_buf(buf, **p);
            (*p)++;
        }
        else if(**p == '/') {
            SBLOCK(block).mCompletionFlags &= ~COMPLETION_FLAGS_ENV;
            add_char_to_buf(buf, **p);
            (*p)++;
        }
        else {
            add_char_to_buf(buf, **p);
            (*p)++;
        }
    }

    if(squote || dquote) {
        err_msg("require to close ' or \"", sname, *sline);
        return FALSE;
    }

    if(**p != ' ' && **p != '\t') {
        SBLOCK(block).mCompletionFlags &= ~COMPLETION_FLAGS_AFTER_SPACE;
    }

    return TRUE;
}

void tilda_expasion(char* file, ALLOC char** file2)
{
    char* p = file;
    p++; // ~

    char result[PATH_MAX];
    
    char* user = MALLOC(16);
    int user_size = 16;
    int user_len = 0;
    user[0] = 0;

    /// get user name ///
    while(*p && *p != '/') {
        if(user_len >= user_size) {
            user_size *= 1.8;
            user = REALLOC(user, user_size);
        }
        user[user_len++] = *p++;
    }
    user[user_len] = 0;

    /// expand HOME evironment
    if(user[0] == 0) {
        FREE(user);

        char* home = getenv("HOME");
        if(home) {
            xstrncpy(result, home, PATH_MAX);
        }
        else {
            struct passwd* pw = getpwuid(getuid());
            if(pw) {
                xstrncpy(result, pw->pw_dir, PATH_MAX);
            }
            else {
                xstrncpy(result, "~", PATH_MAX);
            }
        }
    }
    /// expand user name to home directory
    else {
        struct passwd* pw = getpwnam(user);
        FREE(user);

        if(pw) {
            xstrncpy(result, pw->pw_dir, PATH_MAX);
        }
        else {
            xstrncpy(result, "~", PATH_MAX);
            xstrncat(result, user, PATH_MAX);
        }
    }

    xstrncat(result, p, PATH_MAX);

    *file2 = STRDUP(result);
}

static BOOL read_statment(char**p, sStatment* statment, sObject* block, char* sname, int* sline, sObject** current_object, BOOL read_until_backquote);

// result --> TRUE: success
// result --> FALSE: There is a error message on gErrMsg. Please output the error message and stop running
static BOOL block_parse(char** p, char* sname, int* sline, sObject* block, sObject** current_object)
{
    char* source_head = *p;

    while(**p != ')') {
        /// skip spaces at head of statment ///
        while(**p == ' ' || **p == '\t' || **p == '\n' && (*sline)++) { (*p)++; }

        /// skip a comment ///
        if(**p == '#') skip_to_nextline(p, sline);

        /// go parsing ///
        sStatment* statment = sStatment_new(block, *sline, sname);

        if(!read_statment(p, statment, block, sname, sline, current_object, FALSE))
        {
            return FALSE;
        }

        /// skip comment ///
        if(**p == '#') skip_to_nextline(p, sline);

        /// skip spaces at tail of statment ///
        while(**p == ' ' || **p == '\t' || **p == '\n' && (*sline)++) { (*p)++; }

        /// there is no command, delete last statment
        if(statment->mCommandsNum == 0 && (statment->mFlags & STATMENT_FLAGS_KIND_NODETREE) == 0) {
            sStatment_delete(statment);
            memset(statment, 0, sizeof(sStatment));
            SBLOCK(block).mStatmentsNum--;
        }

        if(**p == 0) {
            err_msg("close block. require ) character before the end of statments", sname, *sline);
            return FALSE;
        }

        SBLOCK(block).mCompletionFlags &= ~COMPLETION_FLAGS_STATMENT_END;
    }
    (*p)++;

    const int len = *p - source_head;
    char* source = MALLOC(len);
    memcpy(source, source_head, len-1);
    source[len-1] = 0;

    SBLOCK(block).mSource = source;

    return TRUE;
}

// result --> TRUE: success
// result --> FALSE: There is a error message on gErrMsg. Please output the error message and stop running
static BOOL block_parse_until_backquote(char** p, char* sname, int* sline, sObject* block, sObject** current_object)
{
    char* source_head = *p;

    while(**p != '`') {
        /// skip spaces at head of statment ///
        while(**p == ' ' || **p == '\t' || **p == '\n' && (*sline)++) { (*p)++; }

        /// skip a comment ///
        if(**p == '#') skip_to_nextline(p, sline);

        /// go parsing ///
        sStatment* statment = sStatment_new(block, *sline, sname);

        if(!read_statment(p, statment, block, sname, sline, current_object, TRUE))
        {
            return FALSE;
        }

        /// skip comment ///
        if(**p == '#') skip_to_nextline(p, sline);

        /// skip spaces at tail of statment ///
        while(**p == ' ' || **p == '\t' || **p == '\n' && (*sline)++) { (*p)++; }

        /// there is no command, delete last statment
        if(statment->mCommandsNum == 0 && (statment->mFlags & STATMENT_FLAGS_KIND_NODETREE) == 0) {
            sStatment_delete(statment);
            memset(statment, 0, sizeof(sStatment));
            SBLOCK(block).mStatmentsNum--;
        }

        if(**p == 0) {
            err_msg("close block. require ) character before the end of statments", sname, *sline);
            return FALSE;
        }

        SBLOCK(block).mCompletionFlags &= ~COMPLETION_FLAGS_STATMENT_END;
    }
    (*p)++;

    const int len = *p - source_head;
    char* source = MALLOC(len);
    memcpy(source, source_head, len-1);
    source[len-1] = 0;

    SBLOCK(block).mSource = source;

    return TRUE;
}

static BOOL add_argument_to_command(MANAGED sBuf* buf, sCommand* command, char* sname, int* sline)
{
    /// tilda expression ///
    if(buf->mTilda) {
        char* buf2;
        tilda_expasion(buf->mBuf, ALLOC &buf2);
        FREE(buf->mBuf);
        buf->mBuf = buf2;
        buf->mTilda = FALSE;
    }

    /// redirect ///
    if(buf->mRedirect) {
        if(buf->mOption) {
            err_msg("can't take redirect with option", sname, *sline);
            return FALSE;
        }
        if(!sCommand_add_redirect(command, MANAGED buf->mBuf, buf->mEnv, buf->mQuotedString, buf->mGlob, buf->mRedirect, sname, *sline)) {
            return FALSE;
        }
    }
    /// add buf to command ///
    else if(buf->mQuotedString || buf->mBuf[0] != 0) {
        if(!sCommand_add_arg(command, MANAGED buf->mBuf, buf->mEnv, buf->mQuotedString, buf->mQuotedHead, buf->mGlob, buf->mOption, sname, *sline)) {
            return FALSE;
        }
    }
    else {
        FREE(buf->mBuf);
    }

    return TRUE;
}

static void clear_completion_flags_at_end_of_statment(sObject* block)
{
    SBLOCK(block).mCompletionFlags &= ~(COMPLETION_FLAGS_INPUTING_COMMAND_NAME|COMPLETION_FLAGS_ENV|COMPLETION_FLAGS_AFTER_REDIRECT|COMPLETION_FLAGS_AFTER_EQUAL);
    SBLOCK(block).mCompletionFlags |= COMPLETION_FLAGS_STATMENT_END; // add this to flags
}

static void clear_complation_flags_at_space(sObject* block)
{
    SBLOCK(block).mCompletionFlags &= ~(COMPLETION_FLAGS_INPUTING_COMMAND_NAME|COMPLETION_FLAGS_ENV|COMPLETION_FLAGS_AFTER_REDIRECT|COMPLETION_FLAGS_AFTER_EQUAL);
    SBLOCK(block).mCompletionFlags |= COMPLETION_FLAGS_AFTER_SPACE;
}

static BOOL read_command(char** p, sCommand* command, sStatment* statment, sObject* block, char* sname, int* sline, sObject** current_object, BOOL read_until_backquote)
{
    while(**p) {
        sBuf buf;
        memset(&buf, 0, sizeof(sBuf));
        buf.mBuf = MALLOC(64);
        buf.mBuf[0] = 0;
        buf.mSize = 64;

        if(!read_one_argument(p, &buf, sname, sline, command, block, current_object, FALSE, read_until_backquote)) 
        {
            if(buf.mEnv) {  // for readline
                (void)sCommand_add_arg(command, MANAGED buf.mBuf, TRUE, buf.mQuotedString, buf.mQuotedHead, buf.mGlob, buf.mOption, sname, *sline);
            }
            else {
                FREE(buf.mBuf);
            }
            return FALSE;
        }

        /// message passing ///
        if(buf.mMessagePassing) {
            if(!sCommand_add_message(command, MANAGED buf.mBuf, sname, *sline)) {
                return FALSE;
            }

            continue;
        }
        else if(!add_argument_to_command(MANAGED &buf, command, sname, sline)) {
            return FALSE;
        }

        /// subshell, block ///
        if(**p == '(') {
            (*p)++;

            if(command->mArgsNum == 0) {
                sObject* block2 = BLOCK_NEW_STACK();
                if(!block_parse(p, sname, sline, block2, current_object)) {
                    (void)sCommand_add_block(command, block2, sname, *sline);
                    SBLOCK(block).mCompletionFlags |= COMPLETION_FLAGS_BLOCK;
                    SBLOCK(block).mCompletionFlags |= command->mBlocksNum & COMPLETION_FLAGS_BLOCK_OR_ENV_NUM;
                    return FALSE;
                }

                if(!sCommand_add_arg(command, MANAGED STRDUP("subshell"), 0, 0, 0, 0, 0 , sname, *sline)) {
                    return FALSE;
                }
                if(!sCommand_add_block(command, block2, sname, *sline)) {
                    return FALSE;
                }
            }
            else {
                sObject* current_object_before;

                /// if user call run method, change current_object
                if(current_object) {
                    current_object_before = *current_object;

                    if(strcmp(command->mArgs[0], "run") == 0 && command->mMessagesNum > 0) {
                        sObject* current_object2 = *current_object;
                        sObject* object = access_object(command->mMessages[0], &current_object2, NULL);

                        if(object && STYPE(object) == T_UOBJECT) {
                            int i;
                            for(i=1; i<command->mMessagesNum; i++) {
                                object = uobject_item(object, command->mMessages[i]);

                                if(object == NULL || STYPE(object) != T_UOBJECT) {
                                    break;
                                }
                            }

                            if(object && STYPE(object) == T_UOBJECT) {
                                *current_object = object;
                            }
                        }
                    }
                }

                sObject* block2 = BLOCK_NEW_STACK();
                if(!block_parse(p, sname, sline, block2, current_object)) {
                    (void)sCommand_add_block(command, block2, sname, *sline);
                    SBLOCK(block).mCompletionFlags |= COMPLETION_FLAGS_BLOCK;
                    SBLOCK(block).mCompletionFlags |= command->mBlocksNum & COMPLETION_FLAGS_BLOCK_OR_ENV_NUM;
                    return FALSE;
                }

                /// if user call run method, change current_object
                if(current_object) {
                    *current_object = current_object_before;
                }

                if(!sCommand_add_block(command, block2, sname, *sline)) {
                    return FALSE;
                }
            }
            SBLOCK(block).mCompletionFlags &= ~COMPLETION_FLAGS_INPUTING_COMMAND_NAME;
        }
        /// spaces ///
        else if(**p == ' ' || **p == '\t') {
            skip_spaces(p);
            clear_complation_flags_at_space(block);
        }
        else {
            break;
        }
    }

    return TRUE;
}

/// read statment
/// TRUE: OK
/// FALSE: indicate to occure error
static BOOL read_statment(char**p, sStatment* statment, sObject* block, char* sname, int* sline, sObject** current_object, BOOL read_until_backquote)
{
    /// expression ///
    if(**p == ':' && *(*p+1) != ':') {
        (*p)++;
        skip_spaces(p);

        if(**p == '@') {
            statment->mFlags |= STATMENT_FLAGS_NODETREE_OUTPUT;
            (*p)++;
            skip_spaces(p);
        }

        statment->mFlags |= STATMENT_FLAGS_KIND_NODETREE;

        SBLOCK(block).mCompletionFlags = 0;

        if(!colon_statment(p, statment, sname, sline)) {
            return FALSE;
        }
    }
    /// normal pipe statment ///
    else {
        SBLOCK(block).mCompletionFlags |= COMPLETION_FLAGS_STATMENT_HEAD;

        /// a reversing code ///
        if(**p == '!') {
            (*p)++;
            skip_spaces(p);

            statment->mFlags |= STATMENT_FLAGS_REVERSE;
        }

        /// a global pipe in ///
        if(**p == '|' && *(*p+1) == '>') {
            (*p)+=2;

            skip_spaces(p);

            statment->mFlags |= STATMENT_FLAGS_GLOBAL_PIPE_IN;
        }
        /// a context pipe ///
        else if(**p == '|' && *(*p+1) != '|') {
            (*p)++;

            char buf[16];
            char* p2 = buf;
            while(**p == '-' || **p >= '0' && **p <= '9') {
                *p2++ = **p;
                (*p)++;
            }
            *p2 = 0;

            skip_spaces(p);

            statment->mFlags |= STATMENT_FLAGS_CONTEXT_PIPE;
            statment->mFlags |= atoi(buf) & STATMENT_FLAGS_CONTEXT_PIPE_NUMBER;
        }

        while(**p) {
            if(statment->mCommandsNum >= STATMENT_COMMANDS_MAX) {
                err_msg("Overflow command number. Please use global pipe.", sname, *sline);
                return FALSE;
            }

            SBLOCK(block).mCompletionFlags &= ~(COMPLETION_FLAGS_ENV);
            SBLOCK(block).mCompletionFlags |= COMPLETION_FLAGS_INPUTING_COMMAND_NAME;

            sCommand* command = sCommand_new(statment);
            if(!read_command(p, command, statment, block, sname, sline, current_object, read_until_backquote)) {
                SBLOCK(block).mCompletionFlags &= ~COMPLETION_FLAGS_STATMENT_HEAD;
                return FALSE;
            }

            SBLOCK(block).mCompletionFlags &= ~COMPLETION_FLAGS_STATMENT_HEAD;

            /// There is no argument, so delete last command in block ///
            if(command->mArgsNum == 0)
            {
                if(command->mMessagesNum > 0) {
                    err_msg("require command name", sname, *sline);
                    return FALSE;
                }
                else {
                    sCommand_delete(command);
                    memset(command, 0, sizeof(sCommand));
                    statment->mCommandsNum--;
                }
            }

            if(**p == '|' && *(*p+1) != '>' && *(*p+1) != '|') // chains a next command with pipe
            {
                (*p)++;

                skip_spaces(p);

                if(**p == 0) {
                    SBLOCK(block).mCompletionFlags |= COMPLETION_FLAGS_COMMAND_END;
                    break;
                }
            }
            else {
                break;
            }
        }

        /// global pipe out
        if(**p == '|' && *(*p+1) == '>') {
            (*p)+=2;

            if(**p == '>') {
                (*p)++;
                statment->mFlags |= STATMENT_FLAGS_GLOBAL_PIPE_APPEND;
            }
            else {
                statment->mFlags |= STATMENT_FLAGS_GLOBAL_PIPE_OUT;
            }

            skip_spaces(p);
        }

        /// the termination of a statment
        if(**p == '&' && *(*p+1) == '&') {
            (*p)+=2;
            skip_spaces(p);

            statment->mFlags |= STATMENT_FLAGS_ANDAND;
            clear_completion_flags_at_end_of_statment(block);
        }
        else if(**p == '|' && *(*p+1) == '|') {
            (*p)+=2;
            skip_spaces(p);

            statment->mFlags |= STATMENT_FLAGS_OROR;
            clear_completion_flags_at_end_of_statment(block);
        }
        else if(**p == '&') {
            (*p)++;
            skip_spaces(p);

            if(gAppType == kATOptC) {
                err_msg("In script mode a job can't run on background. On interactive shell a job can run on background", sname, *sline);
                return FALSE;
            }

            statment->mFlags |= STATMENT_FLAGS_BACKGROUND;
            clear_completion_flags_at_end_of_statment(block);
        }
        else if(**p == '\n') {
            (*p)++; 
            skip_spaces(p);

            statment->mFlags |= STATMENT_FLAGS_NORMAL;
            clear_completion_flags_at_end_of_statment(block);
            (*sline)++;
        }
        else if(**p == ';') {
            (*p)++;
            skip_spaces(p);

            statment->mFlags |= STATMENT_FLAGS_NORMAL;
            clear_completion_flags_at_end_of_statment(block);
        }
        else if(**p == 0) {
            statment->mFlags |= STATMENT_FLAGS_NORMAL;
        }
        else if(**p == ')' || **p == '#' || read_until_backquote && **p == '`') {
        }
        else {
            char buf[128];
            snprintf(buf, 128, "unexpected token3 -->(%c)\n", **p);
            err_msg(buf, sname, *sline);
            return FALSE;
        }
    }

    return TRUE;
}

// result --> TRUE: success
// result --> FALSE: There is an error message on gErrMsg. Please output the error message and stop running
BOOL parse(char* p, char* sname, int* sline, sObject* block, sObject** current_object)
{
    SBLOCK(block).mSource = STRDUP(p);

    while(1) {
        /// skip spaces at head of statment ///
        while(*p == ' ' || *p == '\t' || *p == '\n' && (*sline)++) { p++; }

        /// skip comment ///
        if(*p == '#') skip_to_nextline(&p, sline);

        /// go parsing ///
        sStatment* statment = sStatment_new(block, *sline, sname);

        if(!read_statment(&p, statment, block, sname, sline, current_object, FALSE))
        {
            return FALSE;
        }

        /// There is not a command, so delete last statment in block
        if(statment->mCommandsNum == 0 && (statment->mFlags & STATMENT_FLAGS_KIND_NODETREE) == 0) {
            sStatment_delete(statment);
            memset(statment, 0, sizeof(sStatment));
            SBLOCK(block).mStatmentsNum--;
        }

        /// skip spaces at tail of statment ///
        while(*p == ' ' || *p == '\t' || *p == '\n' && (*sline)++) { p++; }

        if(*p == ')') {
            char buf[128];
            snprintf(buf, 128, "unexpected token3 -->(%c)\n", *p);
            err_msg(buf, sname, *sline);
            return FALSE;
        }
        /// skip comment ///
        else if(*p == '#') {
            skip_to_nextline(&p, sline);
        }
        else if(*p == 0) {
            break;
        }

        SBLOCK(block).mCompletionFlags &= ~COMPLETION_FLAGS_STATMENT_END;
    }

    return TRUE;
}
