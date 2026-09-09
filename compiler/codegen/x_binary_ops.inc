// BINARY_OP 的参数全集。X(枚举名, 助记符)
//
// and / or 不在这里：它们靠 JUMP_IF_FALSE 短路，不落在这条指令上。

// 算术
X(Add, "Add")
X(Sub, "Sub")
X(Mul, "Mul")
X(Div, "Div")
X(DivFloor, "DivFloor")
X(Mod, "Mod")
X(Pow, "Pow")

// 位运算
X(BitAnd, "BitAnd")
X(BitOr, "BitOr")
X(BitXor, "BitXor")
X(LShift, "LShift")
X(RShift, "RShift")

// ..
X(Range, "Range")

// 成员测试
X(In, "In")

// 比较
X(Lt, "Lt")
X(Le, "Le")
X(Gt, "Gt")
X(Ge, "Ge")
X(Eq, "Eq")
X(Ne, "Ne")

// 身份
X(Is, "Is")
