// 指令全集清单，字节码格式的唯一真源。语义见 bytecode.md 的指令表。
// X(枚举名, 助记符, 参数种类, 顺序流的栈效应)
//
// 栈效应表达式里的 arg 是这条指令的参数值，算的是**顺序流继续执行时**的净变化。跳转那条
// 路径的差额记在 opcode.h 的 stack_effect_extra_on_jump()。
//
// 于是 CALL_FINALLY / END_FINALLY 这里都是 0：返回地址走的是帧自己的那个小栈，不占操作数栈。
// SETUP_* 同理，压的是块栈。

// ------------------------------------------------------------
// 栈与常量
// ------------------------------------------------------------
X(EXTENDED_ARG, "EXTENDED_ARG", Raw, 0)
X(POP_TOP, "POP_TOP", None, -1)
X(COPY, "COPY", Depth, +1)
X(INSERT, "INSERT", Depth, 0)
X(LOAD_CONST, "LOAD_CONST", Const, +1)
X(LOAD_INTERNAL, "LOAD_INTERNAL", Raw, +1)

// ------------------------------------------------------------
// 名字
// ------------------------------------------------------------
X(LOAD_NAME, "LOAD_NAME", Name, +1)
X(LOAD_NAME_STRICT, "LOAD_NAME_STRICT", Name, +1)
X(STORE_NAME, "STORE_NAME", Name, -1)
X(DELETE_NAME, "DELETE_NAME", Name, 0)
X(DECLARE_GLOBAL, "DECLARE_GLOBAL", Name, 0)
X(LOAD_GL, "LOAD_GL", Raw, +1)

// ------------------------------------------------------------
// 属性与索引
// ------------------------------------------------------------
X(GET_ATTR, "GET_ATTR", Name, 0)
X(SET_ATTR, "SET_ATTR", Name, -2)
X(DELETE_ATTR, "DELETE_ATTR", Name, -1)
X(GET_INDEX, "GET_INDEX", Count, -static_cast<int>(arg))
X(SET_INDEX, "SET_INDEX", Count, -static_cast<int>(arg) - 2)
X(GET_INDEX_EX, "GET_INDEX_EX", None, -1)
X(SET_INDEX_EX, "SET_INDEX_EX", None, -3)

// ------------------------------------------------------------
// 运算符
// ------------------------------------------------------------
X(UNARY_OP, "UNARY_OP", UnaryOp, 0)
X(BINARY_OP, "BINARY_OP", BinaryOp, -1)

// ------------------------------------------------------------
// 容器
// ------------------------------------------------------------
X(MAKE_TUPLE, "MAKE_TUPLE", Count, 1 - static_cast<int>(arg))
X(MAKE_LIST, "MAKE_LIST", Count, 1 - static_cast<int>(arg))
X(MAKE_DICT, "MAKE_DICT", Count, 1 - 2 * static_cast<int>(arg))
X(LIST_APPEND, "LIST_APPEND", Depth, -1)
X(LIST_EXTEND, "LIST_EXTEND", Depth, -1)
X(DICT_PUT, "DICT_PUT", Depth, -2)
X(DICT_MERGE, "DICT_MERGE", Depth, -1)
X(COUNT_INC, "COUNT_INC", Depth, 0)
X(LIST_TO_TUPLE, "LIST_TO_TUPLE", None, 0)
X(UNPACK, "UNPACK", Count, static_cast<int>(arg) - 1)
X(UNPACK_EX, "UNPACK_EX", Raw, static_cast<int>((arg & 0xFFu) + (arg >> 8u)))

// ------------------------------------------------------------
// 跳转
// ------------------------------------------------------------
X(JUMP, "JUMP", Jump, 0)
X(TO_BOOL, "TO_BOOL", None, 0)
X(JUMP_IF_FALSE, "JUMP_IF_FALSE", Jump, -1)
X(GET_ITER, "GET_ITER", None, 0)
X(FOR_ITER, "FOR_ITER", Jump, +1)

// ------------------------------------------------------------
// 异常与 finally
// ------------------------------------------------------------
X(SETUP_EXCEPT, "SETUP_EXCEPT", Jump, 0)
X(SETUP_FINALLY, "SETUP_FINALLY", Jump, 0)
X(POP_BLOCK, "POP_BLOCK", None, 0)
X(CALL_FINALLY, "CALL_FINALLY", Jump, 0)
X(END_FINALLY, "END_FINALLY", None, 0)
X(CHECK_EXC_MATCH, "CHECK_EXC_MATCH", None, 0)
X(RAISE, "RAISE", None, -1)
X(RERAISE, "RERAISE", None, -1)

// ------------------------------------------------------------
// 调用与建立
// ------------------------------------------------------------
X(CALL, "CALL", Count, -static_cast<int>(arg))
X(CALL_KW, "CALL_KW", Count, -static_cast<int>(arg) - 1)
X(CALL_EX, "CALL_EX", None, -2)
X(MAKE_FUNC, "MAKE_FUNC", NestedCode, -4)
X(MAKE_CLASS, "MAKE_CLASS", NestedCode, -3)
X(RETURN_VALUE, "RETURN_VALUE", None, -1)
X(IMPORT, "IMPORT", Name, +1)
X(EVAL, "EVAL", None, -1)
