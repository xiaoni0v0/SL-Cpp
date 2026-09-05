// 内置类型清单。
//
// **所有类型都在 Runtime::init() 里一次建完，不按"启动/编译用得到"分批、不惰性构造**——理由见
// .ai/context.md「bootstrap 建什么」。该惰性化的是以后的方法/内置函数对象，不是类型本身，所以
// 别把方法塞进 Type 的构造函数。
//
// 属于模块的类（decimal 的各信号、exceptions.EncodingError）不在这张表里，它们要等模块系统；
// 但 numbers.Number/numbers.Real 这两个类型对象照建不误，int 的 MRO 指着它们。
//
// X(枚举名, 访问器名, SL 层类名, 基类枚举名；基类那列填 NoBase 表示没有基类)
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

// 异常类型清单
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
