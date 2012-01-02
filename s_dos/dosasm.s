/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "asm_i386.h"

.data
fpenv:	.long	0, 0, 0, 0, 0, 0, 0, 0

.text
.globl	C(StartMSRInterval)
C(StartMSRInterval):
	movl	$0x11,%ecx	// read the CESR
	.byte	0x0F
	.byte	0x32		// RDMSR

	andl	$0xFE3FFE3F,%eax	// stop both counters
	.byte	0x0F
	.byte	0x30		// WRMSR

	movl	4(%esp),%eax	// point counter 0 to desired event, with counters
	andl	$0x3F,%eax		//  still stopped
	movl	$0x11,%ecx
	.byte	0x0F
	.byte	0x30		// WRMSR

	movl	$0x12,%ecx	// set counter 0 to the value 0
	subl	%eax,%eax
	subl	%edx,%edx
	.byte	0x0F
	.byte	0x30		// WRMSR

	movl	4(%esp),%eax // restart counter 0 with selected event
	andl	$0x3F,%eax
	subl	%edx,%edx
	orl		$0xC0,%eax
	movl	$0x11,%ecx	// control and event select
	.byte	0x0F
	.byte	0x30		// WRMSR

	ret

.globl	C(EndMSRInterval)
C(EndMSRInterval):
	movl	$0x12,%ecx	// counter 0
	.byte	0x0F
	.byte	0x32		// RDMSR

	ret					// lower 32 bits of count in %eax

#if 0
	.data
Lxxx:	.long	0

	.text

.globl C(setstackcheck)
C(setstackcheck):

	movl	%esp,%eax
	subl	$0x38000,%eax
	movl	$0x5A5A5A5A,(%eax)
	movl	%eax,Lxxx

	ret


.globl C(dostackcheck)
C(dostackcheck):

	movl	Lxxx,%edx
	movl	$0,%eax

	cmpl	$0x5A5A5A5A,(%edx)
	jz		qqq
	incl	%eax
qqq:

	ret
#endif

