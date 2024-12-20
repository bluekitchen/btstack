#
# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import array
import ctypes
import enum
import glob
import os

from ctypes import c_bool, c_byte, c_int, c_uint, c_size_t, c_void_p
from ctypes.util import find_library


class _Base:

    def __init__(self, frame_duration, samplerate, nchannels, **kwargs):

        self.hrmode = False
        self.dt_us = int(frame_duration * 1000)
        self.sr_hz = int(samplerate)
        self.sr_pcm_hz = self.sr_hz
        self.nchannels = nchannels

        libpath = None

        for k in kwargs.keys():
            if k == 'hrmode':
                self.hrmode = bool(kwargs[k])
            elif k == 'pcm_samplerate':
                self.sr_pcm_hz = int(kwargs[k])
            elif k == 'libpath':
                libpath = kwargs[k]
            else:
                raise ValueError("Invalid keyword argument: " + k)

        if self.dt_us not in [2500, 5000, 7500, 10000]:
            raise ValueError(
                "Invalid frame duration: %.1f ms" % frame_duration)

        allowed_samplerate = [8000, 16000, 24000, 32000, 48000] \
            if not self.hrmode else [48000, 96000]

        if self.sr_hz not in allowed_samplerate:
            raise ValueError("Invalid sample rate: %d Hz" % samplerate)

        if libpath is None:
            mesonpy_lib = glob.glob(os.path.join(os.path.dirname(__file__), '.lc3.mesonpy.libs', '*lc3*'))

            if mesonpy_lib:
                libpath = mesonpy_lib[0]
            else:
                libpath = find_library("lc3")
            if not libpath:
                raise Exception("LC3 library not found")

        lib = ctypes.cdll.LoadLibrary(libpath)

        try:
            lib.lc3_hr_frame_samples \
                and lib.lc3_hr_frame_block_bytes \
                and lib.lc3_hr_resolve_bitrate \
                and lib.lc3_hr_delay_samples

        except AttributeError:

            if self.hrmode:
                raise Exception('High-Resolution interface not available')

            lib.lc3_hr_frame_samples = \
                lambda hrmode, dt_us, sr_hz: \
                lib.lc3_frame_samples(dt_us, sr_hz)

            lib.lc3_hr_frame_block_bytes = \
                lambda hrmode, dt_us, sr_hz, nchannels, bitrate: \
                nchannels * lib.lc3_frame_bytes(dt_us, bitrate // 2)

            lib.lc3_hr_resolve_bitrate = \
                lambda hrmode, dt_us, sr_hz, nbytes: \
                lib.lc3_resolve_bitrate(dt_us, nbytes)

            lib.lc3_hr_delay_samples = \
                lambda hrmode, dt_us, sr_hz: \
                lib.lc3_delay_samples(dt_us, sr_hz)

        lib.lc3_hr_frame_samples.argtypes = [c_bool, c_int, c_int]
        lib.lc3_hr_frame_block_bytes.argtypes = \
            [c_bool, c_int, c_int, c_int, c_int]
        lib.lc3_hr_resolve_bitrate.argtypes = [c_bool, c_int, c_int, c_int]
        lib.lc3_hr_delay_samples.argtypes = [c_bool, c_int, c_int]
        self.lib = lib

        libc = ctypes.cdll.LoadLibrary(find_library("c"))

        self.malloc = libc.malloc
        self.malloc.argtypes = [c_size_t]
        self.malloc.restype = c_void_p

        self.free = libc.free
        self.free.argtypes = [c_void_p]

    def get_frame_samples(self):
        """
        Returns the number of PCM samples in an LC3 frame
        """
        ret = self.lib.lc3_hr_frame_samples(
            self.hrmode, self.dt_us, self.sr_pcm_hz)
        if ret < 0:
            raise ValueError("Bad parameters")
        return ret

    def get_frame_bytes(self, bitrate):
        """
        Returns the size of LC3 frame blocks, from bitrate in bit per seconds.
        A target `bitrate` equals 0 or `INT32_MAX` returns respectively
        the minimum and maximum allowed size.
        """
        ret = self.lib.lc3_hr_frame_block_bytes(
            self.hrmode, self.dt_us, self.sr_hz, self.nchannels, bitrate)
        if ret < 0:
            raise ValueError("Bad parameters")
        return ret

    def resolve_bitrate(self, nbytes):
        """
        Returns the bitrate in bits per seconds, from the size of LC3 frames.
        """
        ret = self.lib.lc3_hr_resolve_bitrate(
            self.hrmode, self.dt_us, self.sr_hz, nbytes)
        if ret < 0:
            raise ValueError("Bad parameters")
        return ret

    def get_delay_samples(self):
        """
         Returns the algorithmic delay, as a number of samples.
         """
        ret = self.lib.lc3_hr_delay_samples(
            self.hrmode, self.dt_us, self.sr_pcm_hz)
        if ret < 0:
            raise ValueError("Bad parameters")
        return ret

    @staticmethod
    def _resolve_pcm_format(bitdepth):
        PCM_FORMAT_S16 = 0
        PCM_FORMAT_S24 = 1
        PCM_FORMAT_S24_3LE = 2
        PCM_FORMAT_FLOAT = 3

        match bitdepth:
            case 16: return (PCM_FORMAT_S16, ctypes.c_int16)
            case 24: return (PCM_FORMAT_S24_3LE, 3 * ctypes.c_byte)
            case None: return (PCM_FORMAT_FLOAT, ctypes.c_float)
            case _: raise ValueError("Could not interpret PCM bitdepth")


class Encoder(_Base):
    """
    LC3 Encoder wrapper

    The `frame_duration` expressed in milliseconds is any of 2.5, 5.0, 7.5
    or 10.0. The `samplerate`, in Hertz, is any of 8000, 16000, 24000, 32000
    or 48000, unless High-Resolution mode is enabled. In High-Resolution mode,
    the `samplerate` is 48000 or 96000.

    By default, one channel is processed. When `nchannels` is greater than one,
    the PCM input stream is read interleaved and consecutives LC3 frames are
    output, for each channel.

    Keyword arguments:
        hrmode    : Enable High-Resolution mode, default is `False`.
        sr_pcm_hz : Input PCM samplerate, enable downsampling of input.
        libpath   : LC3 library path and name
    """

    class c_encoder_t(c_void_p):
        pass

    def __init__(self, frame_duration, samplerate, nchannels=1, **kwargs):

        super().__init__(frame_duration, samplerate, nchannels, **kwargs)

        lib = self.lib

        try:
            lib.lc3_hr_encoder_size \
                and lib.lc3_hr_setup_encoder

        except AttributeError:

            assert not self.hrmode

            lib.lc3_hr_encoder_size = \
                lambda hrmode, dt_us, sr_hz: \
                lib.lc3_encoder_size(dt_us, sr_hz)

            lib.lc3_hr_setup_encoder = \
                lambda hrmode, dt_us, sr_hz, sr_pcm_hz, mem: \
                lib.lc3_setup_encoder(dt_us, sr_hz, sr_pcm_hz, mem)

        lib.lc3_hr_encoder_size.argtypes = [c_bool, c_int, c_int]
        lib.lc3_hr_encoder_size.restype = c_uint

        lib.lc3_hr_setup_encoder.argtypes = \
            [c_bool, c_int, c_int, c_int, c_void_p]
        lib.lc3_hr_setup_encoder.restype = self.c_encoder_t

        lib.lc3_encode.argtypes = \
            [self.c_encoder_t, c_int, c_void_p, c_int, c_int, c_void_p]

        def new_encoder(): return lib.lc3_hr_setup_encoder(
            self.hrmode, self.dt_us, self.sr_hz, self.sr_pcm_hz,
            self.malloc(lib.lc3_hr_encoder_size(
                self.hrmode, self.dt_us, self.sr_pcm_hz)))

        self.__encoders = [new_encoder() for i in range(nchannels)]

    def __del__(self):

        try:
            (self.free(encoder) for encoder in self.__encoders)
        finally:
            return

    def encode(self, pcm, nbytes, bitdepth=None):
        """
        Encode LC3 frame(s), for each channel.

        The `pcm` input is given in two ways. When no `bitdepth` is defined,
        it's a vector of floating point values from -1 to 1, coding the sample
        levels. When `bitdepth` is defined, `pcm` is interpreted as a byte-like
        object, each sample coded on `bitdepth` bits (16 or 24).
        The machine endianness, or little endian, is used for 16 or 24 bits
        width, respectively.
        In both cases, the `pcm` vector data is padded with zeros when
        its length is less than the required input samples for the encoder.
        Channels concatenation of encoded LC3 frames, of `nbytes`, is returned.
        """

        nchannels = self.nchannels
        frame_samples = self.get_frame_samples()

        (pcm_fmt, pcm_t) = self._resolve_pcm_format(bitdepth)
        pcm_len = nchannels * frame_samples

        if bitdepth is None:
            pcm_buffer = array.array('f', pcm)

            # Invert test to catch NaN
            if not abs(sum(pcm)) / frame_samples < 2:
                raise ValueError("Out of range PCM input")

            padding = max(pcm_len - frame_samples, 0)
            pcm_buffer.extend(array.array('f', [0] * padding))

        else:
            padding = max(pcm_len * ctypes.sizeof(pcm_t) - len(pcm), 0)
            pcm_buffer = bytearray(pcm) + bytearray(padding)

        data_buffer = (c_byte * nbytes)()
        data_offset = 0

        for (ich, encoder) in enumerate(self.__encoders):

            pcm_offset = ich * ctypes.sizeof(pcm_t)
            pcm = (pcm_t * (pcm_len - ich)).from_buffer(pcm_buffer, pcm_offset)

            data_size = nbytes // nchannels + int(ich < nbytes % nchannels)
            data = (c_byte * data_size).from_buffer(data_buffer, data_offset)
            data_offset += data_size

            ret = self.lib.lc3_encode(
                encoder, pcm_fmt, pcm, nchannels, len(data), data)
            if ret < 0:
                raise ValueError("Bad parameters")

        return bytes(data_buffer)


class Decoder(_Base):
    """
    LC3 Decoder wrapper

    The `frame_duration` expressed in milliseconds is any of 2.5, 5.0, 7.5
    or 10.0. The `samplerate`, in Hertz, is any of 8000, 16000, 24000, 32000
    or 48000, unless High-Resolution mode is enabled. In High-Resolution
    mode, the `samplerate` is 48000 or 96000.

    By default, one channel is processed. When `nchannels` is greater than one,
    the PCM input stream is read interleaved and consecutives LC3 frames are
    output, for each channel.

    Keyword arguments:
        hrmode    : Enable High-Resolution mode, default is `False`.
        sr_pcm_hz : Output PCM samplerate, enable upsampling of output.
        libpath   : LC3 library path and name
    """

    class c_decoder_t(c_void_p):
        pass

    def __init__(self, frame_duration, samplerate, nchannels=1, **kwargs):

        super().__init__(frame_duration, samplerate, nchannels, **kwargs)

        lib = self.lib

        try:
            lib.lc3_hr_decoder_size \
                and lib.lc3_hr_setup_decoder

        except AttributeError:

            assert not self.hrmode

            lib.lc3_hr_decoder_size = \
                lambda hrmode, dt_us, sr_hz: \
                lib.lc3_decoder_size(dt_us, sr_hz)

            lib.lc3_hr_setup_decoder = \
                lambda hrmode, dt_us, sr_hz, sr_pcm_hz, mem: \
                lib.lc3_setup_decoder(dt_us, sr_hz, sr_pcm_hz, mem)

        lib.lc3_hr_decoder_size.argtypes = [c_bool, c_int, c_int]
        lib.lc3_hr_decoder_size.restype = c_uint

        lib.lc3_hr_setup_decoder.argtypes = \
            [c_bool, c_int, c_int, c_int, c_void_p]
        lib.lc3_hr_setup_decoder.restype = self.c_decoder_t

        lib.lc3_decode.argtypes = \
            [self.c_decoder_t, c_void_p, c_int, c_int, c_void_p, c_int]

        def new_decoder(): return lib.lc3_hr_setup_decoder(
            self.hrmode, self.dt_us, self.sr_hz, self.sr_pcm_hz,
            self.malloc(lib.lc3_hr_decoder_size(
                self.hrmode, self.dt_us, self.sr_pcm_hz)))

        self.__decoders = [new_decoder() for i in range(nchannels)]

    def __del__(self):

        try:
            (self.free(decoder) for decoder in self.__decoders)
        finally:
            return

    def decode(self, data, bitdepth=None):
        """
        Decode an LC3 frame

        The input `data` is the channels concatenation of LC3 frames in a
        byte-like object. Interleaved PCM samples are returned according to
        the `bitdepth` indication.
        When no `bitdepth` is defined, it's a vector of floating point values
        from -1 to 1, coding the sample levels. When `bitdepth` is defined,
        it returns a byte array, each sample coded on `bitdepth` bits.
        The machine endianness, or little endian, is used for 16 or 24 bits
        width, respectively.
        """

        nchannels = self.nchannels
        frame_samples = self.get_frame_samples()

        (pcm_fmt, pcm_t) = self._resolve_pcm_format(bitdepth)
        pcm_len = nchannels * self.get_frame_samples()
        pcm_buffer = (pcm_t * pcm_len)()

        data_buffer = bytearray(data)
        data_offset = 0

        for (ich, decoder) in enumerate(self.__decoders):
            pcm_offset = ich * ctypes.sizeof(pcm_t)
            pcm = (pcm_t * (pcm_len - ich)).from_buffer(pcm_buffer, pcm_offset)

            data_size = len(data_buffer) // nchannels + \
                int(ich < len(data_buffer) % nchannels)
            data = (c_byte * data_size).from_buffer(data_buffer, data_offset)
            data_offset += data_size

            ret = self.lib.lc3_decode(
                decoder, data, len(data), pcm_fmt, pcm, self.nchannels)
            if ret < 0:
                raise ValueError("Bad parameters")

        return array.array('f', pcm_buffer) \
            if bitdepth is None else bytes(pcm_buffer)
