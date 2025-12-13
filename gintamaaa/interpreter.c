#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "interpreter.h"

#define NODE_PRINT_PART 7

// static void debug_print_ast(Node *node, int depth);

typedef struct Variable {
    char *name;
    union {
        int int_val;
        char *str_val;
    } value;
    bool is_string;
    bool initialized;
} Variable;

struct InterpreterState {
    Variable *vars;
    int var_count;
    int var_capacity;
    OutputCapture *output;
};

static Variable* find_variable(InterpreterState *state, const char *name) {
    for(int i = 0; i < state->var_count; i++) {
        if(strcmp(state->vars[i].name, name) == 0) {
            return &state->vars[i];
        }
    }
    return NULL;
}

static Variable* add_variable(InterpreterState *state, const char *name) {
    if(state->var_count >= state->var_capacity) {
        state->var_capacity = state->var_capacity ? state->var_capacity * 2 : 10;
        state->vars = realloc(state->vars, sizeof(Variable) * state->var_capacity);
    }
    
    Variable *var = &state->vars[state->var_count++];
    var->name = strdup(name);
    var->value.int_val = 0;
    var->initialized = false;
    var->is_string = false;
    return var;
}

static InterpreterState* create_state() {
    InterpreterState *state = malloc(sizeof(InterpreterState));
    state->var_capacity = 10;
    state->var_count = 0;
    state->vars = malloc(sizeof(Variable) * state->var_capacity);
    state->output = malloc(sizeof(OutputCapture));
    capture_init(state->output);
    return state;
}

static void free_state(InterpreterState *state) {
    for(int i = 0; i < state->var_count; i++) {
        free(state->vars[i].name);
        ////
        if(state->vars[i].is_string && state->vars[i].initialized) {
            free(state->vars[i].value.str_val);  // free string if it's a string var
        }
        /////
    }
    free(state->vars);
    if(state->output) {
        capture_free(state->output);
        free(state->output);
    }
    free(state);
}

///////
// get integer value from variable
static int get_int_value(Variable *var) {
    if(!var || !var->initialized)
        return 0;
    if(var->is_string)
        return 0;  // string treated as 0 for integer operations
    return var->value.int_val;
}

// get string value from variable (caller must not free)
static const char* get_str_value(Variable *var) {
    if(!var || !var->initialized)
        return "";
    if(!var->is_string)
        return "";  // int treated as empty string for string operations
    return var->value.str_val ? var->value.str_val : "";
}
////////

static int evaluate_expression(Node *node, InterpreterState *state) {
    if(!node)
        return 0;
    
    switch(node->node_type) {
        case 0: // NODE_NUM
            return node->int_val;
            
        case 2: // NODE_ID
        {
            Variable *var = find_variable(state, node->str_val);
            if(!var) {
                var = add_variable(state, node->str_val);
            }
            if(!var->initialized) {
                // no warning, just 0
                //printf("DEBUG: Variable '%s' not initialized, returning 0\n", node->str_val);
            }
            return get_int_value(var);
        }
            
        case 3: // NODE_BINOP
        {
            int left = evaluate_expression(node->binop.left, state);
            int right = evaluate_expression(node->binop.right, state);
            
            switch(node->binop.op) {
                case '+': return left + right;
                case '-': return left - right;
                case '*': return left * right;
                case '/': return right != 0 ? left / right : 0;
                case '=': // should be handled in execute_statement
                    return left;
            }
            return 0;
        }
            
        default:
            return 0;
    }
}

static void execute_statement(Node *node, InterpreterState *state) {
    if(!node)
        return;
    
    switch(node->node_type) {
        case 4: // NODE_DECL
        {
            Node *current = node->list.items;
            while(current) {
                if(current->node_type == 3 && current->binop.op == '=') {
                    // declaration w/ initialization
                    Node *left = current->binop.left;
                    Node *right = current->binop.right;
                    
                    Variable *var = find_variable(state, left->str_val);
                    if(!var) {
                        var = add_variable(state, left->str_val);
                    }
                    
                    var->value.int_val = evaluate_expression(right, state);
                    var->initialized = true;
                    var->is_string = false;
                } else if(current->node_type == NODE_STR_ASSIGN) {  // string assignment in declaration
                    // ch var = "string"
                    Node *id_node = current->str_assign.id;
                    Node *str_node = current->str_assign.str;
                    
                    Variable *var = find_variable(state, id_node->str_val);
                    if(!var) {
                        var = add_variable(state, id_node->str_val);
                    }
                    
                    var->value.str_val = strdup(str_node->str_val);
                    var->initialized = true;
                    var->is_string = true;  // string type
                    
                } else if(current->node_type == 2) {
                    // declaration without initialization
                    Variable *var = find_variable(state, current->str_val);
                    if(!var) {
                        var = add_variable(state, current->str_val);
                    }
                    var->initialized = false;
                    var->value.int_val = 0;
                    // we dont know if it's string or int from just "ch var" or "int var"
                    // we need type info from semantics
                }
                current = current->list.next;
            }
            break;
        }
            
         case 5: // NODE_ASSIGN
        {
            Node *current = node->list.items;
            while(current) {
                if(current->node_type == 3 && current->binop.op == '=') {
                    // integer assignment: x = expr
                    Node *left = current->binop.left;
                    Node *right = current->binop.right;
                    
                    Variable *var = find_variable(state, left->str_val);
                    if(!var) {
                        var = add_variable(state, left->str_val);
                    }
                    
                    var->value.int_val = evaluate_expression(right, state);
                    var->initialized = true;
                    var->is_string = false;
                    
                } else if(current->node_type == NODE_STR_ASSIGN) {  // string assignment
                    // var = "string"
                    Node *id_node = current->str_assign.id;
                    Node *str_node = current->str_assign.str;
                    
                    Variable *var = find_variable(state, id_node->str_val);
                    if(!var) {
                        var = add_variable(state, id_node->str_val);
                    }
                
                    // free old string if it exists
                    if(var->is_string && var->initialized && var->value.str_val) {
                        free(var->value.str_val);
                    }
                    
                    var->value.str_val = strdup(str_node->str_val);
                    var->initialized = true;
                    var->is_string = true;
                }
                current = current->list.next;
            }
            break;
        }

         case 6: // NODE_PRINT
        {
            Node *current = node->print_stmt.parts;
            while(current) {
                if(current->node_type == NODE_PRINT_PART) {
                    Node *content = current->list.items;
                    if(content->node_type == 1) {  // STR literal
                        //printf("DEBUG: Printing string: %s\n", content->str_val);
                        capture_printf(state->output, "%s", content->str_val);
                    } else if(content->node_type == 2) {  // ID (variable)
                        Variable *var = find_variable(state, content->str_val);
                        if(var && var->initialized) {
                            if(var->is_string) {
                                //printf("DEBUG: Printing string var '%s': %s\n", 
                                   // content->str_val, var->value.str_val);
                                capture_printf(state->output, "%s", var->value.str_val);
                            } else {
                                //printf("DEBUG: Printing int var '%s': %d\n", 
                                  //  content->str_val, var->value.int_val);
                                capture_printf(state->output, "%d", var->value.int_val);
                            }
                        } else {
                            //printf("DEBUG: Variable '%s' not found/initialized\n", content->str_val);
                            capture_printf(state->output, "0");
                        }
                    } else {  // expression
                        int value = evaluate_expression(content, state);
                        //printf("DEBUG: Printing expression result: %d\n", value);
                        capture_printf(state->output, "%d", value);
                    }
                }
                current = current->list.next;
            }
            // newline after print statement
            capture_printf(state->output, "\n");
            break;
        }
    }
}

char* interpret_program(Node *program) {
    InterpreterState *state = create_state();

    // execute all statements
    Node *current = program;
    while(current) {
        execute_statement(current, state);
        current = current->list.next;
    }
    
    char *result = strdup(capture_get(state->output));
    
    free_state(state);
    
    return result;
}


// // debugging....
// static void debug_print_ast(Node *node, int depth) {
//     for(int i = 0; i < depth; i++) 
//         printf("  ");
//     if(!node) { 
//             printf("NULL\n"); 
//             return; 
//     }
    
//     printf("Node type: %d", node->node_type);
//     switch (node->node_type) {
//         case 0: printf(" (NUM) value: %d\n", node->int_val); break;
//         case 1: printf(" (STR) value: %s\n", node->str_val); break;
//         case 2: printf(" (ID) name: %s\n", node->str_val); break;
//         case 3: printf(" (BINOP) op: %c\n", node->binop.op); 
//                 debug_print_ast(node->binop.left, depth + 1);
//                 debug_print_ast(node->binop.right, depth + 1);
//                 break;
//         case 4: printf(" (DECL)\n"); 
//                 debug_print_ast(node->list.items, depth + 1);
//                 debug_print_ast(node->list.next, depth + 1);
//                 break;
//         case 5: printf(" (ASSIGN)\n");
//                 debug_print_ast(node->list.items, depth + 1);
//                 debug_print_ast(node->list.next, depth + 1);
//                 break;
//         case 6: printf(" (PRINT)\n");
//                 debug_print_ast(node->print_stmt.parts, depth + 1);
//                 break;
//         default: printf(" (UNKNOWN)\n");
//     }
// }