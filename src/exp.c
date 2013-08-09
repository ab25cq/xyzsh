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

sNodeTree* sNodeTree_create_operand(enum eOperand operand, sNodeTree* left, sNodeTree* right, sNodeTree* middle)
{
    sNodeTree* self = MALLOC(sizeof(sNodeTree));

    self->mType = 0;
    self->mOperand = operand;
    self->mLeft = left;
    self->mRight = right;
    self->mMiddle = middle;

    return self;
}

sNodeTree* sNodeTree_create_value(int value, sNodeTree* left, sNodeTree* right, sNodeTree* middle)
{
    sNodeTree* self = MALLOC(sizeof(sNodeTree));

    self->mType = 1;
    self->mValue = value;

    self->mLeft = left;
    self->mRight = right;
    self->mMiddle = middle;

    return self;
}

sNodeTree* sNodeTree_create_string_value(char* value, sNodeTree* left, sNodeTree* right, sNodeTree* middle)
{
    sNodeTree* self = MALLOC(sizeof(sNodeTree));

    self->mType = 4;
    self->mStringValue = STRING_NEW_MALLOC(value);

    self->mLeft = left;
    self->mRight = right;
    self->mMiddle = middle;

    return self;
}

sNodeTree* sNodeTree_create_var(char* var_name, sNodeTree* left, sNodeTree* right, sNodeTree* middle)
{
    sNodeTree* self = MALLOC(sizeof(sNodeTree));

    self->mType = 2;
    self->mVarName = STRING_NEW_MALLOC(var_name);

    self->mLeft = left;
    self->mRight = right;
    self->mMiddle = middle;

    return self;
}

sNodeTree* sNodeTree_create_global_var(char* var_name, sNodeTree* left, sNodeTree* right, sNodeTree* middle)
{
    sNodeTree* self = MALLOC(sizeof(sNodeTree));

    self->mType = 3;
    self->mVarName = STRING_NEW_MALLOC(var_name);

    self->mLeft = left;
    self->mRight = right;
    self->mMiddle = middle;

    return self;
}

sNodeTree* sNodeTree_create_function(char* name, char* argument, sNodeTree* left, sNodeTree* right, sNodeTree* middle)
{
    sNodeTree* self = MALLOC(sizeof(sNodeTree));

    self->mType = 5;
    self->mFunName = STRDUP(name);
    self->mFunArg = STRDUP(argument);

    self->mLeft = left;
    self->mRight = right;
    self->mMiddle = middle;

    return self;
}

sNodeTree* sNodeTree_clone(sNodeTree* source)
{
    if(source == NULL) {
        return NULL;
    }

    sNodeTree* self = MALLOC(sizeof(sNodeTree));

    self->mType = source->mType;
    switch(self->mType) {
        case 0:
            self->mOperand = source->mOperand;
            break;

        case 1:
            self->mValue = source->mValue;
            break;

        case 2:
            self->mVarName = STRING_NEW_MALLOC(string_c_str(source->mVarName));
            break;

        case 3:
            self->mVarName = STRING_NEW_MALLOC(string_c_str(source->mVarName));
            break;

        case 4:
            self->mStringValue = STRING_NEW_MALLOC(string_c_str(source->mStringValue));
            break;
            
        case 5:
            self->mFunName = STRDUP(source->mFunName);
            self->mFunArg = STRDUP(source->mFunArg);
            break;
    }

    if(source->mLeft) {
        self->mLeft = sNodeTree_clone(source->mLeft);
    }
    else {
        self->mLeft = NULL;
    }
    if(source->mRight) {
        self->mRight = sNodeTree_clone(source->mRight);
    }
    else {
        self->mRight = NULL;
    }
    if(source->mMiddle) {
        self->mMiddle = sNodeTree_clone(source->mMiddle);
    }
    else {
        self->mMiddle = NULL;
    }

    return self;
}

void sNodeTree_free(sNodeTree* self)
{
    if(self) {
        if(self->mLeft) sNodeTree_free(self->mLeft);
        if(self->mRight) sNodeTree_free(self->mRight);
        if(self->mMiddle) sNodeTree_free(self->mMiddle);

        if(self->mType == 2 || self->mType == 3) {
            string_delete_on_malloc(self->mVarName);
        }
        else if(self->mType == 4) {
            string_delete_on_malloc(self->mStringValue);
        }
        else if(self->mType == 5) {
            FREE(self->mFunName);
            FREE(self->mFunArg);
        }

        FREE(self);
    }
}

static BOOL expression_node(ALLOC sNodeTree** node, char** p, char* sname, int* sline)
{
    if(**p >= '0' && **p <= '9' || **p == '-' || **p == '+') {
        char buf[128];
        char* p2 = buf;

        if(**p == '-') {
            *p2++ = '-';
            (*p)++;
        }
        else if(**p =='+') {
            (*p)++;
        }

        while(**p >= '0' && **p <= '9') {
            *p2++ = **p;
            (*p)++;

            if(p2 - buf >= 128) {
                err_msg("overflow node of number",  sname, *sline);
                return FALSE;
            }
        }
        *p2 = 0;
        skip_spaces(p);

        *node = ALLOC sNodeTree_create_value(atoi(buf), NULL, NULL, NULL);
    }
    else if(**p == '"') {
        (*p)++;

        sObject* value = STRING_NEW_STACK("");

        while(1) {
            if(**p == '"') {
                (*p)++;
                break;
            }
            else if(**p == '\\') {
                (*p)++;
                switch(**p) {
                    case 'n':
                        string_push_back2(value, '\n');
                        (*p)++;
                        break;

                    case 't':
                        string_push_back2(value, '\t');
                        (*p)++;
                        break;

                    case 'r':
                        string_push_back2(value, '\r');
                        (*p)++;
                        break;

                    case 'a':
                        string_push_back2(value, '\a');
                        (*p)++;
                        break;

                    case '\\':
                        string_push_back2(value, '\\');
                        (*p)++;
                        break;

                    default:
                        string_push_back2(value, **p);
                        (*p)++;
                        break;
                }
            }
            else if(**p == 0) {
                err_msg("close \" to make string value", sname, *sline);
                return FALSE;
            }
            else {
                string_push_back2(value, **p);
                (*p)++;
            }
        }

        skip_spaces(p);

        *node = ALLOC sNodeTree_create_string_value(string_c_str(value), NULL, NULL, NULL);
    }
    else if(**p == '(') {
        (*p)++;
        skip_spaces(p);

        if(!node_expression(ALLOC node, p, sname, sline)) {
            return FALSE;
        }
        skip_spaces(p);

        if(**p != ')') {
            err_msg("require )", sname, *sline);
            return FALSE;
        }
        (*p)++;
        skip_spaces(p);

        if(*node == NULL) {
            err_msg("require expression as ( operand", sname, *sline);
            return FALSE;
        }
    }
    else if(**p == '&' && (isalpha(*(*p+1)) || *(*p+1) == '_')) {
        (*p)++;

        sNodeTree* fnodes[STATMENT_COMMANDS_MAX];
        memset(fnodes, 0, sizeof(sNodeTree*)*STATMENT_COMMANDS_MAX);
        int fnodes_count = 0;

        while(1) {
            /// function name ///
            char name[128];
            char* p2 = name;

            while(isalpha(**p) || **p == '_') {
                *p2++ = **p;
                (*p)++;

                if(p2 - name >= 128) {
                    err_msg("overflow node of function name",  sname, *sline);
                    return FALSE;
                }
            }
            *p2 = 0;
            skip_spaces(p);

            /// argument ///
            char arg[128];

            if(**p == '(') {
                (*p)++;
                p2 = arg;
                while(**p && **p != ')') {
                    *p2++ = **p;
                    (*p)++;

                    if(p2 - arg >= 128) {
                        err_msg("overflow node of argument name",  sname, *sline);
                        return FALSE;
                    }
                }
                *p2 = 0;
                skip_spaces(p);

                if(**p == ')') {
                    (*p)++;
                    skip_spaces(p);
                }
                else {
                    err_msg("require ) at last of function call", sname, *sline);
                    return FALSE;
                }
            }
            else {
                *arg = 0;
            }

            if(fnodes_count >= STATMENT_COMMANDS_MAX) {
                err_msg("overflow funcation chain", sname, *sline);
                return FALSE;
            }

            fnodes[fnodes_count++] = ALLOC sNodeTree_create_function(name, arg, NULL, NULL, NULL);

            if(**p == '.') {
                (*p)++;
                skip_spaces(p);
            }
            else {
                break;
            }
        }

        int i;
        for(i=0; i<fnodes_count; i++) {
            fnodes[i]->mLeft = fnodes[i+1];
        }

        *node = fnodes[0];
    }
    else if(**p == 'm' && *(*p+1) == 'y') {
        (*p)+=2;

        skip_spaces(p);

        if(**p == '$') {
            (*p)++;
        }
        else {
            err_msg("invalid character after \"my\"", sname, *sline);
            return FALSE;
        }

        if(isalpha(**p) || **p == '_') {
            char buf[128];
            char* p2 = buf;

            while(isalpha(**p) || **p == '_') {
                *p2++ = **p;
                (*p)++;

                if(p2 - buf >= 128) {
                    err_msg("overflow node of variable name",  sname, *sline);
                    return FALSE;
                }
            }
            *p2 = 0;
            skip_spaces(p);

            *node = ALLOC sNodeTree_create_var(buf, NULL, NULL, NULL);

            /// tail ///
            if(**p == '+' && *(*p+1) == '+') {
                (*p)+=2;
                skip_spaces(p);

                *node = ALLOC sNodeTree_create_operand(kOpPlusPlus2, *node, NULL, NULL);
            }
            else if(**p == '-' && *(*p+1) == '-') {
                (*p)+=2;
                skip_spaces(p);

                *node = ALLOC sNodeTree_create_operand(kOpMinusMinus2, *node, NULL, NULL);
            }
        }
        else {
            err_msg("invalid character after \"my\"", sname, *sline);
            return FALSE;
        }
    }
    else if(**p == '$' && (isalpha(*(*p+1)) || *(*p+1) == '_')) {
        (*p)++;

        char buf[128];
        char* p2 = buf;

        while(isalpha(**p) || **p == '_') {
            *p2++ = **p;
            (*p)++;

            if(p2 - buf >= 128) {
                err_msg("overflow node of variable name",  sname, *sline);
                return FALSE;
            }
        }
        *p2 = 0;
        skip_spaces(p);

        *node = ALLOC sNodeTree_create_global_var(buf, NULL, NULL, NULL);

        /// tail ///
        if(**p == '+' && *(*p+1) == '+') {
            (*p)+=2;
            skip_spaces(p);

            *node = ALLOC sNodeTree_create_operand(kOpPlusPlus2, *node, NULL, NULL);
        }
        else if(**p == '-' && *(*p+1) == '-') {
            (*p)+=2;
            skip_spaces(p);

            *node = ALLOC sNodeTree_create_operand(kOpMinusMinus2, *node, NULL, NULL);
        }
    }
    else if(**p == ';') {
        *node = NULL;
    }
    else {
        char buf[128];
        snprintf(buf, 128, "invalid character (%c)", **p);
        err_msg(buf, sname, *sline);
        *node = NULL;
        return FALSE;
    }

    return TRUE;
}

static BOOL expression_node_prefix(ALLOC sNodeTree** node, char** p, char* sname, int* sline)
{
    if(**p == '+' && *(*p+1) == '+') {
        (*p)+=2;
        skip_spaces(p);

        if(!expression_node(ALLOC node, p, sname, sline)) {
            return FALSE;
        }

        if(*node == NULL || ((*node)->mType != 2 && (*node)->mType != 3)) {
            err_msg("require variable name as incremetal operand", sname, *sline);
            return FALSE;
        }

        *node = sNodeTree_create_operand(kOpPlusPlus, *node, NULL, NULL);
    }
    else if(**p == '-' && *(*p+1) == '-') {
        (*p)+=2;
        skip_spaces(p);

        if(!expression_node(ALLOC node, p, sname, sline)) {
            return FALSE;
        }

        if(*node == NULL || ((*node)->mType != 2 && (*node)->mType != 3)) {
            err_msg("require variable name as incremetal operand", sname, *sline);
            return FALSE;
        }

        *node = sNodeTree_create_operand(kOpMinusMinus, *node, NULL, NULL);
    }
    else if(**p == '~') {
        (*p)++;
        skip_spaces(p);

        if(!expression_node(ALLOC node, p, sname, sline)) {
            return FALSE;
        }

        if(*node == NULL) {
            err_msg("require int value as ~ operand", sname, *sline);
            return FALSE;
        }

        *node = sNodeTree_create_operand(kOpTilda, *node, NULL, NULL);
    }
    else if(**p == '!') {
        (*p)++;
        skip_spaces(p);

        if(!expression_node(ALLOC node, p, sname, sline)) {
            return FALSE;
        }

        if(*node == NULL) {
            err_msg("require value as ! operand", sname, *sline);
            return FALSE;
        }

        *node = sNodeTree_create_operand(kOpExclamation, *node, NULL, NULL);
    }
    else {
        if(!expression_node(ALLOC node, p, sname, sline)) {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL expression_mult_div(ALLOC sNodeTree** node, char** p, char* sname, int* sline)
{
    if(!expression_node_prefix(ALLOC node, p, sname, sline)) {
        return FALSE;
    }

    while(**p) {
        if(**p == '*' && *(*p+1) != '=') {
            (*p)++;
            skip_spaces(p);
            sNodeTree* right;
            if(!expression_node_prefix(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpMult, *node, right, NULL);
        }
        else if(**p == '/' && *(*p+1) != '=') {
            (*p)++;
            skip_spaces(p);
            sNodeTree* right;
            if(!expression_node_prefix(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpDiv, *node, right, NULL);
        }
        else if(**p == '%' && *(*p+1) != '=') {
            (*p)++;
            skip_spaces(p);
            sNodeTree* right;
            if(!expression_node_prefix(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpMod, *node, right, NULL);
        }
        else {
            break;
        }
    }

    return TRUE;
}

static BOOL expression_add_sub(ALLOC sNodeTree** node, char** p, char* sname, int* sline)
{
    if(!expression_mult_div(ALLOC node, p, sname, sline)) {
        return FALSE;
    }

    while(**p) {
        if(**p == '+' && *(*p+1) != '=' && *(*p+1) != '+') {
            (*p)++;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_mult_div(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpAdd, *node, right, NULL);
        }
        else if(**p == '-' && *(*p+1) != '=') {
            (*p)++;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_mult_div(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpSub, *node, right, NULL);
        }
        else if(**p == '.') {
            (*p)++;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_mult_div(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpStrAdd, *node, right, NULL);
        }
        else {
            break;
        }
    }

    return TRUE;
}

static BOOL expression_shift(ALLOC sNodeTree** node, char** p, char* sname, int* sline)
{
    if(!expression_add_sub(ALLOC node, p, sname, sline)) {
        return FALSE;
    }

    while(**p) {
        if(**p == '<' && *(*p+1) == '<' && *(*p+2) != '=') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_add_sub(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpLeftShift, *node, right, NULL);
        }
        else if(**p == '>' && *(*p+1) == '>' && *(*p+2) != '=') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_add_sub(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpRightShift, *node, right, NULL);
        }
        else {
            break;
        }
    }

    return TRUE;
}

/// from left to right order
static BOOL expression_gtlt(ALLOC sNodeTree** node, char** p, char* sname, int* sline)
{
    if(!expression_shift(ALLOC node, p, sname, sline)) {
        return FALSE;
    }

    while(**p) {
        if(**p == '>' && *(*p+1) == '=') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_shift(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpGte, *node, right, NULL);
        }
        else if(**p == '>' && *(*p+1) != '>') {
            (*p)++;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_shift(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpGt, *node, right, NULL);
        }
        else if(**p == '<' && *(*p+1) == '=') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_shift(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpLte, *node, right, NULL);
        }
        else if(**p == '<' && *(*p+1) != '<') {
            (*p)++;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_shift(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpLt, *node, right, NULL);
        }
        else if(**p == 'l' && *(*p+1) == 't') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_shift(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpStrLt, *node, right, NULL);
        }
        else if(**p == 'g' && *(*p+1) == 't') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_shift(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpStrGt, *node, right, NULL);
        }
        else if(**p == 'l' && *(*p+1) == 'e') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_shift(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpStrLe, *node, right, NULL);
        }
        else if(**p == 'g' && *(*p+1) == 'e') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_shift(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpStrGe, *node, right, NULL);
        }
        else {
            break;
        }
    }

    return TRUE;
}

/// from left to right order
static BOOL expression_eqeq(ALLOC sNodeTree** node, char** p, char* sname, int* sline)
{
    if(!expression_gtlt(ALLOC node, p, sname, sline)) {
        return FALSE;
    }

    while(**p) {
        if(**p == '=' && *(*p+1) == '=') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_gtlt(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpEqEq, *node, right, NULL);
        }
        else if(**p == '!' && *(*p+1) == '=') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_gtlt(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpNotEqEq, *node, right, NULL);
        }
        else if(**p == 'e' && *(*p+1) == 'q') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_gtlt(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpStrEqEq, *node, right, NULL);
        }
        else if(**p == 'n' && *(*p+1) == 'e') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_gtlt(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpStrNotEqEq, *node, right, NULL);
        }
        else if(**p == 'c' && *(*p+1) == 'm' && *(*p+2) == 'p') {
            (*p)+=3;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_gtlt(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpStrCmp, *node, right, NULL);
        }
        else {
            break;
        }
    }

    return TRUE;
}

/// from left to right order
static BOOL expression_and(ALLOC sNodeTree** node, char** p, char* sname, int* sline)
{
    if(!expression_eqeq(ALLOC node, p, sname, sline)) {
        return FALSE;
    }

    while(**p) {
        if(**p == '&' && *(*p+1) != '&' && *(*p+1) != '=') {
            (*p)++;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_eqeq(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpAnd, *node, right, NULL);
        }
        else {
            break;
        }
    }

    return TRUE;
}

/// from left to right order
static BOOL expression_xor(ALLOC sNodeTree** node, char** p, char* sname, int* sline)
{
    if(!expression_and(ALLOC node, p, sname, sline)) {
        return FALSE;
    }

    while(**p) {
        if(**p == '^' && *(*p+1) != '=') {
            (*p)++;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_and(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpXor, *node, right, NULL);
        }
        else {
            break;
        }
    }

    return TRUE;
}

/// from left to right order
static BOOL expression_or(ALLOC sNodeTree** node, char** p, char* sname, int* sline)
{
    if(!expression_xor(ALLOC node, p, sname, sline)) {
        return FALSE;
    }

    while(**p) {
        if(**p == '|' && *(*p+1) != '|' && *(*p+1) != '=') {
            (*p)++;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_xor(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpOr, *node, right, NULL);
        }
        else {
            break;
        }
    }

    return TRUE;
}

/// from left to right order
static BOOL expression_andnand(ALLOC sNodeTree** node, char** p, char* sname, int* sline)
{
    if(!expression_or(ALLOC node, p, sname, sline)) {
        return FALSE;
    }

    while(**p) {
        if(**p == '&' && *(*p+1) == '&') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_or(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpAndAnd, *node, right, NULL);
        }
        else {
            break;
        }
    }

    return TRUE;
}

/// from left to right order
static BOOL expression_oror(ALLOC sNodeTree** node, char** p, char* sname, int* sline)
{
    if(!expression_andnand(ALLOC node, p, sname, sline)) {
        return FALSE;
    }

    while(**p) {
        if(**p == '|' && *(*p+1) == '|') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_andnand(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpOrOr, *node, right, NULL);
        }
        else {
            break;
        }
    }

    return TRUE;
}

// from right to left order
static BOOL expression_question(ALLOC sNodeTree** node, char** p, char* sname, int* sline)
{
    if(!expression_oror(ALLOC node, p, sname, sline)) {
        return FALSE;
    }

    while(**p) {
        if(**p == '?') {
            (*p)++;
            skip_spaces(p);
            sNodeTree* middle;
            if(!node_expression(ALLOC &middle, p, sname, sline)) {
                sNodeTree_free(middle);
                return FALSE;
            }

            skip_spaces(p);
            if(**p != ':') {
                err_msg("invalid ? expression", sname, *sline);
                sNodeTree_free(middle);
                return FALSE;
            }

            (*p)++;
            skip_spaces(p);

            sNodeTree* right;
            if(!node_expression(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(middle);
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(middle);
                sNodeTree_free(right);
                return FALSE;
            }
            if(middle == NULL) {
                err_msg("require middle value", sname, *sline);
                sNodeTree_free(middle);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(middle);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpQuestion, *node, right, middle);
        }
        else {
            break;
        }
    }

    return TRUE;
}

// from right to left order
static BOOL expression_equal(ALLOC sNodeTree** node, char** p, char* sname, int* sline)
{
    if(!expression_question(ALLOC node, p, sname, sline)) {
        return FALSE;
    }

    while(**p) {
        if(**p == '=') {
            (*p)++;
            skip_spaces(p);
            sNodeTree* right;
            if(!expression_equal(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            if((*node)->mType != 2 && (*node)->mType != 3) {
                err_msg("require varible name on left value of equal", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpEqual, *node, right, NULL);
        }
        else if(**p == '+' && *(*p+1) == '=') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_equal(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            if((*node)->mType != 2 && (*node)->mType != 3) {
                err_msg("require varible name on left value of equal", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpEqualPlus, *node, right, NULL);
        }
        else if(**p == '-' && *(*p+1) == '=') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_equal(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            if((*node)->mType != 2 && (*node)->mType != 3) {
                err_msg("require varible name on left value of equal", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpEqualMinus, *node, right, NULL);
        }
        else if(**p == '*' && *(*p+1) == '=') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_equal(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            if((*node)->mType != 2 && (*node)->mType != 3) {
                err_msg("require varible name on left value of equal", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpEqualMult, *node, right, NULL);
        }
        else if(**p == '/' && *(*p+1) == '=') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_equal(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            if((*node)->mType != 2 && (*node)->mType != 3) {
                err_msg("require varible name on left value of equal", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpEqualDiv, *node, right, NULL);
        }
        else if(**p == '%' && *(*p+1) == '=') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_equal(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            if((*node)->mType != 2 && (*node)->mType != 3) {
                err_msg("require varible name on left value of equal", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpEqualMod, *node, right, NULL);
        }
        else if(**p == '<' && *(*p+1) == '<' && *(*p+2) == '=') {
            (*p)+=3;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_equal(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            if((*node)->mType != 2 && (*node)->mType != 3) {
                err_msg("require varible name on left value of equal", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpEqualShiftLeft, *node, right, NULL);
        }
        else if(**p == '>' && *(*p+1) == '>' && *(*p+2) == '=') {
            (*p)+=3;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_equal(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            if((*node)->mType != 2 && (*node)->mType != 3) {
                err_msg("require varible name on left value of equal", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpEqualShiftRight, *node, right, NULL);
        }
        else if(**p == '&' && *(*p+1) == '=') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_equal(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            if((*node)->mType != 2 && (*node)->mType != 3) {
                err_msg("require varible name on left value of equal", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpEqualAnd, *node, right, NULL);
        }
        else if(**p == '|' && *(*p+1) == '=') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_equal(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            if((*node)->mType != 2 && (*node)->mType != 3) {
                err_msg("require varible name on left value of equal", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpEqualOr, *node, right, NULL);
        }
        else if(**p == '^' && *(*p+1) == '=') {
            (*p)+=2;
            skip_spaces(p);

            sNodeTree* right;
            if(!expression_equal(ALLOC &right, p, sname, sline)) {
                sNodeTree_free(right);
                return FALSE;
            }

            if(*node == NULL) {
                err_msg("require left value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }
            if(right == NULL) {
                err_msg("require right value", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            if((*node)->mType != 2 && (*node)->mType != 3) {
                err_msg("require varible name on left value of equal", sname, *sline);
                sNodeTree_free(right);
                return FALSE;
            }

            *node = sNodeTree_create_operand(kOpEqualXor, *node, right, NULL);
        }
        else {
            break;
        }
    }

    return TRUE;
}

BOOL node_expression(ALLOC sNodeTree** node, char** p, char* sname, int* sline)
{
    return expression_equal(ALLOC node, p, sname, sline);
}

static BOOL get_variable_from_node(sObject** var, char* var_name, sNodeTree* node, sObject* current_object, sRunInfo* runinfo)
{
    if(node->mType == 2) {
        if(strstr(var_name, "::")) {
            if(!get_object_from_argument(var, var_name, current_object, runinfo->mRunningObject, runinfo)) {
                return FALSE;
            }
        }
        else {
            sObject* current_object2 = current_object;
            *var = access_object(var_name, &current_object2, runinfo->mRunningObject);

            if(*var == NULL) {
                char buf[128];
                snprintf(buf, 128, "this variable don't exist (%s)\n", var_name);
                err_msg(buf, runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
        }
    }
    else { // must be node->mType == 3
        sObject* name = STRING_NEW_STACK("");
        if(!get_object_prefix_and_name_from_argument(var, name, var_name, runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) {
            return FALSE;
        }
    }

    return TRUE;
}

static int object_to_int(sObject* value)
{
    switch(STYPE(value)) {
    case T_INT:
        return SINT(value);

    case T_STRING:
        return atoi(string_c_str(value));

    default:
        return 0;
    }
}

static sObject* object_to_str(sObject* value)
{
    switch(STYPE(value)) {
    case T_INT: {
        char buf[128];
        snprintf(buf, 128, "%d", SINT(value));
        return STRING_NEW_STACK(buf);
        }
        break;

    case T_STRING:
        return value;

    default:
        return STRING_NEW_STACK("");
    }
}

static BOOL run_node(sObject** result, sNodeTree* node, sObject* nextin, sObject* nextout, sRunInfo* runinfo, sObject* current_object)
{
    sObject *lobject, *robject, *mobject;
    int value, lvalue, rvalue, mvalue;

    switch(node->mType) {
        /// operand ///
        case 0:
            switch(node->mOperand) {
            case kOpAdd: 
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) + object_to_int(robject);
                *result = INT_NEW_STACK(value);
                break;

            case kOpSub: 
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) - object_to_int(robject);

                *result = INT_NEW_STACK(value);
                break;

            case kOpMult: 
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) * object_to_int(robject);

                *result = INT_NEW_STACK(value);
                break;

            case kOpDiv: 
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) / object_to_int(robject);

                *result = INT_NEW_STACK(value);
                break;

            case kOpMod: 
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) % object_to_int(robject);

                *result = INT_NEW_STACK(value);
                break;

            case kOpEqEq: {
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) == object_to_int(robject);

                *result = INT_NEW_STACK(value);
                }
                break;

            case kOpNotEqEq: {
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) != object_to_int(robject);

                *result = INT_NEW_STACK(value);
                }
                break;

            case kOpGt: {
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) > object_to_int(robject);

                *result = INT_NEW_STACK(value);
                }
                break;

            case kOpGte: {
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) >= object_to_int(robject);

                *result = INT_NEW_STACK(value);
                }
                break;

            case kOpLt: {
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) < object_to_int(robject);

                *result = INT_NEW_STACK(value);
                }
                break;

            case kOpLte: {
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) <= object_to_int(robject);

                *result = INT_NEW_STACK(value);
                }
                break;

            case kOpOr: {
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) | object_to_int(robject);

                *result = INT_NEW_STACK(value);
                }
                break;

            case kOpXor: {
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) ^ object_to_int(robject);

                *result = INT_NEW_STACK(value);
                }
                break;

            case kOpAnd: {
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) & object_to_int(robject);

                *result = INT_NEW_STACK(value);
                }
                break;

            case kOpLeftShift: {
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) << object_to_int(robject);

                *result = INT_NEW_STACK(value);
                }
                break;

            case kOpRightShift: {
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) >> object_to_int(robject);

                *result = INT_NEW_STACK(value);
                }
                break;

            case kOpTilda: {
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = ~object_to_int(lobject);

                *result = INT_NEW_STACK(value);
                }
                break;

            case kOpExclamation: {
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = !object_to_int(lobject);

                *result = INT_NEW_STACK(value);
                }
                break;

            case kOpOrOr: {
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) || object_to_int(robject);

                *result = INT_NEW_STACK(value);
                }
                break;

            case kOpAndAnd: {
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = object_to_int(lobject) && object_to_int(robject);

                *result = INT_NEW_STACK(value);
                }
                break;

            case kOpPlusPlus: 
            case kOpMinusMinus: 
            case kOpPlusPlus2: 
            case kOpMinusMinus2: {
                /// get variable object ///
                sObject* var;
                sNodeTree* lnode = node->mLeft;   /// This must be varible name
                char* var_name = string_c_str(lnode->mVarName);
                if(!get_variable_from_node(&var, var_name, lnode, current_object, runinfo)) {
                    return FALSE;
                }

                if(STYPE(var) != T_STRING) {
                    char buf[128];
                    snprintf(buf, 128, "this variable is not value\n", var_name);
                    err_msg(buf, runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }

                char buf[128];

                switch(node->mOperand) {
                case kOpPlusPlus:
                    value = atoi(string_c_str(var)) + 1;

                    snprintf(buf, 128, "%d", value);
                    string_put(var, buf);

                    *result = INT_NEW_STACK(value);
                    break;

                case kOpMinusMinus:
                    value = atoi(string_c_str(var)) - 1;

                    snprintf(buf, 128, "%d", value);
                    string_put(var, buf);

                    *result = INT_NEW_STACK(value);
                    break;

                case kOpPlusPlus2:
                    value = atoi(string_c_str(var)) + 1;

                    snprintf(buf, 128, "%d", value);
                    string_put(var, buf);

                    *result = INT_NEW_STACK(value-1);
                    break;

                case kOpMinusMinus2:
                    value = atoi(string_c_str(var)) - 1;

                    snprintf(buf, 128, "%d", value);
                    string_put(var, buf);

                    *result = INT_NEW_STACK(value+1);
                    break;
                }
                }
                break;

            case kOpEqual: {
                sNodeTree* lnode = node->mLeft;   /// This must be varible name
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                sObject* uobject;
                char* name;

                if(lnode->mType == 2) {
                    uobject = SFUN(runinfo->mRunningObject).mLocalObjects;
                    name = string_c_str(lnode->mVarName);
                }
                else { // must be lnode->mType == 3
                    sObject* object;
                    sObject* name2 = STRING_NEW_STACK("");
                    if(!get_object_prefix_and_name_from_argument(&object, name2, string_c_str(lnode->mVarName)
                        , runinfo->mCurrentObject, runinfo->mRunningObject, runinfo)) 
                    {
                        return FALSE;
                    }

                    uobject = object;
                    name = string_c_str(name2);
                }

                sObject* new_var;

                switch(STYPE(robject)) {
                    case T_STRING:
                        new_var = STRING_NEW_GC(string_c_str(robject), TRUE);
                        uobject_put(uobject, name, new_var);
                        *result = STRING_NEW_STACK(string_c_str(robject));
                        break;

                    case T_INT:
                        value = SINT(robject);
                        char buf[128];
                        snprintf(buf, 128, "%d", value);
                        new_var = STRING_NEW_GC(buf, TRUE);
                        uobject_put(uobject, name, new_var);
                        *result = INT_NEW_STACK(value);
                        break;

                    default:
                        err_msg("unexpexted error of type on = operand", runinfo->mSName, runinfo->mSLine);
                        return FALSE;
                }
                }
                break;

            case kOpEqualPlus:
            case kOpEqualMinus:
            case kOpEqualMult:
            case kOpEqualDiv:
            case kOpEqualMod:
            case kOpEqualShiftLeft:
            case kOpEqualShiftRight:
            case kOpEqualAnd:
            case kOpEqualOr:
            case kOpEqualXor:
                {
                /// get variable object from left node ///
                sObject* var;
                sNodeTree* lnode = node->mLeft;   /// This must be varible name
                char* var_name = string_c_str(lnode->mVarName);
                if(!get_variable_from_node(&var, var_name, lnode, current_object, runinfo)) {
                    return FALSE;
                }

                if(STYPE(var) != T_STRING) {
                    char buf[128];
                    snprintf(buf, 128, "this variable is not value\n", var_name);
                    err_msg(buf, runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }

                /// run right ///
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                int value = atoi(string_c_str(var));
                int value2 = object_to_int(robject);
                
                switch(node->mOperand) {
                    case kOpEqualPlus:
                        value = value + value2;
                        break;

                    case kOpEqualMinus:
                        value = value - value2;
                        break;

                    case kOpEqualMult:
                        value = value * value2;
                        break;

                    case kOpEqualDiv:
                        value = value / value2;
                        break;

                    case kOpEqualMod:
                        value = value % value2;
                        break;

                    case kOpEqualShiftLeft:
                        value = value << value2;
                        break;

                    case kOpEqualShiftRight:
                        value = value >> value2;
                        break;

                    case kOpEqualAnd:
                        value = value & value2;
                        break;

                    case kOpEqualOr:
                        value = value | value2;
                        break;

                    case kOpEqualXor:
                        value = value ^ value2;
                        break;
                }

                char buf[128];
                snprintf(buf, 128, "%d", value);
                string_put(var, buf);

                *result = INT_NEW_STACK(value);
                }
                break;

            case kOpQuestion:
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                value = object_to_int(lobject);
                if(value) {
                    if(!run_node(&mobject, node->mMiddle, nextin, nextout, runinfo, current_object)) {
                        return FALSE;
                    }

                    *result = INT_NEW_STACK(object_to_int(mobject));
                }
                else {
                    if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                        return FALSE;
                    }

                    *result = INT_NEW_STACK(object_to_int(robject));
                }
                break;

            case kOpStrAdd:
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                *result = STRING_NEW_STACK(string_c_str(object_to_str(lobject)));
                string_push_back(*result, string_c_str(object_to_str(robject)));
                break;

            case kOpStrEqEq:
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = strcmp(string_c_str(object_to_str(lobject)), string_c_str(object_to_str(robject))) == 0;

                *result = INT_NEW_STACK(value);
                break;

            case kOpStrNotEqEq:
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = strcmp(string_c_str(object_to_str(lobject)), string_c_str(object_to_str(robject))) != 0;

                *result = INT_NEW_STACK(value);
                break;

            case kOpStrCmp:
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = strcmp(string_c_str(object_to_str(lobject)), string_c_str(object_to_str(robject)));

                if(value > 0) 
                    value = 1;
                else if(value < 0) 
                    value = -1;

                *result = INT_NEW_STACK(value);
                break;

            case kOpStrLt:
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = strcmp(string_c_str(object_to_str(lobject)), string_c_str(object_to_str(robject))) < 0;

                *result = INT_NEW_STACK(value);
                break;

            case kOpStrLe:
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = strcmp(string_c_str(object_to_str(lobject)), string_c_str(object_to_str(robject))) <= 0;

                *result = INT_NEW_STACK(value);
                break;

            case kOpStrGt:
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = strcmp(string_c_str(object_to_str(lobject)), string_c_str(object_to_str(robject))) > 0;

                *result = INT_NEW_STACK(value);
                break;

            case kOpStrGe:
                // node->mLeft, node->mRight must be non NULL. Checked on parse time
                if(!run_node(&lobject, node->mLeft, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }
                if(!run_node(&robject, node->mRight, nextin, nextout, runinfo, current_object)) {
                    return FALSE;
                }

                value = strcmp(string_c_str(object_to_str(lobject)), string_c_str(object_to_str(robject))) >= 0;

                *result = INT_NEW_STACK(value);
                break;

            default:
                err_msg("unexpected error on run_node\n", runinfo->mSName, runinfo->mSLine);
                return FALSE;
            }
            break;

        /// number ///
        case 1:
            *result = INT_NEW_STACK(node->mValue);
            break;

        /// variable name ///
        /// global variable name ///
        case 2:
        case 3: {
            /// get variable object from left node ///
            sObject* var;
            char* var_name = string_c_str(node->mVarName);
            if(strstr(var_name, "::")) {
                if(!get_object_from_argument(&var, var_name, current_object, runinfo->mRunningObject, runinfo)) {
                    return FALSE;
                }
            }
            else {
                sObject* current_object2 = current_object;
                var = access_object(var_name, &current_object2, runinfo->mRunningObject);

                if(var == NULL) {
                    char buf[128];
                    snprintf(buf, 128, "this variable don't exist (%s)\n", var_name);
                    err_msg(buf, runinfo->mSName, runinfo->mSLine);
                    return FALSE;
                }
            }

            *result = var;
            }
            break;

        //// string value ///
        case 4:
            *result = STRING_NEW_STACK(string_c_str(node->mStringValue));
            break;

        //// function call ///
        case 5: {
            sObject* command = STRING_NEW_STACK("");
            while(node) {
                string_push_back(command, node->mFunName);
                string_push_back2(command, ' ');
                string_push_back(command, node->mFunArg);

                if(node->mLeft) {
                    string_push_back2(command, '|');
                }

                node = node->mLeft;
            }

            sObject* nextout2 = FD_NEW_STACK();

            int rcode;
            if(!xyzsh_eval(&rcode, string_c_str(command), "colon statment", NULL, nextin, nextout2, 0, NULL, current_object))
            {
                return FALSE;
            }
            xyzsh_set_signal();  // changed signal settings in xyzsh_eval()

            if(SFD(nextout2).mBufLen == 0) {
                *result = INT_NEW_STACK(rcode == 0);
            }
            else {
                *result = STRING_NEW_STACK(SFD(nextout2).mBuf); // mBuf is null terminated if the data is not binary data.
            }
            }
            break;
    }

    return TRUE;
}

BOOL node_tree(sStatment* statment, sObject* nextin, sObject* nextout, sRunInfo* runinfo, sObject* current_object)
{
    int i;
    for(i=0; i < statment->mNodeTreeNum; i++) {
        sNodeTree* node = statment->mNodeTree[i];

        if(node) {
            sObject* value;
            if(!run_node(&value, node, nextin, nextout, runinfo, current_object)) {
                return FALSE;
            }

            switch(STYPE(value)) {
                case T_INT: 
                    runinfo->mRCode = SINT(value) == 0;
                    break;

                case T_STRING: 
                default:
                    runinfo->mRCode = 0;
                    break;
            }

            if(statment->mFlags & STATMENT_FLAGS_NODETREE_OUTPUT) {
                switch(STYPE(value)) {
                    case T_INT: {
                        char buf[128];
                        int size = snprintf(buf, 128, "%d\n", SINT(value));
                        if(!fd_write(nextout,  buf, size)) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        }
                        break;

                    case T_STRING: {
                        if(!fd_write(nextout, string_c_str(value), string_length(value))) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        if(!fd_writec(nextout, '\n')) {
                            err_msg("signal interrupt", runinfo->mSName, runinfo->mSLine);
                            runinfo->mRCode = RCODE_SIGNAL_INTERRUPT;
                            return FALSE;
                        }
                        }
                        break;
                }
            }
        }
    }

    return TRUE;
}

