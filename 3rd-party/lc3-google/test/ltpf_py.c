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

#include <ltpf.c>
#include "ctypes.h"

static PyObject *resample_py(PyObject *m, PyObject *args)
{
    unsigned dt, sr;
    PyObject *hp50_obj, *x_obj, *y_obj;
    struct lc3_ltpf_hp50_state hp50;
    int16_t *x, *y;

    if (!PyArg_ParseTuple(args, "IIOOO", &dt, &sr, &hp50_obj, &x_obj, &y_obj))
        return NULL;

    CTYPES_CHECK("dt", (unsigned)dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", (unsigned)sr < LC3_NUM_SRATE);
    CTYPES_CHECK(NULL, hp50_obj = to_ltpf_hp50_state(hp50_obj, &hp50));

    int ns = lc3_ns(dt, sr), nt = lc3_nt(sr);
    int ny = sizeof((struct lc3_ltpf_analysis){ }.x_12k8) / sizeof(int16_t);
    int n  = (1 + dt) * 32;

    CTYPES_CHECK("x", x_obj = to_1d_ptr(x_obj, NPY_INT16, ns+nt, &x));
    CTYPES_CHECK("y", y_obj = to_1d_ptr(y_obj, NPY_INT16, ny, &y));

    resample_12k8[sr](&hp50, x + nt, y + (ny - n), n);

    from_ltpf_hp50_state(hp50_obj, &hp50);
    return Py_BuildValue("O", y_obj);
}

static PyObject *analyse_py(PyObject *m, PyObject *args)
{
    PyObject *ltpf_obj, *x_obj;
    unsigned dt, sr;
    struct lc3_ltpf_analysis ltpf;
    struct lc3_ltpf_data data = { 0 };
    int16_t *x;

    if (!PyArg_ParseTuple(args, "IIOO", &dt, &sr, &ltpf_obj, &x_obj))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", sr < LC3_NUM_SRATE);
    CTYPES_CHECK(NULL, ltpf_obj = to_ltpf_analysis(ltpf_obj, &ltpf));

    int ns = lc3_ns(dt, sr), nt = lc3_nt(sr);

    CTYPES_CHECK("x", x_obj = to_1d_ptr(x_obj, NPY_INT16, ns+nt, &x));

    int pitch_present =
        lc3_ltpf_analyse(dt, sr, &ltpf, x + nt, &data);

    from_ltpf_analysis(ltpf_obj, &ltpf);
    return Py_BuildValue("iN", pitch_present, new_ltpf_data(&data));
}

static PyObject *synthesize_py(PyObject *m, PyObject *args)
{
    PyObject *ltpf_obj, *data_obj, *x_obj;
    struct lc3_ltpf_synthesis ltpf;
    struct lc3_ltpf_data data;
    bool pitch_present;
    unsigned dt, sr;
    int nbytes;
    float *x;

    if (!PyArg_ParseTuple(args, "IIiOOO",
                &dt, &sr, &nbytes, &ltpf_obj, &data_obj, &x_obj))
        return NULL;

    CTYPES_CHECK("dt", dt < LC3_NUM_DT);
    CTYPES_CHECK("sr", sr < LC3_NUM_SRATE);
    CTYPES_CHECK("nbytes", nbytes >= 20 && nbytes <= 400);
    CTYPES_CHECK(NULL, ltpf_obj = to_ltpf_synthesis(ltpf_obj, &ltpf));

    if ((pitch_present = (data_obj != Py_None)))
        CTYPES_CHECK(NULL, data_obj = to_ltpf_data(data_obj, &data));

    int ns = lc3_ns(dt,sr), nd = 18 * (lc3_ns_4m[sr] / 4);

    CTYPES_CHECK("x", x_obj = to_1d_ptr(x_obj, NPY_FLOAT, nd+ns, &x));

    lc3_ltpf_synthesize(dt, sr, nbytes,
        &ltpf, pitch_present ? &data : NULL, x, x + nd);

    from_ltpf_synthesis(ltpf_obj, &ltpf);
    return Py_BuildValue("O", x_obj);
}

static PyObject *get_nbits_py(PyObject *m, PyObject *args)
{
    int pitch_present;

    if (!PyArg_ParseTuple(args, "i", &pitch_present))
        return NULL;

    int nbits = lc3_ltpf_get_nbits(pitch_present);

    return Py_BuildValue("i", nbits);
}

static PyMethodDef methods[] = {
    { "ltpf_resample"  , resample_py  , METH_VARARGS },
    { "ltpf_analyse"   , analyse_py   , METH_VARARGS },
    { "ltpf_synthesize", synthesize_py, METH_VARARGS },
    { "ltpf_get_nbits" , get_nbits_py , METH_VARARGS },
    { NULL },
};

PyMODINIT_FUNC lc3_ltpf_py_init(PyObject *m)
{
    import_array();

    PyModule_AddFunctions(m, methods);

    return m;
}
