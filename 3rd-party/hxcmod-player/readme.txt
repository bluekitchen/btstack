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

A video demo of the native Mod player can be seen on youtube :
https://www.youtube.com/watch?v=MEU9FGZzjac

--------------------------------------------------------------------------------------
 HxCMOD Core API
--------------------------------------------------------------------------------------

int  hxcmod_init( modcontext * modctx )

- Initialize the modcontext buffer. Must be called before doing anything else.
  Return 1 if success. 0 in case of error.

int  hxcmod_setcfg( modcontext * modctx, int samplerate, int bits, int stereo, int stereo_separation, int filter);

- Configure the player :
  samplerate specify the sample rate. (44100 by default).
  bits specify the number of bits (16 bits only for the moment).
  stereo - if non null, the stereo mode is selected (default)
  stereo_separation - Left/Right channel separation.
  filter - if non null, the filter is applied (default)

int  hxcmod_load( modcontext * modctx, void * mod_data, int mod_data_size )

- "Load" a MOD from memory (from "mod_data" with size "mod_data_size").
  Return 1 if success. 0 in case of error.


void hxcmod_fillbuffer( modcontext * modctx, unsigned short * outbuffer, unsigned long nbsample, tracker_buffer_state * trkbuf )

- Generate and return the next samples chunk to outbuffer.
  nbsample specify the number of stereo 16bits samples you want.
  The output format is signed 44100Hz 16-bit Stereo PCM samples.
  The output buffer size in byte must be equal to ( nbsample * 2 * 2 ).
  The optional trkbuf parameter can be used to get detailed status of the player. Put NULL/0 if unused.


void hxcmod_unload( modcontext * modctx )
- "Unload" / clear the player status.


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
