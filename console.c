// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"


#define INPUT_BUF 128
#define BUFFER_SIZE 80 *25 //max buffer size
static int active_console;

//function that will check active console
 int check_active_console(){
    struct proc* current_proc = myproc();
    if (current_proc == 0) {
        return 1;
    }
    if (current_proc->console_id == active_console)
    { 
        return 1;
    
    }
    return 0;
}

//screen struct

static struct {
    ushort  videobuffer[BUFFER_SIZE];
    int     cursorposition;
    int     flag;
} consbuffer[MAXCONSOLE];


//function that helps to find a flags
 int flag_finder()
{
    for (int i = 0; i < MAXCONSOLE ; i++)
    {
        if (consbuffer[i].flag  == 0)
        {
            return i;

        }
        
    }
    return -1;
}

//function that finds console - necessary for hotkey functionality

int console_finder()
{
    for (int i = active_console + 1; i < MAXCONSOLE ; i++)
    {
        if (consbuffer[i].flag  == 1)
        {
            return i;

        }
    }
    return 0;

}
//function that sets the flags for us

 void set_flag(int id, int flag)
{
    consbuffer[id].flag = flag;

}
struct kbdbuffer {
    char buf[INPUT_BUF];
    uint r;  // Read index
    uint w;  // Write index
    uint e;  // Edit index
};

struct kbdbuffer inputBuffer;

struct kbdbuffer * input = 0;

#define C(x)  ((x) - '@')  // Control-x



static void consputc(int);

static int panicked = 0;

static struct {
    struct spinlock lock;
    int locking;
} cons;

static void printint(int xx, int base, int sign) {
    static char digits[] = "0123456789abcdef";
    char buf[16];
    int i;
    uint x;

    if (sign && (sign = xx < 0)) {
        x = -xx;
    }
    else {
        x = xx;
    }

    i = 0;
    do {
        buf[i++] = digits[x % base];
    }
    while ((x /= base) != 0);

    if (sign) {
        buf[i++] = '-';
    }

    while (--i >= 0) {
        consputc(buf[i]);
    }
}
// Print to the console. only understands %d, %x, %p, %s.
void cprintf(char *fmt, ...) {
    int i, c, locking;
    uint *argp;
    char *s;

    locking = cons.locking;
    if (locking) {
        acquire(&cons.lock);
    }

    if (fmt == 0) {
        panic("null fmt");
    }

    argp = (uint*)(void*)(&fmt + 1);
    for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
        if (c != '%') {
            consputc(c);
            continue;
        }
        c = fmt[++i] & 0xff;
        if (c == 0) {
            break;
        }
        switch (c) {
            case 'd':
                printint(*argp++, 10, 1);
                break;
            case 'x':
            case 'p':
                printint(*argp++, 16, 0);
                break;
            case 's':
                if ((s = (char*)*argp++) == 0) {
                    s = "(null)";
                }
                for (; *s; s++) {
                    consputc(*s);
                }
                break;
            case '%':
                consputc('%');
                break;
            default:
                // Print unknown % sequence to draw attention.
                consputc('%');
                consputc(c);
                break;
        }
    }

    if (locking) {
        release(&cons.lock);
    }
}

void panic(char *s) {
    int i;
    uint pcs[10];

    cli();
    cons.locking = 0;
    // use lapiccpunum so that we can call panic from mycpu()
    cprintf("lapicid %d: panic: ", lapicid());
    cprintf(s);
    cprintf("\n");
    getcallerpcs(&s, pcs);
    for (i = 0; i < 10; i++) {
        cprintf(" %p", pcs[i]);
    }
    panicked = 1; // freeze other CPU
    for (;;) {
        ;
    }
}

#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory


static void sethardwarecursorposition(int position) {
	outb(CRTPORT, 14);
	outb(CRTPORT + 1, (position >> 8) & 0xFF);
	outb(CRTPORT, 15);
	outb(CRTPORT + 1, position & 0xFF);
}
int gethardwarecursorposition() {
	int position;

	outb(CRTPORT, 14);
	position = inb(CRTPORT + 1) << 8;
	outb(CRTPORT, 15);
	position |= inb(CRTPORT + 1);
	return position;

}
void grabscreentobuffer(int id) {
	memmove(&consbuffer[id].videobuffer[0], crt, sizeof(ushort) * 80 * 25);
	consbuffer[id].cursorposition = gethardwarecursorposition();
}

void outputbuffertoscreen(int id) {
	memmove(crt, &consbuffer[id].videobuffer[0], sizeof(ushort) * 80 * 25);
	sethardwarecursorposition(consbuffer[id].cursorposition);
}

//Helper function that makes sure we get correct content
void switchConsole(int id)
{
    int oldId = active_console;
    active_console = id;
    grabscreentobuffer(oldId);
    outputbuffertoscreen(id);

}

// Modified to use our new functions
static void cgaputc(int c) {
    int pos;
    ushort * videomemory;

    if (!check_active_console()) {
        pos = consbuffer[myproc()->console_id].cursorposition;
        videomemory = &consbuffer[myproc()->console_id].videobuffer[0];
    }
    else {
        pos = gethardwarecursorposition();
        videomemory = crt;
    }

    if (c == '\n') {
        pos += 80 - pos % 80;
    }
    else if (c == BACKSPACE) {
        if (pos > 0) {
            --pos;
        }
    }
    else {
        videomemory[pos++] = (c & 0xff) | 0x0700;  // black on white

    }
    if (pos < 0 || pos > 25 * 80) {
        panic("pos under/overflow");
    }

    if ((pos / 80) >= 24) { // Scroll up.
        memmove(videomemory, videomemory + 80, sizeof(videomemory[0]) * 23 * 80);
        pos -= 80;
        memset(videomemory + pos, 0, sizeof(videomemory[0]) * (24 * 80 - pos));
    }
    if (check_active_console()) {
        sethardwarecursorposition(pos);
    }
    else {
        consbuffer[myproc()->console_id].cursorposition = pos;
    }
    videomemory[pos] = ' ' | 0x0700;
}


void consputc(int c) {
    if (panicked) {
        cli();
        for (;;) {
            ;
        }
    }

    if (c == BACKSPACE) {
        uartputc('\b');
        uartputc(' ');
        uartputc('\b');
    }
    else {
        uartputc(c);
    }
    cgaputc(c);
}

int consoleget(void) {
    int c;

    acquire(&cons.lock);

    while ((c = kbdgetc()) <= 0) {
        if (c == 0) {
            c = kbdgetc();
        }
    }

    release(&cons.lock);

    return c;
}

void consoleintr(int (*getc)(void)) {
    int c, doprocdump = 0;

    acquire(&cons.lock);
    while ((c = getc()) >= 0) {
        switch (c) {
            case C('P'):  // Process listing.
                // procdump() locks cons.lock indirectly; invoke later
                doprocdump = 1;
                break;
            case C('U'):  // Kill line.
                while (input->e != input->w &&
                       input->buf[(input->e - 1) % INPUT_BUF] != '\n') {
                    input->e--;
                    consputc(BACKSPACE);
                }
                break;
            case C('H'):
            case '\x7f':  // Backspace
                if (input->e != input->w) {
                    input->e--;
                    consputc(BACKSPACE);
                }
                break;
                // hot-key to change between consoles
            case C('T'):
                int id = console_finder();
                switchConsole(id);
                break;
            default:
                if (c != 0 && input->e - input->r < INPUT_BUF) {
                    c = (c == '\r') ? '\n' : c;
                    input->buf[input->e++ % INPUT_BUF] = c;
                    consputc(c);
                    if (c == '\n' || c == C('D') || input->e == input->r + INPUT_BUF) {
                        input->w = input->e;
                        wakeup(&(input->r));
                    }
                }
                break;
        }
    }
    release(&cons.lock);
    if (doprocdump) {
        procdump();  // now call procdump() wo. cons.lock held
    }
}

//will stop any other process than the active console reading the keyboard
int consoleread(struct inode *ip, char *dst, int n) {
    uint target;
    int c;

    iunlock(ip);
    target = n;
    acquire(&cons.lock);
    while (n > 0) {
        while (input->r == input->w) {
            if (myproc()->killed) {
                release(&cons.lock);
                ilock(ip);
                return -1;
            }
            sleep(&(input->r), &cons.lock);
        }
        if (check_active_console())
        {
                
            c = input->buf[input->r++ % INPUT_BUF];
            if (c == C('D')) { // EOF
                if (n < target) {
                    // Save ^D for next time, to make sure
                    // caller gets a 0-byte result.
                    input->r--;
                }
                break;
            }
            *dst++ = c;
            --n;
            if (c == '\n') {
                break;
            }
        }
        else
        {
            sleep(&(input->r), &cons.lock);
        }
    
    }
    release(&cons.lock);
    ilock(ip);

    return target - n;
}

int consolewrite(struct inode *ip, char *buf, int n) {
    int i;

    iunlock(ip);
    acquire(&cons.lock);
    for (i = 0; i < n; i++) {
        consputc(buf[i] & 0xff);
    }
    release(&cons.lock);
    ilock(ip);

    return n;
}

void consoleinit(void) {
    initlock(&cons.lock, "console");
    consbuffer[0].flag = 1;

    // Initialise pointer to point to our console input buffer
    input = &inputBuffer;

    devsw[CONSOLE].write = consolewrite;
    devsw[CONSOLE].read = consoleread;
    cons.locking = 1;

    ioapicenable(IRQ_KBD, 0);
}

