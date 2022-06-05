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

static struct PyModuleDef module_def = {
    PyModuleDef_HEAD_INIT,
    .m_name = "LC3",
    .m_doc = "LC3 Test Python Module",
    .m_size = -1,
};

PyMODINIT_FUNC lc3_mdct_py_init(PyObject *);
PyMODINIT_FUNC lc3_energy_py_init(PyObject *);
PyMODINIT_FUNC lc3_attdet_py_init(PyObject *);
PyMODINIT_FUNC lc3_bwdet_py_init(PyObject *);
PyMODINIT_FUNC lc3_ltpf_py_init(PyObject *);
PyMODINIT_FUNC lc3_sns_py_init(PyObject *);
PyMODINIT_FUNC lc3_tns_py_init(PyObject *);
PyMODINIT_FUNC lc3_spec_py_init(PyObject *);
PyMODINIT_FUNC lc3_interface_py_init(PyObject *);

PyMODINIT_FUNC PyInit_lc3(void)
{
    PyObject *m = PyModule_Create(&module_def);

    if (m) m = lc3_mdct_py_init(m);
    if (m) m = lc3_energy_py_init(m);
    if (m) m = lc3_attdet_py_init(m);
    if (m) m = lc3_bwdet_py_init(m);
    if (m) m = lc3_ltpf_py_init(m);
    if (m) m = lc3_sns_py_init(m);
    if (m) m = lc3_tns_py_init(m);
    if (m) m = lc3_spec_py_init(m);
    if (m) m = lc3_interface_py_init(m);

    return m;
}
