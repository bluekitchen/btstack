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

#ifndef __CTYPES_H
#define __CTYPES_H

#include <Python.h>
#include <numpy/ndarrayobject.h>

#include <stdbool.h>


#define CTYPES_CHECK(exc, t) \
    do { \
        if (!(t)) return (exc) ? PyErr_Format(PyExc_TypeError, exc) : NULL; \
    } while(0)


/**
 * From C types to Numpy Array types
 */

#define to_scalar(obj, t, ptr) \
    __to_scalar(obj, t, (void *)(ptr))

#define to_1d_ptr(obj, t, n, ptr) \
    __to_1d_ptr(obj, t, n, (void **)(ptr))

#define to_2d_ptr(obj, t, n1, n2, ptr) \
    __to_2d_ptr(obj, t, n1, n2, (void **)(ptr))

#define to_1d_copy(obj, t, ptr, n) \
    __to_1d_copy(obj, t, ptr, n)

#define to_2d_copy(obj, t, ptr, n1, n2) \
    __to_2d_copy(obj, t, ptr, n1, n2)


/**
 * From Numpy Array types to C types
 */

#define new_scalar(obj, ptr) \
    __new_scalar(obj, ptr)

#define new_1d_ptr(t, n, ptr) \
    __new_1d_ptr(t, n, (void **)(ptr))

#define new_2d_ptr(t, n1, n2, ptr) \
    __new_2d_ptr(t, n1, n2, (void **)(ptr))

#define new_1d_copy(t, n, src) \
    __new_1d_copy(t, n, src)

#define new_2d_copy(t, n1, n2, src) \
    __new_2d_copy(t, n1, n2, src)


/* -------------------------------------------------------------------------- */

__attribute__((unused))
static PyObject *__to_scalar(PyObject *obj, int t, void *ptr)
{
    obj = obj ? PyArray_FROMANY(obj, t, 0, 0, NPY_ARRAY_FORCECAST) : obj;
    if (!obj)
        return NULL;

    memcpy(ptr, PyArray_DATA((PyArrayObject *)obj),
        PyArray_NBYTES((PyArrayObject *)obj));

    return obj;
}

__attribute__((unused))
static PyObject *__to_1d_ptr(PyObject *obj, int t, int n, void **ptr)
{
    obj = obj ? PyArray_FROMANY(obj,
        t, 1, 1, NPY_ARRAY_FORCECAST|NPY_ARRAY_CARRAY) : obj;
    if (!obj || (n && PyArray_SIZE((PyArrayObject *)obj) != n))
        return NULL;

    *ptr = PyArray_DATA((PyArrayObject *)obj);
    return obj;
}

__attribute__((unused))
static PyObject *__to_2d_ptr(PyObject *obj, int t, int n1, int n2, void **ptr)
{
    obj = obj ? PyArray_FROMANY(obj,
        t, 2, 2, NPY_ARRAY_FORCECAST|NPY_ARRAY_CARRAY) : obj;
    if (!obj || (n1 && PyArray_DIMS((PyArrayObject *)obj)[0] != n1)
             || (n2 && PyArray_DIMS((PyArrayObject *)obj)[1] != n2))
        return NULL;

    *ptr = PyArray_DATA((PyArrayObject *)obj);
    return obj;
}

__attribute__((unused))
static PyObject *__to_1d_copy(PyObject *obj, int t, void *v, int n)
{
    void *src;

    if ((obj = to_1d_ptr(obj, t, n, &src)))
        memcpy(v, src, PyArray_NBYTES((PyArrayObject *)obj));

    return obj;
}

__attribute__((unused))
static PyObject *__to_2d_copy(PyObject *obj, int t, void *v, int n1, int n2)
{
    void *src;

    if ((obj = to_2d_ptr(obj, t, n1, n2, &src)))
        memcpy(v, src, PyArray_NBYTES((PyArrayObject *)obj));

    return obj;
}

/* -------------------------------------------------------------------------- */

__attribute__((unused))
static PyObject *__new_scalar(int t, const void *ptr)
{
    PyObject *obj = PyArray_SimpleNew(0, NULL, t);

    memcpy(PyArray_DATA((PyArrayObject *)obj), ptr,
        PyArray_NBYTES((PyArrayObject *)obj));

    return obj;
}

__attribute__((unused))
static PyObject *__new_1d_ptr(int t, int n, void **ptr)
{
    PyObject *obj = PyArray_SimpleNew(1, (const npy_intp []){ n }, t);

    *ptr = PyArray_DATA((PyArrayObject *)obj);
    return obj;
}

__attribute__((unused))
static PyObject *__new_2d_ptr(int t, int n1, int n2, void **ptr)
{
    PyObject *obj;

    obj = PyArray_SimpleNew(2, ((const npy_intp []){ n1, n2 }), t);

    *ptr = PyArray_DATA((PyArrayObject *)obj);
    return obj;
}

__attribute__((unused))
static PyObject *__new_1d_copy(int t, int n, const void *src)
{
    PyObject *obj;
    void *dst;

    if ((obj = new_1d_ptr(t, n, &dst)))
        memcpy(dst, src, PyArray_NBYTES((PyArrayObject *)obj));

    return obj;
}

__attribute__((unused))
static PyObject *__new_2d_copy(int t, int n1, int n2, const void *src)
{
    PyObject *obj;
    void *dst;

    if ((obj = new_2d_ptr(t, n1, n2, &dst)))
        memcpy(dst, src, PyArray_NBYTES((PyArrayObject *)obj));

    return obj;
}

/* -------------------------------------------------------------------------- */

#include <lc3.h>

__attribute__((unused))
static PyObject *to_attdet_analysis(
    PyObject *obj, struct lc3_attdet_analysis *attdet)
{
    CTYPES_CHECK("attdet", obj && PyDict_Check(obj));

    CTYPES_CHECK("attdet.en1", to_scalar(
        PyDict_GetItemString(obj, "en1"), NPY_INT32, &attdet->en1));

    CTYPES_CHECK("attdet.an1", to_scalar(
        PyDict_GetItemString(obj, "an1"), NPY_INT32, &attdet->an1));

    CTYPES_CHECK("attdet.p_att", to_scalar(
        PyDict_GetItemString(obj, "p_att"), NPY_INT, &attdet->p_att));

    return obj;
}

__attribute__((unused))
static PyObject *from_attdet_analysis(
    PyObject *obj, const struct lc3_attdet_analysis *attdet)
{
    if (!obj) obj = PyDict_New();

    PyDict_SetItemString(obj, "en1",
        new_scalar(NPY_INT32, &attdet->en1));

    PyDict_SetItemString(obj, "an1",
        new_scalar(NPY_INT32, &attdet->an1));

    PyDict_SetItemString(obj, "p_att",
        new_scalar(NPY_INT, &attdet->p_att));

    return obj;
}

/* -------------------------------------------------------------------------- */

#include <ltpf.h>

__attribute__((unused))
static PyObject *to_ltpf_hp50_state(
    PyObject *obj, struct lc3_ltpf_hp50_state *hp50)
{
    CTYPES_CHECK("hp50", obj && PyDict_Check(obj));

    CTYPES_CHECK("hp50.s1", to_scalar(
        PyDict_GetItemString(obj, "s1"), NPY_INT64, &hp50->s1));

    CTYPES_CHECK("hp50.s2", to_scalar(
        PyDict_GetItemString(obj, "s2"), NPY_INT64, &hp50->s2));

    return obj;
}

__attribute__((unused))
static PyObject *from_ltpf_hp50_state(
    PyObject *obj, const struct lc3_ltpf_hp50_state *hp50)
{
    PyDict_SetItemString(obj, "s1",
        new_scalar(NPY_INT64, &hp50->s1));

    PyDict_SetItemString(obj, "s2",
        new_scalar(NPY_INT64, &hp50->s2));

    return obj;
}

__attribute__((unused))
static PyObject *to_ltpf_analysis(
    PyObject *obj, struct lc3_ltpf_analysis *ltpf)
{
    PyObject *nc_obj, *x_12k8_obj, *x_6k4_obj;
    const int n_12k8 = sizeof(ltpf->x_12k8) / sizeof(*ltpf->x_12k8);
    const int n_6k4 = sizeof(ltpf->x_6k4) / sizeof(*ltpf->x_6k4);

    CTYPES_CHECK("ltpf", obj && PyDict_Check(obj));

    CTYPES_CHECK("ltpf.active", to_scalar(
        PyDict_GetItemString(obj, "active"), NPY_BOOL, &ltpf->active));

    CTYPES_CHECK("ltpf.pitch", to_scalar(
        PyDict_GetItemString(obj, "pitch"), NPY_INT, &ltpf->pitch));

    CTYPES_CHECK("ltpf.nc", nc_obj = to_1d_copy(
        PyDict_GetItemString(obj, "nc"), NPY_FLOAT, ltpf->nc, 2));
    PyDict_SetItemString(obj, "nc", nc_obj);

    CTYPES_CHECK(NULL, to_ltpf_hp50_state(
        PyDict_GetItemString(obj, "hp50"), &ltpf->hp50));

    CTYPES_CHECK("ltpf.x_12k8", x_12k8_obj = to_1d_copy(
        PyDict_GetItemString(obj, "x_12k8"), NPY_INT16, ltpf->x_12k8, n_12k8));
    PyDict_SetItemString(obj, "x_12k8", x_12k8_obj);

    CTYPES_CHECK("ltpf.x_6k4", x_6k4_obj = to_1d_copy(
        PyDict_GetItemString(obj, "x_6k4"), NPY_INT16, ltpf->x_6k4, n_6k4));
    PyDict_SetItemString(obj, "x_6k4", x_6k4_obj);

    CTYPES_CHECK("ltpf.tc", to_scalar(
        PyDict_GetItemString(obj, "tc"), NPY_INT, &ltpf->tc));

    return obj;
}

__attribute__((unused))
static PyObject *from_ltpf_analysis(
    PyObject *obj, const struct lc3_ltpf_analysis *ltpf)
{
    const int n_12k8 = sizeof(ltpf->x_12k8) / sizeof(*ltpf->x_12k8);
    const int n_6k4 = sizeof(ltpf->x_6k4) / sizeof(*ltpf->x_6k4);

    if (!obj) obj = PyDict_New();

    PyDict_SetItemString(obj, "active",
        new_scalar(NPY_BOOL, &ltpf->active));

    PyDict_SetItemString(obj, "pitch",
        new_scalar(NPY_INT, &ltpf->pitch));

    PyDict_SetItemString(obj, "nc",
        new_1d_copy(NPY_FLOAT, 2, &ltpf->nc));

    PyDict_SetItemString(obj, "hp50",
        from_ltpf_hp50_state(PyDict_New(), &ltpf->hp50));

    PyDict_SetItemString(obj, "x_12k8",
        new_1d_copy(NPY_INT16, n_12k8, &ltpf->x_12k8));

    PyDict_SetItemString(obj, "x_6k4",
        new_1d_copy(NPY_INT16, n_6k4, &ltpf->x_6k4));

    PyDict_SetItemString(obj, "tc",
        new_scalar(NPY_INT, &ltpf->tc));

    return obj;
}

__attribute__((unused))
static PyObject *to_ltpf_synthesis(
    PyObject *obj, struct lc3_ltpf_synthesis *ltpf)
{
    PyObject *c_obj, *x_obj;

    CTYPES_CHECK("ltpf", obj && PyDict_Check(obj));

    CTYPES_CHECK("ltpf.active", to_scalar(
        PyDict_GetItemString(obj, "active"), NPY_BOOL, &ltpf->active));

    CTYPES_CHECK("ltpf.pitch", to_scalar(
        PyDict_GetItemString(obj, "pitch"), NPY_INT, &ltpf->pitch));

    CTYPES_CHECK("ltpf.c", c_obj = to_1d_copy(
        PyDict_GetItemString(obj, "c"), NPY_FLOAT, ltpf->c, 2*12));
    PyDict_SetItemString(obj, "c", c_obj);

    CTYPES_CHECK("ltpf.x", x_obj = to_1d_copy(
        PyDict_GetItemString(obj, "x"), NPY_FLOAT, ltpf->x, 12));
    PyDict_SetItemString(obj, "x", x_obj);

    return obj;
}

__attribute__((unused))
static PyObject *from_ltpf_synthesis(
    PyObject *obj, const struct lc3_ltpf_synthesis *ltpf)
{
    if (!obj) obj = PyDict_New();

    PyDict_SetItemString(obj, "active",
        new_scalar(NPY_BOOL, &ltpf->active));

    PyDict_SetItemString(obj, "pitch",
        new_scalar(NPY_INT, &ltpf->pitch));

    PyDict_SetItemString(obj, "c",
        new_1d_copy(NPY_FLOAT, 2*12, &ltpf->c));

    PyDict_SetItemString(obj, "x",
        new_1d_copy(NPY_FLOAT, 12, &ltpf->x));

    return obj;
}

__attribute__((unused))
static PyObject *new_ltpf_data(const struct lc3_ltpf_data *data)
{
    PyObject *obj = PyDict_New();

    PyDict_SetItemString(obj, "active",
        new_scalar(NPY_BOOL, &data->active));

    PyDict_SetItemString(obj, "pitch_index",
        new_scalar(NPY_INT, &data->pitch_index));

    return obj;
}

__attribute__((unused))
static PyObject *to_ltpf_data(
     PyObject *obj, const struct lc3_ltpf_data *data)
{
    PyObject *item;

    CTYPES_CHECK("ltpf", obj && PyDict_Check(obj));

    if ((item = PyDict_GetItemString(obj, "active")))
        CTYPES_CHECK("ltpf.active",
            to_scalar(item, NPY_BOOL, &data->active));

    if ((item = PyDict_GetItemString(obj, "pitch_index")))
        CTYPES_CHECK("ltpf.pitch_index",
            to_scalar(item, NPY_INT, &data->pitch_index));

    return obj;
}

/* -------------------------------------------------------------------------- */

#include <sns.h>

__attribute__((unused))
static PyObject *new_sns_data(const struct lc3_sns_data *data)
{
    PyObject *obj = PyDict_New();

    PyDict_SetItemString(obj, "lfcb",
        new_scalar(NPY_INT, &data->lfcb));

    PyDict_SetItemString(obj, "hfcb",
        new_scalar(NPY_INT, &data->hfcb));

    PyDict_SetItemString(obj, "shape",
        new_scalar(NPY_INT, &data->shape));

    PyDict_SetItemString(obj, "gain",
        new_scalar(NPY_INT, &data->gain));

    PyDict_SetItemString(obj, "idx_a",
        new_scalar(NPY_INT, &data->idx_a));

    PyDict_SetItemString(obj, "ls_a",
        new_scalar(NPY_BOOL, &data->ls_a));

    PyDict_SetItemString(obj, "idx_b",
        new_scalar(NPY_INT, &data->idx_b));

    PyDict_SetItemString(obj, "ls_b",
        new_scalar(NPY_BOOL, &data->ls_b));

    return obj;
}

__attribute__((unused))
static PyObject *to_sns_data(PyObject *obj, struct lc3_sns_data *data)
{
    PyObject *item;

    CTYPES_CHECK("sns", obj && PyDict_Check(obj));

    if ((item = PyDict_GetItemString(obj, "lfcb")))
        CTYPES_CHECK("sns.lfcb", to_scalar(item, NPY_INT, &data->lfcb));

    if ((item = PyDict_GetItemString(obj, "hfcb")))
        CTYPES_CHECK("sns.hfcb", to_scalar(item, NPY_INT, &data->hfcb));

    if ((item = PyDict_GetItemString(obj, "shape")))
        CTYPES_CHECK("sns.shape", to_scalar(item, NPY_INT, &data->shape));

    if ((item = PyDict_GetItemString(obj, "gain")))
        CTYPES_CHECK("sns.gain", to_scalar(item, NPY_INT, &data->gain));

    if ((item = PyDict_GetItemString(obj, "idx_a")))
        CTYPES_CHECK("sns.idx_a", to_scalar(item, NPY_INT, &data->idx_a));

    if ((item = PyDict_GetItemString(obj, "ls_a")))
        CTYPES_CHECK("sns.ls_a", to_scalar(item, NPY_INT, &data->ls_a));

    if ((item = PyDict_GetItemString(obj, "idx_b")))
        CTYPES_CHECK("sns.idx_b", to_scalar(item, NPY_INT, &data->idx_b));

    if ((item = PyDict_GetItemString(obj, "ls_b")))
        CTYPES_CHECK("sns.ls_b", to_scalar(item, NPY_INT, &data->ls_b));

    return obj;
}

/* -------------------------------------------------------------------------- */

#include <tns.h>

__attribute__((unused))
static PyObject *new_tns_data(const struct lc3_tns_data *side)
{
    PyObject *obj = PyDict_New();

    PyDict_SetItemString(obj, "nfilters",
        new_scalar(NPY_INT, &side->nfilters));

    PyDict_SetItemString(obj, "lpc_weighting",
        new_scalar(NPY_BOOL, &side->lpc_weighting));

    PyDict_SetItemString(obj, "rc_order",
        new_1d_copy(NPY_INT, 2, side->rc_order));

    PyDict_SetItemString(obj, "rc",
        new_2d_copy(NPY_INT, 2, 8, side->rc));

    return obj;
}

__attribute__((unused))
static PyObject *to_tns_data(PyObject *obj, struct lc3_tns_data *side)
{
    PyObject *item;

    CTYPES_CHECK("tns", obj && PyDict_Check(obj));

    if ((item = PyDict_GetItemString(obj, "nfilters")))
        CTYPES_CHECK("tns.nfilters",
            to_scalar(item, NPY_INT, &side->nfilters));

    if ((item = PyDict_GetItemString(obj, "lpc_weighting"))) {
        CTYPES_CHECK("tns.lpc_weighting",
            to_scalar(item, NPY_BOOL, &side->lpc_weighting));
    }

    if ((item = PyDict_GetItemString(obj, "rc_order"))) {
        CTYPES_CHECK("tns.rc_order",
            item = to_1d_copy(item, NPY_INT, side->rc_order, 2));
        PyDict_SetItemString(obj, "rc_order", item);
    }

    if ((item = PyDict_GetItemString(obj, "rc"))) {
        CTYPES_CHECK("tns.rc",
            item = to_2d_copy(item, NPY_INT, side->rc, 2, 8));
        PyDict_SetItemString(obj, "rc", item);
    }

    return obj;
}

/* -------------------------------------------------------------------------- */

#include <spec.h>

__attribute__((unused))
static PyObject *from_spec_analysis(
    PyObject *obj, const struct lc3_spec_analysis *spec)
{
    if (!obj) obj = PyDict_New();

    PyDict_SetItemString(obj, "nbits_off",
        new_scalar(NPY_FLOAT, &spec->nbits_off));

    PyDict_SetItemString(obj, "nbits_spare",
        new_scalar(NPY_INT, &spec->nbits_spare));

    return obj;
}

__attribute__((unused))
static PyObject *to_spec_analysis(
    PyObject *obj, struct lc3_spec_analysis *spec)
{
    CTYPES_CHECK("spec", obj && PyDict_Check(obj));

    CTYPES_CHECK("spec.nbits_off",
        to_scalar(PyDict_GetItemString(obj, "nbits_off"),
            NPY_FLOAT, &spec->nbits_off));

    CTYPES_CHECK("spec.nbits_spare",
        to_scalar(PyDict_GetItemString(obj, "nbits_spare"),
            NPY_INT, &spec->nbits_spare));

    return obj;
}

__attribute__((unused))
static PyObject *new_spec_side(const struct lc3_spec_side *side)
{
    PyObject *obj = PyDict_New();

    PyDict_SetItemString(obj, "g_idx",
        new_scalar(NPY_INT, &side->g_idx));

    PyDict_SetItemString(obj, "nq",
        new_scalar(NPY_INT, &side->nq));

    PyDict_SetItemString(obj, "lsb_mode",
        new_scalar(NPY_BOOL, &side->lsb_mode));

    return obj;
}

__attribute__((unused))
static PyObject *to_spec_data(
    PyObject *obj, struct lc3_spec_side *side)
{
    PyObject *item;

    CTYPES_CHECK("side", obj && PyDict_Check(obj));

    if ((item = PyDict_GetItemString(obj, "g_idx")))
        CTYPES_CHECK("side.g_idx",
            to_scalar(item, NPY_INT, &side->g_idx));

    if ((item = PyDict_GetItemString(obj, "nq")))
        CTYPES_CHECK("side.nq",
            to_scalar(item, NPY_INT, &side->nq));

    if ((item = PyDict_GetItemString(obj, "lsb_mode")))
        CTYPES_CHECK("side.lsb_mode",
            to_scalar(item, NPY_BOOL, &side->lsb_mode));

    return obj;
}

/* -------------------------------------------------------------------------- */

#ifdef __CTYPES_LC3_C

__attribute__((unused))
static PyObject *new_side_data(const struct side_data *side)
{
    PyObject *obj = PyDict_New();

    PyDict_SetItemString(obj, "bw",
        new_scalar(NPY_INT, &(int){ side->bw }));

    PyDict_SetItemString(obj, "ltpf",
        new_ltpf_data(&side->ltpf));

    PyDict_SetItemString(obj, "sns",
        new_sns_data(&side->sns));

    PyDict_SetItemString(obj, "tns",
        new_tns_data(&side->tns));

    return obj;
}

__attribute__((unused))
static PyObject *to_side_data(PyObject *obj, struct side_data *side)
{
    PyObject *item;

    CTYPES_CHECK("frame", obj && PyDict_Check(obj));

    if ((item = PyDict_GetItemString(obj, "bw"))) {
        int bw;
        CTYPES_CHECK("frame.bw", to_scalar(item, NPY_INT, &bw));
        side->bw = bw;
    }

    if ((item = PyDict_GetItemString(obj, "ltpf")))
        to_ltpf_data(item, &side->ltpf);

    if ((item = PyDict_GetItemString(obj, "sns")))
        to_sns_data(item, &side->sns);

    if ((item = PyDict_GetItemString(obj, "tns")))
        to_tns_data(item, &side->tns);

    return obj;
}

__attribute__((unused))
static PyObject *new_plc_state(const struct lc3_plc_state *plc)
{
    PyObject *obj = PyDict_New();

    PyDict_SetItemString(obj, "seed",
        new_scalar(NPY_UINT16, &plc->seed));

    PyDict_SetItemString(obj, "count",
        new_scalar(NPY_INT, &plc->count));

    PyDict_SetItemString(obj, "alpha",
        new_scalar(NPY_FLOAT, &plc->alpha));

    return obj;
}

__attribute__((unused))
static PyObject *to_plc_state(
    PyObject *obj, struct lc3_plc_state *plc)
{
    CTYPES_CHECK("plc", obj && PyDict_Check(obj));

    CTYPES_CHECK("plc.seed", to_scalar(
        PyDict_GetItemString(obj, "seed"), NPY_UINT16, &plc->seed));

    CTYPES_CHECK("plc.count", to_scalar(
        PyDict_GetItemString(obj, "count"), NPY_INT, &plc->count));

    CTYPES_CHECK("plc.alpha", to_scalar(
        PyDict_GetItemString(obj, "alpha"), NPY_FLOAT, &plc->alpha));

    return obj;
}

__attribute__((unused))
static PyObject *from_encoder(PyObject *obj, const struct lc3_encoder *enc)
{
    unsigned dt = enc->dt, sr = enc->sr;
    unsigned sr_pcm = enc->sr_pcm;
    int ns = LC3_NS(dt, sr);
    int nd = LC3_ND(dt, sr);
    int nt = LC3_NT(sr);

    if (!obj) obj = PyDict_New();

    PyDict_SetItemString(obj, "dt",
        new_scalar(NPY_INT, &dt));

    PyDict_SetItemString(obj, "sr",
        new_scalar(NPY_INT, &sr));

    PyDict_SetItemString(obj, "sr_pcm",
        new_scalar(NPY_INT, &sr_pcm));

    PyDict_SetItemString(obj, "attdet",
        from_attdet_analysis(NULL, &enc->attdet));

    PyDict_SetItemString(obj, "ltpf",
        from_ltpf_analysis(NULL, &enc->ltpf));

    PyDict_SetItemString(obj, "quant",
        from_spec_analysis(NULL, &enc->spec));

    PyDict_SetItemString(obj, "xt",
        new_1d_copy(NPY_INT16, nt+ns, enc->xt-nt));

    PyDict_SetItemString(obj, "xs",
        new_1d_copy(NPY_FLOAT, ns, enc->xs));

    PyDict_SetItemString(obj, "xd",
        new_1d_copy(NPY_FLOAT, nd, enc->xd));

    return obj;
}

__attribute__((unused))
static PyObject *to_encoder(PyObject *obj, struct lc3_encoder *enc)
{
    unsigned dt, sr, sr_pcm;
    PyObject *xt_obj, *xs_obj, *xd_obj;

    CTYPES_CHECK("encoder", obj && PyDict_Check(obj));

    CTYPES_CHECK("encoder.dt", to_scalar(
        PyDict_GetItemString(obj, "dt"), NPY_INT, &dt));
    CTYPES_CHECK("encoder.dt", (unsigned)(enc->dt = dt) < LC3_NUM_DT);

    CTYPES_CHECK("encoder.sr", to_scalar(
        PyDict_GetItemString(obj, "sr"), NPY_INT, &sr));
    CTYPES_CHECK("encoder.sr", (unsigned)(enc->sr = sr) < LC3_NUM_SRATE);

    CTYPES_CHECK("encoder.sr_pcm", to_scalar(
        PyDict_GetItemString(obj, "sr_pcm"), NPY_INT, &sr_pcm));
    CTYPES_CHECK("encoder.s_pcmr",
        (unsigned)(enc->sr_pcm = sr_pcm) < LC3_NUM_SRATE);

    int ns = LC3_NS(dt, sr);
    int nd = LC3_ND(dt, sr);
    int nt = LC3_NT(sr);

    CTYPES_CHECK(NULL, to_attdet_analysis(
        PyDict_GetItemString(obj, "attdet"), &enc->attdet));

    CTYPES_CHECK(NULL, to_ltpf_analysis(
        PyDict_GetItemString(obj, "ltpf"), &enc->ltpf));

    CTYPES_CHECK(NULL, to_spec_analysis(
        PyDict_GetItemString(obj, "quant"), &enc->spec));

    CTYPES_CHECK("encoder.xt", xt_obj = to_1d_copy(
        PyDict_GetItemString(obj, "xt"), NPY_INT16, enc->xt-nt, ns+nt));
    PyDict_SetItemString(obj, "xt", xt_obj);

    CTYPES_CHECK("encoder.xs", xs_obj = to_1d_copy(
        PyDict_GetItemString(obj, "xs"), NPY_FLOAT, enc->xs, ns));
    PyDict_SetItemString(obj, "xs", xs_obj);

    CTYPES_CHECK("encoder.xd", xd_obj = to_1d_copy(
        PyDict_GetItemString(obj, "xd"), NPY_FLOAT, enc->xd, nd));
    PyDict_SetItemString(obj, "xd", xd_obj);

    return obj;
}

__attribute__((unused))
static PyObject *from_decoder(PyObject *obj, const struct lc3_decoder *dec)
{
    unsigned dt = dec->dt, sr = dec->sr;
    unsigned sr_pcm = dec->sr_pcm;
    unsigned xs_pos = dec->xs - dec->xh;
    int nh = LC3_NH(dt, sr);
    int ns = LC3_NS(dt, sr);
    int nd = LC3_ND(dt, sr);

    if (!obj) obj = PyDict_New();

    PyDict_SetItemString(obj, "dt",
        new_scalar(NPY_INT, &dt));

    PyDict_SetItemString(obj, "sr",
        new_scalar(NPY_INT, &sr));

    PyDict_SetItemString(obj, "sr_pcm",
        new_scalar(NPY_INT, &sr_pcm));

    PyDict_SetItemString(obj, "ltpf",
        from_ltpf_synthesis(NULL, &dec->ltpf));

    PyDict_SetItemString(obj, "plc",
        new_plc_state(&dec->plc));

    PyDict_SetItemString(obj, "xh",
        new_1d_copy(NPY_FLOAT, nh, dec->xh));

    PyDict_SetItemString(obj, "xs_pos",
        new_scalar(NPY_INT, &xs_pos));

    PyDict_SetItemString(obj, "xd",
        new_1d_copy(NPY_FLOAT, nd, dec->xd));

    PyDict_SetItemString(obj, "xg",
        new_1d_copy(NPY_FLOAT, ns, dec->xg));

    return obj;
}

__attribute__((unused))
static PyObject *to_decoder(PyObject *obj, struct lc3_decoder *dec)
{
    unsigned dt, sr, sr_pcm, xs_pos;
    PyObject *xh_obj, *xd_obj, *xg_obj;

    CTYPES_CHECK("decoder", obj && PyDict_Check(obj));

    CTYPES_CHECK("decoder.dt", to_scalar(
        PyDict_GetItemString(obj, "dt"), NPY_INT, &dt));
    CTYPES_CHECK("decoder.dt", (unsigned)(dec->dt = dt) < LC3_NUM_DT);

    CTYPES_CHECK("decoder.sr", to_scalar(
        PyDict_GetItemString(obj, "sr"), NPY_INT, &sr));
    CTYPES_CHECK("decoder.sr", (unsigned)(dec->sr = sr) < LC3_NUM_SRATE);

    CTYPES_CHECK("decoder.sr_pcm", to_scalar(
        PyDict_GetItemString(obj, "sr_pcm"), NPY_INT, &sr_pcm));
    CTYPES_CHECK("decoder.sr_pcm",
        (unsigned)(dec->sr_pcm = sr_pcm) < LC3_NUM_SRATE);

    int nh = LC3_NH(dt, sr);
    int ns = LC3_NS(dt, sr);
    int nd = LC3_ND(dt, sr);

    CTYPES_CHECK(NULL, to_ltpf_synthesis(
        PyDict_GetItemString(obj, "ltpf"), &dec->ltpf));

    CTYPES_CHECK(NULL, to_plc_state(
        PyDict_GetItemString(obj, "plc"), &dec->plc));

    CTYPES_CHECK("decoder.xh", xh_obj = to_1d_copy(
        PyDict_GetItemString(obj, "xh"), NPY_FLOAT, dec->xh, nh));
    PyDict_SetItemString(obj, "xh", xh_obj);

    CTYPES_CHECK("decoder.xs", to_scalar(
        PyDict_GetItemString(obj, "xs_pos"), NPY_INT, &xs_pos));
    dec->xs = dec->xh + xs_pos;

    CTYPES_CHECK("decoder.xd", xd_obj = to_1d_copy(
        PyDict_GetItemString(obj, "xd"), NPY_FLOAT, dec->xd, nd));
    PyDict_SetItemString(obj, "xd", xd_obj);

    CTYPES_CHECK("decoder.xg", xg_obj = to_1d_copy(
        PyDict_GetItemString(obj, "xg"), NPY_FLOAT, dec->xg, ns));
    PyDict_SetItemString(obj, "xg", xg_obj);

    return obj;
}


/* -------------------------------------------------------------------------- */

#endif /* __CTYPES_LC3_C */

#endif /* __CTYPES */
