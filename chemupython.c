#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
// #include <ncurses.h>
#include <Python.h>
#include "memory.h"
#include "cpu.h"

// bg_val used to create white letters/black backgroud or black letters/white background
static int bg_val = 0; // 0 - black, 1 - white, -1 - error

#define MINLINES 41
#define MINCOLS 136

#define REGWID 54
#define REGHGT 14
#define REGLIN 1
#define REGCOL 1

#define INSWID 70
#define INSHGT 14
#define INSLIN 1
#define INSCOL 60

#define CMDWID 54
#define CMDHGT 23
#define CMDLIN 16
#define CMDCOL 1

#define RESWID 70
#define RESHGT 23
#define RESLIN 16
#define RESCOL 60

// Externs from cpu.c used in the register window
// These could be retrieved via getters, get_reg()
extern int registers[16];
extern int cpsr;
extern char mem_changed[80];

/***** Register Window *****/
static const char *regwinl2 = "r0 :%08x r1 :%08x r2 :%08x r3 :%08x";
static const char *regwinl3 = "r4 :%08x r5 :%08x r6 :%08x r7 :%08x";
static const char *regwinl4 = "r8 :%08x r9 :%08x r10:%08x r11:%08x";
static const char *regwinl5 = "r12:%08x r13:%08x r14:%08x r15:%08x";
static const char *regwinl6 = "cpsr:%08x Z: %d, N: %d, C: %d, V: %d, U: %d, OS: %d";
static const char *regwinl8 = "%s";

void printregwin() {
    printf(regwinl2, registers[0],  registers[1],  registers[2],  registers[3]);
    printf("\n");
    printf(regwinl3, registers[4],  registers[5],  registers[6],  registers[7]);
    printf("\n");
    printf(regwinl4, registers[8],  registers[9],  registers[10], registers[11]);
    printf("\n");
    printf(regwinl5, registers[12], registers[13], registers[14], registers[15]);
    printf("\n");
    printf(regwinl6, cpsr, cpsr>>N&1, cpsr>>Z&1, cpsr>>C&1, cpsr>>V&1, cpsr>>U&1, cpsr>>OS&1);
    printf("\n");
    printf(regwinl8, mem_changed);
    printf("\n");
}

/***** Instruction Window *****/
char instinfo[25][80] = {
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    };
int insti = 0;
void addinst(char *inst) {
    strcpy(instinfo[insti], "                                                                ");
    strcpy(instinfo[insti], inst);
    insti++;
}
static const char *inswinln  = "%s";

void printinswin() {
    for (int i = 0; i < 11; i++) {
        printf(inswinln, instinfo[i]);
        printf("\n");
    }
    insti = 0;
}

char cmdhist[25][80] = {
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    {"                                      "},
    };

// chemut uses the terminal as its command window

#define RESVALSLINES 50
char resvals[RESVALSLINES][80] = {
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    {"                                      "}, {"                                      "},
    };
int resi = 0; // count of lines added since last update
int resn = 0; // where to place line in resvals[], circular buffer
void addresult(char *res) {
    strcpy(resvals[resi], "                                                                ");
    strcpy(resvals[resi], res);
    resi++;
}

// printres has variable arguments just like fprintf
__attribute__((format(printf, 1, 2)))
void printres(char *fmt, ...) {
    char res[80];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(res, fmt, ap);
    va_end(ap);
    addresult(res);
}

static const char *reswinln = "%s";

void printreswin() {
    for (int i = 0; i < resi; i++) {
        printf(reswinln, resvals[i]);
        printf("\n");
    }
    resi = 0;
}

extern int doscanf;

int cmdgetstr(char **ps, char *es, char **str);
#define MAXARGS 10
static char *cmdargv[MAXARGS];
int do_cmd(int argc, char **cmdargv);
void do_script(char *scriptfilename);
int process_args(int argc, char **argv);
int getcmd(char *buf, int nbuf);

int registers_last_step[17];

/**
 * @brief The C implementation of the chemu.do(command) method
 * 
 * @param self The calling Python object
 * @param args The arguments passed via the Python call to the function
 * @return PyObject* a dict containing changed registers and the current 17 visible instructions
 */
static PyObject *method_do(PyObject *self, PyObject *args) {
    char *command;
    if (!PyArg_ParseTuple(args, "s", &command)) {
        return NULL;
    }

    // we now know what the user sent over...
    char command_copy[256];
    strncpy(command_copy, command, 256);
    do_cmd(1, &command);

    PyObject *instruction_list = PyList_New(0);
    for (int i = 0; i < 11; i++) {
        PyList_Append(instruction_list, Py_BuildValue("{ssss}", "instruction", instinfo[i], "address", "0xfeedabee"));
    }
    // ensures the emulator outputs to the correct index on next go
    insti = 0;

    PyObject *output_strings = PyList_New(0);
    for (int i = 0; i < resi; i++) {
        PyList_Append(output_strings, Py_BuildValue("s", resvals[i]));
    }
    resi = 0;

    PyObject* register_updates_list = PyList_New(0);

    char hex_value[11];
    for (int i = 0; i < 17; i++) {
        int reg = registers[i];
        if (reg != registers_last_step[i]) {
            snprintf(hex_value, 11, "0x%08X", reg);
            PyObject *reg_update = Py_BuildValue("{siss}", "register", i, "value", hex_value);
            PyList_Append(register_updates_list, reg_update);
            registers_last_step[i] = registers[i];
        }
    }

    PyObject *returnValue = Py_BuildValue("{sOsOsO}", "registers", register_updates_list, "instructions", instruction_list,
            "output", output_strings);
    return returnValue;
}

// Null-terminated array of methods available in the module
static PyMethodDef chemuMethods[] = {
    {"do", method_do, METH_VARARGS, "Python interface for Chemu"},
    {NULL, NULL, 0, NULL}
};

// Module struct pointing to chemuMethods
static struct PyModuleDef chemuModule = {
    PyModuleDef_HEAD_INIT,
    "chemu",
    "Python interface for Chemu",
    -1,
    chemuMethods
};

/**
 * @brief Function called by Python to initialize the module
 * 
 * @return PyMODINIT_FUNC An object containing the module's methods
 */
PyMODINIT_FUNC PyInit_chemu(void) {

    for (int i = 0; i < 17; i++) {
        registers_last_step[i] = 0;
    }

    // load object file... to-do: move this to an init function
    load_memory("figisa17.o");
    
    // return the filled-in module struct
    return PyModule_Create(&chemuModule);
}