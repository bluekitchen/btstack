/*
 * Lc3Config.hpp
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

#ifndef __LC3_CONFIG_HPP_
#define __LC3_CONFIG_HPP_

#include <cstdint>

/*
 * The LC3 encoder and decoder have a common set of essential session
 * configuration parameters - see LC3 specification Sections 2.2 "Encoder interfaces"
 * and Section 2.4 "Decoder interfaces" (dr09r07).
 * The set of common session configuration parameters is represented
 * by an instance of class Lc3Config.
 *
 * Lc3Config is designed such that all parameters need to be
 * providing when constructing an instance. Afterwards, success of creation,
 * the actual parameter values, and derived parameter values are provided
 * by corresponding const getter methods or public const fields.
 *
 * When parameters need to be changes, a new instance of Lc3Config has to be
 * created.
 *
 * Instances of Lc3Config are provided to instances of Lc3Encoder and Lc3Decoder
 * when constructing them.
 *
 * The main purpose of Lc3Config is to verify and handle common LC3 session
 * configuration parameters in a consistent way.
 *
 */
class Lc3Config
{
    public:
        /*
         * Enumeration of supported frame durations N_ms = 7.5ms and N_ms = 10ms.
         *
         * Note: although the LC3 specification describes frame duration N_ms as
         *       a parameter with floating point values 7.5 and 10.0, we decided
         *       to make this parameter a discrete enumeration of supported
         *       frame durations. There are too many codec parts that need
         *       specifically listed constants dependent on the configured
         *       frame duration, so that it never will be possible to chose from a
         *       larger set of floating point values for N_ms.
         *         Making the data type of N_ms an enumeration supports
         *       straight forwards verification that meaningful parameters are given.
         */
        enum class FrameDuration
        {
            d10ms,
            d7p5ms
        };

        // configuration error (see "getErrorStatus")
        static const uint8_t ERROR_FREE                 = 0x00;
        static const uint8_t INVALID_SAMPLING_RATE      = 0x01;
        static const uint8_t INVALID_FRAME_DURATION     = 0x02;
        static const uint8_t INVALID_NUMBER_OF_CHANNELS = 0x04;

        /*
         * Constructor of an instance of Lc3Config
         * Parameters:
         *     (see also see LC3 specification Sections 2.2 "Encoder interfaces"
         *      and Section 2.4 "Decoder interfaces" (dr09r07) )
         *
         *  Fs_ : Sampling frequency in Hz -> defines public constant Fs
         *        Supported values are
         *           8000 Hz
         *          16000 Hz
         *          24000 Hz
         *          32000 Hz
         *          44100 Hz
         *          48000 Hz
         *
         *  N_ms_ : Frame duration given by value from enumeration FrameDuration (see above)
         *          -> defines public constant N_ms
         *          Supported values see enumeration FrameDuration.
         *
         *  Nc_ : Number of channels -> defines public constant Nc
         *          Supported values are Nc > 0 and Nc < 256 such that device resources are not exhausted.
         *          Notes:
         *            - Exhausting device resources can mean that there is not enough memory
         *              to instantiate the corresponding number of encoders and/or decoders, but
         *              also that computing the encoders and/or decoders in real-time is not possible.
         *            - The parameter N_c_max described in the LC3 specifciation is not given here,
         *              since there is too little knowledge on the target devices where this code
         *              may be used.
         *
         */
        Lc3Config(uint16_t Fs_, FrameDuration N_ms_, uint8_t Nc_);

        // no default constructor supported
        Lc3Config() = delete;

        // Destructor
        virtual ~Lc3Config();

        /*
         * Getter "isValid" provides a convenience function that returns true when an instance of Lc3Config
         * could be created without error.
         */
        bool isValid() const;

        /*
         * Getter "getErrorStatus" provides details on the success of instantiating Lc3Config.
         * The possible return values are listed as constants in this class (see configuration error constants above)
         */
        uint8_t getErrorStatus() const;

        /*
         * Getter "getByteCountFromBitrate" provides the number of bytes available in one encoded frame
         * of one channel (see "nbytes" as described in Section 3.2.5 "Bit budget and bitrate" (dr09r07) )
         *
         * The number of bytes "nbytes" to use for encoding a single channel is a required external input to
         * each single channel LC3 encoder. The same number of bytes (now to be used for decoding) is also
         * a required external input to each single channel LC3 decoder. The corresponding number of bits
         * available in one frame is thus "nbits  8*nbytes".
         *   The codec works on a byte boundary, i.e. the variable "nbytes" shall be an integer number.
         * A certain "bitrate" can be converted to a number of bytes "nbytes" where the number of bytes is
         * rounded towards the nearest lower integer.
         *
         * The algorithm is verified from the bitrate corresponding to
         *  nbytes = 20 up to the bitrate corresponding to
         *  nbytes = 400 per channel for all sampling rates.
         * The LC3 specification does not specify or recommend what bitrate to use for encoding a frame of
         * audio samples. This is specified by the profiles making use of the LC3.
         */
        uint16_t getByteCountFromBitrate(uint32_t bitrate) const; // meant for a single channel

        /*
         * Getter "getBitrateFromByteCount" provides the bitrate of the codec in bits per second
         * of one channel (see Section 3.2.5 "Bit budget and bitrate" (dr1.0r03) )
         *
         * This is a convenience utility and not directly used by the LC3 implementation.
         *
         * The bitrate of the codec in bits per second is
         * "bitrate = ceil(nbits / frame_duration) = ceil(nbits*Fs/NF) = ceil(8*nbytes*Fs/NF)".
         *
         * The LC3 specification does not specify or recommend what bitrate to use for encoding a frame of
         * audio samples. This is specified by the profiles making use of the LC3.
         */
        uint32_t getBitrateFromByteCount(uint16_t nbytes) const;

        /*
         * Getter "getFscal" provides a utility used within the LC3 implementation
         * for easier parameter mapping when Fs == 44100
         * (see Section 3.2.2 "Sampling rates" (dr1.0r03) )
         */
        double getFscal() const;


        /*
         * Getter "getNmsValue" provides a utility used within the LC3 implementation
         * that converts FrameDuration N_ms enumeration values to duration values in milliseconds.
         */
        double getNmsValue() const;

    private:
        // internal utilities used for verifying the given input parameters and computing derived parameters
        uint8_t getFs_ind(uint16_t Fs);
        uint16_t getNF(uint16_t Fs, FrameDuration N_ms);
        uint16_t getNE(uint16_t NF, FrameDuration N_ms);

        // errors status set during construction and returned by getErrorStatus()
        uint8_t errorStatus;

    public:
        // configuration details -> see also Section 3.1.2 "Mathematical symbols" (dr09r07)
        const uint16_t Fs;        // Sampling rate (in Hz)
        const uint8_t Fs_ind;     // Sampling rate index (see also Section 3.2.2 "Sampling rates" (dr09r07)
        const FrameDuration N_ms; // Frame duration -> see Lc3Config constructor documentation
                                  // Note: that the actual frame duration is longer by a factor of 480/441
                                  //       if the sampling rate is 44100 Hz
        const uint16_t NF;        // Number of samples processed in one frame of one channel (also known as frame size)
        const uint16_t NE;        // Number of encoded spectral lines (per frame and channel)
        const uint16_t Z;         // Number of leading zeros in MDCT window
        const uint8_t Nc;         // Number of channels
        const uint8_t N_b;        // Number of bands
};

#endif // __LC3_CONFIG_HPP_
