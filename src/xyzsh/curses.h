/* 
 * terminal controlmanagement library
 */

#ifndef XYZSH_CURSES_H
#define XYZSH_CURSES_H

#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/types.h>

///////////////////////////////////////////////////////////////////////
// variables
///////////////////////////////////////////////////////////////////////
enum eTerminalKanjiCode { kTKEucjp, kTKSjis, kTKUtf8 };
extern enum eTerminalKanjiCode gTerminalKanjiCode;

#define kKeyMetaFirst 128

/////////////////////////////////////////////////////////////////////
// functions
/////////////////////////////////////////////////////////////////////
void mcurses_init(enum eTerminalKanjiCode code);
void mcurses_final();

void mclear_immediately();
void mclear_online(int y);

void mmove_immediately(int y, int x);

void mbox(int y, int x, int width, int height);

int mgetmaxx();
int mgetmaxy();

int mis_raw_mode();
void mreset_tty();
void msave_screen();
void mrestore_screen();
void mrestore_ttysettings();
void msave_ttysettings();

#endif

