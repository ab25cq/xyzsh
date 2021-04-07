#include "config.h"

#include "xyzsh.h"
#include "temulator.h"

#if defined(__DARWIN__)
#include <util.h>
#include <sys/time.h>
#elif defined(__LINUX__)
#define _XOPEN_SOURCE
#define __USE_XOPEN
#define __USE_UNIX98
#include <wchar.h>
#include <pty.h>
#include <ctype.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#if defined(__CYGWIN__)
#include <sys/time.h>
#include <pty.h>
#include <ctype.h>
#endif

sTEmulator* temulator_init(int height, int width)
{
    sTEmulator* self = MALLOC(sizeof(sTEmulator));
    memset(self, 0, sizeof(sTEmulator));

    self->mHeight = height;
    self->mWidth = width;

    self->mScrollTop = 0;
    self->mScrollBottom = height-1;

    self->mLines = MALLOC(sizeof(wchar_t*)*height);
    self->mAttr = MALLOC(sizeof(int*)*height);
    self->mDirty = MALLOC(sizeof(BOOL)*height);

    int i;
    for(i=0; i<height; i++) {
        self->mLines[i] = MALLOC(sizeof(wchar_t)*width);
        memset(self->mLines[i], 0, sizeof(wchar_t)*width);
        self->mAttr[i] = MALLOC(sizeof(int)*width);
        memset(self->mAttr[i], 0, sizeof(int)*width);
        self->mDirty[i] = TRUE;
    }

    self->mRightEdgeLoop = TRUE;

    self->mBufSize = 0;

    self->mEdgeX = -1;

    return self;
}

void temulator_final(sTEmulator* self)
{
    int i;
    for(i=0; i<self->mHeight; i++) {
        FREE(self->mLines[i]);
        FREE(self->mAttr[i]);
    }

    FREE(self->mLines);
    FREE(self->mAttr);
    FREE(self->mDirty);

    FREE(self);
}

static int gColors;

void temulator_init_colors()
{
    if(COLOR_PAIRS > 81) {
        use_default_colors();

        int fg, bg;
        for(fg = 0; fg <= 8; fg++) {
            for(bg = 0; bg <= 8; bg++) {
                init_pair(fg * 9 + bg, fg-1, bg-1);
            }
        }

        gColors = TRUE;
    }
    else {
        gColors = FALSE;
    }
}

static int get_attr(sTEmulator* self)
{
    if(gColors) 
        return self->mAttrNow | COLOR_PAIR(self->mFgColor * 9 + self->mBgColor);
    else
        return self->mAttrNow;
}

static void temulator_reset_terminal(sTEmulator* self)
{
    int i;
    for(i=0; i<self->mHeight; i++) {
        memset(self->mLines[i], 0, sizeof(wchar_t)*self->mWidth);
        int j;
        for(j=0; j<self->mWidth; j++) {
            self->mAttr[i][j] = get_attr(self);
        }

        self->mDirty[i] = TRUE;
    }
}

static void temulator_reset_terminal_to_tail(sTEmulator* self)
{
    memset(self->mLines[self->mCursorY] + self->mCursorX, 0, sizeof(wchar_t)*(self->mWidth - self->mCursorX));
    int i;
    for(i=self->mCursorX; i<self->mWidth; i++) {
        self->mAttr[self->mCursorY][i] = get_attr(self);
    }
    self->mDirty[self->mCursorY] = TRUE;

    for(i=self->mCursorY+1; i<self->mHeight; i++) {
        memset(self->mLines[i], 0, sizeof(wchar_t)*self->mWidth);

        int j;
        for(j=0; j<self->mWidth; j++) {
            self->mAttr[i][j] = get_attr(self);
        }

        self->mDirty[i] = TRUE;
    }
}

static void temulator_reset_terminal_from_head(sTEmulator* self)
{
    memset(self->mLines[self->mCursorY], 0, sizeof(wchar_t) * self->mCursorX);
    int i;
    for(i=0; i<self->mCursorX; i++) {
        self->mAttr[self->mCursorY][i] = get_attr(self);
    }
    self->mDirty[self->mCursorY] = TRUE;

    for(i=0; i<self->mCursorY-1; i++) {
        memset(self->mLines[i], 0, sizeof(wchar_t)*self->mWidth);
        int j;
        for(j=0; j<self->mWidth; j++) {
            self->mAttr[i][j] = get_attr(self);
        }

        self->mDirty[i] = TRUE;
    }
}

static void temulator_reset_line(sTEmulator* self)
{
    memset(self->mLines[self->mCursorY], 0, sizeof(wchar_t)*self->mWidth);
    int i;
    for(i=0; i<self->mWidth; i++) {
        self->mAttr[self->mCursorY][i] = get_attr(self);
    }
    self->mDirty[self->mCursorY] = TRUE;
}

static void temulator_reset_line_to_tail(sTEmulator* self)
{
    memset(self->mLines[self->mCursorY] + self->mCursorX, 0, sizeof(wchar_t)*(self->mWidth - self->mCursorX));
    int i;
    for(i=self->mCursorX; i<self->mWidth; i++) {
        self->mAttr[self->mCursorY][i] = get_attr(self);
    }
    self->mDirty[self->mCursorY] = TRUE;
}

static void temulator_reset_line_from_head(sTEmulator* self)
{
    memset(self->mLines[self->mCursorY], 0, sizeof(wchar_t) * self->mCursorX);
    int i;
    for(i=0; i<self->mCursorX; i++) {
        self->mAttr[self->mCursorY][i] = get_attr(self);
    }
    self->mDirty[self->mCursorY] = TRUE;
}

BOOL temulator_open(sTEmulator* self, fTEmulator fun, void* arg)
{
    struct winsize ws = { self->mHeight, self->mWidth, 0, 0 };

    int amaster;
    pid_t pid = forkpty(&amaster, NULL, NULL, &ws);

    if(pid < 0) {
        return FALSE;
    }
    else if(pid == 0) {
        setsid();
        setenv("TERM", "xterm", 1);

        fun(arg);

        exit(0);
    }

    self->mFD = amaster;
    self->mPid = pid;

    return TRUE;
}


static void temulator_revise_cursor_x(sTEmulator* self)
{
    if(self->mCursorX < 0) {
        self->mCursorY--;
        self->mCursorX = self->mWidth-1;
    }
    if(self->mCursorX >= self->mWidth) {
        if(self->mRightEdgeLoop) {
            self->mCursorX = 0;
            self->mCursorY++;
        }
        else {
            self->mCursorX = self->mWidth -1;
        }
    }
}

static void temulator_scroll(sTEmulator* self)
{
    if(self->mCursorY < self->mScrollTop) {
        /// scroll ///
        const int scroll_number = self->mScrollTop - self->mCursorY; 

        int i;
        for(i=0; i<scroll_number; i++) {
            FREE(self->mLines[self->mScrollBottom]);
            FREE(self->mAttr[self->mScrollBottom]);

            int j;
            for(j=self->mScrollBottom; j>self->mScrollTop; j--) {
                self->mLines[j] = self->mLines[j-1];
                self->mAttr[j] = self->mAttr[j-1];
                self->mDirty[j] = TRUE;
            }

            self->mLines[self->mScrollTop] = MALLOC(sizeof(wchar_t)*self->mWidth);
            memset(self->mLines[self->mScrollTop], 0, sizeof(wchar_t)*self->mWidth);
            self->mAttr[self->mScrollTop] = MALLOC(sizeof(int)*self->mWidth);
            memset(self->mAttr[self->mScrollTop], 0, sizeof(int)*self->mWidth);

            self->mDirty[self->mScrollTop] = TRUE;

            self->mCursorY++;
        }
    }
    if(self->mCursorY > self->mScrollBottom) {
        /// scroll ///
        const int scroll_number = self->mCursorY - self->mScrollBottom;

        int i;
        for(i=0; i<scroll_number; i++) {
            FREE(self->mLines[self->mScrollTop]);
            FREE(self->mAttr[self->mScrollTop]);

            int j;
            for(j=self->mScrollTop; j<self->mScrollBottom; j++) {
                self->mLines[j] = self->mLines[j+1];
                self->mAttr[j] = self->mAttr[j+1];
                self->mDirty[j] = TRUE;
            }
            self->mLines[j] = MALLOC(sizeof(wchar_t)*self->mWidth);
            memset(self->mLines[j], 0, sizeof(wchar_t)*self->mWidth);
            self->mAttr[j] = MALLOC(sizeof(int)*self->mWidth);
            memset(self->mAttr[j], 0, sizeof(int)*self->mWidth);

            self->mDirty[j] = TRUE;

            self->mCursorY--;
        }
    }
}

static void temulator_interpret_csi(sTEmulator* self)
{
    const int csi_param_max = 32;
    int csiparam[csi_param_max];
    memset(csiparam, 0, sizeof(int)*csi_param_max);

    char* p = self->mEscapedBuf + 1;

    if(*p == '?') { p++; }

    /// parse numeric parameters ///
    int param_count = 0;
    while(isdigit(*p) || *p == ';' || *p =='?') {
        if(*p == '?') {
        }
        else if(*p == ';') {
            param_count++;
            if(param_count >= csi_param_max) {
                return;
            }
        } else {
            if(param_count == 0) param_count++;

            csiparam[param_count-1] *= 10;
            csiparam[param_count-1] += *p - '0';
        }

        p++;
    }

    if(self->mEscapedBuf[1] == '?') { 
        switch(*p) {
            case 'h':
                if(param_count == 1) {
                    switch(csiparam[0]) {
                        case 7:
                            self->mRightEdgeLoop = TRUE;
                            break;

                        case 25:
                            break;

                        default:
                            break;
                    }
                }
                break;

            case 'l':
                if(param_count == 1) {
                    switch(csiparam[0]) {
                        case 7:
                            self->mRightEdgeLoop = FALSE;
                            break;

                        case 25:
                            break;

                        default:
                            break;
                    }
                }
                break;

            default: 
                break;
        }
    }
    else {
        int param0;
        if(param_count > 0 && csiparam[0] > 0) {
            param0 = csiparam[0];
        }
        else {
            param0 = 1;
        }

        switch(*p) {
            case 'A':
                if(param_count == 0) {
                    self->mCursorY --;
                }
                else if(param_count >= 1) {
                    self->mCursorY -= param0;
                }

                if(self->mCursorY < 0) { // no scroll
                    self->mCursorY = 0;
                }
                break;

            case 'B':
            case 'e':
                if(param_count == 0) {
                    self->mCursorY ++;
                }
                else if(param_count >= 1) {
                    self->mCursorY += param0;
                }

                if(self->mCursorY >= self->mHeight) { // no scroll
                    self->mCursorY = self->mHeight-1;
                }
                break;

            case 'C':
            case 'a':
                if(param_count == 0) {
                    self->mCursorX ++;
                }
                else if(param_count >= 1) {
                    self->mCursorX += param0;
                }
                if(self->mCursorX >= self->mWidth) { // no scroll
                    self->mCursorX = self->mWidth -1;
                }
                break;

            case 'D':
                if(param_count == 0) {
                    self->mCursorX --;
                }
                else if(param_count >= 1) {
                    self->mCursorX -= param0;
                }
                if(self->mCursorX < 0) { // no scroll
                    self->mCursorX = 0;
                }
                break;

            case 'E':
                if(param_count == 0) {
                    self->mCursorX = 0;
                    self->mCursorY ++;
                }
                else if(param_count >= 1) {
                    self->mCursorX = 0;
                    self->mCursorY += param0;
                }
                if(self->mCursorY >= self->mHeight) { // no scroll
                    self->mCursorY = self->mHeight-1;
                }
                break;

            case 'F':
                if(param_count == 0) {
                    self->mCursorX = 0;
                    self->mCursorY --;
                }
                else if(param_count >= 1) {
                    self->mCursorX = 0;
                    self->mCursorY -= param0;
                }

                if(self->mCursorY < 0) { // no scroll
                    self->mCursorY = 0;
                }
                break;

            case 'G':
            case '`':
                if(param_count >= 1) {
                    self->mCursorX = csiparam[0] -1;
                }
                if(self->mCursorX < 0) { // no scroll
                    self->mCursorX = 0;
                }
                if(self->mCursorX >= self->mWidth) {
                    self->mCursorX = self->mWidth-1;
                }
                break;

            case 'd':
                if(param_count >= 1) {
                    self->mCursorY = csiparam[0] -1;
                }
                if(self->mCursorY < 0) {
                    self->mCursorY = 0;
                }
                if(self->mCursorY >= self->mHeight) {
                    self->mCursorY = self->mHeight-1;
                }
                break;

            case 'H':
            case 'f':
                if(param_count == 0) {
                    self->mCursorY = 0;
                    self->mCursorX = 0;
                }
                else if(param_count >= 2) {
                    self->mCursorY = csiparam[0]-1;
                    self->mCursorX = csiparam[1]-1;
                }
                if(self->mCursorX < 0) {
                    self->mCursorX = 0;
                }
                else if(self->mCursorX >= self->mWidth) {
                    self->mCursorX = self->mWidth -1;
                }

                if(self->mCursorY < 0) {
                    self->mCursorY = 0;
                }
                else if(self->mCursorY >= self->mHeight) {
                    self->mCursorY = self->mHeight -1;
                }
                break;

            case 'J':
                if(param_count == 0) {
                    temulator_reset_terminal_to_tail(self);
                }
                else if(param_count >= 1) {
                    switch(csiparam[0]) {
                        case 0:
                            temulator_reset_terminal_to_tail(self);
                            break;

                        case 1:
                            temulator_reset_terminal_from_head(self);
                            break;

                        case 2:
                            temulator_reset_terminal(self);
                            break;

                        default:
                            break;
                    }
                }
                break;

            case 'K':
                if(param_count == 0) {
                    temulator_reset_line_to_tail(self);
                }
                else if(param_count >= 1) {
                    switch(csiparam[0]) {
                        case 0:
                            temulator_reset_line_to_tail(self);
                            break;
                        
                        case 1:
                            temulator_reset_line_from_head(self);
                            break;

                        case 2:
                            temulator_reset_line(self);
                            break;

                        default:
                            break;
                    }
                }
                break;

            case 'c':
                if(param_count == 0) {
                }
                else if(param_count >= 1) {
                    if(csiparam[0] == 0) {
                    }
                }
                break;

            /// tab stop ///
            case 'g':
                if(param_count == 0) {
                }
                else if(param_count >= 1) {
                    if(csiparam[0] == 0) {
                    }
                    else if(csiparam[0] == 3) {
                    }
                }
                break;

            case 'h':
                break;

            case 'l':
                break;

            case 'm':
                if(param_count == 0) {
                    self->mAttrNow = 0;
                    self->mFgColor = 0;
                    self->mBgColor = 0;
                }
                else if(param_count >= 1) {
                    int i;
                    for(i=0; i<param_count; i++) {
                        switch(csiparam[i]) {
                            case 0:
                                self->mAttrNow = A_NORMAL;
                                self->mFgColor = 0;
                                self->mBgColor = 0;
                                break;

                            case 1:
                                self->mAttrNow |= A_BOLD;
                                break;

                            case 4:
                                self->mAttrNow |= A_UNDERLINE;
                                break;

                            case 5:
                            case 6:
                                self->mAttrNow |= A_BLINK;
                                break;

                            case 7:
                                self->mAttrNow |= A_REVERSE;
                                break;

                            case 8:
                                self->mAttrNow |= A_INVIS;
                                break;

                            case 22:
                                self->mAttrNow &= ~A_BOLD;
                                break;

                            case 24:
                                self->mAttrNow &= ~A_UNDERLINE;
                                break;

                            case 25:
                                self->mAttrNow &= ~A_BLINK;
                                break;

                            case 27:
                                self->mAttrNow &= ~A_REVERSE;
                                break;

                            case 28:
                                self->mAttrNow &= ~A_INVIS;
                                break;

                            case 30:
                            case 31:
                            case 32:
                            case 33:
                            case 34:
                            case 35:
                            case 36:
                            case 37:
                                self->mFgColor = csiparam[i] - 30 + 1;
                                break;

                            case 39:
                                self->mFgColor = 0;
                                break;

                            case 40:
                            case 41:
                            case 42:
                            case 43:
                            case 44:
                            case 45:
                            case 46:
                            case 47:
                                self->mBgColor = csiparam[i] - 40 + 1;
                                break;

                            case 49:
                                self->mBgColor = 0;
                                break;

                            default:
                                break;

                        }
                    }
                }
                break;

            case 'n':
                break;

            case 'q':
                break;

            case 'r':
                if(param_count == 0) {
                    self->mScrollTop = 0;
                    self->mScrollBottom = self->mHeight-1;
                }
                else if(param_count == 2 && csiparam[0] < csiparam[1]) {
                    self->mScrollTop = csiparam[0]-1;
                    self->mScrollBottom = csiparam[1]-1;

                    if(self->mScrollTop < 0) {
                        self->mScrollTop = 0;
                    }
                    else if(self->mScrollTop >= self->mHeight) {
                        self->mScrollTop = self->mHeight-1;
                    }
                    if(self->mScrollBottom < 0) {
                        self->mScrollBottom = 0;
                    }
                    else if(self->mScrollBottom >= self->mHeight) {
                        self->mScrollBottom = self->mHeight-1;
                    }
                    
                    self->mCursorX = 0;
                    self->mCursorY = 0;
                }
                break;

            case 'y':
                break;

            case 'x':
                break;

            case '@': {
                int space = param0;
                if(self->mCursorX + space >= self->mWidth) {
                    space = self->mWidth - self->mCursorX;
                }

                int x;
                for(x=self->mWidth-1; x >= self->mCursorX+space; x--) {
                    self->mLines[self->mCursorY][x] = self->mLines[self->mCursorY][x - space];
                    self->mAttr[self->mCursorY][x] = self->mAttr[self->mCursorY][x - space];
                }

                for(x=self->mCursorX; x<self->mCursorX+space; x++) {
                    self->mLines[self->mCursorY][x] = 0;
                    self->mAttr[self->mCursorY][x] = get_attr(self);
                }

                self->mDirty[self->mCursorY] = TRUE;
                }
                break;

            case 'P': {
                int n;
                if(param0 <= 0) {
                    n = 1;
                }
                else {
                    n = param0;
                }
                
                int x;
                for(x=self->mCursorX; x<self->mHeight; x++) {
                    if(x+n >= self->mWidth) {
                        self->mLines[self->mCursorY][x] = 0;
                        self->mAttr[self->mCursorY][x] = get_attr(self);
                    }
                    else {
                        self->mLines[self->mCursorY][x] = self->mLines[self->mCursorX][x+n];
                        self->mAttr[self->mCursorY][x] = self->mAttr[self->mCursorY][x+n];
                    }

                    self->mDirty[self->mCursorY] = TRUE;
                }
                }
                break;

            case 'L': {
                int n;
                if(param0 <= 0) {
                    n = 1;
                }
                else {
                    n = param0;
                }

                int y;
                for(y=self->mScrollBottom-n+1; y<=self->mScrollBottom; y++) {
                    FREE(self->mLines[y]);
                    FREE(self->mAttr[y]);
                }

                for(y=self->mScrollBottom; y>=self->mCursorY+n; y--) {
                    self->mLines[y] = self->mLines[y-n];
                    self->mAttr[y] = self->mAttr[y-n];
                    self->mDirty[y] = TRUE;
                }

                for(y=self->mCursorY; y<self->mCursorY+n; y++) {
                    self->mLines[y] = MALLOC(sizeof(wchar_t)*self->mWidth);
                    memset(self->mLines[y], 0, sizeof(wchar_t)*self->mWidth);
                    self->mAttr[y] = MALLOC(sizeof(int)*self->mWidth);
                    int j;
                    for(j=0; j<self->mWidth; j++) {
                        self->mAttr[y][j] = get_attr(self);
                    }
                    self->mDirty[y] = TRUE;
                }
                }
                break;

            case 'M': {
                int n;
                if(param0 <= 0) {
                    n = 1;
                }
                else {
                    n = param0;
                }

                int y;
                for(y=self->mCursorY; y<self->mCursorY+n; y++) {
                    FREE(self->mLines[y]);
                    FREE(self->mAttr[y]);
                }

                for(y=self->mCursorY; y<self->mScrollBottom-n+1; y++) {
                    self->mLines[y] = self->mLines[y+n];
                    self->mAttr[y] = self->mAttr[y+n];
                    self->mDirty[y] = TRUE;
                }

                for(y=self->mScrollBottom+1-n; y<=self->mScrollBottom; y++) {
                    self->mLines[y] = MALLOC(sizeof(wchar_t)*self->mWidth);
                    memset(self->mLines[y], 0, sizeof(wchar_t)*self->mWidth);
                    self->mAttr[y] = MALLOC(sizeof(int)*self->mWidth);
                    int j;
                    for(j=0; j<self->mWidth; j++) {
                        self->mAttr[y][j] = get_attr(self);
                    }
                    self->mDirty[y] = TRUE;
                }
                }
                break;

            case 'X': {
                int n;
                if(param0 <= 0) {
                    n = 1;
                }
                else {
                    n = param0;
                }

                int i;
                for(i=self->mCursorX; i<self->mCursorX+n; i++) {
                    self->mLines[self->mCursorY][i] = 0;
                    self->mAttr[self->mCursorY][i] = get_attr(self);
                }
                self->mDirty[self->mCursorY] = TRUE;
                }
                break;

            case 's':
                self->mSavedCursorX = self->mCursorX;
                self->mSavedCursorY = self->mCursorY;
                break;

            case 'u':
                self->mCursorX = self->mSavedCursorX;
                self->mCursorY = self->mSavedCursorY;
                break;

            default:
                break;
        }
    }
}

static void temulator_try_interpret_escape_squence(sTEmulator* self, BOOL* processed)
{
    const int len = strlen(self->mEscapedBuf);
    char last_char = self->mEscapedBuf[len-1];

    switch(self->mEscapedBuf[0]) {
        /// save cursor position and attributes ///
        case '7':
            self->mSavedCursorX2 = self->mCursorX;
            self->mSavedCursorY2 = self->mCursorY;
            self->mSavedAttr = self->mAttrNow;
            self->mSavedFgColor = self->mFgColor;
            self->mSavedBgColor = self->mBgColor;
            *processed = TRUE;
            break;

        /// reset cursor position and attributes ///
        case '8':
            self->mCursorX = self->mSavedCursorX2;
            self->mCursorY = self->mSavedCursorY2;
            self->mAttrNow = self->mSavedAttr;
            self->mFgColor = self->mSavedFgColor;
            self->mBgColor = self->mSavedBgColor;
            *processed = TRUE;
            break;

        /// application key pad mode ///
        case '=':
            *processed = TRUE;
            break;

        /// numeristic key pad mode ///
        case '>':
            *processed = TRUE;
            break;

        /// cursor down ///
        case 'D':
            self->mCursorY++;
            temulator_scroll(self);
            *processed = TRUE;
            break;

        /// new line ///
        case 'E': 
            self->mCursorX = 0;
            self->mCursorY++;
            temulator_revise_cursor_x(self);
            temulator_scroll(self);
            *processed = TRUE;
            break;

        /// tab stop ///
        case 'H':
            *processed = TRUE;
            break;

        /// cursor up ///
        case 'M':
            self->mCursorY--;
            temulator_revise_cursor_x(self);
            temulator_scroll(self);
            *processed = TRUE;
            break;

        /// terminal ID sequence ///
        case 'Z':
            *processed = TRUE;
            break;

        /// reset ///
        case 'c':
            temulator_reset_terminal(self);
            self->mCursorX = 0;
            self->mCursorY = 0;
            self->mAttrNow = 0;
            self->mFgColor = 0;
            self->mBgColor = 0;
            *processed = TRUE;
            break;

        /// 
        case '#':
            if(len == 2) {
                switch(self->mEscapedBuf[1]) {
                    case '3':
                        break;

                    case '4':
                        break;

                    case '5':
                        break;

                    case '6':
                        break;

                    case '8':
                        break;
                }
                *processed = TRUE;
            }
            else {
            }
            break;

        case '(':
        case ')':
            if(len == 2) {
                *processed = TRUE;
            }
            else {
            }
            break;
            
        case '[':
            if((last_char >= 'a' && last_char <= 'z')
                || (last_char >= 'A' && last_char <= 'Z')
                || last_char == '@')
            {
                temulator_interpret_csi(self);
                *processed = TRUE;
            }
            break;

        case '$':
            break;

        default:
            break;
    }
}

//#define ESCDEBUG

#ifdef ESCDEBUG
static void debug_output_all_interpret(char c)
{
    FILE* f = fopen("output.txt", "a");
    fprintf(f, "---\n");
    fprintf(f, "(%c) (%d)\n", c, c);
    fclose(f);
}

static void debug_write_escape_squence(sTEmulator* self)
{
    FILE* f = fopen("escape_squences.txt", "a");
    fprintf(f, "---\n");
    int j;
    for(j=0; j<strlen(self->mEscapedBuf); j++) {
        fprintf(f, "(%c) (%d)\n", self->mEscapedBuf[j], self->mEscapedBuf[j]);
    }
    fclose(f);
}
#endif

// buf requires null sentinel at tail
static BOOL temulator_interpret(sTEmulator* self)
{
    unsigned char* p = self->mBuf;
    while(p < self->mBuf + self->mBufSize) {

#ifdef ESCDEBUG
debug_output_all_interpret(*p);
#endif

        if(self->mEscaped) {
            char buf[32];
            buf[0] = *p++;
            buf[1] = 0;
            xstrncat(self->mEscapedBuf, buf, 32);

            BOOL processed = FALSE;
            temulator_try_interpret_escape_squence(self, &processed);

#ifdef ESCDEBUG
debug_write_escape_squence(self);
#endif

            if(processed) {
                xstrncpy(self->mEscapedBuf, "", 32);
                self->mEscaped = FALSE;
            }
        }
        else if(*p == '\n') {
            //self->mCursorX = 0;
            self->mCursorY++;
            //temulator_revise_cursor_x(self);
            temulator_scroll(self);
            p++;
        }
        else if(*p == '\r') {
            self->mCursorX = 0;
            p++;
        }
        else if(*p == '\t') {
            self->mCursorX = (self->mCursorX + 8) & ~7;
            if(self->mCursorX >= self->mWidth) {
                self->mCursorX = self->mWidth -1;
            }
            p++;
            temulator_revise_cursor_x(self);
            temulator_scroll(self);
        }
        /// enter graphical character mode ///
        else if(*p == 14) {
            p++;
        }
        /// exit graphical character mode ///
        else if(*p == 15) {
            p++;
        }
        /// csi character. equivalent to ESC [ ///
        else if(*p == 155) {
            self->mEscaped = TRUE;
            self->mEscapedBuf[0] = '[';
            self->mEscapedBuf[1] = 0;
            p++;
        }
        /// these interrupt escape sequences ///
        else if(*p == 24 || *p == 26) {
            self->mEscaped = FALSE;
            self->mEscapedBuf[0] = 0;
            p++;
        }
        /// bel ///
        else if(*p == '\a') {
            p++;
        }
        /// back space ///
        else if(*p == 8) {
            //self->mLines[self->mCursorY][self->mCursorX] = 0;
            self->mCursorX--;
            //temulator_revise_cursor_x(self);
            //temulator_scroll(self);
            if(self->mCursorX < 0) { // no scroll
                self->mCursorX = 0;
            }
            p++;
        }
        /// escape squence ///
        else if(*p == 27) {
            self->mEscaped = TRUE;
            self->mEscapedBuf[0] = 0;
            p++;
        }
        /// normal character ///
        else if(*p >= ' ' && *p < 127) {
            if(self->mEdgeX != -1) {
                if(self->mCursorX == self->mEdgeX && self->mCursorY == self->mEdgeY) {
                    self->mCursorX++;
                    temulator_revise_cursor_x(self);
                    temulator_scroll(self);
                }
                self->mEdgeX = -1;
            }

            self->mLines[self->mCursorY][self->mCursorX] = *p;
            self->mAttr[self->mCursorY][self->mCursorX] = get_attr(self);
            self->mDirty[self->mCursorY] = TRUE;
            p++;


            if(self->mCursorX == self->mWidth-1) {
                self->mEdgeX = self->mCursorX;
                self->mEdgeY = self->mCursorY;
            }
            else {
                self->mCursorX++;
                temulator_revise_cursor_x(self);
                temulator_scroll(self);
            }
        }
        else if(*p >= 128) {
            wchar_t wc;
            int size = ((*p & 0x80) >> 7) + ((*p & 0x40) >> 6) + ((*p & 0x20) >> 5) + ((*p & 0x10) >> 4);

            if(p + size <= self->mBuf + self->mBufSize) {
                wchar_t wc;
                mbrtowc(&wc, p, size, NULL);

                if(self->mEdgeX != -1) {
                    if(self->mCursorX == self->mEdgeX && self->mCursorY == self->mEdgeY) {
                        self->mCursorX++;
                        temulator_revise_cursor_x(self);
                        temulator_scroll(self);
                    }
                    self->mEdgeX = -1;
                }

                self->mLines[self->mCursorY][self->mCursorX] = wc;
                self->mAttr[self->mCursorY][self->mCursorX] = get_attr(self);
                self->mDirty[self->mCursorY] = TRUE;
                p+=size;

                if(self->mCursorX == self->mWidth-1) {
                    self->mEdgeX = self->mCursorX;
                    self->mEdgeY = self->mCursorY;
                }
                else {
                    self->mCursorX += wcwidth(wc);
                    temulator_revise_cursor_x(self);
                    temulator_scroll(self);
                }
            }
            else {
                break;
            }
        }
        else {
#ifdef ESCDEBUG
FILE* f = fopen("nonsuported_keys.txt", "a");
fprintf(f, "%d ", *p);
fclose(f);
#endif
            p++;
        }
    }

    const int size = self->mBuf + self->mBufSize - p;
    if(size > 0) { memcpy(self->mBuf, p, size); }
    self->mBufSize = size;

    return TRUE;
}

BOOL temulator_read(sTEmulator* self)
{
    int size = read(self->mFD, self->mBuf + self->mBufSize, BUFSIZ - self->mBufSize);
    self->mBufSize += size;

    if(size < 0) {
        return FALSE;
    }

    if(!temulator_interpret(self)) {
        return FALSE;
    }

    return TRUE;
}

static char* kKeyTable[KEY_MAX+1] = {
    ['\n']          = "\r",
    [KEY_UP]        = "\eOA",
    [KEY_DOWN]      = "\eOB",
    [KEY_RIGHT]     = "\eOC",
    [KEY_LEFT]      = "\eOD",
    [KEY_BACKSPACE] = "\b",
    [KEY_IC]        = "\e[2~",
    [KEY_DC]        = "\e[3~",
    [KEY_END]       = "\e[8~",
    [KEY_HOME]      = "\e[7~",
    [KEY_PPAGE]     = "\e[5~",
    [KEY_NPAGE]     = "\e[6~",
    [KEY_SUSPEND]   = "\x1A",     // ctrl-z
    [KEY_F(1)]      = "\e[[A",
    [KEY_F(2)]      = "\e[[B",
    [KEY_F(3)]      = "\e[[C",
    [KEY_F(4)]      = "\e[[D",
    [KEY_F(5)]      = "\e[[E",
    [KEY_F(6)]      = "\e[17~",
    [KEY_F(7)]      = "\e[18~",
    [KEY_F(8)]      = "\e[19~",
    [KEY_F(9)]      = "\e[20~",
    [KEY_F(10)]     = "\e[21~",
    [KEY_F(11)]     = "\e[22~",
    [KEY_F(12)]     = "\e[23~",
};

BOOL temulator_write(sTEmulator* self, int keycode)
{
    char buf[32];
    int size;

    if(keycode >= 0 && keycode < KEY_MAX && kKeyTable[keycode]) {
        xstrncpy(buf, (char*)kKeyTable[keycode], 32);
        size = strlen(buf);
    }
    else {
        buf[0] = (char)keycode;
        size = 1;
    }

    int result = write(self->mFD, buf, size);

    if(result < 0) {
        return FALSE;
    }

    return TRUE;
}

BOOL temulator_resize(sTEmulator* self, int height, int width)
{
    if(height < 10 || width < 10) {
        return FALSE;
    }

    /// old one is bigger than new one
    if(height < self->mHeight) {
        int i;
        for(i=height; i<self->mHeight; i++) {
            FREE(self->mLines[i]);
            FREE(self->mAttr[i]);
        }

        self->mLines = REALLOC(self->mLines, sizeof(wchar_t*)*height);
        self->mAttr = REALLOC(self->mAttr, sizeof(int*)*height);
        self->mDirty = REALLOC(self->mDirty, sizeof(BOOL)*height);

        for(i=0; i<height; i++) {
            self->mLines[i] = REALLOC(self->mLines[i], sizeof(wchar_t)*width);
            self->mAttr[i] = REALLOC(self->mAttr[i], sizeof(int)*width);

            if(width > self->mWidth) {
                memset(self->mLines[i] + self->mWidth, 0, sizeof(wchar_t)*(width-self->mWidth));
                memset(self->mAttr[i] + self->mWidth, 0, sizeof(int)*(width-self->mWidth));
            }

            self->mDirty[i] = TRUE;
        }
    }
    /// new one is bigger than old one
    else {
        self->mLines = REALLOC(self->mLines, sizeof(wchar_t*)*height);
        self->mAttr = REALLOC(self->mAttr, sizeof(int*)*height);
        self->mDirty = REALLOC(self->mDirty, sizeof(BOOL)*height);

        int i;
        for(i=0; i<self->mHeight; i++) {
            self->mLines[i] = REALLOC(self->mLines[i], sizeof(wchar_t)*width);
            self->mAttr[i] = REALLOC(self->mAttr[i], sizeof(int)*width);

            if(width > self->mWidth) {
                memset(self->mLines[i] + self->mWidth, 0, sizeof(wchar_t)*(width-self->mWidth));
                memset(self->mAttr[i] + self->mWidth, 0, sizeof(int)*(width-self->mWidth));
            }

            self->mDirty[i] = TRUE;
        }

        for(; i<height; i++) {
            self->mLines[i] = MALLOC(sizeof(wchar_t)*width);
            memset(self->mLines[i], 0, sizeof(wchar_t)*width);
            self->mAttr[i] = MALLOC(sizeof(int)*width);
            memset(self->mAttr[i], 0, sizeof(int)*width);

            self->mDirty[i] = TRUE;
        }
    }


    if(self->mCursorX >= width) {
        if(width > 0) 
            self->mCursorX = width-1;
        else 
            self->mCursorX = 0;
    }
    if(self->mCursorY >= height) {
        if(height > 0) 
            self->mCursorY = height-1;
        else
            self->mCursorY = 0;
    }

    //self->mAttrNow = 0;;
    //self->mFgColor = 0;
    //self->mBgColor = 0;

    self->mHeight = height;
    self->mWidth = width;

    self->mScrollTop = 0;
    self->mScrollBottom = height-1;

    self->mSavedCursorX = 0;
    self->mSavedCursorY = 0;

    self->mSavedCursorX2 = 0;
    self->mSavedCursorY2 = 0;
    self->mSavedAttr = 0;
    self->mSavedFgColor = 0;
    self->mSavedBgColor = 0;

    self->mEdgeX = -1;
    self->mEdgeY = 0;

    self->mEscaped = FALSE;
    self->mEscapedBuf[0] = 0;

    self->mRightEdgeLoop = TRUE;

    self->mBufSize = 0;

    struct winsize ws;
    ws.ws_row = height;
    ws.ws_col = width;

    ioctl(self->mFD, TIOCSWINSZ, &ws);
    kill(-self->mPid, SIGWINCH);

    return TRUE;
}

BOOL temulator_draw_on_curses(sTEmulator* self, WINDOW* win, int win_pos_y, int win_pos_x)
{
    curs_set(0);

    char buf[16];
    int len;
    wchar_t wc;

    static int cursor_y_before;

    int x;
    int y;
    for(y=0; y<self->mHeight; y++) {
        if(self->mDirty[y] || self->mCursorY == y || cursor_y_before == y) {
            wmove(win, y, 0);
            for(x=0; x<self->mWidth; ) {
                int attr;
                if(self->mCursorY == y && self->mCursorX == x) {
                    attr = self->mAttr[y][x] | A_REVERSE;
                }
                else {
                    attr = self->mAttr[y][x];
                }

                if(attr) {
                    wattrset(win, attr);
                }
                    
                wc = self->mLines[y][x];

                if(wc >= 128) {
                    int clumn = wcwidth(wc);

                    len = wcrtomb(buf, wc, NULL);
                    waddnstr(win, buf, len);

                    x += clumn;
                }
                else {
                    waddch(win, wc > ' ' ? wc : ' ');

                    x++;
                }

                if(attr) {
                    wattroff(win, attr);
                }
            }

            self->mDirty[y] = FALSE;
        }
    }

    move(self->mCursorY + win_pos_x, self->mCursorX + win_pos_y);
    
    cursor_y_before = self->mCursorY;

    return TRUE;
}

static BOOL gSigChld = FALSE;
static BOOL gSigWinch = FALSE;

static void handler(int signo)
{
    switch (signo) {
      case SIGCHLD:
        gSigChld = TRUE;
        break;
      case SIGWINCH:
        gSigWinch = TRUE;
        break;
    }
}

static int is_expired(struct timeval now, struct timeval expiry)
{
    return now.tv_sec > expiry.tv_sec
        || (now.tv_sec == expiry.tv_sec && now.tv_usec > expiry.tv_usec);
}

static struct timeval const slice = { 0, 1000 * 1000 / 100 };

static struct timeval timeval_add(struct timeval a, struct timeval b)
{
    int usec = a.tv_usec + b.tv_usec;
    a.tv_sec += b.tv_sec;
    while (usec > 1000 * 1000) {
        a.tv_sec += 1;
        usec -= 1000 * 1000;
    }
    a.tv_usec = usec;
    return a;
}


void run_on_temulator(fTEmulator fun, void* arg)
{
    gSigChld = FALSE;
    gSigWinch = FALSE;

    signal(SIGCHLD, handler);
    signal(SIGWINCH, handler);

    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();

    int temulator_y = 0;
    int temulator_x = 0;
    int temulator_height = maxy;
    int temulator_width = maxx;

    sTEmulator* temulator = temulator_init(temulator_height, temulator_width);

    temulator_open(temulator, fun, arg);

    initscr();
    start_color();
    noecho();
    raw();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    curs_set(0);
#if !defined(__CYGWIN__)
    ESCDELAY=50;
#endif

    temulator_init_colors();

    WINDOW* term_win = newwin(temulator_height, temulator_width, temulator_y, temulator_x);

    int pty = temulator->mFD;

    fd_set mask, read_ok;
    FD_ZERO(&mask);
    FD_SET(0, &mask);
    FD_SET(pty, &mask);

    int dirty = 0;
    struct timeval next;

    gettimeofday(&next, NULL);
    while(1) {
        struct timeval tv = { 0, 1000 * 1000 / 100 };
        read_ok = mask;

        if(select(pty+1, &read_ok, NULL, NULL, &tv) > 0) {
            if(FD_ISSET(pty, &read_ok)) {
                temulator_read(temulator);
                dirty = 1;
            }
        }

        int key;
        while((key = getch()) != ERR) {
            temulator_write(temulator, key);
            dirty = 1;
        }

        gettimeofday(&tv, NULL);
        if(dirty && is_expired(tv, next)) {
            temulator_draw_on_curses(temulator, term_win, temulator_y, temulator_x);
            wrefresh(term_win);
            dirty = 0;
            next = timeval_add(tv, slice);
        }

        if(gSigChld) {
            gSigChld = FALSE;
            break;
        }

        if(gSigWinch) {
            gSigWinch = 0;

            temulator_height = mgetmaxy();
            temulator_width = mgetmaxx();

            if(temulator_width >= 10 && temulator_height >= 10) {
                resizeterm(temulator_height, temulator_width);

                wresize(term_win, temulator_height, temulator_width);
                temulator_resize(temulator, temulator_height, temulator_width);

                dirty = 1;
            }
        }
    }

    endwin();

    temulator_final(temulator);
}
