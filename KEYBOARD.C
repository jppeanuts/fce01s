/* Keyboard library functions */
/* H. Hanemaayer (hhanemaa@cs.ruu.nl) */

/*
 * Keyboard I/O based on showkey.c from kbd-0.84 package.
 *
 * This is an initial version, it isn't very safe yet since it only catches
 * sigsegv.
 */

/* DOS port by Mihai Cartoaje ( moisemih@ift.ulaval.ca ).
 */

#ifdef VGA

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#ifndef __DJGPP
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <linux/kd.h>
/* linux/keyboard.h defines NR_KEYS and some scancode-like constants, so it */
/* should also be useful for svgalib programs using the keyboard. It misses */
/* a few KERNEL ifdefs around kernel data structures though. */
#include <linux/keyboard.h>
#include <sys/vt.h>
#include <vga.h>
#include "../libvga.h"
#endif
#ifdef __DJGPP
#include <pc.h>
#define	port_in	inportb
#define	port_out	outportb
#include "vgakeybo.h"
#include <dpmi.h>
#include <go32.h>
#else
#include "vgakeyboard.h"
#endif

static void (*__keyboard_eventhandler) (int, int);

#ifndef __DJGPP
static struct termios oldkbdtermios, newkbdtermios;
static int oldkbmode;
#endif

/* vga.c needs to check that: */
int kbd_fd = -1;
static int ctrl_c_state_c, ctrl_c_state_control;
static int alt_state, functionkey_state;
static int translatemode = 0;

#ifndef __DJGPP
static unsigned char state[NR_KEYS];	/* NR_KEYS is defined in linux/keyboard.h */
#else
static unsigned char state[127];        /* This should be enough for most keyboards*/
#endif


static void default_handler(int, int);

#ifdef __DJGPP

static _go32_dpmi_seginfo wrapper;
static __dpmi_paddr oldHandler, newHandler;

#define DJGPPKEYBOARDBUFFERSIZE 256

char 	djgpp_buf[DJGPPKEYBOARDBUFFERSIZE];
int 	djgpp_bufferstart= 0, djgpp_bufferend = 0;
/* Must do our own interrupt handling. Luckily, DOS allows this. */

/* Temporary */

void kbdintr ( void ) 
{
	int tmp;

	tmp = port_in ( 0x60 );

	/* Insert keystroke into a queue awaiting processing */

	djgpp_buf [ djgpp_bufferend ++ ] = tmp;
	djgpp_bufferend &= DJGPPKEYBOARDBUFFERSIZE - 1;
	if ( djgpp_bufferend == djgpp_bufferstart ) {
		djgpp_bufferstart ++;
		djgpp_bufferstart &= DJGPPKEYBOARDBUFFERSIZE - 1;
	} 
        port_out ( 0x20, 0x20 );        /* Acknowledge the interrupt */
}

int lock_memory(const void *ptr, unsigned long bytes) {
  unsigned long base;

  if(__dpmi_get_segment_base_address(_my_ds(),&base)!=-1) {
	__dpmi_meminfo mem;
	mem.handle=0;
	mem.size=bytes;
	mem.address=(unsigned long)((char *)ptr+base);
	if(__dpmi_lock_linear_region(&mem)!=-1)
	  return 1;
  }

  return -1;
}


#endif

int keyboard_init_return_fd(void)
{

    /* Install default keyboard handler. */
    __keyboard_eventhandler = default_handler;

#ifndef __DJGPP
    if ((kbd_fd != __svgalib_console_fd) && (kbd_fd >= 0))
	close(kbd_fd);

    if (__svgalib_console_fd != -1)
	kbd_fd = __svgalib_console_fd;
    else
	/* svgalib has not already opened the console at */
	/* initialization. */

    if ((kbd_fd = open("/dev/console", O_RDONLY)) < 0) {
	printf("svgalib (keyboard): cannot open /dev/console.\n");
	return -1;
    }

    if (ioctl(kbd_fd, KDGKBMODE, &oldkbmode)) {
	printf("svgalib: cannot get keyboard mode.\n");
	return -1;
    }
    tcgetattr(kbd_fd, &oldkbdtermios);
    newkbdtermios = oldkbdtermios;

    newkbdtermios.c_lflag &= ~(ICANON | ECHO | ISIG);
    newkbdtermios.c_iflag &= ~(ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON);
    newkbdtermios.c_cc[VMIN] = 0;	/* Making these 0 seems to have the */
    newkbdtermios.c_cc[VTIME] = 0;	/* desired effect. */

    tcsetattr(kbd_fd, TCSAFLUSH, &newkbdtermios);

    ioctl(kbd_fd, KDSKBMODE, K_MEDIUMRAW);
#else
	kbd_fd = 1;		/* So that vga.c can verify keyboard has been inited*/

	wrapper.pm_offset=(int)kbdintr;
	wrapper.pm_selector=_my_cs();
	_go32_dpmi_allocate_iret_wrapper(&wrapper);
	newHandler.offset32=wrapper.pm_offset;
	newHandler.selector=wrapper.pm_selector;

        lock_memory(kbdintr,(long)keyboard_init_return_fd-(long)kbdintr);

	__dpmi_get_protected_mode_interrupt_vector(9,&oldHandler);
	__dpmi_get_and_disable_virtual_interrupt_state();
	__dpmi_set_protected_mode_interrupt_vector(9,&newHandler);
	__dpmi_get_and_enable_virtual_interrupt_state();

        __djgpp_nearptr_enable();
#endif

    keyboard_clearstate();

    return kbd_fd;		/* OK, return fd. */
}

/* Old compatible init function. */

int keyboard_init(void)
{
    if (keyboard_init_return_fd() == -1)
	return -1;
    else
	return 0;
}

void keyboard_close(void)
{
    if (kbd_fd < 0)
	return;

#ifndef __DJGPP
    ioctl(kbd_fd, KDSKBMODE, oldkbmode);
    tcsetattr(kbd_fd, 0, &oldkbdtermios);

    if (kbd_fd != __svgalib_console_fd) {
	/* Close the console if it was opened in keyboard_init, */
	/* rather than vga_init. */
	close(kbd_fd);
    }
#else
	__dpmi_get_and_disable_virtual_interrupt_state();
	__dpmi_set_protected_mode_interrupt_vector(9,&oldHandler);
	__dpmi_get_and_enable_virtual_interrupt_state();

	_go32_dpmi_free_iret_wrapper(&wrapper);
#endif

    kbd_fd = -1;
}


/* For now, we assume there's no console switching. */
/* (Actually, there won't be any unless we catch the console switching */
/* keys). */


#define KBDREADBUFFERSIZE 32

static int keyboard_getevents(int wait)
{
/* Read keyboard device, and handle events. */
/* If wait == 1, process at least one event and return. */
/* If wait == 0, handle all accumulated events; return 0 if no events */
/* were handled, 1 otherwise. */
/* Wait mode doesn't seem to work very well; the keyboard repeat delay is */
/* present. I don't understand fcntl. */
    static unsigned char buf[KBDREADBUFFERSIZE];
    static int kfdmode = 0;	/* 1 = DELAY, 0 = NDELAY */
    int bytesread, i;
    int eventhandled;

    eventhandled = 0;

  again:
    if (kfdmode == 1) {		/* This only happens for wait == 1. */
#if 0
	struct termios kbdtermios;
#endif
	int flags;

#ifndef __DJGPP
	/* We don't want to wait, set NDELAY mode. */
	fcntl(kbd_fd, F_GETFL, &flags);
	fcntl(kbd_fd, F_SETFL, flags | O_NDELAY);
#endif

#if 0
	tcgetattr(kbd_fd, &kbdtermios);
	kbdtermios.c_lflag = kbdtermios.c_lflag & ~(ICANON | ECHO | ISIG);
	kbdtermios.c_cc[VMIN] = 0;
	kbdtermios.c_cc[VTIME] = 0;
	tcsetattr(kbd_fd, TCSANOW, &kbdtermios);
#endif

	kfdmode = 0;
    }
#ifndef __DJGPP
    bytesread = read(kbd_fd, buf, KBDREADBUFFERSIZE);
#else
	waiting_loop:
	if ( bytesread = djgpp_bufferend - djgpp_bufferstart ) {
		int i;
		if ( bytesread < 0 ) bytesread += DJGPPKEYBOARDBUFFERSIZE;
		if ( bytesread > KBDREADBUFFERSIZE ) bytesread = KBDREADBUFFERSIZE;
		for ( i = 0; i < bytesread; i ++ ) {
			buf [ i ] = djgpp_buf [ djgpp_bufferstart ++ ];
			djgpp_bufferstart &= DJGPPKEYBOARDBUFFERSIZE - 1;
		}
	} else if ( wait == 1 ) goto waiting_loop; 
#endif

#ifndef __DJGPP
    if (wait == 1 && bytesread < 1) {
#if 0
	struct termios kbdtermios;
#endif
	int flags;
	/* We already handled an event, no need to wait for another. */
	if (eventhandled)
	    return 1;
	/* Wait mode, we'll sleep on reads. */
	fcntl(kbd_fd, F_GETFL, &flags);
	fcntl(kbd_fd, F_SETFL, flags & ~O_NDELAY);

#if 0
	tcgetattr(kbd_fd, &kbdtermios);
	kbdtermios.c_lflag = kbdtermios.c_lflag & ~(ICANON | ECHO | ISIG);
	kbdtermios.c_cc[VMIN] = 0;
	kbdtermios.c_cc[VTIME] = 0;
	tcsetattr(kbd_fd, TCSANOW, &kbdtermios);
#endif

	kfdmode = 1;
	bytesread = read(kbd_fd, buf, 1);
    }
#endif

    if (wait == 0 && bytesread < 1)
	return eventhandled;

    if (bytesread >= 1)
	eventhandled = 1;

    for (i = 0; i < bytesread; i++) {
	/* Check for ctrl-c. */
	if ((buf[i] & 0x7f) == SCANCODE_C)
	    if (buf[i] & 0x80)
		ctrl_c_state_c = 0;
	    else
		ctrl_c_state_c = 1;
	if ((buf[i] & 0x7f) == SCANCODE_LEFTCONTROL
	    || (buf[i] & 0x7f) == SCANCODE_RIGHTCONTROL)
	    if (buf[i] & 0x80)
		ctrl_c_state_control = 0;
	    else
		ctrl_c_state_control = 1;
	if ((buf[i] & 0x7f) == SCANCODE_LEFTALT
	    || (buf[i] & 0x7f) == SCANCODE_RIGHTALT)
	    if (buf[i] & 0x80)
		alt_state = 0;
	    else
		alt_state = 1;
	if ((buf[i] & 0x7f) >= SCANCODE_F1
	    && (buf[i] & 0x7f) <= SCANCODE_F10)
	    if (buf[i] & 0x80)
		functionkey_state &= ~(1 << ((buf[i] & 0x7f)
					     - SCANCODE_F1));
	    else
		functionkey_state |= 1 << ((buf[i] & 0x7f)
					   - SCANCODE_F1);
	if (ctrl_c_state_control && ctrl_c_state_c
	    && !(translatemode & DONT_CATCH_CTRLC))
	    raise(SIGINT);
#ifndef __DJGPP
	if (alt_state && functionkey_state) {
	    /* VT switch. */
	    /* *** what about F11 & F12? */
	    int j, vt = 0;
	    for (j = 0; j < 10; j++)
		if (functionkey_state & (1 << j)) {
		    vt = j + 1;
		    break;
		}
	    /*
	     * This will generate a signal catched by
	     * svgalib to restore textmode.
	     */
	    ioctl(__svgalib_tty_fd, VT_ACTIVATE, vt);
	    return 1;
	}
#endif
	__keyboard_eventhandler(buf[i] & 0x7f,
		    (buf[i] & 0x80) ? KEY_EVENTRELEASE : KEY_EVENTPRESS);
    }

    /* Handle other events that have accumulated. */
    goto again;
}

int keyboard_update(void)
{
    return keyboard_getevents(0);	/* Don't wait. */
}

void keyboard_waitforupdate(void)
{
    keyboard_getevents(1);	/* Wait for event. */
    return;
}

void keyboard_seteventhandler(void (*handler) (int, int))
{
    __keyboard_eventhandler = handler;
}



/* Default event handler. */

void keyboard_setdefaulteventhandler(void)
{
    __keyboard_eventhandler = default_handler;
}

static int checkscancode(int scancode)
{
#ifndef __DJGPP
    if (scancode < 0 || scancode >= NR_KEYS) {
#else
        if ( scancode < 0 || scancode >= 127 ) {
#endif
	printf("svgalib: keyboard scancode out of range (%d).\n",
	       scancode);
	return 1;
    }
    return 0;
}

static void default_handler(int scancode, int newstate)
{
    if (checkscancode(scancode))
	return;
    if (translatemode & TRANSLATE_CURSORKEYS)
	/* Map cursor key block to keypad cursor keys. */
	switch (scancode) {
	case SCANCODE_CURSORBLOCKUP:
	    scancode = SCANCODE_CURSORUP;
	    break;
	case SCANCODE_CURSORBLOCKLEFT:
	    scancode = SCANCODE_CURSORLEFT;
	    break;
	case SCANCODE_CURSORBLOCKRIGHT:
	    scancode = SCANCODE_CURSORRIGHT;
	    break;
	case SCANCODE_CURSORBLOCKDOWN:
	    scancode = SCANCODE_CURSORDOWN;
	    break;
	}
    if (translatemode & TRANSLATE_DIAGONAL) {
	/* Translate diagonal keypad keys to two keypad cursor keys. */
	switch (scancode) {
	case SCANCODE_CURSORUPLEFT:
	    state[SCANCODE_CURSORUP] = newstate;
	    state[SCANCODE_CURSORLEFT] = newstate;
	    return;
	case SCANCODE_CURSORUPRIGHT:
	    state[SCANCODE_CURSORUP] = newstate;
	    state[SCANCODE_CURSORRIGHT] = newstate;
	    return;
	case SCANCODE_CURSORDOWNLEFT:
	    state[SCANCODE_CURSORDOWN] = newstate;
	    state[SCANCODE_CURSORLEFT] = newstate;
	    return;
	case SCANCODE_CURSORDOWNRIGHT:
	    state[SCANCODE_CURSORDOWN] = newstate;
	    state[SCANCODE_CURSORRIGHT] = newstate;
	    return;
	}
    }
    if ((translatemode & TRANSLATE_KEYPADENTER) && scancode ==
	SCANCODE_KEYPADENTER)
	scancode = SCANCODE_ENTER;

#if 0				/* This happens very often. */
    if (state[scancode] == newstate) {
	printf("svgalib: keyboard event does not match (scancode = %d)\n",
	       scancode);
	return;
    }
#endif
    state[scancode] = newstate;
}

void keyboard_clearstate(void)
{
#ifndef __DJGPP
    memset(state, 0, NR_KEYS);
#else
        memset ( state, 0, 127 );
#endif
    ctrl_c_state_c = 0;
    ctrl_c_state_control = 0;
    alt_state = 0;
    functionkey_state = 0;
}

int keyboard_keypressed(int scancode)
{
    if (checkscancode(scancode))
	return 0;
    return state[scancode];
}

char *
 keyboard_getstate(void)
{
    return state;
}

void keyboard_translatekeys(int mode)
{
    translatemode = mode;
}

#endif