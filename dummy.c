#define PY_SSIZE_T_CLEAN
#include <Python.h>

// /Library/Developer/CommandLineTools/Library/Frameworks/Python3.framework/Versions/3.8/Headers
#define CHEMU_INSTRUCTION_DISPLAY_COUNT 17
static int array[CHEMU_INSTRUCTION_DISPLAY_COUNT];

static PyObject *method_do(PyObject *self, PyObject *args) {
    char *command;
    if (!PyArg_ParseTuple(args, "s", &command)) {
        return NULL;
    }
    // we now know what the user sent over...

    // printf("%s", command);
    PyObject *instructions = PyList_New(0);
    for (int i = 0; i < CHEMU_INSTRUCTION_DISPLAY_COUNT; i++) {
        PyList_Append(instructions, Py_BuildValue("{ssss}", "instruction", "testing", "address", "0x12345678"));
    }

    PyObject *registers = PyList_New(0);

    PyObject *reg16 = Py_BuildValue("{siss}", "register", 0, "value", "0xfeedabee");

    PyList_Append(registers, reg16);

    PyObject *returnValue = Py_BuildValue("{sOsO}", "registers", registers, "instructions", instructions);
    return returnValue;
}

static PyMethodDef chemuMethods[] = {
    {"do", method_do, METH_VARARGS, "Python interface for Chemu"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef chemuModule = {
    PyModuleDef_HEAD_INIT,
    "chemu",
    "Python interface for Chemu",
    -1,
    chemuMethods
};

PyMODINIT_FUNC PyInit_chemu(void) {
    for (int i = 0; i < CHEMU_INSTRUCTION_DISPLAY_COUNT; i++) {
        array[i] = i;
    }
    return PyModule_Create(&chemuModule);
}