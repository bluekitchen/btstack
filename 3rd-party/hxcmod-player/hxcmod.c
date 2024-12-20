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
// File : hxcmod.c
// Contains: a tiny mod player
//
// Written by: Jean Francois DEL NERO
//
// You are free to do what you want with this code.
// A credit is always appreciated if you include it into your prod :)
//
// This file include some parts of the Noisetracker/Soundtracker/Protracker
// Module Format documentation written by Andrew Scott (Adrenalin Software)
// (modformat.txt)
//
// The core (hxcmod.c/hxcmod.h) is designed to have the least external dependency.
// So it should be usable on almost all OS and systems.
// Please also note that no dynamic allocation is done into the HxCMOD core.
//
// Change History (most recent first):
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
// void hxcmod_fillbuffer( modcontext * modctx, unsigned short * outbuffer, mssize nbsample, tracker_buffer_state * trkbuf )
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

#include "hxcmod.h"

// BK4BSTACK_CHANGE START

// MODs ar either in ROM (MCUs) or in .text segment
#define HXCMOD_MOD_FILE_IN_ROM

#ifdef __GNUC__
#ifndef	__clang__
#pragma GCC diagnostic push
// -Wstringop-overflow unknown to Clang 16.0.0
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif
#endif

// BK4BSTACK_CHANGE END

///////////////////////////////////////////////////////////////////////////////////

// Effects list
#define EFFECT_ARPEGGIO              0x0 // Supported
#define EFFECT_PORTAMENTO_UP         0x1 // Supported
#define EFFECT_PORTAMENTO_DOWN       0x2 // Supported
#define EFFECT_TONE_PORTAMENTO       0x3 // Supported
#define EFFECT_VIBRATO               0x4 // Supported
#define EFFECT_VOLSLIDE_TONEPORTA    0x5 // Supported
#define EFFECT_VOLSLIDE_VIBRATO      0x6 // Supported
#define EFFECT_VOLSLIDE_TREMOLO      0x7 // - TO BE DONE -
#define EFFECT_SET_PANNING           0x8 // - TO BE DONE -
#define EFFECT_SET_OFFSET            0x9 // Supported
#define EFFECT_VOLUME_SLIDE          0xA // Supported
#define EFFECT_JUMP_POSITION         0xB // Supported
#define EFFECT_SET_VOLUME            0xC // Supported
#define EFFECT_PATTERN_BREAK         0xD // Supported

#define EFFECT_EXTENDED              0xE
#define EFFECT_E_FINE_PORTA_UP       0x1 // Supported
#define EFFECT_E_FINE_PORTA_DOWN     0x2 // Supported
#define EFFECT_E_GLISSANDO_CTRL      0x3 // - TO BE DONE -
#define EFFECT_E_VIBRATO_WAVEFORM    0x4 // - TO BE DONE -
#define EFFECT_E_SET_FINETUNE        0x5 // Supported
#define EFFECT_E_PATTERN_LOOP        0x6 // Supported
#define EFFECT_E_TREMOLO_WAVEFORM    0x7 // - TO BE DONE -
#define EFFECT_E_SET_PANNING_2       0x8 // - TO BE DONE -
#define EFFECT_E_RETRIGGER_NOTE      0x9 // Supported
#define EFFECT_E_FINE_VOLSLIDE_UP    0xA // Supported
#define EFFECT_E_FINE_VOLSLIDE_DOWN  0xB // Supported
#define EFFECT_E_NOTE_CUT            0xC // Supported
#define EFFECT_E_NOTE_DELAY          0xD // Supported
#define EFFECT_E_PATTERN_DELAY       0xE // Supported
#define EFFECT_E_INVERT_LOOP         0xF // Supported (W.I.P)
#define EFFECT_SET_SPEED             0xF0 // Supported
#define EFFECT_SET_TEMPO             0xF2 // Supported

#define PERIOD_TABLE_LENGTH  MAXNOTES


//
// Finetuning periods -> Amiga period * 2^(-finetune/12/8)
//

static const short periodtable[]=
{
	// Finetune 0 (* 1.000000), Offset 0x0000
	27392, 25856, 24384, 23040, 21696, 20480, 19328, 18240, 17216, 16256, 15360, 14496,
	13696, 12928, 12192, 11520, 10848, 10240,  9664,  9120,  8606,  8128,  7680,  7248,
	 6848,  6464,  6096,  5760,  5424,  5120,  4832,  4560,  4304,  4064,  3840,  3624,
	 3424,  3232,  3048,  2880,  2712,  2560,  2416,  2280,  2152,  2032,  1920,  1812,
	 1712,  1616,  1524,  1440,  1356,  1280,  1208,  1140,  1076,  1016,   960,   906,
	  856,   808,   762,   720,   678,   640,   604,   570,   538,   508,   480,   453,
	  428,   404,   381,   360,   339,   320,   302,   285,   269,   254,   240,   226,
	  214,   202,   190,   180,   170,   160,   151,   143,   135,   127,   120,   113,
	  107,   101,    95,    90,    85,    80,    75,    71,    67,    63,    60,    56,
	   53,    50,    47,    45,    42,    40,    37,    35,    33,    31,    30,    28,
	   27,    25,    24,    22,    21,    20,    19,    18,    17,    16,    15,    14,
	   13,    13,    12,    11,    11,    10,     9,     9,     8,     8,     7,     7,

	// Finetune 1 (* 0.992806), Offset 0x0120
	27195, 25670, 24209, 22874, 21540, 20333, 19189, 18109, 17092, 16139, 15249, 14392,
	13597, 12835, 12104, 11437, 10770, 10166,  9594,  9054,  8544,  8070,  7625,  7196,
	 6799,  6417,  6052,  5719,  5385,  5083,  4797,  4527,  4273,  4035,  3812,  3598,
	 3399,  3209,  3026,  2859,  2692,  2542,  2399,  2264,  2137,  2017,  1906,  1799,
	 1700,  1604,  1513,  1430,  1346,  1271,  1199,  1132,  1068,  1009,   953,   899,
	  850,   802,   757,   715,   673,   635,   600,   566,   534,   504,   477,   450,
	  425,   401,   378,   357,   337,   318,   300,   283,   267,   252,   238,   224,
	  212,   201,   189,   179,   169,   159,   150,   142,   134,   126,   119,   112,
	  106,   100,    94,    89,    84,    79,    74,    70,    67,    63,    60,    56,
	   53,    50,    47,    45,    42,    40,    37,    35,    33,    31,    30,    28,
	   27,    25,    24,    22,    21,    20,    19,    18,    17,    16,    15,    14,
	   13,    13,    12,    11,    11,    10,     9,     9,     8,     8,     7,     7,

	// Finetune 2 (* 0.985663), Offset 0x0240
	26999, 25485, 24034, 22710, 21385, 20186, 19051, 17978, 16969, 16023, 15140, 14288,
	13500, 12743, 12017, 11355, 10692, 10093,  9525,  8989,  8483,  8011,  7570,  7144,
	 6750,  6371,  6009,  5677,  5346,  5047,  4763,  4495,  4242,  4006,  3785,  3572,
	 3375,  3186,  3004,  2839,  2673,  2523,  2381,  2247,  2121,  2003,  1892,  1786,
	 1687,  1593,  1502,  1419,  1337,  1262,  1191,  1124,  1061,  1001,   946,   893,
	  844,   796,   751,   710,   668,   631,   595,   562,   530,   501,   473,   447,
	  422,   398,   376,   355,   334,   315,   298,   281,   265,   250,   237,   223,
	  211,   199,   187,   177,   168,   158,   149,   141,   133,   125,   118,   111,
	  105,   100,    94,    89,    84,    79,    74,    70,    66,    62,    59,    55,
	   52,    49,    46,    44,    41,    39,    36,    34,    33,    31,    30,    28,
	   27,    25,    24,    22,    21,    20,    19,    18,    17,    16,    15,    14,
	   13,    13,    12,    11,    11,    10,     9,     9,     8,     8,     7,     7,

	// Finetune 3 (* 0.978572), Offset 0x0360
	26805, 25302, 23862, 22546, 21231, 20041, 18914, 17849, 16847, 15908, 15031, 14185,
	13403, 12651, 11931, 11273, 10616, 10021,  9457,  8925,  8422,  7954,  7515,  7093,
	 6701,  6325,  5965,  5637,  5308,  5010,  4728,  4462,  4212,  3977,  3758,  3546,
	 3351,  3163,  2983,  2818,  2654,  2505,  2364,  2231,  2106,  1988,  1879,  1773,
	 1675,  1581,  1491,  1409,  1327,  1253,  1182,  1116,  1053,   994,   939,   887,
	  838,   791,   746,   705,   663,   626,   591,   558,   526,   497,   470,   443,
	  419,   395,   373,   352,   332,   313,   296,   279,   263,   249,   235,   221,
	  209,   198,   186,   176,   166,   157,   148,   140,   132,   124,   117,   111,
	  105,    99,    93,    88,    83,    78,    73,    69,    66,    62,    59,    55,
	   52,    49,    46,    44,    41,    39,    36,    34,    32,    30,    29,    27,
	   26,    24,    23,    22,    21,    20,    19,    18,    17,    16,    15,    14,
	   13,    13,    12,    11,    11,    10,     9,     9,     8,     8,     7,     7,

	// Finetune 4 (* 0.971532), Offset 0x0480
	26612, 25120, 23690, 22384, 21078, 19897, 18778, 17721, 16726, 15793, 14923, 14083,
	13306, 12560, 11845, 11192, 10539,  9948,  9389,  8860,  8361,  7897,  7461,  7042,
	 6653,  6280,  5922,  5596,  5270,  4974,  4694,  4430,  4181,  3948,  3731,  3521,
	 3327,  3140,  2961,  2798,  2635,  2487,  2347,  2215,  2091,  1974,  1865,  1760,
	 1663,  1570,  1481,  1399,  1317,  1244,  1174,  1108,  1045,   987,   933,   880,
	  832,   785,   740,   700,   659,   622,   587,   554,   523,   494,   466,   440,
	  416,   392,   370,   350,   329,   311,   293,   277,   261,   247,   233,   220,
	  208,   196,   185,   175,   165,   155,   147,   139,   131,   123,   117,   110,
	  104,    98,    92,    87,    83,    78,    73,    69,    65,    61,    58,    54,
	   51,    49,    46,    44,    41,    39,    36,    34,    32,    30,    29,    27,
	   26,    24,    23,    21,    20,    19,    18,    17,    17,    16,    15,    14,
	   13,    13,    12,    11,    11,    10,     9,     9,     8,     8,     7,     7,

	// Finetune 5 (* 0.964542), Offset 0x05a0
	26421, 24939, 23519, 22223, 20927, 19754, 18643, 17593, 16606, 15680, 14815, 13982,
	13210, 12470, 11760, 11112, 10463,  9877,  9321,  8797,  8301,  7840,  7408,  6991,
	 6605,  6235,  5880,  5556,  5232,  4938,  4661,  4398,  4151,  3920,  3704,  3496,
	 3303,  3117,  2940,  2778,  2616,  2469,  2330,  2199,  2076,  1960,  1852,  1748,
	 1651,  1559,  1470,  1389,  1308,  1235,  1165,  1100,  1038,   980,   926,   874,
	  826,   779,   735,   694,   654,   617,   583,   550,   519,   490,   463,   437,
	  413,   390,   367,   347,   327,   309,   291,   275,   259,   245,   231,   218,
	  206,   195,   183,   174,   164,   154,   146,   138,   130,   122,   116,   109,
	  103,    97,    92,    87,    82,    77,    72,    68,    65,    61,    58,    54,
	   51,    48,    45,    43,    41,    39,    36,    34,    32,    30,    29,    27,
	   26,    24,    23,    21,    20,    19,    18,    17,    16,    15,    14,    14,
	   13,    13,    12,    11,    11,    10,     9,     9,     8,     8,     7,     7,

	// Finetune 6 (* 0.957603), Offset 0x06c0
	26231, 24760, 23350, 22063, 20776, 19612, 18509, 17467, 16486, 15567, 14709, 13881,
	13115, 12380, 11675, 11032, 10388,  9806,  9254,  8733,  8241,  7783,  7354,  6941,
	 6558,  6190,  5838,  5516,  5194,  4903,  4627,  4367,  4122,  3892,  3677,  3470,
	 3279,  3095,  2919,  2758,  2597,  2451,  2314,  2183,  2061,  1946,  1839,  1735,
	 1639,  1547,  1459,  1379,  1299,  1226,  1157,  1092,  1030,   973,   919,   868,
	  820,   774,   730,   689,   649,   613,   578,   546,   515,   486,   460,   434,
	  410,   387,   365,   345,   325,   306,   289,   273,   258,   243,   230,   216,
	  205,   193,   182,   172,   163,   153,   145,   137,   129,   122,   115,   108,
	  102,    97,    91,    86,    81,    77,    72,    68,    64,    60,    57,    54,
	   51,    48,    45,    43,    40,    38,    35,    34,    32,    30,    29,    27,
	   26,    24,    23,    21,    20,    19,    18,    17,    16,    15,    14,    13,
	   12,    12,    11,    11,    11,    10,     9,     9,     8,     8,     7,     7,

	// Finetune 7 (* 0.950714), Offset 0x07e0
	26042, 24582, 23182, 21904, 20627, 19471, 18375, 17341, 16367, 15455, 14603, 13782,
	13021, 12291, 11591, 10952, 10313,  9735,  9188,  8671,  8182,  7727,  7301,  6891,
	 6510,  6145,  5796,  5476,  5157,  4868,  4594,  4335,  4092,  3864,  3651,  3445,
	 3255,  3073,  2898,  2738,  2578,  2434,  2297,  2168,  2046,  1932,  1825,  1723,
	 1628,  1536,  1449,  1369,  1289,  1217,  1148,  1084,  1023,   966,   913,   861,
	  814,   768,   724,   685,   645,   608,   574,   542,   511,   483,   456,   431,
	  407,   384,   362,   342,   322,   304,   287,   271,   256,   241,   228,   215,
	  203,   192,   181,   171,   162,   152,   144,   136,   128,   121,   114,   107,
	  102,    96,    90,    86,    81,    76,    71,    68,    64,    60,    57,    53,
	   50,    48,    45,    43,    40,    38,    35,    33,    31,    29,    29,    27,
	   26,    24,    23,    21,    20,    19,    18,    17,    16,    15,    14,    13,
	   12,    12,    11,    10,    10,    10,     9,     9,     8,     8,     7,     7,

	// Finetune -8 (* 1.059463), Offset 0x0900
	29021, 27393, 25834, 24410, 22986, 21698, 20477, 19325, 18240, 17223, 16273, 15358,
	14510, 13697, 12917, 12205, 11493, 10849, 10239,  9662,  9118,  8611,  8137,  7679,
	 7255,  6848,  6458,  6103,  5747,  5424,  5119,  4831,  4560,  4306,  4068,  3839,
	 3628,  3424,  3229,  3051,  2873,  2712,  2560,  2416,  2280,  2153,  2034,  1920,
	 1814,  1712,  1615,  1526,  1437,  1356,  1280,  1208,  1140,  1076,  1017,   960,
	  907,   856,   807,   763,   718,   678,   640,   604,   570,   538,   509,   480,
	  453,   428,   404,   381,   359,   339,   320,   302,   285,   269,   254,   239,
	  227,   214,   201,   191,   180,   170,   160,   152,   143,   135,   127,   120,
	  113,   107,   101,    95,    90,    85,    79,    75,    71,    67,    64,    59,
	   56,    53,    50,    48,    44,    42,    39,    37,    35,    33,    32,    30,
	   29,    26,    25,    23,    22,    21,    20,    19,    18,    17,    16,    15,
	   14,    14,    13,    12,    12,    11,    10,    10,     8,     8,     7,     7,

	// Finetune -7 (* 1.051841), Offset 0x0a20
	28812, 27196, 25648, 24234, 22821, 21542, 20330, 19186, 18108, 17099, 16156, 15247,
	14406, 13598, 12824, 12117, 11410, 10771, 10165,  9593,  9052,  8549,  8078,  7624,
	 7203,  6799,  6412,  6059,  5705,  5385,  5082,  4796,  4527,  4275,  4039,  3812,
	 3602,  3400,  3206,  3029,  2853,  2693,  2541,  2398,  2264,  2137,  2020,  1906,
	 1801,  1700,  1603,  1515,  1426,  1346,  1271,  1199,  1132,  1069,  1010,   953,
	  900,   850,   802,   757,   713,   673,   635,   600,   566,   534,   505,   476,
	  450,   425,   401,   379,   357,   337,   318,   300,   283,   267,   252,   238,
	  225,   212,   200,   189,   179,   168,   159,   150,   142,   134,   126,   119,
	  113,   106,   100,    95,    89,    84,    79,    75,    70,    66,    63,    59,
	   56,    53,    49,    47,    44,    42,    39,    37,    35,    33,    32,    29,
	   28,    26,    25,    23,    22,    21,    20,    19,    18,    17,    16,    15,
	   14,    14,    13,    12,    12,    11,     9,     9,     8,     8,     7,     7,

	// Finetune -6 (* 1.044274), Offset 0x0b40
	28605, 27001, 25464, 24060, 22657, 21387, 20184, 19048, 17978, 16976, 16040, 15138,
	14302, 13500, 12732, 12030, 11328, 10693, 10092,  9524,  8987,  8488,  8020,  7569,
	 7151,  6750,  6366,  6015,  5664,  5347,  5046,  4762,  4495,  4244,  4010,  3784,
	 3576,  3375,  3183,  3008,  2832,  2673,  2523,  2381,  2247,  2122,  2005,  1892,
	 1788,  1688,  1591,  1504,  1416,  1337,  1261,  1190,  1124,  1061,  1003,   946,
	  894,   844,   796,   752,   708,   668,   631,   595,   562,   530,   501,   473,
	  447,   422,   398,   376,   354,   334,   315,   298,   281,   265,   251,   236,
	  223,   211,   198,   188,   178,   167,   158,   149,   141,   133,   125,   118,
	  112,   105,    99,    94,    89,    84,    78,    74,    70,    66,    63,    58,
	   55,    52,    49,    47,    44,    42,    39,    37,    34,    32,    31,    29,
	   28,    26,    25,    23,    22,    21,    20,    19,    18,    17,    16,    15,
	   14,    14,    13,    11,    11,    10,     9,     9,     8,     8,     7,     7,

	// Finetune -5 (* 1.036761), Offset 0x0c60
	28399, 26806, 25280, 23887, 22494, 21233, 20039, 18911, 17849, 16854, 15925, 15029,
	14199, 13403, 12640, 11943, 11247, 10616, 10019,  9455,  8922,  8427,  7962,  7514,
	 7100,  6702,  6320,  5972,  5623,  5308,  5010,  4728,  4462,  4213,  3981,  3757,
	 3550,  3351,  3160,  2986,  2812,  2654,  2505,  2364,  2231,  2107,  1991,  1879,
	 1775,  1675,  1580,  1493,  1406,  1327,  1252,  1182,  1116,  1053,   995,   939,
	  887,   838,   790,   746,   703,   664,   626,   591,   558,   527,   498,   470,
	  444,   419,   395,   373,   351,   332,   313,   295,   279,   263,   249,   234,
	  222,   209,   197,   187,   176,   166,   157,   148,   140,   132,   124,   117,
	  111,   105,    98,    93,    88,    83,    78,    74,    69,    65,    62,    58,
	   55,    52,    49,    47,    44,    41,    38,    36,    34,    32,    31,    29,
	   28,    26,    25,    23,    22,    21,    20,    19,    18,    17,    16,    15,
	   13,    13,    12,    11,    11,    10,     9,     9,     8,     8,     7,     7,

	// Finetune -4 (* 1.029302), Offset 0x0d80
	28195, 26614, 25099, 23715, 22332, 21080, 19894, 18774, 17720, 16732, 15810, 14921,
	14097, 13307, 12549, 11858, 11166, 10540,  9947,  9387,  8858,  8366,  7905,  7460,
	 7049,  6653,  6275,  5929,  5583,  5270,  4974,  4694,  4430,  4183,  3953,  3730,
	 3524,  3327,  3137,  2964,  2791,  2635,  2487,  2347,  2215,  2092,  1976,  1865,
	 1762,  1663,  1569,  1482,  1396,  1318,  1243,  1173,  1108,  1046,   988,   933,
	  881,   832,   784,   741,   698,   659,   622,   587,   554,   523,   494,   466,
	  441,   416,   392,   371,   349,   329,   311,   293,   277,   261,   247,   233,
	  220,   208,   196,   185,   175,   165,   155,   147,   139,   131,   124,   116,
	  110,   104,    98,    93,    87,    82,    77,    73,    69,    65,    62,    58,
	   55,    51,    48,    46,    43,    41,    38,    36,    34,    32,    31,    29,
	   28,    26,    25,    23,    22,    21,    20,    19,    17,    16,    15,    14,
	   13,    13,    12,    11,    11,    10,     9,     9,     8,     8,     7,     7,

	// Finetune -3 (* 1.021897), Offset 0x0ea0
	27992, 26422, 24918, 23545, 22171, 20928, 19751, 18639, 17593, 16612, 15696, 14813,
	13996, 13211, 12459, 11772, 11086, 10464,  9876,  9320,  8794,  8306,  7848,  7407,
	 6998,  6606,  6229,  5886,  5543,  5232,  4938,  4660,  4398,  4153,  3924,  3703,
	 3499,  3303,  3115,  2943,  2771,  2616,  2469,  2330,  2199,  2076,  1962,  1852,
	 1749,  1651,  1557,  1472,  1386,  1308,  1234,  1165,  1100,  1038,   981,   926,
	  875,   826,   779,   736,   693,   654,   617,   582,   550,   519,   491,   463,
	  437,   413,   389,   368,   346,   327,   309,   291,   275,   260,   245,   231,
	  219,   206,   194,   184,   174,   164,   154,   146,   138,   130,   123,   115,
	  109,   103,    97,    92,    87,    82,    77,    73,    68,    64,    61,    57,
	   54,    51,    48,    46,    43,    41,    38,    36,    34,    32,    31,    29,
	   28,    26,    25,    22,    21,    20,    19,    18,    17,    16,    15,    14,
	   13,    13,    12,    11,    11,    10,     9,     9,     8,     8,     7,     7,

	// Finetune -2 (* 1.014545), Offset 0x0fc0
	27790, 26232, 24739, 23375, 22012, 20778, 19609, 18505, 17466, 16492, 15583, 14707,
	13895, 13116, 12369, 11688, 11006, 10389,  9805,  9253,  8731,  8246,  7792,  7353,
	 6948,  6558,  6185,  5844,  5503,  5194,  4902,  4626,  4367,  4123,  3896,  3677,
	 3474,  3279,  3092,  2922,  2751,  2597,  2451,  2313,  2183,  2062,  1948,  1838,
	 1737,  1640,  1546,  1461,  1376,  1299,  1226,  1157,  1092,  1031,   974,   919,
	  868,   820,   773,   730,   688,   649,   613,   578,   546,   515,   487,   460,
	  434,   410,   387,   365,   344,   325,   306,   289,   273,   258,   243,   229,
	  217,   205,   193,   183,   172,   162,   153,   145,   137,   129,   122,   115,
	  109,   102,    96,    91,    86,    81,    76,    72,    68,    64,    61,    57,
	   54,    51,    48,    46,    43,    41,    38,    36,    33,    31,    30,    28,
	   27,    25,    24,    22,    21,    20,    19,    18,    17,    16,    15,    14,
	   13,    13,    12,    11,    11,    10,     9,     9,     8,     8,     7,     7,

	// Finetune -1 (* 1.007246), Offset 0x10e0
	27590, 26043, 24561, 23207, 21853, 20628, 19468, 18372, 17341, 16374, 15471, 14601,
	13795, 13022, 12280, 11603, 10927, 10314,  9734,  9186,  8668,  8187,  7736,  7301,
	 6898,  6511,  6140,  5802,  5463,  5157,  4867,  4593,  4335,  4093,  3868,  3650,
	 3449,  3255,  3070,  2901,  2732,  2579,  2434,  2297,  2168,  2047,  1934,  1825,
	 1724,  1628,  1535,  1450,  1366,  1289,  1217,  1148,  1084,  1023,   967,   913,
	  862,   814,   768,   725,   683,   645,   608,   574,   542,   512,   483,   456,
	  431,   407,   384,   363,   341,   322,   304,   287,   271,   256,   242,   228,
	  216,   203,   191,   181,   171,   161,   152,   144,   136,   128,   121,   114,
	  108,   102,    96,    91,    86,    81,    76,    72,    67,    63,    60,    56,
	   53,    50,    47,    45,    42,    40,    37,    35,    33,    31,    30,    28,
	   27,    25,    24,    22,    21,    20,    19,    18,    17,    16,    15,    14,
	   13,    13,    12,    11,    11,    10,     9,     9,     8,     8,     7,     7
};

static const short * periodtable_finetune_ptr[]=
{
	&periodtable[0x0000], &periodtable[0x0090], &periodtable[0x0120], &periodtable[0x01B0],
	&periodtable[0x0240], &periodtable[0x02D0], &periodtable[0x0360], &periodtable[0x03F0],
	&periodtable[0x0480], &periodtable[0x0510], &periodtable[0x05A0], &periodtable[0x0630],
	&periodtable[0x06C0], &periodtable[0x0750], &periodtable[0x07E0], &periodtable[0x0870]
};

static const short sintable[]={
	  0,  24,  49,  74,  97, 120, 141, 161,
	180, 197, 212, 224, 235, 244, 250, 253,
	255, 253, 250, 244, 235, 224, 212, 197,
	180, 161, 141, 120,  97,  74,  49,  24
};

static const muchar InvertLoopTable[]={
	  0,   5,   6,   7,   8,  10,  11, 13,
	 16,  19,  22,  26,  32,  43,  64, 128
};

typedef struct modtype_
{
	unsigned char signature[5];
	int numberofchannels;
}modtype;

static modtype modlist[]=
{
	{ "M!K!",4},
	{ "M.K.",4},
	{ "M&K!",4},
	{ "PATT",4},
	{ "NSMS",4},
	{ "LARD",4},
	{ "FEST",4},
	{ "FIST",4},
	{ "N.T.",4},
	{ "OKTA",8},
	{ "OCTA",8},
	{ "$CHN",-1},
	{ "$$CH",-1},
	{ "$$CN",-1},
	{ "$$$C",-1},
	{ "FLT$",-1},
	{ "EXO$",-1},
	{ "CD$1",-1},
	{ "TDZ$",-1},
	{ "FA0$",-1},
	{ "",0}
};

#ifdef HXCMOD_BIGENDIAN_MACHINE

#define GET_BGI_W( big_endian_word ) ( big_endian_word )

#else

#define GET_BGI_W( big_endian_word ) ( (big_endian_word >> 8) | ((big_endian_word&0xFF) << 8) )

#endif


///////////////////////////////////////////////////////////////////////////////////

static void memcopy( void * dest, void *source, unsigned long size )
{
	unsigned long i;
	unsigned char * d,*s;

	d=(unsigned char*)dest;
	s=(unsigned char*)source;
	for(i=0;i<size;i++)
	{
		d[i]=s[i];
	}
}

static void memclear( void * dest, unsigned char value, unsigned long size )
{
	unsigned long i;
	unsigned char * d;

	d = (unsigned char*)dest;
	for(i=0;i<size;i++)
	{
		d[i]=value;
	}
}

static int getnote( modcontext * mod, unsigned short period )
{
// BK4BSTACK_CHANGE START
	(void) mod;
// BK4BSTACK_CHANGE END

	int i;
	const short * ptr;

	ptr = periodtable_finetune_ptr[0];

	for(i = 0; i < MAXNOTES; i++)
	{
		if(period >= ptr[i])
		{
			return i;
		}
	}

	return MAXNOTES;
}

static void doFunk(hxcmod_channel_t * cptr)
{
	if(cptr->funkspeed)
	{
		cptr->funkoffset += InvertLoopTable[cptr->funkspeed];
		if( cptr->funkoffset > 128 )
		{
			cptr->funkoffset = 0;
			if( cptr->sampdata && cptr->length && (cptr->replen > 1) )
			{
				if( ( (cptr->samppos) >> 11 ) >= (unsigned long)(cptr->replen+cptr->reppnt) )
				{
					cptr->samppos = ((unsigned long)(cptr->reppnt)<<11) + (cptr->samppos % ((unsigned long)(cptr->replen+cptr->reppnt)<<11));
				}

#ifndef HXCMOD_MOD_FILE_IN_ROM
				// Note : Directly modify the sample in the mod buffer...
				// The current Invert Loop effect implementation can't be played from ROM.
				cptr->sampdata[cptr->samppos >> 10] = -1 - cptr->sampdata[cptr->samppos >> 10];
#endif
			}
		}
	}
}

static void worknote( note * nptr, hxcmod_channel_t * cptr,char t,modcontext * mod )
{
	// BK4BSTACK_CHANGE START
	(void) t;
	// we rename sample into a_sample to avoid shadowing typedef struct { .. } sample; from hxcmod.h
	// BK4BSTACK_CHANGE END

	muint a_sample, period, effect, operiod;
	muint curnote, arpnote;
	muchar effect_op;
	muchar effect_param,effect_param_l,effect_param_h;
	muint enable_nxt_smp;
	const short * period_table_ptr;

	a_sample = (nptr->sampperiod & 0xF0) | (nptr->sampeffect >> 4);
	period = ((nptr->sampperiod & 0xF) << 8) | nptr->period;
	effect = ((nptr->sampeffect & 0xF) << 8) | nptr->effect;
	effect_op = nptr->sampeffect & 0xF;
	effect_param = nptr->effect;
	effect_param_l = effect_param & 0x0F;
	effect_param_h = effect_param >> 4;

	enable_nxt_smp = 0;

	operiod = cptr->period;

	if ( period || a_sample )
	{
		if( a_sample && ( a_sample < 32 ) )
		{
			cptr->sampnum = a_sample - 1;
		}

		if( period || a_sample )
		{
			if( period )
			{
				if( ( effect_op != EFFECT_TONE_PORTAMENTO ) || ( ( effect_op == EFFECT_TONE_PORTAMENTO ) && !cptr->sampdata ) )
				{
					// Not a Tone Partamento effect or no sound currently played :
					if ( ( effect_op != EFFECT_EXTENDED || effect_param_h != EFFECT_E_NOTE_DELAY ) || ( ( effect_op == EFFECT_EXTENDED && effect_param_h == EFFECT_E_NOTE_DELAY ) && !effect_param_l ) )
					{
						// Immediately (re)trigger the new note
						cptr->sampdata = mod->sampledata[cptr->sampnum];
						cptr->length = GET_BGI_W( mod->song.samples[cptr->sampnum].length );
						cptr->reppnt = GET_BGI_W( mod->song.samples[cptr->sampnum].reppnt );
						cptr->replen = GET_BGI_W( mod->song.samples[cptr->sampnum].replen );

						cptr->lst_sampdata = cptr->sampdata;
						cptr->lst_length = cptr->length;
						cptr->lst_reppnt = cptr->reppnt;
						cptr->lst_replen = cptr->replen;
					}
					else
					{
						cptr->dly_sampdata = mod->sampledata[cptr->sampnum];
						cptr->dly_length = GET_BGI_W( mod->song.samples[cptr->sampnum].length );
						cptr->dly_reppnt = GET_BGI_W( mod->song.samples[cptr->sampnum].reppnt );
						cptr->dly_replen = GET_BGI_W( mod->song.samples[cptr->sampnum].replen );
						cptr->note_delay = effect_param_l;
					}
					// Cancel any delayed note...
					cptr->update_nxt_repeat = 0;
				}
				else
				{
					// Partamento effect - Play the new note after the current sample.
					if( effect_op == EFFECT_TONE_PORTAMENTO )
						enable_nxt_smp = 1;
				}
			}
			else // Note without period : Trigger it after the current sample.
				enable_nxt_smp = 1;

			if ( enable_nxt_smp )
			{
				// Prepare the next sample retrigger after the current one
				cptr->nxt_sampdata = mod->sampledata[cptr->sampnum];
				cptr->nxt_length = GET_BGI_W( mod->song.samples[cptr->sampnum].length );
				cptr->nxt_reppnt = GET_BGI_W( mod->song.samples[cptr->sampnum].reppnt );
				cptr->nxt_replen = GET_BGI_W( mod->song.samples[cptr->sampnum].replen );

				if(cptr->nxt_replen < 2)   // Protracker : don't play the sample if not looped...
					cptr->nxt_sampdata = 0;

				cptr->update_nxt_repeat = 1;
			}

			cptr->finetune = (mod->song.samples[cptr->sampnum].finetune) & 0xF;

			if( effect_op != EFFECT_VIBRATO && effect_op != EFFECT_VOLSLIDE_VIBRATO )
			{
				cptr->vibraperiod = 0;
				cptr->vibrapointeur = 0;
			}
		}

		if( (a_sample != 0) && ( effect_op != EFFECT_VOLSLIDE_TONEPORTA ) )
		{
			cptr->volume = mod->song.samples[cptr->sampnum].volume;
			cptr->volumeslide = 0;
#ifdef HXCMOD_USE_PRECALC_VOLUME_TABLE
			cptr->volume_table = mod->volume_selection_table[cptr->volume];
#endif
		}

		if( ( effect_op != EFFECT_TONE_PORTAMENTO ) && ( effect_op != EFFECT_VOLSLIDE_TONEPORTA ) )
		{
			if ( period != 0 )
				cptr->samppos = 0;
		}

		cptr->decalperiod = 0;
		if( period )
		{
			if( cptr->finetune )
			{
				period_table_ptr = periodtable_finetune_ptr[cptr->finetune&0xF];
				period = period_table_ptr[getnote(mod,period)];
			}

			cptr->period = period;
		}
	}

	cptr->effect = 0;
	cptr->parameffect = 0;
	cptr->effect_code = effect;

#ifdef EFFECTS_USAGE_STATE
	if(effect_op || ((effect_op==EFFECT_ARPEGGIO) && effect_param))
	{
		mod->effects_event_counts[ effect_op ]++;
	}

	if(effect_op == 0xE)
		mod->effects_event_counts[ 0x10 + effect_param_h ]++;
#endif

	switch ( effect_op )
	{
		case EFFECT_ARPEGGIO:
			/*
			[0]: Arpeggio
			Where [0][x][y] means "play note, note+x semitones, note+y
			semitones, then return to original note". The fluctuations are
			carried out evenly spaced in one pattern division. They are usually
			used to simulate chords, but this doesn't work too well. They are
			also used to produce heavy vibrato. A major chord is when x=4, y=7.
			A minor chord is when x=3, y=7.
			*/

			if( effect_param )
			{
				cptr->effect = EFFECT_ARPEGGIO;
				cptr->parameffect = effect_param;

				cptr->ArpIndex = 0;

				curnote = getnote(mod,cptr->period);

				cptr->Arpperiods[0] = cptr->period;

				period_table_ptr = periodtable_finetune_ptr[cptr->finetune&0xF];

				arpnote = curnote + (((cptr->parameffect>>4)&0xF));
				if( arpnote >= MAXNOTES )
					arpnote = (MAXNOTES) - 1;

				cptr->Arpperiods[1] = period_table_ptr[arpnote];

				arpnote = curnote + (((cptr->parameffect)&0xF));
				if( arpnote >= MAXNOTES )
					arpnote = (MAXNOTES) - 1;

				cptr->Arpperiods[2] = period_table_ptr[arpnote];
			}
		break;

		case EFFECT_PORTAMENTO_UP:
			/*
			[1]: Slide up
			Where [1][x][y] means "smoothly decrease the period of current
			sample by x*16+y after each tick in the division". The
			ticks/division are set with the 'set speed' effect (see below). If
			the period of the note being played is z, then the final period
			will be z - (x*16 + y)*(ticks - 1). As the slide rate depends on
			the speed, changing the speed will change the slide. You cannot
			slide beyond the note B3 (period 113).
			*/

			cptr->effect = EFFECT_PORTAMENTO_UP;
			cptr->parameffect = effect_param;
		break;

		case EFFECT_PORTAMENTO_DOWN:
			/*
			[2]: Slide down
			Where [2][x][y] means "smoothly increase the period of current
			sample by x*16+y after each tick in the division". Similar to [1],
			but lowers the pitch. You cannot slide beyond the note C1 (period
			856).
			*/

			cptr->effect = EFFECT_PORTAMENTO_DOWN;
			cptr->parameffect = effect_param;
		break;

		case EFFECT_TONE_PORTAMENTO:
			/*
			[3]: Slide to note
			Where [3][x][y] means "smoothly change the period of current sample
			by x*16+y after each tick in the division, never sliding beyond
			current period". The period-length in this channel's division is a
			parameter to this effect, and hence is not played. Sliding to a
			note is similar to effects [1] and [2], but the slide will not go
			beyond the given period, and the direction is implied by that
			period. If x and y are both 0, then the old slide will continue.
			*/

			cptr->effect = EFFECT_TONE_PORTAMENTO;
			if( effect_param != 0 )
			{
				cptr->portaspeed = (short)( effect_param );
			}

			if(period!=0)
			{
				cptr->portaperiod = period;
				cptr->period = operiod;
			}
		break;

		case EFFECT_VIBRATO:
			/*
			[4]: Vibrato
			Where [4][x][y] means "oscillate the sample pitch using a
			particular waveform with amplitude y/16 semitones, such that (x *
			ticks)/64 cycles occur in the division". The waveform is set using
			effect [14][4]. By placing vibrato effects on consecutive
			divisions, the vibrato effect can be maintained. If either x or y
			are 0, then the old vibrato values will be used.
			*/

			cptr->effect = EFFECT_VIBRATO;
			if( effect_param_l != 0 ) // Depth continue or change ?
				cptr->vibraparam = ( cptr->vibraparam & 0xF0 ) | effect_param_l;
			if( effect_param_h != 0 ) // Speed continue or change ?
				cptr->vibraparam = ( cptr->vibraparam & 0x0F ) | ( effect_param_h << 4 );

		break;

		case EFFECT_VOLSLIDE_TONEPORTA:
			/*
			[5]: Continue 'Slide to note', but also do Volume slide
			Where [5][x][y] means "either slide the volume up x*(ticks - 1) or
			slide the volume down y*(ticks - 1), at the same time as continuing
			the last 'Slide to note'". It is illegal for both x and y to be
			non-zero. You cannot slide outside the volume range 0..64. The
			period-length in this channel's division is a parameter to this
			effect, and hence is not played.
			*/

			if( period != 0 )
			{
				cptr->portaperiod = period;
				cptr->period = operiod;
			}

			cptr->effect = EFFECT_VOLSLIDE_TONEPORTA;
			if( effect_param != 0 )
				cptr->volumeslide = effect_param;

		break;

		case EFFECT_VOLSLIDE_VIBRATO:
			/*
			[6]: Continue 'Vibrato', but also do Volume slide
			Where [6][x][y] means "either slide the volume up x*(ticks - 1) or
			slide the volume down y*(ticks - 1), at the same time as continuing
			the last 'Vibrato'". It is illegal for both x and y to be non-zero.
			You cannot slide outside the volume range 0..64.
			*/

			cptr->effect = EFFECT_VOLSLIDE_VIBRATO;
			if( effect_param != 0 )
				cptr->volumeslide = effect_param;
		break;

		case EFFECT_SET_OFFSET:
			/*
			[9]: Set sample offset
			Where [9][x][y] means "play the sample from offset x*4096 + y*256".
			The offset is measured in words. If no sample is given, yet one is
			still playing on this channel, it should be retriggered to the new
			offset using the current volume.
			If xy is 00, the previous value is used.
			*/

			cptr->samppos = ( ( ((muint)effect_param_h) << 12) + ( (((muint)effect_param_l) << 8) ) ) << 10;

			if(!cptr->samppos)
				cptr->samppos = cptr->last_set_offset;

			cptr->last_set_offset = cptr->samppos;
		break;

		case EFFECT_VOLUME_SLIDE:
			/*
			[10]: Volume slide
			Where [10][x][y] means "either slide the volume up x*(ticks - 1) or
			slide the volume down y*(ticks - 1)". If both x and y are non-zero,
			then the y value is ignored (assumed to be 0). You cannot slide
			outside the volume range 0..64.
			*/

			cptr->effect = EFFECT_VOLUME_SLIDE;
			cptr->volumeslide = effect_param;
		break;

		case EFFECT_JUMP_POSITION:
			/*
			[11]: Position Jump
			Where [11][x][y] means "stop the pattern after this division, and
			continue the song at song-position x*16+y". This shifts the
			'pattern-cursor' in the pattern table (see above). Legal values for
			x*16+y are from 0 to 127.
			*/

			mod->tablepos = effect_param;
			if(mod->tablepos >= mod->song.length)
				mod->tablepos = 0;
			mod->patternpos = 0;
			mod->jump_loop_effect = 1;

		break;

		case EFFECT_SET_VOLUME:
			/*
			[12]: Set volume
			Where [12][x][y] means "set current sample's volume to x*16+y".
			Legal volumes are 0..64.
			*/

			cptr->volume = effect_param;

			if(cptr->volume > 64)
				cptr->volume = 64;

#ifdef HXCMOD_USE_PRECALC_VOLUME_TABLE
			cptr->volume_table = mod->volume_selection_table[cptr->volume];
#endif
		break;

		case EFFECT_PATTERN_BREAK:
			/*
			[13]: Pattern Break
			Where [13][x][y] means "stop the pattern after this division, and
			continue the song at the next pattern at division x*10+y" (the 10
			is not a typo). Legal divisions are from 0 to 63 (note Protracker
			exception above).
			*/

			mod->patternpos = ( ((muint)(effect_param_h) * 10) + effect_param_l );

			if(mod->patternpos >= 64)
				mod->patternpos = 63;

			mod->patternpos *= mod->number_of_channels;

			if(!mod->jump_loop_effect)
			{
				mod->tablepos++;
				if(mod->tablepos >= mod->song.length)
					mod->tablepos = 0;
			}

			mod->jump_loop_effect = 1;
		break;

		case EFFECT_EXTENDED:
			switch( effect_param_h )
			{
				case EFFECT_E_FINE_PORTA_UP:
					/*
					[14][1]: Fineslide up
					Where [14][1][x] means "decrement the period of the current sample
					by x". The incrementing takes place at the beginning of the
					division, and hence there is no actual sliding. You cannot slide
					beyond the note B3 (period 113).
					*/

					cptr->period -= effect_param_l;
					if( cptr->period < 113 )
						cptr->period = 113;
				break;

				case EFFECT_E_FINE_PORTA_DOWN:
					/*
					[14][2]: Fineslide down
					Where [14][2][x] means "increment the period of the current sample
					by x". Similar to [14][1] but shifts the pitch down. You cannot
					slide beyond the note C1 (period 856).
					*/

					cptr->period += effect_param_l;
					if( cptr->period > 856 )
						cptr->period = 856;
				break;

				case EFFECT_E_GLISSANDO_CTRL:
					/*
					[14][3]: Set glissando on/off
					Where [14][3][x] means "set glissando ON if x is 1, OFF if x is 0".
					Used in conjunction with [3] ('Slide to note'). If glissando is on,
					then 'Slide to note' will slide in semitones, otherwise will
					perform the default smooth slide.
					*/

					cptr->glissando = effect_param_l;
				break;

				case EFFECT_E_FINE_VOLSLIDE_UP:
					/*
					[14][10]: Fine volume slide up
					Where [14][10][x] means "increment the volume of the current sample
					by x". The incrementing takes place at the beginning of the
					division, and hence there is no sliding. You cannot slide beyond
					volume 64.
					*/

					cptr->volume += effect_param_l;
					if( cptr->volume > 64 )
						cptr->volume = 64;
#ifdef HXCMOD_USE_PRECALC_VOLUME_TABLE
					cptr->volume_table = mod->volume_selection_table[cptr->volume];
#endif
				break;

				case EFFECT_E_FINE_VOLSLIDE_DOWN:
					/*
					[14][11]: Fine volume slide down
					Where [14][11][x] means "decrement the volume of the current sample
					by x". Similar to [14][10] but lowers volume. You cannot slide
					beyond volume 0.
					*/

					cptr->volume -= effect_param_l;
					if( cptr->volume > 200 )
						cptr->volume = 0;
#ifdef HXCMOD_USE_PRECALC_VOLUME_TABLE
					cptr->volume_table = mod->volume_selection_table[cptr->volume];
#endif
				break;

				case EFFECT_E_SET_FINETUNE:
					/*
					[14][5]: Set finetune value
					Where [14][5][x] means "sets the finetune value of the current
					sample to the signed nibble x". x has legal values of 0..15,
					corresponding to signed nibbles 0..7,-8..-1 (see start of text for
					more info on finetune values).
					*/

					cptr->finetune = effect_param_l;

					if( period )
					{
						period_table_ptr = periodtable_finetune_ptr[cptr->finetune&0xF];
						period = period_table_ptr[getnote(mod,period)];
						cptr->period = period;
					}

				break;

				case EFFECT_E_PATTERN_LOOP:
					/*
					[14][6]: Loop pattern
					Where [14][6][x] means "set the start of a loop to this division if
					x is 0, otherwise after this division, jump back to the start of a
					loop and play it another x times before continuing". If the start
					of the loop was not set, it will default to the start of the
					current pattern. Hence 'loop pattern' cannot be performed across
					multiple patterns. Note that loops do not support nesting, and you
					may generate an infinite loop if you try to nest 'loop pattern's.
					*/

					if( effect_param_l )
					{
						if( cptr->patternloopcnt )
						{
							cptr->patternloopcnt--;
							if( cptr->patternloopcnt )
							{
								mod->patternpos = cptr->patternloopstartpoint;
								mod->jump_loop_effect = 1;
							}
							else
							{
								cptr->patternloopstartpoint = mod->patternpos ;
							}
						}
						else
						{
							cptr->patternloopcnt = effect_param_l;
							mod->patternpos = cptr->patternloopstartpoint;
							mod->jump_loop_effect = 1;
						}
					}
					else // Start point
					{
						cptr->patternloopstartpoint = mod->patternpos;
					}

				break;

				case EFFECT_E_PATTERN_DELAY:
					/*
					[14][14]: Delay pattern
					Where [14][14][x] means "after this division there will be a delay
					equivalent to the time taken to play x divisions after which the
					pattern will be resumed". The delay only relates to the
					interpreting of new divisions, and all effects and previous notes
					continue during delay.
					*/

					mod->patterndelay = effect_param_l;
				break;

				case EFFECT_E_RETRIGGER_NOTE:
					/*
					[14][9]: Retrigger sample
					 Where [14][9][x] means "trigger current sample every x ticks in
					 this division". If x is 0, then no retriggering is done (acts as if
					 no effect was chosen), otherwise the retriggering begins on the
					 first tick and then x ticks after that, etc.
					*/

					if( effect_param_l )
					{
						cptr->effect = EFFECT_EXTENDED;
						cptr->parameffect = (EFFECT_E_RETRIGGER_NOTE<<4);
						cptr->retrig_param = effect_param_l;
						cptr->retrig_cnt = 0;
					}
				break;

				case EFFECT_E_NOTE_CUT:
					/*
					[14][12]: Cut sample
					Where [14][12][x] means "after the current sample has been played
					for x ticks in this division, its volume will be set to 0". This
					implies that if x is 0, then you will not hear any of the sample.
					If you wish to insert "silence" in a pattern, it is better to use a
					"silence"-sample (see above) due to the lack of proper support for
					this effect.
					*/

					cptr->effect = EFFECT_E_NOTE_CUT;
					cptr->cut_param = effect_param_l;
					if( !cptr->cut_param )
					{
						cptr->volume = 0;
#ifdef HXCMOD_USE_PRECALC_VOLUME_TABLE
						cptr->volume_table = mod->volume_selection_table[cptr->volume];
#endif
					}
				break;

				case EFFECT_E_NOTE_DELAY:
					/*
					 Where [14][13][x] means "do not start this division's sample for
					 the first x ticks in this division, play the sample after this".
					 This implies that if x is 0, then you will hear no delay, but
					 actually there will be a VERY small delay. Note that this effect
					 only influences a sample if it was started in this division.
					*/

					cptr->effect = EFFECT_EXTENDED;
					cptr->parameffect = (EFFECT_E_NOTE_DELAY<<4);
				break;

				case EFFECT_E_INVERT_LOOP:
					/*
					Where [14][15][x] means "if x is greater than 0, then play the
					current sample's loop upside down at speed x". Each byte in the
					sample's loop will have its sign changed (negated). It will only
					work if the sample's loop (defined previously) is not too big. The
					speed is based on an internal table.
					*/

					cptr->funkspeed = effect_param_l;

					doFunk(cptr);

				break;

				default:

				break;
			}
		break;

		case 0xF:
			/*
			[15]: Set speed
			Where [15][x][y] means "set speed to x*16+y". Though it is nowhere
			near that simple. Let z = x*16+y. Depending on what values z takes,
			different units of speed are set, there being two: ticks/division
			and beats/minute (though this one is only a label and not strictly
			true). If z=0, then what should technically happen is that the
			module stops, but in practice it is treated as if z=1, because
			there is already a method for stopping the module (running out of
			patterns). If z<=32, then it means "set ticks/division to z"
			otherwise it means "set beats/minute to z" (convention says that
			this should read "If z<32.." but there are some composers out there
			that defy conventions). Default values are 6 ticks/division, and
			125 beats/minute (4 divisions = 1 beat). The beats/minute tag is
			only meaningful for 6 ticks/division. To get a more accurate view
			of how things work, use the following formula:
									 24 * beats/minute
				  divisions/minute = -----------------
									  ticks/division
			Hence divisions/minute range from 24.75 to 6120, eg. to get a value
			of 2000 divisions/minute use 3 ticks/division and 250 beats/minute.
			If multiple "set speed" effects are performed in a single division,
			the ones on higher-numbered channels take precedence over the ones
			on lower-numbered channels. This effect has a large number of
			different implementations, but the one described here has the
			widest usage.
			*/


			if( effect_param )
			{

				if( effect_param < 0x20 )
				{
					mod->song.speed = effect_param;
				}
				else
				{   // effect_param >= 0x20
					///	 HZ = 2 * BPM / 5
					mod->bpm = effect_param;
				}

#ifdef HXCMOD_16BITS_TARGET
				// song.speed = 1 <> 31
				// playrate = 8000 <> 22050
				// bpm = 32 <> 255

				mod->patternticksem = (muint)( ( (mulong)mod->playrate * 5 ) / ( (muint)mod->bpm * 2 ) );
#else
				// song.speed = 1 <> 31
				// playrate = 8000 <> 96000
				// bpm = 32 <> 255

				mod->patternticksem = ( ( mod->playrate * 5 ) / ( (mulong)mod->bpm * 2 ) );
#endif
				mod->patternticksaim = mod->song.speed * mod->patternticksem;
			}

		break;

		default:
		// Unsupported effect
		break;

	}

}

static void workeffect( modcontext * modctx, note * nptr, hxcmod_channel_t * cptr )
{
	// BK4BSTACK_CHANGE START
	(void) nptr;
	// BK4BSTACK_CHANGE END

	doFunk(cptr);

	switch(cptr->effect)
	{
		case EFFECT_ARPEGGIO:

			if( cptr->parameffect )
			{
				cptr->decalperiod = cptr->period - cptr->Arpperiods[cptr->ArpIndex];

				cptr->ArpIndex++;
				if( cptr->ArpIndex>2 )
					cptr->ArpIndex = 0;
			}
		break;

		case EFFECT_PORTAMENTO_UP:

			if( cptr->period )
			{
				cptr->period -= cptr->parameffect;

				if( cptr->period < 113 || cptr->period > 20000 )
					cptr->period = 113;
			}

		break;

		case EFFECT_PORTAMENTO_DOWN:

			if( cptr->period )
			{
				cptr->period += cptr->parameffect;

				if( cptr->period > 20000 )
					cptr->period = 20000;
			}

		break;

		case EFFECT_VOLSLIDE_TONEPORTA:
		case EFFECT_TONE_PORTAMENTO:

			if( cptr->period && ( cptr->period != cptr->portaperiod ) && cptr->portaperiod )
			{
				if( cptr->period > cptr->portaperiod )
				{
					if( cptr->period - cptr->portaperiod >= cptr->portaspeed )
					{
						cptr->period -= cptr->portaspeed;
					}
					else
					{
						cptr->period = cptr->portaperiod;
					}
				}
				else
				{
					if( cptr->portaperiod - cptr->period >= cptr->portaspeed )
					{
						cptr->period += cptr->portaspeed;
					}
					else
					{
						cptr->period = cptr->portaperiod;
					}
				}

				if( cptr->period == cptr->portaperiod )
				{
					// If the slide is over, don't let it to be retriggered.
					cptr->portaperiod = 0;
				}
			}

			if( cptr->glissando )
			{
				// TODO : Glissando effect.
			}

			if( cptr->effect == EFFECT_VOLSLIDE_TONEPORTA )
			{
				if( cptr->volumeslide & 0xF0 )
				{
					cptr->volume += ( cptr->volumeslide >> 4 );

					if( cptr->volume > 63 )
						cptr->volume = 63;
				}
				else
				{
					cptr->volume -= ( cptr->volumeslide & 0x0F );

					if( cptr->volume > 63 )
						cptr->volume = 0;
				}
#ifdef HXCMOD_USE_PRECALC_VOLUME_TABLE
				cptr->volume_table = modctx->volume_selection_table[cptr->volume];
#endif
			}
		break;

		case EFFECT_VOLSLIDE_VIBRATO:
		case EFFECT_VIBRATO:

			cptr->vibraperiod = ( (cptr->vibraparam&0xF) * sintable[cptr->vibrapointeur&0x1F] )>>7;

			if( cptr->vibrapointeur > 31 )
				cptr->vibraperiod = -cptr->vibraperiod;

			cptr->vibrapointeur = ( cptr->vibrapointeur + ( ( cptr->vibraparam>>4 ) & 0x0F) ) & 0x3F;

			if( cptr->effect == EFFECT_VOLSLIDE_VIBRATO )
			{
				if( cptr->volumeslide & 0xF0 )
				{
					cptr->volume += ( cptr->volumeslide >> 4 );

					if( cptr->volume > 64 )
						cptr->volume = 64;
				}
				else
				{
					cptr->volume -= cptr->volumeslide;

					if( cptr->volume > 64 )
						cptr->volume = 0;
				}
#ifdef HXCMOD_USE_PRECALC_VOLUME_TABLE
				cptr->volume_table = modctx->volume_selection_table[cptr->volume];
#endif
			}

		break;

		case EFFECT_VOLUME_SLIDE:

			if( cptr->volumeslide & 0xF0 )
			{
				cptr->volume += ( cptr->volumeslide >> 4 );

				if( cptr->volume > 64 )
					cptr->volume = 64;
			}
			else
			{
				cptr->volume -= cptr->volumeslide;

				if( cptr->volume > 64 )
					cptr->volume = 0;
			}
#ifdef HXCMOD_USE_PRECALC_VOLUME_TABLE
			cptr->volume_table = modctx->volume_selection_table[cptr->volume];
#endif
		break;

		case EFFECT_EXTENDED:
			switch( cptr->parameffect >> 4 )
			{

				case EFFECT_E_NOTE_CUT:
					if( cptr->cut_param )
						cptr->cut_param--;

					if( !cptr->cut_param )
					{
						cptr->volume = 0;
#ifdef HXCMOD_USE_PRECALC_VOLUME_TABLE
						cptr->volume_table = modctx->volume_selection_table[cptr->volume];
#endif
					}
				break;

				case EFFECT_E_RETRIGGER_NOTE:
					cptr->retrig_cnt++;
					if( cptr->retrig_cnt >= cptr->retrig_param )
					{
						cptr->retrig_cnt = 0;

						cptr->sampdata = cptr->lst_sampdata;
						cptr->length = cptr->lst_length;
						cptr->reppnt = cptr->lst_reppnt;
						cptr->replen = cptr->lst_replen;
						cptr->samppos = 0;
					}
				break;

				case EFFECT_E_NOTE_DELAY:
					if( cptr->note_delay )
					{
						if( (unsigned char)( cptr->note_delay - 1 ) == modctx->tick_cnt )
						{
							cptr->sampdata = cptr->dly_sampdata;
							cptr->length = cptr->dly_length;
							cptr->reppnt = cptr->dly_reppnt;
							cptr->replen = cptr->dly_replen;

							cptr->lst_sampdata = cptr->sampdata;
							cptr->lst_length = cptr->length;
							cptr->lst_reppnt = cptr->reppnt;
							cptr->lst_replen = cptr->replen;
							cptr->note_delay = 0;
						}
					}
				break;
				default:
				break;
			}
		break;

		default:
		break;

	}

}

///////////////////////////////////////////////////////////////////////////////////
int hxcmod_init(modcontext * modctx)
{
#ifdef HXCMOD_USE_PRECALC_VOLUME_TABLE
	muint c;
	mint  i,j;
#endif
	if( modctx )
	{
		memclear(modctx,0,sizeof(modcontext));
		modctx->playrate = 44100;
		modctx->stereo = 1;
		modctx->stereo_separation = 1;
		modctx->bits = 16;
		modctx->filter = 1;

#ifdef HXCMOD_USE_PRECALC_VOLUME_TABLE
		c = 0;
		for(i=0;i<65;i++)
		{
			for(j=-128;j<128;j++)
			{
				modctx->precalc_volume_array[c] = i * j;
				c++;
			}

			modctx->volume_selection_table[i] = &modctx->precalc_volume_array[(i*256) + 128];
		}
#endif

		return 1;
	}

	return 0;
}

int hxcmod_setcfg(modcontext * modctx, int samplerate, int stereo_separation, int filter)
{
	if( modctx )
	{
		modctx->playrate = samplerate;

		if(stereo_separation < 4)
		{
			modctx->stereo_separation = stereo_separation;
		}

		if( filter )
			modctx->filter = 1;
		else
			modctx->filter = 0;

		return 1;
	}

	return 0;
}

int hxcmod_load( modcontext * modctx, void * mod_data, int mod_data_size )
{
	muint i, j, max, digitfactor;
	sample *sptr;
	unsigned char * modmemory,* endmodmemory;

	modmemory = (unsigned char *)mod_data;
	endmodmemory = modmemory + mod_data_size;

	if( modmemory )
	{
		if( modctx )
		{
#ifdef FULL_STATE
			memclear(&(modctx->effects_event_counts),0,sizeof(modctx->effects_event_counts));
#endif
			memcopy(&(modctx->song),modmemory,1084);

			i = 0;
			modctx->number_of_channels = 0;
			while(modlist[i].numberofchannels && !modctx->number_of_channels)
			{
				digitfactor = 0;

				j = 0;
				while( j < 4 )
				{
					if( modlist[i].signature[j] == '$' )
					{
						if(digitfactor)
							digitfactor *= 10;
						else
							digitfactor = 1;
					}
					j++;
				}

				modctx->number_of_channels = 0;

				j = 0;
				while( j < 4 )
				{
					if( (modlist[i].signature[j] == modctx->song.signature[j]) || modlist[i].signature[j] == '$' )
					{
						if( modlist[i].signature[j] == '$' )
						{
							if(modctx->song.signature[j] >= '0' && modctx->song.signature[j] <= '9')
							{
								modctx->number_of_channels += (modctx->song.signature[j] - '0') * digitfactor;
								digitfactor /= 10;
							}
							else
							{
								modctx->number_of_channels = 0;
								break;
							}
						}
						j++;
					}
					else
					{
						modctx->number_of_channels = 0;
						break;
					}
				}

				if( j == 4 )
				{
					if(!modctx->number_of_channels)
						modctx->number_of_channels = modlist[i].numberofchannels;
				}

				i++;
			}

			if( !modctx->number_of_channels )
			{
				// 15 Samples modules support
				// Shift the whole datas to make it look likes a standard 4 channels mod.
				memcopy(&(modctx->song.signature), "M.K.", 4);
				memcopy(&(modctx->song.length), &(modctx->song.samples[15]), 130);
				memclear(&(modctx->song.samples[15]), 0, 480);
				modmemory += 600;
				modctx->number_of_channels = 4;
			}
			else
			{
				modmemory += 1084;
			}

			if( modctx->number_of_channels > NUMMAXCHANNELS )
				return 0; // Too much channels ! - Increase/define HXCMOD_MAXCHANNELS !

			if( modmemory >= endmodmemory )
				return 0; // End passed ? - Probably a bad file !

			// Patterns loading
			for (i = max = 0; i < 128; i++)
			{
				while (max <= modctx->song.patterntable[i])
				{
					modctx->patterndata[max] = (note*)modmemory;
					modmemory += (256*modctx->number_of_channels);
					max++;

					if( modmemory >= endmodmemory )
						return 0; // End passed ? - Probably a bad file !
				}
			}

			for (i = 0; i < 31; i++)
				modctx->sampledata[i]=0;

			// Samples loading
			for (i = 0, sptr = modctx->song.samples; i <31; i++, sptr++)
			{
				if (sptr->length == 0) continue;

				modctx->sampledata[i] = (mchar*)modmemory;
				modmemory += (GET_BGI_W(sptr->length)*2);

				if (GET_BGI_W(sptr->replen) + GET_BGI_W(sptr->reppnt) > GET_BGI_W(sptr->length))
					sptr->replen = GET_BGI_W((GET_BGI_W(sptr->length) - GET_BGI_W(sptr->reppnt)));

				if( modmemory > endmodmemory )
					return 0; // End passed ? - Probably a bad file !
			}

			// States init

			modctx->tablepos = 0;
			modctx->patternpos = 0;
			modctx->song.speed = 6;
			modctx->bpm = 125;

#ifdef HXCMOD_16BITS_TARGET
			// song.speed = 1 <> 31
			// playrate = 8000 <> 22050
			// bpm = 32 <> 255

			modctx->patternticksem = (muint)( ( (mulong)modctx->playrate * 5 ) / ( (muint)modctx->bpm * 2 ) );
#else
			// song.speed = 1 <> 31
			// playrate = 8000 <> 96000
			// bpm = 32 <> 255

			modctx->patternticksem = ( ( modctx->playrate * 5 ) / ( (mulong)modctx->bpm * 2 ) );
#endif
			modctx->patternticksaim = modctx->song.speed * modctx->patternticksem;

			modctx->patternticks = modctx->patternticksaim + 1;

			modctx->sampleticksconst = ((3546894UL * 16) / modctx->playrate) << 6; //8448*428/playrate;

			for(i=0; i < modctx->number_of_channels; i++)
			{
				modctx->channels[i].volume = 0;
				modctx->channels[i].period = 0;
#ifdef HXCMOD_USE_PRECALC_VOLUME_TABLE
				modctx->channels[i].volume_table = modctx->volume_selection_table[0];
#endif
			}

			modctx->mod_loaded = 1;

			return 1;
		}
	}

	return 0;
}

void hxcmod_fillbuffer(modcontext * modctx, msample * outbuffer, mssize nbsample, tracker_buffer_state * trkbuf)
{
	mssize i;
	muint  j;
	muint c;

	unsigned long k;
	unsigned int state_remaining_steps;

#ifdef HXCMOD_OUTPUT_FILTER
#ifndef HXCMOD_MONO_OUTPUT
	int ll,tl;
#endif
	int lr,tr;
#endif

#ifndef HXCMOD_MONO_OUTPUT
	int l;
#endif
	int r;

	short finalperiod;
	note	*nptr;
	hxcmod_channel_t *cptr;

	if( modctx && outbuffer )
	{
		if(modctx->mod_loaded)
		{
			state_remaining_steps = 0;

#ifdef HXCMOD_STATE_REPORT_SUPPORT
			if( trkbuf )
			{
				trkbuf->cur_rd_index = 0;

				memcopy(trkbuf->name,modctx->song.title,sizeof(modctx->song.title));

				for(i=0;i<31;i++)
				{
					memcopy(trkbuf->instruments[i].name,modctx->song.samples[i].name,sizeof(trkbuf->instruments[i].name));
				}
			}
#endif

#ifdef HXCMOD_OUTPUT_FILTER
	#ifndef HXCMOD_MONO_OUTPUT
			ll = modctx->last_l_sample;
	#endif
			lr = modctx->last_r_sample;
#endif

			for (i = 0; i < nbsample; i++)
			{
				//---------------------------------------
				if( modctx->patternticks++ > modctx->patternticksaim )
				{
					if( !modctx->patterndelay )
					{
						nptr = modctx->patterndata[modctx->song.patterntable[modctx->tablepos]];
						nptr = nptr + modctx->patternpos;
						cptr = modctx->channels;

						modctx->tick_cnt = 0;

						modctx->patternticks = 0;
						modctx->patterntickse = 0;

						for(c=0;c<modctx->number_of_channels;c++)
						{
							worknote((note*)(nptr), (hxcmod_channel_t*)(cptr),(char)(c+1),modctx);

							if (cptr->period != 0)
							{
								finalperiod = cptr->period - cptr->decalperiod - cptr->vibraperiod;
								if (finalperiod)
								{
									cptr->sampinc = ((modctx->sampleticksconst) / finalperiod);
								}
								else
								{
									cptr->sampinc = 0;
								}
							}
							else
								cptr->sampinc = 0;

							nptr++;
							cptr++;
						}

						if( !modctx->jump_loop_effect )
							modctx->patternpos += modctx->number_of_channels;
						else
							modctx->jump_loop_effect = 0;

						if( modctx->patternpos == 64*modctx->number_of_channels )
						{
							modctx->tablepos++;
							modctx->patternpos = 0;
							if(modctx->tablepos >= modctx->song.length)
								modctx->tablepos = 0;
						}
					}
					else
					{
						modctx->patterndelay--;
						modctx->patternticks = 0;
						modctx->patterntickse = 0;
						modctx->tick_cnt = 0;
					}

				}

				if (modctx->patterntickse++ > modctx->patternticksem)
				{
					nptr = modctx->patterndata[modctx->song.patterntable[modctx->tablepos]];
					nptr = nptr + modctx->patternpos;
					cptr = modctx->channels;

					for(c=0;c<modctx->number_of_channels;c++)
					{
						workeffect( modctx, nptr, cptr );

						if (cptr->period != 0)
						{
							finalperiod = cptr->period - cptr->decalperiod - cptr->vibraperiod;
							if (finalperiod)
							{
								cptr->sampinc = ((modctx->sampleticksconst) / finalperiod);
							}
							else
							{
								cptr->sampinc = 0;
							}
						}
						else
							cptr->sampinc = 0;

						nptr++;
						cptr++;
					}

					modctx->tick_cnt++;
					modctx->patterntickse = 0;
				}

				//---------------------------------------

#ifdef HXCMOD_STATE_REPORT_SUPPORT
				if( trkbuf && !state_remaining_steps )
				{
					if( trkbuf->nb_of_state < trkbuf->nb_max_of_state )
					{
						memclear(&trkbuf->track_state_buf[trkbuf->nb_of_state],0,sizeof(tracker_state));
					}
				}
#endif

#ifndef HXCMOD_MONO_OUTPUT
				l=0;
#endif
				r=0;

				for( j = 0, cptr = modctx->channels; j < modctx->number_of_channels ; j++, cptr++)
				{
					if( cptr->period != 0 )
					{
						cptr->samppos += cptr->sampinc;

						if( cptr->replen < 2 )
						{
							if( ( cptr->samppos >> 11) >= cptr->length )
							{
								cptr->length = 0;
								cptr->reppnt = 0;

								if(cptr->update_nxt_repeat)
								{
									cptr->replen = cptr->nxt_replen;
									cptr->reppnt = cptr->nxt_reppnt;
									cptr->sampdata = cptr->nxt_sampdata;
									cptr->length = cptr->nxt_length;

									cptr->lst_sampdata = cptr->sampdata;
									cptr->lst_length = cptr->length;
									cptr->lst_reppnt = cptr->reppnt;
									cptr->lst_replen = cptr->replen;

									cptr->update_nxt_repeat = 0;
								}

								if( cptr->length )
									cptr->samppos = cptr->samppos % (((unsigned long)cptr->length)<<11);
								else
									cptr->samppos = 0;
							}
						}
						else
						{
							if( ( cptr->samppos >> 11 ) >= (unsigned long)(cptr->replen+cptr->reppnt) )
							{
								if( cptr->update_nxt_repeat )
								{
									cptr->replen = cptr->nxt_replen;
									cptr->reppnt = cptr->nxt_reppnt;
									cptr->sampdata = cptr->nxt_sampdata;
									cptr->length = cptr->nxt_length;

									cptr->lst_sampdata = cptr->sampdata;
									cptr->lst_length = cptr->length;
									cptr->lst_reppnt = cptr->reppnt;
									cptr->lst_replen = cptr->replen;

									cptr->update_nxt_repeat = 0;
								}

								if( cptr->sampdata )
								{
									cptr->samppos = ((unsigned long)(cptr->reppnt)<<11) + (cptr->samppos % ((unsigned long)(cptr->replen+cptr->reppnt)<<11));
								}
							}
						}

						k = cptr->samppos >> 10;

#ifdef HXCMOD_MONO_OUTPUT
						if( cptr->sampdata!=0 )
						{
#ifdef HXCMOD_USE_PRECALC_VOLUME_TABLE
							r += cptr->volume_table[cptr->sampdata[k]];
#else
							r += ( cptr->sampdata[k] *  cptr->volume );
#endif
						}
#else
						if (cptr->sampdata != 0)
						{
							if ( !(j & 3) || ((j & 3) == 3) )
							{
#ifdef HXCMOD_USE_PRECALC_VOLUME_TABLE
								l += cptr->volume_table[cptr->sampdata[k]];
#else
								l += (cptr->sampdata[k] * cptr->volume);
#endif
							}
							else
							{
#ifdef HXCMOD_USE_PRECALC_VOLUME_TABLE
								r += cptr->volume_table[cptr->sampdata[k]];
#else
								r += (cptr->sampdata[k] * cptr->volume);
#endif
							}
						}
#endif

#ifdef HXCMOD_STATE_REPORT_SUPPORT
						if( trkbuf && !state_remaining_steps )
						{
							if( trkbuf->nb_of_state < trkbuf->nb_max_of_state )
							{
								trkbuf->track_state_buf[trkbuf->nb_of_state].number_of_tracks = modctx->number_of_channels;
								trkbuf->track_state_buf[trkbuf->nb_of_state].buf_index = i;
								trkbuf->track_state_buf[trkbuf->nb_of_state].cur_pattern = modctx->song.patterntable[modctx->tablepos];
								trkbuf->track_state_buf[trkbuf->nb_of_state].cur_pattern_pos = modctx->patternpos / modctx->number_of_channels;
								trkbuf->track_state_buf[trkbuf->nb_of_state].cur_pattern_table_pos = modctx->tablepos;
								trkbuf->track_state_buf[trkbuf->nb_of_state].bpm = modctx->bpm;
								trkbuf->track_state_buf[trkbuf->nb_of_state].speed = modctx->song.speed;
								trkbuf->track_state_buf[trkbuf->nb_of_state].tracks[j].cur_effect = cptr->effect_code;
								trkbuf->track_state_buf[trkbuf->nb_of_state].tracks[j].cur_parameffect = cptr->parameffect;
								if(cptr->sampinc)
									trkbuf->track_state_buf[trkbuf->nb_of_state].tracks[j].cur_period = (muint)(modctx->sampleticksconst / cptr->sampinc);
								else
									trkbuf->track_state_buf[trkbuf->nb_of_state].tracks[j].cur_period = 0;
								trkbuf->track_state_buf[trkbuf->nb_of_state].tracks[j].cur_volume = cptr->volume;
								trkbuf->track_state_buf[trkbuf->nb_of_state].tracks[j].instrument_number = (unsigned char)cptr->sampnum;
							}
						}
#endif
					}
				}

#ifdef HXCMOD_STATE_REPORT_SUPPORT
				if( trkbuf && !state_remaining_steps )
				{
					state_remaining_steps = trkbuf->sample_step;

					if(trkbuf->nb_of_state < trkbuf->nb_max_of_state)
						trkbuf->nb_of_state++;
				}
				else
#endif
				{
					state_remaining_steps--;
				}

#ifdef HXCMOD_MONO_OUTPUT

	#ifdef HXCMOD_OUTPUT_FILTER
				tr = (short)r;

				if ( modctx->filter )
				{
					// Filter
					r = (r+lr)>>1;
				}
	#endif

	#ifdef HXCMOD_CLIPPING_CHECK
				// Level limitation
				if( r > 32767 ) r = 32767;
				if( r < -32768 ) r = -32768;
	#endif
				// Store the final sample.
	#ifdef HXCMOD_8BITS_OUTPUT

		#ifdef HXCMOD_UNSIGNED_OUTPUT
				*outbuffer++ = (r >> 8) + 127;
		#else
				*outbuffer++ = r >> 8;
		#endif

	#else

		#ifdef HXCMOD_UNSIGNED_OUTPUT
				*outbuffer++ = r + 32767;
		#else
				*outbuffer++ = r;
		#endif

	#endif

	#ifdef HXCMOD_OUTPUT_FILTER
				lr = tr;
	#endif

#else

	#ifdef HXCMOD_OUTPUT_FILTER
				tl = (short)l;
				tr = (short)r;

				if ( modctx->filter )
				{
					// Filter
					l = (l+ll)>>1;
					r = (r+lr)>>1;
				}
	#endif

	#ifdef HXCMOD_OUTPUT_STEREO_MIX
				if ( modctx->stereo_separation == 1 )
				{
					// Left & Right Stereo panning
					l = (l+(r>>1));
					r = (r+(l>>1));
				}
	#endif

	#ifdef HXCMOD_CLIPPING_CHECK
				// Level limitation
				if( l > 32767 ) l = 32767;
				if( l < -32768 ) l = -32768;
				if( r > 32767 ) r = 32767;
				if( r < -32768 ) r = -32768;
	#endif
				// Store the final sample.


	#ifdef HXCMOD_8BITS_OUTPUT

		#ifdef HXCMOD_UNSIGNED_OUTPUT
				*outbuffer++ = ( l >> 8 ) + 127;
				*outbuffer++ = ( r >> 8 ) + 127;
		#else
				*outbuffer++ = l >> 8;
				*outbuffer++ = r >> 8;
		#endif

	#else

		#ifdef HXCMOD_UNSIGNED_OUTPUT
				*outbuffer++ = l + 32767;
				*outbuffer++ = r + 32767;
		#else
				*outbuffer++ = l;
				*outbuffer++ = r;
		#endif

	#endif

	#ifdef HXCMOD_OUTPUT_FILTER
				ll = tl;
				lr = tr;
	#endif

#endif // HXCMOD_MONO_OUTPUT

			}

#ifdef HXCMOD_OUTPUT_FILTER
	#ifndef HXCMOD_MONO_OUTPUT
			modctx->last_l_sample = ll;
	#endif
			modctx->last_r_sample = lr;
#endif
		}
		else
		{
			for (i = 0; i < nbsample; i++)
			{
				// Mod not loaded. Return blank buffer.
#ifdef HXCMOD_MONO_OUTPUT
				outbuffer[i] = 0;
#else
				*outbuffer++ = 0;
				*outbuffer++ = 0;
#endif
			}

#ifdef HXCMOD_STATE_REPORT_SUPPORT
			if(trkbuf)
			{
				trkbuf->nb_of_state = 0;
				trkbuf->cur_rd_index = 0;
				trkbuf->name[0] = 0;
				memclear(trkbuf->track_state_buf,0,sizeof(tracker_state) * trkbuf->nb_max_of_state);
				memclear(trkbuf->instruments,0,sizeof(trkbuf->instruments));
			}
#endif
		}
	}
}

void hxcmod_unload( modcontext * modctx )
{
	if(modctx)
	{
		memclear(&modctx->song,0,sizeof(modctx->song));
		memclear(&modctx->sampledata,0,sizeof(modctx->sampledata));
		memclear(&modctx->patterndata,0,sizeof(modctx->patterndata));
		modctx->tablepos = 0;
		modctx->patternpos = 0;
		modctx->patterndelay  = 0;
		modctx->jump_loop_effect = 0;
		modctx->bpm = 0;
		modctx->patternticks = 0;
		modctx->patterntickse = 0;
		modctx->patternticksaim = 0;
		modctx->sampleticksconst = 0;

		memclear(modctx->channels,0,sizeof(modctx->channels));

		modctx->number_of_channels = 0;

		modctx->mod_loaded = 0;

		modctx->last_r_sample = 0;
		modctx->last_l_sample = 0;
	}
}
