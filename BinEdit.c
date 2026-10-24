
// Yummy shit

// W's on the chat dudeeeeeeeeee
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <signal.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define isatty _isatty
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#endif

#define U(x) ((unsigned long long)(x))
// fuck off bitch

// Uwu
enum {
    K_UP = 256, K_DOWN, K_LEFT, K_RIGHT, K_PGUP, K_PGDN,
    K_HOME, K_END, K_DEL, K_INS, K_REFRESH, K_EOF
}; // Semi-colons are retarded like @averagepc_ bitch on discord

typedef struct {
    int kind;
    size_t pos;
    unsigned char val;
} Op;

// I learnt static electricity ages ago, so fuck off dude
static unsigned char *buf;
static size_t len, cap;
static Op *undo;
static size_t ucount, ucap;
static size_t cur, top, bpr = 16;
static int nib, ascii_mode, insert_mode, modified;
static int rows = 24, cols = 80;
static char path[4096];
static char msg[256];
static char *ob;
static size_t olen, ocap;
static unsigned char *find_pat;
static size_t find_len;
static int term_on;

#ifdef _WIN32 // braindead little guy

static HANDLE hin, hout; // UwU
static DWORD oldin, oldout;

static void term_size(void)
{
    CONSOLE_SCREEN_BUFFER_INFO i; // Green people are cute
    if (GetConsoleScreenBufferInfo(hout, &i)) {
        cols = i.srWindow.Right - i.srWindow.Left + 1;
        rows = i.srWindow.Bottom - i.srWindow.Top + 1;
    }
}

static void term_restore(void)
{
    if (!term_on) return;
    term_on = 0;
    fputs("\x1b[0m\x1b[?25h\x1b[?1049l", stdout);
    fflush(stdout);
    SetConsoleMode(hin, oldin);
    SetConsoleMode(hout, oldout);
}

static int term_init(void)
{
    hin = GetStdHandle(STD_INPUT_HANDLE);
    hout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!GetConsoleMode(hin, &oldin) || !GetConsoleMode(hout, &oldout)) return 0;
    if (!SetConsoleMode(hout, oldout | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) return 0;
    SetConsoleMode(hin, ENABLE_WINDOW_INPUT);
    term_on = 1;
    fputs("\x1b[?1049h\x1b[2J", stdout);
    fflush(stdout);
    return 1;
}

static int read_key(void)
{
    INPUT_RECORD r;
    DWORD n;
    for (;;) {
        if (!ReadConsoleInput(hin, &r, 1, &n) || n == 0) return K_EOF;
        if (r.EventType == WINDOW_BUFFER_SIZE_EVENT) return K_REFRESH;
        if (r.EventType != KEY_EVENT || !r.Event.KeyEvent.bKeyDown) continue;
        switch (r.Event.KeyEvent.wVirtualKeyCode) {
        case VK_UP: return K_UP;
        case VK_DOWN: return K_DOWN;
        case VK_LEFT: return K_LEFT;
        case VK_RIGHT: return K_RIGHT;
        case VK_PRIOR: return K_PGUP;
        case VK_NEXT: return K_PGDN;
        case VK_HOME: return K_HOME;
        case VK_END: return K_END;
        case VK_DELETE: return K_DEL;
        case VK_INSERT: return K_INS;
        case VK_TAB: return 9;
        case VK_RETURN: return 13;
        case VK_BACK: return 8;
        case VK_ESCAPE: return 27;
        default: break;
        }
        {
            unsigned char ch = (unsigned char)r.Event.KeyEvent.uChar.AsciiChar;
            if (ch) return ch;
        }
    }
}

#else

static struct termios oldt;

static void onwinch(int s)
{
    (void)s;
}

static void term_size(void)
{
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        rows = ws.ws_row;
        cols = ws.ws_col;
    }
}

static void term_restore(void)
{
    static const char s[] = "\x1b[0m\x1b[?25h\x1b[?1049l";
    ssize_t w;
    if (!term_on) return;
    term_on = 0;
    w = write(1, s, sizeof s - 1);
    (void)w;
    tcsetattr(0, TCSAFLUSH, &oldt);
}

static void onterm(int s)
{
    term_restore();
    _exit(128 + s);
}

static int term_init(void)
{
    struct termios t;
    struct sigaction sa;
    ssize_t w;
    static const char s[] = "\x1b[?1049h\x1b[2J";
    if (tcgetattr(0, &oldt) != 0) return 0;
    t = oldt;
    t.c_iflag &= ~(tcflag_t)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    t.c_oflag &= ~(tcflag_t)OPOST;
    t.c_cflag |= CS8;
    t.c_lflag &= ~(tcflag_t)(ECHO | ICANON | IEXTEN | ISIG);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSAFLUSH, &t) != 0) return 0;
    term_on = 1;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = onwinch;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGWINCH, &sa, NULL);
    signal(SIGTERM, onterm);
    signal(SIGHUP, onterm);
    w = write(1, s, sizeof s - 1);
    (void)w;
    return 1;
}

static int ready(void)
{
    fd_set f;
    struct timeval t;
    t.tv_sec = 0;
    t.tv_usec = 50000;
    FD_ZERO(&f);
    FD_SET(0, &f);
    return select(1, &f, NULL, NULL, &t) > 0;
}

static int getb(void)
{
    unsigned char c;
    if (read(0, &c, 1) == 1) return c;
    return -1;
}

static int read_key(void)
{
    unsigned char c;
    ssize_t n = read(0, &c, 1);
    int c2, ch;
    if (n < 0) return errno == EINTR ? K_REFRESH : K_EOF;
    if (n == 0) return K_EOF;
    if (c != 27) return c;
    if (!ready()) return 27;
    c2 = getb();
    if (c2 == '[') {
        int a = 0, b = 0, st = 0;
        (void)b;
        while ((ch = getb()) >= 0) {
            if (ch >= '0' && ch <= '9') {
                if (st) b = b * 10 + ch - '0';
                else a = a * 10 + ch - '0';
                continue;
            }
            if (ch == ';') {
                st = 1;
                continue;
            }
            switch (ch) {
            case 'A': return K_UP;
            case 'B': return K_DOWN;
            case 'C': return K_RIGHT;
            case 'D': return K_LEFT;
            case 'H': return K_HOME;
            case 'F': return K_END;
            case '~':
                switch (a) {
                case 1: case 7: return K_HOME;
                case 2: return K_INS;
                case 3: return K_DEL;
                case 4: case 8: return K_END;
                case 5: return K_PGUP;
                case 6: return K_PGDN;
                default: return K_REFRESH;
                }
            default: return K_REFRESH;
            }
        }
        return 27;
    }
    if (c2 == 'O') {
        ch = getb();
        switch (ch) {
        case 'A': return K_UP;
        case 'B': return K_DOWN;
        case 'C': return K_RIGHT;
        case 'D': return K_LEFT;
        case 'H': return K_HOME;
        case 'F': return K_END;
        default: return K_REFRESH;
        }
    }
    return K_REFRESH;
}

#endif

static void setmsg(const char *f, ...)
{
    va_list a;
    va_start(a, f);
    vsnprintf(msg, sizeof msg, f, a);
    va_end(a);
}

static int hexv(int c)
{
    return isdigit(c) ? c - '0' : tolower(c) - 'a' + 10;
}

static int ensure(size_t need)
{
    size_t nc;
    unsigned char *p;
    if (need <= cap) return 1;
    nc = cap ? cap : 4096;
    while (nc < need) nc *= 2;
    p = realloc(buf, nc);
    if (!p) return 0;
    buf = p;
    cap = nc;
    return 1;
}

static void push_op(int kind, size_t pos, unsigned char val)
{
    if (ucount == ucap) {
        size_t nc = ucap ? ucap * 2 : 1024;
        Op *p = realloc(undo, nc * sizeof(Op));
        if (!p) return;
        undo = p;
        ucap = nc;
    }
    undo[ucount].kind = kind;
    undo[ucount].pos = pos;
    undo[ucount].val = val;
    ucount++;
}

static int raw_insert(size_t pos, unsigned char v)
{
    if (!ensure(len + 1)) return 0;
    memmove(buf + pos + 1, buf + pos, len - pos);
    buf[pos] = v;
    len++;
    return 1;
}

static void raw_delete(size_t pos)
{
    memmove(buf + pos, buf + pos + 1, len - pos - 1);
    len--;
}

static void do_undo(void)
{
    Op o;
    if (!ucount) {
        setmsg("Nothing to undo");
        return;
    }
    o = undo[--ucount];
    if (o.kind == 0) buf[o.pos] = o.val;
    else if (o.kind == 1) raw_delete(o.pos);
    else raw_insert(o.pos, o.val);
    cur = o.pos > len ? len : o.pos;
    nib = 0;
    modified = 1;
}

static void oadd(const char *s, size_t n)
{
    if (olen + n > ocap) {
        size_t nc = ocap ? ocap : 65536;
        char *p;
        while (nc < olen + n) nc *= 2;
        p = realloc(ob, nc);
        if (!p) return;
        ob = p;
        ocap = nc;
    }
    memcpy(ob + olen, s, n);
    olen += n;
}

static void os(const char *s)
{
    oadd(s, strlen(s));
}

static void ofmt(const char *f, ...)
{
    char t[1024];
    va_list a;
    int n;
    va_start(a, f);
    n = vsnprintf(t, sizeof t, f, a);
    va_end(a);
    if (n > 0) oadd(t, (size_t)(n < (int)sizeof t ? n : (int)sizeof t - 1));
}

static void draw(void)
{
    static const char help[] =
        " ^S Save  ^Q Quit  ^Z Undo  ^G Goto  ^F Find  ^N Next  Tab Hex/ASCII  Ins Ins/Ovr  Del Delete";
    int vis, r;
    size_t crow, i, base, idx;
    char st[512];
    int n;

    term_size();
    bpr = cols >= 77 ? 16 : 8;
    vis = rows - 3;
    if (vis < 1) vis = 1;
    crow = cur / bpr;
    if (crow < top) top = crow;
    if (crow >= top + (size_t)vis) top = crow - (size_t)vis + 1;

    olen = 0;
    os("\x1b[?25l");
    os("\x1b[1;1H\x1b[7mOffset    ");
    for (i = 0; i < bpr; i++) {
        ofmt("%02X ", (unsigned)i);
        if (i == 7 && bpr == 16) os(" ");
    }
    os(" ASCII\x1b[K\x1b[0m");

    for (r = 0; r < vis; r++) {
        ofmt("\x1b[%d;1H", r + 2);
        base = (top + (size_t)r) * bpr;
        if (base <= len) {
            ofmt("%08llX  ", U(base));
            for (i = 0; i < bpr; i++) {
                char hx[3];
                idx = base + i;
                if (idx < len) snprintf(hx, sizeof hx, "%02X", buf[idx]);
                else strcpy(hx, "  ");
                if (idx == cur) {
                    if (ascii_mode) {
                        os("\x1b[4m");
                        os(hx);
                        os("\x1b[0m");
                    } else if (nib == 0) {
                        os("\x1b[7m");
                        oadd(hx, 1);
                        os("\x1b[0m");
                        oadd(hx + 1, 1);
                    } else {
                        oadd(hx, 1);
                        os("\x1b[7m");
                        oadd(hx + 1, 1);
                        os("\x1b[0m");
                    }
                } else {
                    os(hx);
                }
                os(" ");
                if (i == 7 && bpr == 16) os(" ");
            }
            os(" ");
            for (i = 0; i < bpr; i++) {
                char ch = ' ';
                idx = base + i;
                if (idx < len) ch = (buf[idx] >= 32 && buf[idx] < 127) ? (char)buf[idx] : '.';
                if (idx == cur) {
                    os("\x1b[7m");
                    oadd(&ch, 1);
                    os("\x1b[0m");
                } else {
                    oadd(&ch, 1);
                }
            }
        }
        os("\x1b[K");
    }

    n = snprintf(st, sizeof st, " %s%s | 0x%llX (%llu) / %llu | ",
                 path[0] ? path : "[No Name]", modified ? " [+]" : "",
                 U(cur), U(cur), U(len));
    if (cur < len) {
        unsigned b = buf[cur];
        char bits[9];
        int j;
        for (j = 0; j < 8; j++) bits[j] = ((b >> (7 - j)) & 1) ? '1' : '0';
        bits[8] = 0;
        n += snprintf(st + n, sizeof st - (size_t)n, "%02X %u %s", b, b, bits);
        if (cur + 2 <= len) {
            unsigned v = (unsigned)buf[cur] | ((unsigned)buf[cur + 1] << 8);
            n += snprintf(st + n, sizeof st - (size_t)n, " u16:%u", v);
        }
        if (cur + 4 <= len) {
            uint32_t v = (uint32_t)buf[cur] | ((uint32_t)buf[cur + 1] << 8) |
                         ((uint32_t)buf[cur + 2] << 16) | ((uint32_t)buf[cur + 3] << 24);
            n += snprintf(st + n, sizeof st - (size_t)n, " u32:%llu", U(v));
        }
        n += snprintf(st + n, sizeof st - (size_t)n, " | ");
    }
    snprintf(st + n, sizeof st - (size_t)n, "%s %s", insert_mode ? "INS" : "OVR", ascii_mode ? "ASCII" : "HEX");
    if (strlen(st) > (size_t)cols) st[cols] = 0;
    ofmt("\x1b[%d;1H\x1b[7m%s\x1b[K\x1b[0m", rows - 1, st);
    ofmt("\x1b[%d;1H\x1b[K%.*s", rows, cols > 0 ? cols : 0, msg[0] ? msg : help);

    fwrite(ob, 1, olen, stdout);
    fflush(stdout);
}

static int prompt(const char *label, char *out, size_t max)
{
    size_t n = 0;
    out[0] = 0;
    for (;;) {
        int k;
        term_size();
        printf("\x1b[%d;1H\x1b[K%s%s\x1b[?25h", rows, label, out);
        fflush(stdout);
        k = read_key();
        if (k == '\r' || k == '\n') {
            printf("\x1b[?25l");
            fflush(stdout);
            return 1;
        }
        if (k == 27 || k == K_EOF) {
            printf("\x1b[?25l");
            fflush(stdout);
            return 0;
        }
        if (k == 8 || k == 127) {
            if (n) out[--n] = 0;
        } else if (k >= 32 && k < 127 && n + 1 < max) {
            out[n++] = (char)k;
            out[n] = 0;
        }
    }
}

static void do_goto(void)
{
    char s[64], *e;
    unsigned long long v;
    if (!prompt("Goto offset (0x.. hex or decimal): ", s, sizeof s) || !s[0]) return;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) v = strtoull(s + 2, &e, 16);
    else v = strtoull(s, &e, 10);
    if (*e) {
        setmsg("Bad offset");
        return;
    }
    cur = v > U(len) ? len : (size_t)v;
    nib = 0;
}

static void do_find(int fresh)
{
    size_t last, start, k, i;
    if (fresh) {
        char s[256];
        unsigned char *p;
        size_t n = 0;
        if (!prompt("Find (h:4D 5A hex bytes, else text): ", s, sizeof s) || !s[0]) return;
        p = malloc(strlen(s) + 1);
        if (!p) {
            setmsg("Out of memory");
            return;
        }
        if (s[0] == 'h' && s[1] == ':') {
            int hi = -1;
            char *c;
            for (c = s + 2; *c; c++) {
                int v;
                if (isspace((unsigned char)*c) || *c == ',') continue;
                if (!isxdigit((unsigned char)*c)) {
                    setmsg("Bad hex");
                    free(p);
                    return;
                }
                v = hexv((unsigned char)*c);
                if (hi < 0) hi = v;
                else {
                    p[n++] = (unsigned char)((hi << 4) | v);
                    hi = -1;
                }
            }
            if (hi >= 0 || !n) {
                setmsg("Bad hex length");
                free(p);
                return;
            }
        } else {
            n = strlen(s);
            memcpy(p, s, n);
        }
        free(find_pat);
        find_pat = p;
        find_len = n;
    }
    if (!find_len) {
        setmsg("No search pattern");
        return;
    }
    if (find_len > len) {
        setmsg("Not found");
        return;
    }
    last = len - find_len;
    start = cur < len ? cur + 1 : 0;
    for (k = 0; k <= last; k++) {
        i = (start + k) % (last + 1);
        if (buf[i] == find_pat[0] && !memcmp(buf + i, find_pat, find_len)) {
            cur = i;
            nib = 0;
            setmsg("Found at 0x%llX", U(i));
            return;
        }
    }
    setmsg("Not found");
}

static void save_file(void)
{
    FILE *f;
    size_t w = 0;
    int ok;
    if (!path[0]) {
        char s[sizeof path];
        if (!prompt("Save as: ", s, sizeof s) || !s[0]) {
            setmsg("Save cancelled");
            return;
        }
        strcpy(path, s);
    }
    f = fopen(path, "wb");
    if (!f) {
        setmsg("Cannot write %s", path);
        return;
    }
    if (len) w = fwrite(buf, 1, len, f);
    ok = (w == len);
    if (fclose(f) != 0) ok = 0;
    if (ok) {
        modified = 0;
        setmsg("Saved %llu bytes to %s", U(len), path);
    } else {
        setmsg("Write error");
    }
}

static int load_file(const char *p)
{
    FILE *f = fopen(p, "rb");
    size_t n;
    int err;
    if (!f) return 0;
    for (;;) {
        if (!ensure(len + 65536)) {
            fclose(f);
            return -1;
        }
        n = fread(buf + len, 1, 65536, f);
        len += n;
        if (n < 65536) break;
    }
    err = ferror(f);
    fclose(f);
    return err ? -1 : 1;
}

int main(int argc, char **argv)
{
    int armed = 0;
    if (argc > 1) {
        int r;
        if (strlen(argv[1]) >= sizeof path) {
            fprintf(stderr, "path too long\n");
            return 1;
        }
        strcpy(path, argv[1]);
        r = load_file(path);
        if (r < 0) {
            fprintf(stderr, "cannot read %s\n", path);
            return 1;
        }
        if (r == 0) setmsg("New file: %s", path);
    }
    if (!isatty(0) || !isatty(1)) {
        fprintf(stderr, "BinEdit needs an interactive terminal\n");
        return 1;
    }
    if (!term_init()) {
        fprintf(stderr, "BinEdit: terminal setup failed\n");
        return 1;
    }
    atexit(term_restore);

    for (;;) {
        int k;
        draw();
        k = read_key();
        if (k == K_EOF) break;
        if (k == K_REFRESH) continue;
        msg[0] = 0;
        if (k != 17) armed = 0;
        switch (k) {
        case K_UP:
            if (cur >= bpr) cur -= bpr;
            nib = 0;
            break;
        case K_DOWN:
            if (cur / bpr < len / bpr) cur = (cur + bpr > len) ? len : cur + bpr;
            nib = 0;
            break;
        case K_LEFT:
            if (cur > 0) cur--;
            nib = 0;
            break;
        case K_RIGHT:
            if (cur < len) cur++;
            nib = 0;
            break;
        case K_PGUP: {
            size_t step = (size_t)(rows > 4 ? rows - 3 : 1) * bpr;
            cur = cur >= step ? cur - step : cur % bpr;
            nib = 0;
            break;
        }
        case K_PGDN: {
            size_t step = (size_t)(rows > 4 ? rows - 3 : 1) * bpr;
            cur = cur + step > len ? len : cur + step;
            nib = 0;
            break;
        }
        case K_HOME:
            cur -= cur % bpr;
            nib = 0;
            break;
        case K_END:
            cur = cur - cur % bpr + bpr - 1;
            if (cur > len) cur = len;
            nib = 0;
            break;
        case K_DEL:
            if (cur < len) {
                push_op(2, cur, buf[cur]);
                raw_delete(cur);
                modified = 1;
            }
            nib = 0;
            break;
        case K_INS:
            insert_mode = !insert_mode;
            nib = 0;
            break;
        case 9:
            ascii_mode = !ascii_mode;
            nib = 0;
            break;
        case 8:
        case 127:
            if (insert_mode) {
                if (cur > 0) {
                    cur--;
                    push_op(2, cur, buf[cur]);
                    raw_delete(cur);
                    modified = 1;
                }
            } else if (cur > 0) {
                cur--;
            }
            nib = 0;
            break;
        case 17:
            if (!modified || armed) goto done;
            armed = 1;
            setmsg("Unsaved changes. Ctrl-Q again to discard and quit.");
            break;
        case 19:
            save_file();
            break;
        case 26:
            do_undo();
            break;
        case 7:
            do_goto();
            break;
        case 6:
            do_find(1);
            break;
        case 14:
            do_find(0);
            break;
        default:
            if (!ascii_mode && k < 128 && isxdigit(k)) {
                int v = hexv(k);
                if (cur >= len || (insert_mode && nib == 0)) {
                    if (!raw_insert(cur, (unsigned char)(v << 4))) {
                        setmsg("Out of memory");
                        break;
                    }
                    push_op(1, cur, 0);
                    modified = 1;
                    nib = 1;
                } else {
                    unsigned char o = buf[cur];
                    unsigned char nv = nib == 0 ? (unsigned char)((o & 0x0F) | (v << 4))
                                                : (unsigned char)((o & 0xF0) | v);
                    push_op(0, cur, o);
                    buf[cur] = nv;
                    modified = 1;
                    if (nib == 0) nib = 1;
                    else {
                        nib = 0;
                        cur++;
                    }
                }
            } else if (ascii_mode && k >= 32 && k < 127) {
                if (cur >= len || insert_mode) {
                    if (!raw_insert(cur, (unsigned char)k)) {
                        setmsg("Out of memory");
                        break;
                    }
                    push_op(1, cur, 0);
                } else {
                    push_op(0, cur, buf[cur]);
                    buf[cur] = (unsigned char)k;
                }
                modified = 1;
                cur++;
                nib = 0;
            }
            break;
        }
    }
done:
    term_restore();
    free(buf);
    free(undo);
    free(ob);
    free(find_pat);
    return 0;
}
