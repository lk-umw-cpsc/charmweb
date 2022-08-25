#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
// #include <ncurses.h>
#include <Python.h>
#include "memory.h"
#include "cpu.h"

#include <stdarg.h>
#include <stdbool.h>

#include "bit_functions.h"
#include "isa.h"
#include "charmopcodes.h"
#include "dict.h"

unsigned char memory[MAX_MEM]; // 32K bytes

void memory_store(int address, unsigned char value) {
    memory[address] = value;
}

void memory_store_word(int address, unsigned int value) {
    memory[address    ] = value >> 24 & 0xff;
    memory[address + 1] = value >> 16 & 0xff;
    memory[address + 2] = value >> 8  & 0xff;
    memory[address + 3] = value       & 0xff;
}

unsigned char memory_fetch(int address) {
    return memory[address];
}

unsigned int memory_fetch_word(int address) {
    return memory[address]     << 24 |
           memory[address + 1] << 16 |
           memory[address + 2] << 8  |
           memory[address + 3];
}

// read/write 32-bit value from/to memory - return values
// 0 - success
// 1 - memory address bad
int system_bus(int address, int *value, int control) {
    if (address < 0 || address >= MAX_MEM)
        return 1;
    if (control == READ)
        *value = memory_fetch_word(address);
    else
        memory_store_word(address, *value);
    return 0;
}

// read/write byte (8-bit) value from/to memory - return values
// 0 - success
// 1 - memory address bad
int system_bus_b(int address, unsigned char *value, int control) {
    if (address < 0 || address >= MAX_MEM)
        return 1;
    if (control == READ)
        *value = memory_fetch(address);
    else
        memory_store(address, *value);
    return 0;
}

void printres(char *fmt, ...) __attribute__((format(printf, 1, 2)));

// dump memory bytes (8-bit quantities) - return values
// 0 - success
// 1 - memory address bad
int dump_memory(int start_address, int num_bytes) {
    int start_boundary = start_address - (start_address % 8);
    int boundary_bytes = num_bytes + (start_address - start_boundary);
    if (start_boundary < 0 || boundary_bytes >= MAX_MEM)
        return 1;
    for (int i = start_boundary; i < start_boundary+boundary_bytes; i+=8) {
        printres("0x%04x (0d%04d) 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x ", 
               i, i,
               memory[i  ], memory[i+1], memory[i+2], memory[i+3],
               memory[i+4], memory[i+5], memory[i+6], memory[i+7]);
    }
    return 0;
}

// dumb memory words (32-bit quantities) - return values
// 0 - success
// 1 - memory address bad
int dump_memory_word(int start_address, int num_words) {
    if (start_address < 0 || (start_address+(num_words*4) >= MAX_MEM))
        return 1;
    for (int i = start_address; i < start_address+(num_words*4); i+=16) {
        printres("0x%04x (0d%04d) 0x%08x 0x%08x 0x%08x 0x%08x  ", 
               i, i,
               memory[i   ] << 24 | memory[i+1 ] << 16 | memory[i+2 ] << 8 | memory[i+3 ],
               memory[i+4 ] << 24 | memory[i+5 ] << 16 | memory[i+6 ] << 8 | memory[i+7 ],
               memory[i+8 ] << 24 | memory[i+9 ] << 16 | memory[i+10] << 8 | memory[i+11],
               memory[i+12] << 24 | memory[i+13] << 16 | memory[i+14] << 8 | memory[i+15]);
    }
    return 0;
}

#define MAX_LINE 100
static char buf[MAX_LINE];
/*
 File Format - Each line is one of the following
.data - continue address from previous
.data number - number is address
.text - continue address from previous
.text number - number is address
.stak number - number is address put into r13 (sp)
number - number is 32-bit value to be placed in memory
The first .text encountered set the pc to the address
 */
void load_memory(char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        printres("File %s not found!", filename);
        return;
    }
    int address = 0;
    int symbols = 0;
    while (fgets(buf, MAX_LINE, fp)) {
        int pos = 0, base = 10, mem_value = 0, directive = 0;
        buf[strcspn(buf, "\n")] = '\0';
        //printf("%s\n", buf);
        if (buf[0] == '.') { // directive 
            directive = 1;
            if (strlen(buf) > 5) { // directive has a number
                int numpos = 6;    // position of number on directive line
                if (buf[1] == 's') // .stack is 6 chars
                    numpos = 7;    // position of number on .stack directive
                if (buf[numpos] == '0' && buf[numpos+1] =='x') {
                    base = 16;
                    pos = numpos + 2;
                }
                else 
                    pos = numpos;
            }
            else
                directive = 2; // directive without a number
        }
        else if (symbols) { // processing symbols
            char symbol[40];
            int value, symsection; // symsection used by chlinker
            sscanf(buf, "%s %x %d", symbol, &value, &symsection);
            dictput(symbol, value);
            // printf("%s %d\n", symbol, value);
        }
        else { // number
            if (buf[0] == '0' && buf[1] =='x') {
                base = 16;
                pos = 2;
            }
        }
        if (directive) {
            if (directive == 1) { // directive with number
                int taddress = (int)strtol(buf+pos, NULL, base);
                if (buf[1] == 's') // .stack directive
                    set_reg(13, taddress);
                else // either .text or .data directive
                    address = taddress;
            }
            if (buf[1] == 't') // .text
                set_reg(15, address);
            else if (buf[1] == 'y') // .ymbl for symbols
                symbols = 1;
        }
        else { // number
            mem_value = (int)strtoul(buf+pos, NULL, base);
            memory_store_word(address, mem_value);
            address += 4;
        }
    }
    fclose(fp);
    printres("File %s loaded!", filename);
}

void addresult(char *res);

/******************************************************************
 ***************************   STEPS   ****************************
 ** global total_steps - counts the num of instructions executed **
 ******************************************************************/
int total_steps = 0;

/******************************************************************
 ***************************  EXECUTE  ****************************
 ** global verbose_cpu - main() changes this via v command       **
 ******************************************************************/
int verbose_cpu = 0;

/******************************************************************
 ************************  INST HISTORY  **************************
 ** save the last INSTHIST instructions executed                 **
 ******************************************************************/
#define INSTHIST 5
struct insthist {
    int addr;
    int inst;
};
static int insthist_i = 0; // next available slot in insthist
static struct insthist insthist[INSTHIST] = { {0,0} };

/******************************************************************
 **************************  REGISTERS  ***************************
 ******************************************************************/
int registers[16]; // 16 32-bit registers
int cpsr;          // status register
int rupt;          // base address of interrupt vector table
                   // *(rupt+0) - address of user to kernel interrupt
                   // *(rupt+4) - address of I/O interrupt

/******************************************************************
 *************************  PIPELINE  *****************************
 ** display last 5 inst, current inst, and next 5 inst           **
 ******************************************************************/
void addinst(char *inst);
char instresult[80]; // used to add a result to ncurses result window

void pipeline() {
    int x = insthist_i;
    for (int i = 0; i < INSTHIST; i++) {
        if (verbose_cpu)
            sprintf(instresult, "0x%08x: 0x%08x: %s", insthist[x].addr, insthist[x].inst, disassemble(insthist[x].inst));
        else
            sprintf(instresult, "0x%08x: %s", insthist[x].addr, disassemble(insthist[x].inst));
        addinst(instresult);
        x = (x + 1) % INSTHIST;
    }
    int address = registers[PC], inst, branch = 0, baddress;
    for (int i = 0; i < INSTHIST+1; i++) {
        system_bus(address, &inst, READ);
        int n;
        if (verbose_cpu)
            n = sprintf(instresult, "0x%08x: 0x%08x: %s", address, inst, disassemble(inst));
        else
            n = sprintf(instresult, "0x%08x: %s", address, disassemble(inst));
        if (branch) { // branch displays both paths of branch
            char branresult[128];
            system_bus(baddress, &inst, READ);
            strncat(instresult, "               ", 30 - n);
            if (verbose_cpu)
                sprintf(branresult, "%s  0x%08x: 0x%08x: %s", instresult, baddress, inst, disassemble(inst));
            else
                sprintf(branresult, "%s  0x%08x: %s", instresult, baddress, disassemble(inst));
            strncpy(instresult, branresult, 80);
            //addinst(instresult);
            baddress += 4;
        }
        if (i == 0) {
            strcat(instresult, "<- pc");
            //printf(" <- pc");
            int opcodeupper = (inst >> 28) & 0xf; // TODO - should use decode() for this
            if (opcodeupper >= B_ADDR && opcodeupper <= B_REL) {
                branch = 1;
                baddress = inst & 0xfffff; // TODO - get address from reg, etc.
            }
        }
        addinst(instresult);
        address += 4;
    }
}

/******************************************************************
 ***********************  SETTERS/GETTERS *************************
 ******************************************************************/
void set_reg(int reg, int value) {
    registers[reg] =  value;
    if (reg == 15)
        total_steps = 0;
}

int get_reg(int reg) {
    return registers[reg];
}

void set_cpsr(int bit) {
    bit_set(&cpsr, bit);
}

int get_cpsr() {
    return cpsr;
}

void set_rupt(int value) {
    rupt =  value;
}

int get_rupt() {
    return rupt;
}

/******************************************************************
 ***********************  CHEMU IOI *******************************
 ******************************************************************
 *
 * User Mode initiates a printf via ker 0x11 and scanf via ker 0x10
 * The 0x10 and 0x11 are passed to kernel in r0.
 * Prior to issuing the ker 0x10/0x11, a user places a fmt string in
 * r1. For each %fmt in the fmt string, the uses places a matching
 * parameter in r2 and r3.
 * The ker instruction traps to the OS. The OS address is placed in the
 * interrupt vector table, which is located at address 0x7ff0.
 * The OS issues a kernel mode ioi insruction to perform the scanf/printf.
 * The ioi operand is 0x10 for scanf and 0x11 for printf.
 * Register values when chemuioi occurs are the following.
 * r0 - contains 0x10 or 0x11
 * r1 - contains the format string
 * r2 - conaints value to match first %fmt
 * r3 - contains value to match second %fmt
 */
void byteswap(int *p) {
    char b0 = *p         & 0xff;
    char b1 = (*p >> 8)  & 0xff;
    char b2 = (*p >> 16) & 0xff;
    char b3 = (*p >> 24) & 0xff;
    *p = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}
int doscanf = 0, scanfdone = 0;
static int perc; // the number of % in fmt string
// chemu is big endian, laptops are little endian
// After scanf(%d), must byteswap. percd[] tracks %d
static int percd[2]; // a 1 indicates %d
static int percs[2]; // a 1 indicates %s
static int percf[2]; // a 1 indicates %f
// cheumscanf called from main() in chemu.c to apply scanf to str.
// char *str points the the user's input
void chemuscanf(char *str) {
    //char temp[80];
    //snprintf(temp, 80, "%s %d %d\n", str, perc, percd[0]);
    //addresult(temp);
    if (perc == 1) {
        sscanf(str, &memory[registers[1]], &memory[registers[2]]);
        if (percd[0]) // must byteswap
            byteswap((int *)&memory[registers[2]]);
    }
    else if (perc == 2) {
        sscanf(str, &memory[registers[1]], &memory[registers[2]], &memory[registers[3]]);
        if (percd[0]) // must byteswap
            byteswap((int *)&memory[registers[2]]);
        if (percd[1]) // must byteswap
            byteswap((int *)&memory[registers[3]]);
    }
    doscanf = 0;
    scanfdone = 1;

}

// TODO - currently printf("%f" ) works with one %f, update to allow %s %f, %d %f, %f %f
// The address in registers[1] is a local chemu address
// NOTE: size of format string must be < 80 (0 to 79 index)
void chemuioi(int op2) {
    percd[0] = 0;
    percd[1] = 0;
    percs[0] = 0;
    percs[1] = 0;
    percf[0] = 0;
    percf[1] = 0;
    if (op2 == 0x10 || op2 == 0x11) { // scanf or printf
        char *p = &memory[registers[1]];
        perc = 0; // count % chars in format string
        for (int i = 0; i < 80; i++) {
            if (*p == 0)
                break;
            if (*p++ == '%') {
                perc++;
                if (*p == 'd') // %d
                    percd[perc-1] = 1;
                else if (*p == 's')
                    percs[perc-1] = 1;
                else if (*p == 'f')
                    percf[perc-1] = 1;
            }
        }
        if (perc > 2)
            perc = 2; // max of two format chars
        char temp[80];
        if (op2 == 0x11) { // printf
            switch (perc) {
              case 0:
                strncpy(temp, &memory[registers[1]], 80);
                break;
              case 1:
                if (percd[0] == 1)
                    snprintf(temp, 80, &memory[registers[1]], registers[2]);
                else if (percf[0] == 1) {
                    union {
                        float f;
                        int i;
                    } u;
                    u.i = registers[2];
                    //u.i = 0x3fa00000; // 1.25 - original used for testing
                    snprintf(temp, 80, &memory[registers[1]], u.f);
                } else
                    snprintf(temp, 80, &memory[registers[1]], &memory[registers[2]]);
                break;
              case 2:
                if (percd[0] == 1 && percd[1] == 1) // two %d
                    snprintf(temp, 80, &memory[registers[1]], registers[2], registers[3]);
                else if (percd[0] == 1 && percs[1] == 1) // %d %s
                    snprintf(temp, 80, &memory[registers[1]], registers[2], &memory[registers[3]]);
                else if (percd[1] == 1 && percs[0] == 1) // %s %d
                    snprintf(temp, 80, &memory[registers[1]], &memory[registers[2]], registers[3]);
                else // %s %s
                    snprintf(temp, 80, &memory[registers[1]], &memory[registers[2]], &memory[registers[3]]);
                break;
            }
            addresult(temp);
        }
        else { // scanf
            doscanf = 1;
        }
   }
}

/******************************************************************
 ************************  BREAKPOINT *****************************
 ******************************************************************/
// TODO - add more breakpoints #define NUM_BREAKPOINTS 2
// static int breakpoints[NUM_BREAKPOINTS]; // allow 2 break points

// global breakpoint - main() changes this via b command
int breakpoint0 = -1, breakpoint1 = -1; // >0 is address of a breakpoint
static int breakpoint0executed = 0, breakpoint1executed = 0;
static int memaddr_changed = 0, memval_before = 0, memval_after = 0;

/******************************************************************
 ****************************  STEP  ******************************
 ** step() does fetch, decode, execute for one instruction       **
 ** return 0 normal                                              **
 ** return 1 when breakpoint is hit                              **
 ** return 2 when waiting on user input for scanf                **
 ** return 4 when memory access error                            **
 ** return -1 for illegal instruction                            **
 ******************************************************************/
enum stepret step() {
    total_steps++;
    memaddr_changed = 0; // state of changed memory
/******************************************************************
 ****************************  FETCH  *****************************
 ******************************************************************/
    int pc = registers[PC];
    if (pc == breakpoint0 && !breakpoint0executed) {
        breakpoint0executed = 1;
        return BREAKPOINT;
    }
    if (pc == breakpoint1 && !breakpoint1executed) {
        breakpoint1executed = 1;
        return BREAKPOINT;
    }
    breakpoint0executed = 0;
    breakpoint1executed = 0;
    int inst; 
    if (system_bus(pc, &inst, READ))
        return MEMERROR;
    insthist[insthist_i].addr = pc;
    insthist[insthist_i].inst = inst;
    insthist_i = (insthist_i + 1) % INSTHIST;
/******************************************************************
 ****************************  DECODE  ****************************
 ******************************************************************/
    decoded *d = decode(inst);
    if (!d) {
        if (verbose_cpu) {
            printf("Invalid instruction at address: %08x", registers[PC]);
            printf("pc: 0x%08x, inst: 0x%08x\n", registers[PC], inst);
        }
        registers[PC] = pc;
        return ILLEGALINST;

    }
    if (verbose_cpu)
        printf("pre-step  : cpsr: 0x%08x, pc: 0x%08x, inst: 0x%08x, %s\n", cpsr, registers[PC], inst, disassemble(inst));
    int address = 0, control;
    enum stepret retval = NORMAL; // normal return
    int opcode = d->opcode >> 4 & 0xf; // & 0xf needed due to arithmetic left shift
/******************************************************************
 ***************************  EXECUTE  ****************************
 ** Each case of the switch statements perform execute           **
 ******************************************************************/
    switch (opcode) {
      case LDR: case LDB: case STR: case STB: // ldr, ldb, str, stb
        if (opcode < STR)
            control = READ;
        else
            control = WRITE;
        int word = 1; // assume ldr and str, which are 32-bit operations
        if (opcode == LDB || opcode == STB) // 2 is ldb, 4 is stb
            word = 0; // ldb or stb
        switch (d->opcode & 0xf) {
          case ADDR:
            address = d->address;
            break;
          case BASE:
            address = registers[d->rm];
            break;
          case BASE_OFF:
            address = registers[d->rm] + d->immediate16;
            break;
          case BASE_REG:
            address = registers[d->rm] + registers[d->rn];
            break;
          case PREINC_OFF:
            registers[d->rm] += d->immediate16;
            address = registers[d->rm];
            break;
          case PREINC_REG:
            registers[d->rm] += registers[d->rn];
            address = registers[d->rm];
            break;
          case POSTINC_OFF:
            address = registers[d->rm];
            registers[d->rm] += d->immediate16;
            break;
          case POSTINC_REG:
            address = registers[d->rm];
            registers[d->rm] += registers[d->rn];
            break;
          default:
            printf("bad ldrstr\n");
            retval = ILLEGALINST;
        }
        unsigned char ldbstb;
        if (control == WRITE) {
            memaddr_changed = address;
            if (word) {
                memval_after = registers[d->rd];  // str changes 32-bits
                system_bus(address, &memval_before, READ);
            }
            else {
                memval_after = registers[d->rd] & 0xff; // stb changes a byte
                system_bus_b(address, &ldbstb, READ);
                memval_before = ldbstb;
            }
        }
        int mem_access_error = 0;
        if (word)
            mem_access_error = system_bus(address, &registers[d->rd], control);
        else {
            ldbstb = registers[d->rd] & 0xff;
            mem_access_error = system_bus_b(address, &ldbstb, control);
            if (control == READ)
                registers[d->rd] = ldbstb;
        }
        if (mem_access_error)
            return MEMERROR;
        pc += 4;
        break;
      case ADD_RD_RM_RN: case ADD_RD_RM_IMM:; // arilog ins - 5 is add r1,r2,r3 - 6 is add r1,r2,8
        int op2;
        if (opcode == ADD_RD_RM_RN)
            op2 = registers[d->rn];
        else
            op2 = d->immediate16;
        switch (d->opcode & 0xf) {
          float frd, frm, frn;
          case ADD:
              registers[d->rd] = registers[d->rm] + op2;
              break;
          case SUB:
              registers[d->rd] = registers[d->rm] - op2;
              break;
          case MUL:
              registers[d->rd] = registers[d->rm] * op2;
              break;
          case DIV:
              registers[d->rd] = registers[d->rm] / op2; // TODO check that divisor is 0
              break;
          case MOD:
              registers[d->rd] = registers[d->rm] % op2;
              break;
          case AND:
              registers[d->rd] = registers[d->rm] & op2;
              break;
          case ORR:
              registers[d->rd] = registers[d->rm] | op2;
              break;
          case EOR:
              registers[d->rd] = registers[d->rm] ^ op2;
              break;
          case ADC: // TODO fix so it does add with carry
              registers[d->rd] = registers[d->rm] + op2;
              break;
          case SBC: // TODO fix so it does sub with carry
              registers[d->rd] = registers[d->rm] - op2;
              break;
          case ADF:
              memcpy(&frm, &registers[d->rm], 4);
              memcpy(&frn, &op2, 4);
              frd = frm + frn;
              memcpy(&registers[d->rd], &frd, 4);
              break;
          case SUF:
              memcpy(&frm, &registers[d->rm], 4);
              memcpy(&frn, &op2, 4);
              frd = frm - frn;
              memcpy(&registers[d->rd], &frd, 4);
              break;
          case MUF:
              memcpy(&frm, &registers[d->rm], 4);
              memcpy(&frn, &op2, 4);
              frd = frm * frn;
              memcpy(&registers[d->rd], &frd, 4);
              break;
          case DIF:
              memcpy(&frm, &registers[d->rm], 4);
              memcpy(&frn, &op2, 4);
              frd = frm / frn;
              memcpy(&registers[d->rd], &frd, 4);
              break;
          default:
            printf("bad arilog\n");
            retval = ILLEGALINST;
        }
        pc += 4;
        break;
      case MOV_RD_RM: case MOV_RD_IMM:  // movcmp ins - 7 is cmp r1,r2 - 8 is cmp r1,8
        switch (d->opcode & 0xf) {
          case MOV: case MVA:  // mov mva
            if (opcode == MOV_RD_RM) // mov rd, rm or mva rd, rm
                registers[d->rd] = registers[d->rm];
            else if (opcode == MOV_RD_IMM && ((d->opcode & 0xf) == MVA)) // mva rd, #addr
                registers[d->rd] = d->address;
            else // mov rd, #imm
                registers[d->rd] = d->immediate20;
            pc += 4;
            if (d->rd == 15) // mov r15, rXX
                pc = registers[PC];
            break;
          case CMP: case TST: case TEQ:; // cmp, tst teq
            int val, op2;
            if (opcode == MOV_RD_RM) // rd, rm
                op2 = registers[d->rm];
            else // rd, #imm
                op2 = d->immediate20;
            switch (d->opcode & 0xf) {
              case CMP: // cmp
                val = registers[d->rd] - op2;
                break;
              case TST: // tst
                val = registers[d->rd] & op2;
                break;
              case TEQ: // teq
                val = registers[d->rd] ^ op2;
                break;
            }
            if (val == 0) {
                bit_set(&cpsr, Z);
                bit_clear(&cpsr, N);
                bit_clear(&cpsr, C);
                bit_clear(&cpsr, V);
            }
            else if (val < 0) {
                bit_clear(&cpsr, Z);
                bit_set(&cpsr, N);
                bit_clear(&cpsr, C);
                bit_clear(&cpsr, V);
            }
            else {
                bit_clear(&cpsr, Z);
                bit_clear(&cpsr, N);
                bit_clear(&cpsr, C);
                bit_clear(&cpsr, V);
            }
            pc += 4;
            break;
          case SHF: case SHA: case ROT: // shf, sha, rot
            if (opcode == MOV_RD_RM) // rd, rm
                op2 = registers[d->rm];
            else // rd, #imm
                op2 = d->immediate20;
            int left = 1;       // op2 > 0 is a left shift. reg value gets bigger
            if (op2 < 0) {  // op2 < 0 is a right shift. reg value gets smaller
                left = 0;
                op2 = -op2;
            }
            op2 %= 32; // make shift < 32
            switch (d->opcode & 0xf) {
              case SHF: // shf - logical shift
                if (left)
                    registers[d->rd] = registers[d->rd] << op2;
                else
                    // >> may be arithmetic shift. C standard does not state the type of shift
                    registers[d->rd] = (int)((unsigned int)registers[d->rd] >> op2);
                break;
              case SHA: // sha - arithmetic shift
                if (left) // left shift, propagate bit 0
                    if (bit_test(registers[d->rd], 0))
                        registers[d->rd] = ~(~registers[d->rd] << op2);
                    else
                        registers[d->rd] = registers[d->rd] << op2;
                else         // right shift, propagate bit 31
                             // reg < 0 ? ~(~reg >> op2) : reg >> op2;
                    if (bit_test(registers[d->rd], 31))
                        registers[d->rd] = ~(~registers[d->rd] >> op2);
                    else
                        registers[d->rd] = registers[d->rd] >> op2;
                break;
              case ROT: // rot - rotate
                if (left)
                    registers[d->rd] = (registers[d->rd] << op2) | 
                                       (int)((unsigned int)registers[d->rd] >> (32 - op2));
                else
                    registers[d->rd] = (registers[d->rd] >> op2) | (registers[d->rd] << (32 - op2));
                break;
            }
            pc += 4;
            break;
          case ONE: // one
            if (opcode == MOV_RD_RM) // rd, rm
                op2 = registers[d->rm];
            else // rd, #imm
                op2 = d->immediate20;
            registers[d->rd] = ~op2;
            pc += 4;
            break;
          case FTI: // fti
            if (opcode == MOV_RD_RM) // rd, rm
                op2 = registers[d->rm];
            else // rd, #imm
                op2 = d->immediate20;
            registers[d->rd] = (int)op2;
            pc += 4;
            break;
          case ITF: // itf - HERE - This needs to be rethought
            if (opcode == MOV_RD_RM) // rd, rm
                op2 = registers[d->rm];
            else // rd, #imm
                op2 = d->immediate20;
            float f = op2;
            memcpy(&registers[d->rd], &f, 4);
            pc += 4;
            break;
          case CMF: // cmf - Do we need compare floating point
            // Just use cmp since it sets CC on rd - op2
            break;
        }
        break;
      case B_ADDR: case B_REG: case B_REL: // branch inst
        if (opcode == B_ADDR)
            address = d->address;
        else if (opcode == B_REG)
            address = registers[d->rd];
        else
            address = registers[PC] + d->immediate20;
        switch (d->opcode & 0xf) {
          case BAL:
              pc = address;
              break;
          case BEQ:
              if (bit_test(cpsr, Z))
                  pc = address;
              else
                  pc += 4;
              break;
          case BNE:
              if (!bit_test(cpsr, Z))
                  pc = address;
              else
                  pc += 4;
              break;
          case BLE:
              if (bit_test(cpsr, Z) || (bit_test(cpsr, N) != bit_test(cpsr, V)))
                  pc = address;
              else
                  pc += 4;
              break;
          case BLT:
              if ((bit_test(cpsr, N) != bit_test(cpsr, V)))
                  pc = address;
              else
                  pc += 4;
              break;
          case BGE:
              if ((bit_test(cpsr, N) == bit_test(cpsr, V)))
                  pc = address;
              else
                  pc += 4;
              break;
          case BGT:
              if (!bit_test(cpsr, Z) && (bit_test(cpsr, N) == bit_test(cpsr, V)))
                  pc = address;
              else
                  pc += 4;
              break;
          case BLR:
              registers[LR] = pc + 4;
              pc = address;
              break;
          default:
              printf("Bad Branch Instruction\n");
              retval = ILLEGALINST;
        }
        break;
      case K_INST: // kernel inst
         switch (d->opcode & 0xf) {
           case KER: // ker, user to kernel mode
               if (bit_test(cpsr, U) && bit_test(cpsr, OS)) { // user mode and OS loaded
                   registers[0] = d->immediate20;  // ker #imm value to r0
                   registers[LR] = pc + 4;         // return address to LR
                   bit_clear(&cpsr, U);            // clear user mode bit
                   system_bus(rupt, &pc, READ);    // get base addr of OS from interrupt table
               }
               else {
                   printf("Illegal use of ker Instruction\n");
                   retval = ILLEGALINST;
               }
               break;
           case SRG: // srg, status reg instruction
               if (!bit_test(cpsr, U)) { // 0xb1 must be done in kernel mode
                   int bit_pos = d->immediate20 & 0x1f; // 5 bits for 0 to 31
                   if (d->immediate20 >> 5 & 1)
                      bit_set(&cpsr, bit_pos);
                   else
                      bit_clear(&cpsr, bit_pos);
                   pc += 4;
               }
               else {
                   printf("Illegal use of srg Instruction\n");
                   retval = ILLEGALINST;
               }
               break;
           case IOI: // ioi, input output instruction
               // 0xb2 must be done in kernel mode with the OS loaded
               if (!bit_test(cpsr, U) && bit_test(cpsr, OS)) {
                   if (scanfdone) {
                       pc += 4;
                       scanfdone = 0;
                   }
                   else {
                       chemuioi(d->immediate20);
                       if (doscanf)
                           return SCANF;
                       else
                           pc += 4;
                   }
               }
               else {
                   printf("Illegal use of ioi Instruction\n");
                   retval = ILLEGALINST;
               }
               break;
         }
       break;
    }
    free(d); // free the decoded struct
    registers[PC] = pc;
    if (verbose_cpu) {
        system_bus(pc, &inst, READ);
        printf("post-step:  cpsr: 0x%08x, pc: 0x%08x, inst: 0x%08x, %s\n", cpsr, registers[PC], inst, disassemble(inst));
    }
    return retval;
}

/*******************************************************************
 ***************************  STEP_N  ******************************
 ** step_n() calls step() n times                                 **
 ** return values                                                 **
 **  return ILLEGALINST step_n encountered an illegal instruction **
 **  return NORMAL  step_n stepped all steps                      **
 **  return BREAKPOINT  step_n encountered a breakpoint           **
 **  return BALTOSELF  step_n encountered a bal to itself         **
 **  return SCANF  step_n waiting on input from scanf             **
 **  return MEMERROR  step_n encountered memory access error      **
 *******************************************************************/
enum stepret step_n(int n) {
    enum stepret s = NORMAL;
    for (int i = 0; i < n; i++) {
        int pc = registers[15];
        s = step();
        if (registers[15] == pc && s == NORMAL)
            return BALTOSELF;
        if (s == NORMAL)
            continue;
        return s;
    }
    return s;
}

/******************************************************************
 *********************  SHOW_MEM_CHANGED  *************************
 ** the variables memaddr_changed, memval_before, memval_after   **
 ** are modified by step() and used by show_mem_changed()        **
 ** show_mem_changed() sprintf's the changed values into the     **
 ** variable mem_changed, which is displayed by chemu.c          **
 ******************************************************************/
// mem_changed is written to regisers window
char mem_changed[80] = "addr: 0xffff, before: 0xffffffff, after: 0xffffffff";
void show_mem_changed() {
    if (memaddr_changed)
        sprintf(mem_changed, "addr: 0x%04x, before: 0x%08x, after: 0x%08x", memaddr_changed, memval_before, memval_after);
    else
        strcpy(mem_changed, "addr: 0xffff, before: 0xffffffff, after: 0xffffffff");
}


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

int cmdgetstr(char **ps, char *es, char **str);
#define MAXARGS 10
static char *cmdargv[MAXARGS];
int do_cmd(int argc, char **cmdargv);
void do_script(char *scriptfilename);
int process_args(int argc, char **argv);
int getcmd(char *buf, int nbuf);

// Register values
#define CHEMU_INSTRUCTION_DISPLAY_COUNT 17
// static int registers[CHEMU_INSTRUCTION_DISPLAY_COUNT];

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
    char **argv = { command };
    // we now know what the user sent over...
    do_cmd(1, argv);
    // printf("%s", command);
    PyObject *instructions = PyList_New(0);
    char hex_buffer[11];
    char pc_value[11];
    for (int i = 0; i < CHEMU_INSTRUCTION_DISPLAY_COUNT; i++) {
        snprintf(hex_buffer, 11, "0x%08X", registers[i]);
        if (i == 8) {
            strcpy(pc_value, hex_buffer);
        }
        PyList_Append(instructions, Py_BuildValue("{ssss}", "instruction", "testing", "address", hex_buffer));
    }

    PyObject *registers = PyList_New(0);
    char cpsr_value[11];
    snprintf(cpsr_value, 11, "0x%08X", cpsr);
    PyObject *reg16 = Py_BuildValue("{siss}", "register", 16, "value", cpsr_value);
    PyObject *cpsr = Py_BuildValue("{siss}", "register", 15, "value", pc_value);
    
    PyList_Append(registers, reg16);
    PyList_Append(registers, cpsr);

    PyObject *returnValue = Py_BuildValue("{sOsO}", "registers", registers, "instructions", instructions);
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
    // initalize RNG with current time as seed
    time_t t;
    time(&t);
    srand(t);
    load_memory("figisa30.o");
    // return the filled-in module struct
    return PyModule_Create(&chemuModule);
}