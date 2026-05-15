# 从零开始的 JSON 库教程（八）：访问与其他功能 解答篇

本文是[《从零开始的 JSON 库教程》](https://zhuanlan.zhihu.com/json-tutorial)第八个单元练习的解答。解答代码位于 [json-tutorial/tutorial08_answer](https://github.com/miloyip/json-tutorial/blob/master/tutorial08_answer)。

## 练习解答

### 1. 完成对象比较

在 `lept_is_equal()` 中，对象比较需要考虑键值对的无序性。我们利用 `lept_find_object_index()` 在右侧对象中查找对应的键：

~~~c
case LEPT_OBJECT:
    if (lhs->u.o.size != rhs->u.o.size)
        return 0;
    for (i = 0; i < lhs->u.o.size; i++) {
        const char* key = lept_get_object_key(lhs, i);
        size_t klen = lept_get_object_key_length(lhs, i);
        size_t index = lept_find_object_index(rhs, key, klen);
        if (index == LEPT_KEY_NOT_EXIST)
            return 0;
        if (!lept_is_equal(&lhs->u.o.m[i].v, &rhs->u.o.m[index].v))
            return 0;
    }
    return 1;
~~~

### 2. 数组插入、删除、清空

`lept_insert_array_element()` 在指定位置插入元素，需要将后续元素后移：

~~~c
lept_value* lept_insert_array_element(lept_value* v, size_t index) {
    size_t i;
    assert(v != NULL && v->type == LEPT_ARRAY && index <= v->u.a.size);
    if (v->u.a.size == v->u.a.capacity)
        lept_reserve_array(v, v->u.a.capacity == 0 ? 1 : v->u.a.capacity * 2);
    for (i = v->u.a.size; i > index; i--)
        v->u.a.e[i] = v->u.a.e[i - 1];
    lept_init(&v->u.a.e[index]);
    v->u.a.size++;
    return &v->u.a.e[index];
}
~~~

`lept_erase_array_element()` 先释放被删除的元素，然后将后续元素前移：

~~~c
void lept_erase_array_element(lept_value* v, size_t index, size_t count) {
    size_t i;
    assert(v != NULL && v->type == LEPT_ARRAY && index + count <= v->u.a.size);
    for (i = index; i < index + count; i++)
        lept_free(&v->u.a.e[i]);
    for (i = index + count; i < v->u.a.size; i++)
        v->u.a.e[i - count] = v->u.a.e[i];
    v->u.a.size -= count;
}
~~~

`lept_clear_array()` 可以直接复用 `lept_erase_array_element()`：

~~~c
void lept_clear_array(lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    lept_erase_array_element(v, 0, v->u.a.size);
}
~~~

### 3. 动态对象函数

动态对象的实现与动态数组类似，使用 `capacity` 字段管理已分配的空间。关键函数如 `lept_set_object_value()` 采用「查找或插入」模式。

### 4. 深度复制数组和对象

`lept_copy()` 中数组和对象的复制需要递归处理每个元素：

~~~c
case LEPT_ARRAY:
    lept_set_array(dst, src->u.a.capacity);
    for (i = 0; i < src->u.a.size; i++) {
        lept_init(&dst->u.a.e[i]);
        lept_copy(&dst->u.a.e[i], &src->u.a.e[i]);
    }
    dst->u.a.size = src->u.a.size;
    break;
~~~

完整代码请参考本目录下的 `leptjson.c`。
