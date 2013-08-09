#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wchar.h>
#include <errno.h>
#include <unistd.h>

#if !defined(__CYGWIN__)
#include <term.h>
#endif

#include "config.h"

#if defined(HAVE_CURSES_H)
#include <curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/ncurses.h>
#endif

#if defined(__CYGWIN__)
#include <ncurses/term.h>
#include <sys/time.h>
#endif

#include "xyzsh.h"

static int wis_2cols(wchar_t c)
{
    wchar_t buf[256];

    buf[0] = c;
    buf[1] = 0;

    return wcswidth(buf, 10) == 2;
}

static int gMCTtyFd;

static int write_escape_suqence(char* str)
{
    if(str) {
        char* p = str;
        while(*p && *p != '$') { p++; }
        return write(gMCTtyFd, str, p - str);
    }
}

static int ttywritec(char c)
{
    return write(gMCTtyFd, &c, 1);
}

static size_t ttyread(void *buf, size_t nbytes)
{
    return read(gMCTtyFd, buf, nbytes);
}

//////////////////////////////////////////////////////////////////////
// is hankaku character
//////////////////////////////////////////////////////////////////////
static int mhas_color()
{
    return tigetstr("setaf") == NULL;
}

void mbackspace_immediately()
{
    write_escape_suqence(tigetstr("cub1"));
    write_escape_suqence(tigetstr("dch1"));
}

void mbackspace_head_of_line_immediately()
{
    const int maxx = mgetmaxx();

    write_escape_suqence(tigetstr("cuu1"));
    write_escape_suqence(tparm(tigetstr("cuf"), maxx));
    write_escape_suqence(tigetstr("dch1"));
}

void mcursor_left_immediately(int n)
{
    write_escape_suqence(tparm(tigetstr("cub"), n));
}

void mcursor_right_immediately(int n)
{
    write_escape_suqence(tparm(tigetstr("cuf"), n));
}


void mmove_line_home_immediately()
{
    const int maxx = mgetmaxx();

    write_escape_suqence(tparm(tigetstr("cub"), maxx));
}

void mcurses_init()
{
    if(setupterm(NULL, STDOUT_FILENO, (int*) 0) == ERR) {
        fprintf(stderr, "invalid TERM setting");
        exit(1);
    }

    gMCTtyFd = open("/dev/tty", O_RDWR);
}

void mcurses_final()
{
    close(gMCTtyFd);
    
    //mreset_tty();
}

struct termios gSaveTty;

void msave_ttysettings()
{
    tcgetattr(gMCTtyFd, &gSaveTty);
}

void mrestore_ttysettings()
{
    tcsetattr(gMCTtyFd, TCSANOW, &gSaveTty);
}


int mgetmaxx()
{
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

    return ws.ws_col;
}

int mgetmaxy()
{
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

    return ws.ws_row;
}

void mreset_tty()
{
    struct termios t;

    t.c_cc[VINTR] =  3;
    t.c_cc[VQUIT] =  28;
    t.c_cc[VERASE] =  127;
    t.c_cc[VKILL] =  21;
    t.c_cc[VEOF] =  4;
    t.c_cc[VTIME] =  0;
    t.c_cc[VMIN] =  1;
#if defined(VSWTC)
    t.c_cc[VSWTC] =  0;
#endif
    t.c_cc[VSTART] =  17;
    t.c_cc[VSTOP] =  19;
    t.c_cc[VSUSP] =  26;
    t.c_cc[VEOL] =  0;
    t.c_cc[VREPRINT] =  18;
    t.c_cc[VDISCARD] =  15;
    t.c_cc[VWERASE] =  23;
    t.c_cc[VLNEXT] =  22;
    t.c_cc[VEOL2] =  0;

    t.c_iflag = BRKINT | IGNPAR | ICRNL | IXON | IMAXBEL;
    t.c_oflag = OPOST | ONLCR;
    t.c_cflag = CREAD | CS8 | B38400;
    t.c_lflag = ISIG | ICANON 
                | ECHO | ECHOE | ECHOK 
                //| ECHOKE 
                //| ECHOCTL | ECHOKE 
                | IEXTEN;

    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    //system("tset");
}

int mis_raw_mode()
{
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    
    if((tty.c_lflag & ICANON) == ICANON) {
        return 0;
    }
    else {
        return 1;
    }
}

void msave_screen()
{
#if defined(__FREEBSD__)
#else
    write_escape_suqence(tigetstr("smcup"));
    write_escape_suqence(tigetstr("sc"));
#endif
}

void mrestore_screen()
{
#if defined(__FREEBSD__)
#else
    write_escape_suqence(tigetstr("rmcup"));
    write_escape_suqence(tigetstr("rc"));
#endif
}

void mclear_immediately()
{
    write_escape_suqence(tigetstr("clear"));
}

void mclear()
{
clear();
//erase();
/*
    char space[1024];
    char space2[1024];
    int x, y;
    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();

    for(x=0; x<maxx; x++) {
       space[x] = ' ';
    }
    space[x] = 0;

    for(y=0; y<maxy-1; y++) {
       mvprintw(y, 0, space);
    }
    space[maxx-1] = 0;
    mvprintw(y, 0, space);
*/
}

void mclear_online(int y)
{
    char space[1024];
    int x;

    const int maxx = mgetmaxx();
    const int maxy = mgetmaxy();

    for(x=0; x<maxx; x++) {
        space[x] = ' ';
    }
    space[x] = 0;

    if(y == maxy -1) {
        space[maxx-1] = 0;
        mvprintw(y, 0, space);
    }
    else {
        mvprintw(y, 0, space);
    }
}

void mmove_immediately(int y, int x)
{
    write_escape_suqence(tparm(tigetstr("cup"), y, x));
}
