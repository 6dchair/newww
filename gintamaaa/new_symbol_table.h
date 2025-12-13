#ifndef NEW_SYMBOL_TABLE_H
#define NEW_SYMBOL_TABLE_H

#include <stdint.h>

#define MAX_SYMBOLS 100
#define MAX_NAME_LEN 50
#define REG_MIN 1 // start from r1
#define REG_MAX 19 // up to r19

void PrintDataSection(FILE *out);
void SymbolInit();
int GetRegisterOfTheSymbol(const char *name);
int SymbolExists(const char *name);
int AllocateRegisterForTheSymbol(const char *name);
uint64_t GetOffsetOfTheSymbol(const char *name);
void PrintAllSymbols(FILE *out);

#endif