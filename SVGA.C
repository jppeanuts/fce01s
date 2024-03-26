/*
 * Svga.c - a Driver for Linux SVGALib in MasterGear
 *
 * Heavily derived from code by Marat Fayzullin, and useless 
 * without the MasterGear sources.
 *
 * Modified BERO for NES emulator
 */

#if( defined(VGA) )

/* Standard and SVGALib includes */

#include <stdio.h>
#include <stdlib.h>
#include "vga.h"
#include "vgakeybo.h"

/* Private includes */

/*
#include "SMS.h"
*/
typedef unsigned char byte;

#ifdef SOUND
#include "SB.h"
#endif

#include "palette.h"

/* Sound related stuff */
#ifdef SOUND
int UseSound = 0;
int SndSwitch = 0x0F;
int SndVolume = 200;
#endif

/* SVGALib variables */

char* ScreenMem = NULL;

/* Miscellaneous variables/functions */

#define WIDTH (256+16)
#define HEIGHT 240

static byte Black = 0;

byte *XBuf;
int Verbose = 1;
int Debug=1;

int SaveCPU = 1;
static int changepal;
static int CPURunning;

void OnBreak(int Arg){ vga_setmode(TEXT); keyboard_close(); CPURunning=0; }

void SetPalette(int n,int c)
{
  	vga_setpalette(n,palette[c].r,palette[c].g,palette[c].b);
}

static int vwidth,vheight;
void PutImage(int X, int Y, int W, int H)
{
/* Hmm... This code requires a Linear Framebuffer. As I don't manually 
 * initialise a linear framebuffer, that means that this code depends on a 
 * 320x200 video mode... I do it this way for speed. 
 */
 byte* dest, *source;
 int i, xcent, ycent;
/*
 extern int SyncScreen;
 if (SyncScreen) WaitVBlank();
 if (changepal) {
  changepal=0;
  SetPalette();
 }
*/
 if (Debug) return;
 //WaitVBlank();
 if (H>vheight) H=vheight;
 dest = ScreenMem + (vheight-H)/2 * vwidth + (vwidth-W)/2;
 source = XBuf + Y * WIDTH + X;
 for( i = 0; i < H ; i++ )
  {
   memcpy(dest, source, W);
   dest += vwidth;
   source += WIDTH;
  }
 /* And there you have it - a fast and buggy update routine */
}

/* TrashMachine - Uninitialise everything */
void TrashMachine(void)
{
 if( Verbose ) printf("Shutting down...\n");
 
 free(XBuf);
 if (!Debug) {
 keyboard_close();
 vga_setmode(TEXT);
 }
#ifdef SOUND
 TrashSound();
#endif 
}

int vmode=0;
/* InitMachine - Starts up all the Unix/SVGA related stuff */
int InitMachine(void)
{
 int i, j;

 if( Verbose ) 
   printf("Initialising Dos/VGA driver:\n Allocating buffers...\n");
 
 XBuf = (byte*) malloc(sizeof(byte) * WIDTH * HEIGHT);
 if( !XBuf ) 
  { 
   if( Verbose )
     printf("FAILED\n");
   return (0); 
  }

 if( Verbose )
   printf("OK\n Initing sound...\n");
 
#ifdef SOUND
 InitSound(UseSound);
 SetChannels(SndVolume,SndSwitch);
 SetSound(3,SND_NOISE);
#endif SOUND
 
// signal(SIGHUP,OnBreak);signal(SIGINT,OnBreak);
// signal(SIGQUIT,OnBreak);signal(SIGTERM,OnBreak);
 
 if (!Debug) {
 vga_init();
 switch(vmode){
 case 1:vga_setmode(G256x240x256);vwidth=256;vheight=240;break;
 default:vga_setmode(G320x200x256);vwidth=320;vheight=200;break;
 }

 ScreenMem = vga_getgraphmem();
 
/*
 if( Verbose ) printf("OK\n Allocating colors...\n");
 for(i=0;i<64;i++)
	SetPalette(i,i);
*/
 keyboard_init();
 keyboard_translatekeys(TRANSLATE_CURSORKEYS | TRANSLATE_DIAGONAL);
 }
 return(1);
}

void ClearGraph(void)
{
 memset(ScreenMem,Black,320*200);
}

/* Josticks - Handles input. Due to differences between SVGAlib and X, this
 * may be a bit bonkers. You have been warned. Due to SVGAlib, and my 
 * laziness, only a select few keys are supported. But anyway, the key 
 * mappings I have dropped are nasty, horrid ones. So don't worry.
 */

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
 static int JS = 0x0000;
 static byte N = 0;
 char* keys;
 if (Debug) return JS;
 if( keyboard_update() == 0 )
   return JS;
 
 JS = 0;
 
 keys = keyboard_getstate();
 
#define KEY(__a) keys[SCANCODE_##__a]
 
 if( KEY(ESCAPE) || KEY(F12) )
   //CPURunning = 0;
   JS|=0x10000;
#if 0 //def DEBUG
 if( KEY(F1) )
   Trace = !Trace;
 if( KEY(F2) )
  {
   int J;
   printf("***** SprTab = %04Xh *****\n",SprTab-VRAM);
   for(J=0;J<64;J++)
     printf("SPRITE#%02d: %d,%d  %02Xh  %02Xh\n",
	    J,SprX(J),SprY(J),SprP(J),SprA(J));
  }
#endif
#if 0 //def SOUND
 if( KEY(1) )
   SetChannels(SndVolume, SndSwitch ^= 0x01);
 if( KEY(2) )
   SetChannels(SndVolume, SndSwitch ^= 0x02);
 if( KEY(3) )
   SetChannels(SndVolume, SndSwitch ^= 0x04);
 if( KEY(4) )
   SetChannels(SndVolume, SndSwitch ^= 0x08);
 if( KEY(0) )
   SetChannels(SndVolume, SndSwitch = (SndSwitch ? 0x00 : 0x0F));
 if( KEY(MINUS) || KEY(KEYPADMINUS) )
  {
   if(SndVolume>0) SndVolume-=10;
   SetChannels(SndVolume,SndSwitch);
  }
 if( KEY(EQUAL) || KEY(KEYPADPLUS) )
  {
   if(SndVolume<250) SndVolume+=10;
   SetChannels(SndVolume,SndSwitch);
  }
#endif
 if( KEY(LEFTCONTROL) || KEY(X))	JS|=JOY_A;
 if( KEY(LEFTALT) || KEY(SPACE) || KEY(Z) ) JS |= JOY_B;
 if( KEY(ENTER) )	JS |= JOY_START;
 if( KEY(TAB) )		JS |= JOY_SELECT;
 if( KEY(CURSORDOWN) )	JS |= JOY_DOWN;
 if( KEY(CURSORUP) )	JS |= JOY_UP;
 if( KEY(CURSORLEFT) )	JS |= JOY_LEFT;
 if( KEY(CURSORRIGHT) )	JS |= JOY_RIGHT;
 return JS;
}


//#include "Common.h"

#endif
