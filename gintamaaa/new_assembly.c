#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "new_assembly.h"
#include "new_symbol_table.h"
#include "ast.h"

// string table for storing string literals
typedef struct {
    char *label;
    char *value;
} StringEntry;

static StringEntry string_table[100];
static int string_count = 0;
static int string_label_counter = 0;

// track w/c vars have been initialized
static char *initialized_vars[100];
static int init_var_count = 0;

// r4 for syscall arguments
// r10-r19 for temporary calculations
static int temp_start = 10;
static int temp_next = 10;
static int temp_max = 19;

// get or create label for a string literal
static char* GetStringLabel(const char *str) {
    // process escape sequences
    char *processed_str = malloc(strlen(str) * 2 + 1);
    char *dst = processed_str;
    
    for(const char *src = str; *src; src++) {
        if(*src == '\\' && *(src+1)) {
            src++;
            switch(*src) {
                case 'n': *dst++ = '\n'; break;
                case 't': *dst++ = '\t'; break;
                case '"': *dst++ = '"'; break;
                case '\\': *dst++ = '\\'; break;
                default:
                    *dst++ = '\\';
                    *dst++ = *src;
                    break;
            }
        } else {
            *dst++ = *src;
        }
    }
    *dst = '\0';
    
    // check if string alr exists
    for(int i = 0; i < string_count; i++) {
        if(strcmp(string_table[i].value, processed_str) == 0) {
            free(processed_str);
            return string_table[i].label;
        }
    }
    
    // create new string entry
    if(string_count >= 100) {
        free(processed_str);
        return NULL;
    }
    
    string_table[string_count].value = processed_str;
    char *label = malloc(20);
    sprintf(label, "str%d", string_label_counter++);
    string_table[string_count].label = label;
    
    string_count++;
    return label;
}

// mark variable as initialized
static void mark_initialized(const char *name) {
    for(int i = 0; i < init_var_count; i++) {
        if(strcmp(initialized_vars[i], name) == 0)
            return;
    }
    if(init_var_count < 100) {
        initialized_vars[init_var_count++] = strdup(name);
    }
}

// initialize assembly generator
void AssemblyInit() {
    temp_next = temp_start;
    init_var_count = 0;
}

// allocate a temporary reg (r10-r19)
static int NewTempRegister() {
    int r = temp_next++;
    if(temp_next > temp_max)
        temp_next = temp_start;
    return r;
}

// reset temporary reg allocation
static void ResetTempRegister() {
    temp_next = temp_start;
}

// load var from memory
static void LoadVariable(FILE *out, int reg, const char *name) {
    fprintf(out, "ld r%d, %s(r0)\n", reg, name);
}

// store var to memory
static void StoreVariable(FILE *out, int reg, const char *name) {
    fprintf(out, "sd r%d, %s(r0)\n", reg, name);
}

// load immediate value into register
static void GenerateLoadImmediate(FILE *out, int reg, long long imm) {
    fprintf(out, "daddiu r%d, r0, #%lld\n", reg, imm);
}

// collect symbols and strings from AST
static void CollectSymbolsFromAST(Node *node) {
    if(!node)
        return;
    
    Node *current = node;
    while(current) {
        switch(current->node_type) {
            case 1: // NODE_STR - string literal
                GetStringLabel(current->str_val);
                break;
                
            case 4: { // NODE_DECL - declaration
                Node *item = current->list.items;
                while(item) {
                    if(item->node_type == 2) {
                        // simple declaration: int x
                        AllocateRegisterForTheSymbol(item->str_val);
                    } else if(item->node_type == 3 && item->binop.op == '=') {
                        // initialized declaration: int x = expr
                        if(item->binop.left && item->binop.left->node_type == 2) {
                            AllocateRegisterForTheSymbol(item->binop.left->str_val);
                        }
                        CollectSymbolsFromAST(item->binop.right);
                    }
                    item = item->list.next;
                }
                break;
            }
                
            case 5: { // NODE_ASSIGN - assignment
                Node *assign = current->list.items;
                while(assign) {
                    if(assign->node_type == 3 && assign->binop.op == '=') {
                        if(assign->binop.left && assign->binop.left->node_type == 2) {
                            AllocateRegisterForTheSymbol(assign->binop.left->str_val);
                        }
                        CollectSymbolsFromAST(assign->binop.right);
                    }
                    assign = assign->list.next;
                }
                break;
            }
                
            case 6: { // NODE_PRINT - print statement
                Node *part = current->print_stmt.parts;
                while(part) {
                    if(part->node_type == 7) {  // NODE_PRINT_PART
                        Node *content = part->list.items;
                        if(content && content->node_type == 1) {
                            GetStringLabel(content->str_val);
                        } else {
                            CollectSymbolsFromAST(content);
                        }
                    } else if(part->node_type == 1) {
                        GetStringLabel(part->str_val);
                    } else {
                        CollectSymbolsFromAST(part);
                    }
                    part = part->list.next;
                }
                break;
            }
                
            case 3: // NODE_BINOP - expression
                CollectSymbolsFromAST(current->binop.left);
                CollectSymbolsFromAST(current->binop.right);
                break;
                
            case 2: // NODE_ID - variable reference
                AllocateRegisterForTheSymbol(current->str_val);
                break;
                
            case 7: // NODE_PRINT_PART
                CollectSymbolsFromAST(current->list.items);
                break;
        }
        
        current = current->list.next;
    }
}

// generate code for an expression
static int GenerateExpression(Node *node, FILE *out, int target_reg) {
    if(!node)
        return 0;
    
    // handle NODE_PRINT_PART wrapper
    if(node->node_type == 7) {
        return GenerateExpression(node->list.items, out, target_reg);
    }

    switch(node->node_type) {
        case 0: { // NODE_NUM - number literal
            int reg = target_reg ? target_reg : NewTempRegister();
            GenerateLoadImmediate(out, reg, node->int_val);
            return reg;
        }
            
        case 2: { // NODE_ID - variable reference
            if(target_reg) {
                // load directly into target register
                fprintf(out, "ld r%d, %s(r0)\n", target_reg, node->str_val);
                return target_reg;
            } else {
                // load into temporary register
                int reg = NewTempRegister();
                fprintf(out, "ld r%d, %s(r0)\n", reg, node->str_val);
                return reg;
            }
        }
            
        case 3: { // NODE_BINOP - binary operation
            if(node->binop.op == '=') {
                return GenerateExpression(node->binop.left, out, target_reg);
            }
            
            // for binary ops w/ target_reg (can be optimized)
            if(target_reg) {
                // evaluate left into temp
                int left_reg = GenerateExpression(node->binop.left, out, 0);
                
                // if operation is commutative (+, *), it could be potentially
                // evaluated right into target_reg if it's simple
                int right_reg = GenerateExpression(node->binop.right, out, 0);
                
                // generate operation w/ target_reg as destination
                switch(node->binop.op) {
                    case '+':
                        fprintf(out, "daddu r%d, r%d, r%d\n", target_reg, left_reg, right_reg);
                        break;
                    case '-':
                        fprintf(out, "dsubu r%d, r%d, r%d\n", target_reg, left_reg, right_reg);
                        break;
                    case '*':
                        fprintf(out, "dmult r%d, r%d\n", left_reg, right_reg);
                        fprintf(out, "mflo r%d\n", target_reg);
                        break;
                    case '/':
                        fprintf(out, "ddiv r%d, r%d\n", left_reg, right_reg);
                        fprintf(out, "mflo r%d\n", target_reg);
                        break;
                }
                
                return target_reg;
            } else {
                // no target_reg specified, use normal evaluation
                int left_reg = GenerateExpression(node->binop.left, out, 0);
                int right_reg = GenerateExpression(node->binop.right, out, 0);
                int result_reg = NewTempRegister();
                
                switch(node->binop.op) {
                    case '+':
                        fprintf(out, "daddu r%d, r%d, r%d\n", result_reg, left_reg, right_reg);
                        break;
                    case '-':
                        fprintf(out, "dsubu r%d, r%d, r%d\n", result_reg, left_reg, right_reg);
                        break;
                    case '*':
                        fprintf(out, "dmult r%d, r%d\n", left_reg, right_reg);
                        fprintf(out, "mflo r%d\n", result_reg);
                        break;
                    case '/':
                        fprintf(out, "ddiv r%d, r%d\n", left_reg, right_reg);
                        fprintf(out, "mflo r%d\n", result_reg);
                        break;
                }
                
                return result_reg;
            }
        }
    }
    
    return 0;
}

static void GenerateDeclaration(Node *node, FILE *out) {
    if(!node || node->node_type != 4)
        return;
    
    Node *current = node->list.items;
    while(current) {
        if(current->node_type == 3 && current->binop.op == '=') {
            Node *left = current->binop.left;
            Node *right = current->binop.right;
            
            // allocate symbol
            AllocateRegisterForTheSymbol(left->str_val);
            mark_initialized(left->str_val);
            
            // evaluate expression into r4
            GenerateExpression(right, out, 4);
            
            // store from r4 to memory
            fprintf(out, "sd r4, %s(r0)\n", left->str_val);
            
        } else if(current->node_type == 2) {
            // simple declaration (no initialization)
            AllocateRegisterForTheSymbol(current->str_val);
        }
        current = current->list.next;
    }
}

static void GenerateAssignment(Node *node, FILE *out) {
    if(!node || node->node_type != 5)
        return;
    
    Node *current = node->list.items;
    while(current) {
        if(current->node_type == 3 && current->binop.op == '=') {
            Node *left = current->binop.left;
            Node *right = current->binop.right;
            
            // get symbol
            GetRegisterOfTheSymbol(left->str_val);
            mark_initialized(left->str_val);
            
            // evaluate expression into r4
            GenerateExpression(right, out, 4); 
            
            // stire from r4 to memory
            fprintf(out, "sd r4, %s(r0)\n", left->str_val);
        }
        current = current->list.next;
    }
}

// generate code for print statement
static void GeneratePrint(Node *node, FILE *out) {
    if(!node || node->node_type != 6)
        return;
    
    Node *current = node->print_stmt.parts;
    
    // first print all strings
    while(current) {
        Node *content = current;
        if(current->node_type == 7) {
            content = current->list.items;
        }
        
        if(content && content->node_type == 1) {  // string
            char *label = GetStringLabel(content->str_val);
            if(label) {
                fprintf(out, "daddiu r4, r0, %s\n", label);
                fprintf(out, "syscall 4\n");
            }
        }
        current = current->list.next;
    }
    
    // then print all expressions - EVALUATE DIRECTLY INTO r4
    current = node->print_stmt.parts;
    while(current) {
        Node *content = current;
        if(current->node_type == 7) {
            content = current->list.items;
        }
        
        if(content && content->node_type != 1) {  // not a string
            // evaluate expression directly into r4
            GenerateExpression(content, out, 4);  // target reg = 4
            
            fprintf(out, "syscall 1\n");
        }
        current = current->list.next;
    }
    
    // print newline
    fprintf(out, "daddiu r4, r0, #10\n");
    fprintf(out, "syscall 11\n");
}

// generate code for a single AST node
void GenerateAssemblyNode(Node *node, FILE *out) {
    if(!node || !out)
        return;
    
    ResetTempRegister();
    
    switch(node->node_type) {
        case 4: // NODE_DECL
            GenerateDeclaration(node, out);
            break;
        case 5: // NODE_ASSIGN
            GenerateAssignment(node, out);
            break;
        case 6: // NODE_PRINT
            GeneratePrint(node, out);
            break;
        default:
            // traverse list nodes
            if(node->list.items) {
                GenerateAssemblyNode(node->list.items, out);
            }
            if(node->list.next) {
                GenerateAssemblyNode(node->list.next, out);
            }
            break;
    }
}

// generate complete assembly program
void GenerateAssemblyProgram(Node *program, FILE *out) {
    if(!program || !out)
        return;
    
    // initialize
    SymbolInit();
    AssemblyInit();
    string_count = 0;
    string_label_counter = 0;
    
    // collect all symbols and strings
    CollectSymbolsFromAST(program);
    
    // generate .data section
    fprintf(out, ".data\n");
    PrintDataSection(out);  // vars
    
    // generate string literals
    for(int i = 0; i < string_count; i++) {
        fprintf(out, "%s: .asciiz \"", string_table[i].label);
        for(char *p = string_table[i].value; *p; p++) {
            if(*p == '\n') fprintf(out, "\\n");
            else if(*p == '"') fprintf(out, "\\\"");
            else if(*p == '\\') fprintf(out, "\\\\");
            else fputc(*p, out);
        }
        fprintf(out, "\"\n");
    }
    fprintf(out, "\n.code\n");
    
    // generate code
    Node *current = program;
    while(current) {
        GenerateAssemblyNode(current, out);
        current = current->list.next;
    }
    
    // exit program
    fprintf(out, "syscall 10\n");
    
    // cleanup
    for(int i = 0; i < string_count; i++) {
        free(string_table[i].value);
        free(string_table[i].label);
    }
}