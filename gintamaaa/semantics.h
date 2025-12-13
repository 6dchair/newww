#ifndef SEMANTICS_H
#define SEMANTICS_H

#include <stdbool.h>
#include "ast.h"

// symbol table entry
typedef struct Symbol {
    char *name;
    int declared_line;
    bool initialized;
    bool is_string;
    struct Symbol *next;
} Symbol;

// semantic analyzer state
typedef struct Semantics {
    Symbol *symbol_table;
    int current_line;
    int error_count;
    bool in_decl_line;  // are we parsing a declaration line?
} Semantics;

// initialize semantic analyzer
void sem_init(Semantics *sem);

// set current line number
void sem_set_line(Semantics *sem, int line);

// set declaration line flag
void sem_set_decl_line(Semantics *sem, bool is_decl_line);

// check if variable is declared b4 use
bool sem_check_declared(Semantics *sem, const char *name);

// check 4 duplicate declaration
bool sem_is_duplicate(Semantics *sem, const char *name);

// get error count
int sem_get_error_count(Semantics *sem);

// print symbol table (for debugging)
void sem_print_symbols(Semantics *sem);

// clean up
void sem_cleanup(Semantics *sem);

// add to semantics.h
bool sem_check_division_by_zero(Node *expr_node);

// update sem_add_symbol signature
bool sem_add_symbol(Semantics *sem, const char *name, bool is_string);
bool sem_is_string_type(Semantics *sem, const char *name);

int eval_constant_expression(Node *expr);
bool is_constant_expression(Node *expr);

// FIX 8: do not add to symbol table if vars are declared/assigned a value incorrectly
bool is_string_expression(Node *expr);

#endif