# 从零开始的 JSON 库教程（九）：性能优化与基准测试

* [你的名字]
* 2025/12/11

本文是《从零开始的 JSON 库教程》的第九个单元。代码位于 [json-tutorial/tutorial09](https://github.com/你的用户名/json-tutorial/tree/master/tutorial09)。

本单元内容：

1. [完善 API 接口](#1-完善-api-接口)
2. [添加解析选项支持](#2-添加解析选项支持)
3. [内存分配器抽象](#3-内存分配器抽象)
4. [性能优化技巧](#4-性能优化技巧)
5. [集成到 nativejson-benchmark](#5-集成到-nativejson-benchmark)
6. [与 RapidJSON 对比分析](#6-与-rapidjson-对比分析)
7. [总结与展望](#7-总结与展望)

## 1. 完善 API 接口

为了能够参与 nativejson-benchmark 测试，我们需要完善现有的 API 接口。benchmark 测试框架通常需要统一的接口签名。

### 1.1 添加缺失的类型检查函数

当前我们的库只有基本的类型获取函数，但缺少布尔判断函数：

```c
/* leptjson.h */
int lept_is_null(const lept_value* v);
int lept_is_true(const lept_value* v);
int lept_is_false(const lept_value* v);
int lept_is_bool(const lept_value* v);
int lept_is_number(const lept_value* v);
int lept_is_string(const lept_value* v);
int lept_is_array(const lept_value* v);
int lept_is_object(const lept_value* v);

/* leptjson.c */
int lept_is_null(const lept_value* v) {
    assert(v != NULL);
    return v->type == LEPT_NULL;
}

int lept_is_true(const lept_value* v) {
    assert(v != NULL);
    return v->type == LEPT_TRUE;
}

int lept_is_false(const lept_value* v) {
    assert(v != NULL);
    return v->type == LEPT_FALSE;
}

int lept_is_bool(const lept_value* v) {
    assert(v != NULL);
    return v->type == LEPT_TRUE || v->type == LEPT_FALSE;
}

int lept_is_number(const lept_value* v) {
    assert(v != NULL);
    return v->type == LEPT_NUMBER;
}

int lept_is_string(const lept_value* v) {
    assert(v != NULL);
    return v->type == LEPT_STRING;
}

int lept_is_array(const lept_value* v) {
    assert(v != NULL);
    return v->type == LEPT_ARRAY;
}

int lept_is_object(const lept_value* v) {
    assert(v != NULL);
    return v->type == LEPT_OBJECT;
}
```

### 1.2 添加带长度参数的解析函数

nativejson-benchmark 通常使用带长度参数的解析函数：

```c
/* leptjson.h */
int lept_parse_len(lept_value* v, const char* json, size_t length);

/* leptjson.c */
int lept_parse_len(lept_value* v, const char* json, size_t length) {
    assert(v != NULL);
    assert(json != NULL);
    
    /* 创建以 null 结尾的字符串副本 */
    char* json_copy = (char*)malloc(length + 1);
    if (!json_copy) return LEPT_PARSE_INVALID_VALUE;
    
    memcpy(json_copy, json, length);
    json_copy[length] = '\0';
    
    int ret = lept_parse(v, json_copy);
    free(json_copy);
    return ret;
}
```

### 1.3 添加错误信息获取

为了更好的调试体验，我们添加错误信息获取功能：

```c
/* leptjson.h */
const char* lept_get_error_message(int error_code);

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
```

## 2. 添加解析选项支持

为了进行更全面的测试，我们支持不同的解析选项：

```c
/* leptjson.h */
typedef enum {
    LEPT_PARSE_DEFAULT = 0,
    LEPT_PARSE_ALLOW_COMMENTS = 1 << 0,      /* 允许注释 */
    LEPT_PARSE_ALLOW_TRAILING_COMMAS = 1 << 1, /* 允许尾随逗号 */
    LEPT_PARSE_ALLOW_NAN_AND_INF = 1 << 2,   /* 允许 NaN 和 Infinity */
    LEPT_PARSE_PRECISE_FLOAT = 1 << 3,       /* 精确浮点数解析 */
} lept_parse_option;

int lept_parse_with_opts(lept_value* v, const char* json, lept_parse_option opts);
```

### 测试驱动开发

让我们先编写测试用例，然后实现功能：

```c
/* test.c - 新增测试函数 */
static void test_parse_options() {
    lept_value v;
    
    /* 测试默认选项 */
    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK, lept_parse(&v, "123"));
    EXPECT_EQ_INT(LEPT_NUMBER, lept_get_type(&v));
    EXPECT_EQ_DOUBLE(123.0, lept_get_number(&v));
    lept_free(&v);
    
    /* 测试允许 NaN 和 Infinity */
#if 0  /* 待实现 */
    lept_init(&v);
    EXPECT_EQ_INT(LEPT_PARSE_OK, 
        lept_parse_with_opts(&v, "NaN", LEPT_PARSE_ALLOW_NAN_AND_INF));
    EXPECT_TRUE(isnan(lept_get_number(&v)));
    lept_free(&v);
#endif
    
    printf("test_parse_options: pass\n");
}
```

## 3. 内存分配器抽象

为了性能优化和灵活性，我们引入内存分配器抽象：

```c
/* leptjson.h */
typedef struct lept_context lept_context;

typedef void* (*lept_malloc_fn)(size_t size, void* userdata);
typedef void (*lept_free_fn)(void* ptr, void* userdata);
typedef void* (*lept_realloc_fn)(void* ptr, size_t size, void* userdata);

struct lept_context {
    lept_malloc_fn malloc_fn;
    lept_free_fn free_fn;
    lept_realloc_fn realloc_fn;
    void* userdata;
};

void lept_set_context(lept_context* ctx);
lept_context* lept_get_context(void);
```

### 3.1 实现默认分配器

```c
/* leptjson.c */
static lept_context global_context = {
    malloc,
    free,
    realloc,
    NULL
};

static lept_context* current_context = &global_context;

void lept_set_context(lept_context* ctx) {
    if (ctx == NULL) {
        current_context = &global_context;
    } else {
        current_context = ctx;
    }
}

lept_context* lept_get_context(void) {
    return current_context;
}

/* 替换所有 malloc/free/realloc 调用 */
#define LEPT_MALLOC(size) \
    current_context->malloc_fn((size), current_context->userdata)

#define LEPT_FREE(ptr) \
    do { \
        if (ptr) \
            current_context->free_fn((ptr), current_context->userdata); \
    } while(0)

#define LEPT_REALLOC(ptr, size) \
    current_context->realloc_fn((ptr), (size), current_context->userdata)
```

## 4. 性能优化技巧

### 4.1 SIMD 优化字符串解析

对于现代 CPU，我们可以使用 SIMD 指令加速字符串处理：

```c
#ifdef __SSE2__
#include <emmintrin.h>

/* 快速检查字符串是否包含控制字符 */
static int lept_check_string_sse2(const char* p, size_t len) {
    __m128i zero = _mm_set1_epi8(0);
    __m128i space = _mm_set1_epi8(0x20);
    
    size_t i = 0;
    for (; i + 16 <= len; i += 16) {
        __m128i s = _mm_loadu_si128((const __m128i*)(p + i));
        __m128i lt_space = _mm_cmplt_epi8(s, space);
        __m128i eq_zero = _mm_cmpeq_epi8(s, zero);
        __m128i result = _mm_or_si128(lt_space, eq_zero);
        if (!_mm_testz_si128(result, result)) {
            return 0; /* 发现控制字符或空字符 */
        }
    }
    
    /* 处理剩余字节 */
    for (; i < len; i++) {
        if ((unsigned char)p[i] < 0x20 || p[i] == '\0')
            return 0;
    }
    
    return 1;
}
#endif
```

### 4.2 优化数字解析

使用更快的浮点数解析算法：

```c
static int lept_parse_number_fast(lept_context* c, lept_value* v) {
    const char* p = c->json;
    
    /* 快速检查是否为整数 */
    int is_integer = 1;
    const char* q = p;
    
    if (*q == '-') q++;
    while (*q) {
        if (*q == '.' || *q == 'e' || *q == 'E') {
            is_integer = 0;
            break;
        }
        if (!ISDIGIT(*q)) break;
        q++;
    }
    
    if (is_integer) {
        /* 使用整数解析，更快 */
        long long integer = 0;
        int sign = 1;
        
        if (*p == '-') {
            sign = -1;
            p++;
        }
        
        while (ISDIGIT(*p)) {
            if (integer > LLONG_MAX / 10) {
                /* 溢出，回退到标准解析 */
                return lept_parse_number(c, v);
            }
            integer = integer * 10 + (*p++ - '0');
        }
        
        v->u.n = (double)(integer * sign);
        v->type = LEPT_NUMBER;
        c->json = p;
        return LEPT_PARSE_OK;
    }
    
    /* 否则使用标准解析 */
    return lept_parse_number(c, v);
}
```

## 5. 集成到 nativejson-benchmark

### 5.1 创建适配器文件

我们需要创建一个适配器文件，将我们的库接口适配到 nativejson-benchmark：

```c
/* test/perf_leptjson.cpp */
#include "nativejson-benchmark/test.h"
#include "leptjson.h"

class LeptJson : public NativeJsonBenchmark {
public:
    virtual const char* GetName() const { return "leptjson"; }
    virtual const char* GetVersion() const { return "1.0.0"; }
    virtual const char* GetFilename() const { return __FILE__; }
    
    virtual bool Parse(const char* json, size_t length, std::string& result) {
        lept_value v;
        lept_init(&v);
        
        int ret = lept_parse_len(&v, json, length);
        if (ret != LEPT_PARSE_OK) {
            lept_free(&v);
            return false;
        }
        
        size_t len;
        char* str = lept_stringify(&v, &len);
        if (str) {
            result.assign(str, len);
            free(str);
        }
        
        lept_free(&v);
        return true;
    }
    
    virtual bool Stringify(const char* json, size_t length, std::string& result) {
        lept_value v;
        lept_init(&v);
        
        int ret = lept_parse_len(&v, json, length);
        if (ret != LEPT_PARSE_OK) {
            lept_free(&v);
            return false;
        }
        
        size_t len;
        char* str = lept_stringify(&v, &len);
        if (str) {
            result.assign(str, len);
            free(str);
        }
        
        lept_free(&v);
        return true;
    }
};

REGISTER_BENCHMARK(LeptJson);
```

### 5.2 编写性能测试

```c
/* test/benchmark.c */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "leptjson.h"

#define TEST_ROUNDS 10000
#define TEST_JSON "{\"string\":\"Hello, World!\",\"number\":123.456,\"array\":[1,2,3],\"object\":{\"key\":\"value\"}}"

static void benchmark_parse(void) {
    lept_value v;
    clock_t start, end;
    int i, success = 0;
    
    start = clock();
    for (i = 0; i < TEST_ROUNDS; i++) {
        lept_init(&v);
        if (lept_parse(&v, TEST_JSON) == LEPT_PARSE_OK) {
            success++;
        }
        lept_free(&v);
    }
    end = clock();
    
    double time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Parse benchmark:\n");
    printf("  Rounds: %d\n", TEST_ROUNDS);
    printf("  Success: %d\n", success);
    printf("  Time: %.3f seconds\n", time_used);
    printf("  Speed: %.0f ops/sec\n", TEST_ROUNDS / time_used);
}

static void benchmark_stringify(void) {
    lept_value v;
    char* json;
    size_t len;
    clock_t start, end;
    int i;
    
    lept_init(&v);
    lept_parse(&v, TEST_JSON);
    
    start = clock();
    for (i = 0; i < TEST_ROUNDS; i++) {
        json = lept_stringify(&v, &len);
        free(json);
    }
    end = clock();
    
    double time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    lept_free(&v);
    
    printf("Stringify benchmark:\n");
    printf("  Rounds: %d\n", TEST_ROUNDS);
    printf("  Time: %.3f seconds\n", time_used);
    printf("  Speed: %.0f ops/sec\n", TEST_ROUNDS / time_used);
}

int main(void) {
    printf("leptjson Performance Benchmark\n");
    printf("===============================\n\n");
    
    benchmark_parse();
    printf("\n");
    benchmark_stringify();
    
    return 0;
}
```

## 6. 与 RapidJSON 对比分析

让我们创建一个对比测试：

```c
/* test/comparison.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef USE_RAPIDJSON
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#endif

#include "leptjson.h"

#define TEST_COUNT 1000

static const char* test_jsons[] = {
    /* 简单对象 */
    "{\"key\":\"value\"}",
    /* 嵌套对象 */
    "{\"a\":{\"b\":{\"c\":123}}}",
    /* 大数组 */
    "[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]",
    /* 混合类型 */
    "{\"string\":\"test\",\"number\":123.456,\"bool\":true,\"null\":null,\"array\":[1,2,3]}",
    NULL
};

static void run_leptjson_test(const char* json) {
    lept_value v;
    int i;
    clock_t start, end;
    
    start = clock();
    for (i = 0; i < TEST_COUNT; i++) {
        lept_init(&v);
        lept_parse(&v, json);
        lept_free(&v);
    }
    end = clock();
    
    printf("leptjson: %.3f ms\n", 
        ((double)(end - start) * 1000) / CLOCKS_PER_SEC / TEST_COUNT);
}

#ifdef USE_RAPIDJSON
static void run_rapidjson_test(const char* json) {
    using namespace rapidjson;
    int i;
    clock_t start, end;
    
    start = clock();
    for (i = 0; i < TEST_COUNT; i++) {
        Document d;
        d.Parse(json);
    }
    end = clock();
    
    printf("RapidJSON: %.3f ms\n", 
        ((double)(end - start) * 1000) / CLOCKS_PER_SEC / TEST_COUNT);
}
#endif

int main(void) {
    int i = 0;
    
    printf("JSON Library Comparison Test\n");
    printf("=============================\n\n");
    
    while (test_jsons[i]) {
        printf("Test %d (length: %zu):\n", 
            i + 1, strlen(test_jsons[i]));
        printf("  JSON: %.40s...\n", test_jsons[i]);
        
        run_leptjson_test(test_jsons[i]);
        
#ifdef USE_RAPIDJSON
        run_rapidjson_test(test_jsons[i]);
#endif
        
        printf("\n");
        i++;
    }
    
    return 0;
}
```

## 7. 总结与展望

### 7.1 性能测试结果分析

通过基准测试，你可能会发现：

1. **解析速度**：leptjson 在简单 JSON 上可能有不错的表现，但在复杂嵌套结构上可能不如 RapidJSON
2. **内存使用**：由于我们的简单设计，内存使用可能比优化过的库更高
3. **功能完整性**：缺少一些高级功能（如注释支持、schema 验证等）

### 7.2 可能的优化方向

1. **更高效的内存分配策略**：
   ```c
   /* 使用内存池 */
   typedef struct {
       char* buffer;
       size_t size;
       size_t capacity;
   } lept_buffer;
   ```

2. **解析器状态机优化**：
   ```c
   /* 使用查表法加速字符分类 */
   static const unsigned char type_table[256] = {
       ['t'] = TYPE_TRUE,
       ['f'] = TYPE_FALSE,
       ['n'] = TYPE_NULL,
       ['"'] = TYPE_STRING,
       ['0'...'9'] = TYPE_NUMBER,
       ['-'] = TYPE_NUMBER,
       ['['] = TYPE_ARRAY,
       ['{'] = TYPE_OBJECT
   };
   ```

3. **流式解析支持**：
   ```c
   typedef struct {
       lept_parse_state state;
       lept_value* current;
       /* ... */
   } lept_stream_parser;
   
   int lept_parse_chunk(lept_stream_parser* parser, 
                       const char* chunk, size_t length);
   ```

### 7.3 练习内容

1. 实现 `lept_parse_with_opts()` 函数，支持不同的解析选项
2. 为 leptjson 添加内存池分配器，测试性能提升
3. 将 leptjson 集成到 nativejson-benchmark，生成性能报告
4. 尝试实现 SIMD 优化的字符串验证函数
5. 添加 JSON Patch 和 JSON Pointer 支持

### 7.4 结语

恭喜你完成了从零开始实现 JSON 库的旅程！通过这个教程，你不仅学会了如何实现一个功能完整的 JSON 解析器和生成器，还深入了解了：

- 递归下降解析器的设计与实现
- Unicode 和 UTF-8 编码处理
- 动态数据结构（数组和对象）
- 内存管理和资源所有权
- 性能优化技巧
- 测试驱动开发

虽然我们的 leptjson 在性能上可能无法与工业级的 RapidJSON 媲美，但它简洁、易懂，是一个优秀的学习项目。你可以在此基础上继续扩展，比如添加 JSON Schema 验证、JSONPath 查询、流式解析等高级功能。

记住，优秀的软件不是一蹴而就的，而是通过不断迭代、测试和优化形成的。继续探索，继续编码！

**"Talk is cheap. Show me the code." - Linus Torvalds**