/* ------------------------------------------------------------------ */
/*  TANK.C  —  Turbo C Tank Battle                                     */
/*                                                                     */
/*  Move:  arrows / WASD                Restart: R    Quit: Q         */
/*                                                                     */
/*  Rules: every move you make, each enemy tank steps toward you.      */
/*  Any tank that drives onto a land mine is destroyed.                */
/*  Tanks colliding means GAME OVER — so does hitting the border.      */
/*  You have 1 life.                                                   */
/*                                                                     */
/*  Turbo C 3.0:   tcc tank.c                                          */
/*  Linux (shim):  gcc -I. tank.c -o tank  (or: make)                  */
/* ------------------------------------------------------------------ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __TURBOC__
/* Real Turbo C: use its own headers */
#include <conio.h>
#include <dos.h>
#else
/* ---- conio.h / dos.h compatibility layer (ANSI escapes + termios) ---- */
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* Turbo C colour constants */
enum
{
    BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LIGHTGRAY,
    DARKGRAY, LIGHTBLUE, LIGHTGREEN, LIGHTCYAN, LIGHTRED,
    LIGHTMAGENTA, YELLOW, WHITE
};
#define BLINK 128

static struct termios _tc_orig;
static int _tc_raw = 0;
static int _tc_pending = -1;   /* pushed-back scan code */

static void _tc_restore(void)
{
    if (_tc_raw)
    {
        tcsetattr(0, TCSANOW, &_tc_orig);
        _tc_raw = 0;
    }
    printf("\033[?25h\033[0m");   /* show cursor, reset attrs */
    fflush(stdout);
}

static void _tc_init(void)
{
    static int done = 0;
    struct termios t;
    if (done) return;
    done = 1;
    if (isatty(0))
    {
        tcgetattr(0, &_tc_orig);
        t = _tc_orig;
        t.c_lflag &= ~(ICANON | ECHO);
        t.c_cc[VMIN] = 1;
        t.c_cc[VTIME] = 0;
        tcsetattr(0, TCSANOW, &t);
        _tc_raw = 1;
    }
    atexit(_tc_restore);
    printf("\033[?25l");          /* hide cursor */
    fflush(stdout);
}

/* cursor position is 1-based, (1,1) = top-left — same as Turbo C */
static void gotoxy(int x, int y)
{
    printf("\033[%d;%dH", y, x);
}

static void clrscr(void)
{
    _tc_init();
    printf("\033[2J\033[H");
    fflush(stdout);
}

static void textcolor(int c)
{
    int fg = c & 0x7F;
    printf("\033[0m");
    if (fg < 8) printf("\033[%dm", 30 + fg);
    else        printf("\033[%dm", 90 + (fg - 8));
    if (c & BLINK) printf("\033[5m");
}

static void textbackground(int c)
{
    if (c < 8) printf("\033[%dm", 40 + c);
    else       printf("\033[%dm", 100 + (c - 8));
}

static int _tc_more(void)
{
    int n = 0;
    ioctl(0, FIONREAD, &n);
    return n > 0;
}

/* Behaves like TC's getch(): arrow/function keys return 0 first,
 * then the next call returns the DOS scan code (up=72, down=80,
 * left=75, right=77). */
static int getch(void)
{
    int c, c2, c3;
    _tc_init();
    if (_tc_pending >= 0)
    {
        c = _tc_pending;
        _tc_pending = -1;
        return c;
    }
    c = getchar();
    if (c == 27 && _tc_more())    /* escape sequence */
    {
        c2 = getchar();
        if (c2 == '[' || c2 == 'O')
        {
            if (!_tc_more()) return 27;
            c3 = getchar();
            switch (c3)
            {
                case 'A': _tc_pending = 72; return 0;  /* up    */
                case 'B': _tc_pending = 80; return 0;  /* down  */
                case 'C': _tc_pending = 77; return 0;  /* right */
                case 'D': _tc_pending = 75; return 0;  /* left  */
                case 'H': _tc_pending = 71; return 0;  /* home  */
                case 'F': _tc_pending = 79; return 0;  /* end   */
            }
        }
        return 27;
    }
    return c;
}

static int kbhit(void)
{
    _tc_init();
    return _tc_pending >= 0 || _tc_more();
}

static void textmode(int m) { (void)m; }

static void delay(unsigned milliseconds)
{
    struct timespec ts;
    ts.tv_sec  = milliseconds / 1000;
    ts.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}
#endif /* __TURBOC__ */

/* ---- board ------------------------------------------------------- */
#define W     36          /* grid width  (cells) */
#define H     18          /* grid height (cells) */
#define OX    2           /* screen column of board frame (1-based) */
#define OY    4           /* screen row of board frame            */
#define MAXE  10
#define MAXM  30

/* ---- directions -------------------------------------------------- */
#define UP    0
#define DOWN  1
#define LEFT  2
#define RIGHT 3

static const char *TANKCH[4] = { "^^", "vv", "<<", ">>" };

/* ---- game state -------------------------------------------------- */
static int  px, py, pdir, php;                  /* player           */
static int  ex[MAXE], ey[MAXE], ealive[MAXE];   /* enemies          */
static int  nenemies;
static int  mx[MAXM], my[MAXM], nmines;         /* land mines       */
static int  score, turns, over;
static char message[80];

/* ---- options ----------------------------------------------------- */
static int opt_enemies   = 4;
static int opt_mines     = 8;
static int opt_showmines = 1;

/* ------------------------------------------------------------------ */
static int in_bounds(int x, int y)
{
    return x >= 0 && y >= 0 && x < W && y < H;
}

static int mine_at(int x, int y)
{
    int i;
    for (i = 0; i < nmines; i++)
        if (mx[i] == x && my[i] == y) return i;
    return -1;
}

static void remove_mine(int i)
{
    mx[i] = mx[nmines - 1];
    my[i] = my[nmines - 1];
    nmines--;
}

static int enemy_at(int x, int y)
{
    int i;
    for (i = 0; i < nenemies; i++)
        if (ealive[i] && ex[i] == x && ey[i] == y) return i;
    return -1;
}

static int alive_enemies(void)
{
    int i, n = 0;
    for (i = 0; i < nenemies; i++) n += ealive[i];
    return n;
}

/* explosion overlay — drawn briefly, then the board redraws over it */
static void boom(int x, int y)
{
    gotoxy(OX + 1 + x * 2, OY + 1 + y);
    textcolor(YELLOW);
    printf("##");
    fflush(stdout);
    delay(150);
}

/* ------------------------------------------------------------------ */
/*  Random placement rules:                                            */
/*    - mines go down first, anywhere in the field, no duplicates      */
/*    - tanks never spawn on a mine, on another tank, or on the        */
/*      border edge                                                    */
/*    - enemies also keep a minimum distance from the player           */
/* ------------------------------------------------------------------ */
static void spawn(void)
{
    static int occ[H][W];
    int i, x, y, tries, want;

    memset(occ, 0, sizeof(occ));
    php = 1; pdir = UP; over = 0;
    message[0] = '\0';

    /* land mines first — anywhere, no duplicates */
    nmines = 0;
    for (i = 0; i < opt_mines && nmines < MAXM; i++)
    {
        tries = 0;
        do {
            x = rand() % W; y = rand() % H; tries++;
        } while (tries < 500 && occ[y][x]);
        if (tries >= 500) break;
        mx[nmines] = x; my[nmines] = y; nmines++;
        occ[y][x] = 1;
    }

    /* player — inside the border, never on a mine */
    tries = 0;
    do {
        px = 1 + rand() % (W - 2);
        py = 1 + rand() % (H - 2);
        tries++;
    } while (tries < 500 && occ[py][px]);
    occ[py][px] = 1;

    /* enemies — inside the border, never on a mine/tank,
       and at least 4 cells away from the player */
    want = opt_enemies;
    if (want > MAXE) want = MAXE;
    nenemies = 0;
    for (i = 0; i < want; i++)
    {
        tries = 0;
        do {
            x = 1 + rand() % (W - 2);
            y = 1 + rand() % (H - 2);
            tries++;
        } while (tries < 500 &&
                 (occ[y][x] || abs(x - px) + abs(y - py) < 4));
        if (tries >= 500) break;
        ex[nenemies] = x; ey[nenemies] = y; ealive[nenemies] = 1;
        nenemies++;
        occ[y][x] = 1;
    }
}

/* ------------------------------------------------------------------ */
static void move_enemies(void)
{
    static int occ[H][W];
    int i, o, dx, dy, nx, ny, first, k;
    int mxv[2], myv[2];

    memset(occ, 0, sizeof(occ));
    for (i = 0; i < nenemies; i++)
        if (ealive[i]) occ[ey[i]][ex[i]] = 1;

    for (i = 0; i < nenemies; i++)
    {
        if (!ealive[i]) continue;
        occ[ey[i]][ex[i]] = 0;

        dx = px > ex[i] ? 1 : (px < ex[i] ? -1 : 0);
        dy = py > ey[i] ? 1 : (py < ey[i] ? -1 : 0);

        /* greedy step toward the player, longer axis first */
        mxv[0] = mxv[1] = myv[0] = myv[1] = 0;
        first = abs(px - ex[i]) >= abs(py - ey[i]);
        if (first) { if (dx) mxv[0] = dx;  if (dy) myv[1] = dy; }
        else       { if (dy) myv[0] = dy;  if (dx) mxv[1] = dx; }

        for (o = 0; o < 2; o++)
        {
            if (mxv[o] == 0 && myv[o] == 0) continue;
            nx = ex[i] + mxv[o]; ny = ey[i] + myv[o];
            if (!in_bounds(nx, ny) || occ[ny][nx]) continue;
            ex[i] = nx; ey[i] = ny;
            break;
        }
        occ[ey[i]][ex[i]] = 1;

        k = mine_at(ex[i], ey[i]);
        if (k >= 0)                      /* enemy hit a mine — destroyed */
        {
            remove_mine(k);
            ealive[i] = 0;
            score += 10;
            boom(ex[i], ey[i]);
        }
        else if (ex[i] == px && ey[i] == py)   /* tanks collide — game over */
        {
            php = 0;
            over = 1;
            strcpy(message, "GAME OVER — an enemy tank rammed you!");
            boom(px, py);
        }
    }
}

static void end_turn(void)
{
    if (over) return;
    turns++;
    move_enemies();
    if (!over && alive_enemies() == 0)
    {
        score += 100;
        over = 1;
        strcpy(message, "FIELD CLEARED — YOU WIN! +100 pts");
    }
}

static void try_move(int dx, int dy, int dir)
{
    int nx = px + dx, ny = py + dy, k;
    pdir = dir;
    if (in_bounds(nx, ny))
    {
        px = nx; py = ny;
        k = mine_at(nx, ny);
        if (k >= 0)                          /* stepped on a mine — boom */
        {
            remove_mine(k);
            php = 0;
            over = 1;
            strcpy(message, "GAME OVER — you hit a land mine!");
            boom(nx, ny);
        }
        else if (enemy_at(nx, ny) >= 0)      /* tanks collide — game over */
        {
            php = 0;
            over = 1;
            strcpy(message, "GAME OVER — tank collision!");
            boom(nx, ny);
        }
    }
    else                                     /* ran into the border */
    {
        php = 0;
        over = 1;
        strcpy(message, "GAME OVER — you crashed into the border!");
        boom(px, py);
    }
    end_turn();
}

/* ------------------------------------------------------------------ */
static void draw_board(void)
{
    int x, y, i, d, k;

    /* HUD */
    gotoxy(1, 1);
    textcolor(WHITE);
    printf(" TANK BATTLE   Lives:%d  Score:%-4d Enemies:%-2d Turns:%-4d",
           php > 0 ? 1 : 0, score, alive_enemies(), turns);
    gotoxy(1, 2);
    textcolor(over && php > 0 ? LIGHTGREEN : LIGHTRED);
    printf(" %-72s", message);

    /* top frame */
    gotoxy(OX, OY);
    textcolor(LIGHTGRAY);
    putchar('+');
    for (x = 0; x < W * 2; x++) putchar('-');
    putchar('+');

    for (y = 0; y < H; y++)
    {
        gotoxy(OX, OY + 1 + y);
        textcolor(LIGHTGRAY);
        putchar('|');
        for (x = 0; x < W; x++)
        {
            if (x == px && y == py && !(over && php <= 0))
            {
                textcolor(LIGHTGREEN);
                printf("%s", TANKCH[pdir]);
            }
            else if ((i = enemy_at(x, y)) >= 0)
            {
                d = abs(px - ex[i]) > abs(py - ey[i])
                        ? (px > ex[i] ? RIGHT : LEFT)
                        : (py > ey[i] ? DOWN  : UP);
                textcolor(LIGHTRED);
                printf("%s", TANKCH[d]);
            }
            else if ((k = mine_at(x, y)) >= 0 && opt_showmines)
            {
                textcolor(YELLOW);
                printf("**");
            }
            else
            {
                printf("  ");
            }
        }
        textcolor(LIGHTGRAY);
        putchar('|');
    }

    /* bottom frame */
    gotoxy(OX, OY + H + 1);
    putchar('+');
    for (x = 0; x < W * 2; x++) putchar('-');
    putchar('+');

    gotoxy(1, OY + H + 2);
    textcolor(LIGHTGRAY);
    printf(" Arrows/WASD move   R restart   Q quit%s",
           over ? "   — press R or Q" : "");
    fflush(stdout);
}

/* ------------------------------------------------------------------ */
static int menu(void)          /* returns 1 = play, 0 = quit */
{
    int ch;
    clrscr();
    for (;;)
    {
        gotoxy(4, 3);
        textcolor(LIGHTCYAN);
        printf("=======  TANK BATTLE — OPTIONS  =======");
        gotoxy(6, 6);  textcolor(WHITE);
        printf("Enemies        [ - / + ] : %d ", opt_enemies);
        gotoxy(6, 7);
        printf("Land mines     [ , / . ] : %d ", opt_mines);
        gotoxy(6, 8);
        printf("ENTER start      Q quit");
        gotoxy(6, 12);
        printf("Enemies close in on every move. Mines destroy");
        gotoxy(6, 14);
        printf("ANY tank that steps on them — use them as bait!");
        fflush(stdout);

        ch = getch();
        if (ch == 0) { getch(); continue; }      /* skip scan code */
        switch (ch)
        {
            case '-': case '_': if (opt_enemies > 1)     opt_enemies--;   break;
            case '+': case '=': if (opt_enemies < MAXE)  opt_enemies++;   break;
            case ',':           if (opt_mines > 0)       opt_mines--;     break;
            case '.':           if (opt_mines < MAXM)    opt_mines++;     break;
            case 'm': case 'M': opt_showmines = !opt_showmines; break;
            case '\r': case '\n': return 1;
            case 'q': case 'Q': return 0;
        }
    }
}

/* ------------------------------------------------------------------ */
int main(void)
{
    int ch;

    srand((unsigned)time(NULL));
    clrscr();
    if (!menu())
    {
        clrscr();
        return 0;
    }

    score = 0; turns = 0;
    clrscr();
    spawn();
    draw_board();

    for (;;)
    {
        ch = getch();
        if (ch == EOF) break;            /* stdin closed (piped input) */
        if (ch == 0) ch = getch();       /* extended key -> scan code */

        if (ch == 'q' || ch == 'Q') break;
        if (ch == 'r' || ch == 'R')
        {
            score = 0; turns = 0;
            spawn();
            draw_board();
            continue;
        }
        if (over) continue;

        switch (ch)
        {
            case 72: case 'w': case 'W': try_move( 0, -1, UP);    break;
            case 80: case 's': case 'S': try_move( 0,  1, DOWN);  break;
            case 75: case 'a': case 'A': try_move(-1,  0, LEFT);  break;
            case 77: case 'd': case 'D': try_move( 1,  0, RIGHT); break;
            default: continue;
        }
        draw_board();
    }

    clrscr();
    gotoxy(1, 1);
    return 0;
}
