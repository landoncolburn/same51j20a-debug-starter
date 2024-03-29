// SPDX-License-Identifier: GPL-2.0-or-later

/***************************************************************************
 *   Copyright (C) 2008 by Dominic Rath                                    *
 *   Dominic.Rath@gmx.de                                                   *
 *   Copyright (C) 2008 by Spencer Oliver                                  *
 *   spen@spen-soft.co.uk                                                  *
 *   Copyright (C) 2008 by Frederik Kriewtz                                *
 *   frederik@kriewitz.eu                                                  *
 ***************************************************************************/

#include "sam.h"
#include "dcc_stdio.h"

#define TARGET_REQ_TRACEMSG 0x00
#define TARGET_REQ_DEBUGMSG_ASCII 0x01
#define TARGET_REQ_DEBUGMSG_HEXMSG(size) (0x01 | ((size & 0xff) << 8))
#define TARGET_REQ_DEBUGCHAR 0x02

#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_6SM__)

/* we use the System Control Block DCRDR reg to simulate a arm7_9 dcc channel
 * DCRDR[7:0] is used by target for status
 * DCRDR[15:8] is used by target for write buffer
 * DCRDR[23:16] is used for by host for status
 * DCRDR[31:24] is used for by host for write buffer */

#define NVIC_DBG_DATA_R (*((volatile unsigned short *)0xE000EDF8))

#define BUSY 1

void dbg_write(unsigned long dcc_data)
{
	int len = 4;

	// MZ: added check for debug enabled so we don't hang on normal run
	if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) == CoreDebug_DHCSR_C_DEBUGEN_Msk)
	{
		while (len--)
		{
			/* wait for data ready */
			while (NVIC_DBG_DATA_R & BUSY)
				;

			/* write our data and set write flag - tell host there is data*/
			NVIC_DBG_DATA_R = (unsigned short)(((dcc_data & 0xff) << 8) | BUSY);
			dcc_data >>= 8;
		}
	}
}

#elif defined(__ARM_ARCH_4T__) || defined(__ARM_ARCH_5TE__) || defined(__ARM_ARCH_5T__)

void dbg_write(unsigned long dcc_data)
{
	unsigned long dcc_status;

	do
	{
		asm volatile("mrc p14, 0, %0, c0, c0" : "=r"(dcc_status));
	} while (dcc_status & 0x2);

	asm volatile("mcr p14, 0, %0, c1, c0" : : "r"(dcc_data));
}

#else
#error unsupported target
#endif

void dbg_trace_point(unsigned long number)
{
	dbg_write(TARGET_REQ_TRACEMSG | (number << 8));
}

void dbg_write_u32(const unsigned long *val, long len)
{
	dbg_write(TARGET_REQ_DEBUGMSG_HEXMSG(4) | ((len & 0xffff) << 16));

	while (len > 0)
	{
		dbg_write(*val);

		val++;
		len--;
	}
}

void dbg_write_u16(const unsigned short *val, long len)
{
	unsigned long dcc_data;

	dbg_write(TARGET_REQ_DEBUGMSG_HEXMSG(2) | ((len & 0xffff) << 16));

	while (len > 0)
	{
		dcc_data = val[0] | ((len > 1) ? val[1] << 16 : 0x0000);

		dbg_write(dcc_data);

		val += 2;
		len -= 2;
	}
}

void dbg_write_u8(const unsigned char *val, long len)
{
	unsigned long dcc_data;

	dbg_write(TARGET_REQ_DEBUGMSG_HEXMSG(1) | ((len & 0xffff) << 16));

	while (len > 0)
	{
		dcc_data = val[0] | ((len > 1) ? val[1] << 8 : 0x00) | ((len > 2) ? val[2] << 16 : 0x00) | ((len > 3) ? val[3] << 24 : 0x00);

		dbg_write(dcc_data);

		val += 4;
		len -= 4;
	}
}

void dbg_write_str(const char *msg)
{
	long len;
	unsigned long dcc_data;

	for (len = 0; msg[len] && (len < 65536); len++)
		;

	dbg_write(TARGET_REQ_DEBUGMSG_ASCII | ((len & 0xffff) << 16));

	while (len > 0)
	{
		dcc_data = msg[0] | ((len > 1) ? msg[1] << 8 : 0x00) | ((len > 2) ? msg[2] << 16 : 0x00) | ((len > 3) ? msg[3] << 24 : 0x00);
		dbg_write(dcc_data);

		msg += 4;
		len -= 4;
	}
}

void dbg_write_char(char msg)
{
	dbg_write(TARGET_REQ_DEBUGCHAR | ((msg & 0xff) << 16));
}
