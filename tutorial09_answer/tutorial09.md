# 从零开始的 JSON 库教程（九）：终点与新开始

* Milo Yip
* 2018/6/2

本文是[《从零开始的 JSON 库教程》](https://zhuanlan.zhihu.com/json-tutorial)的第九个单元，也是最后一个单元。代码位于 [json-tutorial/tutorial09](https://github.com/miloyip/json-tutorial/blob/master/tutorial09)。

经过前面八个单元的学习，我们已经实现了一个功能完整的 JSON 库 ── 包括解析器、生成器、以及对 JSON 值的各种访问和修改操作。本单元将作为整个教程的收尾：我们会完善一些 API 接口，加入解析选项支持，编写简单的性能测试，介绍如何集成到 [nativejson-benchmark](https://github.com/miloyip/nativejson-benchmark)，并回顾整个教程的历程。

本单元内容：

1. [完善 API 接口](#1-完善-api-接口)
2. [添加解析选项支持](#2-添加解析选项支持)
3. [编写性能测试](#3-编写性能测试)
4. [集成到 nativejson-benchmark](#4-集成到-nativejson-benchmark)
5. [总结与展望](#5-总结与展望)

## 1. 完善 API 接口

在将我们的库推向实际应用（或参与 benchmark）之前，有一些小的 API 缺口值得补上。

### 1.1 类型检查函数

目前我们获取值的类型需要调用 `lept_get_type()` 再与枚举比较。虽然这已经够用，但提供一组布尔判断函数可以让使用代码更简洁、可读性更好：

~~~c
/* leptjson.h */
int lept_is_null(const lept_value* v);
int lept_is_true(const lept_value* v);
int lept_is_false(const lept_value* v);
int lept_is_bool(const lept_value* v);
int lept_is_number(const lept_value* v);
int lept_is_string(const lept_value* v);
int lept_is_array(const lept_value* v);
int lept_is_object(const lept_value* v);
~~~

实现非常直接，以 `lept_is_null()` 为例：

~~~c
int lept_is_null(const lept_value* v) {
    assert(v != NULL);
    return v->type == LEPT_NULL;
}
~~~

其他函数类推。`lept_is_bool()` 稍有不同，它同时检查 `LEPT_TRUE` 和 `LEPT_FALSE`：

~~~c
int lept_is_bool(const lept_value* v) {
    assert(v != NULL);
    return v->type == LEPT_TRUE || v->type == LEPT_FALSE;
}
~~~

这组函数的价值不在于实现复杂度，而在于 API 的表达力。对比：

~~~c
/* 之前 */
if (lept_get_type(&v) == LEPT_STRING) { ... }

/* 之后 */
if (lept_is_string(&v)) { ... }
~~~

### 1.2 带长度参数的解析函数

标准的 `lept_parse()` 接受一个以空字符结尾的 C 字符串。但在许多场景中（例如从网络接收的数据、或文件映射的缓冲区），JSON 文本并不一定以 `'\0'` 结尾，而是以长度标记边界。为此，我们增加一个带长度参数的版本：

~~~c
/* leptjson.h */
int lept_parse_len(lept_value* v, const char* json, size_t length);
~~~

实现上，最简单的方式是复制一份并加上空字符：

~~~c
int lept_parse_len(lept_value* v, const char* json, size_t length) {
    int ret;
    char* json_copy;
    assert(v != NULL);
    assert(json != NULL);
    json_copy = (char*)malloc(length + 1);
    if (!json_copy) return LEPT_PARSE_INVALID_VALUE;
    memcpy(json_copy, json, length);
    json_copy[length] = '\0';
    ret = lept_parse(v, json_copy);
    free(json_copy);
    return ret;
}
~~~

这里的额外内存分配和复制是一个性能损耗。如果性能要求很高，可以修改解析器内部，让它在到达 `length` 位置时视为结束，而非依赖 `'\0'`。这可以作为一个优化练习。

### 1.3 错误信息获取

调试解析错误时，仅靠错误码不太方便。我们加一个把错误码转为可读字符串的函数：

~~~c
/* leptjson.h */
const char* lept_get_error_message(int error_code);
~~~

~~~c
/* leptjson.c */
static const char* error_messages[] = {
    "parse ok",
    "expect value",
    "invalid value",
    "root not singular",
    "number too big",
    "miss quotation mark",
    "invalid string escape",
    "invalid string char",
    "invalid unicode hex",
    "invalid unicode surrogate",
    "miss comma or square bracket",
    "miss key",
    "miss colon",
    "miss comma or curly bracket"
};

const char* lept_get_error_message(int error_code) {
    if (error_code >= 0 && error_code < sizeof(error_messages)/sizeof(error_messages[0]))
        return error_messages[error_code];
    return "unknown error";
}
~~~

这里用一个静态数组，通过错误码作为索引直接查表，时间复杂度 $\mathrm{O}(1)$。注意数组的顺序必须与错误码枚举一致。

## 2. 添加解析选项支持

标准 JSON 的语法是非常严格的 ── 不允许注释、不允许尾随逗号、不允许 `NaN` 和 `Infinity`。但在实际应用中，许多 JSON 解析器都提供了宽松模式。我们可以通过解析选项（parse options）来支持这些扩展，同时保持默认的严格行为。

### 2.1 定义选项

我们用位标志（bit flags）来定义选项：

~~~c
/* leptjson.h */
typedef enum {
    LEPT_PARSE_DEFAULT = 0,
    LEPT_PARSE_ALLOW_COMMENTS = 1 << 0,
    LEPT_PARSE_ALLOW_TRAILING_COMMAS = 1 << 1,
    LEPT_PARSE_ALLOW_NAN_AND_INF = 1 << 2,
    LEPT_PARSE_PRECISE_FLOAT = 1 << 3
} lept_parse_option;

int lept_parse_with_opts(lept_value* v, const char* json, lept_parse_option opts);
~~~

使用位标志的好处是可以用位或（`|`）组合多个选项：

~~~c
lept_parse_with_opts(&v, json,
    LEPT_PARSE_ALLOW_COMMENTS | LEPT_PARSE_ALLOW_TRAILING_COMMAS);
~~~

### 2.2 实现

要让解析器感知选项，我们在 `lept_context` 中加入 `opts` 字段：

~~~c
typedef struct {
    const char* json;
    char* stack;
    size_t size, top;
    lept_parse_option opts;
} lept_context;
~~~

然后在 `lept_parse_value()` 的开头，检查是否启用了 `LEPT_PARSE_ALLOW_NAN_AND_INF`。如果启用，则在分发到各解析函数之前，先尝试匹配 `NaN`、`Infinity`、`-Infinity` 这三个字面量：

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
    switch (*c->json) {
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        default:   return lept_parse_number(c, v);
        case '"':  return lept_parse_string(c, v);
        case '[':  return lept_parse_array(c, v);
        case '{':  return lept_parse_object(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}
~~~

`NAN` 和 `INFINITY` 是 C99 `<math.h>` 定义的宏。在不启用该选项时，`NaN` 会走到 `default` 分支的 `lept_parse_number()`，由于不符合数字语法，自然返回 `LEPT_PARSE_INVALID_VALUE`。

`lept_parse_with_opts()` 的实现与 `lept_parse()` 几乎一样，只是多传了 `opts`：

~~~c
int lept_parse_with_opts(lept_value* v, const char* json, lept_parse_option opts) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    c.opts = opts;
    lept_init(v);
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}
~~~

而原来的 `lept_parse()` 只需确保 `opts` 设为 `LEPT_PARSE_DEFAULT`：

~~~c
int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    /* ... */
    c.opts = LEPT_PARSE_DEFAULT;
    /* ... */
}
~~~

### 2.3 测试

我们为新功能编写测试：

~~~c
static void test_parse_options() {
    lept_value v;

    /* 默认不允许 NaN */
    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_INVALID_VALUE, lept_parse(&v, "NaN"));
    lept_free(&v);

    /* 启用选项后允许 NaN */
    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK,
        lept_parse_with_opts(&v, "NaN", LEPT_PARSE_ALLOW_NAN_AND_INF));
    EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(&v));
    lept_free(&v);

    /* Infinity 和 -Infinity */
    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK,
        lept_parse_with_opts(&v, "Infinity", LEPT_PARSE_ALLOW_NAN_AND_INF));
    EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(&v));
    lept_free(&v);

    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK,
        lept_parse_with_opts(&v, "-Infinity", LEPT_PARSE_ALLOW_NAN_AND_INF));
    EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(&v));
    lept_free(&v);
}
~~~

注意测试的策略：先验证默认行为（拒绝 `NaN`），再验证启用选项后的行为。这种正反对比测试可以确保选项开关确实起作用。

## 3. 编写性能测试

功能完备之后，一个自然的问题是：我们的库有多快？编写性能测试（benchmark）可以帮助我们了解库的性能特征，并为后续优化提供基准。

一个最简单的 benchmark 可以用 `clock()` 来计时：

~~~c
#include <time.h>

#define TEST_ROUNDS 10000
#define TEST_JSON \
    "{\"string\":\"Hello, World!\",\"number\":123.456," \
    "\"array\":[1,2,3],\"object\":{\"key\":\"value\"}}"

static void benchmark_parse(void) {
    lept_value v;
    clock_t start, end;
    int i, success = 0;

    start = clock();
    for (i = 0; i < TEST_ROUNDS; i++) {
        lept_init(&v);
        if (lept_parse(&v, TEST_JSON) == LEPT_PARSE_OK)
            success++;
        lept_free(&v);
    }
    end = clock();

    printf("Parse: %d rounds, %.3f sec, %.0f ops/sec\n",
        TEST_ROUNDS,
        (double)(end - start) / CLOCKS_PER_SEC,
        TEST_ROUNDS / ((double)(end - start) / CLOCKS_PER_SEC));
}
~~~

类似地可以写 `benchmark_stringify()`。读者可以把这段代码放到单独的 `benchmark.c` 中运行。

在真实的性能测试中，需要注意以下几点：

1. **预热**（warm up）：先跑几轮不计时的循环，让 CPU 缓存和分支预测器进入稳定状态。
2. **多次采样取中位数**：单次测量受系统调度影响大，多次采样后取中位数更稳定。
3. **使用足够大的输入**：小 JSON 的解析时间可能被函数调用开销主导，难以反映真实的解析性能。
4. **避免编译器优化掉被测代码**：确保解析结果被使用（例如累加 `success`），否则编译器可能将整个循环优化掉。

## 4. 集成到 nativejson-benchmark

[nativejson-benchmark](https://github.com/miloyip/nativejson-benchmark) 是一个比较 41 个开源原生 JSON 库的标准符合程度及性能的项目。如果想把 leptjson 加入其中，需要编写一个 C++ 适配器。

由于 leptjson 是纯 C 库，在 C++ 中可以直接 `#include`。适配器需要实现几个标准接口：

~~~cpp
/* leptjson_adapter.cpp（示意代码） */
extern "C" {
#include "leptjson.h"
}

class LeptJsonAdapter {
public:
    bool Parse(const char* json, size_t length) {
        lept_init(&value_);
        return lept_parse_len(&value_, json, length) == LEPT_PARSE_OK;
    }

    bool Stringify(std::string& result) {
        size_t len;
        char* str = lept_stringify(&value_, &len);
        if (str) {
            result.assign(str, len);
            free(str);
            return true;
        }
        return false;
    }

    ~LeptJsonAdapter() {
        lept_free(&value_);
    }

private:
    lept_value value_;
};
~~~

实际的 nativejson-benchmark 接口稍有不同，需要继承其测试基类并注册。有兴趣的读者可以参考项目中其他库的适配器代码。

集成后，可以用 nativejson-benchmark 提供的标准测试数据（如 `twitter.json`、`citm_catalog.json`）来评估 leptjson 的性能。预期结果：

- **标准符合度**：由于我们严格按照 RFC 7159 实现，标准符合度应该很高。
- **解析速度**：作为一个教学用途的库，我们没有做太多性能优化，速度可能不及 RapidJSON 等高度优化的库。
- **内存使用**：我们的数据结构比较朴素（每个值一个 `lept_value` 结构体，每个字符串单独 `malloc`），内存效率有提升空间。

## 5. 总结与展望

### 5.1 回顾

让我们回顾一下这 9 个单元走过的路：

| 单元 | 主题 | 关键知识点 |
|---|---|---|
| 1 | 启程 | TDD、递归下降解析器、宏技巧 |
| 2 | 解析数字 | 重构、ABNF 语法、`strtod()` |
| 3 | 解析字符串 | union、自动扩展栈、内存管理 |
| 4 | Unicode | UTF-8 编码、代理对 |
| 5 | 解析数组 | 复合类型、栈上临时存储 |
| 6 | 解析对象 | 数据结构选择、函数重构 |
| 7 | 生成器 | JSON 序列化、缓冲区复用 |
| 8 | 访问与修改 | 动态数组/对象、复制/移动/交换语义 |
| 9 | 终点与新开始 | API 完善、解析选项、性能测试 |

整个库的核心代码（`leptjson.c`）约 900 行，加上头文件约 120 行，非常适合作为学习材料。

### 5.2 可能的优化方向

如果你想继续在这个基础上探索，以下是一些值得尝试的方向：

1. **更高效的内存分配**：用内存池（memory pool）替代逐个 `malloc`，减少分配次数和碎片化。
2. **查表法加速字符分类**：用 256 字节的查找表替代 `switch` 分支，可以加速字符串解析中的字符分类。
3. **原地解析**（in-situ parsing）：字符串不另外分配内存，而是直接在输入缓冲区上修改（RapidJSON 支持此特性）。
4. **SIMD 优化**：用 SSE2/AVX2 指令并行处理字符串中的转义字符检测。
5. **流式解析**（streaming/SAX-style）：不构建完整的 DOM 树，而是通过回调逐事件处理，适合超大 JSON。

### 5.3 延伸阅读

- [RFC 7159 - The JavaScript Object Notation (JSON) Data Interchange Format](https://tools.ietf.org/html/rfc7159)
- [Parsing JSON is a Minefield](http://seriot.ch/parsing_json.php) ── 各 JSON 库在边界情况下的差异
- [RapidJSON](https://github.com/miloyip/rapidjson) ── 一个高性能的 C++ JSON 库，其设计思路值得学习

### 5.4 练习

1. 实现 `LEPT_PARSE_ALLOW_COMMENTS` 选项，支持 `//` 和 `/* */` 两种注释。
2. 实现 `LEPT_PARSE_ALLOW_TRAILING_COMMAS` 选项，允许数组和对象末尾的逗号（如 `[1,2,3,]`）。
3. 优化 `lept_parse_len()`，不复制输入数据，而是修改解析器使其支持长度限制。
4. 为 leptjson 添加 JSON Pointer（[RFC 6901](https://tools.ietf.org/html/rfc6901)）支持。
5. 尝试将 leptjson 集成到 nativejson-benchmark，生成性能报告。

### 5.5 结语

恭喜你完成了从零开始实现 JSON 库的旅程！通过这九个单元，你不仅学会了如何实现一个功能完整的 JSON 解析器和生成器，还深入实践了：

- 测试驱动开发
- 递归下降解析器的设计
- Unicode 和 UTF-8 编码处理
- 动态数据结构
- 内存管理与资源所有权
- API 设计

虽然 leptjson 在性能上无法与工业级的 RapidJSON 媲美，但它简洁、易懂，是一个优秀的学习项目。更重要的是，通过亲手实现每一个功能，你已经对 JSON 和 C 语言有了远比「使用现成库」更深的理解。

**"Talk is cheap. Show me the code." ── Linus Torvalds**
