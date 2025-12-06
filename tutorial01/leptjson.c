#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL */

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)

typedef struct {
    const char* json;
}lept_context;

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    /*直接跳过特殊字符，没有返回值*/
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int lept_parse_null(lept_context* c, lept_value* v) {
    EXPECT(c, 'n'); /*判断是'n'之后就c->json 就++了，所以下面直接c->json[0] ++ */
    if (c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 3; /*直接跳过ull三个字符，然后给v->type置null*/
    v->type = LEPT_NULL;
    return LEPT_PARSE_OK; /*解析完毕了*/
}

static int lept_parse_true(lept_context* c, lept_value* v){
    EXPECT(c, 't'); /*判断是't'之后就c->json 就++了，所以下面直接c->json[0]*/
    if (c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 3; /*直接跳过rue三个字符，然后给v->type置true*/
    v->type = LEPT_TRUE;
    return LEPT_PARSE_OK; /*解析完毕了*/
}

static int lept_parse_false(lept_context* c, lept_value* v){
    EXPECT(c, 'f'); /*判断是'f'之后就c->json 就++了，所以下面直接c->json[0]*/
    if (c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's' || c->json[3] != 'e')
        return LEPT_PARSE_INVALID_VALUE;
    c->json += 4; /*直接跳过alse四个字符，然后给v->type置false*/
    v->type = LEPT_FALSE;
    return LEPT_PARSE_OK; /*解析完毕了*/
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 'n':  return lept_parse_null(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
        default:   return LEPT_PARSE_INVALID_VALUE; /*默认当作无效词解析*/
    }
}

/* value只有false和true还有null，此处我们直接补全*/
int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    assert(v != NULL); /*判断指针不为空*/
    c.json = json;
    v->type = LEPT_NULL;
    lept_parse_whitespace(&c); 
    int ret ;
    ret=  lept_parse_value(&c,v);
    /*空白之后如果还有其他字符，返回LEPT_PARSE_ROOT_NOT_SINGULAR */
    if(ret == LEPT_PARSE_OK){/*解析成功了*/
        /*继续跳过空白*/
        lept_parse_whitespace(&c); 
        if(*c.json != '\0') ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        v->type = LEPT_NULL; /*出错置为NULL*/
    }
    return ret;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}