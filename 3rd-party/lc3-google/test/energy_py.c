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

#include <energy.c>
#include "ctypes.h"

static PyObject *energy_compute_py(PyObject *m, PyObject *args)
{
    unsigned dt, sr;
    PyObject *x_obj, *e_obj;
    float *x, *e;

    if (!PyArg_ParseTuple(args, "IIO", &dt, &sr, &x_obj))
        return NULL;

    CTYPES_CHECK("dt", (unsigned)dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", (unsigned)sr < LC3_NUM_SRATE);

    int ns = LC3_NS(dt, sr);

    CTYPES_CHECK("x", to_1d_ptr(x_obj, NPY_FLOAT, ns, &x));
    e_obj = new_1d_ptr(NPY_FLOAT, LC3_NUM_BANDS, &e);

    int nn_flag = lc3_energy_compute(dt, sr, x, e);

    return Py_BuildValue("Ni", e_obj, nn_flag);
}

static PyMethodDef methods[] = {
    { "energy_compute", energy_compute_py, METH_VARARGS },
    { NULL },
};

PyMODINIT_FUNC lc3_energy_py_init(PyObject *m)
{
    import_array();

    PyModule_AddFunctions(m, methods);

    return m;
}
