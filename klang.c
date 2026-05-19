#include <__stddef_unreachable.h>
#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stddef.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#define DEBUG 0

#define TOKEN_TYPES \
    X(NONE_NIL)    \
    X(SIGN_END)    \
    X(LINE_STR)    \
    X(LINE_END)    \
    X(OPER_PLS)    \
    X(OPER_MIN)    \
    X(OPER_MUL)    \
    X(OPER_DIV)    \
    X(OPER_EXP)    \
    X(OPER_LOG)    \
    X(OPER_MOD)    \
    X(OPER_EQL)    \
    X(OPER_MOR)    \
    X(OPER_LES)    \
    X(UNRY_NEG)    \
    X(UNRY_INV)    \
    X(LITR_NUM)    \
    X(PARN_OPN)    \
    X(PARN_CLS)    \
    X(LITR_STR)    \
    X(EXPR_ASG)    \
    X(EXPR_FUN)    \
    X(EXPR_MUT)    \
    X(IDDT_ASG)    \
    X(IIOO_OUT)    \
    X(CTRL_THN)    \
    X(CTRL_DOX)    \
    X(LOOP_TIM)    \
    X(LOOP_WHL)    \
    X(CTRL_WHN)    \

typedef enum {
    #define X(name) name,
    TOKEN_TYPES
    #undef X
} TokenType;

static const char * const tokenTypes[] = {
    #define X(name) [name] = #name,
    TOKEN_TYPES
    #undef X
};

static const char *tokenToReal[] = {
    [OPER_PLS] = "+",
    [OPER_MIN] = "-",
    [OPER_MUL] = "*",
    [OPER_DIV] = "/",
    [OPER_EXP] = "^",
    [OPER_EQL] = "==",
    [OPER_MOR] = ">",
    [OPER_LOG] = "log",
    [OPER_MOD] = "%",
    [OPER_LES] = "<",
    [UNRY_NEG] = "-",
    [UNRY_INV] = "!",
    [PARN_OPN] = "(",
    [PARN_CLS] = ")",
    [LITR_STR] = "\"",
    [EXPR_ASG] = "is",
    [EXPR_MUT] = "now",
    [EXPR_FUN] = "of",
    [IIOO_OUT] = "say",
    [CTRL_THN] = ",",
    [CTRL_DOX] = "do",
    [LOOP_TIM] = "times",
    [LOOP_WHL] = "while",
    [CTRL_WHN] = "when"
};

typedef struct 
{
    TokenType type;
    long double val;
    char *str;
} Token;

typedef enum
{
    OPERATION,
    PARENTHESES,
    NUM,
    STRING,
    IDENTITY
} ExpressionType;

typedef struct expr Expression;

typedef struct
{
    struct expr *lhs;
    struct expr *rhs; 
    TokenType oper;
} Operation;

typedef union
{
    Operation operation;
    long double num;
    char *string;
    char *identity;
    struct expr *parentheses;
} ExpressionAs;

struct expr 
{
    ExpressionAs as;
    Expression *nextLine;
    ExpressionType type;
};

typedef struct {
    char *key;
    Expression* val;
} Identity;

const char *tokenToStr(Token token)
{
    switch (token.type)
    {
        case IDDT_ASG: return token.str;
        default: return tokenToReal[token.type];
    }
}

void error(const char *why)
{
    printf("%s\n", why);
    abort();
}

Token *tokenize(char* in)
{
    __block size_t tokenCap = 2;
    __block size_t tokenCount = 0;
    __block Token *tokens = malloc(tokenCap * sizeof(Token));
    if (tokens == NULL)
        error("failed to allocate memory for tokenizer");
    __block size_t ptr = 0;
    bool munchVal = false;
    bool munchStr = false;
    bool munchIdentity = false;

    char* (^unescape)(char*) = ^char*(char *in)
    {
        char *out = calloc(strlen(in) + 1, 1);
        size_t j = 0;
        for (size_t i = 0; in[i] != '\0'; i++)
            if (in[i] == '\\')
            {
                switch (in[++i])
                {
                    case 'n':  out[j++] = '\n'; break;
                    case 't':  out[j++] = '\t'; break;
                    case 'r':  out[j++] = '\r'; break;
                    case '\\': out[j++] = '\\'; break;
                    case '"':  out[j++] = '\"';  break;
                    case '0':  out[j++] = '\0'; break;
                    default:
                        out[j++] = '\\';
                        out[j++] = in[i];
                    break;
                }
            }
            else out[j++] = in[i];
        out[j] = '\0';
        return out;
    };

    auto checkCapacity = ^
    {
        if (tokenCount >= tokenCap)
        {
            tokenCap *= 2;
            Token *temp = realloc(tokens, tokenCap * sizeof(Token));
            if (temp == NULL)
            {
                free(tokens);
                error("failed to allocate memory for tokenizer");
            }
            tokens = temp;
        }
    };

    auto addToken = ^(TokenType type){
        checkCapacity();
        tokens[tokenCount] = (Token) {
            .type = type,
            .val = NAN,
            .str = NULL
        };
        tokenCount++;
    };

    auto addVal = ^(TokenType type, long double val){
        checkCapacity();
        tokens[tokenCount] = (Token) {
            .type = type,
            .val = val,
            .str = NULL
        };
        tokenCount++;
    };

    auto addStr = ^(TokenType type, char *str){
        checkCapacity();
        tokens[tokenCount] = (Token) {
            .type = type,
            .val = NAN,
            .str = str
        };
        tokenCount++;
    };

    auto detectKeyword = ^bool(const char *keyword, TokenType type)
    {
        char *temp = strdup(in+ptr);
        temp[strlen(keyword)] = '\0';
        char next = in[ptr+strlen(keyword)];

        int res = strcmp(temp, keyword);
        free(temp);
        if (!res && !isalpha(next) && !isdigit(next) && next != '_')
        {
            addToken(type);
            ptr += strlen(keyword) - 1;
            return true;
        }

        return false;
    };

    auto detectKeywordForOper = ^bool(const char *keyword, TokenType type)
    {
        char *temp = strdup(in+ptr);
        temp[strlen(keyword)] = '\0';
        char next = in[ptr+strlen(keyword)];

        int res = strcmp(temp, keyword);
        free(temp);
        if (!res && !isalpha(next) && next != '_')
        {
            addToken(type);
            ptr += strlen(keyword) - 1;
            return true;
        }

        return false;
    };

    auto detectKeywordWithValue = ^bool(const char *keyword, long double val)
    {
        char *temp = strdup(in+ptr);
        temp[strlen(keyword)] = '\0';
        char next = in[ptr+strlen(keyword)];

        int res = strcmp(temp, keyword);
        free(temp);
        if (!res && !isalpha(next) && !isdigit(next) && next != '_')
        {
            addVal(LITR_NUM, val);
            ptr += strlen(keyword) - 1;
            return true;
        }

        return false;
    };

    addToken(LINE_STR);
    for (ptr = 0; in[ptr] != '\0'; ptr++)
    {
        if (munchIdentity && !isalpha(in[ptr]) && !isdigit(in[ptr]) && in[ptr] != '_') 
            munchIdentity = false;

        if (munchStr && in[ptr] == '"' && in[ptr - 1] != '\\') 
        {
            ptr++;
            munchStr = false;
        }

        if (munchVal && !isdigit(in[ptr]) && (in[ptr] != '.' || !isdigit(in[ptr + 1])))
            munchVal = false;

        if (munchIdentity || munchStr || munchVal) continue;

        if (!isspace(in[ptr]) || in[ptr] == '\n')
        {
            switch (in[ptr])
            {
                case '0'...'9':
                    long double val = NAN;
                    sscanf(in + ptr, "%Lg", &val); 
                    munchVal = true;
                    addVal(LITR_NUM, val);            
                break;
                case 'a' ... 'z':
                case 'A' ... 'Z':
                case '_':
                {
                    if (detectKeyword("all", PARN_CLS))             continue;
                    if (detectKeyword("is equal to", OPER_EQL))     continue;
                    if (detectKeyword("are equal to", OPER_EQL))    continue;
                    if (detectKeyword("is more than", OPER_MOR))    continue;
                    if (detectKeyword("are more than", OPER_MOR))   continue;
                    if (detectKeyword("is less than", OPER_LES))    continue;
                    if (detectKeyword("are less than", OPER_LES))   continue;
                    if (detectKeyword("a thing that does", EXPR_FUN)) continue;
                    if (detectKeyword("is", EXPR_ASG))              continue;
                    if (detectKeyword("now", EXPR_MUT))             continue;
                    if (detectKeyword("becomes", EXPR_MUT))         continue;
                    if (detectKeyword("become", EXPR_MUT))          continue;
                    if (detectKeyword("and", CTRL_THN))             continue;
                    if (detectKeyword("then", CTRL_THN))            continue;
                    if (detectKeyword("when", CTRL_WHN))            continue;
                    if (detectKeyword("say", IIOO_OUT))             continue;
                    if (detectKeyword("keep doing", CTRL_DOX))      continue;
                    if (detectKeyword("do", CTRL_DOX))              continue;
                    if (detectKeyword("while", LOOP_WHL))           continue;
                    if (detectKeyword("times", LOOP_TIM))           continue;
                    if (detectKeywordForOper("log", OPER_LOG))      continue;
                    if (detectKeywordWithValue("true", 1))      continue;
                    if (detectKeywordWithValue("false", 0))      continue;
                    if (detectKeywordWithValue("a dozen", 12))      continue;
                    if (detectKeywordWithValue("a score", 20))      continue;
                    if (detectKeywordWithValue("the gross", 144))   continue;

                    char *buff = calloc(8192, 1);
                    sscanf(in + ptr, "%126[a-zA-Z0-9_]", buff);
                    munchIdentity = true;

                    addStr(IDDT_ASG, buff);
                }                                       
                break;
                case '"' :
                {
                    char *buff = calloc(8192, 1);
                    sscanf(in + ptr, "%*c%126[^\"]", buff);
                    char *unescaped = unescape(buff);
                    free(buff);
                    munchStr = true;
                    addStr(LITR_STR, unescaped);
                }                                       
                break;
                case '-' : 
                    if (tokenCount > 0 
                        && (tokens[tokenCount - 1].type == LITR_NUM 
                            || tokens[tokenCount - 1].type == PARN_CLS
                            || tokens[tokenCount - 1].type == LITR_STR
                            || tokens[tokenCount - 1].type == IDDT_ASG
                        )
                    )       addToken(OPER_MIN);
                    else    addToken(UNRY_NEG);
                break;
                case '=' :  
                    if (in[++ptr] == '=') 
                            addToken(OPER_EQL);    
                    else error("= used instead of =="); 
                break;
                case '.':  
                    addToken(LINE_END);    
                    addToken(LINE_STR);
                break;
                case ',' :  addToken(CTRL_THN);    break;
                case '<' :  addToken(OPER_LES);    break;
                case '>' :  addToken(OPER_MOR);    break;
                case '!' :  addToken(UNRY_INV);    break;
                case '^' :  addToken(OPER_EXP);    break;
                case '*' :  addToken(OPER_MUL);    break;
                case '/' :  addToken(OPER_DIV);    break;
                case '%' :  addToken(OPER_MOD);    break;
                case '+' :  addToken(OPER_PLS);    break;
                case ';' :
                case '(' :  addToken(PARN_OPN);    break;
                case ')' :  addToken(PARN_CLS);    break;
            }
        }
    }
    if (tokens[tokenCount].type != LINE_END) addToken(LINE_END);
    addToken(SIGN_END);

    return tokens;
}

void displayTokens(const Token *tokens)
{
    for (size_t i = 0; tokens[i].type != SIGN_END; i++)
    {
        char str[64];
        if (!isnan(tokens[i].val) && (int)tokens[i].val == tokens[i].val)
            sprintf(str, "(%d)", (int)tokens[i].val);
        else if (tokens[i].str)
            sprintf(str, "(\"%s\")", tokens[i].str);
        else sprintf(str, "(%Lg)", tokens[i].val);
        
        printf("%s%s%s", tokenTypes[tokens[i].type], (isnan(tokens[i].val) && !tokens[i].str) ? "" : str, tokens[i].type == LINE_END ? "\n" : " ");
    }
}

size_t tokenLen(const Token *tokens)
{
    size_t count = 0;
    while (tokens[count++].type != SIGN_END);
    return count;
}

void freeOperTree(Expression *tree)
{
    switch (tree->type)
    {
        case OPERATION:
            Expression *lhs = tree->as.operation.lhs;
            Expression *rhs = tree->as.operation.rhs;
            if (lhs) freeOperTree(lhs);
            if (rhs) freeOperTree(rhs);
        break;
        case PARENTHESES: freeOperTree(tree->as.parentheses);
        break;
        case STRING: free(tree->as.string);
        break;
        case IDENTITY: free(tree->as.identity);
        default: break;
    }

    if (tree->nextLine) 
        freeOperTree(tree->nextLine);
    free(tree);
};


Expression *parser(Token* in)
{
    size_t len = tokenLen(in);
    __block Token* tokens = malloc(len * sizeof(Token));
    if (tokens == NULL) return NULL;
    memcpy(tokens, in, len * sizeof(Token));

    __block size_t tokenPtr = 0;

    auto match = ^bool(TokenType type[], size_t size)
    {   
        for (size_t i = 0; i < size; i++)
            if (tokens[tokenPtr].type == type[i])
            {
                tokenPtr++;
                return true;
            }
        
        return false;
    };

    auto expect = ^void(TokenType type, const char *why)
    {
        if (!match((TokenType[]){ type }, 1))
            error(why);
    };

    Token *(^self)() = ^Token*{ return &tokens[tokenPtr - 1]; };

    Expression *(^__block parseExpr)();

    Expression *(^parseLiteral)() = ^Expression*(){        
        if (match((TokenType[]){ LITR_NUM, LITR_STR, IDDT_ASG, PARN_OPN }, 4))
        { 
            Expression *expr = calloc(1, sizeof(Expression));
            Token *self = &tokens[tokenPtr - 1];

            switch(self->type)
            {
                case LITR_NUM:
                    expr->type = NUM;
                    expr->as.num = self->val;
                break;
                case LITR_STR:
                    expr->type = STRING;
                    expr->as.string = self->str;
                break;
                case IDDT_ASG:
                    expr->type = IDENTITY;
                    expr->as.identity = self->str;
                break;
                case PARN_OPN:
                    expr->type = PARENTHESES;
                    expr->as.parentheses = parseExpr();
                    if (tokens[tokenPtr].type != LINE_END)
                        expect(PARN_CLS, "parentheses not closed.\nhint: use ')' or \"all\" to close parentheses.");
                break;
                default: unreachable();
            }
            return expr;
        }

        TokenType before = tokens[tokenPtr - 1].type;
        char buff[22];
        sprintf(buff, "no literal after '%s'", tokenToReal[before]);
        error(buff);

        return NULL;
    };
    
    Expression *(^__block parseUnary)() = ^Expression*(){
        if (match((TokenType[]){ UNRY_NEG, UNRY_INV }, 2))
        {
            Expression *node = calloc(1, sizeof(Expression));
            if (node == NULL) return NULL;

            node->type = OPERATION;
            node->as.operation.oper = self()->type;
            node->as.operation.lhs = NULL;
            node->as.operation.rhs = parseUnary();

            return node;
        }
        return parseLiteral();
    };

    Expression *(^__block parseExp)() = ^Expression*{
        Expression *expr = calloc(1, sizeof(Expression));
        if (expr == NULL) return NULL;
        expr = parseUnary();
        
        if (match((TokenType[]){ OPER_EXP, OPER_LOG }, 2))
        {
            Expression *node = calloc(1, sizeof(Expression));
            if (node == NULL) goto err;

            node->type = OPERATION;
            node->as.operation.oper = self()->type;
            node->as.operation.lhs = expr;
            node->as.operation.rhs = parseExp();
            expr = node;
        }

        return expr;
    err:
        freeOperTree(expr);
        return NULL;
    };

    Expression *(^parseMul)() = ^Expression*{
        Expression *expr = calloc(1, sizeof(Expression));
        if (expr == NULL) return NULL;
        expr = parseExp();
        
        while (match((TokenType[]){ OPER_MUL, OPER_DIV, OPER_MOD }, 3))
        {
            Expression *node = calloc(1, sizeof(Expression));
            if (node == NULL) goto err;

            node->type = OPERATION;
            node->as.operation.oper = self()->type;
            node->as.operation.lhs = expr;
            node->as.operation.rhs = parseExp();

            expr = node;
        }

        return expr;
    err:
        freeOperTree(expr);
        return NULL;
    };

    Expression *(^parseAdd)() = ^Expression*{
        Expression *expr = calloc(1, sizeof(Expression));
        if (expr == NULL) return NULL;
        expr = parseMul();
        
        while (match((TokenType[]){ OPER_PLS, OPER_MIN }, 2))
        {
            Expression *node = calloc(1, sizeof(Expression));
            if (node == NULL) goto err;

            node->type = OPERATION;
            node->as.operation.oper = self()->type;
            node->as.operation.lhs = expr;
            node->as.operation.rhs = parseMul();

            expr = node;
        }

        return expr;
    err:
        freeOperTree(expr);
        return NULL;
    };
    
    Expression *(^parseComp)() = ^Expression*
    {
        Expression *expr = calloc(1, sizeof(Expression));
        if (expr == NULL) return NULL;
        expr = parseAdd();
        
        while (match((TokenType[]){ OPER_EQL, OPER_MOR, OPER_LES }, 3))
        {
            Expression *node = calloc(1, sizeof(Expression));
            if (node == NULL) goto err;

            node->type = OPERATION;
            node->as.operation.oper = self()->type;
            node->as.operation.lhs = expr;
            node->as.operation.rhs = parseAdd();

            expr = node;
        }

        return expr;
    err:
        freeOperTree(expr);
        return NULL;
    };

    Expression *(^parseIs)() = ^Expression*
    {
        Expression *expr = calloc(1, sizeof(Expression));
        if (expr == NULL) return NULL;
        expr = parseComp();
        
        if (match((TokenType[]){ EXPR_ASG, EXPR_MUT, EXPR_FUN }, 3))
        {
            while (match((TokenType[]){ EXPR_MUT, EXPR_FUN }, 2)); // match for 'now' or 'a thing that does' keyword
          
            Expression *node = calloc(1, sizeof(Expression));
            if (node == NULL) goto err;

            node->type = OPERATION;
            node->as.operation.oper = self()->type;
            node->as.operation.lhs = expr;
            node->as.operation.rhs = parseComp();

            return node;
        }

        return expr;
    err:
        freeOperTree(expr);
        return NULL;
    };

    Expression *(^parsePrint)() = ^Expression*
    {
        if (match((TokenType[]){ IIOO_OUT }, 1))
        {
            Expression *node = calloc(1, sizeof(Expression));
            if (node == NULL) return NULL;

            node->type = OPERATION;
            node->as.operation.oper = self()->type;
            node->as.operation.lhs = NULL;
            node->as.operation.rhs = parseIs();

            return node;
        }
        return parseIs();
    };

    Expression *(^parseDo)() = ^Expression*
    {
        if (match((TokenType[]){ CTRL_DOX }, 1))
        {
            Expression *node = calloc(1, sizeof(Expression));
            if (node == NULL) return NULL;

            node->type = OPERATION;
            node->as.operation.oper = self()->type;
            node->as.operation.lhs = parsePrint();
            size_t savedPtr = tokenPtr;

            if (tokens[tokenPtr].type == LINE_END 
                || tokens[tokenPtr].type == CTRL_THN 
                || tokens[tokenPtr].type == PARN_CLS)
            {
                node->as.operation.rhs = NULL;
                return node;
            }

            if (match((TokenType[]){ LOOP_WHL }, 1))
            {
                node->as.operation.oper = self()->type;
                node->as.operation.rhs = parsePrint();
                return node;
            }

            node->as.operation.rhs = parsePrint();
            if (match((TokenType[]){ LOOP_TIM }, 1))
            {
                node->as.operation.oper = self()->type;
                return node;
            }

            freeOperTree(node->as.operation.rhs);
            node->as.operation.rhs = NULL;
            tokenPtr = savedPtr;

            return node;
        };

        return parsePrint();
    };

    Expression *(^parseWhen)() = ^Expression*
    {   
        if (match((TokenType[]){ CTRL_WHN }, 1))
        {
            Expression *node = calloc(1, sizeof(Expression));
            if (node == NULL) return NULL;

            node->type = OPERATION;
            node->as.operation.oper = self()->type;
            node->as.operation.lhs = parseDo();
            node->as.operation.rhs = parseDo();

            return node;
        }

        return parseDo();
    };

    Expression *(^__block parseThen)() = ^Expression*
    {
        Expression *expr = calloc(1, sizeof(Expression));
        if (expr == NULL) return NULL;
        expr = parseWhen();
        
        if (match((TokenType[]){ CTRL_THN }, 1))
        {
            while (match((TokenType[]){ CTRL_THN }, 1));
            
            Expression *node = calloc(1, sizeof(Expression));
            if (node == NULL) goto err;

            node->type = OPERATION;
            node->as.operation.oper = self()->type;
            node->as.operation.lhs = expr;
            node->as.operation.rhs = parseThen();

            return node;
        }

        return expr;
    err:
        freeOperTree(expr);
        return NULL;
    };

    parseExpr = ^Expression*{ 
        if (tokens[tokenPtr].type == SIGN_END) return NULL;

        if (match((TokenType[]){ LINE_STR }, 1))
        {
            if (match((TokenType[]){ LINE_END }, 1))
                return parseExpr();

            Expression *expr = parseExpr();

            expect(LINE_END, "line not ended");
            expr->nextLine = parseExpr();

            return expr;
        } else return parseThen();
    };

    Expression *expr = parseExpr();
    free(tokens);
    return expr;
}

void lispify(Expression *expr)
{
    static const char *tokenToLisp[] = {
        [OPER_PLS] = "+",
        [OPER_MIN] = "-",
        [OPER_MUL] = "*",
        [OPER_DIV] = "/",
        [OPER_EXP] = "expt",
        [OPER_EQL] = "=",
        [OPER_LOG] = "log",
        [OPER_MOD] = "%",
        [OPER_MOR] = ">",
        [OPER_LES] = "<",
        [UNRY_NEG] = "-",
        [UNRY_INV] = "!",
        [PARN_OPN] = "(",
        [PARN_CLS] = ")",
        [LITR_STR] = "\"",
        [EXPR_ASG] = "let",
        [EXPR_MUT] = "become",
        [EXPR_FUN] = "defun",
        [IIOO_OUT] = "say",
        [CTRL_THN] = "then",
        [CTRL_DOX] = "do",
        [CTRL_WHN] = "when",
        [LOOP_WHL] = "while",
        [LOOP_TIM] = "dotimes"
    };

    if (expr == NULL) 
    {
        printf("NULL\n");
        return;
    }

    char *(^__block display)(Expression*) = ^char*(Expression *expr){
        switch (expr->type) {
            case OPERATION:
            {
                char *buff = calloc(8192, 1);
                char buff2[8192];
                TokenType oper = expr->as.operation.oper;
                char *lhs = expr->as.operation.lhs 
                            ? display(expr->as.operation.lhs) 
                            : strdup("");
                char *rhs = expr->as.operation.rhs
                            ? display(expr->as.operation.rhs)
                            : strdup("");
                sprintf(buff, "(%s ", tokenToLisp[oper]);
                sprintf(buff2, "%s%s%s)", lhs, expr->as.operation.lhs && expr->as.operation.rhs ? " " : "", rhs);
                strcat(buff, buff2);

                free(lhs);
                free(rhs);
                return buff;
            }
            case PARENTHESES: return display(expr->as.parentheses);
            case NUM:
            {
                char *buff = malloc(8192);
                if ((int)expr->as.num == expr->as.num)
                    sprintf(buff,"%d", (int)expr->as.num);
                else sprintf(buff,"%Lg", expr->as.num);
                return buff;
            }
            case STRING: 
            {
                char *buff = malloc(8192);
                sprintf(buff,"\"%s\"", expr->as.string);
                return buff;
            }
            case IDENTITY:
            {
                char *buff = malloc(8192);
                sprintf(buff, "%s", expr->as.identity);
                return buff;
            }
            default: unreachable();
        };
    };

    char *lisp = display(expr);
    char *acc = malloc(8192);
    strcpy(acc, lisp);
    printf("%s\n", acc);
    free(lisp);
    free(acc);
    if (expr->nextLine) lispify(expr->nextLine);
}

void evaluate(Expression* expr)
{
    __block size_t varCount = 0;
    __block size_t varCap = 2;
    __block Identity *variables = malloc(varCap * sizeof(Identity));
    __block bool failedResize = false;

    Expression* (^__block copyExpr)(Expression*) = ^Expression*(Expression *e)
    {
        if (!e) return NULL;
        Expression *c = calloc(1, sizeof(Expression));
        c->type     = e->type;
        c->nextLine = NULL;

        switch (e->type)
        {
            case NUM:         c->as.num         = e->as.num;                    break;
            case STRING:      c->as.string      = strdup(e->as.string);         break;
            case IDENTITY:    c->as.identity    = strdup(e->as.identity);       break;
            case PARENTHESES: c->as.parentheses = copyExpr(e->as.parentheses);  break;
            case OPERATION:
                c->as.operation.oper = e->as.operation.oper;
                c->as.operation.lhs  = copyExpr(e->as.operation.lhs);
                c->as.operation.rhs  = copyExpr(e->as.operation.rhs);
            break;
        }

        return c;
    };

    auto addVar = ^(char *key, Expression* val)
    {
        for (size_t i = 0; i < varCount; i++)
            if (strcmp(variables[i].key, key) == 0)
            {
                char err[strlen(key) + 64];
                sprintf(err, "'%s' already defined.\nhint: did you mean to use 'is now'?", key);
                error(err);
            }
        
        if (varCount >= varCap)
        {
            varCap *= 2;
            Identity *temp = realloc(variables, varCap * sizeof(Identity));
            if (!temp) 
            {
                free(variables);
                variables = NULL;
                failedResize = true;
                return;
            }
            variables = temp;
        }
        
        variables[varCount] = (Identity) {
            .key = key,
            .val = copyExpr(val)
        };
        varCount++;
    };

    auto getVar = ^Expression*(char *key)
    {
        for (size_t i = 0; i < varCount; i++)
            if (strcmp(variables[i].key, key) == 0)  
                return copyExpr(variables[i].val);
        char err[strlen(key) + 64];
        sprintf(err, "variable \"%s\" undefined", key);
        error(err);
        return NULL;
    };

    auto setVar = ^(char *key, Expression *val)
    {
        for (size_t i = 0; i < varCount; i++)
            if (strcmp(variables[i].key, key) == 0)  
            {
                variables[i].val = copyExpr(val);
                return;
            }
        
        char err[strlen(key) + 64];
        sprintf(err, "variable \"%s\" undefined", key);
        error(err);
    };
    
    Expression* (^__block eval)(Expression*) = ^Expression*(Expression *expr)
    {
        if (!expr) return NULL;
        if (!variables) return NULL;

        switch (expr->type)
        {
            case OPERATION:
                Expression *lhs = 
                expr->as.operation.oper == CTRL_DOX 
                || expr->as.operation.oper == EXPR_ASG 
                || expr->as.operation.oper == EXPR_MUT
                || expr->as.operation.oper == EXPR_FUN 
                || expr->as.operation.oper == LOOP_TIM 
                || expr->as.operation.oper == LOOP_WHL 
                    ? expr->as.operation.lhs 
                    : eval(expr->as.operation.lhs); 

                Expression *rhs = 
                expr->as.operation.oper == CTRL_WHN
                || expr->as.operation.oper == LOOP_WHL 
                || expr->as.operation.oper == EXPR_FUN 
                    ? expr->as.operation.rhs
                    : eval(expr->as.operation.rhs);

                Expression *result = calloc(1, sizeof(Expression));
                result->nextLine = expr->nextLine;
                if ((lhs && lhs->type == STRING) || (rhs && rhs->type == STRING))
                    result->type = STRING;
                else result->type = NUM;

                switch (expr->as.operation.oper)
                {
                    case EXPR_ASG:
                        if (lhs->type != IDENTITY)
                            error("you can only assign to variables.");

                        addVar(lhs->as.identity, rhs); 
                        if (failedResize) return NULL;

                        result = copyExpr(rhs);
                    break;
                    case EXPR_MUT:
                        if (lhs->type != IDENTITY)
                            error("you can only assign to variables.");
                        
                        setVar(lhs->as.identity, rhs); 

                        result = copyExpr(rhs);
                    break;
                    case EXPR_FUN:
                        if (lhs->type != IDENTITY)
                            error("you can only assign to variables.");
                        
                        addVar(lhs->as.identity, rhs); 

                        result = copyExpr(rhs);
                    break;
                    case IIOO_OUT:
                        switch (rhs->type)
                        {
                            case NUM: printf("%Lg", rhs->as.num);       break;
                            case STRING: printf("%s", rhs->as.string);  break;
                            default: unreachable();
                        }
                        result = copyExpr(rhs);
                    break;
                    case LOOP_TIM:
                        if (rhs->type != NUM)
                            error("condition must be a number (boolean).");

                        for (size_t i = 0; i < rhs->as.num; i++)
                            result = eval(copyExpr(lhs));
                    break;
                    case LOOP_WHL:
                        while (rhs && eval(copyExpr(rhs))->as.num)
                            result = eval(copyExpr(lhs));
                    break;
                    case OPER_PLS:
                        if (result->type == STRING)
                        {
                            char *buff = calloc(8192, 1);

                            switch (lhs->type)
                            {
                                case NUM: sprintf(buff, "%Lg%s", lhs->as.num, rhs->as.string);  break;
                                case STRING:
                                    switch (rhs->type)
                                    {
                                        case NUM: sprintf(buff, "%s%Lg", lhs->as.string, rhs->as.num);      break;
                                        case STRING: sprintf(buff, "%s%s", lhs->as.string, rhs->as.string); break;
                                        default: unreachable();
                                    }
                                break;
                                default: unreachable();
                            } 
                            
                            result->as.string = buff;
                        }
                        else result->as.num = (lhs->as.num + rhs->as.num);
                    break;
                    case CTRL_WHN: 
                        if (lhs->type != NUM)
                            error("condition must be a number (boolean).");

                        if (lhs->as.num)
                        {
                            result = eval(rhs);
                            rhs = NULL;
                        }
                        else result = copyExpr(lhs);
                    break;
                    case CTRL_DOX: 
                        result = lhs->type == IDENTITY ? eval(lhs) : lhs;
                        result = eval(result);
                        lhs = NULL;            
                    break;
                    case CTRL_THN: result = copyExpr(rhs);                          break;
                    case OPER_EQL: result->as.num = (lhs->as.num == rhs->as.num);   break;
                    case OPER_MIN: result->as.num = (lhs->as.num - rhs->as.num);    break;
                    case OPER_MUL: result->as.num = (lhs->as.num * rhs->as.num);    break;
                    case OPER_DIV: result->as.num = (lhs->as.num / rhs->as.num);    break;
                    case OPER_MOD: result->as.num = 
                        (long double)((int)lhs->as.num % (int)rhs->as.num);        break;
                    case OPER_EXP: result->as.num = powl(lhs->as.num, rhs->as.num); break;
                    case OPER_LOG: 
                        result->as.num = log10l(rhs->as.num) / log10l(lhs->as.num); break;
                    case OPER_MOR: result->as.num = lhs->as.num > rhs->as.num;      break;
                    case OPER_LES: result->as.num = lhs->as.num < rhs->as.num;      break;
                    case UNRY_INV: result->as.num = !rhs->as.num;                   break;
                    case UNRY_NEG: result->as.num = -rhs->as.num;                   break;
                    default: unreachable();
                }
                
                if (lhs) free(lhs);
                if (rhs) free(rhs);
                result->nextLine = expr->nextLine;
                free(expr);
                return result;
            case IDENTITY:      return getVar(expr->as.identity);
            case PARENTHESES:   return eval(expr->as.parentheses);
            case NUM:           return expr;
            case STRING:        return expr;
        }
    };

    Expression *result = eval(expr);

    while (result->nextLine)
        result = eval(result->nextLine);

    if (!failedResize) free(variables);
}

int main(int argc, char *argv[])
{
    bool debugFlag = false;

    if (argc == 1) error("no input");
    FILE *klang = NULL;

    if (argc == 3 && !strcmp(argv[2], "--debug")) 
        debugFlag = true;

    klang = fopen(argv[1], "r");
    if (!klang) 
    {
        perror("");
        return 1;
    }

    fseek(klang, 0, SEEK_END);
    long size = ftell(klang);
    rewind(klang);

    char *buff = calloc(size + 1, 1);
    if (buff == NULL) 
    {
        fclose(klang);
        return 1;
    }
    char lineBuff[1024];

    while (fgets(lineBuff, sizeof(lineBuff), klang))
        strcat(buff, lineBuff);

    fclose(klang);

    Token *tokens = tokenize(buff);
    if (DEBUG || debugFlag) displayTokens(tokens);
    Expression *parsed = parser(tokens);
    if (DEBUG || debugFlag) lispify(parsed);
    evaluate(parsed);
    printf("\n");

    free(tokens);

    return 0;
}