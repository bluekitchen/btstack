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
from __future__ import annotations

import array
import ctypes
import enum
import glob
import os
import typing

from ctypes import c_bool, c_byte, c_int, c_uint, c_size_t, c_void_p
from ctypes.util import find_library
from collections.abc import Iterable


class BaseError(Exception):
    """Base error raised by liblc3."""


class InitializationError(RuntimeError, BaseError):
    """Error raised when liblc3 cannot be initialized."""


class InvalidArgumentError(ValueError, BaseError):
    """Error raised when a bad argument is given."""


class _PcmFormat(enum.IntEnum):
    S16 = 0
    S24 = 1
    S24_3LE = 2
    FLOAT = 3


class _Base:

    def __init__(
        self,
        frame_duration_us: int,
        sample_rate_hz: int,
        num_channels: int,
        hrmode: bool = False,
        pcm_sample_rate_hz: int | None = None,
        libpath: str | None = None,
    ) -> None:

        self.hrmode = hrmode
        self.frame_duration_us = frame_duration_us
        self.sample_rate_hz = sample_rate_hz
        self.pcm_sample_rate_hz = pcm_sample_rate_hz or self.sample_rate_hz
        self.num_channels = num_channels

        if self.frame_duration_us not in [2500, 5000, 7500, 10000]:
            raise InvalidArgumentError(
                f"Invalid frame duration: {self.frame_duration_us} us ({self.frame_duration_us / 1000:.1f} ms)"
            )

        allowed_samplerate = (
            [8000, 16000, 24000, 32000, 48000] if not self.hrmode else [48000, 96000]
        )

        if self.sample_rate_hz not in allowed_samplerate:
            raise InvalidArgumentError(f"Invalid sample rate: {sample_rate_hz} Hz")

        if libpath is None:
            mesonpy_lib = glob.glob(
                os.path.join(os.path.dirname(__file__), ".lc3py.mesonpy.libs", "*lc3*")
            )

            if mesonpy_lib:
                libpath = mesonpy_lib[0]
            else:
                libpath = find_library("lc3")
            if not libpath:
                raise InitializationError("LC3 library not found")

        lib = ctypes.cdll.LoadLibrary(libpath)

        if not all(
            hasattr(lib, func)
            for func in (
                "lc3_hr_frame_samples",
                "lc3_hr_frame_block_bytes",
                "lc3_hr_resolve_bitrate",
                "lc3_hr_delay_samples",
            )
        ):
            if self.hrmode:
                raise InitializationError("High-Resolution interface not available")

            lc3_hr_frame_samples = lambda hrmode, dt_us, sr_hz: lib.lc3_frame_samples(
                dt_us, sr_hz
            )
            lc3_hr_frame_block_bytes = (
                lambda hrmode, dt_us, sr_hz, num_channels, bitrate: num_channels
                * lib.lc3_frame_bytes(dt_us, bitrate // 2)
            )
            lc3_hr_resolve_bitrate = (
                lambda hrmode, dt_us, sr_hz, nbytes: lib.lc3_resolve_bitrate(
                    dt_us, nbytes
                )
            )
            lc3_hr_delay_samples = lambda hrmode, dt_us, sr_hz: lib.lc3_delay_samples(
                dt_us, sr_hz
            )
            setattr(lib, "lc3_hr_frame_samples", lc3_hr_frame_samples)
            setattr(lib, "lc3_hr_frame_block_bytes", lc3_hr_frame_block_bytes)
            setattr(lib, "lc3_hr_resolve_bitrate", lc3_hr_resolve_bitrate)
            setattr(lib, "lc3_hr_delay_samples", lc3_hr_delay_samples)

        lib.lc3_hr_frame_samples.argtypes = [c_bool, c_int, c_int]
        lib.lc3_hr_frame_block_bytes.argtypes = [c_bool, c_int, c_int, c_int, c_int]
        lib.lc3_hr_resolve_bitrate.argtypes = [c_bool, c_int, c_int, c_int]
        lib.lc3_hr_delay_samples.argtypes = [c_bool, c_int, c_int]
        self.lib = lib

        if not (libc_path := find_library("c")):
            raise InitializationError("Unable to find libc")
        libc = ctypes.cdll.LoadLibrary(libc_path)

        self.malloc = libc.malloc
        self.malloc.argtypes = [c_size_t]
        self.malloc.restype = c_void_p

        self.free = libc.free
        self.free.argtypes = [c_void_p]

    def get_frame_samples(self) -> int:
        """
        Returns the number of PCM samples in an LC3 frame.
        """
        ret = self.lib.lc3_hr_frame_samples(
            self.hrmode, self.frame_duration_us, self.pcm_sample_rate_hz
        )
        if ret < 0:
            raise InvalidArgumentError("Bad parameters")
        return ret

    def get_frame_bytes(self, bitrate: int) -> int:
        """
        Returns the size of LC3 frame blocks, from bitrate in bit per seconds.
        A target `bitrate` equals 0 or `INT32_MAX` returns respectively
        the minimum and maximum allowed size.
        """
        ret = self.lib.lc3_hr_frame_block_bytes(
            self.hrmode,
            self.frame_duration_us,
            self.sample_rate_hz,
            self.num_channels,
            bitrate,
        )
        if ret < 0:
            raise InvalidArgumentError("Bad parameters")
        return ret

    def resolve_bitrate(self, num_bytes: int) -> int:
        """
        Returns the bitrate in bits per seconds, from the size of LC3 frames.
        """
        ret = self.lib.lc3_hr_resolve_bitrate(
            self.hrmode, self.frame_duration_us, self.sample_rate_hz, num_bytes
        )
        if ret < 0:
            raise InvalidArgumentError("Bad parameters")
        return ret

    def get_delay_samples(self) -> int:
        """
        Returns the algorithmic delay, as a number of samples.
        """
        ret = self.lib.lc3_hr_delay_samples(
            self.hrmode, self.frame_duration_us, self.pcm_sample_rate_hz
        )
        if ret < 0:
            raise InvalidArgumentError("Bad parameters")
        return ret

    @classmethod
    def _resolve_pcm_format(cls, bit_depth: int | None) -> tuple[
        _PcmFormat,
        type[ctypes.c_int16] | type[ctypes.Array[ctypes.c_byte]] | type[ctypes.c_float],
    ]:
        match bit_depth:
            case 16:
                return (_PcmFormat.S16, ctypes.c_int16)
            case 24:
                return (_PcmFormat.S24_3LE, 3 * ctypes.c_byte)
            case None:
                return (_PcmFormat.FLOAT, ctypes.c_float)
            case _:
                raise InvalidArgumentError("Could not interpret PCM bit_depth")


class Encoder(_Base):
    """
    LC3 Encoder wrapper.

    The `frame_duration_us`, in microsecond, is any of 2500, 5000, 7500, or 10000.
    The `sample_rate_hz`, in Hertz, is any of 8000, 16000, 24000, 32000
    or 48000, unless High-Resolution mode is enabled. In High-Resolution mode,
    the `sample_rate_hz` is 48000 or 96000.

    By default, one channel is processed. When `num_channels` is greater than one,
    the PCM input stream is read interleaved and consecutives LC3 frames are
    output, for each channel.

    Optional arguments:
        hrmode               : Enable High-Resolution mode, default is `False`.
        input_sample_rate_hz : Input PCM samplerate, enable downsampling of input.
        libpath              : LC3 library path and name
    """

    class c_encoder_t(c_void_p):
        pass

    def __init__(
        self,
        frame_duration_us: int,
        sample_rate_hz: int,
        num_channels: int = 1,
        hrmode: bool = False,
        input_sample_rate_hz: int | None = None,
        libpath: str | None = None,
    ) -> None:

        super().__init__(
            frame_duration_us,
            sample_rate_hz,
            num_channels,
            hrmode,
            input_sample_rate_hz,
            libpath,
        )

        lib = self.lib

        if not all(
            hasattr(lib, func)
            for func in ("lc3_hr_encoder_size", "lc3_hr_setup_encoder")
        ):
            if self.hrmode:
                raise InitializationError("High-Resolution interface not available")

            lc3_hr_encoder_size = lambda hrmode, dt_us, sr_hz: lib.lc3_encoder_size(
                dt_us, sr_hz
            )

            lc3_hr_setup_encoder = (
                lambda hrmode, dt_us, sr_hz, sr_pcm_hz, mem: lib.lc3_setup_encoder(
                    dt_us, sr_hz, sr_pcm_hz, mem
                )
            )
            setattr(lib, "lc3_hr_encoder_size", lc3_hr_encoder_size)
            setattr(lib, "lc3_hr_setup_encoder", lc3_hr_setup_encoder)

        lib.lc3_hr_encoder_size.argtypes = [c_bool, c_int, c_int]
        lib.lc3_hr_encoder_size.restype = c_uint

        lib.lc3_hr_setup_encoder.argtypes = [c_bool, c_int, c_int, c_int, c_void_p]
        lib.lc3_hr_setup_encoder.restype = self.c_encoder_t

        lib.lc3_encode.argtypes = [
            self.c_encoder_t,
            c_int,
            c_void_p,
            c_int,
            c_int,
            c_void_p,
        ]

        def new_encoder():
            return lib.lc3_hr_setup_encoder(
                self.hrmode,
                self.frame_duration_us,
                self.sample_rate_hz,
                self.pcm_sample_rate_hz,
                self.malloc(
                    lib.lc3_hr_encoder_size(
                        self.hrmode, self.frame_duration_us, self.pcm_sample_rate_hz
                    )
                ),
            )

        self.__encoders = [new_encoder() for _ in range(num_channels)]

    def __del__(self) -> None:

        try:
            (self.free(encoder) for encoder in self.__encoders)
        finally:
            return

    @typing.overload
    def encode(
        self,
        pcm: bytes | bytearray | memoryview | Iterable[float],
        num_bytes: int,
        bit_depth: None = None,
    ) -> bytes: ...

    @typing.overload
    def encode(
        self, pcm: bytes | bytearray | memoryview, num_bytes: int, bit_depth: int
    ) -> bytes: ...

    def encode(self, pcm, num_bytes: int, bit_depth: int | None = None) -> bytes:
        """
        Encodes LC3 frame(s), for each channel.

        The `pcm` input is given in two ways. When no `bit_depth` is defined,
        it's a vector of floating point values from -1 to 1, coding the sample
        levels. When `bit_depth` is defined, `pcm` is interpreted as a byte-like
        object, each sample coded on `bit_depth` bits (16 or 24).
        The machine endianness, or little endian, is used for 16 or 24 bits
        width, respectively.
        In both cases, the `pcm` vector data is padded with zeros when
        its length is less than the required input samples for the encoder.
        Channels concatenation of encoded LC3 frames, of `nbytes`, is returned.
        """

        nchannels = self.num_channels
        frame_samples = self.get_frame_samples()

        (pcm_fmt, pcm_t) = self._resolve_pcm_format(bit_depth)
        pcm_len = nchannels * frame_samples

        if bit_depth is None:
            pcm_buffer = array.array("f", pcm)

            # Invert test to catch NaN
            if not abs(sum(pcm_buffer)) / frame_samples < 2:
                raise InvalidArgumentError("Out of range PCM input")

            padding = max(pcm_len - frame_samples, 0)
            pcm_buffer.extend(array.array("f", [0] * padding))

        else:
            padding = max(pcm_len * ctypes.sizeof(pcm_t) - len(pcm), 0)
            pcm_buffer = bytearray(pcm) + bytearray(padding)  # type: ignore

        data_buffer = (c_byte * num_bytes)()
        data_offset = 0

        for ich, encoder in enumerate(self.__encoders):

            pcm_offset = ich * ctypes.sizeof(pcm_t)
            pcm = (pcm_t * (pcm_len - ich)).from_buffer(pcm_buffer, pcm_offset)

            data_size = num_bytes // nchannels + int(ich < num_bytes % nchannels)
            data = (c_byte * data_size).from_buffer(data_buffer, data_offset)
            data_offset += data_size

            ret = self.lib.lc3_encode(encoder, pcm_fmt, pcm, nchannels, len(data), data)
            if ret < 0:
                raise InvalidArgumentError("Bad parameters")

        return bytes(data_buffer)


class Decoder(_Base):
    """
    LC3 Decoder wrapper.

    The `frame_duration_us`, in microsecond, is any of 2500, 5000, 7500, or 10000.
    The `sample_rate_hz`, in Hertz, is any of 8000, 16000, 24000, 32000
    or 48000, unless High-Resolution mode is enabled. In High-Resolution mode,
    the `sample_rate_hz` is 48000 or 96000.

    By default, one channel is processed. When `num_chanels` is greater than one,
    the PCM input stream is read interleaved and consecutives LC3 frames are
    output, for each channel.

    Optional arguments:
        hrmode                : Enable High-Resolution mode, default is `False`.
        output_sample_rate_hz : Output PCM sample_rate_hz, enable upsampling of output.
        libpath               : LC3 library path and name
    """

    class c_decoder_t(c_void_p):
        pass

    def __init__(
        self,
        frame_duration_us: int,
        sample_rate_hz: int,
        num_channels: int = 1,
        hrmode: bool = False,
        output_sample_rate_hz: int | None = None,
        libpath: str | None = None,
    ) -> None:

        super().__init__(
            frame_duration_us,
            sample_rate_hz,
            num_channels,
            hrmode,
            output_sample_rate_hz,
            libpath,
        )

        lib = self.lib

        if not all(
            hasattr(lib, func)
            for func in ("lc3_hr_decoder_size", "lc3_hr_setup_decoder")
        ):
            if self.hrmode:
                raise InitializationError("High-Resolution interface not available")

            lc3_hr_decoder_size = lambda hrmode, dt_us, sr_hz: lib.lc3_decoder_size(
                dt_us, sr_hz
            )

            lc3_hr_setup_decoder = (
                lambda hrmode, dt_us, sr_hz, sr_pcm_hz, mem: lib.lc3_setup_decoder(
                    dt_us, sr_hz, sr_pcm_hz, mem
                )
            )
            setattr(lib, "lc3_hr_decoder_size", lc3_hr_decoder_size)
            setattr(lib, "lc3_hr_setup_decoder", lc3_hr_setup_decoder)

        lib.lc3_hr_decoder_size.argtypes = [c_bool, c_int, c_int]
        lib.lc3_hr_decoder_size.restype = c_uint

        lib.lc3_hr_setup_decoder.argtypes = [c_bool, c_int, c_int, c_int, c_void_p]
        lib.lc3_hr_setup_decoder.restype = self.c_decoder_t

        lib.lc3_decode.argtypes = [
            self.c_decoder_t,
            c_void_p,
            c_int,
            c_int,
            c_void_p,
            c_int,
        ]

        def new_decoder():
            return lib.lc3_hr_setup_decoder(
                self.hrmode,
                self.frame_duration_us,
                self.sample_rate_hz,
                self.pcm_sample_rate_hz,
                self.malloc(
                    lib.lc3_hr_decoder_size(
                        self.hrmode, self.frame_duration_us, self.pcm_sample_rate_hz
                    )
                ),
            )

        self.__decoders = [new_decoder() for i in range(num_channels)]

    def __del__(self) -> None:

        try:
            (self.free(decoder) for decoder in self.__decoders)
        finally:
            return

    @typing.overload
    def decode(
        self, data: bytes | bytearray | memoryview | None, bit_depth: None = None
    ) -> array.array[float]: ...

    @typing.overload
    def decode(self, data: bytes | bytearray | memoryview | None, bit_depth: int) -> bytes: ...

    def decode(
        self, data: bytes | bytearray | memoryview | None, bit_depth: int | None = None
    ) -> bytes | array.array[float]:
        """
        Decodes an LC3 frame.

        The input `data` is the channels concatenation of LC3 frames in a
        byte-like object. Interleaved PCM samples are returned according to
        the `bit_depth` indication.
        Setting `data` to `None` enables PLC (Packet Loss Concealment)
        reconstruction for the block of LC3 frames.
        When no `bit_depth` is defined, it's a vector of floating point values
        from -1 to 1, coding the sample levels. When `bit_depth` is defined,
        it returns a byte array, each sample coded on `bit_depth` bits.
        The machine endianness, or little endian, is used for 16 or 24 bits
        width, respectively.
        """

        num_channels = self.num_channels

        (pcm_fmt, pcm_t) = self._resolve_pcm_format(bit_depth)
        pcm_len = num_channels * self.get_frame_samples()
        pcm_buffer = (pcm_t * pcm_len)()

        if data is not None:
            data_buffer = bytearray(data)
            data_offset = 0

        for ich, decoder in enumerate(self.__decoders):
            pcm_offset = ich * ctypes.sizeof(pcm_t)
            pcm = (pcm_t * (pcm_len - ich)).from_buffer(pcm_buffer, pcm_offset)

            if data is None:
                ret = self.lib.lc3_decode(
                    decoder, None, 0, pcm_fmt, pcm, self.num_channels
                )
            else:
                data_size = len(data_buffer) // num_channels + int(
                    ich < len(data_buffer) % num_channels
                )
                buf = (c_byte * data_size).from_buffer(data_buffer, data_offset)
                data_offset += data_size
                ret = self.lib.lc3_decode(
                    decoder, buf, len(buf), pcm_fmt, pcm, self.num_channels
                )

            if ret < 0:
                raise InvalidArgumentError("Bad parameters")

        return array.array("f", pcm_buffer) if bit_depth is None else bytes(pcm_buffer)
