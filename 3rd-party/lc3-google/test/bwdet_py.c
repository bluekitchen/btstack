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

#include <bwdet.c>
#include "ctypes.h"

static PyObject *bwdet_run_py(PyObject *m, PyObject *args)
{
    unsigned dt, sr;
    PyObject *e_obj;
    float *e;

    if (!PyArg_ParseTuple(args, "IIO", &dt, &sr, &e_obj))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", sr < LC3_NUM_SRATE);
    CTYPES_CHECK("e", to_1d_ptr(e_obj, NPY_FLOAT, LC3_MAX_BANDS, &e));

    int bw = lc3_bwdet_run(dt, sr, e);

    return Py_BuildValue("i", bw);
}

static PyMethodDef methods[] = {
    { "bwdet_run", bwdet_run_py, METH_VARARGS },
    { NULL },
};

PyMODINIT_FUNC lc3_bwdet_py_init(PyObject *m)
{
    import_array();

    PyModule_AddFunctions(m, methods);

    return m;
}
