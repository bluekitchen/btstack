Local copy used for A2DP Source demo in BTstack.

Github repository: https://github.com/jfdelnero/HxCModPlayer
Thanks for providing this nice and compact implementation!

--------------------------------------------------------------------------------------
Original readme.txt
--------------------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------//
//-------------------------------------------------------------------------------//
//-----------H----H--X----X-----CCCCC----22222----0000-----0000------11----------//
//----------H----H----X-X-----C--------------2---0----0---0----0---1-1-----------//
//---------HHHHHH-----X------C----------22222---0----0---0----0-----1------------//
//--------H----H----X--X----C----------2-------0----0---0----0-----1-------------//
//-------H----H---X-----X---CCCCC-----222222----0000-----0000----1111------------//
//-------------------------------------------------------------------------------//
//----------------------------------------------------- http://hxc2001.free.fr --//
///////////////////////////////////////////////////////////////////////////////////

                                    HxCMOD player

The HxCMOD player is a tiny music module player.

It currently supports the Noisetracker/Soundtracker/Protracker Module Format (*.mod)

The core (hxcmod.c/hxcmod.h) is designed to have the least external dependency.
So it should be usable on almost all OS and systems.
You can use the hxcmod.c / hxcmod.h files to add a mod replay support
to a demo/game/software.

You are free to do what you want with this code.
(A credit is always appreciated if you include it into your prod ;) )

The test program is into the win32 folder. Just drag and drop a mod on the main
window to load it. Linux & Mac OS X version it planned.

Please note that this core was "Emscriptened" successfully and is now working in
JavaScript with Android, Chrome, Firefox, Edge, Safari browsers and probably
with others browsers supporting the Web Audio API support.

You can test it at this address : http://hxc2001.free.fr/hxcmod/

The native Mod player demonstration video is available on youtube :
https://www.youtube.com/watch?v=MEU9FGZzjac

Another video showing the player working on a STM32F105 microcontroller based device :
https://www.youtube.com/watch?v=kiOT8-zWVkA

Another case, this time with an STM32 microcontroller without DAC hardware. 
The firmware implements a 1 bit delta sigma / PDM stream generator :
https://www.youtube.com/watch?v=AQ--IiXPUGA

--------------------------------------------------------------------------------------
 HxCMOD Core API
--------------------------------------------------------------------------------------

int  hxcmod_init( modcontext * modctx )

- Initialize the modcontext buffer. Must be called before doing anything else.
  Return 1 if success. 0 in case of error.

int  hxcmod_setcfg( modcontext * modctx, int samplerate, int stereo_separation, int filter);

- Configure the player :
  samplerate specify the sample rate. (44100 by default).
  stereo_separation - Left/Right channel separation.
  filter - if non null, the filter is applied (default)

int  hxcmod_load( modcontext * modctx, void * mod_data, int mod_data_size )

- "Load" a MOD from memory (from "mod_data" with size "mod_data_size").
  Return 1 if success. 0 in case of error.


void hxcmod_fillbuffer( modcontext * modctx, unsigned short * outbuffer, unsigned long nbsample, tracker_buffer_state * trkbuf )

- Generate and return the next samples chunk to outbuffer.
  nbsample specify the number of stereo 16bits samples you want.
  The default output format is signed 44100Hz 16-bit Stereo PCM samples.
  The output format can be changed with the C flags HXCMOD_MONO_OUTPUT, HXCMOD_8BITS_OUTPUT and HXCMOD_UNSIGNED_OUTPUT.
  The output buffer size in byte must be equal to ( nbsample * sample_size * (mono=1 or stereo=2) ).
  The optional trkbuf parameter can be used to get detailed status of the player. Put NULL/0 if unused.


void hxcmod_unload( modcontext * modctx )
- "Unload" / clear the player status.

Compile-time C defines options :

HXCMOD_MONO_OUTPUT       : Turn the output format in mono mode.
HXCMOD_8BITS_OUTPUT      : Set the output wave format in 8bits.
HXCMOD_UNSIGNED_OUTPUT   : Set the output wave format unsigned.
HXCMOD_MAXCHANNELS       : Set the maximum supported channels (default : 32).
                           Reduce it to gain some RAM space.
                           Increase it if you want to support some specials mods
                           (up to 999).
HXCMOD_BIGENDIAN_MACHINE : The target machine is big endian.
HXCMOD_SLOW_TARGET       : For slow targets : Disable stereo mixing and output filter.
HXCMOD_USE_PRECALC_VOLUME_TABLE : Use precalculated volume table for the mixing.
                                  suppress the multiply operations for slow targets.
                                  The table need 32KB of RAM.
--------------------------------------------------------------------------------------
 Files on the repository
--------------------------------------------------------------------------------------

- hxcmod.c / hxcmod.h
The HxC core mod replay routines. These files don't have any dependency with others files
and can be used into other project.

- framegenerator.c / framegenerator.h
Generate a 640*480 framebuffer from the player status to be displayed in real-time.
(not needed by the core)

- win32/
The Windows test software.
(linux & Mac version planned)

- js_emscripten/
The Web browser/JavaScript version. (Build with Emscripten)

- STM32/
STM32 microcontroller / "Gotek" demo.

- packer/
Data compression utility. Used to embed one mod and some graphical stuff into the executable.
Not directly used by the core.

- data/
Some packed data files.

--------------------------------------------------------------------------------------

Jean-François DEL NERO (Jeff) / HxC2001

Email : jeanfrancoisdelnero <> free.fr

http://hxc2001.free.fr

11 July 2015
