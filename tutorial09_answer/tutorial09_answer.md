# 从零开始的 JSON 库教程（九）：终点与新开始 解答篇

本文是[《从零开始的 JSON 库教程》](https://zhuanlan.zhihu.com/json-tutorial)第九个单元练习的解答。解答代码位于 [json-tutorial/tutorial09_answer](https://github.com/miloyip/json-tutorial/blob/master/tutorial09_answer)。

## 练习解答

### 实现 `lept_parse_with_opts()`

要支持解析选项，我们在 `lept_context` 中加入 `opts` 字段：

~~~c
typedef struct {
    const char* json;
    char* stack;
    size_t size, top;
    lept_parse_option opts;
} lept_context;
~~~

在 `lept_parse_value()` 中，检查 `LEPT_PARSE_ALLOW_NAN_AND_INF` 选项，匹配 `NaN`、`Infinity`、`-Infinity`：

~~~c
static int lept_parse_value(lept_context* c, lept_value* v) {
    if (c->opts & LEPT_PARSE_ALLOW_NAN_AND_INF) {
        if (strncmp(c->json, "NaN", 3) == 0) {
            c->json += 3;
            v->u.n = NAN;
            v->type = LEPT_NUMBER;
            return LEPT_PARSE_OK;
        }
        if (strncmp(c->json, "Infinity", 8) == 0) {
            c->json += 8;
            v->u.n = INFINITY;
            v->type = LEPT_NUMBER;
            return LEPT_PARSE_OK;
        }
        if (strncmp(c->json, "-Infinity", 9) == 0) {
            c->json += 9;
            v->u.n = -INFINITY;
            v->type = LEPT_NUMBER;
            return LEPT_PARSE_OK;
        }
    }
    switch (*c->json) { /* ... */ }
}
~~~

`lept_parse_with_opts()` 的实现与 `lept_parse()` 几乎一样，只是多传了 `opts` 到上下文中。

完整代码请参考本目录下的 `leptjson.c`。
