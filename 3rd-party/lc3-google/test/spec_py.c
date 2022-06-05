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

#include <spec.c>
#include "ctypes.h"

static PyObject *estimate_gain_py(PyObject *m, PyObject *args)
{
    PyObject *x_obj;
    unsigned dt, sr;
    float *x;
    int nbits_budget;
    float nbits_off;
    int g_off;
    bool reset_off;

    if (!PyArg_ParseTuple(args, "IIOifi", &dt, &sr,
                &x_obj, &nbits_budget, &nbits_off, &g_off))
        return NULL;

    CTYPES_CHECK("dt", (unsigned)dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", (unsigned)sr < LC3_NUM_SRATE);

    int ne = LC3_NE(dt, sr);

    CTYPES_CHECK("x", x_obj = to_1d_ptr(x_obj, NPY_FLOAT, ne, &x));

    int g_int = estimate_gain(dt, sr,
        x, nbits_budget, nbits_off, g_off, &reset_off);

    return Py_BuildValue("ii", g_int, reset_off);
}

static PyObject *adjust_gain_py(PyObject *m, PyObject *args)
{
    unsigned sr;
    int g_idx, nbits, nbits_budget;

    if (!PyArg_ParseTuple(args, "Iiii", &sr, &g_idx, &nbits, &nbits_budget))
        return NULL;

    CTYPES_CHECK("sr", (unsigned)sr < LC3_NUM_SRATE);
    CTYPES_CHECK("g_idx", g_idx >= 0 && g_idx <= 255);

    g_idx = adjust_gain(sr, g_idx, nbits, nbits_budget);

    return Py_BuildValue("i", g_idx);
}

static PyObject *quantize_py(PyObject *m, PyObject *args)
{
    PyObject *x_obj, *xq_obj;
    unsigned dt, sr;
    float *x;
    int16_t *xq;
    int g_int, nq;

    if (!PyArg_ParseTuple(args, "IIiO", &dt, &sr, &g_int, &x_obj))
        return NULL;

    CTYPES_CHECK("dt", (unsigned)dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", (unsigned)sr < LC3_NUM_SRATE);
    CTYPES_CHECK("g_int", g_int >= -255 && g_int <= 255);

    int ne = LC3_NE(dt, sr);

    CTYPES_CHECK("x", x_obj = to_1d_ptr(x_obj, NPY_FLOAT, ne, &x));

    xq_obj = new_1d_ptr(NPY_INT16, ne, &xq);
    uint16_t __xq[ne];

    quantize(dt, sr, g_int, x, __xq, &nq);

    for (int i = 0; i < nq; i++)
        xq[i] = __xq[i] & 1 ? -(__xq[i] >> 1) : (__xq[i] >> 1);

    return Py_BuildValue("ONi", x_obj, xq_obj, nq);
}

static PyObject *compute_nbits_py(PyObject *m, PyObject *args)
{
    PyObject *xq_obj;
    unsigned dt, sr, nbytes;
    int16_t *xq;
    int nq, nbits_budget;
    bool lsb_mode;

    if (!PyArg_ParseTuple(args, "IIIOii", &dt, &sr,
                &nbytes, &xq_obj, &nq, &nbits_budget))
        return NULL;

    CTYPES_CHECK("dt", (unsigned)dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", (unsigned)sr < LC3_NUM_SRATE);

    int ne = LC3_NE(dt, sr);

    CTYPES_CHECK("xq", xq_obj = to_1d_ptr(xq_obj, NPY_INT16, ne, &xq));

    uint16_t __xq[ne];
    for (int i = 0; i < ne; i++)
        __xq[i] = xq[i] < 0 ? (-xq[i] << 1) + 1 : (xq[i] << 1);

    int nbits = compute_nbits(
        dt, sr, nbytes, __xq, &nq, nbits_budget, &lsb_mode);

    return Py_BuildValue("iii", nbits, nq, lsb_mode);
}

static PyObject *analyze_py(PyObject *m, PyObject *args)
{
    PyObject *tns_obj, *spec_obj, *x_obj, *xq_obj;
    struct lc3_tns_data tns = { 0 };
    struct lc3_spec_analysis spec = { 0 };
    struct lc3_spec_side side = { 0 };
    unsigned dt, sr, nbytes;
    int pitch;
    float *x;
    int16_t *xq;

    if (!PyArg_ParseTuple(args, "IIIpOOO", &dt, &sr, &nbytes,
                &pitch, &tns_obj, &spec_obj, &x_obj))
        return NULL;

    CTYPES_CHECK("dt", (unsigned)dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", (unsigned)sr < LC3_NUM_SRATE);

    int ne = LC3_NE(dt, sr);

    CTYPES_CHECK(NULL, tns_obj = to_tns_data(tns_obj, &tns));
    CTYPES_CHECK(NULL, spec_obj = to_spec_analysis(spec_obj, &spec));
    CTYPES_CHECK("x", x_obj = to_1d_ptr(x_obj, NPY_FLOAT, ne, &x));

    xq_obj = new_1d_ptr(NPY_INT16, ne, &xq);
    uint16_t __xq[ne];

    lc3_spec_analyze(dt, sr, nbytes, pitch, &tns, &spec, x, __xq, &side);

    for (int i = 0; i < ne; i++)
        xq[i] = __xq[i] & 1 ? -(__xq[i] >> 1) : (__xq[i] >> 1);

    from_spec_analysis(spec_obj, &spec);
    return Py_BuildValue("ONN", x_obj, xq_obj, new_spec_side(&side));
}

static PyObject *estimate_noise_py(PyObject *m, PyObject *args)
{
    PyObject *x_obj, *xq_obj;
    unsigned dt, bw;
    int16_t *xq;
    float *x;
    int nq;

    if (!PyArg_ParseTuple(args, "IIOIO", &dt, &bw, &xq_obj, &nq, &x_obj))
        return NULL;

    CTYPES_CHECK("dt", (unsigned)dt < LC3_NUM_DT);
    CTYPES_CHECK("bw", (unsigned)bw < LC3_NUM_BANDWIDTH);

    int ne = LC3_NE(dt, bw);

    CTYPES_CHECK("xq", xq_obj = to_1d_ptr(xq_obj, NPY_INT16, ne, &xq));
    CTYPES_CHECK("x" , x_obj = to_1d_ptr(x_obj, NPY_FLOAT, ne, &x ));

    uint16_t __xq[nq];
    for (int i = 0; i < nq; i++)
        __xq[i] = xq[i] < 0 ? (-xq[i] << 1) + 1 : (xq[i] << 1);

    int noise_factor = estimate_noise(dt, bw, __xq, nq, x);

    return Py_BuildValue("i", noise_factor);
}

static PyMethodDef methods[] = {
    { "spec_estimate_gain" , estimate_gain_py , METH_VARARGS },
    { "spec_adjust_gain"   , adjust_gain_py   , METH_VARARGS },
    { "spec_quantize"      , quantize_py      , METH_VARARGS },
    { "spec_compute_nbits" , compute_nbits_py , METH_VARARGS },
    { "spec_analyze"       , analyze_py       , METH_VARARGS },
    { "spec_estimate_noise", estimate_noise_py, METH_VARARGS },
    { NULL },
};

PyMODINIT_FUNC lc3_spec_py_init(PyObject *m)
{
    import_array();

    PyModule_AddFunctions(m, methods);

    return m;
}
