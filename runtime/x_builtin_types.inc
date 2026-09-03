// 内置类型清单：X(枚举名, 访问器名, SL 层类名, 基类枚举名)
//
// 顺序即建立顺序，基类必须排在自己的子类前面。这份清单同时驱动三处：枚举、访问器声明/定义、
// bootstrap 的建立循环——加一个内置类型只改这一个文件。
//
// 继承关系照抄 SL.md 4.4 那张图。**注意 bool 的父类是 numbers.Real 而不是 int**
// （跟 Python 不同，SL 里 bool 不是 int 的子类，见 SL.md 4.2.5）。

X(Object, object_type, "object", NoBase)
X(Type, type_type, "type", Object)
X(NoneType, none_type, "NoneType", Object)
X(SingletonType, singleton_type, "SingletonType", Object)
X(Number, number_type, "numbers.Number", Object)
X(Real, real_type, "numbers.Real", Number)
X(Int, int_type, "int", Real)
X(Decimal, decimal_type, "decimal", Real)
X(Bool, bool_type, "bool", Real)
X(Str, str_type, "str", Object)
X(Tuple, tuple_type, "tuple", Object)
