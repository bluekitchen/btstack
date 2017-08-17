///////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------//
//-------------------------------------------------------------------------------//
//-----------H----H--X----X-----CCCCC----22222----0000-----0000------11----------//
//----------H----H----X-X-----C--------------2---0----0---0----0--1--1-----------//
//---------HHHHHH-----X------C----------22222---0----0---0----0-----1------------//
//--------H----H----X--X----C----------2-------0----0---0----0-----1-------------//
//-------H----H---X-----X---CCCCC-----222222----0000-----0000----1111------------//
//-------------------------------------------------------------------------------//
//----------------------------------------------------- http://hxc2001.free.fr --//
///////////////////////////////////////////////////////////////////////////////////
// File : hxcmod.h
// Contains: a tiny mod player
//
// Written by: Jean François DEL NERO
//
// Change History (most recent first):
///////////////////////////////////////////////////////////////////////////////////

#ifndef __HXCMOD_H
#define __HXCMOD_H

#if defined __cplusplus
extern "C" {
#endif


#ifndef MODPLAY_DEF
#define MODPLAY_DEF

// Basic type
typedef unsigned char	muchar;
typedef signed   char   mchar;
typedef unsigned short	muint;
typedef          short	mint;
typedef unsigned long	mulong;

#define NUMMAXCHANNELS 32
#define MAXNOTES 12*12
#define SAMPLE_RATE 44100
//
// MOD file structures
//

#pragma pack(1)

typedef struct {
	muchar  name[22];
	muint   length;
	muchar  finetune;
	muchar  volume;
	muint   reppnt;
	muint   replen;
} sample;

typedef struct {
	muchar  sampperiod;
	muchar  period;
	muchar  sampeffect;
	muchar  effect;
} note;

typedef struct {
	muchar  title[20];
	sample  samples[31];
	muchar  length;
	muchar  protracker;
	muchar  patterntable[128];
	muchar  signature[4];
	muchar  speed;
} module;

#pragma pack()

//
// HxCMod Internal structures
//
typedef struct {
	mchar * sampdata;
	muint   sampnum;
	muint   length;
	muint   reppnt;
	muint   replen;
	mulong  samppos;
	muint   period;
	muchar  volume;
	mulong  ticks;
	muchar  effect;
	muchar  parameffect;
	muint   effect_code;


	mint    decalperiod;
	mint    portaspeed;
	mint    portaperiod;
	mint    vibraperiod;
	mint    Arpperiods[3];
	muchar  ArpIndex;

	mint    oldk;
	muchar  volumeslide;

	muchar  vibraparam;
	muchar  vibrapointeur;

	muchar  finetune;

	muchar  cut_param;

	muint   patternloopcnt;
	muint   patternloopstartpoint;
} hxcmod_channel;

typedef struct {
	module  song;
	mchar * sampledata[31];
	note *  patterndata[128];

	mulong  playrate;
	muint   tablepos;
	muint   patternpos;
	muint   patterndelay;
	muint   jump_loop_effect;
	muchar  bpm;
	mulong  patternticks;
	mulong  patterntickse;
	mulong  patternticksaim;
	mulong  sampleticksconst;

	mulong  samplenb;

	hxcmod_channel channels[NUMMAXCHANNELS];

	muint   number_of_channels;

	muint   fullperiod[MAXNOTES * 8];

	muint   mod_loaded;

	mint    last_r_sample;
	mint    last_l_sample;

	mint    stereo;
	mint    stereo_separation;
	mint    bits;
	mint    filter;

} modcontext;

//
// Player states structures
//
typedef struct track_state_
{
	unsigned char instrument_number;
	unsigned short cur_period;
	unsigned char  cur_volume;
	unsigned short cur_effect;
	unsigned short cur_parameffect;
}track_state;

typedef struct tracker_state_
{
	int number_of_tracks;
	int bpm;
	int speed;
	int cur_pattern;
	int cur_pattern_pos;
	int cur_pattern_table_pos;
	unsigned int buf_index;
	track_state tracks[32];
}tracker_state;

typedef struct tracker_state_instrument_
{
	char name[22];
	int active;
}tracker_state_instrument;

typedef struct tracker_buffer_state_
{
	int nb_max_of_state;
	int nb_of_state;
	int cur_rd_index;
	int sample_step;
	char name[64];
	tracker_state_instrument instruments[31];
	tracker_state * track_state_buf;
}tracker_buffer_state;

///////////////////////////////////////////////////////////////////////////////////
// HxCMOD Core API:
// -------------------------------------------
// int  hxcmod_init(modcontext * modctx)
//
// - Initialize the modcontext buffer. Must be called before doing anything else.
//   Return 1 if success. 0 in case of error.
// -------------------------------------------
// int  hxcmod_load( modcontext * modctx, void * mod_data, int mod_data_size )
//
// - "Load" a MOD from memory (from "mod_data" with size "mod_data_size").
//   Return 1 if success. 0 in case of error.
// -------------------------------------------
// void hxcmod_fillbuffer( modcontext * modctx, unsigned short * outbuffer, unsigned long nbsample, tracker_buffer_state * trkbuf )
//
// - Generate and return the next samples chunk to outbuffer.
//   nbsample specify the number of stereo 16bits samples you want.
//   The output format is signed 44100Hz 16-bit Stereo PCM samples.
//   The output buffer size in byte must be equal to ( nbsample * 2 * 2 ).
//   The optional trkbuf parameter can be used to get detailed status of the player. Put NULL/0 is unused.
// -------------------------------------------
// void hxcmod_unload( modcontext * modctx )
// - "Unload" / clear the player status.
// -------------------------------------------
///////////////////////////////////////////////////////////////////////////////////

int  hxcmod_init( modcontext * modctx );
int  hxcmod_setcfg( modcontext * modctx, int samplerate, int bits, int stereo, int stereo_separation, int filter);
int  hxcmod_load( modcontext * modctx, void * mod_data, int mod_data_size );
void hxcmod_fillbuffer( modcontext * modctx, unsigned short * outbuffer, unsigned long nbsample, tracker_buffer_state * trkbuf );
void hxcmod_unload( modcontext * modctx );

#endif


#if defined __cplusplus
}
#endif

#endif // __HXCMOD_H
