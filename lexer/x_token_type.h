// 字面量
X(LITERAL_NONE)     // None
X(LITERAL_TRUE)     // True
X(LITERAL_FALSE)    // False
X(LITERAL_G)        // _G
X(LITERAL_L)        // _L
X(LITERAL_ELLIPSIS) // ...
X(LITERAL_INT)      // 123
X(LITERAL_FLOAT)    // 123.45
X(LITERAL_STR)      // "Hello" 'Hello'

// 标识符
X(IDENTIFIER)

// 关键字
X(KW_NOT)
X(KW_AND)
X(KW_OR)
X(KW_IS)
X(KW_DEL)
X(KW_GLOBAL)
X(KW_IF)
X(KW_ELIF)
X(KW_ELSE)
X(KW_FOR)
X(KW_WHILE)
X(KW_BREAK)
X(KW_CONTINUE)
X(KW_FUNC)
X(KW_RETURN)
X(KW_RAISE)
X(KW_TRY)
X(KW_EXCEPT)
X(KW_FINALLY)
X(KW_CLASS)

// 保留字
X(RW_ASSERT)
X(RW_IN)
X(RW_WHEN)
X(RW_CASE)
X(RW_YIELD)
X(RW_WITH)
X(RW_ASYNC)
X(RW_AWAIT)

X(RW_DEFINE)
X(RW_AS)
X(RW_CONST)
X(RW_STATIC)
X(RW_LOCAL)

// 符号：括号
X(SIGN_LPAREN)   // (
X(SIGN_RPAREN)   // )
X(SIGN_LBRACKET) // [
X(SIGN_RBRACKET) // ]
X(SIGN_LBRACE)   // {
X(SIGN_RBRACE)   // }

// 符号：分隔符
X(SIGN_COMMA)     // ,
X(SIGN_SEMICOLON) // ;
X(SIGN_COLON)     // :
X(SIGN_AT)        // @
X(SIGN_DOLLAR)    // $

// 符号：算术运算符
X(SIGN_PLUS)        // +
X(SIGN_DOUBLEPLUS)  // ++
X(SIGN_MINUS)       // -
X(SIGN_DOUBLEMINUS) // --
X(SIGN_STAR)        // *
X(SIGN_DOUBLESTAR)  // **
X(SIGN_SLASH)       // /
X(SIGN_DOUBLESLASH) // //
X(SIGN_PERCENT)     // %

// 符号：位运算
X(SIGN_AMPERSAND) // &
X(SIGN_PIPE)      // |
X(SIGN_CARET)     // ^
X(SIGN_TILDE)     // ~
X(SIGN_LSHIFT)    // <<
X(SIGN_RSHIFT)    // >>

// 符号：比较
X(SIGN_EQ)  // ==
X(SIGN_NEQ) // !=
X(SIGN_LT)  // <
X(SIGN_LE)  // <=
X(SIGN_GT)  // >
X(SIGN_GE)  // >=

// 符号：赋值
X(SIGN_ASSIGN) // =

// 符号：复合赋值
X(SIGN_PLUS_ASSIGN)        // +=
X(SIGN_MINUS_ASSIGN)       // -=
X(SIGN_STAR_ASSIGN)        // *=
X(SIGN_DOUBLESTAR_ASSIGN)  // **=
X(SIGN_SLASH_ASSIGN)       // /=
X(SIGN_DOUBLESLASH_ASSIGN) // //=
X(SIGN_PERCENT_ASSIGN)     // %=
X(SIGN_AMPERSAND_ASSIGN)   // &=
X(SIGN_PIPE_ASSIGN)        // |=
X(SIGN_CARET_ASSIGN)       // ^=
X(SIGN_LSHIFT_ASSIGN)      // <<=
X(SIGN_RSHIFT_ASSIGN)      // >>=

// 符号：其他
X(SIGN_DOT)      // .
X(SIGN_DOTDOT)   // ..
X(SIGN_QUESTION) // ?
X(SIGN_EXCLAIM)  // !

// 结构
X(NEWLINE)     // \n
X(END_OF_FILE) // EOF
