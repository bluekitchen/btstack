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

#include <tns.c>
#include "ctypes.h"

static PyObject *compute_lpc_coeffs_py(PyObject *m, PyObject *args)
{
    PyObject *x_obj, *a_obj, *g_obj;
    unsigned dt, bw;
    float *x, *g, (*a)[9];

    if (!PyArg_ParseTuple(args, "IIO", &dt, &bw, &x_obj))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", bw < LC3_NUM_BANDWIDTH);

    int ne = lc3_ne(dt, bw);
    int maxorder = dt <= LC3_DT_5M ? 4 : 8;

    CTYPES_CHECK("x", to_1d_ptr(x_obj, NPY_FLOAT, ne, &x));

    g_obj = new_1d_ptr(NPY_FLOAT, 2, &g);
    a_obj = new_2d_ptr(NPY_FLOAT, 2, 9, &a);

    compute_lpc_coeffs(dt, bw, maxorder, x, g, a);

    return Py_BuildValue("NN", g_obj, a_obj);
}

static PyObject *lpc_reflection_py(PyObject *m, PyObject *args)
{
    PyObject *a_obj, *rc_obj;
    unsigned dt;
    float *a, *rc;

    if (!PyArg_ParseTuple(args, "IO", &dt, &a_obj))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);

    int maxorder = dt <= LC3_DT_5M ? 4 : 8;

    CTYPES_CHECK("a", to_1d_ptr(a_obj, NPY_FLOAT, 9, &a));
    rc_obj = new_1d_ptr(NPY_FLOAT, maxorder, &rc);

    lpc_reflection(a, maxorder, rc);

    return Py_BuildValue("N", rc_obj);
}

static PyObject *quantize_rc_py(PyObject *m, PyObject *args)
{
    PyObject *rc_obj, *rc_q_obj;
    unsigned dt;
    float *rc;
    int rc_order, *rc_q;

    if (!PyArg_ParseTuple(args, "iO", &dt, &rc_obj))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);

    int maxorder = dt <= LC3_DT_5M ? 4 : 8;

    CTYPES_CHECK("rc", to_1d_ptr(rc_obj, NPY_FLOAT, 8, &rc));
    rc_q_obj = new_1d_ptr(NPY_INT, 8, &rc_q);

    quantize_rc(rc, maxorder, &rc_order, rc_q);

    return Py_BuildValue("iN", rc_order, rc_q_obj);
}

static PyObject *unquantize_rc_py(PyObject *m, PyObject *args)
{
    PyObject *rc_q_obj, *rc_obj;
    int rc_order, *rc_q;
    float *rc;

    if (!PyArg_ParseTuple(args, "OI", &rc_q_obj, &rc_order))
        return NULL;

    CTYPES_CHECK("rc_q", to_1d_ptr(rc_q_obj, NPY_INT, 8, &rc_q));
    CTYPES_CHECK("rc_order", (unsigned)rc_order <= 8);

    rc_obj = new_1d_ptr(NPY_FLOAT, 8, &rc);

    unquantize_rc(rc_q, rc_order, rc);

    return Py_BuildValue("N", rc_obj);
}

static PyObject *analyze_py(PyObject *m, PyObject *args)
{
    PyObject *x_obj;
    struct lc3_tns_data data = { 0 };
    unsigned dt, bw;
    int nn_flag, nbytes;
    float *x;

    if (!PyArg_ParseTuple(args, "IIpiO",
            &dt, &bw, &nn_flag, &nbytes, &x_obj))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);
    CTYPES_CHECK("bw", bw < LC3_NUM_BANDWIDTH);

    int ne = lc3_ne(dt, bw);

    CTYPES_CHECK("x", x_obj = to_1d_ptr(x_obj, NPY_FLOAT, ne, &x));

    lc3_tns_analyze(dt, bw, nn_flag, nbytes, &data, x);

    return Py_BuildValue("ON", x_obj, new_tns_data(&data));
}

static PyObject *synthesize_py(PyObject *m, PyObject *args)
{
    PyObject *data_obj, *x_obj;
    unsigned dt, bw;
    struct lc3_tns_data data;
    float *x;

    if (!PyArg_ParseTuple(args, "IIOO", &dt, &bw, &data_obj, &x_obj))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);
    CTYPES_CHECK("bw", bw < LC3_NUM_BANDWIDTH);
    CTYPES_CHECK(NULL, data_obj = to_tns_data(data_obj, &data));

    int ne = lc3_ne(dt, bw);

    CTYPES_CHECK("x", x_obj = to_1d_ptr(x_obj, NPY_FLOAT, ne, &x));

    lc3_tns_synthesize(dt, bw, &data, x);

    return Py_BuildValue("O", x_obj);
}

static PyObject *get_nbits_py(PyObject *m, PyObject *args)
{
    PyObject *data_obj;
    struct lc3_tns_data data = { 0 };

    if (!PyArg_ParseTuple(args, "O", &data_obj))
        return NULL;

    CTYPES_CHECK("data", to_tns_data(data_obj, &data));

    int nbits = lc3_tns_get_nbits(&data);

    return Py_BuildValue("i", nbits);
}

static PyMethodDef methods[] = {
    { "tns_compute_lpc_coeffs", compute_lpc_coeffs_py, METH_VARARGS },
    { "tns_lpc_reflection"    , lpc_reflection_py    , METH_VARARGS },
    { "tns_quantize_rc"       , quantize_rc_py       , METH_VARARGS },
    { "tns_unquantize_rc"     , unquantize_rc_py     , METH_VARARGS },
    { "tns_analyze"           , analyze_py           , METH_VARARGS },
    { "tns_synthesize"        , synthesize_py        , METH_VARARGS },
    { "tns_get_nbits"         , get_nbits_py         , METH_VARARGS },
    { NULL },
};

PyMODINIT_FUNC lc3_tns_py_init(PyObject *m)
{
    import_array();

    PyModule_AddFunctions(m, methods);

    return m;
}
