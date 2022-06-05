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

#include <Python.h>
#include <numpy/ndarrayobject.h>

#include <mdct.c>
#include "ctypes.h"


static PyObject *mdct_forward_py(PyObject *m, PyObject *args)
{
    PyObject *x_obj, *xd_obj, *y_obj, *d_obj;
    enum lc3_dt dt;
    enum lc3_srate sr;
    float *x, *xd, *y, *d;

    if (!PyArg_ParseTuple(args, "iiOO", &dt, &sr, &x_obj, &xd_obj))
        return NULL;

    CTYPES_CHECK("dt", (unsigned)dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", (unsigned)sr < LC3_NUM_SRATE);

    int ns = LC3_NS(dt, sr), nd = LC3_ND(dt, sr);

    CTYPES_CHECK("x", to_1d_ptr(x_obj, NPY_FLOAT, ns, &x));
    CTYPES_CHECK("xd", to_1d_ptr(xd_obj, NPY_FLOAT, nd, &xd));
    d_obj = new_1d_ptr(NPY_FLOAT, nd, &d);
    y_obj = new_1d_ptr(NPY_FLOAT, ns, &y);

    memcpy(d, xd, nd * sizeof(float));

    lc3_mdct_forward(dt, sr, sr, x, d, y);

    return Py_BuildValue("NN", y_obj, d_obj);
}

static PyObject *mdct_inverse_py(PyObject *m, PyObject *args)
{
    PyObject *x_obj, *xd_obj, *d_obj, *y_obj;
    enum lc3_dt dt;
    enum lc3_srate sr;
    float *x, *xd, *d, *y;

    if (!PyArg_ParseTuple(args, "iiOO", &dt, &sr, &x_obj, &xd_obj))
        return NULL;

    CTYPES_CHECK("dt", (unsigned)dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", (unsigned)sr < LC3_NUM_SRATE);

    int ns = LC3_NS(dt, sr), nd = LC3_ND(dt, sr);

    CTYPES_CHECK("x", to_1d_ptr(x_obj, NPY_FLOAT, ns, &x));
    CTYPES_CHECK("xd", to_1d_ptr(xd_obj, NPY_FLOAT, nd, &xd));
    d_obj = new_1d_ptr(NPY_FLOAT, nd, &d);
    y_obj = new_1d_ptr(NPY_FLOAT, ns, &y);

    memcpy(d, xd, nd * sizeof(float));

    lc3_mdct_inverse(dt, sr, sr, x, d, y);

    return Py_BuildValue("NN", y_obj, d_obj);
}

static PyMethodDef methods[] = {
    { "mdct_forward", mdct_forward_py, METH_VARARGS },
    { "mdct_inverse", mdct_inverse_py, METH_VARARGS },
    { NULL },
};

PyMODINIT_FUNC lc3_mdct_py_init(PyObject *m)
{
    import_array();

    PyModule_AddFunctions(m, methods);

    return m;
}
