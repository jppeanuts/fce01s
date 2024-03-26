/*
	Mapper routine
*/
typedef unsigned char byte;
typedef unsigned short word;
#include	"fce.h"

#define	DISABLE	0
#define	ENABLE	1
#define	LAST_BANK	ROM_size-1
#define	ROM16_INIT()
#define	VROM8_INIT()
#define	VROM_BANK1(A,V)	VPage[(A)>>10]=&VROM[(V)*0x400]-(A)
#define	VROM_BANK4(A,V)	VPage[(A)>>10]=VPage[((A)>>10)+1]= \
	VPage[((A)>>10)+2]=VPage[((A)>>10)+3]=&VROM[(V)*0x1000]-(A)
#define	VROM_BANK8(A,V)	VPage[0]=VPage[1]=VPage[2]=VPage[3]= \
	VPage[4]=VPage[5]=VPage[6]=VPage[7]=&VROM[(V)*0x2000]
#define	ROM_BANK8(A,V)	Page[(A)>>13]=&ROM[V*0x2000]-(A)
#define	ROM_BANK16(A,V)	Page[(A)>>13]=Page[((A)>>13)+1]=&ROM[(V)*0x4000]-(A)
#define	ROM_BANK32(A,V)	Page[4]=Page[5]=Page[6]=Page[7]=&ROM[(V)*0x8000]-(A)
#define	MIRROR_SET(V)	Mirrorxor = 0x400<<(Mirroring=(V))

void (*MMC_write)(word A,byte V);

static word IRQ_counter;
static byte IRQ_flg,IRQ_ratch;
static int vmask;
static void NONE_init(void)
{
	ROM_BANK16(0x8000,0); /* first 16K */
	ROM_BANK16(0xC000,LAST_BANK); /* last 16K */
	VPage[0]=VPage[1]=VPage[2]=VPage[3]=
	VPage[4]=VPage[5]=VPage[6]=VPage[7]=(VROM_size>0)?VROM:VRAM;
	vmask = VROM_size-1;
}

static void NONE_write(word A,byte V)
{
	printf("cannot ROM write at:%04x,%02x\n",A,V);
}

static void ROM32_init(void)
{
	ROM_BANK32(0x8000,0);
}

/* 1: nintendo MMC1 */
static byte MMC1_reg[4],MMC1_sft,MMC1_buf; //MMC1_sft[4],MMC1_buf[4];
static void MMC1_init(void)
{
	int i;
	for(i=0;i<4;i++) MMC1_reg[i]=0; //MMC1_sft[i]=MMC1_buf[i]=0;
	MMC1_sft = MMC1_buf = 0;
	ROM16_INIT();
}
#define	MIRROR_H	0
#define	MIRROR_V	1
#define	MIRROR_ONE	2

/* not finish */
static void MMC1_write(word A,byte V)
{
	int n=(A>>13)-4;
	if (V&0x80) { MMC1_sft=MMC1_buf=0;MMC1_reg[0]|=0xC; return;}
	MMC1_buf|=(V&1)<<(MMC1_sft++);
  if (MMC1_sft==5) {
	MMC1_reg[n]=V=MMC1_buf;
	MMC1_sft=MMC1_buf=0;
	switch(n){
	case 0:
	/*	if (!(V&2)) Mirroring = MIRROR_ONE;
		else*/ MIRROR_SET(V&1);
	/*	if (!(V&8)) ROM_BANK32(0x8000,MMC1_reg[3]>>1);
		else if (V&4) {
			ROM_BANK16(0x8000,MMC1_reg[3]);
			ROM_BANK16(0xC000,LAST_BANK);
		} else {
			ROM_BANK16(0xC000,MMC1_reg[3]);
			ROM_BANK16(0x8000,0);
		}*/
		break;
	case 1:
		if (MMC1_reg[0]&0x10) VROM_BANK4(0x0000,V);
		else VROM_BANK8(0x0000,V>>1);
		break;
	case 2:
		if (MMC1_reg[0]&0x10) VROM_BANK4(0x1000,V);
		break;
	case 3:
		if ((MMC1_reg[0]&8)) {
			if ((MMC1_reg[0]&4)) ROM_BANK16(0x8000,V);
			else ROM_BANK16(0xC000,V);
		} else {
			ROM_BANK32(0x8000,V>>1);
		}
		break;
	}
  }
}

/* 2:74HC161/74HC32 */
static void iNES2_write(word A,byte V)
{
	//printf("%04x %02x\n",A,V);
	ROM_BANK16(0x8000,V);
}

/* 3:VROM switch */
static void VROMsw_write(word A,byte V)
{
	VROM_BANK8(0x0000,V&vmask);
}

/* 4:nintendo MMC3 */
/* not finish */
static byte MMC3_cmd;
static int MMC3_IRQ(int scanline)
{
	if (IRQ_flg) {
		if (--IRQ_counter==0) return TRUE;
	}
	return FALSE;
}

static void MMC3_init(void)
{
}
static void MMC3_write(word A,byte V)
{
	switch(A){
	case 0x8000:MMC3_cmd = V;break;
	case 0x8001:
		switch(MMC3_cmd&0xC7){
		case 0:V&=0xFE; VROM_BANK1(0x0000,V);VROM_BANK1(0x0400,V+1); break;
		case 1:V&=0xFE; VROM_BANK1(0x0800,V);VROM_BANK1(0x0C00,V+1); break;
		case 2:VROM_BANK1(0x1000,V); break;
		case 3:VROM_BANK1(0x1400,V); break;
		case 4:VROM_BANK1(0x1800,V); break;
		case 5:
			if (MMC3_cmd&0x40) {
				ROM_BANK8(0xC000,V);
				ROM_BANK8(0x8000,(ROM_size-1)*2);
			} else {
				ROM_BANK8(0x8000,V);
				ROM_BANK8(0xC000,(ROM_size-1)*2);
			}
			break;
		case 6:
			if (MMC3_cmd&0x40) {
				ROM_BANK8(0xA000,V);
				ROM_BANK8(0x8000,(ROM_size-1)*2);
			} else {
				ROM_BANK8(0xA000,V);
				ROM_BANK8(0xC000,(ROM_size-1)*2);
			}
			break;
		}
		break;
	case 0xA000:MIRROR_SET(V&1); break;
	case 0xA001:/* ?? */ break;
	case 0xE000:V=IRQ_ratch;
	case 0xC000:IRQ_counter=V;if(V==0) IRQ_flg=DISABLE; break;
	case 0xC001:IRQ_ratch=V;break;
	case 0xE001:IRQ_flg=ENABLE;break;
	}
}

/* 5: nintendo MMC5 */
/* not finish */
static void MMC5_write(word A,byte V)
{
}

/* 6: FFE F4xxx */
/* not finish */
static void FFEF4_write(word A,byte V)
{
	switch(A){
	case 0x42FC:
	case 0x42FD:
	case 0x42EE:
	case 0x42FF:
	case 0x43FE:
	case 0x4500:
	case 0x4501:
	case 0x4502:
	case 0x4503:
	case 0x8000:
	case 0xA000:
	case 0xC000:
	case 0xE000:
	}
}

/* 7: ROM switch  */
static void ROMsw_init(void)
{
	ROM32_init();
}
static void ROMsw_write(word A,byte V)
{
	ROM_BANK32(0x8000,V&0xf);
//??	if ((V&0x10)==0) nametbl=0x2000; else nametbl=0x2400;
}

/* 8: FFE F3xxx */
static void FFEF3_init(void)
{
	ROM_BANK16(0x8000,0);
	ROM_BANK16(0xC000,1);
	VROM8_INIT();
}
static void FFEF3_write(word A,byte V)
{
	ROM_BANK16(0x8000,V>>3);
	VROM_BANK8(0x0000,V&7);
}

/* 9: nintendo MMC2 */
static byte MMC2_latch;
static void MMC2_init(void)
{
	MMC2_latch=0xFE;
	ROM_BANK16(0x8000,LAST_BANK-1);
	ROM_BANK16(0xC000,LAST_BANK);
	ROM_BANK8(0x8000,0);
}
static void MMC2_write(word A,byte V)
{
	switch(A&0xF000){
	case 0xA000:
		ROM_BANK8(0x8000,V);
	case 0xB000:
	case 0xC000:
		VROM_BANK4(0x0000,V);
		break;
	case 0xD000:
		if (MMC2_latch==0xFD) VROM_BANK4(0x1000,V);
		break;
	case 0xE000:
		if (MMC2_latch==0xFE) VROM_BANK4(0x1000,V);
		break;
	case 0xF000:
		MIRROR_SET(V&1);
		break;
	}
}

/* 10: nintendo MMC4 */
static byte MMC4_latch1,MMC4_latch2;
static void MMC4_init(void)
{
	MMC4_latch1 = MMC4_latch2=0xFE;
	ROM16_INIT();
}
static void MMC4_write(word A,byte V)
{
	switch(A&0xF000){
	case 0xA000:
		ROM_BANK16(0x8000,V);
		break;
	case 0xB000:
		if (MMC4_latch1==0xFD) VROM_BANK4(0x0000,V);
		break;
	case 0xC000:
		if (MMC4_latch1==0xFE) VROM_BANK4(0x0000,V);
		break;
	case 0xD000:
		if (MMC4_latch2==0xFD) VROM_BANK4(0x1000,V);
		break;
	case 0xE000:
		if (MMC4_latch2==0xFE) VROM_BANK4(0x1000,V);
		break;
	case 0xF000:
		MIRROR_SET(V&1);
		break;
	}
}


/* 11: Color dreams */
static void iNES11_init(void)
{
	ROM32_init();
	VROM8_INIT();
}
static void iNES11_write(word A,byte V)
{
	ROM_BANK32(0x8000,V&15);
	VROM_BANK8(0x0000,V>>4);
}

/* 15: 100 in 1 */
static void in1_init(void)
{
	ROM32_init();
}
static void in1_write(word A,byte V)
{
	switch(A){
	case 0x8000:
		MIRROR_SET((V>>6)&1);
		V&=0x3F;
		ROM_BANK16(0x8000,V);
		ROM_BANK16(0xC000,V+1);
		// swap ??
		break;
	case 0x8003:
		MIRROR_SET((V>>6)&1);
	case 0x8001:
		if (V&0x80) {
			V&=0x3F;
			ROM_BANK8(0xC000,V*2+1);
			ROM_BANK8(0xE000,V*2);
		} else {
			ROM_BANK16(0xC000,V);
		}
		break;
	case 0x8002:
		//??
		if (V&0x80) {
			//ROM_BANK8();
		} else {
			//ROM_BANK8();
		}
		break;
	}
}

/* 16: bandai */
static void Bandai_init(void)
{
	IRQ_flg = IRQ_counter = 0;
	ROM16_INIT();
}
static void Bandai_write(word A,byte V)
{
  /* A=0x6000..0xFFFF
  	 use 0x600x,0x7FFx,0x800x
  */
	switch(A&0xF){
	case 0x0: VROM_BANK1(0x0000,V);break;
	case 0x1: VROM_BANK1(0x0400,V);break;
	case 0x2: VROM_BANK1(0x0800,V);break;
	case 0x3: VROM_BANK1(0x0C00,V);break;
	case 0x4: VROM_BANK1(0x1000,V);break;
	case 0x5: VROM_BANK1(0x1400,V);break;
	case 0x6: VROM_BANK1(0x1800,V);break;
	case 0x7: VROM_BANK1(0x1C00,V);break;
	case 0x8: ROM_BANK16(0x8000,V);break;
	case 0x9: //mirror; break;
	case 0xA: IRQ_flg=V&1;break;
	case 0xB: IRQ_counter=(IRQ_counter&0xFF00)|V;break;
	case 0xC: IRQ_counter=(IRQ_counter&0xFF)|(V<<8);break;
	case 0xD: /* WRAM read */
	}
}
static byte Bandai_read(word A)
{
	/* WRAM write */
	if (A&15) return 0xFF; /* NODATA */
	return 0xFF;
}


static void Bandai1_write(word A,byte V)
{
	ROM_BANK32(0x8000,(V>>4)&3);
	VROM_BANK8(0x0000,V&3);
}

static void Bandai2_write(word A,byte V)
{
	MIRROR_SET(V>>7);
	ROM_BANK32(0x8000,(V>>4)&7);
	VROM_BANK8(0x0000,V&15);
}

/* 17: FFE F8xxx */
static void FFEF8_init(void)
{
	ROM16_INIT();
}
static void FFEF8_write(word A,byte V)
{
	switch(A){
	case 0x42FC: //??
	case 0x42FD: //??
	case 0x42FE: /* mirror*/; break;
	case 0x42FF: MIRROR_SET((V>>4)&1); break;
	case 0x4501: IRQ_flg=DISABLE;break;
	case 0x4502: IRQ_counter=(IRQ_counter&0xFF00)|V;break;
	case 0x4503: IRQ_counter=(IRQ_counter&0xFF)|(V<<8);IRQ_flg=ENABLE;break;
	case 0x4504: ROM_BANK8(0x8000,V);break;
	case 0x4505: ROM_BANK8(0xA000,V);break;
	case 0x4506: ROM_BANK8(0xC000,V);break;
	case 0x4507: ROM_BANK8(0xE000,V);break;
	case 0x4510: VROM_BANK1(0x0000,V);break;
	case 0x4511: VROM_BANK1(0x0400,V);break;
	case 0x4512: VROM_BANK1(0x0800,V);break;
	case 0x4513: VROM_BANK1(0x0C00,V);break;
	case 0x4514: VROM_BANK1(0x1000,V);break;
	case 0x4515: VROM_BANK1(0x1400,V);break;
	case 0x4516: VROM_BANK1(0x1800,V);break;
	case 0x4517: VROM_BANK1(0x1C00,V);break;
	}
}

#if 0
/* 18: Jaleco */
static byte V0,V1,V2,P0,P1,P2,P3;
static void Jaleco_init(void)
{
	ROM16_INIT();
}
static void Jaleco_write(word A,byte V)
{
	switch(A){
	case 0x8000: V0 = (V0&0xF0)|(V&15); RAM_BANK8(0x8000,V0); break;
	case 0x8001: V0 = (V0&0xF) |(V<<4); RAM_BANK8(0x8000,V0); break;
	case 0x8002: V1 = (V1&0xF0)|(V&15); RAM_BANK8(0xA000,V1); break;
	case 0x8003: V1 = (V1&0xF) |(V<<4); RAM_BANK8(0xA000,V1); break;
	case 0x9000: V2 = (V2&0xF0)|(V&15); RAM_BANK8(0xC000,V2); break;
	case 0x9001: V2 = (V2&0xF) |(V<<4); RAM_BANK8(0xC000,V2); break;
	case 0xA000: P0 = (P0&0xF0)|(V&15); VROM_BANK1(0x0000,P0); break;
	case 0xA001: P0 = (P0&0xF) |(V<<4); VROM_BANK1(0x0000,P0); break;
	case 0xA002: P1 = (P1&0xF0)|(V&15); VROM_BANK1(0x0400,P1); break;
	case 0xA003: P1 = (P1&0xF) |(V<<4); VROM_BANK1(0x0400,P1); break;
	// ‚¢‚Á‚Ï‚¢
	case 0xE000: IRQ_counter=(IRQ_counter&0xFF00)|V;break;
	case 0xE001: IRQ_counter=(IRQ_counter&0xFF)|(V<<8);break;
	}
}

/* 19: Namcot 106 */
static void Namco_init(void)
{
	ROM16_INIT();
	// if (vromsize) VROM_BANK8(0x0000,0);
}
static void Namco_write(word A,byte V)
{
	switch(A&F800){
	case 0x5000: IRQ_counter=(IRQ_counter&0xFF00)|V;break;
	case 0x5800: IRQ_counter=(IRQ_counter&0xFF)|((V&7F)<<8);IRQ_flg=V&1;break;
	case 0x8000: VROM_BANK1(0x0000,V);break;
	case 0x8800: VROM_BANK1(0x0400,V);break;
	case 0x9000: VROM_BANK1(0x0800,V);break;
	case 0x9800: VROM_BANK1(0x0C00,V);break;
	case 0xA000: VROM_BANK1(0x1000,V);break;
	case 0xA800: VROM_BANK1(0x1400,V);break;
	case 0xB000: VROM_BANK1(0x1800,V);break;
	case 0xB800: VROM_BANK1(0x1C00,V);break;
	case 0xC000: VROM_BANK1(0x2000,V);break;
	case 0xC800: VROM_BANK1(0x2400,V);break;
	case 0xD000: VROM_BANK1(0x2800,V);break;
	case 0xD800: VROM_BANK1(0x2C00,V);break;
	case 0xE000: ROM_BANK8(0x8000,V);break;
	case 0xE800: ROM_BANK8(0xA000,V);break;
	case 0xF000: ROM_BANK8(0xC000,V);break;
	}
}

/* 21: Konami VRC4 */

/* 22: Konami VRC2 type A */
static void Konami2A_init(void)
{
	ROM16_INIT();
	VROM8_INIT();
}

static void Konami2A_write(word A,byte V)
{
	switch(A){
	case 0x8000: ROM_BANK8(0x8000,V);break;
	case 0x9000: //mirror
	case 0xA000: ROM_BANK8(0xA000,V);break;
	case 0xB000: VROM_BANK1(0x0000,V>>1);break;
	case 0xB001: VROM_BANK1(0x0400,V>>1);break;
	case 0xC000: VROM_BANK1(0x0800,V>>1);break;
	case 0xC001: VROM_BANK1(0x0C00,V>>1);break;
	case 0xD000: VROM_BANK1(0x1000,V>>1);break;
	case 0xD001: VROM_BANK1(0x1400,V>>1);break;
	case 0xE000: VROM_BANK1(0x1800,V>>1);break;
	case 0xE001: VROM_BANK1(0x1C00,V>>1);break;
	}
}

/* 23: Konami VRC2 type B */

/* 32: Irem */

/* 33: Taito */
static void Taito_write(word A,byte V)
{
	switch(A){
	case 0x8000:ROM_BANK8(0x8000,V&0x1F); MIRROR_SET((V>>6)&1); break;
	case 0x8001:ROM_BANK8(0xA000,V&0x1F); break;
	case 0x8002:V*=2;VROM_BANK1(0x0000,V);VROM_BANK1(0x0400,V); break;
	case 0x8002:V*=2;VROM_BANK1(0x0800,V);VROM_BANK1(0x0C00,V); break;
	case 0xA000:VROM_BANK1(0x1000,V); break;
	case 0xA001:VROM_BANK1(0x1400,V); break;
	case 0xA002:VROM_BANK1(0x1800,V); break;
	case 0xA003:VROM_BANK1(0x1C00,V); break;
	/* type 5 */
	case 0xC000:IRQ_counter = V;break;
	case 0xC001:IRQ_flg = ENABLE;break;
	case 0xC002:IRQ_flg = DISABLE;break;
	case 0xC003:IRQ_counter = V;break;
	case 0xE000:MIRROR_SET(V&1);break;
	}
}
static void Taito2_write(word A,byte V)
{
	MIRROR_SET(V>>7);
	ROM_BANK16(0x8000,(V>>4)&7);
	VROM_BANK8(0x0000,V&15);
}
static void Taito3_write(word A,byte V)
{
	switch(A){
	case 0x7EF0:/*MIRROR_SET(V>>7)*/;
		V=(V*0x7F);VROM_BANK1(0x0000,V);VROM_BANK1(0x0400,V+1);break;
	case 0x7EF1:/*MIRROR_SET(V>>7)*/;
		V=(V*0x7F);VROM_BANK1(0x0800,V);VROM_BANK1(0x0C00,V+1);break;
	case 0x7EF2:VROM_BANK1(0x1000,V);break;
	case 0x7EF3:VROM_BANK1(0x1400,V);break;
	case 0x7EF4:VROM_BANK1(0x1800,V);break;
	case 0x7EF5:VROM_BANK1(0x1C00,V);break;
	case 0x7EFA:
	case 0x7EFB:ROM_BANK8(0x8000,V);break;
	case 0x7EFC:
	case 0x7EFD:ROM_BANK8(0xA000,V);break;
	case 0x7EFC:
	case 0x7EFD:ROM_BANK8(0xC000,V);break;
	}
}

/* 34: iNES #34 */
static void iNES34_write(word A,byte V)
{
	if (A>=0x8000 || A==0x7FFD) {
		ROM_BANK32(0x8000,V);
	} else
	switch(A){
	case 0x7FFE:
		VROM_BANK4(0x0000,V);
		break;
	case 0x7FFF:
		VROM_BANK4(0x1000,V);
		break;
	}
}
#endif

int MMC_init(int type)
{
	NONE_init();
	MMC_write = NONE_write;
	IRQ_flg = 0;
	IRQ_counter = 0;
	switch(type){
	case 0: break;
	case  1: MMC1_init(); MMC_write = MMC1_write; break;
	case  2: MMC_write = iNES2_write; break;
	case  3: MMC_write = VROMsw_write; break;
	case  4: MMC_write = MMC3_write; break;
	case  5: MMC_write = MMC5_write; break;
	case  6: MMC_write = FFEF4_write; break;
	case  7: ROM32_init(); MMC_write = ROMsw_write; break;
	case  8: FFEF3_init();  MMC_write = FFEF3_write; break;
	case  9: MMC2_init();  MMC_write = MMC2_write; break;
	case 10: MMC4_init();  MMC_write = MMC4_write; break;
	case 11: ROM32_init(); MMC_write = iNES11_write; break;
	case 15: ROM32_init(); MMC_write = in1_write; break;
	case 16: MMC_write = Bandai_write; break;
	case 17: MMC_write = FFEF8_write; break;
#if 0
	case 18: MMC_write = Jaleco_write; break;
	case 19: MMC_write = Namco_write; break;
	case 21: MMC_write = Konami4_write; break;
	case 22: MMC_write = Konami2A_write; break;
	case 23: MMC_write = Konami2B_write; break;
	case 32: MMC_write = Irem_write; break;
	case 33: MMC_write = Taito_write; break;
	case 34: MMC_write = iNES34_write; break;
#endif
	default:
		/*  */
		printf("Not Support Mapper %d\n",type);
		return -1;
	}
	return 0;
}