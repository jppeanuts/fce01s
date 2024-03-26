#ifndef _VGA_H_

#define	TEXT	3
#define	G320x200x256	0x13
#define	G256x240x256	0x14
#define	G256x200x256	0x15

    extern int vga_setmode(int mode);
    extern int vga_setpalette(int index, int red, int green, int blue);
    extern unsigned char *vga_getgraphmem(void);
    extern int vga_init(void);	/* Used to return void in svgalib <= 1.12. */

#endif
