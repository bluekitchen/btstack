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

#include <attdet.c>
#include "ctypes.h"

static PyObject *attdet_run_py(PyObject *m, PyObject *args)
{
    unsigned dt, sr, nbytes;
    PyObject *attdet_obj, *x_obj;
    struct lc3_attdet_analysis attdet = { 0 };
    int16_t *x;

    if (!PyArg_ParseTuple(args, "IIIOO",
                &dt, &sr, &nbytes, &attdet_obj, &x_obj))
        return NULL;

    CTYPES_CHECK("dt", (unsigned)dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", (unsigned)sr < LC3_NUM_SRATE);
    CTYPES_CHECK(NULL, attdet_obj = to_attdet_analysis(attdet_obj, &attdet));

    int ns = LC3_NS(dt, sr);

    CTYPES_CHECK("x", x_obj = to_1d_ptr(x_obj, NPY_INT16, ns+6, &x));

    int att = lc3_attdet_run(dt, sr, nbytes, &attdet, x+6);

    from_attdet_analysis(attdet_obj, &attdet);
    return Py_BuildValue("i", att);
}

static PyMethodDef methods[] = {
    { "attdet_run", attdet_run_py, METH_VARARGS },
    { NULL },
};

PyMODINIT_FUNC lc3_attdet_py_init(PyObject *m)
{
    import_array();

    PyModule_AddFunctions(m, methods);

    return m;
}
