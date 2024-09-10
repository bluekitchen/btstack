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
#include <Python.h>
#include <numpy/ndarrayobject.h>

#include <sns.c>
#include "ctypes.h"

static PyObject *compute_scale_factors_py(PyObject *m, PyObject *args)
{
    unsigned dt, sr;
    int nbytes, att;
    PyObject *eb_obj, *scf_obj;
    float *eb, *scf;

    if (!PyArg_ParseTuple(args, "IIiOp", &dt, &sr, &nbytes, &eb_obj, &att))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", sr < LC3_NUM_SRATE);

    int nb = lc3_num_bands[dt][sr];

    CTYPES_CHECK("eb", to_1d_ptr(eb_obj, NPY_FLOAT, nb, &eb));
    scf_obj = new_1d_ptr(NPY_FLOAT, 16, &scf);

    compute_scale_factors(dt, sr, nbytes, eb, att, scf);

    return Py_BuildValue("N", scf_obj);
}

static PyObject *resolve_codebooks_py(PyObject *m, PyObject *args)
{
    PyObject *scf_obj;
    float *scf;
    int lfcb_idx, hfcb_idx;

    if (!PyArg_ParseTuple(args, "O", &scf_obj))
        return NULL;

    CTYPES_CHECK("eb", to_1d_ptr(scf_obj, NPY_FLOAT, 16, &scf));

    resolve_codebooks(scf, &lfcb_idx, &hfcb_idx);

    return Py_BuildValue("ii", lfcb_idx, hfcb_idx);
}

static PyObject *quantize_py(PyObject *m, PyObject *args)
{
    PyObject *scf_obj, *y_obj, *yn_obj;
    float *scf;

    int lfcb_idx, hfcb_idx;
    int shape_idx, gain_idx;
    float (*yn)[16];
    int (*y)[16];

    if (!PyArg_ParseTuple(args, "Oii", &scf_obj, &lfcb_idx, &hfcb_idx))
        return NULL;

    CTYPES_CHECK("scf", to_1d_ptr(scf_obj, NPY_FLOAT, 16, &scf));
    CTYPES_CHECK("lfcb_idx", (unsigned)lfcb_idx < 32);
    CTYPES_CHECK("hfcb_idx", (unsigned)hfcb_idx < 32);

    y_obj = new_2d_ptr(NPY_INT, 4, 16, &y);
    yn_obj = new_2d_ptr(NPY_FLOAT, 4, 16, &yn);

    quantize(scf, lfcb_idx, hfcb_idx,
        y, yn, &shape_idx, &gain_idx);

    return Py_BuildValue("NNii", y_obj, yn_obj, shape_idx, gain_idx);
}

static PyObject *unquantize_py(PyObject *m, PyObject *args)
{
    PyObject *y_obj, *scf_obj;
    int lfcb_idx, hfcb_idx;
    int shape, gain;
    float *y, *scf;

    if (!PyArg_ParseTuple(args, "iiOii",
                &lfcb_idx, &hfcb_idx, &y_obj, &shape, &gain))
        return NULL;

    CTYPES_CHECK("lfcb_idx", (unsigned)lfcb_idx < 32);
    CTYPES_CHECK("hfcb_idx", (unsigned)hfcb_idx < 32);
    CTYPES_CHECK("y", to_1d_ptr(y_obj, NPY_FLOAT, 16, &y));
    CTYPES_CHECK("shape", (unsigned)shape < 4);
    CTYPES_CHECK("gain",
        (unsigned)gain < (unsigned)lc3_sns_vq_gains[shape].count);

    scf_obj = new_1d_ptr(NPY_FLOAT, 16, &scf);

    unquantize(lfcb_idx, hfcb_idx, y, shape, gain, scf);

    return Py_BuildValue("N", scf_obj);
}

static PyObject *spectral_shaping_py(PyObject *m, PyObject *args)
{
    PyObject *scf_q_obj, *x_obj;
    unsigned dt, sr;
    float *scf_q, *x;
    int inv;

    if (!PyArg_ParseTuple(args, "IIOpO", &dt, &sr, &scf_q_obj, &inv, &x_obj))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", sr < LC3_NUM_SRATE);

    int ne = lc3_ne(dt, sr);

    CTYPES_CHECK("scf_q", to_1d_ptr(scf_q_obj, NPY_FLOAT, 16, &scf_q));
    CTYPES_CHECK("x", x_obj = to_1d_ptr(x_obj, NPY_FLOAT, ne, &x));

    spectral_shaping(dt, sr, scf_q, inv, x, x);

    return Py_BuildValue("O", x_obj);
}

static PyObject *analyze_py(PyObject *m, PyObject *args)
{
    PyObject *eb_obj, *x_obj;
    struct lc3_sns_data data = { 0 };
    unsigned dt, sr;
    int nbytes, att;
    float *eb, *x;

    if (!PyArg_ParseTuple(args, "IIiOpO",
            &dt, &sr, &nbytes, &eb_obj, &att, &x_obj))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", sr < LC3_NUM_SRATE);

    int ne = lc3_ne(dt, sr);
    int nb = lc3_num_bands[dt][sr];

    CTYPES_CHECK("eb", to_1d_ptr(eb_obj, NPY_FLOAT, nb, &eb));
    CTYPES_CHECK("x", x_obj = to_1d_ptr(x_obj, NPY_FLOAT, ne, &x));

    lc3_sns_analyze(dt, sr, nbytes, eb, att, &data, x, x);

    return Py_BuildValue("ON", x_obj, new_sns_data(&data));
}

static PyObject *synthesize_py(PyObject *m, PyObject *args)
{
    PyObject *data_obj, *x_obj;
    struct lc3_sns_data data;
    unsigned dt, sr;
    float *x;

    if (!PyArg_ParseTuple(args, "IIOO", &dt, &sr, &data_obj, &x_obj))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", sr < LC3_NUM_SRATE);
    CTYPES_CHECK(NULL, data_obj = to_sns_data(data_obj, &data));

    int ne = lc3_ne(dt, sr);

    CTYPES_CHECK("x", x_obj = to_1d_ptr(x_obj, NPY_FLOAT, ne, &x));

    lc3_sns_synthesize(dt, sr, &data, x, x);

    return Py_BuildValue("O", x_obj);
}

static PyObject *get_nbits_py(PyObject *m, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    int nbits = lc3_sns_get_nbits();

    return Py_BuildValue("i", nbits);
}

static PyMethodDef methods[] = {
    { "sns_compute_scale_factors", compute_scale_factors_py, METH_VARARGS },
    { "sns_resolve_codebooks"    , resolve_codebooks_py    , METH_VARARGS },
    { "sns_quantize"             , quantize_py             , METH_VARARGS },
    { "sns_unquantize"           , unquantize_py           , METH_VARARGS },
    { "sns_spectral_shaping"     , spectral_shaping_py     , METH_VARARGS },
    { "sns_analyze"              , analyze_py              , METH_VARARGS },
    { "sns_synthesize"           , synthesize_py           , METH_VARARGS },
    { "sns_get_nbits"            , get_nbits_py            , METH_VARARGS },
    { NULL },
};

PyMODINIT_FUNC lc3_sns_py_init(PyObject *m)
{
    import_array();

    PyModule_AddFunctions(m, methods);

    return m;
}
