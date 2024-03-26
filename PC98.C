#include	<stdio.h>
#include	<stdlib.h>
#include	<limits.h>
#include	<dos.h>
#include	<pc.h>
#include	<sys/nearptr.h>

#ifdef SOUND
#include "SB.h"
#endif

#include	"palette.h"

#ifdef SOUND
int UseSound = 0;
int SndSwitch = 0x0F;
int SndVolume = 200;
#endif

static unsigned char *ScreenMem,*lowmem;
unsigned char *XBuf;
int Verbose = 1;
int Debug=1;
int vmode=0;
static int pc9821,mode21,scan31,height21;

int InitMachine(void);
void TrashMachine(void);
void PutImage(int X, int Y, int W, int H);
void SetPalette(int n,int c);
int JoySticks(void);

	static unsigned long cvtbl[32]={
		0x00000000,0x00000003,0x00000300,0x00000303,
		0x00030000,0x00030003,0x00030300,0x00030303,
		0x03000000,0x03000003,0x03000300,0x03000303,
		0x03030000,0x03030003,0x03030300,0x03030303,
		0x00000000,0x00000003,0x00000300,0x00000303,
		0x00030000,0x00030003,0x00030300,0x00030303,
		0x03000000,0x03000003,0x03000300,0x03000303,
		0x03030000,0x03030003,0x03030300,0x03030303,
	};

static pal nowpal[32];
#define	MUL2(a)	(a)*(a)

void SetPalette(int n,int c)
{
	if (pc9821) {
		outp(0xa8,n);
		outp(0xaa,palette[c].g<<2);
		outp(0xac,palette[c].r<<2);
		outp(0xae,palette[c].b<<2);
	} else {
		nowpal[n]=palette[c];
		if (n>=16) return;
		outp(0xa8,n);
		outp(0xaa,palette[c].g>>2);
		outp(0xac,palette[c].r>>2);
		outp(0xae,palette[c].b>>2);
	}
}


static void setconvpal(void)
{
	int i,j,c,d,m;
	pal *c1,*c2;
	for(i=16,c1=&nowpal[16];i<32;i++,c1++) {
		if (i&3) {
			/* 一番近い色を探す */
			m = INT_MAX;
			for(j=0,c2=&nowpal[0];j<16;j++,c2++) {
				d = MUL2(c1->r-c2->r)+MUL2(c1->g-c2->g)+MUL2(c1->b-c2->b);
				if (m>d) {
					m=d;c=j;
				}
			}
			/* 一番近い色に置換 */
			cvtbl[i]=cvtbl[c];
		}
	}
}

static void convput(unsigned char *vrm,unsigned char *p,int w)
{
	int i,c;

	for(i=0;i<w/4;i++){
		c  = (cvtbl[p[0]]<<6)|(cvtbl[p[1]]<<4)|(cvtbl[p[2]]<<2)|(cvtbl[p[3]]);
		vrm[0x00000] = c;		/* B plane */
		vrm[0x08000] = c>>8;	/* R plane */
		vrm[0x10000] = c>>16;	/* G plane */
		vrm[0x38000] = c>>24;	/* I plane */
		p+=4;
		vrm++;
	}
}

#define WIDTH (256+16)
#define HEIGHT 240
#define	VHEIGHT	200
#define	VWIDTH	80
#define	VW21	640

static void convput21(unsigned char *vrm,unsigned char *p,int w)
{
	int i,c;
	for(i=0;i<w;i++) {
		vrm[0]=vrm[1]=vrm[VW21]=vrm[VW21+1]=p[0];
		p++;
		vrm+=2;
	}
}

void PutImage(int X,int Y,int W,int H)
{
	int i;
	unsigned char *dest,*src;

	if (H>VHEIGHT && height21!=480) {
		Y+=(H-VHEIGHT)/2;
		H=VHEIGHT;
	}
	if (pc9821) {
		int ofs = (height21/2-H)/2*(VW21*2) + (VW21/2-W);
		src = XBuf + Y* WIDTH + X;
		for(i=0;i<H;i++) {
#if 1
			*(short*)(&lowmem[0xe0004])=ofs>>15;
			*(short*)(&lowmem[0xe0006])=(ofs>>15)+1;
			dest = ScreenMem+(ofs&0x7fff);
			convput21(dest,src,W);
#endif
			ofs+=VW21*2;
			src+=WIDTH;
		}
	} else {
		setconvpal();
		dest = ScreenMem+(VHEIGHT-H)/2*VWIDTH + (VWIDTH*4-W)/(2*4);
		src = XBuf + Y* WIDTH + X;
		for(i=0;i<H;i++) {
			convput(dest,src,W);
			dest+=VWIDTH;
			src+=WIDTH;
		}
	}
}

int InitMachine(void)
{
	union REGS r;

	if (__djgpp_nearptr_enable()==0) return 0;
	lowmem = (unsigned char*)__djgpp_conventional_base;
	ScreenMem = lowmem+0xa8000;

	if( Verbose ) 
		printf("Initialising PC-9801/9821 driver:\n Allocating buffers...\n");

	XBuf = malloc(sizeof(char) * WIDTH * HEIGHT);
	if( !XBuf ) {
		if( Verbose ) printf("FAILED\n");
		return (0); 
	}

#ifdef SOUND
	InitSound(UseSound);
	SetChannels(SndVolume,SndSwitch);
	SetSound(3,SND_NOISE);
#endif SOUND

	if (lowmem[0x45c]&0x40) pc9821=1;
	r.h.ah = 0xd;	/* text off */
	int86(0x18,&r,&r);

	//pc9821 = 0;
	if (pc9821) {
		int i;
#if 0
		r.h.ah = 0x42;
		r.h.ch = 0xc0;
		int86(0x18,&r,&r);
		outp(0x6a,7); /* 変更可 */
		outp(0x6a,0x21); /* 256色モード */
		outp(0x6a,6); /* 変更不可 */
#else
		/* get previous mode */
		r.h.ah = 0x31;
		int86(0x18,&r,&r);
		mode21 = r.h.bh;
		scan31 = r.h.al&4;

		/* set scan,height */
		r.h.ah = 0x30;
		r.h.al |= 8;
		if (scan31) {
			height21 = 480;
			r.h.bh = (r.h.bh&0xf)|0x30;
		} else {
			height21 = 400;
			r.h.bh = (r.h.bh&0xf)|0x20;
		}
		/* set mode */
		int86(0x18,&r,&r);
		r.h.ah = 0x4d;
		r.h.ch = 1;
		int86(0x18,&r,&r);
#endif
		lowmem[0xe0100] = 0; /* packed pixel */
		for(i=0;i<15;i++) {
			*(short*)(&lowmem[0xe0004])=i;
			memset(ScreenMem,0,0x8000);
		}
	} else {
		r.h.ah = 0x42;
		r.h.ch = 0x80;
		int86(0x18,&r,&r); /* graph 640x200 */

		outp(0x68,8);	/* 200ラインの隙間うめる */
		outp(0x6a,1); /* 16 color */
		outp(0xa4,0); /* display page */
		outp(0xa6,0); /* draw page */

		memset(ScreenMem,0,0x18000); /* B,R,G plane clear */
		memset(ScreenMem+0x38000,0,0x8000); /* I plane clear */
	}

	r.h.ah = 0x40;
	int86(0x18,&r,&r); /* graph on */
	lowmem[0x500]|=0x20; /* key repeat off */
	return 1;
}

void TrashMachine(void)
{
	union REGS r;

	if( Verbose ) printf("Shutting down...\n");
	if (XBuf) free(XBuf);
#ifdef SOUND
	TrashSound();
#endif 

	lowmem[0x500]&=~0x20; /* key repeat on */
	__djgpp_nearptr_disable();

	r.h.ah = 0x41;
	int86(0x18,&r,&r); /* graph off */
	outp(0xa8,0);
	outp(0xaa,0);
	outp(0xac,0);
	outp(0xae,0);

	if (pc9821) {
#if 0
		outp(0x6a,7); /* 変更可 */
		outp(0x6a,0x20); /* 16色モード */
		outp(0x6a,6); /* 変更不可 */
#else
		/* 16色モード */
		r.h.ah = 0x4d;
		r.h.ch = 0x0;
		int86(0x18,&r,&r);
		/* 以前のモードに戻す */
		r.h.ah = 0x30;
		r.h.al = scan31|8;
		r.h.bh = mode21;
		int86(0x18,&r,&r);
#endif
	} else {
		r.h.ah = 0x42;
		r.h.ch = 0xc0;
		int86(0x18,&r,&r); /* graph 640x400 */
	}
	r.h.ah = 0xc;	/* text on */
	int86(0x18,&r,&r);
}

#define	JOY_A	1
#define	JOY_B	2
#define	JOY_SELECT	4
#define	JOY_START	8
#define	JOY_UP	0x10
#define	JOY_DOWN	0x20
#define	JOY_LEFT	0x40
#define	JOY_RIGHT	0x80

int Joysticks(void)
{
	int JS;
	char *keys=lowmem+0x52a;

#define	KEY(__a)	(keys[SCANCODE_##__a>>3]&(1<<(SCANCODE_##__a&7)))

#define	SCANCODE_ESCAPE	0x00
#define	SCANCODE_F10	0x6b
#define	SCANCODE_LEFTCONTROL	0x74
#define	SCANCODE_LEFTALT	0x73
#define	SCANCODE_SPACE	0x34
#define	SCANCODE_ENTER	0x1c
#define	SCANCODE_TAB	0x0f
#define	SCANCODE_CURSORDOWN	0x3d
#define	SCANCODE_CURSORUP	0x3a
#define	SCANCODE_CURSORLEFT	0x3b
#define	SCANCODE_CURSORRIGHT	0x3c
#define	SCANCODE_Z	0x29
#define	SCANCODE_X	0x2a

	JS = 0x0000;

	if( KEY(ESCAPE) || KEY(F10) )	JS|=0x10000;
	if( KEY(LEFTCONTROL) || KEY(X))	JS|=JOY_A;
	if( KEY(LEFTALT) || KEY(SPACE) || KEY(Z) ) JS |= JOY_B;
	if( KEY(ENTER) )	JS |= JOY_START;
	if( KEY(TAB) )		JS |= JOY_SELECT;
	if( KEY(CURSORDOWN) )	JS |= JOY_DOWN;
	if( KEY(CURSORUP) )	JS |= JOY_UP;
	if( KEY(CURSORLEFT) )	JS |= JOY_LEFT;
	if( KEY(CURSORRIGHT) )	JS |= JOY_RIGHT;
	return JS;
}