// 内置类型清单：X(枚举名, 访问器名, SL 层类名, 基类枚举名)
//
// 顺序即建立顺序，基类必须排在自己的子类前面。这份清单同时驱动三处：枚举、访问器声明/定义、
// bootstrap 的建立循环——加一个内置类型只改这一个文件。
//
// 这张表是"运行期存在哪些类"，不是"内置表里有哪些名字"——SL.md 4.4 那批不入内置表的类（NoneType、
// SingletonType 等）同样要在这里建。function/builtin_function/method/MethodDescriptor
// 也属于那一批， 等函数对象、描述器真正落地时再加进来。
//
// 继承关系照抄 SL.md 4.5 那张图。**注意 bool 的父类是 numbers.Real 而不是 int**
// （跟 Python 不同，SL 里 bool 不是 int 的子类，见 SL.md 4.2.4）。

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

// —— 异常类树，照抄 SL.md 4.2.23 那张图 ——
// 这里只有"全局的一批常用异常"。两处刻意没进来，它们要等对应的模块有了落脚处：
//   - decimal 的各信号（SL.md 4.2.5.3，DecimalException 的子类），跟 decimal 模块一起走；
//   - exceptions.EncodingError（IOError 的子类，SL.md 4.3.3），跟 exceptions 模块一起走。
// **宿主层的 SyntaxError/InternalError/EncodingError/FileNotFoundError 跟这里的同名类不是一回事**，
// 见 .ai/notes/cpp-layer-vs-sl-layer.md。InternalError 更是永远不会在这张表里出现
X(BaseException, base_exception_type, "BaseException", Object)
X(SystemExit, system_exit_type, "SystemExit", BaseException)
X(KeyboardInterrupt, keyboard_interrupt_type, "KeyboardInterrupt", BaseException)
X(Exception, exception_type, "Exception", BaseException)
X(SyntaxError, syntax_error_type, "SyntaxError", Exception)
X(TypeError, type_error_type, "TypeError", Exception)
X(ValueError, value_error_type, "ValueError", Exception)
X(NameError, name_error_type, "NameError", Exception)
X(AttributeError, attribute_error_type, "AttributeError", Exception)
X(IndexError, index_error_type, "IndexError", Exception)
X(MathError, math_error_type, "MathError", Exception)
X(DecimalException, decimal_exception_type, "decimal.DecimalException", MathError)
X(DispatchError, dispatch_error_type, "DispatchError", Exception)
X(RecursionError, recursion_error_type, "RecursionError", Exception)
X(MemoryError, memory_error_type, "MemoryError", Exception)
X(IOError, io_error_type, "IOError", Exception)
X(ImportError, import_error_type, "ImportError", Exception)
