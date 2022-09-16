/******************************************************************************
 *
 *  Copyright 2022 Google LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include "lc3.h"

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <numpy/ndarrayobject.h>

#include <lc3.c>

#define __CTYPES_LC3_C
#include "ctypes.h"

static PyObject *setup_encoder_py(PyObject *m, PyObject *args)
{
    int dt_us, sr_hz;

    if (!PyArg_ParseTuple(args, "ii", &dt_us, &sr_hz))
        return NULL;

    CTYPES_CHECK("dt_us", LC3_CHECK_DT_US(dt_us));
    CTYPES_CHECK("sr_hz", LC3_CHECK_SR_HZ(sr_hz));

    lc3_encoder_t encoder = lc3_setup_encoder(dt_us, sr_hz, 0,
            malloc(lc3_encoder_size(dt_us, sr_hz)));

    PyObject *encoder_obj = from_encoder(NULL, encoder);

    free(encoder);

    return Py_BuildValue("N", encoder_obj);
}

static PyObject *encode_py(PyObject *m, PyObject *args)
{
    PyObject *encoder_obj, *pcm_obj;
    int nbytes;
    int16_t *pcm;

    if (!PyArg_ParseTuple(args, "OOi", &encoder_obj, &pcm_obj, &nbytes))
        return NULL;

    lc3_encoder_t encoder =
        lc3_setup_encoder(10000, 48000, 0, &(lc3_encoder_mem_48k_t){ });

    CTYPES_CHECK(NULL, encoder_obj = to_encoder(encoder_obj, encoder));

    int ns = LC3_NS(encoder->dt, encoder->sr);

    CTYPES_CHECK("x", pcm_obj = to_1d_ptr(pcm_obj, NPY_INT16, ns, &pcm));
    CTYPES_CHECK("nbytes", nbytes >= 20 && nbytes <= 400);

    uint8_t out[nbytes];

    lc3_encode(encoder, LC3_PCM_FORMAT_S16, pcm, 1, nbytes, out);

    from_encoder(encoder_obj, encoder);

    return Py_BuildValue("N",
        PyBytes_FromStringAndSize((const char *)out, nbytes));
}

static PyObject *setup_decoder_py(PyObject *m, PyObject *args)
{
    int dt_us, sr_hz;

    if (!PyArg_ParseTuple(args, "ii", &dt_us, &sr_hz))
        return NULL;

    CTYPES_CHECK("dt_us", LC3_CHECK_DT_US(dt_us));
    CTYPES_CHECK("sr_hz", LC3_CHECK_SR_HZ(sr_hz));

    lc3_decoder_t decoder = lc3_setup_decoder(dt_us, sr_hz, 0,
            malloc(lc3_decoder_size(dt_us, sr_hz)));

    PyObject *decoder_obj = from_decoder(NULL, decoder);

    free(decoder);

    return Py_BuildValue("N", decoder_obj);
}

static PyObject *decode_py(PyObject *m, PyObject *args)
{
    PyObject *decoder_obj, *pcm_obj, *in_obj;
    int16_t *pcm;

    if (!PyArg_ParseTuple(args, "OO", &decoder_obj, &in_obj))
        return NULL;

    CTYPES_CHECK("in", in_obj == Py_None || PyBytes_Check(in_obj));

    char *in = in_obj == Py_None ? NULL : PyBytes_AsString(in_obj);
    int nbytes = in_obj == Py_None ? 0 : PyBytes_Size(in_obj);

    lc3_decoder_t decoder =
        lc3_setup_decoder(10000, 48000, 0, &(lc3_decoder_mem_48k_t){ });

    CTYPES_CHECK(NULL, decoder_obj = to_decoder(decoder_obj, decoder));

    int ns = LC3_NS(decoder->dt, decoder->sr);
    pcm_obj = new_1d_ptr(NPY_INT16, ns, &pcm);

    lc3_decode(decoder, in, nbytes, LC3_PCM_FORMAT_S16, pcm, 1);

    from_decoder(decoder_obj, decoder);

    return Py_BuildValue("N", pcm_obj);
}

static PyMethodDef methods[] = {
    { "setup_encoder"      , setup_encoder_py      , METH_VARARGS },
    { "encode"             , encode_py             , METH_VARARGS },
    { "setup_decoder"      , setup_decoder_py      , METH_VARARGS },
    { "decode"             , decode_py             , METH_VARARGS },
    { NULL },
};

PyMODINIT_FUNC lc3_interface_py_init(PyObject *m)
{
    import_array();

    PyModule_AddFunctions(m, methods);

    return m;
}
