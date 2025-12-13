#include <stdio.h>
#include "ast.h"

void print_ast(Node *node, int depth) {
    for(int i = 0; i < depth; i++)
        printf("  ");
    if(!node) { 
        printf("NULL\n"); 
        return; 
    }
    
    printf("Node type: %d", node->node_type);
    switch(node->node_type) {
        case 0: printf(" (NUM) value: %d\n", node->int_val); break;
        case 1: printf(" (STR) value: %s\n", node->str_val); break;
        case 2: printf(" (ID) name: %s\n", node->str_val); break;
        case 3: printf(" (BINOP) op: %c\n", node->binop.op); 
                print_ast(node->binop.left, depth + 1);
                print_ast(node->binop.right, depth + 1);
                break;
        case 4: printf(" (DECL)\n"); 
                // ror DECL nodes, traverse list.items
                print_ast(node->list.items, depth + 1);
                // also traverse list.next for next statement
                if(node->list.next) {
                    print_ast(node->list.next, depth);
                }
                break;
        case 5: printf(" (ASSIGN)\n");
                // for ASSIGN nodes, traverse list.items
                print_ast(node->list.items, depth + 1);
                // also traverse list.next for next statement
                if(node->list.next) {
                    print_ast(node->list.next, depth);
                }
                break;
        case 6: printf(" (PRINT)\n");
                // for PRINT nodes, traverse print_stmt.parts as a linked list
                {
                    Node *part = node->print_stmt.parts;
                    while(part) {
                        print_ast(part, depth + 1);
                        // parts are linked via list.next
                        part = part->list.next;
                    }
                }
                // also traverse list.next for next statement
                if(node->list.next) {
                    print_ast(node->list.next, depth);
                }
                break;
        // fix attempt
        case 7: printf(" (PRINT_PART)\n");
                print_ast(node->list.items, depth + 1);
                if(node->list.next) {
                    print_ast(node->list.next, depth);
                }
                break;
        default: printf(" (UNKNOWN)\n");
    }
}