#include "ast.h"
#include "math_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <math.h>

#define LINE_BUF 4096

// ── AST node management ──
AstNode* ast_new(NodeType type) {
    AstNode *n = calloc(1, sizeof(AstNode));
    n->type = type;
    n->color[0] = 0.4; n->color[1] = 0.4; n->color[2] = 0.4; n->color[3] = 1;
    return n;
}

void ast_add_child(AstNode *parent, AstNode *child) {
    if (!parent || !child) return;
    if (parent->numChildren >= parent->capChildren) {
        int newCap = parent->capChildren ? parent->capChildren * 2 : 8;
        parent->children = realloc(parent->children, newCap * sizeof(AstNode*));
        parent->capChildren = newCap;
    }
    parent->children[parent->numChildren++] = child;
}

void ast_free(AstNode *node) {
    if (!node) return;
    for (int i = 0; i < node->numChildren; i++)
        ast_free(node->children[i]);
    free(node->children);
    free(node);
}

AstNode* ast_last_child(AstNode *parent) {
    if (!parent || parent->numChildren == 0) return NULL;
    return parent->children[parent->numChildren - 1];
}

// ── Tokenizer (same logic as original parser) ──
typedef enum { TK_NUM, TK_KW, TK_PUNCT, TK_COLOR, TK_ID } TokenType;

static int kw_of(const char* s) {
    struct { const char* n; int kw; } map[] = {
        {"TITLE",1},{"RANGE",2},{"POINT",3},{"START",4},
        {"MOVE",5},{"VELOCITY",6},{"SPEED",7},{"COLOR",8},
        {"LINE",9},{"CHECK",10},{"KEYPOINT",11},{"OUTPUT",12},
        {"AREA",13},{"MARK",14},{"CONGRUENT",15},{"SIMILAR",16},
        {"FUNCTION",17},{"VECTOR",18},{"MATRIX",19},{"TABLE",20},
        {"TEXT",21},{"SOLVE",22},{"REPEAT",23},{"LOOP",24},
        {"END_REPEAT",25},{"END_LOOP",26},{"LABEL",27},
        {"MESSAGE",28},{"RECORD",29},{"NUMBER",30},{"IMPORT",31},
        {"REL",32},{"FOR",33},{"AS",34},{"INF",35},
        {"IF",36},{"ELSE",37},{"END_IF",38},{"SUB",39},
        {"END_SUB",40},{"CONDITION",41},{"CURVE",42},{"PARAM",43},
        {"FROM",44},{"TO",45},{"STEP",46},
        {NULL,0}
    };
    char u[64]; int i=0; while(s[i]&&i<63){u[i]=toupper((unsigned char)s[i]);i++;}u[i]=0;
    for(int j=0;map[j].n;j++) if(strcmp(u,map[j].n)==0) return map[j].kw;
    return 0;
}

static int tokenize(const char* rest, char tok[][64], TokenType* typ, int max) {
    int n=0; const char* p=rest;
    while(*p&&n<max){
        while(*p&&isspace((unsigned char)*p))p++;
        if(!*p)break;
        if(*p==':'){p++;continue;}
        if(*p==';'||*p==','||*p=='('||*p==')'){
            tok[n][0]=*p;tok[n][1]=0;p++;typ[n]=TK_PUNCT;n++;continue;
        }
        if(*p=='#'){
            if(*(p+1)&&isxdigit((unsigned char)*(p+1))){
                int i=0;tok[n][i++]='#';p++;
                while(*p&&isxdigit((unsigned char)*p)&&i<63)tok[n][i++]=*p,p++;
                tok[n][i]=0;typ[n]=TK_COLOR;n++;continue;
            } else break;
        }
        if(isdigit((unsigned char)*p)||(*p=='-'&&isdigit((unsigned char)*(p+1)))||*p=='.'){
            int i=0;if(*p=='-'){tok[n][i++]='-';p++;}
            while(*p&&(isdigit((unsigned char)*p)||*p=='.'||*p=='e'||*p=='E')){
                if((*p=='-'||*p=='+')&&i>0&&(tok[n][i-1]=='e'||tok[n][i-1]=='E'))tok[n][i++]=*p,p++;
                else if(*p=='e'||*p=='E'||*p=='.'||isdigit((unsigned char)*p))tok[n][i++]=*p,p++;
                else break;
            }
            tok[n][i]=0;typ[n]=TK_NUM;n++;continue;
        }
        if(isalpha((unsigned char)*p)||*p=='_'){
            int i=0;while(*p&&(isalnum((unsigned char)*p)||*p=='_'||(unsigned char)*p>127))tok[n][i++]=*p,p++;
            tok[n][i]=0;
            if(*p=='('){int d=1;tok[n][i++]='(';p++;while(*p&&d>0&&i<63){if(*p=='(')d++;if(*p==')')d--;if(d>=0)tok[n][i++]=*p;p++;}tok[n][i]=0;}
            typ[n]=kw_of(tok[n])?TK_KW:TK_ID;n++;continue;
        }
        p++;
    }
    return n;
}

static double numval(const char* s) {
    double v = strtod(s, NULL);
    if (v==0&&s[0]!='0'&&s[0]!='.') return eval_expr(s);
    return v;
}

static void parse_color_ast(const char* s, double c[4]) {
    c[0]=0.4;c[1]=0.4;c[2]=0.4;c[3]=1;
    if(!s||!*s)return;
    if(s[0]=='#'){
        unsigned r=0,g=0,b=0;
        size_t sl=strlen(s);
        if(sl>=7&&sscanf(s+1,"%2x%2x%2x",&r,&g,&b)==3){c[0]=r/255.0;c[1]=g/255.0;c[2]=b/255.0;}
        else if(sl==4){char bf[8];snprintf(bf,8,"#%c%c%c%c%c%c",s[1],s[1],s[2],s[2],s[3],s[3]);sscanf(bf+1,"%2x%2x%2x",&r,&g,&b);c[0]=r/255.0;c[1]=g/255.0;c[2]=b/255.0;}
    } else if(strncmp(s,"rgba",4)==0){
        double r,g,b,a;
        if(sscanf(s,"rgba(%lf,%lf,%lf,%lf)",&r,&g,&b,&a)==4){c[0]=r/255.0;c[1]=g/255.0;c[2]=b/255.0;c[3]=a;}
    }
}

// ── Color palette ──
static const double palette[8][4]={{0.91,0.30,0.24,1},{0.20,0.60,0.86,1},{0.18,0.80,0.44,1},{0.95,0.61,0.07,1},{0.61,0.35,0.71,1},{0.10,0.74,0.61,1},{0.90,0.37,0.00,1},{0.20,0.20,0.33,1}};
static int color_idx=0;
static void next_color_ast(double c[4]){int ci=color_idx++%8;for(int i=0;i<4;i++)c[i]=palette[ci][i];}

// ── Parser: DSL text → AST ──
AstNode* ast_parse(const char *text) {
    color_idx = 0;
    AstNode *program = ast_new(NODE_PROGRAM);

    // Raw lines
    char (*raw)[LINE_BUF] = calloc(512, LINE_BUF);
    int nraw = 0;
    if (!raw) { ast_free(program); return NULL; }
    const char *p = text;
    while (*p && nraw < 512) {
        int i = 0;
        while (*p && *p != '\n' && i < LINE_BUF-1) raw[nraw][i++] = *p++;
        raw[nraw][i] = 0; nraw++;
        if (*p == '\n') p++;
    }

    // Expand REPEAT/LOOP
    char (*exp)[LINE_BUF] = calloc(2048, LINE_BUF);
    int nexp = 0;
    for (int i = 0; i < nraw; i++) {
        char *ln = raw[i];
        char *s = ln; while (*s && isspace((unsigned char)*s)) s++;
        if (!*s || *s == '#') continue;
        // Check for REPEAT/LOOP keyword
        char kw[64]; int kl = 0;
        while (s[kl] && !isspace((unsigned char)s[kl]) && s[kl] != ':' && kl < 63) { kw[kl] = toupper((unsigned char)s[kl]); kl++; } kw[kl] = 0;
        if (strcmp(kw, "REPEAT") == 0 || strcmp(kw, "LOOP") == 0) {
            // Check for LOOP: FOR variant
            const char *rp2 = s + kl;
            while (*rp2 && *rp2 != ':') rp2++; if (*rp2 == ':') rp2++;
            while (*rp2 && isspace((unsigned char)*rp2)) rp2++;
            if (strcmp(kw, "LOOP") == 0 && strncasecmp(rp2, "FOR", 3) == 0) {
                // LOOP: FOR var FROM x TO y [STEP z]
                rp2 += 3;
                while (*rp2 && isspace((unsigned char)*rp2)) rp2++;
                char var[64] = ""; double from = 0, to = 1, step = 1;
                int vi = 0; while (*rp2 && !isspace((unsigned char)*rp2) && *rp2 != ':' && vi < 63) { var[vi++] = *rp2; rp2++; } var[vi] = 0;
                rp2 = strstr(rp2, "FROM"); if (rp2) { rp2 += 4; while (*rp2 && isspace((unsigned char)*rp2)) rp2++; from = atof(rp2); }
                rp2 = strstr(rp2, "TO"); if (rp2) { rp2 += 2; while (*rp2 && isspace((unsigned char)*rp2)) rp2++; to = atof(rp2); }
                rp2 = strstr(rp2, "STEP"); if (rp2) { rp2 += 4; while (*rp2 && isspace((unsigned char)*rp2)) rp2++; step = atof(rp2); }
                char (*body)[LINE_BUF] = calloc(512, LINE_BUF); int nb = 0; i++;
                while (i < nraw && nb < 512) {
                    char *bl = raw[i];
                    char *bs2 = bl; while (*bs2 && isspace((unsigned char)*bs2)) bs2++;
                    if (strncasecmp(bs2, "END_LOOP", 8) == 0) { i++; break; }
                    strcpy(body[nb++], bl); i++;
                } i--;
                for (double v = from; (step > 0 ? v <= to : v >= to); v += step) {
                    set_module_var(var, v);
                    for (int b = 0; b < nb; b++) strcpy(exp[nexp++], body[b]);
                }
                free(body);
            } else {
                int inf = (strncasecmp(rp2, "INF", 3) == 0);
                int cnt = inf ? 0 : atoi(rp2);
                char (*body)[LINE_BUF] = calloc(512, LINE_BUF); int nb = 0; i++;
                while (i < nraw && nb < 512) {
                    char *bl = raw[i];
                    char *bs2 = bl; while (*bs2 && isspace((unsigned char)*bs2)) bs2++;
                    if (strncasecmp(bs2, "END_REPEAT", 10) == 0 || strncasecmp(bs2, "END_LOOP", 8) == 0) { i++; break; }
                    strcpy(body[nb++], bl); i++;
                } i--;
                if (inf) {
                    strcpy(exp[nexp++], "__INF_BEGIN__");
                    for (int b = 0; b < nb; b++) strcpy(exp[nexp++], body[b]);
                    strcpy(exp[nexp++], "__INF_END__");
                } else {
                    for (int r = 0; r < cnt; r++)
                        for (int b = 0; b < nb; b++) strcpy(exp[nexp++], body[b]);
                }
                free(body);
            }
        } else if (strcmp(kw, "IF") == 0) {
            // IF: condition — expand only matching branch
            const char *rp2 = s + kl;
            while (*rp2 && *rp2 != ':') rp2++; if (*rp2 == ':') rp2++;
            while (*rp2 && isspace((unsigned char)*rp2)) rp2++;
            // Evaluate condition
            double condVal = eval_expr(rp2);
            int trueBranch = (condVal != 0);
            // Read body lines, pick correct branch
            char thenBody[512][LINE_BUF]; int nThen = 0;
            char elseBody[512][LINE_BUF]; int nElse = 0;
            int inElse = 0; int depth = 1; i++;
            while (i < nraw && depth > 0) {
                char *bl = raw[i];
                char *bs2 = bl; while (*bs2 && isspace((unsigned char)*bs2)) bs2++;
                char lkw[64]; int lkl2 = 0;
                while (bs2[lkl2] && !isspace((unsigned char)bs2[lkl2]) && bs2[lkl2] != ':' && lkl2 < 63) { lkw[lkl2] = toupper((unsigned char)bs2[lkl2]); lkl2++; } lkw[lkl2] = 0;
                if (strcmp(lkw, "IF") == 0) depth++;
                if (strcmp(lkw, "END_IF") == 0) { depth--; if (depth <= 0) break; }
                if (depth == 1 && strcmp(lkw, "ELSE") == 0) { inElse = 1; i++; continue; }
                if (inElse) { if (nElse < 512) strcpy(elseBody[nElse++], bl); }
                else { if (nThen < 512) strcpy(thenBody[nThen++], bl); }
                i++;
            }
            char (*chosen)[LINE_BUF] = trueBranch ? thenBody : (nElse > 0 ? elseBody : thenBody);
            int nChosen = trueBranch ? nThen : (nElse > 0 ? nElse : nThen);
            for (int b = 0; b < nChosen; b++) strcpy(exp[nexp++], chosen[b]);
        } else if (strcmp(kw, "ELSE") == 0 || strcmp(kw, "END_IF") == 0) {
            // consumed by IF handler
        } else if (strcmp(kw, "SUB") == 0) {
            // Store SUB body for later CALL expansion
            // For now, skip body
            int depth = 1; i++;
            while (i < nraw && depth > 0) {
                char *bl = raw[i];
                char *bs2 = bl; while (*bs2 && isspace((unsigned char)*bs2)) bs2++;
                char lkw[64]; int lkl2 = 0;
                while (bs2[lkl2] && !isspace((unsigned char)bs2[lkl2]) && bs2[lkl2] != ':' && lkl2 < 63) { lkw[lkl2] = toupper((unsigned char)bs2[lkl2]); lkl2++; } lkw[lkl2] = 0;
                if (strcmp(lkw, "SUB") == 0) depth++;
                if (strcmp(lkw, "END_SUB") == 0) { depth--; if (depth <= 0) break; }
                i++;
            }
        } else if (strcmp(kw, "END_SUB") == 0 || strcmp(kw, "CALL") == 0) {
            // skip
        } else if (strcmp(kw, "CONDITION") == 0) {
            strcpy(exp[nexp++], s);
        } else if (strcmp(kw, "CURVE") == 0) {
            strcpy(exp[nexp++], s);
        } else if (strcmp(kw, "PARAM") == 0) {
            strcpy(exp[nexp++], s);
        } else {
            strcpy(exp[nexp++], s);
        }
    }

    // Parse expanded lines into AST
    for (int si = 0; si < nexp; si++) {
        char *line = exp[si];
        char *s = line; while (*s && isspace((unsigned char)*s)) s++;
        int colon = -1;
        for (int ci = 0; s[ci]; ci++) { if (s[ci] == ':') { colon = ci; break; } }
        if (colon < 0) continue;

        char kw[64]; int kl = 0;
        while (kl < colon && kl < 63) { kw[kl] = toupper((unsigned char)s[kl]); kl++; } kw[kl] = 0;
        const char *rest = s + colon + 1;
        while (*rest && isspace((unsigned char)*rest)) rest++;

        char tok[64][64]; TokenType typ[64];
        int nt = tokenize(rest, tok, typ, 64);

        if (strcmp(kw, "TITLE") == 0) {
            AstNode *n = ast_new(NODE_TITLE);
            strncpy(n->strVal, rest, MAX_EXPR-1);
            ast_add_child(program, n);
        } else if (strcmp(kw, "RANGE") == 0 && nt >= 4) {
            AstNode *n = ast_new(NODE_RANGE);
            n->children = calloc(4, sizeof(AstNode*));
            n->numChildren = 4; n->capChildren = 4;
            n->children[0] = ast_new(NODE_NUMBER); n->children[0]->numVal = atof(tok[0]);
            n->children[1] = ast_new(NODE_NUMBER); n->children[1]->numVal = atof(tok[1]);
            n->children[2] = ast_new(NODE_NUMBER); n->children[2]->numVal = atof(tok[2]);
            n->children[3] = ast_new(NODE_NUMBER); n->children[3]->numVal = atof(tok[3]);
            ast_add_child(program, n);
        } else if (strcmp(kw, "POINT") == 0) {
            AstNode *pt = ast_new(NODE_POINT_STMT);
            char label[64] = ""; int labelSet = 0;
            double sx = 0, sy = 0;
            double ptColor[4]; next_color_ast(ptColor);
            for (int ti = 0; ti < nt; ti++) {
                if (typ[ti] == TK_KW && strcasecmp(tok[ti], "START") == 0) {
                    if (ti+2 < nt && typ[ti+1] == TK_NUM && typ[ti+2] == TK_NUM) {
                        sx = atof(tok[ti+1]); sy = atof(tok[ti+2]); ti += 2;
                    }
                } else if (typ[ti] == TK_KW && strcasecmp(tok[ti], "COLOR") == 0 && ti+1 < nt) {
                    parse_color_ast(tok[ti+1], ptColor); ti++;
                } else if (typ[ti] == TK_KW && strcasecmp(tok[ti], "MOVE") == 0) {
                    if (ti+2 < nt && typ[ti+1] == TK_NUM && typ[ti+2] == TK_NUM) {
                        double spd = 1;
                        for (int sj = ti+3; sj < nt-1; sj++)
                            if (typ[sj] == TK_KW && strcasecmp(tok[sj], "SPEED") == 0 && typ[sj+1] == TK_NUM) { spd = atof(tok[sj+1]); break; }
                        AstNode *seg = ast_new(NODE_SEGMENT);
                        seg->numVal = spd;
                        seg->children = calloc(2, sizeof(AstNode*));
                        seg->children[0] = ast_new(NODE_NUMBER); seg->children[0]->numVal = atof(tok[ti+1]);
                        seg->children[1] = ast_new(NODE_NUMBER); seg->children[1]->numVal = atof(tok[ti+2]);
                        seg->numChildren = 2; seg->capChildren = 2;
                        strcpy(seg->strVal, "MOVE");
                        ast_add_child(pt, seg);
                    }
                } else if (typ[ti] == TK_KW && strcasecmp(tok[ti], "VELOCITY") == 0 && ti+2 < nt && typ[ti+1] == TK_NUM && typ[ti+2] == TK_NUM) {
                    AstNode *seg = ast_new(NODE_SEGMENT);
                    seg->numVal = hypot(atof(tok[ti+1]), atof(tok[ti+2]));
                    seg->children = calloc(2, sizeof(AstNode*));
                    seg->children[0] = ast_new(NODE_NUMBER); seg->children[0]->numVal = atof(tok[ti+1]);
                    seg->children[1] = ast_new(NODE_NUMBER); seg->children[1]->numVal = atof(tok[ti+2]);
                    seg->numChildren = 2; seg->capChildren = 2;
                    strcpy(seg->strVal, "VELOCITY");
                    ast_add_child(pt, seg);
                    ti += 2;
                } else if (typ[ti] == TK_ID && !labelSet) {
                    strncpy(label, tok[ti], 63); labelSet = 1;
                }
            }
            snprintf(pt->strVal, MAX_EXPR-1, "%s", label[0] ? label : "P");
            pt->numVal = 1; // speed multiplier placeholder
            pt->children = realloc(pt->children, (pt->numChildren + 3) * sizeof(AstNode*));
            // Add start coords and color as children at the end
            AstNode *sxNode = ast_new(NODE_NUMBER); sxNode->numVal = sx;
            AstNode *syNode = ast_new(NODE_NUMBER); syNode->numVal = sy;
            pt->children[pt->numChildren++] = sxNode;
            pt->children[pt->numChildren++] = syNode;
            AstNode *colNode = ast_new(NODE_COLOR);
            for (int ci = 0; ci < 4; ci++) colNode->color[ci] = ptColor[ci];
            pt->children[pt->numChildren++] = colNode;
            pt->capChildren = pt->numChildren;
            ast_add_child(program, pt);
        } else if (strcmp(kw, "LINE") == 0) {
            AstNode *n = ast_new(NODE_LINE);
            char label[64] = ""; double lc[4]; parse_color_ast("#666", lc);
            for (int ti = 0; ti < nt; ti++) {
                if (typ[ti] == TK_KW && strcasecmp(tok[ti], "LABEL") == 0 && ti+1 < nt) { strncpy(label, tok[ti+1], 63); ti++; }
                else if (typ[ti] == TK_KW && strcasecmp(tok[ti], "COLOR") == 0 && ti+1 < nt) { parse_color_ast(tok[ti+1], lc); ti++; }
            }
            strncpy(n->strVal, label, MAX_EXPR-1);
            for (int i = 0; i < 4; i++) n->color[i] = lc[i];
            // Store raw tokens for codegen to resolve
            for (int ti = 0; ti < nt && ti < 8; ti++) {
                if (typ[ti] == TK_KW) continue; // skip keywords
                if (strcasecmp(tok[ti], "LABEL")==0 || strcasecmp(tok[ti], "COLOR")==0) { ti++; continue; }
                AstNode *child = ast_new(NODE_IDENT);
                strncpy(child->strVal, tok[ti], MAX_EXPR-1);
                if (typ[ti] == TK_NUM) child->numVal = atof(tok[ti]);
                ast_add_child(n, child);
            }
            ast_add_child(program, n);
        } else if (strcmp(kw, "CHECK") == 0) {
            AstNode *n = ast_new(NODE_CHECK);
            const char *msg = strstr(rest, "MESSAGE");
            const char *rec = strstr(rest, "RECORD");
            char cond[256] = ""; char msgBuf[256] = "点击继续"; char recBuf[8][64]; int nrec = 0;
            const char *condEnd = rest + strlen(rest);
            if (msg) { condEnd = msg; while (condEnd > rest && isspace((unsigned char)condEnd[-1])) condEnd--; }
            if (rec && rec < condEnd) condEnd = rec;
            int cl = (int)(condEnd - rest); if (cl > 255) cl = 255;
            strncpy(cond, rest, cl); cond[cl] = 0;
            if (msg) { const char *ms = msg + 7; while (*ms && isspace((unsigned char)*ms)) ms++; const char *me = ms + strlen(ms); while (me > ms && isspace((unsigned char)me[-1])) me--; int ml = (int)(me - ms); if (ml > 255) ml = 255; strncpy(msgBuf, ms, ml); msgBuf[ml] = 0; }
            if (rec) { const char *rs = rec + 6; while (*rs && isspace((unsigned char)*rs)) rs++; char rb[256]; int ri = 0; while (*rs && *rs != 'M' && ri < 255) rb[ri++] = *rs++, rs--; rb[ri] = 0; char *tk = strtok(rb, ","); while (tk && nrec < 8) { while (*tk && isspace((unsigned char)*tk)) tk++; strncpy(recBuf[nrec++], tk, 63); tk = strtok(NULL, ","); } }
            snprintf(n->strVal, MAX_EXPR-1, "%s", cond);
            n->numVal = nrec;
            n->children = calloc(nrec + 1, sizeof(AstNode*));
            for (int i = 0; i < nrec; i++) { n->children[i] = ast_new(NODE_IDENT); strncpy(n->children[i]->strVal, recBuf[i], MAX_EXPR-1); }
            n->children[nrec] = ast_new(NODE_EXPR_STRING); strncpy(n->children[nrec]->strVal, msgBuf, MAX_EXPR-1);
            n->numChildren = nrec + 1; n->capChildren = nrec + 1;
            ast_add_child(program, n);
        } else if (strcmp(kw, "OUTPUT") == 0) {
            AstNode *n = ast_new(NODE_OUTPUT);
            int typeLen = 0; const char *typeStart = NULL;
            if (strncasecmp(rest, "length", 6) == 0) { typeLen = 6; typeStart = "length"; }
            else if (strncasecmp(rest, "angle", 5) == 0) { typeLen = 5; typeStart = "angle"; }
            else if (strncasecmp(rest, "area", 4) == 0) { typeLen = 4; typeStart = "area"; }
            else if (strncasecmp(rest, "perimeter", 9) == 0) { typeLen = 9; typeStart = "perimeter"; }
            else if (strncasecmp(rest, "judge", 5) == 0) { typeLen = 5; typeStart = "judge"; }
            if (!typeStart) continue;
            const char *cs = rest + typeLen; while (*cs && isspace((unsigned char)*cs)) cs++;
            char label[64] = ""; double oc[4]; parse_color_ast("#333", oc);
            char judgeType[64] = "";
            char raw[8][64]; int nraw = 0;
            int nto = tokenize(cs, tok, typ, 64);
            for (int ti = 0; ti < nto; ti++) {
                if (typ[ti] == TK_KW && strcasecmp(tok[ti], "LABEL") == 0 && ti+1 < nto) { strncpy(label, tok[ti+1], 63); ti++; }
                else if (typ[ti] == TK_KW && strcasecmp(tok[ti], "COLOR") == 0 && ti+1 < nto) { parse_color_ast(tok[ti+1], oc); ti++; }
                else if (nraw < 8) strncpy(raw[nraw++], tok[ti], 63);
            }
            if (strcmp(typeStart, "judge") == 0 && nraw > 0) {
                strncpy(judgeType, raw[0], 63);
                for (int ri = 1; ri < nraw; ri++) strncpy(raw[ri-1], raw[ri], 63);
                nraw--;
            }
            snprintf(n->strVal, MAX_EXPR-1, "%s", label[0] ? label : typeStart);
            strncpy(n->strVal + MAX_EXPR/2, typeStart, MAX_EXPR/2 - 1);
            strncpy(n->strVal + MAX_EXPR/4*3, judgeType, MAX_EXPR/4 - 1);
            for (int i = 0; i < 4; i++) n->color[i] = oc[i];
            for (int i = 0; i < nraw && i < 8; i++) {
                AstNode *child = ast_new(NODE_IDENT);
                strncpy(child->strVal, raw[i], MAX_EXPR-1);
                if (atof(raw[i]) != 0 || raw[i][0] == '0' || raw[i][0] == '.') child->numVal = atof(raw[i]);
                ast_add_child(n, child);
            }
            ast_add_child(program, n);
        } else if (strcmp(kw, "AREA") == 0) {
            AstNode *n = ast_new(NODE_AREA);
            const char *shapes[] = {"triangle","rect","circle","square","trapezoid","parallelogram",NULL};
            int sh = -1; const char *sp = rest;
            for (int si2 = 0; shapes[si2]; si2++) { int l = strlen(shapes[si2]); if (strncasecmp(rest, shapes[si2], l) == 0 && isspace((unsigned char)rest[l])) { sh = si2; sp = rest + l; break; } }
            if (sh < 0) continue;
            while (*sp && isspace((unsigned char)*sp)) sp++;
            int nat = tokenize(sp, tok, typ, 64);
            char label[64] = "S"; double ac[4]; parse_color_ast("rgba(100,149,237,0.25)", ac);
            char raw[8][64]; int nraw = 0;
            for (int ti = 0; ti < nat; ti++) {
                if (typ[ti] == TK_KW && strcasecmp(tok[ti], "LABEL") == 0 && ti+1 < nat) { strncpy(label, tok[ti+1], 63); ti++; }
                else if (typ[ti] == TK_KW && strcasecmp(tok[ti], "COLOR") == 0 && ti+1 < nat) { parse_color_ast(tok[ti+1], ac); ti++; }
                else if (nraw < 8) strncpy(raw[nraw++], tok[ti], 63);
            }
            strncpy(n->strVal, shapes[sh], MAX_EXPR-1);
            snprintf(n->strVal + MAX_EXPR/2, MAX_EXPR/2 - 1, "%s", label);
            for (int i = 0; i < 4; i++) n->color[i] = ac[i];
            for (int i = 0; i < nraw && i < 8; i++) {
                AstNode *child = ast_new(NODE_IDENT);
                strncpy(child->strVal, raw[i], MAX_EXPR-1);
                ast_add_child(n, child);
            }
            ast_add_child(program, n);
        } else if (strcmp(kw, "MARK") == 0) {
            AstNode *n = ast_new(NODE_MARK);
            int mt = -1;
            if (strncasecmp(rest, "angle", 5) == 0 && isspace((unsigned char)rest[5])) mt = 0;
            else if (strncasecmp(rest, "right-angle", 11) == 0 && isspace((unsigned char)rest[11])) mt = 1;
            else if (strncasecmp(rest, "segment", 7) == 0 && isspace((unsigned char)rest[7])) mt = 2;
            else if (strncasecmp(rest, "parallel", 8) == 0 && isspace((unsigned char)rest[8])) mt = 3;
            if (mt < 0) continue;
            const char *sp2 = rest + (mt == 1 ? 11 : mt == 0 ? 5 : mt == 2 ? 7 : 8);
            while (*sp2 && isspace((unsigned char)*sp2)) sp2++;
            int nmt = tokenize(sp2, tok, typ, 64);
            int num = 0; double mc2[4]; parse_color_ast("#555", mc2);
            char raw2[8][64]; int nraw2 = 0;
            for (int ti = 0; ti < nmt; ti++) {
                if (typ[ti] == TK_KW && strcasecmp(tok[ti], "COLOR") == 0 && ti+1 < nmt) { parse_color_ast(tok[ti+1], mc2); ti++; }
                else if (typ[ti] == TK_KW && strcasecmp(tok[ti], "NUMBER") == 0 && ti+1 < nmt) { num = (int)atof(tok[ti+1]); ti++; }
                else strncpy(raw2[nraw2++], tok[ti], 63);
            }
            const char *mnames[] = {"angle","right-angle","segment","parallel"};
            strncpy(n->strVal, mnames[mt], MAX_EXPR-1);
            for (int i = 0; i < 4; i++) n->color[i] = mc2[i];
            n->numVal = num;
            int exp = mt == 2 ? 2 : mt == 3 ? 4 : 3;
            for (int i = 0; i < exp && i < nraw2; i++) {
                AstNode *child = ast_new(NODE_IDENT);
                strncpy(child->strVal, raw2[i], MAX_EXPR-1);
                ast_add_child(n, child);
            }
            ast_add_child(program, n);
        } else if (strcmp(kw, "CONGRUENT") == 0 || strcmp(kw, "SIMILAR") == 0) {
            AstNode *n = ast_new(NODE_CONGRUENT);
            strncpy(n->strVal, strcmp(kw, "CONGRUENT") == 0 ? "congruent" : "similar", MAX_EXPR-1);
            if (nt >= 6) {
                for (int i = 0; i < 6; i++) {
                    AstNode *child = ast_new(NODE_IDENT);
                    strncpy(child->strVal, tok[i], MAX_EXPR-1);
                    ast_add_child(n, child);
                }
            }
            ast_add_child(program, n);
        } else if (strcmp(kw, "KEYPOINT") == 0) {
            if (nt >= 2) {
                AstNode *n = ast_new(NODE_KEYPOINT);
                n->numVal = atof(tok[0]);
                const char *kl = rest; while (*kl && !isspace((unsigned char)*kl)) kl++; while (*kl && isspace((unsigned char)*kl)) kl++;
                strncpy(n->strVal, kl, MAX_EXPR-1);
                ast_add_child(program, n);
            }
        } else if (strcmp(kw, "FUNCTION") == 0) {
            AstNode *n = ast_new(NODE_FUNCTION);
            double fc[4]; parse_color_ast("#e74c3c", fc);
            char expr[256] = "", fname[64] = "", params[4][64]; int np = 0;
            char fr[256]; strncpy(fr, rest, 255);
            char *colp = strstr(fr, "COLOR"); if (colp) { *colp = 0; const char *cv = colp + 5; while (*cv && isspace((unsigned char)*cv)) cv++; if (*cv == ':') cv++; while (*cv && isspace((unsigned char)*cv)) cv++; parse_color_ast(cv, fc); }
            char *eq = strchr(fr, '='); if (!eq) eq = strchr(fr, ':');
            if (eq) {
                const char *npstart = fr; while (*npstart && isspace((unsigned char)*npstart)) npstart++;
                const char *paren = strchr(npstart, '(');
                if (paren && paren < eq) {
                    int nl = (int)(paren - npstart); if (nl > 63) nl = 63; strncpy(fname, npstart, nl); fname[nl] = 0;
                    const char *pe = paren + 1; const char *pc = strchr(pe, ')');
                    if (pc) { char pb[128]; int pl = (int)(pc - pe); if (pl > 127) pl = 127; strncpy(pb, pe, pl); pb[pl] = 0; char *tk = strtok(pb, ","); while (tk && np < 4) { while (*tk && isspace((unsigned char)*tk)) tk++; strncpy(params[np++], tk, 63); tk = strtok(NULL, ","); } }
                    eq++; while (*eq && isspace((unsigned char)*eq)) eq++;
                    strncpy(expr, eq, 255);
                }
            }
            if (fname[0]) {
                snprintf(n->strVal, MAX_EXPR-1, "%s", fname);
                strncpy(n->strVal + MAX_EXPR/2, expr, MAX_EXPR/2 - 1);
                for (int i = 0; i < np; i++) { AstNode *child = ast_new(NODE_IDENT); strncpy(child->strVal, params[i], MAX_EXPR-1); ast_add_child(n, child); }
                for (int i = 0; i < 4; i++) n->color[i] = fc[i];
                ast_add_child(program, n);
            }
        } else if (strcmp(kw, "VECTOR") == 0) {
            AstNode *n = ast_new(NODE_VECTOR);
            char label[64] = "v"; double vc[4]; parse_color_ast("#3498db", vc);
            char raw[8][64]; int nraw = 0;
            for (int ti = 0; ti < nt; ti++) {
                if (typ[ti] == TK_KW && strcasecmp(tok[ti], "LABEL") == 0 && ti+1 < nt) { strncpy(label, tok[ti+1], 63); ti++; }
                else if (typ[ti] == TK_KW && strcasecmp(tok[ti], "COLOR") == 0 && ti+1 < nt) { parse_color_ast(tok[ti+1], vc); ti++; }
                else if (nraw < 8) strncpy(raw[nraw++], tok[ti], 63);
            }
            strncpy(n->strVal, label, MAX_EXPR-1);
            for (int i = 0; i < 4; i++) n->color[i] = vc[i];
            for (int i = 0; i < nraw && i < 8; i++) {
                AstNode *child = ast_new(NODE_IDENT);
                strncpy(child->strVal, raw[i], MAX_EXPR-1);
                ast_add_child(n, child);
            }
            ast_add_child(program, n);
        } else if (strcmp(kw, "MATRIX") == 0) {
            AstNode *n = ast_new(NODE_MATRIX);
            char label[64] = "M";
            for (int ti = 0; ti < nt; ti++) if (typ[ti] == TK_KW && strcasecmp(tok[ti], "LABEL") == 0 && ti+1 < nt) { strncpy(label, tok[ti+1], 63); break; }
            strncpy(n->strVal, label, MAX_EXPR-1);
            char rowBuf[1024]; strncpy(rowBuf, rest, 1023);
            char *row = strtok(rowBuf, ";");
            while (row && n->numChildren < 8) {
                AstNode *rnode = ast_new(NODE_BLOCK);
                char *c = row; while (*c && rnode->numChildren < 8) {
                    while (*c && isspace((unsigned char)*c)) c++; if (!*c || *c == ';') break;
                    AstNode *val = ast_new(NODE_NUMBER); val->numVal = atof(c);
                    ast_add_child(rnode, val);
                    while (*c && !isspace((unsigned char)*c) && *c != ';') c++;
                }
                if (rnode->numChildren > 0) ast_add_child(n, rnode);
                row = strtok(NULL, ";");
            }
            ast_add_child(program, n);
        } else if (strcmp(kw, "TABLE") == 0) {
            AstNode *n = ast_new(NODE_TABLE);
            char label[64] = "";
            for (int ti = 0; ti < nt; ti++) if (typ[ti] == TK_KW && strcasecmp(tok[ti], "LABEL") == 0 && ti+1 < nt) { strncpy(label, tok[ti+1], 63); break; }
            strncpy(n->strVal, label, MAX_EXPR-1);
            char rowBuf[1024]; strncpy(rowBuf, rest, 1023);
            char *row = strtok(rowBuf, ";");
            while (row && n->numChildren < 8) {
                AstNode *rnode = ast_new(NODE_BLOCK);
                char *c = row; while (*c && rnode->numChildren < 8) {
                    while (*c && isspace((unsigned char)*c)) c++; if (!*c || *c == ';') break;
                    AstNode *val = ast_new(NODE_NUMBER); val->numVal = atof(c);
                    ast_add_child(rnode, val);
                    while (*c && !isspace((unsigned char)*c) && *c != ';') c++;
                }
                if (rnode->numChildren > 0) ast_add_child(n, rnode);
                row = strtok(NULL, ";");
            }
            ast_add_child(program, n);
        } else if (strcmp(kw, "TEXT") == 0) {
            AstNode *n = ast_new(NODE_TEXT);
            double tc[4]; parse_color_ast("#333", tc);
            const char *col = strstr(rest, "COLOR");
            if (col) {
                int txtLen = (int)(col - rest); while (txtLen > 0 && isspace((unsigned char)rest[txtLen-1])) txtLen--;
                strncpy(n->strVal, rest, txtLen); n->strVal[txtLen] = 0;
                const char *cv = col + 5; while (*cv && *cv != ':') cv++; if (*cv == ':') cv++; while (*cv && isspace((unsigned char)*cv)) cv++;
                parse_color_ast(cv, tc);
            } else { strncpy(n->strVal, rest, MAX_EXPR-1); }
            for (int i = 0; i < 4; i++) n->color[i] = tc[i];
            ast_add_child(program, n);
        } else if (strcmp(kw, "SOLVE") == 0) {
            AstNode *n = ast_new(NODE_SOLVE);
            char var[64] = "x", expr[256] = "";
            const char *fp = strstr(rest, "FOR"); if (fp) { fp += 3; while (*fp && *fp != ':') fp++; if (*fp == ':') fp++; while (*fp && isspace((unsigned char)*fp)) fp++; int vi = 0; while (fp[vi] && !isspace((unsigned char)fp[vi]) && fp[vi] != ':' && vi < 63) { var[vi] = fp[vi]; vi++; } var[vi] = 0; }
            const char *eqp = strstr(rest, "="); if (eqp) { const char *es = eqp + 1; while (*es && isspace((unsigned char)*es)) es++; strncpy(expr, es, 255); }
            if (!expr[0]) strncpy(expr, rest, 255);
            strncpy(n->strVal, var, MAX_EXPR-1);
            strncpy(n->strVal + MAX_EXPR/2, expr, MAX_EXPR/2 - 1);
            ast_add_child(program, n);
        } else if (strcmp(kw, "IMPORT") == 0) {
            if (nt >= 1) {
                AstNode *n = ast_new(NODE_IMPORT);
                strncpy(n->strVal, tok[0], MAX_EXPR-1);
                for (int ti = 1; ti < nt-1; ti++) if (strcasecmp(tok[ti], "AS") == 0) { strncpy(n->strVal + MAX_EXPR/2, tok[ti+1], MAX_EXPR/2 - 1); break; }
                ast_add_child(program, n);
            }
        } else if (strcmp(kw, "__INF_BEGIN__") == 0) {
            // Parse INF body lines as segment children
            AstNode *infNode = ast_new(NODE_BLOCK);
            strcpy(infNode->strVal, "__INF");
            si++;
            while (si < nexp && strcmp(exp[si], "__INF_END__") != 0) {
                char *ls = exp[si];
                char *bs = ls; while (*bs && isspace((unsigned char)*bs)) bs++;
                if (strncasecmp(bs, "MOVE", 4) == 0) {
                    const char *col = strchr(bs, ':');
                    if (!col) { si++; continue; }
                    col++;
                    char mt[64][64]; TokenType mtp[64];
                    int mnt = tokenize(col, mt, mtp, 64);
                    int rel = 0; int mti = 0; double mspd = 1;
                    if (mnt > 0 && strcasecmp(mt[0], "REL") == 0) { rel = 1; mti = 1; }
                    double mc[2]; int mnc = 0;
                    for (; mti < mnt && mnc < 2; mti++) {
                        if (mtp[mti] == TK_KW && strcasecmp(mt[mti], "SPEED") == 0 && mti+1 < mnt) { mspd = atof(mt[++mti]); continue; }
                        if (mtp[mti] == TK_NUM) { mc[mnc++] = atof(mt[mti]); }
                    }
                    if (mnc >= 2) {
                        AstNode *seg = ast_new(NODE_SEGMENT);
                        seg->numVal = mspd;
                        seg->children = calloc(2, sizeof(AstNode*));
                        seg->children[0] = ast_new(NODE_NUMBER); seg->children[0]->numVal = mc[0];
                        seg->children[1] = ast_new(NODE_NUMBER); seg->children[1]->numVal = mc[1];
                        seg->numChildren = 2; seg->capChildren = 2;
                        strcpy(seg->strVal, rel ? "MOVE_REL" : "MOVE");
                        ast_add_child(infNode, seg);
                    }
                }
                si++;
            }
            ast_add_child(program, infNode);
        } else if (strcmp(kw, "__INF_END__") == 0) {
            // consumed by INF_BEGIN
        } else if (strcmp(kw, "CONDITION") == 0) {
            AstNode *n = ast_new(NODE_BLOCK);
            strcpy(n->strVal, "CONDITION");
            strncpy(n->strVal + 16, rest, MAX_EXPR-17);
            ast_add_child(program, n);
        } else if (strcmp(kw, "CURVE") == 0) {
            AstNode *n = ast_new(NODE_BLOCK);
            strcpy(n->strVal, "CURVE");
            strncpy(n->strVal + 16, rest, MAX_EXPR-17);
            ast_add_child(program, n);
        } else if (strcmp(kw, "PARAM") == 0) {
            AstNode *n = ast_new(NODE_BLOCK);
            strcpy(n->strVal, "PARAM");
            strncpy(n->strVal + 16, rest, MAX_EXPR-17);
            ast_add_child(program, n);
        }
    }

    free(raw);
    free(exp);
    return program;
}

// ── Codegen: AST → AnimationData ──
typedef struct {
    AstNode *ifs[MAX_CHECKS];
    int numIfs;
} CodegenCtx;

static void resolve_point_label(AstNode *node, AnimationData *data, double *outX, double *outY) {
    if (!node) return;
    for (int pi = 0; pi < data->numPoints; pi++) {
        if (strcmp(data->points[pi].label, node->strVal) == 0) {
            *outX = data->points[pi].startX;
            *outY = data->points[pi].startY;
            return;
        }
    }
    *outX = atof(node->strVal);
    // Try to find second value in next sibling
    *outY = 0;
}

int ast_codegen(AstNode *program, AnimationData *data) {
    if (!program || program->type != NODE_PROGRAM) return 0;

    memset(data, 0, sizeof(*data));
    strcpy(data->title, "数学教学可视化");
    data->xMin = -10; data->xMax = 10; data->yMin = -10; data->yMax = 10;
    data->totalTime = 10;
    color_idx = 0;

    // First pass: title, range, imports
    for (int i = 0; i < program->numChildren; i++) {
        AstNode *n = program->children[i];
        if (n->type == NODE_TITLE) {
            strncpy(data->title, n->strVal, MAX_EXPR-1);
        } else if (n->type == NODE_RANGE && n->numChildren >= 4) {
            data->xMin = n->children[0]->numVal;
            data->xMax = n->children[1]->numVal;
            data->yMin = n->children[2]->numVal;
            data->yMax = n->children[3]->numVal;
        } else if (n->type == NODE_IMPORT && data->numImports < MAX_IMPORTS) {
            Import *im = &data->imports[data->numImports++];
            strncpy(im->name, n->strVal, MAX_LABEL-1);
            if (n->strVal[MAX_EXPR/2]) strncpy(im->alias, n->strVal + MAX_EXPR/2, MAX_LABEL-1);
        }
    }

    // Second pass: points (must be before other elements)
    for (int i = 0; i < program->numChildren; i++) {
        AstNode *n = program->children[i];
        if (n->type != NODE_POINT_STMT) continue;
        if (data->numPoints >= MAX_POINTS) continue;
        Point *pt = &data->points[data->numPoints++];
        strncpy(pt->label, n->strVal, MAX_LABEL-1);

        // Find start position (last 3 children are sx, sy, color)
        int nc = n->numChildren;
        double sx = 0, sy = 0;
        if (nc >= 3) {
            sx = n->children[nc-3]->numVal;
            sy = n->children[nc-2]->numVal;
            pt->startX = sx; pt->startY = sy;
            // Color
            AstNode *col = n->children[nc-1];
            for (int c = 0; c < 4; c++) pt->color[c] = (col && col->type == NODE_COLOR) ? col->color[c] : palette[data->numPoints % 8][c];
        }
        // Segments (exclude last 3)
        double cx = sx, cy = sy;
        for (int j = 0; j < nc - 3; j++) {
            AstNode *seg = n->children[j];
            if (seg->type != NODE_SEGMENT) continue;
            if (pt->numSegments >= MAX_SEGMENTS) break;
            Segment *s = &pt->segments[pt->numSegments];
            if (strcmp(seg->strVal, "VELOCITY") == 0) {
                s->vx = seg->children[0]->numVal;
                s->vy = seg->children[1]->numVal;
                s->speed = seg->numVal;
                s->isVelocity = 1;
            } else { // MOVE
                s->endX = seg->children[0]->numVal;
                s->endY = seg->children[1]->numVal;
                s->speed = seg->numVal;
            }
            pt->numSegments++;
        }
        // Check for __INF block following this point
        for (int k = i + 1; k < program->numChildren; k++) {
            AstNode *inf = program->children[k];
            if (inf->type == NODE_BLOCK && strcmp(inf->strVal, "__INF") == 0) {
                pt->numLoopBody = 0;
                double lx = pt->numSegments > 0 ? pt->segments[pt->numSegments-1].endX : pt->startX;
                double ly = pt->numSegments > 0 ? pt->segments[pt->numSegments-1].endY : pt->startY;
                for (int bi = 0; bi < inf->numChildren; bi++) {
                    AstNode *iseg = inf->children[bi];
                    if (iseg->type != NODE_SEGMENT) continue;
                    if (pt->numLoopBody >= MAX_SEGMENTS) break;
                    Segment *ls = &pt->loopBody[pt->numLoopBody];
                    double ex = iseg->children[0]->numVal;
                    double ey = iseg->children[1]->numVal;
                    if (strcmp(iseg->strVal, "MOVE_REL") == 0) { ex += lx; ey += ly; }
                    ls->endX = ex; ls->endY = ey; ls->speed = iseg->numVal;
                    lx = ex; ly = ey;
                    pt->numLoopBody++;
                }
                // Compute loop duration
                double lt = 0;
                double lcx = pt->numSegments > 0 ? pt->segments[pt->numSegments-1].endX : pt->startX;
                double lcy = pt->numSegments > 0 ? pt->segments[pt->numSegments-1].endY : pt->startY;
                for (int j = 0; j < pt->numLoopBody; j++) {
                    double dx = pt->loopBody[j].endX - lcx, dy = pt->loopBody[j].endY - lcy;
                    double d = hypot(dx, dy);
                    if (pt->loopBody[j].speed > 0) lt += d / pt->loopBody[j].speed;
                    lcx = pt->loopBody[j].endX; lcy = pt->loopBody[j].endY;
                }
                pt->loopDuration = lt;
                break;
            }
            if (inf->type != NODE_BLOCK) break;
        }
    }

    // Third pass: everything else
    for (int i = 0; i < program->numChildren; i++) {
        AstNode *n = program->children[i];
        switch (n->type) {
        case NODE_LINE: {
            if (data->numLines >= MAX_LINES) break;
            Line *l = &data->lines[data->numLines++];
            strncpy(l->label, n->strVal, MAX_LABEL-1);
            for (int i = 0; i < 4; i++) l->color[i] = n->color[i];
            // Resolve children as point labels
            double vals[4]; int nv = 0;
            for (int j = 0; j < n->numChildren && nv < 4; j++) {
                double v = numval(n->children[j]->strVal);
                if (v != 0 || n->children[j]->strVal[0] == '0' || n->children[j]->strVal[0] == '.') {
                    vals[nv++] = v;
                } else {
                    for (int pi = 0; pi < data->numPoints; pi++)
                        if (strcmp(data->points[pi].label, n->children[j]->strVal) == 0) {
                            vals[nv++] = data->points[pi].startX;
                            vals[nv++] = data->points[pi].startY;
                            break;
                        }
                }
            }
            if (nv >= 4) { l->x1 = vals[0]; l->y1 = vals[1]; l->x2 = vals[2]; l->y2 = vals[3]; }
            break;
        }
        case NODE_AREA: {
            if (data->numAreas >= MAX_AREAS) break;
            Area *a = &data->areas[data->numAreas++];
            strncpy(a->type, n->strVal, MAX_LABEL-1);
            // Label stored in second half of strVal
            const char *alabel = n->strVal + MAX_EXPR/2;
            strncpy(a->label, alabel[0] ? alabel : "S", MAX_LABEL-1);
            for (int i = 0; i < 4; i++) a->color[i] = n->color[i];
            a->numPts = 0;
            // Resolve vertices
            if (strcmp(a->type, "triangle") == 0 && n->numChildren >= 3) {
                for (int j = 0; j < 3 && j < n->numChildren; j++) {
                    strncpy(a->vertexLabels[j], n->children[j]->strVal, MAX_LABEL-1);
                    int found = 0;
                    for (int pi = 0; pi < data->numPoints; pi++)
                        if (strcmp(data->points[pi].label, n->children[j]->strVal) == 0) {
                            a->pts[a->numPts++] = (Vec2){data->points[pi].startX, data->points[pi].startY};
                            found = 1; break;
                        }
                    if (!found) {
                        double v = numval(n->children[j]->strVal);
                        a->pts[a->numPts++] = (Vec2){v, j+1 < n->numChildren ? numval(n->children[j+1]->strVal) : 0};
                        if (j+1 < n->numChildren) j++;
                    }
                }
                if (a->numPts >= 3) a->value = compute_polygon_area(a->pts, a->numPts);
            }
            break;
        }
        case NODE_CHECK: {
            if (data->numChecks >= MAX_CHECKS) break;
            Check *ch = &data->checks[data->numChecks++];
            strncpy(ch->condition, n->strVal, MAX_EXPR-1);
            // Last child is message
            if (n->numChildren > 0) {
                AstNode *msgNode = n->children[n->numChildren - 1];
                if (msgNode->type == NODE_EXPR_STRING) strncpy(ch->message, msgNode->strVal, MAX_EXPR-1);
            }
            // Record expressions
            for (int j = 0; j < n->numChildren - 1 && j < 8; j++) {
                strncpy(ch->recordExprs[j], n->children[j]->strVal, MAX_EXPR-1);
                ch->numRecordExprs++;
            }
            ch->triggerTime = -1; ch->triggered = 0;
            break;
        }
        case NODE_OUTPUT: {
            if (data->numOutputs >= MAX_OUTPUTS) break;
            Output *o = &data->outputs[data->numOutputs++];
            strncpy(o->label, n->strVal, MAX_LABEL-1);
            const char *otype = n->strVal + MAX_EXPR/2;
            const char *jtype = n->strVal + MAX_EXPR/4*3;
            strncpy(o->type, otype, MAX_LABEL-1);
            strncpy(o->judgeType, jtype, MAX_LABEL-1);
            for (int i = 0; i < 4; i++) o->color[i] = n->color[i];
            o->showOnCanvas = 1;
            int hasNum = 0;
            for (int j = 0; j < n->numChildren && j < 8; j++) {
                strncpy(o->labels[j], n->children[j]->strVal, MAX_LABEL-1);
                if (n->children[j]->type == NODE_NUMBER || n->children[j]->numVal != 0 ||
                    n->children[j]->strVal[0] == '0' || n->children[j]->strVal[0] == '.') {
                    o->numeric[j] = n->children[j]->numVal;
                    hasNum = 1;
                }
            }
            o->numLabels = n->numChildren;
            if (hasNum) { o->isNumeric = 1; o->numNumeric = n->numChildren; }
            break;
        }
        case NODE_MARK: {
            if (data->numMarks >= MAX_MARKS) break;
            Mark *m = &data->marks[data->numMarks++];
            strncpy(m->type, n->strVal, MAX_LABEL-1);
            for (int i = 0; i < 4; i++) m->color[i] = n->color[i];
            m->number = (int)n->numVal;
            for (int j = 0; j < n->numChildren && j < 4; j++)
                strncpy(m->labels[j], n->children[j]->strVal, MAX_LABEL-1);
            break;
        }
        case NODE_CONGRUENT: {
            if (data->numCongruents >= 8) break;
            Congruent *cg = &data->congruents[data->numCongruents++];
            strncpy(cg->type, n->strVal, MAX_LABEL-1);
            for (int j = 0; j < 3 && j < n->numChildren; j++)
                strncpy(cg->tri1[j], n->children[j]->strVal, MAX_LABEL-1);
            for (int j = 3; j < 6 && j < n->numChildren; j++)
                strncpy(cg->tri2[j-3], n->children[j]->strVal, MAX_LABEL-1);
            break;
        }
        case NODE_KEYPOINT: {
            if (data->numKeypoints >= MAX_KEYPOINTS) break;
            data->keypoints[data->numKeypoints].time = n->numVal;
            strncpy(data->keypoints[data->numKeypoints].label, n->strVal, MAX_LABEL-1);
            data->numKeypoints++;
            break;
        }
        case NODE_FUNCTION: {
            if (data->numFunctions >= MAX_FUNCTIONS) break;
            Function *f = &data->functions[data->numFunctions++];
            strncpy(f->fname, n->strVal, MAX_EXPR-1);
            strncpy(f->expr, n->strVal + MAX_EXPR/2, MAX_EXPR-1);
            for (int j = 0; j < n->numChildren && j < 4; j++)
                strncpy(f->params[j], n->children[j]->strVal, MAX_LABEL-1);
            f->numParams = n->numChildren;
            for (int i = 0; i < 4; i++) f->color[i] = n->color[i];
            break;
        }
        case NODE_VECTOR: {
            if (data->numVectors >= MAX_VECTORS) break;
            Vector *v = &data->vectors[data->numVectors++];
            strncpy(v->type, "coords", MAX_LABEL-1);
            strncpy(v->label, n->strVal, MAX_LABEL-1);
            for (int i = 0; i < 4; i++) v->color[i] = n->color[i];
            double vals[4]; int nv = 0;
            for (int j = 0; j < n->numChildren && nv < 4; j++) {
                double va = numval(n->children[j]->strVal);
                if (va != 0 || n->children[j]->strVal[0] == '0' || n->children[j]->strVal[0] == '.') {
                    vals[nv++] = va;
                } else {
                    for (int pi = 0; pi < data->numPoints; pi++)
                        if (strcmp(data->points[pi].label, n->children[j]->strVal) == 0) {
                            vals[nv++] = data->points[pi].startX;
                            if (j+1 < n->numChildren && nv < 4) { vals[nv++] = data->points[pi].startY; j++; }
                            break;
                        }
                }
            }
            if (nv >= 4) { v->x1 = vals[0]; v->y1 = vals[1]; v->x2 = vals[2]; v->y2 = vals[3]; }
            else if (nv >= 2) { v->x1 = 0; v->y1 = 0; v->x2 = vals[0]; v->y2 = vals[1]; }
            break;
        }
        case NODE_MATRIX: {
            if (data->numMatrices >= MAX_MATRICES) break;
            Matrix *m = &data->matrices[data->numMatrices++];
            strncpy(m->label, n->strVal, MAX_LABEL-1);
            m->rows = 0; m->cols = 0;
            for (int r = 0; r < n->numChildren && r < 8; r++) {
                AstNode *rowNode = n->children[r];
                for (int c = 0; c < rowNode->numChildren && c < 8; c++)
                    m->data[r][c] = rowNode->children[c]->numVal;
                if (rowNode->numChildren > m->cols) m->cols = rowNode->numChildren;
                m->rows++;
            }
            break;
        }
        case NODE_TABLE: {
            if (data->numTables >= MAX_TABLES) break;
            Table *t = &data->tables[data->numTables++];
            strncpy(t->label, n->strVal, MAX_LABEL-1);
            t->rows = 0; t->cols = 0;
            for (int r = 0; r < n->numChildren && r < 8; r++) {
                AstNode *rowNode = n->children[r];
                for (int c = 0; c < rowNode->numChildren && c < 8; c++)
                    t->data[r][c] = rowNode->children[c]->numVal;
                if (rowNode->numChildren > t->cols) t->cols = rowNode->numChildren;
                t->rows++;
            }
            break;
        }
        case NODE_TEXT: {
            if (data->numTexts >= MAX_TEXTS) break;
            Text *t = &data->texts[data->numTexts++];
            strncpy(t->text, n->strVal, MAX_EXPR-1);
            for (int i = 0; i < 4; i++) t->color[i] = n->color[i];
            break;
        }
        case NODE_SOLVE: {
            if (data->numFunctions >= MAX_FUNCTIONS) break;
            Function *f = &data->functions[data->numFunctions++];
            strcpy(f->fname, "solve");
            strncpy(f->params[0], n->strVal, MAX_LABEL-1); f->numParams = 1;
            strncpy(f->expr, n->strVal + MAX_EXPR/2, MAX_EXPR-1);
            for (int i = 0; i < 4; i++) f->color[i] = (double[]){0.61,0.35,0.71,1}[i];
            f->isSolve = 1;
            break;
        }
        case NODE_BLOCK: {
            if (strncmp(n->strVal, "CONDITION", 9) == 0) {
                // Store as a check with the condition string
                if (data->numChecks < MAX_CHECKS) {
                    Check *ch = &data->checks[data->numChecks++];
                    strncpy(ch->condition, n->strVal + 16, MAX_EXPR-1);
                    snprintf(ch->message, MAX_EXPR-1, "条件满足");
                    ch->triggerTime = -1; ch->triggered = 0;
                }
            } else if (strncmp(n->strVal, "CURVE", 5) == 0) {
                // CURVE: name PARAM: t FROM 0 TO 1 expr
                // Store as a function
                if (data->numFunctions < MAX_FUNCTIONS) {
                    Function *f = &data->functions[data->numFunctions++];
                    strcpy(f->fname, "curve");
                    strncpy(f->expr, n->strVal + 16, MAX_EXPR-1);
                    strcpy(f->params[0], "t"); f->numParams = 1;
                    for (int i = 0; i < 4; i++) f->color[i] = (double[]){0.61,0.35,0.71,1}[i];
                }
            } else if (strncmp(n->strVal, "PARAM", 5) == 0) {
                // PARAM: name = value — set as module variable
                const char *p = n->strVal + 16;
                char pname[64]; int pni = 0;
                while (*p && *p != '=' && *p != ':' && pni < 63) { pname[pni++] = *p++; } pname[pni] = 0;
                while (*p && (*p == '=' || *p == ':' || isspace((unsigned char)*p))) p++;
                if (pname[0] && *p) {
                    double pval = eval_expr(p);
                    set_module_var(pname, pval);
                }
            }
            break;
        }
        default: break;
        }
    }

    // Compute total time
    double maxTime = 0; int hasInf = 0;
    for (int i = 0; i < data->numPoints; i++) {
        Point *pt = &data->points[i];
        double t = 0, cx = pt->startX, cy = pt->startY;
        for (int j = 0; j < pt->numSegments; j++) {
            if (pt->segments[j].isVelocity) continue;
            double dx = pt->segments[j].endX - cx, dy = pt->segments[j].endY - cy, d = hypot(dx, dy);
            if (pt->segments[j].speed > 0) t += d / pt->segments[j].speed;
            cx = pt->segments[j].endX; cy = pt->segments[j].endY;
        }
        if (pt->numLoopBody > 0 && pt->loopDuration > 0) { hasInf = 1; t += pt->loopDuration; }
        if (t > maxTime) maxTime = t;
    }
    data->totalTime = maxTime > 0 ? maxTime : 10;
    data->hasInfiniteLoop = hasInf;

    return 1;
}
