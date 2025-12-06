#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL, strtod() */

#include<errno.h>
#include<math.h>

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)

#define ISDIGIT(ch) ((ch) >= '0' && (ch <= '9'))
#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')

typedef struct {
    const char* json;
}lept_context;

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}


static int lept_parse_literal(lept_context* c, lept_value* v,char ch){
    switch (ch) {
        case 't':{
            c->json ++;
            if (c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e')
                return LEPT_PARSE_INVALID_VALUE;
            c->json += 3;
            v->type = LEPT_TRUE;
            break; /*跳出switch*/
        }
            
        case 'f':{
            c->json ++;
            if (c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's' || c->json[3] != 'e')
                return LEPT_PARSE_INVALID_VALUE;
            c->json += 4;
            v->type = LEPT_FALSE;
            break;
        }
        case 'n':{
            c->json ++;
            if (c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')
                return LEPT_PARSE_INVALID_VALUE;
            c->json += 3;
            v->type = LEPT_NULL;
            break;
        }
        default: return LEPT_PARSE_INVALID_VALUE;
    }
    
    return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_context* c, lept_value* v) {
    /* \TODO validate number */

    /*负数 整数 小数 指数*/
    const char * p = c->json;

    if(*p == '-') p++;

    if(*p == '0'){
        p++; 
        /*跳过0*/
    }else{
        if(!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;
        for(p++; ISDIGIT(*p); p++);
    }

    if(*p == '.'){
        p++;
        if(!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for(p++; ISDIGIT(*p); p++);
    }

    if(*p == 'e' || *p == 'E'){
        p++;
        if(*p == '-' || *p == '+'){
            p++;
        }
        if(!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for(p++; ISDIGIT(*p); p++);        
    }
    
 
    
    /*利用标准库将十进制转换成二级制double */
    errno = 0;
    v->n = strtod(c->json, NULL);
    /*检测是不是太大了 */  
    if(errno == ERANGE && ((v->n == HUGE_VAL) || (v->n == - HUGE_VAL))){
        return LEPT_PARSE_NUMBER_TOO_BIG;
    }
    c->json = p;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    
    switch (*c->json) {
        case 't':  return lept_parse_literal(c, v,'t'); 
        case 'f':  return lept_parse_literal(c, v,'f'); 
        case 'n':  return lept_parse_literal(c, v,'n');
        default:   return lept_parse_number(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->n;
}
