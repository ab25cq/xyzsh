#ifndef __TERMEMU_H
#define __TERMEMU_H

#include "config.h"

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

typedef struct {
    wchar_t** mLines;
    int** mAttr;
    BOOL* mDirty;

    int mHeight;
    int mWidth;

    int mCursorX;
    int mCursorY;
    int mAttrNow;

    short mFgColor;
    short mBgColor;

    int mScrollTop;
    int mScrollBottom;

    int mSavedCursorX;
    int mSavedCursorY;

    int mSavedCursorX2;
    int mSavedCursorY2;
    int mSavedAttr;
    short mSavedFgColor;
    short mSavedBgColor;

    int mEdgeX, mEdgeY;

    int mFD;
    pid_t mPid;

    BOOL mEscaped;
    char mEscapedBuf[32];

    BOOL mRightEdgeLoop;

    unsigned char mBuf[BUFSIZ];
    int mBufSize;
} sTEmulator;

typedef void (*fTEmulator)(void* arg);

sTEmulator* temulator_init(int height, int width);
extern void temulator_final(sTEmulator* self);

extern BOOL temulator_open(sTEmulator* self, fTEmulator fun, void* arg);
void temulator_init_colors(void);
extern BOOL temulator_read(sTEmulator* self);
extern BOOL temulator_write(sTEmulator* self, int key);
BOOL temulator_draw_on_curses(sTEmulator* self, WINDOW* win, int win_pos_y, int win_pos_x);
BOOL temulator_resize(sTEmulator* self, int height, int width);

void run_on_temulator(fTEmulator fun, void* arg);

#endif

