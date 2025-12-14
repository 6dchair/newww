%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "semantics.h"
#include "ast.h"
#include "assembly.h"
#include "machine_code.h"
#include "interpreter.h"

#define NODE_PRINT_PART 7
#define NODE_STR_ASSIGN 8 

// FIX 17: delimiters r necessaryy
int found_prog_end = 0;
int found_prog_start = 0;

// AST root
Node *ast_root = NULL;

// global semantic analyzer
Semantics sem_analyzer;

extern int yylex();
extern int yyparse();
extern FILE *yyin;
void yyerror(const char *s);
int yylex_destroy(void);

Node *create_num_node(int val);
Node *create_str_node(char *str);
Node *create_id_node(char *name);
Node *create_binop_node(int op, Node *left, Node *right);
Node *create_decl_node(Node *items);
Node *create_assign_node(Node *items);
Node *create_print_node(Node *parts);
Node *append_to_list(Node *list, Node *item);
Node *create_print_part_node(Node *content);
Node *create_str_assign_node(Node *id_node, Node *str_node);
void free_node(Node *node);

// debug function
void print_ast(Node *node, int depth);
%}

%union {
    int int_val;
    char *str_val;
    void *node_ptr;
}

%token PROG_START PROG_END
%token KW_INT KW_PRINT KW_CH 
%token NEWLINE_TOKEN ILLEGAL
%token <int_val> NUM
%token <str_val> ID STR
%token SEMICOLON // FIX 9: ; as terminator

%type <node_ptr> program lines line stmt decl print_stmt assign
%type <node_ptr> print_list print_item expr term factor

%nonassoc PRINT_EXPR

%%

// FIX 22 & 23: added optional \ns for balance
program: leading_newlines PROG_START lines PROG_END optional_newlines
    // FIX 17: delimiters r necessaryyy
    {
        ast_root = $3;
        found_prog_start = 1;
        found_prog_end = 1;
    }
    | leading_newlines PROG_START lines  // missing <<<
    {
        ast_root = $3;
        found_prog_start = 1; 
        found_prog_end = 0; // FIX 21: another >>> issue
    }
    | leading_newlines lines PROG_END  // no >>>
    {
        ast_root = $2;
        found_prog_end = 1;
        found_prog_start = 1; // FIX 21
    } ////////
    ;


// FIX 22: the error hiro pointed out: newline b4 >>>
leading_newlines
    : /* empty */
    | leading_newlines NEWLINE_TOKEN
    ;

// FIX 23: \n after >>> shoudl work still
optional_newlines: 
    /* empty */
    | optional_newlines NEWLINE_TOKEN
    ;

lines: line lines
    {
        $$ = append_to_list((Node*)$1, (Node*)$2);
    }
    | /* epsilon */
    {
        $$ = NULL;
    }
    ;

line: stmt NEWLINE_TOKEN
    {
        $$ = $1;
        sem_set_line(&sem_analyzer, sem_analyzer.current_line + 1);
    }
    | error NEWLINE_TOKEN
    {
        fprintf(stderr, "Line %d: Syntax error caused by any or one of the ff:\n\t"
        "(a) missing or extra ( or )\n\t"
        "(b) unknown operator: PMDAS only\n\t"
        "(c) keyword in the wrong place: e.g.: int 5 or ch \"Dazai Osamu\"\n\t"
        "(d) missing ':' after p in printing\n\t"
        "(e) invalid escape sequence: only \\n, \\t, \", &, \\\\\n\t"
        "(f) invalid variable name: must be in letter(letter + digit + _)* format\n\t"
        "(g) unsupported statement (declaration, assignment, & print only)\n\t"
        "(h) duplicated/incorrect delimiter (>>> for start; <<< for end)\n\t"
        "\t*** code must start w/ >>>\n\t\t*** code must end with >>>\n", 
        sem_analyzer.current_line); // FIX 10: missing ( or ) & other syntax errors
        sem_analyzer.error_count++; ///////
        $$ = NULL;
        sem_set_line(&sem_analyzer, sem_analyzer.current_line + 1);
        yyerrok;
    }
    | NEWLINE_TOKEN
    {
        $$ = NULL;
        sem_set_line(&sem_analyzer, sem_analyzer.current_line + 1);
    }
    ;

stmt: decl
    {
        $$ = $1;
        // sem_set_decl_line(&sem_analyzer, false); FIX 4: removed for fix 4 as it should be done in decl rule itslef
    }
    | print_stmt
    {
        $$ = $1;
    }
    | assign
    {
        $$ = $1;
    }
    ;

    /*| decl SEMICOLON // FIX 9: ; as terminator
    {
        fprintf(stderr, "Line %d: Invalid line terminator (no need for ';' to end a line)\n",
                sem_analyzer.current_line);
        sem_analyzer.error_count++;
        $$ = NULL;  // discard the declaration entirely
        // dont process the declaration: don't add to symbol table
    }
    | assign SEMICOLON // FIX 9: ; as terminator
    {
        fprintf(stderr, "Line %d: Invalid line terminator; no need for ';' to end a line\n",
                sem_analyzer.current_line);
        sem_analyzer.error_count++;
        $$ = NULL;  // discard the assignment entirely
    }
    | print_stmt SEMICOLON // FIX 9: ; as terminator
    {
        fprintf(stderr, "Line %d: Invalid line terminator; no need for ';' to end a line\n",
                sem_analyzer.current_line);
        sem_analyzer.error_count++;
        $$ = NULL;  // discard the print statement entirely
    }
    ;*/

decl: KW_INT ID
    {
        sem_set_decl_line(&sem_analyzer, true); // FIX 4: to flag redeclaration
        /*if(sem_add_symbol(&sem_analyzer, $2, false)) {
            Node *id_node = create_id_node($2);
            $$ = create_decl_node(id_node);
        } else {
            $$ = NULL;
        }*/ // FIX 8: not add to symbol table if vars are declared/assigned a value incorrectly
        sem_set_decl_line(&sem_analyzer, true);
        if(sem_add_symbol(&sem_analyzer, $2, false)) {
            Node *id_node = create_id_node($2);
            $$ = create_decl_node(id_node);
        } else {
            $$ = NULL;
        } ///////
    }
    |
    KW_INT ID SEMICOLON  // FIX 9: ; as terminator
    {
        fprintf(stderr, "Line %d: Invalid line terminator; no need for ';' to end a line\n",
                sem_analyzer.current_line);
        sem_analyzer.error_count++;
        $$ = NULL;
    }
    | KW_INT ID '=' expr
    {
        /*sem_set_decl_line(&sem_analyzer, true); // FIX 4: to flag redeclaration
        if(sem_add_symbol(&sem_analyzer, $2, false)) {
            if(!sem_check_division_by_zero((Node*)$4)) {
                fprintf(stderr, "Line %d: Division by zero in initialization\n", 
                        sem_analyzer.current_line);
                //sem_analyzer.error_count++;
                $$ = NULL;
            } else {
                //Node *id_node = create_id_node($2);
                //Node *assign_node = create_binop_node('=', id_node, (Node*)$4);
                //$$ = create_decl_node(assign_node);
                // only add to symbol table if validation passes
                if(sem_add_symbol(&sem_analyzer, $2, false)) {
                Node *id_node = create_id_node($2);
                Node *assign_node = create_binop_node('=', id_node, (Node*)$4);
                $$ = create_decl_node(assign_node);
                }
            }
        } else {
            $$ = NULL;
        }*/ // FIX 8: not add to symbol table if vars are declared/assigned a value incorrectly
        sem_set_decl_line(&sem_analyzer, true);
        // VALIDATE FIRST b4 adding to symbol table
        if(!sem_check_division_by_zero((Node*)$4)) {
            fprintf(stderr, "Line %d: Division by zero in initialization\n", 
                    sem_analyzer.current_line);
            //sem_analyzer.error_count++;
            $$ = NULL;
        } else {
            // only add to symbol table if validation passes
            if(sem_add_symbol(&sem_analyzer, $2, false)) {
                Node *id_node = create_id_node($2);
                Node *assign_node = create_binop_node('=', id_node, (Node*)$4);
                $$ = create_decl_node(assign_node);
            } else {
                $$ = NULL;
            }
        } //////
    }
    | KW_INT ID '=' expr SEMICOLON  // FIX 9: ; as terminator
    {
        fprintf(stderr, "Line %d: Invalid line terminator; no need for ';' to end a line\n",
                sem_analyzer.current_line);
        sem_analyzer.error_count++;
        $$ = NULL;
    }
    | KW_INT ID '=' expr ',' ID  // FIX 6: multiple vars in 1 declarayion
    {
        fprintf(stderr, "Line %d: Only one declaration per line allowed. Use separate lines.\n",
                sem_analyzer.current_line);
        //sem_analyzer.error_count++;
        $$ = NULL;
    }
    | KW_INT ID '=' STR  // catch: int y = "string"
    {
        /*if(sem_add_symbol(&sem_analyzer, $2, false)) {
            fprintf(stderr, "Line %d: Cannot assign string to integer variable '%s'\n",
                    sem_analyzer.current_line, $2);
            //sem_analyzer.error_count++;
            $$ = NULL;
        } else {
            $$ = NULL;
        }*/// FIX 8: not add to symbol table if vars are declared/assigned a value incorrectly
        fprintf(stderr, "Line %d: Cannot assign string to integer variable '%s'\n",
                sem_analyzer.current_line, $2);
        //sem_analyzer.error_count++;
        $$ = NULL;
    }
    | KW_INT ID ',' ID  // FIX 6: multiple vars in 1 declaration
    {
        fprintf(stderr, "Line %d: Only one declaration per line allowed. Use separate lines.\n",
                sem_analyzer.current_line);
        //sem_analyzer.error_count++;
        $$ = NULL;
    }
    | KW_CH ID
    {
        sem_set_decl_line(&sem_analyzer, true); // FIX 4: to flag redeclaration
        if(sem_add_symbol(&sem_analyzer, $2, true)) {
            Node *id_node = create_id_node($2);
            $$ = create_decl_node(id_node);
        } else {
            $$ = NULL;
        }
    }
    | KW_CH ID SEMICOLON  // FIX 9: ; as terminator
    {
        fprintf(stderr, "Line %d: Invalid line terminator; no need for ';' to end a line\n",
                sem_analyzer.current_line);
        sem_analyzer.error_count++;
        $$ = NULL;
    }
    | KW_CH ID '=' STR
    {
        sem_set_decl_line(&sem_analyzer, true); // FIX 4: to flag redeclaration
        if(sem_add_symbol(&sem_analyzer, $2, true)) {
            Node *id_node = create_id_node($2);
            Node *str_node = create_str_node($4);
            Node *str_assign = create_str_assign_node(id_node, str_node);
            $$ = create_decl_node(str_assign);
        } else {
            $$ = NULL;
        }
    }
    | KW_CH ID '=' STR SEMICOLON // FIX 9: ; as terminator
    {
        fprintf(stderr, "Line %d: Invalid line terminator; no need for ';' to end a line\n",
                sem_analyzer.current_line);
        sem_analyzer.error_count++;
        $$ = NULL;
    }
    // FIX 1: to flag ch x = expr as error
    | KW_CH ID '=' expr
    {
        /*if(sem_add_symbol(&sem_analyzer, $2, true)) {
            // check if the expression is a string
            //if($4->node_type != 1) 
            if(((Node*)$4)->node_type != 1) { // not a STR node
                fprintf(stderr, "Line %d: Cannot assign numeric expression to string variable '%s'\n",
                        sem_analyzer.current_line, $2);
                //sem_analyzer.error_count++;
                $$ = NULL;
            } else if(!sem_check_division_by_zero((Node*)$4)) {
                fprintf(stderr, "Line %d: Division by zero in initialization\n", 
                        sem_analyzer.current_line);
                //sem_analyzer.error_count++;
                $$ = NULL;
            } else {
                Node *id_node = create_id_node($2);
                Node *str_assign = create_str_assign_node(id_node, (Node*)$4);
                $$ = create_decl_node(str_assign);
            }
        } else {
            $$ = NULL;
        }*/ // FIX 8: not add to symbol table if vars are declared/assigned a value incorrectly
        sem_set_decl_line(&sem_analyzer, true);
        
        // check if the expression is a string FIRST
        if(((Node*)$4)->node_type != 1) { // not a STR node
            fprintf(stderr, "Line %d: Cannot assign numeric expression to string variable '%s'\n",
                    sem_analyzer.current_line, $2);
            //sem_analyzer.error_count++;
            $$ = NULL;  // don't add to symbol table
        } else if(!sem_check_division_by_zero((Node*)$4)) {
            fprintf(stderr, "Line %d: Division by zero in initialization\n", 
                    sem_analyzer.current_line);
            //sem_analyzer.error_count++;
            $$ = NULL;  // dont add to symbol table
        } else {
            // only add to symbol table if all validations pass
            if(sem_add_symbol(&sem_analyzer, $2, true)) {
                Node *id_node = create_id_node($2);
                Node *str_assign = create_str_assign_node(id_node, (Node*)$4);
                $$ = create_decl_node(str_assign);
            } else {
                $$ = NULL;
            }
        }
    }
    | KW_CH ID ',' ID  // FIX 6: multiple vars in 1 declaration
    {
        fprintf(stderr, "Line %d: Only one declaration per line allowed. Use separate lines.\n",
                sem_analyzer.current_line);
        //sem_analyzer.error_count++;
        $$ = NULL;
    }
    ;  

assign: ID '=' expr
    {
        if(sem_check_declared(&sem_analyzer, $1)) {
            if(sem_is_string_type(&sem_analyzer, $1)) {
                fprintf(stderr, "Line %d: Cannot assign integer to string variable '%s'\n",
                        sem_analyzer.current_line, $1);
                //sem_analyzer.error_count++;
                $$ = NULL;
            } else if(!sem_check_division_by_zero((Node*)$3)) {
                fprintf(stderr, "Line %d: Division by zero in assignment\n", 
                        sem_analyzer.current_line);
                //sem_analyzer.error_count++;
                $$ = NULL;
            } else {
                Node *id_node = create_id_node($1);
                Node *assign_node = create_binop_node('=', id_node, (Node*)$3);
                $$ = create_assign_node(assign_node);
            }
        } else {
            $$ = NULL;
        }
    }
    | ID '=' STR
    {
        if(sem_check_declared(&sem_analyzer, $1)) {
            if(!sem_is_string_type(&sem_analyzer, $1)) {
                fprintf(stderr, "Line %d: Cannot assign string to integer variable '%s'\n",
                        sem_analyzer.current_line, $1);
                //sem_analyzer.error_count++;
                $$ = NULL;
            } else {
                Node *id_node = create_id_node($1);
                Node *str_node = create_str_node($3);
                Node *str_assign = create_str_assign_node(id_node, str_node);
                $$ = create_assign_node(str_assign);
            }
        } else {
            $$ = NULL;
        }
    }
    | ID '=' expr ',' ID '=' expr  // FIX 7: multiple assignmenmts in one line
    {
        fprintf(stderr, "Line %d: Only one assignment per line allowed. Use separate lines.\n",
                sem_analyzer.current_line);
        //sem_analyzer.error_count++;
        $$ = NULL;
    }
    ;

print_stmt: KW_PRINT ':' print_list
    {
        $$ = create_print_node((Node*)$3);
    }
    ;

print_list: print_item
    {
        Node *wrapped = create_print_part_node($1);
        $$ = wrapped;
    }
    | print_item ',' print_list
    {
        Node *first_wrapped = create_print_part_node($1);
        $$ = append_to_list(first_wrapped, (Node*)$3);
    }
    ;

print_item: STR
    {
        $$ = create_str_node($1);
    }
    | expr %prec PRINT_EXPR
    {
        $$ = $1;
    }
    ;

expr: expr '+' term
    {
        $$ = create_binop_node('+', (Node*)$1, (Node*)$3);
    }
    | expr '-' term
    {
        $$ = create_binop_node('-', (Node*)$1, (Node*)$3);
    }
    | term
    {
        $$ = $1;
    }
    ;

term: term '*' factor
    {
        $$ = create_binop_node('*', (Node*)$1, (Node*)$3);
    }
    | term '/' factor
    {
        $$ = create_binop_node('/', (Node*)$1, (Node*)$3);
    }
    | factor
    {
        $$ = $1;
    }
    ;

factor: NUM
    {
        $$ = create_num_node($1);
    }
    | ID
    {
        if(sem_check_declared(&sem_analyzer, $1)) {
            $$ = create_id_node($1);
        } else {
            $$ = NULL;
        }
    }
    | '(' expr ')'
    {
        $$ = $2;
    } 
    | '-' factor
    {
        Node *neg_one = create_num_node(-1);
        $$ = create_binop_node('*', neg_one, (Node*)$2);
    }
    ;
%%

// FIX 18: no content should be after <<<
int check_content_after_end_delimiter(const char *filename) {
    FILE *f = fopen(filename, "r");
    if(!f)
        return 0;
    
    char line[256];
    //int found_end = 0; // might use thus in the future
    int reading_after_end = 0;
    int has_error = 0;
    
    while(fgets(line, sizeof(line), f)) {
        // keep og line (w/ newline) for exact checking
        char original_line[256];
        strcpy(original_line, line);
        
        // find where line ends (b4 newline)
        int line_len = strcspn(original_line, "\r\n");
        original_line[line_len] = '\0';  // terrminate at newline
        
        char *p = original_line;
        // skip leading whitespace for content checking
        while(*p && isspace((unsigned char)*p))
            p++;
        
        // skip empty lines & comments
        if(*p == '\0' || (p[0] == '/' && p[1] == '/'))
            continue;
        
        // check if this line contains <<<
        char *end_pos = strstr(original_line, "<<<");
        if(end_pos != NULL) {
            //found_end = 1;
            reading_after_end = 1;  // now we're after <<<
            
            // check for content on same line after <<<
            char *after_end = end_pos + 3; // skip "<<<"
            // check rest of the line
            while(*after_end) {
                if(!isspace((unsigned char)*after_end)) {
                    if(!has_error) {
                        fprintf(stderr, "Extra error: Anything after '<<<' delimiter is not allowed\n");
                        has_error = 1;
                    }
                    break;
                }
                after_end++;
            }
        } 
        // if we're reading after <<< & find non empty, non comment line
        else if(reading_after_end) {
            if(!has_error) {
                fprintf(stderr, "Extra error: Anything after '<<<' delimiter is not allowed\n");
                has_error = 1;
            }
            break;  // found content after <<<, no need to continue
        }
    }
    
    fclose(f);
    return has_error ? 1 : 0;
}

int main(int argc, char **argv) {
    if(argc < 2) {
        fprintf(stderr, "Usage: %s <input_file> [output_file]\n", argv[0]);
        return 1;
    }

    int error_count = 0;

    char *asm_filename = "MIPS64.s";
    char *machine_filename = "MACHINE_CODE.mc";
    
    if(argc >= 3) {
        asm_filename = argv[2];
        // create machine code filename from assembly filename
        char *dot = strrchr(asm_filename, '.');
        if(dot && strcmp(dot, ".s") == 0) {
            // replace .s with .mc
            strcpy(dot, ".mc");
            machine_filename = asm_filename;
            strcpy(dot, ".s"); // restore .s
        } else {
            // append .mc
            machine_filename = malloc(strlen(asm_filename) + 4);
            sprintf(machine_filename, "%s.mc", asm_filename);
        }
    }
    
    // initialize semantic analyzer
    sem_init(&sem_analyzer);
    sem_set_line(&sem_analyzer, 1);
    
    yyin = fopen(argv[1], "r");
    if(!yyin) {
        fprintf(stderr, "Error: Cannot open file %s\n", argv[1]);
        sem_cleanup(&sem_analyzer);
        return 1;
    }
    
    int parse_result = yyparse();
    // FIX 17
    if(!found_prog_start) {
        fprintf(stderr, "Delimiter error: Missing program start delimiter '>>>'\n");
        error_count++;
    } /////
    //error_count = sem_get_error_count(&sem_analyzer);
    error_count += sem_get_error_count(&sem_analyzer); // FIX 19: fix total error; missing <<< error is overwrittem, that's why
    

    // FIX 17: delimiters r necessaryyy
    if(!found_prog_end) {
        fprintf(stderr, "Delimiter error: Missing program end delimiter '<<<'\n");
        error_count++;
    }
    //////
    

    // FIX 18: Check for content AFTER <<< (LAST)
    int after_error = check_content_after_end_delimiter(argv[1]);
    
    // TOTAL errors
    int total_errors = error_count + after_error;
    /////


    if(parse_result == 0 && error_count == 0) {
        //printf("Compilation successful!\n");
        
        // debug: print AST structure
        //printf("\nAST Structure\n");
        //print_ast(ast_root, 0);
        //printf("\n\n");
        
        // open output file for assembly
        FILE *asm_file = fopen(asm_filename, "w");
        if(!asm_file) {
            fprintf(stderr, "Error: Cannot open assembly file %s\n", asm_filename);
            fclose(yyin);
            sem_cleanup(&sem_analyzer);
            free_node(ast_root);
            return 1;
        }
        
        // generate MIPS64 assembly
        GenerateAssemblyProgram(ast_root, asm_file);
        fclose(asm_file);
        
        //printf("MIPS64 assembly written to %s\n", asm_filename);
        
        // now convert assembly to machine code
        //printf("\nConverting assembly to machine code\n");
        if(MachineFromAssembly(asm_filename, machine_filename)) {
            //printf("Machine code written to %s\n", machine_filename);
            
            // display machine code
            //printf("\nMachine Code\n");
            //FILE *mc_file = fopen(machine_filename, "r");
            //if(mc_file) {
            //    char line[256];
            //    while(fgets(line, sizeof(line), mc_file)) {
            //        printf("%s", line);
            //    }
            //    fclose(mc_file);
           // }
        } else {
            //fprintf(stderr, "Error converting assembly to machine code\n");
        }
        
        // FIX 18
        //int after_errors = check_content_after_end_delimiter(argv[1]);
        //total_errors += after_errors;
        ////

        // now interpret the program and display output
        //printf("\nProgram Output\n");
        char *output = interpret_program(ast_root);
        if(output && strlen(output) > 0) {
            printf("%s\n", output);
            
        } else {
            printf("(No output produced)\n");
        }
        free(output);
        
    } else {
        printf("\nCompilation failed with %d error(s)\n", total_errors);
    }
    
    fclose(yyin);
    yylex_destroy();
    sem_cleanup(&sem_analyzer);
    free_node(ast_root);
    
    return (parse_result != 0 || error_count > 0) ? 1 : 0;
}
void yyerror(const char *s) {
    //fprintf(stderr, "Syntax error at line %d: %s\n", sem_analyzer.current_line, s);
    //sem_analyzer.error_count++;
}

// AST Creation Functions
Node *create_num_node(int val) {
    Node *node = malloc(sizeof(Node));
    node->node_type = 0;
    node->int_val = val;
    return node;
}

Node *create_str_node(char *str) {
    Node *node = malloc(sizeof(Node));
    node->node_type = 1;
    node->str_val = strdup(str);
    return node;
}

Node *create_id_node(char *name) {
    Node *node = malloc(sizeof(Node));
    node->node_type = 2;
    node->str_val = strdup(name);
    return node;
}

Node *create_binop_node(int op, Node *left, Node *right) {
    //printf("DEBUG create_binop_node: op='%c', left_type=%d, right_type=%d\n", 
    //       op, left ? left->node_type : -1, right ? right->node_type : -1);
    //Node *node = malloc(sizeof(Node));
    Node *node = calloc(1, sizeof(Node));
    node->node_type = 3;
    node->binop.op = op;
    node->binop.left = left;
    node->binop.right = right;
    return node;
}

Node *create_decl_node(Node *items) {
    //Node *node = malloc(sizeof(Node));
    Node *node = calloc(1, sizeof(Node));
    node->node_type = 4;
    node->list.items = items;
    node->list.next = NULL;
    return node;
}

Node *create_assign_node(Node *items) {
    //Node *node = malloc(sizeof(Node));
    Node *node = calloc(1, sizeof(Node));
    node->node_type = 5;
    node->list.items = items;
    node->list.next = NULL;
    return node;
}

Node *create_print_node(Node *parts) {
    //Node *node = malloc(sizeof(Node));
    Node *node = calloc(1, sizeof(Node));
    node->node_type = 6;
    node->print_stmt.parts = parts;
    return node;
}

/// FIX ATTEMPT
Node *create_print_part_node(Node *content) {
    Node *node = calloc(1, sizeof(Node));
    //Node *node = malloc(sizeof(Node));
    node->node_type = NODE_PRINT_PART;
    node->list.items = content;  // the actual content (STR, ID, BINOP, etc)
    node->list.next = NULL;      // next print part
    return node;
}
//////

//////
// add after other node creation functions
Node *create_str_assign_node(Node *id_node, Node *str_node) {
    Node *node = calloc(1, sizeof(Node));
    node->node_type = NODE_STR_ASSIGN;
    node->str_assign.id = id_node;
    node->str_assign.str = str_node;
    return node;
}
////////

Node *append_to_list(Node *first, Node *rest) {
    if(!first) 
        return rest;
    if(!rest)
        return first;
    
    // both should be NODE_PRINT_PART nodes
    Node *current = first;
    while(current->list.next) {
        current = current->list.next;
    }
    current->list.next = rest;
    return first;
}
////

void free_node(Node *node) {
    if(!node)
        return;
    
    switch(node->node_type) {
        case 1: // STR
        case 2: // ID
            free(node->str_val);
            break;
        case 3: // BINOP
            free_node(node->binop.left);
            free_node(node->binop.right);
            break;
        case 4: // DECL
        case 5: // ASSIGN
        case 6: // PRINT
            free_node(node->list.items);
            free_node(node->list.next);
            break;
    }
    free(node);
}
