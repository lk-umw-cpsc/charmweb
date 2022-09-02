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

union cpsr_bits {
    int32_t n : 1;
    int32_t z : 1;
    int32_t c : 1;
    int32_t v : 1;
    int32_t u : 1;
    int32_t unused : 25;
    int32_t os : 1;
    int32_t unused2 : 1;
};

#define NUM_FLAGS 6

char *flag_names[] = {
    "os",
    "u",
    "v",
    "c",
    "z",
    "n"
};

#define gen_mask(a) (1 << a)

uint32_t flag_masks[] = {
    gen_mask(1),
    gen_mask(27),
    gen_mask(28),
    gen_mask(29),
    gen_mask(30),
    gen_mask(31)
};

static PyObject *grab_flag_updates() {
    PyObject *flags = PyList_New(0);
    for (int i = 0; i < NUM_FLAGS; i++) {
        uint32_t mask = flag_masks[i];
        uint32_t bit = ((uint32_t) cpsr) & mask;
        if (bit != (((uint32_t) registers_last_step[16]) & mask)) {
            uint32_t value = bit != 0;
            PyObject *reg_update = Py_BuildValue("{sssi}",
                    "flag", flag_names[i],
                    "value", value); 
            PyList_Append(flags, reg_update);
        }
    }
    return flags;
}

union registerif {
    int32_t i;
    float f;
    union cpsr_bits bits;
};

union cpsr_bits flags;

static PyObject *grab_output(char *command_executed) {
    char buff[256];
    // format is "% <command>" - %% escapes the %
    snprintf(buff, 256, "%% %s", command_executed);
    PyObject *output_strings = PyList_New(0);
    if (command_executed != NULL) {
        PyList_Append(output_strings, Py_BuildValue("s", buff));
    }
    for (int i = 0; i < resi; i++) {
        PyList_Append(output_strings, Py_BuildValue("s", resvals[i]));
    }
    resi = 0;
    return output_strings;
}

static PyObject *grab_updated_registers() {
    PyObject* register_updates_list = PyList_New(0);

    char hex_value[11];
    for (int i = 0; i < 17; i++) {
        union registerif reg;
        reg.i = registers[i];
        if (reg.i != registers_last_step[i]) {
            snprintf(hex_value, 11, "0x%08X", reg);
            PyObject *reg_update = Py_BuildValue("{sisssisf}", 
                    "register", i, 
                    "value", hex_value,
                    "int", reg.i,
                    "float", reg.f);
            PyList_Append(register_updates_list, reg_update);
            registers_last_step[i] = reg.i;
        }
    }
    return register_updates_list;
}

static PyObject *grab_instructions() {
    PyObject *instruction_list = PyList_New(0);
    for (int i = 0; i < 11; i++) {
        PyList_Append(instruction_list, Py_BuildValue("{ss}", "instruction", instinfo[i]));
    }
    // ensures the emulator outputs to the correct index on next go
    insti = 0;
    return instruction_list;
}

/**
 * @brief The C implementation of the chemu.init() method
 * 
 * @param self The calling Python object
 * @param args The arguments passed via the Python call to the function
 * @return None
 */
static PyObject *method_init(PyObject *self, PyObject *args) {
    char *input_file, *os_file;
    if (!PyArg_ParseTuple(args, "ss", &input_file, &os_file)) {
        return NULL;
    }
    for (int i = 0; i < 17; i++) {
        registers_last_step[i] = 0;
    }

    if (os_file != NULL && strlen(os_file) > 0) {
        load_memory(os_file);
        set_cpsr(OS);
        set_cpsr(U);
        set_rupt(0x7ff0);
    }

    load_memory(input_file);
    char *start = "pl";
    do_cmd(1, &start);

    // retrieve output
    PyObject *output = grab_output(NULL);
    PyObject *flag_updates = grab_flag_updates();
    PyObject *updated_registers = grab_updated_registers();
    PyObject *instructions = grab_instructions();

    return Py_BuildValue("{sOsOsOsO}", 
            "registers", updated_registers, 
            "instructions", instructions,
            "output", output,
            "flags", flag_updates);
}

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

    char command_copy[256];
    strncpy(command_copy, command, 256);

    char *command_args[10];
    char *s = command;
    char *es = s + strlen(s);
    char *str;
    int argc = 0;
    cmdargv[0] = ""; // prevent seg fault when first cmd is empty line
    while (argc < 9 && cmdgetstr(&s, es, &str) != 0) {
        cmdargv[argc] = str;
        argc++;
    }
    cmdargv[argc] = NULL;
    do_cmd(argc, &cmdargv);

    PyObject *instruction_list = grab_instructions();

    PyObject *output_strings = grab_output(command_copy);

    PyObject *flag_updates = grab_flag_updates();

    PyObject *updated_registers = grab_updated_registers();

    PyObject *returnValue = Py_BuildValue("{sOsOsOsO}", 
            "registers", updated_registers, 
            "instructions", instruction_list,
            "output", output_strings,
            "flags", flag_updates);
    return returnValue;
}

// Null-terminated array of methods available in the module
static PyMethodDef chemuMethods[] = {
    {"init", method_init, METH_VARARGS, "Initializes Chemu"},
    {"do", method_do, METH_VARARGS, "Runs a command within Chemu"},
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
    
    // return the filled-in module struct
    return PyModule_Create(&chemuModule);
}