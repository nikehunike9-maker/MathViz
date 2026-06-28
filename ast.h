#ifndef AST_H
#define AST_H

#include "dsl.h"

// ── AST Node Types ──
typedef enum {
    // Statements
    NODE_PROGRAM,
    NODE_TITLE, NODE_RANGE,
    NODE_POINT_STMT, NODE_SEGMENT,
    NODE_LINE, NODE_AREA, NODE_CHECK,
    NODE_OUTPUT, NODE_MARK,
    NODE_CONGRUENT, NODE_SIMILAR,
    NODE_FUNCTION, NODE_VECTOR,
    NODE_MATRIX, NODE_TABLE,
    NODE_TEXT, NODE_KEYPOINT, NODE_IMPORT,
    NODE_SOLVE,
    // Control flow
    NODE_IF, NODE_ELSE, NODE_BLOCK,
    NODE_SUB, NODE_SUB_CALL,
    NODE_REPEAT, NODE_LOOP,
    // Expressions / values
    NODE_COLOR, NODE_NUMBER, NODE_IDENT, NODE_EXPR_STRING,
    NODE_PARAM
} NodeType;

// ── AST Node ──
typedef struct AstNode {
    NodeType type;
    struct AstNode **children;
    int numChildren;
    int capChildren;

    // Common string/number payloads
    char strVal[MAX_EXPR];   // labels, ident names, expression strings
    double numVal;           // numeric values, times
    double color[4];         // color values
} AstNode;

// ── Functions ──
AstNode* ast_new(NodeType type);
void ast_add_child(AstNode *parent, AstNode *child);
void ast_free(AstNode *node);
AstNode* ast_last_child(AstNode *parent);

// ── Parser: DSL text → AST ──
AstNode* ast_parse(const char *text);

// ── Codegen: AST → AnimationData ──
// Returns 1 on success, 0 on failure
int ast_codegen(AstNode *program, AnimationData *data);

#endif
