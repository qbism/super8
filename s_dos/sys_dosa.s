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
//
// sys_dosa.s
// x86 assembly-language DOS-dependent routines.

#include "asm_i386.h"
#include "quakeasm.h"


	.data

	.align	4
fpenv:
	.long	0, 0, 0, 0, 0, 0, 0, 0

	.text

.globl C(MaskExceptions)
C(MaskExceptions):
	fnstenv	fpenv
	orl		$0x3F,fpenv
	fldenv	fpenv

	ret

#if 0
.globl C(unmaskexceptions)
C(unmaskexceptions):
	fnstenv	fpenv
	andl		$0xFFFFFFE0,fpenv
	fldenv	fpenv

	ret
#endif

	.data

	.align	4
.globl	ceil_cw, single_cw, full_cw, cw, pushed_cw
ceil_cw:	.long	0
single_cw:	.long	0
full_cw:	.long	0
cw:			.long	0
pushed_cw:	.long	0

	.text

.globl C(Sys_LowFPPrecision)
C(Sys_LowFPPrecision):
	fldcw	single_cw

	ret

.globl C(Sys_HighFPPrecision)
C(Sys_HighFPPrecision):
	fldcw	full_cw

	ret

.globl C(Sys_PushFPCW_SetHigh)
C(Sys_PushFPCW_SetHigh):
	fnstcw	pushed_cw
	fldcw	full_cw

	ret

.globl C(Sys_PopFPCW)
C(Sys_PopFPCW):
	fldcw	pushed_cw

	ret

.globl C(Sys_SetFPCW)
C(Sys_SetFPCW):
	fnstcw	cw
	movl	cw,%eax
#if	id386
	andb	$0xF0,%ah
	orb		$0x03,%ah	// round mode, 64-bit precision
#endif
	movl	%eax,full_cw

#if	id386
	andb	$0xF0,%ah
	orb		$0x0C,%ah	// chop mode, single precision
#endif
	movl	%eax,single_cw

#if	id386
	andb	$0xF0,%ah
	orb		$0x08,%ah	// ceil mode, single precision
#endif
	movl	%eax,ceil_cw

	ret


.intel_syntax noprefix
	
.globl C(SetYourBlues)
C(SetYourBlues):
	push  ax
	push  dx

	mov   dx, 0x3C8
	mov   ax, 1
	out   dx, al
	inc   dx

	mov   ax, 1
	out   dx, al

	mov   ax, 5
	out   dx, al

	mov   ax, 16
	out   dx, al

	pop   dx
	pop   ax
	ret

	
.globl C(SetYourBrowns)
C(SetYourBrowns):
// Quake startup text mode scheme
// background brown
	push  ax
	push  dx

	mov   dx, 0x3C8
	mov   ax, 1
	out   dx, al
	inc   dx

	mov   ax, 14
	out   dx, al

	mov   ax, 9
	out   dx, al

	mov   ax, 4
	out   dx, al

	pop   dx
	pop   ax


// Console-style REALLY background brown, kind of cool
	push  ax
	push  dx

	mov   dx, 0x3C8
	mov   ax, 0
	out   dx, al
	inc   dx

	mov   ax, 12
	out   dx, al

	mov   ax, 7
	out   dx, al

	mov   ax, 2
	out   dx, al

	pop   dx
	pop   ax

// status grays (are now brown)
/*
push  ax
	push  dx

	mov   dx, 0x3C8
	mov   ax, 7
	out   dx, al
	inc   dx

	mov   ax, 121
	out   dx, al

	mov   ax, 110
	out   dx, al

	mov   ax, 22
	out   dx, al

	pop   dx
	pop   ax
*/

push  ax
	push  dx

	mov   dx, 0x3C8
	mov   ax, 7
	out   dx, al
	inc   dx

	mov   ax, 48
	out   dx, al

	mov   ax, 48
	out   dx, al

	mov   ax, 48
	out   dx, al

	pop   dx
	pop   ax

// Title bar 
	push  ax
	push  dx

	mov   dx, 0x3C8
	mov   ax, 2
	out   dx, al
	inc   dx

	mov   ax, 56
	out   dx, al

	mov   ax, 48
	out   dx, al

	mov   ax, 15
	out   dx, al

	pop   dx
	pop   ax


	ret