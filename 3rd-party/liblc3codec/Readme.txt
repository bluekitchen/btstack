/*
 * Readme.txt
 *
 * Copyright 2019 HIMSA II K/S - www.himsa.com. Represented by EHIMA - www.ehima.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

Contents
========
 (1) Introduction
 (2) Directory Structure
 (3) Naming Conventions
 (4) General Code Structure
 (5) Test and Debug Support

(1) Introduction
================
(derived from Bluetooth SIG LC3 specification dr09r06)
The Low Complexity Communication Codec (LC3) is an efficient Bluetooth Audio Codec for use in
audio profiles. This codec can encode speech and music at a variety of bitrates. The LC3 can
be incorporated in any Bluetooth audio profile.

The LC3 specification allows floating point and fixed point implementations as long as the
resulting decoded audio signals are sufficiently close in quality. The given implementation
exploits floating point operation in the majority of the code.

To deliver satisfactory audio quality under all channel conditions, it is strongly recommended
that some form of Packet Loss Concealment (PLC) should be implemented on the receiving ends of
audio connections. The purpose of packet loss concealment is to conceal the effect of unavailable
or corrupted frame data for decoding. The given code implements the example PLC algorithm provided
in the informative Appendix B of the LC3 specification. Alternative PLC schemes are possible, but have
to meet or exceed the performance of the given implementation.

The initial implementation of this codec has been made in year 2019 in parallel to final
improvements and extensions to the LC3 specification. The final implementation has been matched
to the specification, however, references in the code to the specification are not adapted to
the latest formal changes in the specification. Thus, the revision of the specification referenced
is given with each reference, so that it should be possible to track the links to the specification.

(2) Directory Structure
=======================
  |- Api  -> include files needed by calling application code
  |- Common | -> implementation needed by LC3 encoder and decoder
  |         |-- KissFft -> source code of kissfft library used for fast transforms
  |         |-- Tables -> tables as given in Section 3.7 "Tables and constants" (dr09r06)
  |- Decoder -> implementation of LC3 decoder (*.hpp and *.cpp)
  |- Encoder -> implementation of LC3 encoder (*.hpp and *.cpp)
  |- TestSupport -> interface and dummy/demo implementation of "datapoint" access
                    for test and debug support

(3) Naming Conventions and Coding Style
=======================================
 Names of variables/objects have been chosen to be as close as possible
to the naming within the LC3 specification. This applies not only to the
selected wording, but also to the chosen case and concatenation of words.
 Thus, variable naming may appear somewhat unorthodox to a standard C/C++ developer
and is not always consistent in style. Nevertheless, the close link to the specification
document is considered most valuable so that this approach has been followed.
 The LC3 specification contains textual descriptions, equations, pseudo-code, tables
and reference intermediate outputs. The naming of variables is not perfectly consistent,
particularly in the case of names for intermediate outputs in relation to the
specification text. Thus, there are situations where we had to select one name out of
different options or had to choose different names for internal variables and
"datapoints" provided for test and debug support.

 Some parts of the code are directly converted from pseudo-code given in
the specification document. Changes compared to the specification are made only
when supported by technical arguments. Again, the close link to the specification
is considered most valuable. Note that this implies in some situations that code
is not optimized in terms of computation effort, memory usage and/or clarity of its structure.

Further Conventions:
 - indentation using 4 spaces; no TABS at all
 - directory, file and class names are camel-case starting with a capital letter


(4) General Code Structure
==========================
 The implementation is in C++ where object oriented programming is mainly used to
 formulate the relations of higher level modules/classes. The code of the lower level
 modules is syntactically C++, but the style is more like plain old procedural
 programming. The latter is mainly due to the goal of formulating the code very close
 to the specification - not only with respect to its overall behaviour but also with
 respect to the organization. This is particularly true for the modules depending
 heavily on converted pseudo-code from the LC3 specification.

 A brief overview on the main class relationships is summarized in the following
 basic diagrams.

 Encoder:
 --------
 Lc3Encoder : main API and handling of multi-channel sessions
   |    |(1)------(1) Lc3Config : configuration instance
   |
   |(1)-------(lc3Config.Nc) EncoderTop : toplevel class for single channel session;
                                |   |     dynamically reallocates EncoderFrame instance in case
                                |   |     of bitrate changes
                                |   |
                                |   |---  holds all sub-modules directly that are not dependent
                                |         on variable bitrate;
                                |
                                |(1)-----(1) EncoderFrame : toplevel processing per frame and
                                                            reallocation of sub-modules in case
                                                            of bitrate changes

 Decoder:
 --------
 Lc3Decoder : main API and handling of multi-channel sessions
   |    |(1)------(1) Lc3Config : configuration instance
   |
   |(1)-------(lc3Config.Nc) DecoderTop : toplevel class for single channel session;
                                |   |     dynamically reallocates DecoderFrame instance in case
                                |   |     of bitrate changes
                                |   |
                                |   |---  holds all sub-modules directly that are not dependent
                                |         on variable bitrate;
                                |
                                |(1)-----(1) DecoderFrame : toplevel processing per frame and
                                                            reallocation of sub-modules in case
                                                            of bitrate changes


(5) Test and Debug Support
==========================
The development of the codec implementation has been strongly based on close match of
intermediate values with specified reference values. To be able to access the proper
values in a standardized manner a simple "datapoint" API has been created.
 Note: this API is not fully optimized for usage within in android but may be extended
for this purpose in future.
 The given API can be found in "TestSupport/Datapoints.hpp" with a basic (mainly dummy) implementation
given in "TestSupport/DatapointsAndroid.cpp".
 To use this API an instance of "DatapointContainer" has to be created and provided to the
Lc3Encoder and Lc3Decoder constructors when needed. Note: that this is not intended for final releases
due to the additional resources needed.
