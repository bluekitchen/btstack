/*
 * Lc3Decoder.hpp
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

#ifndef API_LC3DECODER_HPP_
#define API_LC3DECODER_HPP_

#include <cstdint>
#include <vector>
#include "Lc3Config.hpp"

/*
 * Forward declaration of the "internal" class DecoderTop, so
 * that the private vector of single channel decoders can be declared in Lc3Decoder.
 */
namespace Lc3Dec
{
    class DecoderTop;
}

/*
 * The LC3 decoder interface is specified in
 * LC3 specification Sections 2.4 "Decoder interfaces" (dr09r07).
 *
 * Lc3Decoder is designed such that all specified features are provided.
 *
 * In contrast to the Lc3Encoder, even 24bit and 32bit decoded audio
 * data can be provided - however in one fix byte arrangement which will
 * not meet all meaningful options. Providing 24bit and 32bit decoded audio
 * data makes the API more complex but does not increase the needed
 * resources when basic 16bit/sample audio data is desired only.
 *
 * Instantiating Lc3Decoder implies providing a Lc3Config instance. A copy of
 * this instance is available as public const member so that all essential session
 * parameters can be obtained throughout the lifetime of the Lc3Decoder instance.
 *
 * There is no possibility of changing Lc3Config within Lc3Decoder. When session
 * parameters have to be changed, the calling application has to create a new
 * instance of Lc3Decoder.
 *
 * Lc3 supports operation with variable encoded bitrate. It is possible to change
 * the bitrate from frame to frame, where for preciseness the parameter is not
 * given as bitrate directly but in terms of the byte_count per frame. This
 * parameter has to be in the range of 20 to 400
 * (see LC3 specification Section 3.2.5 "Bit budget and bitrate").
 *   LC3 specification Sections 2.4 "Decoder interfaces" specifies "byte_count_max"
 * of the decoder to allow pre-allocation of resources. This parameter is provided
 * here for completeness and used for verifying the byte_count value of individual
 * frames only. The needed memory has to be provided by the calling application anyway,
 * so that it is up to the application whether a pre-allocation of memory is useful.
 *
 */

class Lc3Decoder
{
public:
    /*
     * Convenience constructor of Lc3Decoder with two simple parameter only.
     * Note that this constructor instantiated Lc3Config implicitly with
     *  Lc3Decoder(Lc3Config(Fs,frameDuration, 1)) that means that this
     * constructor provides processing of one channel (mono) and 16bit/sample
     * PCM output only.
     *
     * Parameters:
     *  Fs : Sampling frequency in Hz -> see Lc3Config.hpp for supported values
     *
     *  frameDuration : frame duration of 10ms (default) or 7.5ms
     *                    -> see Lc3Config.hpp for more details
     */
    Lc3Decoder(uint16_t Fs, Lc3Config::FrameDuration frameDuration = Lc3Config::FrameDuration::d10ms);

    /*
     * General constructor of Lc3Decoder.
     *
     * Parameters:
     *  lc3Config_ : instance of Lc3Config. See documentation of Lc3Config for more details.
     *               Note: providing an invalid instance of Lc3Config will result
     *                     in skipping any processing later.
     *               The provided instance of Lc3Config will be copied to the
     *               public field "lc3Config" (see below).
     *
     *  bits_per_audio_sample_dec_ : Bits per audio sample for the output PCM signal.
     *                               See LC3 specification Section 2.4 "Decoder interfaces"
     *                               and Section 3.2.3 "Bits per sample" for the general LC3
     *                               requirement to support 16, 24 and 32 bit.
     *                                 Note: This parameter may differ from the encoder input PCM
     *                               setting "bits_per_audio_sample_enc".
     *
     *  byte_count_max_dec_ : Maximum allowed payload byte_count for a single channel.
     *                          When using and allowing external rate control, the maximum byte
     *                        count for the session may be used to configure the session buffers
     *                        without a need to dynamically reallocate memory during the session.
     *
     * datapoints : pointer to an instance of a class allowing to collect internal data.
     *              Note: this feature is used and prepared for testing of the codec
     *                    implementation only. See general "Readme.txt"
     */
    Lc3Decoder(Lc3Config lc3Config_, uint8_t bits_per_audio_sample_dec_ = 16, uint16_t byte_count_max_dec_ = 400, void* datapoints=nullptr);

    // no default constructor supported
    Lc3Decoder() = delete;

    // Destructor
    virtual ~Lc3Decoder();

    /*
     * Configuration provided during instantiation accessible as public const fields.
     * Note: Lc3Config provides a getter to check whether the configuration is valid.
     */
    const Lc3Config lc3Config;
    const uint8_t bits_per_audio_sample_dec;
    const uint16_t byte_count_max_dec;

    // encoding error (see return values of "run" methods)
    static const uint8_t ERROR_FREE            = 0x00;
    static const uint8_t INVALID_CONFIGURATION = 0x01;
    static const uint8_t INVALID_BYTE_COUNT    = 0x02;
    static const uint8_t INVALID_X_OUT_SIZE    = 0x03;
    static const uint8_t INVALID_BITS_PER_AUDIO_SAMPLE = 0x04;
    static const uint8_t DECODER_ALLOCATION_ERROR      = 0x05;

    /*
     * Decoding one 16 bit/sample output frame for one channel
     *
     * Note that this methods returns the error INVALID_BITS_PER_AUDIO_SAMPLE
     * when the session has been configured for any bits_per_audio_sample_dec != 16.
     *
     * Further, note that this method can be used for multi-channel configurations as well,
     * particularly when the provided multi-channel "run" (see below) is not supporting
     * the kind of byte stream concatenation existing in the calling application.
     *
     * Parameters:
     *   bytes : pointer to byte array holding the input LC3 encoded byte stream
     *           of the given channel.
     *           The size of this memory is given by the byte_count parameter.
     *
     *   byte_count : number of encoded bytes; byte count to be used for decoding
     *                the received frame payload.
     *                Supported values are 20 bytes to byte_count_max_dec bytes.
     *
     *   BFI : Bad Frame Indication flags
     *         "0" signifies that no bit errors where detected in given "bytes"
     *         "1" signifies a corrupt payload packet was detected in given "bytes"
     *
     *   x_out : pointer to output PCM data (16 bit/sample), where the memory
     *           has to be provided by the calling application.
     *
     *   x_out_size : number of 16 bit values supported by x_out
     *                Note: this parameter has been introduced for clarity and
     *                      verification purpose only. The method will return
     *                      the error INVALID_X_OUT_SIZE when x_out_size != lc3Config.NF.
     *
     *   BEC_detect : flag indication bit errors detected during decoding of input bytes
     *                Note: a basic packet loss concealment (PLC) will be applied when
     *                      BEC_detect!=0, so that the returned audio data stays
     *                      somewhat acceptable.
     *
     *  channelNr : index of channel to be processed (default=0), where channelNr < lc3Config.Nc
     *
     *
     * Return value: error code as listed above.
     */
    uint8_t run(const uint8_t* bytes, uint16_t byte_count, uint8_t BFI,
            int16_t* x_out, uint16_t x_out_size, uint8_t& BEC_detect, uint8_t channelNr=0);

    /*
     * Decoding one 16, 24, or 32 bit/sample output frame for one channel
     *
     * Note that every output PCM sample will need one 32 bit memory place in the
     * output stream independently from the configured bits_per_audio_sample_dec.
     *
     * Further, note that this method can be used for multi-channel configurations as well,
     * particularly when the provided multi-channel "run" (see below) is not supporting
     * the kind of byte stream concatenation existing in the calling application.
     *
     * Parameters:
     *   bytes : pointer to byte array holding the input LC3 encoded byte stream
     *           of the given channel.
     *           The size of this memory is given by the byte_count parameter.
     *
     *   byte_count : number of encoded bytes; byte count to be used for decoding
     *                the received frame payload.
     *                Supported values are 20 bytes to byte_count_max_dec bytes.
     *
     *   BFI : Bad Frame Indication flags
     *         "0" signifies that no bit errors where detected in given "bytes"
     *         "1" signifies a corrupt payload packet was detected in given "bytes"
     *
     *   x_out : pointer to output PCM data (memory 32 bit/sample, precision 16 bit/sample,
     *           24 bit/sample or 32 bit/sample), where the memory
     *           has to be provided by the calling application.
     *
     *   x_out_size : number of 32 bit values supported by x_out
     *                Note: this parameter has been introduced for clarity and
     *                      verification purpose only. The method will return
     *                      the error INVALID_X_OUT_SIZE when x_out_size != lc3Config.NF.
     *
     *   BEC_detect : flag indication bit errors detected during decoding of input bytes
     *                Note: a basic packet loss concealment (PLC) will be applied when
     *                      BEC_detect!=0, so that the returned audio data stays
     *                      somewhat acceptable.
     *
     *  channelNr : index of channel to be processed (default=0), where channelNr < lc3Config.Nc
     *
     *
     * Return value: error code as listed above.
     */
    uint8_t run(const uint8_t* bytes, uint16_t byte_count, uint8_t BFI,
            int32_t* x_out, uint16_t x_out_size, uint8_t& BEC_detect, uint8_t channelNr=0);

    /*
     * Decoding one 16 bit/sample output frame for multiple channels.
     *
     * Note that this methods returns the error INVALID_BITS_PER_AUDIO_SAMPLE
     * when the session has been configured for any bits_per_audio_sample_dec != 16.
     *
     * Parameters:
     *   bytes : pointer to byte array holding the input LC3 encoded byte stream
     *           of all given channels.
     *           The size of this memory is given by the sum of all byte_count_per_channel
     *           values (see parameter byte_count_per_channel).
     *           Note that the encoded values of all channels are expected to be
     *           concatenated without any stuffing bytes of meta data in between.
     *
     *   byte_count_per_channel : number of encoded bytes; byte count to be used for decoding
     *                            the received frame payload per channel.
     *                            Thus, byte_count_per_channel is an array of byte_count values
     *                            with length lc3Conig.Nc
     *                            Supported values are 20 bytes to byte_count_max_dec bytes per
     *                            channel.
     *
     *   BFI_per_channel : lc3Conig.Nc length array of Bad Frame Indication flags
     *                     "0" signifies that no bit errors where detected in given "bytes"
     *                     "1" signifies a corrupt payload packet was detected in given "bytes"
     *
     *   x_out : pointer to output 16 bit/sample PCM data, where the memory
     *           has to be provided by the calling application.
     *
     *   x_out_size : number of 16 bit values supported by x_out
     *                Note: this parameter has been introduced for clarity and
     *                      verification purpose only. The method will return
     *                      the error INVALID_X_OUT_SIZE when x_out_size != lc3Config.NF * lc3Conig.Nc.
     *
     *   BEC_detect_per_channel : lc3Conig.Nc length array of flags indicating bit errors
     *                            detected during decoding of input bytes of a certain channel.
     *                           Note: a basic packet loss concealment (PLC) will be applied when
     *                              BEC_detect!=0, so that the returned audio data stays
     *                              somewhat acceptable.
     *
     *
     * Return value: error code via "or" concatenation of the error codes of processing
     *               the individual channels. Note: this "or" concatenation make specific
     *               error diagnosis impossible. Thus only checking != ERROR_FREE is meaningful.
     *               When more specific information is needed, the single channel call (see above)
     *               need to be called.
     */
    uint8_t run(const uint8_t* bytes, const uint16_t* byte_count_per_channel, const uint8_t* BFI_per_channel,
            int16_t* x_out, uint32_t x_out_size, uint8_t* BEC_detect_per_channel);

    /*
     * Decoding one 16, 24, or 32 bit/sample output frame for multiple channels
     *
     * Note that every output PCM sample will need one 32 bit memory place in the
     * output stream independently from the configured bits_per_audio_sample_dec.
     *
     * Parameters:
     *   bytes : pointer to byte array holding the input LC3 encoded byte stream
     *           of all given channels.
     *           The size of this memory is given by the sum of all byte_count_per_channel
     *           values (see parameter byte_count_per_channel).
     *           Note that the encoded values of all channels are expected to be
     *           concatenated without any stuffing bytes of meta data in between.
     *
     *   byte_count_per_channel : number of encoded bytes; byte count to be used for decoding
     *                            the received frame payload per channel.
     *                            Thus, byte_count_per_channel is an array of byte_count values
     *                            with length lc3Conig.Nc
     *                            Supported values are 20 bytes to byte_count_max_dec bytes per
     *                            channel.
     *
     *   BFI_per_channel : lc3Conig.Nc length array of Bad Frame Indication flags
     *                     "0" signifies that no bit errors where detected in given "bytes"
     *                     "1" signifies a corrupt payload packet was detected in given "bytes"
     *
     *   x_out : pointer to output 16, 24, or 32 bit/sample PCM data, where the memory
     *           has to be provided by the calling application.
     *
     *   x_out_size : number of 32 bit values supported by x_out
     *                Note: this parameter has been introduced for clarity and
     *                      verification purpose only. The method will return
     *                      the error INVALID_X_OUT_SIZE when x_out_size != lc3Config.NF * lc3Conig.Nc.
     *
     *   BEC_detect_per_channel : lc3Conig.Nc length array of flags indicating bit errors
     *                            detected during decoding of input bytes of a certain channel.
     *                           Note: a basic packet loss concealment (PLC) will be applied when
     *                              BEC_detect!=0, so that the returned audio data stays
     *                              somewhat acceptable.
     *
     *
     * Return value: error code via "or" concatenation of the error codes of processing
     *               the individual channels. Note: this "or" concatenation make specific
     *               error diagnosis impossible. Thus only checking != ERROR_FREE is meaningful.
     *               When more specific information is needed, the single channel call (see above)
     *               need to be called.
     */
    uint8_t run(const uint8_t* bytes, const uint16_t* byte_count_per_channel, const uint8_t* BFI_per_channel,
            int32_t* x_out, uint32_t x_out_size, uint8_t* BEC_detect_per_channel);



private:
    std::vector<Lc3Dec::DecoderTop*> decoderList;
};

#endif /* API_LC3DECODER_HPP_ */
