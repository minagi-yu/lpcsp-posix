/*----------------------------------------------------------------------------/
/  LPCSP for POSIX - Flash Programmer for LPC8xx/1xxx/2xxx/4xxx  R0.05
/  http://elm-chan.org/
/  https://mngu.net/
/-----------------------------------------------------------------------------/
/ LPCSP is a flash programming software for NXP LPC8xx/1xxx/2xxx/4xxx family MCUs.
/ It is a Free Software opened under license policy of GNU GPL.

   Copyright (C) 2018, ChaN, minagi, all right reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

   * Neither the name of the copyright holders nor the names of
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE. 
*/



#include <stdio.h>
#include <string.h>
// #include <windows.h>

#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>


#define INIFILE "lpcsp.ini"
#define MESS(str) fputs(str, stderr)
#define LD_DWORD(ptr) (uint32_t)(((uint32_t)*((uint8_t*)(ptr)+3)<<24)|((uint32_t)*((uint8_t*)(ptr)+2)<<16)|((uint16_t)*((uint8_t*)(ptr)+1)<<8)|*(uint8_t*)(ptr))
#define ST_DWORD(ptr,val) *(uint8_t*)(ptr)=(uint8_t)(val); *((uint8_t*)(ptr)+1)=(uint8_t)((uint16_t)(val)>>8); *((uint8_t*)(ptr)+2)=(uint8_t)((uint32_t)(val)>>16); *((uint8_t*)(ptr)+3)=(uint8_t)((uint32_t)(val)>>24)
#define	SZ_CODE 88


typedef struct {
	const char* DeviceName;	/* Device name LPC<string> */
	uint32_t Sign;				/* Device signature value */
	const uint8_t* Code;		/* Local code to read flash memory */
	uint32_t RawMode;			/* UU-Encode(0) or Raw(1) for data transfer */
	uint32_t FlashSize;		/* User flash memory size */
	const uint32_t* SectMap;	/* Flash sector organization */
	uint32_t XferAddr;			/* Data write buffer address */
	uint32_t XferSize;			/* Data transfer size of a write transaction */
	uint32_t CRP;				/* CRP address */
	uint32_t Sum;				/* Application check sum address */
} DEVICE;


const char *Usage =
	"LPCSP - LPC8xx/1xxx/2xxx/4xxx Serial Programming tool R0.05 (C)ChaN,2018\n"
	"\n"
	"Write flash memory:    <hex file> [<hex file>] ...\n"
	"Read flash memory:     -R\n"
	"Port number and speed: -P<n>[:<bps>]\n"
	"Oscillator frequency:  -F<n> (used for only LPC21xx/22xx)\n"
	"Do not block CRP3:     -3\n"
	"Signal polarity:       -C<flag> (see lpcsp.ini)\n"
	"Wait on exit:          -W<mode> (see lpcsp.ini)\n"
	"\n"
	"Supported Device:\n"
	"LPC8(02,10,11,12,22,24,32,34,44,45)\n"
	"LPC11(02,10,11,12,13,14,15)\n"
	"LPC11C(12,14,22,24)\n"
	"LPC11A(02,04,11,12,13,14)\n"
	"LPC11E(11,12,13,14,36,37)\n"
	"LPC12(24,25,26,27)\n"
	"LPC13(11,13,15,16,17,42,43,45,46,47)\n"
	"LPC15(17,18,19,47,48,49)\n"
	"LPC17(51,52,54,56,58,59,64,65,66,67,68,69,74,76,77,78,85,86,87,88)\n"
	"LPC21(03,04,05,06,09,19,29,14,24,94,31,32,34,36,38,41,42,44,46,48,94)\n"
	"LPC22(92,94)\n"
	"LPC23(64,65,66,67,68,77,78,87,88)\n"
	"LPC24(58,68,78)\n"
	"LPC40(72,74,76,78,88)\n"
	;



/* Flash sector organizations */
const uint32_t Map1[] = { 0x0000, 0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000, 0x8000, 0x10000, 0x18000, 0x20000, 0x28000, 0x30000, 0x38000, 0x40000, 0x48000, 0x50000, 0x58000, 0x60000, 0x68000, 0x70000, 0x78000, 0x79000, 0x7A000, 0x7B000, 0x7C000, 0x7D000, 0x7E000, 0x7F000, 0x80000 };
const uint32_t Map2[] = { 0x0000, 0x2000, 0x4000, 0x6000, 0x8000, 0xA000, 0xC000, 0xE000, 0x10000, 0x12000, 0x14000, 0x16000, 0x18000, 0x1A000, 0x1C000, 0x1E000, 0x20000 };
const uint32_t Map3[] = { 0x0000, 0x2000, 0x4000, 0x6000, 0x8000, 0xA000, 0xC000, 0xE000, 0x10000, 0x20000, 0x30000, 0x32000, 0x34000, 0x36000, 0x38000, 0x3A000, 0x3C000, 0x3E000, 0x40000 };
const uint32_t Map4[] = { 0x0000, 0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000, 0x8000, 0x9000, 0xA000, 0xB000, 0xC000, 0xD000, 0xE000, 0xF000, 0x10000, 0x18000, 0x20000, 0x28000, 0x30000, 0x38000, 0x40000, 0x48000, 0x50000, 0x58000, 0x60000, 0x68000, 0x70000, 0x78000, 0x80000 };
const uint32_t Map5[] = { 0x0000, 0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000, 0x8000, 0x9000, 0xA000, 0xB000, 0xC000, 0xD000, 0xE000, 0xF000, 0x10000, 0x11000, 0x12000, 0x13000, 0x14000, 0x15000, 0x16000, 0x17000, 0x18000, 0x19000, 0x1A000, 0x1B000, 0x1C000, 0x1D000, 0x1E000, 0x1F000, 0x20000, 0x21000, 0x22000, 0x23000, 0x24000, 0x25000, 0x26000, 0x27000, 0x28000, 0x29000, 0x2A000, 0x2B000, 0x2C000, 0x2D000, 0x2E000, 0x2F000, 0x30000, 0x31000, 0x32000, 0x33000, 0x34000, 0x35000, 0x36000, 0x37000, 0x38000, 0x39000, 0x3A000, 0x3B000, 0x3C000, 0x3D000, 0x3E000, 0x3F000, 0x40000 };
const uint32_t Map6[] = { 0x0000, 0x0400, 0x0800, 0x0C00, 0x1000, 0x1400, 0x1800, 0x1C00, 0x2000, 0x2400, 0x2800, 0x2C00, 0x3000, 0x3400, 0x3800, 0x3C00, 0x4000, 0x4400, 0x4800, 0x4C00, 0x5000, 0x5400, 0x5800, 0x5C00, 0x6000, 0x6400, 0x6800, 0x6C00, 0x7000, 0x7400, 0x7800, 0x7C00, 0x8000 };

/* Flash read code with remap disabled */
const uint8_t Code2000[SZ_CODE] = {
	0x12, 0x4d, 0x01, 0x20, 0x28, 0x60, 0x12, 0x4d, 0x00, 0x23, 0x69, 0x69, 0x01, 0x26, 0x31, 0x42,
	0xfb, 0xd0, 0x28, 0x68, 0xff, 0x26, 0x30, 0x40, 0xaa, 0x28, 0xf6, 0xd1, 0x0d, 0x4a, 0x00, 0x24,
	0x18, 0x78, 0x01, 0x33, 0x24, 0x18, 0x00, 0xf0, 0x0a, 0xf8, 0x01, 0x3a, 0xf8, 0xd1, 0x20, 0x00,
	0x00, 0xf0, 0x05, 0xf8, 0x08, 0x24, 0xe0, 0x40, 0x00, 0xf0, 0x01, 0xf8, 0xe5, 0xe7, 0x69, 0x69,
	0x20, 0x26, 0x31, 0x42, 0xfb, 0xd0, 0x28, 0x60, 0x70, 0x47, 0x00, 0x00, 0x40, 0xc0, 0x1f, 0xe0,
	0x00, 0xc0, 0x00, 0xe0, 0x00, 0x04, 0x00, 0x00
};
const uint8_t Code1700[SZ_CODE] = {
	0x12, 0x4d, 0x01, 0x20, 0x28, 0x60, 0x00, 0x23, 0x11, 0x4d, 0x69, 0x69, 0x01, 0x26, 0x31, 0x42,
	0xfb, 0xd0, 0x28, 0x68, 0xff, 0x26, 0x30, 0x40, 0xaa, 0x28, 0xf6, 0xd1, 0x0d, 0x4a, 0x00, 0x24,
	0x18, 0x78, 0x01, 0x33, 0x24, 0x18, 0x00, 0xf0, 0x0a, 0xf8, 0x01, 0x3a, 0xf8, 0xd1, 0x20, 0x00,
	0x00, 0xf0, 0x05, 0xf8, 0x08, 0x24, 0xe0, 0x40, 0x00, 0xf0, 0x01, 0xf8, 0xe5, 0xe7, 0x69, 0x69,
	0x20, 0x26, 0x31, 0x42, 0xfb, 0xd0, 0x28, 0x60, 0x70, 0x47, 0x00, 0x00, 0x40, 0xc0, 0x0f, 0x40,
	0x00, 0xc0, 0x00, 0x40, 0x00, 0x04, 0x00, 0x00
};
const uint8_t Code1100[SZ_CODE] = {
	0x12, 0x4d, 0x02, 0x20, 0x28, 0x60, 0x12, 0x4d, 0x00, 0x23, 0x69, 0x69, 0x01, 0x26, 0x31, 0x42,
	0xfb, 0xd0, 0x28, 0x68, 0xff, 0x26, 0x30, 0x40, 0xaa, 0x28, 0xf6, 0xd1, 0x0d, 0x4a, 0x00, 0x24,
	0x18, 0x78, 0x01, 0x33, 0x24, 0x18, 0x00, 0xf0, 0x0a, 0xf8, 0x01, 0x3a, 0xf8, 0xd1, 0x20, 0x00,
	0x00, 0xf0, 0x05, 0xf8, 0x08, 0x24, 0xe0, 0x40, 0x00, 0xf0, 0x01, 0xf8, 0xe5, 0xe7, 0x69, 0x69,
	0x20, 0x26, 0x31, 0x42, 0xfb, 0xd0, 0x28, 0x60, 0x70, 0x47, 0x00, 0x00, 0x00, 0x80, 0x04, 0x40,
	0x00, 0x80, 0x00, 0x40, 0x00, 0x04, 0x00, 0x00
};
const uint8_t Code800[SZ_CODE] = {
	0x12, 0x4d, 0x02, 0x20, 0x28, 0x60, 0x12, 0x4d, 0x00, 0x23, 0xa9, 0x68, 0x01, 0x26, 0x31, 0x42,
	0xfb, 0xd0, 0x68, 0x69, 0xff, 0x26, 0x30, 0x40, 0xaa, 0x28, 0xf6, 0xd1, 0x0d, 0x4a, 0x00, 0x24,
	0x18, 0x78, 0x01, 0x33, 0x24, 0x18, 0x00, 0xf0, 0x0a, 0xf8, 0x01, 0x3a, 0xf8, 0xd1, 0x20, 0x00,
	0x00, 0xf0, 0x05, 0xf8, 0x08, 0x24, 0xe0, 0x40, 0x00, 0xf0, 0x01, 0xf8, 0xe5, 0xe7, 0xa9, 0x68,
	0x04, 0x26, 0x31, 0x42, 0xfb, 0xd0, 0xe8, 0x61, 0x70, 0x47, 0x00, 0x00, 0x00, 0x80, 0x04, 0x40,
	0x00, 0x40, 0x06, 0x40, 0x00, 0x04, 0x00, 0x00
};
const uint8_t Code1500[SZ_CODE] = {
	0x12, 0x4d, 0x02, 0x20, 0x28, 0x60, 0x12, 0x4d, 0x00, 0x23, 0xa9, 0x68, 0x01, 0x26, 0x31, 0x42,
	0xfb, 0xd0, 0x68, 0x69, 0xff, 0x26, 0x30, 0x40, 0xaa, 0x28, 0xf6, 0xd1, 0x0d, 0x4a, 0x00, 0x24,
	0x18, 0x78, 0x01, 0x33, 0x24, 0x18, 0x00, 0xf0, 0x0a, 0xf8, 0x01, 0x3a, 0xf8, 0xd1, 0x20, 0x00,
	0x00, 0xf0, 0x05, 0xf8, 0x08, 0x24, 0xe0, 0x40, 0x00, 0xf0, 0x01, 0xf8, 0xe5, 0xe7, 0xa9, 0x68,
	0x04, 0x26, 0x31, 0x42, 0xfb, 0xd0, 0xe8, 0x61, 0x70, 0x47, 0x00, 0x00, 0x00, 0x40, 0x07, 0x40,
	0x00, 0x00, 0x04, 0x40, 0x00, 0x04, 0x00, 0x00
};

/* Device properties */
const DEVICE DevLst[] = {
/*	 *Device        Sign     Code     Raw  Flash   *Map   Buff         Xfer   CRP   Sum */
	{ "802",     0x00008021,  Code800, 1,  0x3F80, Map6, 0x10000380,   0x80, 0x2FC, 0x1C },
	{ "802",     0x00008022,  Code800, 1,  0x3F80, Map6, 0x10000380,   0x80, 0x2FC, 0x1C },
	{ "802",     0x00008023,  Code800, 1,  0x3F80, Map6, 0x10000380,   0x80, 0x2FC, 0x1C },
	{ "802",     0x00008024,  Code800, 1,  0x3F80, Map6, 0x10000380,   0x80, 0x2FC, 0x1C },
	{ "810",     0x00008100,  Code800, 1,  0x1000, Map6, 0x10000300,  0x100, 0x2FC, 0x1C },
	{ "811",     0x00008110,  Code800, 1,  0x2000, Map6, 0x10000300,  0x100, 0x2FC, 0x1C },
	{ "812",     0x00008120,  Code800, 1,  0x4000, Map6, 0x10000300,  0x400, 0x2FC, 0x1C },
	{ "812",     0x00008121,  Code800, 1,  0x4000, Map6, 0x10000300,  0x400, 0x2FC, 0x1C },
	{ "822",     0x00008221,  Code800, 1,  0x4000, Map6, 0x10000300,  0x400, 0x2FC, 0x1C },
	{ "822",     0x00008222,  Code800, 1,  0x4000, Map6, 0x10000300,  0x400, 0x2FC, 0x1C },
	{ "824",     0x00008241,  Code800, 1,  0x8000, Map6, 0x10000300,  0x400, 0x2FC, 0x1C },
	{ "824",     0x00008242,  Code800, 1,  0x8000, Map6, 0x10000300,  0x400, 0x2FC, 0x1C },
	{ "832",     0x00008322,  Code800, 1,  0x8000, Map6, 0x10000300,  0x400, 0x2FC, 0x1C },
	{ "834",     0x00008341,  Code800, 1,  0x8000, Map6, 0x10000300,  0x400, 0x2FC, 0x1C },
	{ "844",     0x00008441,  Code800, 1, 0x10000, Map6, 0x10000600,  0x400, 0x2FC, 0x1C },
	{ "844",     0x00008442,  Code800, 1, 0x10000, Map6, 0x10000600,  0x400, 0x2FC, 0x1C },
	{ "844",     0x00008444,  Code800, 1, 0x10000, Map6, 0x10000600,  0x400, 0x2FC, 0x1C },
	{ "845",     0x00008451,  Code800, 1, 0x10000, Map6, 0x10000600,  0x400, 0x2FC, 0x1C },
	{ "845",     0x00008452,  Code800, 1, 0x10000, Map6, 0x10000600,  0x400, 0x2FC, 0x1C },
	{ "845",     0x00008453,  Code800, 1, 0x10000, Map6, 0x10000600,  0x400, 0x2FC, 0x1C },
	{ "845",     0x00008454,  Code800, 1, 0x10000, Map6, 0x10000600,  0x400, 0x2FC, 0x1C },

	{ "1102",    0x2500102B, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1110",    0x0A07102B, Code1100, 0,  0x1000, Map5, 0x10000200,  0x100, 0x2FC, 0x1C },
	{ "1110",    0x1A07102B, Code1100, 0,  0x1000, Map5, 0x10000200,  0x100, 0x2FC, 0x1C },
	{ "1111",    0x0A16D02B, Code1100, 0,  0x2000, Map5, 0x10000200,  0x100, 0x2FC, 0x1C },
	{ "1111",    0x1A16D02B, Code1100, 0,  0x2000, Map5, 0x10000200,  0x100, 0x2FC, 0x1C },
	{ "1111",    0x041E502B, Code1100, 0,  0x2000, Map5, 0x10000200,  0x100, 0x2FC, 0x1C },
	{ "1111",    0x2516D02B, Code1100, 0,  0x2000, Map5, 0x10000200,  0x100, 0x2FC, 0x1C },
	{ "1111",    0x0416502B, Code1100, 0,  0x2000, Map5, 0x10000200,  0x100, 0x2FC, 0x1C },
	{ "1111",    0x2516902B, Code1100, 0,  0x2000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1111",    0x00010013, Code1100, 0,  0x2000, Map5, 0x10000200,  0x100, 0x2FC, 0x1C },
	{ "1111",    0x00010012, Code1100, 0,  0x2000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1112",    0x0A24902B, Code1100, 0,  0x4000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1112",    0x1A24902B, Code1100, 0,  0x4000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1112",    0x042D502B, Code1100, 0,  0x4000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1112",    0x2524D02B, Code1100, 0,  0x4000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1112",    0x0425502B, Code1100, 0,  0x4000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1112",    0x2524902B, Code1100, 0,  0x4000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1112",    0x00020023, Code1100, 0,  0x4000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1112",    0x00020022, Code1100, 0,  0x4000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1113",    0x0434502B, Code1100, 0,  0x6000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1113",    0x2532902B, Code1100, 0,  0x6000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1113",    0x4034102B, Code1100, 0,  0x6000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1113",    0x2532102B, Code1100, 0,  0x6000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1113",    0x0434102B, Code1100, 0,  0x6000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1113",    0x00030030, Code1100, 0,  0x6000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1113",    0x00030032, Code1100, 0,  0x6000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1114",    0x0A40902B, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1114",    0x1A40902B, Code1100, 0,  0x8000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1114",    0x0444502B, Code1100, 0,  0x8000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1114",    0x2540902B, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1114",    0x0444102B, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1114",    0x2540102B, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1114",    0x00040040, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1114",    0x00040042, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1114",    0x00040060, Code1100, 0,  0xC000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1114",    0x00040070, Code1100, 0,  0xE000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1115",    0x00050080, Code1100, 0, 0x10000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },

	{ "11C12",   0x1421102B, Code1100, 0,  0x4000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "11C14",   0x1440102B, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "11C22",   0x1431102B, Code1100, 0,  0x4000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "11C24",   0x1430102B, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },

	{ "11A02",   0x4D4C802B, Code1100, 0,  0x4000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "11A04",   0x4D80002B, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "11A11",   0x455EC02B, Code1100, 0,  0x2000, Map5, 0x10000200,  0x100, 0x2FC, 0x1C },
	{ "11A12",   0x4574802B, Code1100, 0,  0x4000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "11A13",   0x458A402B, Code1100, 0,  0x6000, Map5, 0x10000200,  0x800, 0x2FC, 0x1C },
	{ "11A14",   0x35A0002B, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "11A14",   0x45A0002B, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },

	{ "11E11",   0x293E902B, Code1100, 0,  0x2000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "11E12",   0x2954502B, Code1100, 0,  0x4000, Map5, 0x10000200,  0x800, 0x2FC, 0x1C },
	{ "11E13",   0x296A102B, Code1100, 0,  0x6000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "11E14",   0x2980102B, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "11E36",   0x00009C41, Code1100, 0, 0x18000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "11E37",   0x00007C41, Code1100, 0, 0x20000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },

	{ "1224",    0x3640C02B, Code1100, 0,  0x8000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1224",    0x3642C02B, Code1100, 0,  0xC000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1225",    0x3650002B, Code1100, 0, 0x10000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1225",    0x3652002B, Code1100, 0, 0x14000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1226",    0x3660002B, Code1100, 0, 0x18000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1227",    0x3670002B, Code1100, 0, 0x20000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },

	{ "1311",    0x2C42502B, Code1100, 0,  0x2000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1313",    0x2C40102B, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1315",    0x3A010523, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1316",    0x1A018524, Code1100, 0,  0xC000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1317",    0x1A020525, Code1100, 0, 0x10000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1342",    0x3D01402B, Code1100, 0,  0x4000, Map5, 0x10000200,  0x400, 0x2FC, 0x1C },
	{ "1343",    0x3D00002B, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1345",    0x28010541, Code1100, 0,  0x8000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1346",    0x08018542, Code1100, 0,  0xC000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1347",    0x08020543, Code1100, 0, 0x10000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },

	{ "1517",    0x00001517, Code1500, 1, 0x10000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1547",    0x00001547, Code1500, 1, 0x10000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1518",    0x00001518, Code1500, 1, 0x20000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1548",    0x00001548, Code1500, 1, 0x20000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1519",    0x00001519, Code1500, 1, 0x40000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1549",    0x00001549, Code1500, 1, 0x40000, Map5, 0x10000200, 0x1000, 0x2FC, 0x1C },

	{ "1751",    0x25001110, Code1700, 0,  0x8000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1751",    0x25001118, Code1700, 0,  0x8000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1752",    0x25001121, Code1700, 0, 0x10000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1754",    0x25011722, Code1700, 0, 0x20000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1756",    0x25011723, Code1700, 0, 0x40000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1758",    0x25013F37, Code1700, 0, 0x80000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1759",    0x25113737, Code1700, 0, 0x80000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1764",    0x26011922, Code1700, 0, 0x20000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1765",    0x26013733, Code1700, 0, 0x40000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1766",    0x26013F33, Code1700, 0, 0x40000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1767",    0x26012837, Code1700, 0, 0x80000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1768",    0x26013F37, Code1700, 0, 0x80000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1769",    0x26113F37, Code1700, 0, 0x80000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1774",    0x27011132, Code1700, 0, 0x20000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1776",    0x27191F43, Code1700, 0, 0x40000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1777",    0x27193747, Code1700, 0, 0x80000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1778",    0x27193F47, Code1700, 0, 0x80000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1785",    0x281D1743, Code1700, 0, 0x40000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1786",    0x281D1F43, Code1700, 0, 0x40000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1787",    0x281D3747, Code1700, 0, 0x80000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "1788",    0x281D3F47, Code1700, 0, 0x80000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },

	{ "4074",    0x47011132, Code1700, 1, 0x20000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "4076",    0x47191F43, Code1700, 1, 0x40000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "4078",    0x47193F47, Code1700, 1, 0x80000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },
	{ "4088",    0x481D3F47, Code1700, 1, 0x80000, Map4, 0x10000200, 0x1000, 0x2FC, 0x1C },

	{ "2103",    0x0004FF11, Code2000, 0,  0x8000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2104",    0xFFF0FF12, Code2000, 0,  0x4000, Map2, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2105",    0xFFF0FF22, Code2000, 0,  0x8000, Map2, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2106",    0xFFF0FF32, Code2000, 0, 0x10000, Map2, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2131/2141",   196353, Code2000, 0,  0x8000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2132/2142",   196369, Code2000, 0, 0x10000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2134/2144",   196370, Code2000, 0, 0x20000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2136/2146",   196387, Code2000, 0, 0x40000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2138/2148",   196389, Code2000, 0, 0x7D000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2109",      33685249, Code2000, 0,  0xE000, Map2, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2119",      33685266, Code2000, 0, 0x1E000, Map2, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2129",      33685267, Code2000, 0, 0x3E000, Map3, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2114",      16908050, Code2000, 0, 0x1E000, Map2, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2124",      16908051, Code2000, 0, 0x3E000, Map3, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2194",      50462483, Code2000, 0, 0x3E000, Map3, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2292",      67239699, Code2000, 0, 0x3E000, Map3, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2294",      84016915, Code2000, 0, 0x3E000, Map3, 0x40000200, 0x1000, 0x1FC, 0x14 },

	{ "2364",     369162498, Code2000, 0, 0x20000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2365",     369158179, Code2000, 0, 0x40000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2366",     369162531, Code2000, 0, 0x40000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2367",     369158181, Code2000, 0, 0x7E000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2368",     369162533, Code2000, 0, 0x7E000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2377",     385935397, Code2000, 0, 0x7E000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2378",     385940773, Code2000, 0, 0x7E000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2387",     402716981, Code2000, 0, 0x7E000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2388",     402718517, Code2000, 0, 0x7E000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },

	{ "2458",     352386869, Code2000, 0, 0x7E000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2468",     369164085, Code2000, 0, 0x7E000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },
	{ "2478",     386006837, Code2000, 0, 0x7E000, Map1, 0x40000200, 0x1000, 0x1FC, 0x14 },

	{      0,             0,        0, 0,       0,    0,          0,      0,     0,    0 }
};


const DEVICE *Device;		/* Detected device property */
const char *Del;		/* Delimiter character of ISP command */

uint32_t AddrRange[2];		/* Loaded address range {lowest, highest} */
uint8_t Buffer[0x80000];	/* Flash data buffer (512K) */

int Freq = 14748;		/* -f<freq> Oscillator frequency [kHz] */
// int Port = 1;			/* -p<port> Port numnber */
char Port[256] = "/dev/ttys1"; /* -p<port> Port name */
// int Baud = 115200;		/* -p<port>:<bps> Bit rate */
int Baud = 9600;		/* -p<port>:<bps> Bit rate */
int Pause;				/* -w<mode> Pause before exit program */
int Pol;				/* -c<flag> Invert signal polarity (b0:ER, b1:RS) */
int Read;				/* -r Read operation */
int Crp3;				/* -3 Do not block to program CRP3 and NO_ISP */


struct termios tio, oldtio;
struct timeval timeout = {0, 0};
typedef enum {
	CLRDTR,
	CLRRTS,
	SETDTR,
	SETRTS
} pinfunc_t;




/*-----------------------------------------------------------------------
  Search and Open configuration file
-----------------------------------------------------------------------*/

static
FILE *open_cfgfile(char *filename)
{
	FILE *fp;
	// char filepath[256], *dmy;


	if ((fp = fopen(filename, "rt")) != NULL) {
		return fp;
	}
	// if (SearchPath(NULL, filename, NULL, sizeof filepath, filepath, &dmy)) {
	// 	if ((fp = fopen(filepath, "rt")) != NULL) {
	// 		return fp;
	// 	}
	// }
	return NULL;
}




/*-----------------------------------------------------------------------
  Hex format manupilations
-----------------------------------------------------------------------*/


/* Pick a hexdecimal value from hex record */

uint32_t get_valh (
	char **lp,	/* pointer to line read pointer */
	int count, 	/* number of digits to get (2,4,6,8) */
	uint8_t *sum	/* byte check sum */
) {
	uint32_t val = 0;
	uint8_t n;


	do {
		n = *(*lp)++;
		if ((n -= '0') >= 10) {
			if ((n -= 7) < 10) return 0xFFFFFFFF;
			if (n > 0xF) return 0xFFFFFFFF;
		}
		val = (val << 4) + n;
		if (count & 1) *sum += (uint8_t)val;
	} while (--count);
	return val;
}




/* Load Intel Hex and Motorola S format file into data buffer */ 

long input_hexfile (
	FILE* fp,			/* input stream */
	uint8_t* buffer,		/* data input buffer */
	uint32_t buffsize,		/* size of data buffer */
	uint32_t* range		/* effective data size in the input buffer */
) {
	char line[600];			/* line input buffer */
	char *lp;				/* line read pointer */
	long lnum = 0;			/* input line number */
	uint16_t seg = 0, hadr = 0;	/* address expantion values for intel hex */
	uint32_t addr, count, n;
	uint8_t sum;


	while (fgets(line, sizeof(line), fp) != NULL) {
		lnum++;
		lp = &line[1]; sum = 0;

		if (line[0] == ':') {	/* Intel Hex format */
			if ((count = get_valh(&lp, 2, &sum)) > 0xFF) return lnum;	/* byte count */
			if ((addr = get_valh(&lp, 4, &sum)) > 0xFFFF) return lnum;	/* offset */

			switch (get_valh(&lp, 2, &sum)) {	/* block type? */
				case 0x00 :	/* data */
					addr += (seg << 4) + (hadr << 16);
					while (count--) {
						n = get_valh(&lp, 2, &sum);		/* pick a byte */
						if (n > 0xFF) return lnum;
						if (addr >= buffsize) continue;	/* clip by buffer size */
						if (addr > range[1]) range[1] = addr;	/* update data size information */
						if (addr < range[0]) range[0] = addr;
						buffer[addr++] = (uint8_t)n;		/* store the data */
					}
					break;

				case 0x01 :	/* end */
					if (count != 0) return lnum;
					break;

				case 0x02 :	/* segment base [19:4] */
					if (count != 2) return lnum;
					seg = (uint16_t)get_valh(&lp, 4, &sum);
					if (seg == 0xFFFF) return lnum;
					break;

				case 0x03 :	/* program start address (segment:offset) */
					if (count != 4) return lnum;
					get_valh(&lp, 8, &sum);
					break;

				case 0x04 :	/* high address base [31:16] */
					if (count != 2) return lnum;
					hadr = (uint16_t)get_valh(&lp, 4, &sum);
					if (hadr == 0xFFFF) return lnum;
					break;

				case 0x05 :	/* program start address (linear) */
					if (count != 4) return lnum;
					get_valh(&lp, 8, &sum);
					break;

				default:	/* invalid block */
					return lnum;
			} /* switch */
			if (get_valh(&lp, 2, &sum) > 0xFF) return lnum;	/* get check sum */
			if (sum) return lnum;							/* test check sum */
			continue;
		} /* if */

		if (line[0] == 'S') {	/* Motorola S format */
			if ((*lp >= '1')&&(*lp <= '3')) {

				switch (*lp++) {	/* record type? (S1/S2/S3) */
					case '1' :
						count = get_valh(&lp, 2, &sum) - 3;
						if (count > 0xFF) return lnum;
						addr = get_valh(&lp, 4, &sum);
						if (addr > 0xFFFF) return lnum;
						break;
					case '2' :
						count = get_valh(&lp, 2, &sum) - 4;
						if (count > 0xFF) return lnum;
						addr = get_valh(&lp, 6, &sum);
						if (addr > 0xFFFFFF) return lnum;
						break;
					default :
						count = get_valh(&lp, 2, &sum) - 5;
						if (count > 0xFF) return lnum;
						addr = get_valh(&lp, 8, &sum);
						if (addr == 0xFFFFFFFF) return lnum;
				}
				while (count--) {
					n = get_valh(&lp, 2, &sum);
					if (n > 0xFF) return lnum;
					if (addr >= buffsize) continue;	/* clip by buffer size */
					if (addr > range[1]) range[1] = addr;	/* update data size information */
					if (addr < range[0]) range[0] = addr;
					buffer[addr++] = (uint8_t)n;		/* store the data */
				}
				if (get_valh(&lp, 2, &sum) > 0xFF) return lnum;	/* get check sum */
				if (sum != 0xFF) return lnum;					/* test check sum */
			} /* switch */
			continue;
		} /* if */

		if (line[0] >= ' ') return lnum;
	} /* while */

	return feof(fp) ? 0 : -1;
}



/* Put an Intel Hex data block */

void put_hexline (
	FILE* fp,			/* output stream */
	const uint8_t* buffer,	/* pointer to data buffer */
	uint16_t ofs,			/* block offset address */
	uint8_t count,			/* data byte count */
	uint8_t type			/* block type */
) {
	uint8_t sum;

	/* Byte count, Offset address and Record type */
	fprintf(fp, ":%02X%04X%02X", count, ofs, type);
	sum = count + (ofs >> 8) + ofs + type;

	/* Data bytes */
	while (count--) {
		fprintf(fp, "%02X", *buffer);
		sum += *buffer++;
	}

	/* Check sum */
	fprintf(fp, "%02X\n", (uint8_t)-sum);
}



/* Output data in Intel Hex format */

void output_ihex (
	FILE* fp,			/* output stream */
	const uint8_t *buffer,	/* pointer to data buffer */
	uint32_t datasize,		/* number of bytes to be output */
	uint8_t blocksize		/* HEX block size (1,2,4,..,128) */
) {
	uint16_t ofs = 0;
	uint8_t hadr[2] = {0,0}, d, n;
	uint32_t bc = datasize;


	while (bc) {
		if ((ofs == 0) && (datasize > 0x10000)) {	/* A16 changed? */
			if (datasize > 0x100000) {
				put_hexline(fp, hadr, 0, 2, 4);
				hadr[1]++;
			} else {
				put_hexline(fp, hadr, 0, 2, 2);
				hadr[0] += 0x10;
			}
		}
		if (bc >= blocksize) {	/* full data block */
			for (d = 0xFF, n = 0; n < blocksize; n++) d &= *(buffer+n);
			if (d != 0xFF) put_hexline(fp, buffer, ofs, blocksize, 0);
			buffer += blocksize;
			bc -= blocksize;
			ofs += blocksize;
		} else {				/* fractional data block */
			for (d = 0xFF, n = 0; n < bc; n++) d &= *(buffer+n);
			if (d != 0xFF) put_hexline(fp, buffer, ofs, (uint8_t)bc, 0);
			bc = 0;
		}
	}

	put_hexline(fp, NULL, 0, 0, 1);	/* End block */
}



/*-----------------------------------------------------------------------
  Command line analysis
-----------------------------------------------------------------------*/


static
int load_commands (int argc, char** argv)
{
	// char *cp, *cmdlst[10], cmdbuff[256];
	char *cp, *pp, *cmdlst[10], cmdbuff[256];
	int cmd;
	FILE *fp;
	long n;


	/* Clear flash data buffer */
	memset(Buffer, 0xFF, sizeof Buffer);	/* Default value (if not loaded) */
	AddrRange[0] = sizeof(Buffer);	/* Lowest address */
	AddrRange[1] = 0;				/* Highest address */

	cmd = 0; cp = cmdbuff;

	/* Import ini file as command line parameters */
	fp = open_cfgfile(INIFILE);
	if (fp != NULL) {
		while (fgets(cp, cmdbuff + sizeof cmdbuff - cp, fp) != NULL) {
			if (cmd >= (sizeof cmdlst / sizeof cmdlst[0] - 1)) break;
			if (*cp <= ' ') break;
			while (cp[strlen(cp) - 1] <= ' ') cp[strlen(cp) - 1] = 0;
			cmdlst[cmd++] = cp; cp += strlen(cp) + 1;
		}
		fclose(fp);
	}

	/* Get command line parameters */
	while (--argc && (cmd < (sizeof cmdlst / sizeof cmdlst[0] - 1))) {
		cmdlst[cmd++] = *++argv;
	}
	cmdlst[cmd] = NULL;

	/* Analyze command line parameters... */
	for (cmd = 0; cmdlst[cmd] != NULL; cmd++) {
		cp = cmdlst[cmd];

		if (*cp == '-') {	/* Command switches... */
			cp++;
			switch (tolower(*cp++)) {
				case 'f' :	/* -f<num> (oscillator frequency) */
					Freq = strtoul(cp, &cp, 10);
					break;

				// case 'p' :	/* -p<num>[:<bps>] (control port and bit rate) */
				case 'p' :	/* -p<name>[:<bps>] (control port and bit rate) */
					pp = Port;
					while (*cp != ':' && *cp > ' ') *pp++ = *cp++;
					*pp = '\0';
					// Port = strtoul(cp, &cp, 10);
					if(*cp == ':')
						Baud = strtoul(cp+1, &cp, 10);
					break;

				case 'r' :	/* -r (read command) */
					Read = 1;
					break;

				case 'w' :	/* -w<num> (pause before exit 1:on error or 2:always) */
					Pause = strtoul(cp, &cp, 10);
					break;

				case 'c' :	/* -c<flag> (polarity of control signals) */
					Pol = strtoul(cp, &cp, 10);
					break;

				case '3' :	/* -3 (force programmed CRP3 and NO_ISP) */
					Crp3 = 1;
					break;

				default :	/* invalid command */
					return 1;
			} /* switch */
			if(*cp >= ' ') return 1;	/* option trails garbage */
		} /* if */

		else {	/* HEX Files */
			fprintf(stderr, "Loading \"%s\"...", cp);
			if ((fp = fopen(cp, "rt")) == NULL) {
				fprintf(stderr, "Unable to open.\n");
				return 2;
			}
			n = input_hexfile(fp, Buffer, sizeof Buffer, AddrRange);
			fclose(fp);
			if (n) {
				if (n < 0) {
					fprintf(stderr, "file access failure.\n");
				} else {
					fprintf(stderr, "hex format error at line %ld.\n", n);
				}
				return 2;
			}
			fprintf(stderr, "passed.\n");
		} /* else */

	} /* for */

	return 0;
}



/*-----------------------------------------------------------------------
  LPC 2000/1000 programming controls
-----------------------------------------------------------------------*/

/* Get top address of the sector contains given address */
static
uint32_t adr2sect (
	uint32_t addr
)
{
	uint32_t n;

	for (n = 1; addr >= Device->SectMap[n]; n++) ;
	return n - 1;
}



/* Create an uuencoded asciz string from a byte array */
static
void uuencode (
	const void* src,	/* Pointer to the input data */
	int srcsize,		/* Size of input data (0 to 45) */
	char* dst			/* Pointer to the output buffer */
)
{
	const unsigned char *bin = (const unsigned char*)src;
	unsigned char c1, c2, c3;
	char c;
	int cc;


	if (srcsize >= 0 || srcsize <= 45) {
		c = srcsize + 0x20;
		*dst++ = (c == ' ') ? '`' : c;

		for (cc = 1; srcsize > 0; srcsize -= 3, cc += 4) {
			c1 = *bin++;
			c2 = c3 = 0;
			if (srcsize >= 2) {
				c2 = *bin++;
				if (srcsize >= 3) {
					c3 = *bin++;
				}
			}
			c = (c1 >> 2) + 0x20;
			*dst++ = (c == ' ') ? '`' : c;
			c = ((c1 & 3) << 4) + (c2 >> 4) + 0x20;
			*dst++ = (c == ' ') ? '`' : c;
			c = ((c2 & 15) << 2) + (c3 >> 6) + 0x20;
			*dst++ = (c == ' ') ? '`' : c;
			c = (c3 & 63) + 0x20;
			*dst++ = (c == ' ') ? '`' : c;
		}
	}
	*dst = 0;
}



/* Create a byte array from an uuencoded string */
static
int uudecode (				/* Returns number of bytes decoded (-1:error) */
	const char* src,		/* Pointer to the input string */
	unsigned char* dst		/* Pointer to the output buffer (must be 45 byte at least) */
)
{
	unsigned char b1, b2, b3, b4;
	int bc, cc;


	b1 = *(src++) - 0x20;
	if (b1 > 64) return -1;
	bc = b1 & 63;

	for (cc = bc; cc > 0; cc -= 3) {
		b1 = *(src++) - 0x20;
		if (b1 > 64) return -1;
		b2 = *(src++) - 0x20;
		if (b2 > 64) return -1;
		*(dst++) = (b1 << 2) + ((b2 >> 4) & 3);
		if (cc >= 2) {
			b3 = *(src++) - 0x20;
			if (b3 > 64) return -1;
			*(dst++) = (b2 << 4) + ((b3 >> 2) & 15);
			if (cc >= 3) {
				b4 = *(src++) - 0x20;
				if (b4 > 64) return -1;
				*(dst++) = (b3 << 6) + (b4 & 63);
			}
		}
	}

	return bc;
}



/* Create CRC32 sum */
static
uint32_t crc32 (
	const uint8_t* src,
	unsigned int cnt
)
{
	uint32_t r, n;


	r = 0xFFFFFFFF;
    do {
		r ^= *src++;
		for (n = 0; n < 8; n++) {
			r = r & 1 ? r >> 1 ^ 0xEDB88320 : r >> 1;
		}
	} while (--cnt);
    return ~r;
}

static 
int receive_serial (
	int com,
	void *buff,
	int bufsize
)
{
	fd_set rfds;
	int len = 0, ready, rc;
	char *p = buff;

	while (len < bufsize) {
		FD_ZERO(&rfds);
		FD_SET(com, &rfds);

		ready = select(com + 1, &rfds, NULL, NULL, &timeout);
		if (ready == 0) {
			return 0;
		} else if (ready == -1) {
			if (errno == EAGAIN) {
				continue;
			} else {
				return 0;
			}
		}

		rc = read(com, p, bufsize - len);
		if (rc < 0) return 0;
		p += rc;
		len += rc;
	}
	return len;
}

/* Get a line from the device */
static
int rcvr_line (
	// HANDLE com,
	int com,
	char* buff,
	int bufsize
)
{
	int rc, i = 0;


	for (;;) {
		// ReadFile(com, &buff[i], 1, &rc, NULL);	/* Get a character */
		rc = receive_serial(com, &buff[i], 1);
		if (rc == 0) {	/* I/O error? */
			i = 0; break;
		}
		if (buff[i] == '\n') break;			/* EOL? */
		if ((uint8_t)buff[i] < 0x20) continue;	/* Ignore invisible chars */
		i++;
		if (i >= bufsize - 1) {				/* Buffer overflow? */
			i = 0; break;
		}
	}
	buff[i] = 0;	/* Remove \n */
	return i;
}

/* Control the RTS and DTR pins */
static int ctrl_pin (int fd, pinfunc_t f)
{
	int flag;
	switch (f) {
		case CLRDTR:
			flag = TIOCM_DTR;
			return ioctl(fd, TIOCMBIC, &flag);
		case CLRRTS:
			flag = TIOCM_RTS;
			return ioctl(fd, TIOCMBIC, &flag);
		case SETDTR:
			flag = TIOCM_DTR;
			return ioctl(fd, TIOCMBIS, &flag);
		case SETRTS:
			flag = TIOCM_RTS;
			return ioctl(fd, TIOCMBIS, &flag);
	}
}



speed_t get_baud (int baud)
{
	switch (baud) {
		case 1200:
			return B1200;
		case 1800:
			return B1800;
		case 2400:
			return B2400;
		case 4800:
			return B4800;
		case 9600:
			return B9600;
		case 19200:
			return B19200;
		case 38400:
			return B38400;
#ifdef B7200
		case 7200:
			return B7200;
#endif
#ifdef B14400
		case 14400:
			return B14400;
#endif
#ifdef B28800
		case 28800:
			return B28800;
#endif
#ifdef B57600
		case 57600:
			return B57600;
#endif
#ifdef B76800
    	case 76800:
			return B76800;
#endif
#ifdef B115200
		case 115200:
			return B115200;
#endif
#ifdef B230400
		case 230400:
			return B230400;
#endif
		default:
			fprintf(stderr, "Not defined baud rate: %d\n", baud);
			return (speed_t)baud;
	}
}



static
int enter_ispmode (
	int* com
)
{
	// DCB dcb = { sizeof(DCB),
	// 			Baud, TRUE, FALSE, FALSE, FALSE,
	// 			DTR_CONTROL_DISABLE, FALSE,
	// 			TRUE, FALSE, FALSE, FALSE, FALSE,
	// 			RTS_CONTROL_DISABLE, FALSE, 0, 0,
	// 			10, 10,
	// 			8, NOPARITY, ONESTOPBIT, '\x11', '\x13', '\xFF', '\xFF', 0 };
	// COMMTIMEOUTS ct1 = { 0, 1, 50, 1, 50},
	// 			 ct2 = { 0, 1, 500, 1, 500};
	tio.c_cflag += CREAD;               // 受信有効
    tio.c_cflag += CLOCAL;              // ローカルライン（モデム制御なし）
    tio.c_cflag += CS8;                 // データビット:8bit
    tio.c_cflag += 0;                   // ストップビット:1bit
    tio.c_cflag += 0;                   // パリティ:None

	char str[20];
	// HANDLE h;
	int h;
	uint32_t wc, n, m;
	int rc = 0;


	/* Open communication port */
	// sprintf(str, "\\\\.\\COM%u", Port);
	// *com = h = CreateFile(str, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	*com = h = open(Port, O_RDWR | O_NOCTTY | O_NONBLOCK);
	// if (h == INVALID_HANDLE_VALUE) {
	if (h < 0) {
		// fprintf(stderr, "%s could not be opened.\n", str);
		fprintf(stderr, "%s could not be opened.\n", Port);
		return 5;
	}
	tcgetattr(h, &oldtio); /* Save current port settings */
	// SetCommState(h, &dcb);		/* Set serial bit rate */
	// SetCommTimeouts(h, &ct1);	/* Set processing timeout of 200m sec */
	timeout.tv_sec = 0;
	timeout.tv_usec = 200 * 1000; /* Set processing timeout of 200m sec */
	// EscapeCommFunction(h, (Pol & 2) ? SETRTS : CLRRTS);	/* Set BOOT pin low if RTS controls it */
	cfsetspeed(&tio, get_baud(Baud)); /* Set serial bit rate */
	cfmakeraw(&tio); /* Set the LAW mode */
    tcsetattr(h, TCSANOW, &tio); /* Reflect settings */
	ctrl_pin(h, (Pol & 2) ? SETRTS : CLRRTS); /* Set BOOT pin low if RTS controls it */

	MESS("Entering ISP mode.");
	for (n = 2; n; n--) {
		/* Reset the device if DTR signal controls RESET pin */
		// EscapeCommFunction(h, (Pol & 1) ? SETDTR : CLRDTR);
		ctrl_pin(h, (Pol & 1) ? SETDTR : CLRDTR);
		// Sleep(50);
		usleep(50000);
		// EscapeCommFunction(h, (Pol & 1) ? SETDTR : CLRDTR);
		ctrl_pin(h, (Pol & 1) ? CLRDTR : SETDTR);
		// Sleep(50);
		usleep(50000);

		for (m = 0; m < 12; m++) {
			// PurgeComm(h, PURGE_RXABORT|PURGE_RXCLEAR);
			tcflush(h, TCIOFLUSH);
			// WriteFile(h, "?", 1, &wc, NULL);
			wc = write(h, "?", 1);
			if (rcvr_line(h, str, sizeof str) && !strcmp(str, "Synchronized")) {
				Del = ((n & 1) ^ (int)(m == 0)) ? "\r\n" : "\n";
				sprintf(str, "Synchronized%s", Del);
				// WriteFile(h, str, strlen(str), &wc, NULL);
				wc = write(h, str, strlen(str));
				rcvr_line(h, str, sizeof str);
				if (rcvr_line(h, str, sizeof str) && !strcmp(str, "OK")) break;
				if (m) m = 12;
			}
		}
		if (m < 12) break;
	}
	if (n) {
		sprintf(str, "%u%s", Freq, Del);
		// WriteFile(h, str, strlen(str), &wc, NULL);
		wc = write(h, str, strlen(str));
		rcvr_line(h, str, sizeof str);
		if (!rcvr_line(h, str, sizeof str) || strcmp(str, "OK")) n = 0;
	}
	if (!n) {
		MESS("failed to sync.\n");
		rc = 6;
	}
	if (!rc) {
		MESS(".");
		sprintf(str, "A 0%s", Del);
		// WriteFile(h, str, strlen(str), &wc, NULL);	/* Echo Off */
		wc = write(h, str, strlen(str));
		rcvr_line(h, str, sizeof str);
		if (!rcvr_line(h, str, sizeof str) || strcmp(str, "0")) {
			MESS("failed(A).\n");
			rc = 6;
		}
	}
	if (!rc) {
		MESS(".");
		sprintf(str, "J%s", Del);
		// WriteFile(h, str, strlen(str), &wc, NULL);	/* Get device ID */
		wc = write(h, str, strlen(str));
		if (!rcvr_line(h, str, sizeof str) || strcmp(str, "0")) {
			MESS("failed(J).\n");
			rc = 6;
		} else {
			/* Find target device type by device ID */
			rcvr_line(h, str, sizeof str);
			wc = atol(str);
			for (Device = DevLst; Device->Sign != wc && Device->Sign; Device++) ;
			if (!Device->Sign) {
				fprintf(stderr, " unknown device (%u).", wc);
				rc = 6;
			}
		}
	}
	if (!rc) {
		MESS(".");
		sprintf(str, "U 23130%s", Del);	/* Unlock */
		// WriteFile(h, str, strlen(str), &wc, NULL);
		wc = write(h, str, strlen(str));
		if (!rcvr_line(h, str, sizeof str) || strcmp(str, "0")) {
			MESS("failed(U).\n");
			rc = 6;
		}
	}
	if (!rc) {
		fprintf(stderr, "passed.\nDetected device is LPC%s (%uK).\n", Device->DeviceName, Device->FlashSize / 1024);
		// SetCommTimeouts(h, &ct2);	/* Set processing timeout of 500m sec */
		timeout.tv_sec = 0;
		timeout.tv_usec = 500 * 1000;
	}

	return rc;
}



static
void exit_ispmode (
	// HANDLE com
	int com
)
{
	/* Apply a reset to the device if DTR/RTS controls RESET/BOOT pin */
	// EscapeCommFunction(com, (Pol & 2) ? CLRRTS : SETRTS);	/* Set BOOT pin high */
	ctrl_pin(com, (Pol & 2) ? CLRRTS : SETRTS);	/* Set BOOT pin high */
	// EscapeCommFunction(com, (Pol & 1) ? SETDTR : CLRDTR);	/* Set RESET pin low */
	ctrl_pin(com, (Pol & 1) ? SETDTR : CLRDTR);	/* Set RESET pin low */
	// Sleep(50);
	usleep(50000);
	// EscapeCommFunction(com, (Pol & 1) ? CLRDTR : SETDTR);	/* Set RESET pin high */
	ctrl_pin(com, (Pol & 1) ? CLRDTR : SETDTR);	/* Set RESET pin high */

	tcsetattr(com, TCSANOW, &oldtio);
    close(com);
}




static
int read_flash (
	// HANDLE com,
	int com,
	uint8_t* buffer
)
{
	uint32_t addr, cc, xc, sum, d, bx;
	uint16_t dsum, vsum;
	int i;
	char buf[80];


	MESS("Reading.");

	/* Download user code to read flash with remapping disabled */
	sprintf(buf, "W %u %u%s", Device->XferAddr, SZ_CODE, Del);
	// WriteFile(com, buf, strlen(buf), &bx, NULL);
	bx = write(com, buf, strlen(buf));
	if (!rcvr_line(com, buf, sizeof buf) || strcmp(buf, "0")) {
		fprintf(stderr, "failed(W,%s).\n", buf);
		return 10;
	}
	if (Device->RawMode) {
		// WriteFile(com, Device->Code, SZ_CODE, &bx, NULL);
		bx = write(com, Device->Code, SZ_CODE);
	} else {
		for (xc = sum = 0; xc < SZ_CODE; xc += cc) {
			cc = (xc + 45 <= SZ_CODE) ? 45 : SZ_CODE - xc;
			uuencode(&Device->Code[xc], cc, buf);
			strcat(buf, "\r\n");
			// WriteFile(com, buf, strlen(buf), &bx, NULL);
			bx = write(com, buf, strlen(buf));
			for (d = 0; d < cc; d++) sum += Device->Code[xc + d];
		}
		sprintf(buf, "%u\r\n", sum);
		// WriteFile(com, buf, strlen(buf), &bx, NULL);
		bx = write(com, buf, strlen(buf));
		if (!rcvr_line(com, buf, sizeof buf) || strcmp(buf, "OK")) {
			fprintf(stderr, "failed(%s).\n", buf);
			return 10;
		}
	}

	/* Execute the loaded code */
	sprintf(buf, "G %u T%s", Device->XferAddr, Del);
	// WriteFile(com, buf, strlen(buf), &bx, NULL);
	bx = write(com, buf, strlen(buf));
	if (!rcvr_line(com, buf, sizeof buf) || strcmp(buf, "0")) {
		fprintf(stderr, "failed(G,%s).\n", buf);
		return 10;
	}

	/* Receive flash memory data and store it to the buffer */
	addr = 0;
	do {
		buffer[addr] = 0xAA;
		// WriteFile(com, &buffer[addr], 1, &bx, NULL);	/* Send a 0xAA to start to transmit a 1KB block */
		bx = write(com, &buffer[addr], 1); /* Send a 0xAA to start to transmit a 1KB block */
		// ReadFile(com, &buffer[addr], 1024, &bx, NULL);	/* Receive a data block */
		bx = receive_serial(com, &buffer[addr], 1024); /* Receive a data block */
		if (bx < 1024) {
			MESS("timeout.\n");
			return 11;
		}
		// ReadFile(com, &vsum, 2, &bx, NULL);				/* Receive BCC */
		bx = receive_serial(com, &vsum, 2); /* Receive BCC */
		if (bx < 2) {
			MESS("timeout.\n");
			return 11;
		}
		for (i = dsum = 0; i < 1024; dsum += buffer[addr + i], i++) ;
		if (dsum != vsum) {
			MESS("data error.\n");
			return 11;
		}
		if (addr % 0x2000 == 0) MESS(".");	/* Display progress indicator at every 8K byte */
		addr += 1024;
	} while (addr < Device->FlashSize);

	MESS("passed.\n");

	return 0;
}




static
int erase_flash (
	// HANDLE com
	int com
)
{
	uint32_t es, n;
	char buf[80];
	// COMMTIMEOUTS ct1 = { 0, 1, 2000, 1, 250},
				//  ct2 = { 0, 1, 500, 1, 500};


	MESS("Erasing.");

	/* End sector is last sector of the device */
	es = adr2sect(Device->FlashSize - 1);

	/* Prepare to write/erase sectors */
	sprintf(buf, "P 0 %u%s", es, Del);
	// WriteFile(com, buf, strlen(buf), &n, NULL);
	n = write(com, buf, strlen(buf));
	MESS(".");
	if (!rcvr_line(com, buf, sizeof buf) || strcmp(buf, "0")) {
		fprintf(stderr, "failed(P,%s).\n", buf);
		return 12;
	}

	// SetCommTimeouts(com, &ct1);	/* Set processing timeout of 2 sec */
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	/* Erase sectors */
	sprintf(buf, "E 0 %u%s", es, Del);
	// WriteFile(com, buf, strlen(buf), &n, NULL);
	n = write(com, buf, strlen(buf));
	MESS(".");
	if (!rcvr_line(com, buf, sizeof buf) || strcmp(buf, "0")) {
		fprintf(stderr, "failed(E,%s).\n", buf);
		return 12;
	}
	// SetCommTimeouts(com, &ct2);	/* Restore processing timeout */
	timeout.tv_sec = 0;
	timeout.tv_usec = 500 * 1000;

	MESS("passed.\n");
	return 0;
}




static
int write_flash (
	// HANDLE com,
	int com,
	const uint8_t* buffer
)
{
	uint32_t wa, pc, n, lc, cc, xc, sum;
	char buf[80], *tp;


	MESS("Writing.");

	wa = (AddrRange[1] + Device->XferSize) & ~(Device->XferSize - 1);	/* Write from high address block */
	pc = 0;

	while (wa > 0) {
		wa -= Device->XferSize;
		/* Send a data block to SRAM */
		sprintf(buf, "W %u %u%s", Device->XferAddr, Device->XferSize, Del);
		// WriteFile(com, buf, strlen(buf), &n, NULL);
		n = write(com, buf, strlen(buf));
		if (!rcvr_line(com, buf, sizeof buf) || strcmp(buf, "0")) {
			fprintf(stderr, "failed(W,%s).\n", buf);
			return 13;
		}
		if (Device->RawMode) {	/* Raw mode transfer */
			// WriteFile(com, &buffer[wa], Device->XferSize, &xc, NULL);	/* Send data */
			xc = write(com, &buffer[wa], Device->XferSize); /* Send data */
			/* Check if data has been sent with no error */
			sprintf(buf, "S %u %u%s", Device->XferAddr, Device->XferSize, Del);
			// WriteFile(com, buf, strlen(buf), &n, NULL);
			n = write(com, buf, strlen(buf));
			if (!rcvr_line(com, buf, sizeof buf) || strcmp(buf, "0") ||
				!rcvr_line(com, buf, sizeof buf) || strtoul(buf, &tp, 10) != crc32(&buffer[wa], Device->XferSize)
				) {
				fprintf(stderr, "failed(S,%s).\n", buf);
				return 13;
			}
		} else {				/* Text mode transfer */
			for (xc = lc = sum = 0; xc < Device->XferSize; xc += cc) {
				if (xc + 45 <= Device->XferSize) {
					cc = 45;
					lc++;
				} else {
					cc = Device->XferSize - xc;
					lc = 20;
				}
				uuencode(&buffer[wa+xc], cc, buf);
				strcat(buf, Del);
				// WriteFile(com, buf, strlen(buf), &n, NULL);
				n = write(com, buf, strlen(buf));
				for (n = 0; n < cc; n++) sum += buffer[wa + xc + n];
				if (lc == 20) {
					sprintf(buf, "%u%s", sum, Del);
					// WriteFile(com, buf, strlen(buf), &n, NULL);
					n = write(com, buf, strlen(buf));
					if (!rcvr_line(com, buf, sizeof buf) || strcmp(buf, "OK")) {
						fprintf(stderr, "failed(%s).\n", buf);
						return 13;
					}
					lc = sum = 0;
				}
			}
		}
		/* Prepare a sector to write flash */
		sprintf(buf, "P %u %u%s", adr2sect(wa), adr2sect(wa), Del);
		// WriteFile(com, buf, strlen(buf), &n, NULL);
		n = write(com, buf, strlen(buf));
		if (!rcvr_line(com, buf, sizeof buf) || strcmp(buf, "0")) {
			fprintf(stderr, "failed(P,%s).\n", buf);
			return 13;
		}
		/* Copy RAM to flash */
		sprintf(buf, "C %u %u %u%s", wa, Device->XferAddr, Device->XferSize, Del);
		// WriteFile(com, buf, strlen(buf), &n, NULL);
		n = write(com, buf, strlen(buf));
		if (!rcvr_line(com, buf, sizeof buf) || strcmp(buf, "0")) {
			fprintf(stderr, "failed(C,%s).\n", buf);
			return 13;
		}

		if (pc % 0x2000 == 0) MESS(".");	/* Display a progress indicator every 8K byte */
		pc += Device->XferSize;
	}

	MESS("passed.\n");
	return 0;
}



void _pause (
	int rc
)
{
	if ((Pause == 2) || ((Pause == 1) && rc)) {
		MESS("\nType Enter to exit...");
		getchar();
	}
}



int main (int argc, char** argv)
{
	int rc;
	uint32_t s, n, i, d;
	// HANDLE hcom;
	int hcom;


	rc = load_commands(argc, argv);
	fprintf(stderr, "port = %s\n", Port);;
	fprintf(stderr, "baud = %d\n", Baud);;
	fprintf(stderr, "Pol = %d\n", Pol);;
	if (rc) {
		if (rc == 1) MESS(Usage);
		_pause(rc);
		return rc;
	}
	if (Read) {	/* Read mode */
		// rc = enter_ispmode(&hcom);
		rc = enter_ispmode(&hcom);
		if (!rc) {
			rc = read_flash(hcom, Buffer);
			if (!rc) {
				/* Check if application code is exist (sum of eight vector data) */
				for (i = n = 0; i < 32; i += 4) {
					n += LD_DWORD(&Buffer[i]);
				}
				if (n) {
					MESS("There is no valid program code.\n");
				} else {
					for (i = n = 0; i < Device->FlashSize; i += 4) {
						if (LD_DWORD(&Buffer[i]) != 0xFFFFFFFF) n = i + 4;
					}
					d = n * 1000 / Device->FlashSize;
					fprintf(stderr, " %u.%u%% of flash memory is used.\n", d / 10, d % 10);
				}
				output_ihex(stdout, Buffer, Device->FlashSize, 32);
			}
			exit_ispmode(hcom);
		}
	} else {	/* Write mode */
		if (AddrRange[1] == 0) {	/* Check if any data is loaded */
			MESS(Usage);
			_pause(1);
			return 1;
		}
		fprintf(stderr, "Loaded address range is %05X-%05X.\n", AddrRange[0], AddrRange[1]);
		if (AddrRange[0] != 0) {	/* Check if vector area is loaded */
			MESS("Vector table is not loaded.\n");
			_pause(1);
			return 1;
		}
		rc = enter_ispmode(&hcom);	/* Open port, enter ispmode and detect device type */
		if (!rc) {
			if (AddrRange[1] >= Device->FlashSize) {
				MESS("Too large data for this device.\n");
				_pause(1);
				return 1;
			}
			s = LD_DWORD(&Buffer[Device->CRP]);
			if (!Crp3 && (s == 0x43218765 || s == 0x4E697370)) {
				MESS("Programming aborted due to CRP3 or NO_ISP.\nSpecify -3 to force program these CRP options.\n");
				_pause(1);
				return 1;
			}
			/* Validate application code (create check sum) */
			for (i = s = 0; i < 32; i += 4) {
				s += LD_DWORD(&Buffer[i]);
			}
			i = Device->Sum;
			n = LD_DWORD(&Buffer[i]) - s;
			ST_DWORD(&Buffer[i], n);
			/* Erase entire flash memory and write application code */
			rc = erase_flash(hcom);
			if (!rc) rc = write_flash(hcom, Buffer);
			exit_ispmode(hcom);
		}
	}

	_pause(rc);
	return rc;
}

