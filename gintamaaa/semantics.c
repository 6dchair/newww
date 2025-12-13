#include "semantics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sem_init(Semantics *sem) {
    sem->symbol_table = NULL;
    sem->current_line = 0;
    sem->error_count = 0;
    sem->in_decl_line = false;
}

void sem_set_line(Semantics *sem, int line) {
    sem->current_line = line;
}

void sem_set_decl_line(Semantics *sem, bool is_decl_line) {
    sem->in_decl_line = is_decl_line;
}

bool sem_check_declared(Semantics *sem, const char *name) {
    Symbol *s = sem->symbol_table;
    while(s) {
        if(strcmp(s->name, name) == 0) {
            return true;
        }
        s = s->next;
    }
    
    fprintf(stderr, "Line %d: Variable '%s' used before declaration\n", 
            sem->current_line, name);
    sem->error_count++;
    return false;
}

bool sem_add_symbol(Semantics *sem, const char *name, bool is_string) {
    // check for duplicate declaration
    Symbol *s = sem->symbol_table;
    while(s) {
        if(strcmp(s->name, name) == 0) {
            if(sem->in_decl_line) {
                fprintf(stderr, "Line %d: Variable '%s' already declared\n", 
                        sem->current_line, name);
                sem->error_count++;
                return false;
            }
            return true;
        }
        s = s->next;
    }
    
    // add new symbol
    Symbol *new_sym = malloc(sizeof(Symbol));
    if(!new_sym) {
        fprintf(stderr, "Memory allocation error\n");
        return false;
    }
    
    new_sym->name = strdup(name);
    new_sym->declared_line = sem->current_line;
    new_sym->initialized = false;
    new_sym->is_string = is_string;  // store type
    new_sym->next = sem->symbol_table;
    sem->symbol_table = new_sym;
    
    return true;
}


bool sem_is_duplicate(Semantics *sem, const char *name) {
    Symbol *s = sem->symbol_table;
    while(s) {
        if(strcmp(s->name, name) == 0) {
            return true;
        }
        s = s->next;
    }
    return false;
}

int sem_get_error_count(Semantics *sem) {
    return sem->error_count;
}

void sem_print_symbols(Semantics *sem) {
    printf("\nSymbol Table\n");
    Symbol *s = sem->symbol_table;
    while(s) {
        printf("  %s (declared at line %d, initialized: %s)\n",
               s->name, s->declared_line, s->initialized ? "yes" : "no");
        s = s->next;
    }
}

void sem_cleanup(Semantics *sem) {
    Symbol *current = sem->symbol_table;
    while(current) {
        Symbol *next = current->next;
        free(current->name);
        free(current);
        current = next;
    }
    sem->symbol_table = NULL;
}

// check for division by zero in constant expressions
bool sem_check_division_by_zero(Node *expr_node) {
    if(!expr_node) 
        return true;
    
    switch(expr_node->node_type) {
        case 3: { // NODE_BINOP
            if(expr_node->binop.op == '/') {
                // check right side
                Node *right = expr_node->binop.right;
                if(right->node_type == 0) { // NODE_NUM
                    if(right->int_val == 0) {
                        return false; // division by zero
                    }
                }
            }
            // recursively check both sides
            return sem_check_division_by_zero(expr_node->binop.left) &&
                   sem_check_division_by_zero(expr_node->binop.right);
        }
        default:
            return true;
    }
}

// check variable type
bool sem_is_string_type(Semantics *sem, const char *name) {
    Symbol *s = sem->symbol_table;
    while(s) {
        if(strcmp(s->name, name) == 0) {
            return s->is_string;
        }
        s = s->next;
    }
    return false;  // not fousnd
}

// check for type mismatch in assignment
bool sem_check_type_compatibility(Semantics *sem, const char *name, bool is_string_assign) {
    Symbol *s = sem->symbol_table;
    while(s) {
        if(strcmp(s->name, name) == 0) {
            if(s->is_string != is_string_assign) {
                fprintf(stderr, "Line %d: Type mismatch for variable '%s'\n",
                        sem->current_line, name);
                sem->error_count++;
                return false;
            }
            return true;
        }
        s = s->next;
    }
    return false;  // var not declared
}

// FIX 8: do not add to symbol table if vars are declared/assigned a value incorrectly
bool is_string_expression(Node *expr) {
    if(!expr)
        return false;
    return expr->node_type == 1; // NODE_STR
}

bool is_constant_expression(Node *expr) {
    if(!expr)
        return false;
    
    switch(expr->node_type) {
        case 0:  // NUM - always constant
            return true;
        case 1:  // STR - always constant
            return true;
        case 3:  // BINOP - check if both children are constant
            return is_constant_expression(expr->binop.left) && 
                   is_constant_expression(expr->binop.right);
        default:
            return false;
    }
}

// evaluate constant numeric expression
int eval_constant_expression(Node *expr) {
    if(!expr)
        return 0;
    
    switch(expr->node_type) {
        case 0:  // NUM
            return expr->int_val;
        case 3:  // BINOP
            int left = eval_constant_expression(expr->binop.left);
            int right = eval_constant_expression(expr->binop.right);
            switch(expr->binop.op) {
                case '+': return left + right;
                case '-': return left - right;
                case '*': return left * right;
                case '/': return right != 0 ? left / right : 0;
                default: return 0;
            }
        default:
            return 0;
    }
}
