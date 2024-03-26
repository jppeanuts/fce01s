/*
	VGA access on DJGPP
	bero
*/

#ifdef VGA

#include	<dpmi.h>
#include	<sys/nearptr.h>
#include	"vga.h"

static unsigned char *vga_graphmem;
int SyncScreen;

void WaitVBlank(void)
{
  while(inp(0x3DA)&0x08);
  while(!(inp(0x3DA)&0x08));
}

int vga_init(void)
{
	if (__djgpp_nearptr_enable()==0) return -1;
	vga_graphmem = (unsigned char*)(0xa0000 + __djgpp_conventional_base);
	return 0;
}

unsigned char *vga_getgraphmem(void)
{
	return vga_graphmem;
}

int vga_setmode(int mode)
{
  /* 256x240 */
  static unsigned char Regs1[25] =
  {
    0x4F,0x3F,0x40,0x92, //水平キャラ、水平表示キャラ-1.非表示,非表示終了
    0x44,0x10,0x0A,0x3E, //水平帰線、水平帰線終了、垂直ライン、垂直ライン上位
    0x00,0x41,0x00,0x00, //スクロール、１キャラライン、カーソル、カーソル
    0x00,0x00,0x00,0x00, //表示アドレス、表示アドレス、カーソル、カーソル
    0xEA,0xAC,0xDF,0x20, //垂直帰線終了、表示ライン数、１ライン幅、下線
    0x40,0xE7,0x06,0xA3,0xE3  //垂直非表示、垂直非表示終了、モード、ラインコンペア
  };
  /* 256x200 */
  static unsigned char Regs2[25]=
  {
    0x4F,0x3F,0x40,0x92,
    0x44,0x10,0xBF,0x1F,
    0x00,0x41,0x00,0x00,
    0x00,0x08,0x00,0x00,
    0x9C,0x8E,0x8F,0x28,
    0x40,0x96,0xB9,0xA3,0x63
  };

	__dpmi_regs regs;
	memset(&regs,0,sizeof(regs));
	if (mode>G320x200x256) regs.x.ax = 0x13;
	else regs.x.ax = mode;
	__dpmi_int(0x10,&regs);


	if (mode==G256x200x256 || mode==G256x240x256) {
		unsigned char *Regs;
		int i;
		if (mode==G256x240x256) Regs = Regs1;
		else if (mode==G256x240x256) Regs = Regs2;
#if 0
		for(i=0;i<24;i++) {
			outp(0x3d4,i);
			printf("%02x,",inp(0x3d5));
		}
		printf("\n");
#endif
	  WaitVBlank();              /* Wait for VBlank       */
	  outpw(0x3C4,0x100);        /* Sequencer reset       */
//	  outp(0x3C2,0xE3);          /* Misc. output register */
	  outp(0x3C2,Regs[24]);          /* Misc. output register */
	  outpw(0x3C4,0x300);        /* Clear sequencer reset */

	  /* Unprotect registers 0-7 */
	  outpw(0x3D4,((Regs[0x11]&0x7F)<<8)+0x11);
	  for(i=0;i<24;i++) outpw(0x3D4,(Regs[i]<<8)+i);
	  memset(vga_graphmem,0,256*240);
#if 0
		for(i=0;i<24;i++) {
			outp(0x3d4,i);
			printf("%02x,",inp(0x3d5));
		}
		printf("\n");
#endif
	}
	return 0;
}

int vga_setpalette(int index, int red, int green, int blue)
{
	outp(0x3c8,index);
	outp(0x3c9,red);
	outp(0x3c9,green);
	outp(0x3c9,blue);
	return 0;
}

#endif