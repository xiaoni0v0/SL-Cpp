// 内部对象表：LOAD_INTERNAL 取的那张全局共享小表，只装 SL 层拿不到的东西。
// 枚举值就是 LOAD_INTERNAL 的参数，所以这张表的顺序是字节码格式的一部分，只许往后追加。
// X(枚举名, 访问器名, 显示名)

// MAKE_FUNC / MAKE_CLASS 缺省参数的哨兵。不能用 None，它是合法的默认值
X(Missing, internal_missing, "<missing>")

// import 调用形态背后的加载函数。函数对象还没落地，现在是个同名占位对象
X(ImportFunc, internal_import_func, "<import>")
