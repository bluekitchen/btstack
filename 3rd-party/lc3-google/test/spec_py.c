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
    unsigned dt, sr;
    int nbytes, nbits_budget, g_off;
    float nbits_off;
    PyObject *x_obj;
    float *x;

    if (!PyArg_ParseTuple(args, "IIOiifi", &dt, &sr,
                &x_obj, &nbytes, &nbits_budget, &nbits_off, &g_off))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", sr < LC3_NUM_SRATE);

    int ne = lc3_ne(dt, sr);

    CTYPES_CHECK("x", x_obj = to_1d_ptr(x_obj, NPY_FLOAT, ne, &x));

    int g_min;
    bool reset_off;

    int g_int = estimate_gain(dt, sr,
        x, nbytes, nbits_budget, nbits_off, g_off, &reset_off, &g_min);

    return Py_BuildValue("iii", g_int, reset_off, g_min);
}

static PyObject *adjust_gain_py(PyObject *m, PyObject *args)
{
    unsigned dt, sr;
    int g_idx, nbits, nbits_budget, g_idx_min;

    if (!PyArg_ParseTuple(args, "IIiiii", &dt, &sr,
                &g_idx, &nbits, &nbits_budget, &g_idx_min))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", sr < LC3_NUM_SRATE);
    CTYPES_CHECK("g_idx", g_idx >= 0 && g_idx <= 255);

    g_idx = adjust_gain(dt, sr, g_idx, nbits, nbits_budget, g_idx_min);

    return Py_BuildValue("i", g_idx);
}

static PyObject *quantize_py(PyObject *m, PyObject *args)
{
    unsigned dt, sr;
    int g_int;
    PyObject *x_obj;
    float *x;
    int nq;

    if (!PyArg_ParseTuple(args, "IIiO", &dt, &sr, &g_int, &x_obj))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", sr < LC3_NUM_SRATE);
    CTYPES_CHECK("g_int", g_int >= -255 && g_int <= 255);

    int ne = lc3_ne(dt, sr);

    CTYPES_CHECK("x", x_obj = to_1d_ptr(x_obj, NPY_FLOAT, ne, &x));

    quantize(dt, sr, g_int, x, &nq);

    return Py_BuildValue("Oi", x_obj, nq);
}

static PyObject *compute_nbits_py(PyObject *m, PyObject *args)
{
    unsigned dt, sr;
    int nbytes, nq, nbits_budget;
    PyObject *x_obj;
    float *x;

    if (!PyArg_ParseTuple(args, "IIiOii", &dt, &sr,
                &nbytes, &x_obj, &nq, &nbits_budget))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", sr < LC3_NUM_SRATE);

    int ne = lc3_ne(dt, sr);

    CTYPES_CHECK("x", x_obj = to_1d_ptr(x_obj, NPY_FLOAT, ne, &x));

    bool lsb_mode;

    int nbits = compute_nbits(
        dt, sr, nbytes, x, &nq, nbits_budget, &lsb_mode);

    return Py_BuildValue("iii", nbits, nq, lsb_mode);
}

static PyObject *analyze_py(PyObject *m, PyObject *args)
{
    unsigned dt, sr;
    int nbytes, pitch;

    PyObject *tns_obj, *spec_obj, *x_obj;
    struct lc3_tns_data tns = { 0 };
    struct lc3_spec_analysis spec = { 0 };
    struct lc3_spec_side side = { 0 };
    float *x;

    if (!PyArg_ParseTuple(args, "IIipOOO", &dt, &sr,
                &nbytes, &pitch, &tns_obj, &spec_obj, &x_obj))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", sr < LC3_NUM_SRATE);

    int ne = lc3_ne(dt, sr);

    CTYPES_CHECK(NULL, tns_obj = to_tns_data(tns_obj, &tns));
    CTYPES_CHECK(NULL, spec_obj = to_spec_analysis(spec_obj, &spec));
    CTYPES_CHECK("x", x_obj = to_1d_ptr(x_obj, NPY_FLOAT, ne, &x));

    lc3_spec_analyze(dt, sr, nbytes, pitch, &tns, &spec, x, &side);

    from_spec_analysis(spec_obj, &spec);
    return Py_BuildValue("ON", x_obj, new_spec_side(&side));
}

static PyObject *estimate_noise_py(PyObject *m, PyObject *args)
{
    unsigned dt, bw;
    PyObject *x_obj;
    float *x;
    int hrmode, n;

    if (!PyArg_ParseTuple(args, "IIpOI",
                &dt, &bw, &hrmode, &x_obj, &n))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);
    CTYPES_CHECK("bw", bw < LC3_NUM_BANDWIDTH);

    int ne = lc3_ne(dt, bw);

    CTYPES_CHECK("x" , x_obj = to_1d_ptr(x_obj, NPY_FLOAT, ne, &x ));

    int noise_factor = estimate_noise(dt, bw, hrmode, x, n);

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
