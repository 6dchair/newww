#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "new_symbol_table.h"

// symbol table entry
typedef struct {
    char name[MAX_NAME_LEN];
    int reg;           // register assigned
    uint64_t offset;   // memory offset
} SymbolEntry;

static SymbolEntry table[MAX_SYMBOLS];
static int symbol_count = 0;
static int next_reg = REG_MIN;
static uint64_t next_offset = 0x0;

// print .data section with .space directives
void PrintDataSection(FILE *out) {
    for(int i = 0; i < symbol_count; i++) {
        fprintf(out, "%s: .space 8\n", table[i].name);
    }
}

// initialize/reset symbol table
void SymbolInit() {
    symbol_count = 0;
    next_reg = REG_MIN;
    next_offset = 0x0;
    for(int i = 0; i < MAX_SYMBOLS; i++) {
        table[i].name[0] = '\0';
    }
}

// get register assigned to symbol
int GetRegisterOfTheSymbol(const char *name) {
    for(int i = 0; i < symbol_count; i++) {
        if(strcmp(table[i].name, name) == 0) {
            return table[i].reg;
        }
    }
    return -1;
}

// check if symbol exists
int SymbolExists(const char *name) {
    return GetRegisterOfTheSymbol(name) != -1;
}

// allocate a register for a new symbol
int AllocateRegisterForTheSymbol(const char *name) {
    // check if alr allocated
    int existing = GetRegisterOfTheSymbol(name);
    if(existing != -1) {
        return existing;
    }
    
    // check limits
    if(symbol_count >= MAX_SYMBOLS) {
        return -1;
    }
    
    // skip r1-r4 (r4 is for syscall args)
    // skip forward to r5 if we're in the syscall range
    if(next_reg >= 1 && next_reg <= 4) {
        next_reg = 5;
    }
    
    if(next_reg > REG_MAX) {
        return -1;
    }
    
    // add symbol to table
    strncpy(table[symbol_count].name, name, MAX_NAME_LEN - 1);
    table[symbol_count].name[MAX_NAME_LEN - 1] = '\0';
    table[symbol_count].reg = next_reg;
    table[symbol_count].offset = next_offset;
    
    symbol_count++;
    next_offset += 8;  // 8 bytes per variable
    
    return next_reg++;
}

// get memory offset for symbol
uint64_t GetOffsetOfTheSymbol(const char *name) {
    for(int i = 0; i < symbol_count; i++) {
        if(strcmp(table[i].name, name) == 0) {
            return table[i].offset;
        }
    }
    return 0;
}

// print symbol table for debugging
void PrintAllSymbols(FILE *out) {
    fprintf(out, "# Symbol Table\n");
    fprintf(out, "# Name\tReg\tOffset\n");
    for(int i = 0; i < symbol_count; i++) {
        fprintf(out, "# %s\tr%d\t0x%lX\n",
                table[i].name,
                table[i].reg,
                (unsigned long)table[i].offset);
    }
    fprintf(out, "\n");
}