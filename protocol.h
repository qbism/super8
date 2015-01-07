/*  Copyright (C) 1996-1997 Id Software, Inc.
    Copyright (C) 1999-2012 other authors as noted in code comments

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.   */
// protocol.h -- communications protocols

#define	PROTOCOL_NETQUAKE	15
#define PROTOCOL_QBS8  8888 //qb:

// if the high bit of the servercmd is set, the low bits are fast update flags:
#define	U_MOREBITS  	(1<<0)
#define	U_ORIGIN1   	(1<<1)
#define	U_ORIGIN2   	(1<<2)
#define	U_ORIGIN3   	(1<<3)
#define	U_ANGLE2    	(1<<4)
#define	U_NOLERP    	(1<<5)		// don't interpolate movement
#define	U_FRAME	    	(1<<6)
#define U_SIGNAL    	(1<<7)		// just differentiates from other updates

// svc_update can pass all of the fast update bits, plus more
#define	U_ANGLE1    	(1<<8)
#define	U_ANGLE3    	(1<<9)
#define	U_MODEL		    (1<<10)
#define	U_COLORMAP	    (1<<11)
#define	U_SKIN		    (1<<12)
#define	U_EFFECTS	    (1<<13)
#define	U_LONGENTITY	(1<<14) //qb: this is the last protocol 15 bit
#define U_EXTEND1		(1<<15)

#define U_ALPHA			(1<<16) // 1 byte, uses ENTALPHA_ENCODE, not sent if equal to baseline
#define U_FRAME2		(1<<17) // 1 byte, this is .frame & 0xFF00 (second byte)
#define U_MODEL2		(1<<18) // 1 byte, this is .modelindex & 0xFF00 (second byte)
#define U_GLOW_SIZE 	(1<<19) // short, -250 to 250.  qb: from Tomazquake
//open                 	(1<<21) //qb: for compatibility with byte stuffcmd (Marcher, grrr...)
#define U_GLOW_RED		(1<<20) //qb: 1 byte, 0 to 1 maps to 0 to 255
#define U_GLOW_GREEN	(1<<21) //qb: 1 byte
#define U_GLOW_BLUE 	(1<<22) //qb: 1 byte
#define U_EXTEND2		(1<<23) // another byte to follow

#define U_EFFECTS2		(1<<24) // qb: jump up to 4 bytes for DP effects
#define U_FRAMEINTERVAL	(1<<25) // Manoel Kasimier - QC frame_interval
#define U_VIEWMODEL		(1<<26) //qb: attaches the model to the view (origin and angles become relative to it), only shown to owner, a more powerful alternative to .weaponmodel and such
#define U_EXTERIORMODEL	(1<<27) //qb causes this model to not be drawn when using a first person view (third person will draw it, first person will not)
#define U_SCALE 		(1<<28) //qb: from Tomazquake
#define U_SCALEV       	(1<<29) //qb: from Tomazquake
//open              	(1<<30)
//open              	(1<<31)

#define	SU_VIEWHEIGHT	(1<<0)
#define	SU_IDEALPITCH	(1<<1)
#define	SU_PUNCH1		(1<<2)
#define	SU_PUNCH2		(1<<3)
#define	SU_PUNCH3		(1<<4)
#define	SU_VELOCITY1	(1<<5)
#define	SU_VELOCITY2	(1<<6)
#define	SU_VELOCITY3	(1<<7)
#define	SU_UNUSED8		(1<<8)  //AVAILABLE BIT
#define	SU_ITEMS		(1<<9)
#define	SU_ONGROUND		(1<<10)		// no data follows, the bit is it
#define	SU_INWATER		(1<<11)		// no data follows, the bit is it
#define	SU_WEAPONFRAME	(1<<12)
#define	SU_ARMOR		(1<<13)
#define	SU_WEAPON		(1<<14)

//qb: from johnfitz -- PROTOCOL_FITZQUAKE -- new bits
#define SU_EXTEND1		(1<<15) // another byte to follow
#define SU_WEAPON2		(1<<16) // 1 byte, this is .weaponmodel & 0xFF00 (second byte)
#define SU_ARMOR2		(1<<17) // 1 byte, this is .armorvalue & 0xFF00 (second byte)
#define SU_AMMO2		(1<<18) // 1 byte, this is .currentammo & 0xFF00 (second byte)
#define SU_SHELLS2		(1<<19) // 1 byte, this is .ammo_shells & 0xFF00 (second byte)
#define SU_NAILS2		(1<<20) // 1 byte, this is .ammo_nails & 0xFF00 (second byte)
#define SU_ROCKETS2		(1<<21) // 1 byte, this is .ammo_rockets & 0xFF00 (second byte)
#define SU_CELLS2		(1<<22) // 1 byte, this is .ammo_cells & 0xFF00 (second byte)
#define SU_EXTEND2		(1<<23) // another byte to follow
#define SU_WEAPONFRAME2	(1<<24) // 1 byte, this is .weaponframe & 0xFF00 (second byte)
#define SU_WEAPONALPHA	(1<<25) // 1 byte, this is alpha for weaponmodel, uses ENTALPHA_ENCODE, not sent if ENTALPHA_DEFAULT
#define SU_UNUSED26		(1<<26)
#define SU_UNUSED27		(1<<27)
#define SU_UNUSED28		(1<<28)
#define SU_UNUSED29		(1<<29)
#define SU_UNUSED30		(1<<30)
#define SU_EXTEND3		(1<<31) // another byte to follow, future expansion
//johnfitz
// a sound with no channel is a local only sound
#define	SND_VOLUME		(1<<0)		// a byte
#define	SND_ATTENUATION	(1<<1)		// a byte
#define	SND_LOOPING		(1<<2)		// a long

//qb:  johnfitz begin
// -- new bits
#define	SND_LARGEENTITY	(1<<3)	// a short + byte (instead of just a short)
#define	SND_LARGESOUND	(1<<4)	// a short soundindex (instead of a byte)

//johnfitz -- PROTOCOL_FITZQUAKE -- flags for entity baseline messages
#define B_LARGEMODEL	(1<<0)	// modelindex is short instead of byte
#define B_LARGEFRAME	(1<<1)	// frame is short instead of byte
#define B_ALPHA			(1<<2)	// 1 byte, uses ENTALPHA_ENCODE, not sent if ENTALPHA_DEFAULT
//johnfitz

//johnfitz -- PROTOCOL_FITZQUAKE -- alpha encoding
#define ENTALPHA_DEFAULT	0	//entity's alpha is "default" (i.e. water obeys r_wateralpha) -- must be zero so zeroed out memory works
#define ENTALPHA_ZERO		1	//entity is invisible (lowest possible alpha)
#define ENTALPHA_ENCODE(a)	(((a)==0)?ENTALPHA_DEFAULT:Q_rint(CLAMP(1,(a)*254.0f+1,255))) //server convert to byte to send to client
#define ENTALPHA_DECODE(a)	(((a)==ENTALPHA_DEFAULT)?1.0f:((float)(a)-1)/(254)) //client convert to float for rendering
#define ENTALPHA_TOSAVE(a)	(((a)==ENTALPHA_DEFAULT)?0.0f:(((a)==ENTALPHA_ZERO)?-1.0f:((float)(a)-1)/(254))) //server convert to float for savegame
//johnfitz

//qb:  johnfitz end

// defaults for clientinfo messages
#define	DEFAULT_VIEWHEIGHT	22


// game types sent by serverinfo
// these determine which intermission screen plays
#define	GAME_COOP			0
#define	GAME_DEATHMATCH		1

//==================
// note that there are some defs.qc that mirror to these numbers
// also related to svc_strings[] in cl_parse
//==================

//
// server to client
//
#define	svc_bad				0
#define	svc_nop				1
#define	svc_disconnect		2
#define	svc_updatestat		3	// [byte] [long]
#define	svc_version			4	// [long] server version
#define	svc_setview			5	// [short] entity number
#define	svc_sound			6	// <see code>
#define	svc_time			7	// [float] server time
#define	svc_print			8	// [string] null terminated string
#define	svc_stufftext		9	// [string] stuffed into client's console buffer
								// the string should be \n terminated
#define	svc_setangle		10	// [angle3] set the view angle to this absolute value

#define	svc_serverinfo		11	// [long] version
						// [string] signon string
						// [string]..[0]model cache
						// [string]...[0]sounds cache
#define	svc_lightstyle		12	// [byte] [string]
#define	svc_updatename		13	// [byte] [string]
#define	svc_updatefrags		14	// [byte] [short]
#define	svc_clientdata		15	// <shortbits + data>
#define	svc_stopsound		16	// <see code>
#define	svc_updatecolors	17	// [byte] [byte]
#define	svc_particle		18	// [vec3] <variable>
#define	svc_damage			19

#define	svc_spawnstatic		20
//#define svc_spawnbinary		21
#define	svc_spawnbaseline	22

#define	svc_temp_entity		23

#define	svc_setpause		24	// [byte] on / off
#define	svc_signonnum		25	// [byte]  used for the signon sequence

#define	svc_centerprint		26	// [string] to put in center of the screen

#define	svc_killedmonster	27
#define	svc_foundsecret		28

#define	svc_spawnstaticsound	29	// [coord3] [byte] samp [byte] vol [byte] aten

#define	svc_intermission	30		// [string] music
#define	svc_finale			31		// [string] music [string] text

#define	svc_cdtrack			32		// [byte] track [byte] looptrack
#define svc_sellscreen		33

#define svc_cutscene		34


//johnfitz -- PROTOCOL_FITZQUAKE -- new server messages
#define	svc_skybox				37	// [string] name
#define svc_bf					40  //bonus flash
#define svc_fog					41	// [byte] density [byte] red [byte] green [byte] blue [float] time
//johnfitz

#define svc_localsound          48  //qb: play sound to a single client, S_LocalSound
#define	svc_spawnstaticsound_large	49	//qb: [coord3] [short] samp [byte] vol [byte] aten
#define	svc_say				    50	//qb: TTS [string] null terminated string
#define	svc_spawnbaseline2      51

#define	svc_spawnbaseline3      54
#define	svc_spawnbaseline4      55

#define	svc_spawnstatic2        57
#define	svc_spawnstatic3        58

#define svc_letterbox		60	// Manoel Kasimier - svc_letterbox
#define svc_vibrate			61	// Manoel Kasimier


//
// client to server
//
#define	clc_bad			0
#define	clc_nop 		1
#define	clc_disconnect	2
#define	clc_move		3			// [usercmd_t]
#define	clc_stringcmd	4		// [string] message


//
// temp entity events
//
#define	TE_SPIKE			0
#define	TE_SUPERSPIKE		1
#define	TE_GUNSHOT			2
#define	TE_EXPLOSION		3
#define	TE_TAREXPLOSION		4
#define	TE_LIGHTNING1		5
#define	TE_LIGHTNING2		6
#define	TE_WIZSPIKE			7
#define	TE_KNIGHTSPIKE		8
#define	TE_LIGHTNING3		9
#define	TE_LAVASPLASH		10
#define	TE_TELEPORT			11
#define TE_EXPLOSION2		12

// PGM 01/21/97
#define TE_BEAM				13
// PGM 01/21/97

//qb: TODO additional TE events
//#define TE_IMPLOSION		14
#define TE_RAILTRAIL		15

