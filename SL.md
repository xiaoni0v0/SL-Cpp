# SL 语言标准

[TOC]

## 1 语言概述

SL 是一种面向对象的编程语言。特点是：

- 大小写敏感
- 缩进不敏感
- 一切皆对象
- 一切皆表达式——我们再也不说“**执行**”（exec），而说“**求值**”（eval）。
- 动态类型
- 自动 GC，主要使用引用计数，辅以堆扫描处理循环引用

## 2 语法

### 2.1 词法

#### 2.1.1 注释

- 单行注释: `# 这是一个注释`

- 多行注释: `/* 这是一个多行注释 */`

  注意多行注释不支持嵌套，即 `/* a /* b */ c */` 中 `b` 后的 `*/` 会结束注释，可能导致语法错误。

#### 2.1.2 关键字与保留字

以下列出的是 SL 的**关键字**，不能作为标识符使用：

- `None`, `True`, `False`
- `_G`, `_L`
- `not`, `and`, `or`, `is`
- `del`
- `global`
- `if`, `elif`, `else`
- `for`, `while`, `break`, `continue`
- `func`, `return`
- `raise`
- `try`, `except`, `finally`
- `class`

以下列出的是 SL 的**保留字**，目前不是关键字，不能作为标识符使用，对应 token 出现在代码中将无条件引发 `SyntaxError`：

- `define`
- `as`
- `yield`
- `async`, `await`
- `in`
- `const`
- `static`
- `with`
- `when`, `case`
- `local`
- `assert`

#### 2.1.3 标识符

**标识符**: 正则 `[a-zA-Z_][a-zA-Z0-9_]*`，且不能是关键字或保留字。

#### 2.1.4 字面量

SL 中有以下**字面量**类型：

- None: `None`；
- bool: `True` 或者 `False`；
- `_G`, `_L`；
- int: `123`，目前尚不支持二进制、八进制、十六进制。负数不是字面量，是一元符号和整数的运算结果；
- float: `123.45`，目前尚不支持科学计数法；整数、小数部分都不能省略，`1.`、`.1` 不是合法的 float 字面量；
- str:
    - `"hello"` 或者 `'hello'`，支持转义但不支持多行；
    - `` `hello` ``（反引号），为**原始字符串**，不处理任何转义、原样天然支持多行；
      反引号字符串内不能出现反引号本身，需要用普通字符串拼接得到；

  三种写法产出的都是 `str`，没有类型区别；
  支持的转义（对于 `"..."`/`'...'`）：
  `'\a'`, `'\b'`, `'\f'`, `'\n'`, `'\r'`, `'\t'`, `'\v'`, `'\0'`, `'\\'`, `'\''`, `'\"'`；
  SL 不支持 C/Python 那种字面量相邻自动拼接；
- tuple: `(1, 2, 3)`，空元组 `()`，单元素元组必须 `(1,)`；括号不可省略，不存在无括号的元组写法（与 Python 不同）；
- list: `[1, 2, 3]`，空列表 `[]`；
- dict: `{k1: v1, k2: v2}`，空字典 `dict()`；
- Ellipsis: `...`。

其他内置类型，如集合等，不提供字面量写法，请使用 `set()` 等类创建。

#### 2.1.5 运算符

以下是**运算符**表，运算时先进行优先级值大的运算。

| 优先级值 | 运算符                                                                              | 名称        | 结合性 | 参数数量 |
|------|----------------------------------------------------------------------------------|-----------|-----|------|
| 160  | `x[arg, ...]`, `x(arg, kwarg=v, ...)`, `x.attribute`                             | 索引、调用、属性  | L   | -    |
| 155  | `++x`, `--x`                                                                     | 自增、自减     | R   | 1    |
| 150  | `x?`, `x!`                                                                       | 问号、感叹号    | L   | 1    |
| 140  | `**`                                                                             | 幂运算       | R   | 2    |
| 130  | `+x`, `-x`, `~x`                                                                 | 正、负、按位取反  | R   | 1    |
| 120  | `*`, `/`, `//`, `%`                                                              | 乘、除、整除、取模 | L   | 2    |
| 110  | `+`, `-`                                                                         | 加、减       | L   | 2    |
| 100  | `..`                                                                             | 范围运算      | L   | 2    |
| 90   | `<<`, `>>`                                                                       | 按位左移、右移   | L   | 2    |
| 80   | `&`                                                                              | 按位与       | L   | 2    |
| 70   | `^`                                                                              | 按位异或      | L   | 2    |
| 60   | `\|`                                                                             | 按位或       | L   | 2    |
| 50   | `<`, `<=`, `>`, `>=`, `!=`, `==`                                                 | 大小、相等比较   | L   | 2    |
| 45   | `is`                                                                             | 是否是同一个对象  | L   | 2    |
| 40   | `not`                                                                            | 逻辑取反      | R   | 1    |
| 30   | `and`                                                                            | 逻辑与       | L   | 2    |
| 20   | `or`                                                                             | 逻辑或       | L   | 2    |
| 10   | `=`, `+=`, `-=`, `*=`, `**=`, `/=`, `//=`, `%=`, `&=`, `\|=`, `^=`, `<<=`, `>>=` | 赋值、复合赋值   | R   | 2    |

其中 `()` 作为调用的传参方式有：

1. 位置传参 `f(1, 2, 3)`；
2. 关键字传参 `f(x=1, y=2, z=3)`。此时若想表达“赋值的同时传参”请显式地加括号。

传参顺序的规则见 3.5 所述。

其中 `=` 左边的**左值**（可以出现在赋值左边的表达式）包括：

1. 标识符；
2. `target.attr`（属性访问，不管 `target` 本身是什么结构）；
3. `target[index, ...]`（元素访问，同上）；
4. 由以上三种左值组成的元组/列表字面量 `(lv1, lv2, ...)`/`[lv1, lv2, ...]`（**解构赋值**），
   其中至多一个左值可以带 `*` 前缀（收集剩余元素），可嵌套，语义同 Python 的打包解包。

以上运算符：

1. 可使用 `()` 改变运算顺序；
2. **注意**：`<`、`<=`、`>`、`>=`、`!=`、`==` 这 6 者之间支持链式写法（如 `a < b <= c`），
   `is` 自身也支持链式（如 `a is b is c`）；
   但这 6 者和 `is` 之间不链式，混写按各自优先级正常求值（如 `a < b is c` 即 `(a < b) is c`）。
   语义见 3.3；
3. 除了 `is`、`and`、`or`、`not`、求属性、`=` 以及所有复合赋值，其他运算符均可重载；
4. `++x` 和 `--x` 均只有前缀形式，没有后缀形式。

#### 2.1.6 空白字符

将以下视为**空白**：

- 注释
- 空格 ' '
- 制表符 '\t'

符号之间若会产生歧义则必须有空白或换行、括号等分隔符分隔，否则可省略。
例如，`for $ (i : ls) { i ** 2 }` 可以写为 `for$(i:ls){i**2}`。但 `a! == b` 不可写为 `a!==b`。

### 2.2 表达式

量词：条，一条**表达式**（A Piece of Expression）。

记号约定：下文语法描述中，`⟦X⟧` 表示 `X` 可选（出现 0 次或 1 次）；这只是本文描述语法用的记号，不是 SL 的语法。
重复（0 个或多个、1 个或多个）仍用 `...` 加文字说明，不单独引入记号。

#### 2.2.1 表达式分隔符

对于一个块的内部，用如下方式分隔表达式：

按 `;` token 和换行符 token 切分，得到若干子式，称为**待定表达式**（Pending Expression）。
每条待定表达式记录其终止符类型（`;` 或 EOF 记为硬终止，换行符记为软终止）

对每条待定表达式，尝试将其解析为一个完整表达式。

- 若解析成功
    - 若为软终止，检查是否满足以下条件，若满足则合并下一条待定表达式，重新解析。否则将待定表达式作为表达式。
        1. 解析结果为 `if` 表达式，且无 `else`，且下一条以 `elif` 或 `else` 开头。
        2. 解析结果为 `try` 表达式，且无 `finally`，且下一条以 `except` 或 `finally` 开头。
    - 若为硬终止，则将待定表达式作为表达式
- 若解析失败
    - 若为软终止，将下一条待定表达式合并到当前待定表达式，重新解析。
    - 若为硬终止，在开始合并的行抛出 `SyntaxError`；

例如：

```
a = b
+ c
# 以上解析为 a = b; +c;
# 原因：第一行完整，不管后边，第一行成一个表达式

d = e +
f
# 以上解析为 d = e + f;
# 原因：第一行的二元运算符 `+` 只有一个左参数，不完整，合并下一条

x.
func()
# 以上解析为 x.func();
# 原因：第一行的二元运算符 `.` 只有一个左参数，不完整，合并下一条

x
.func()
# 以上解析为 x; .func(); 会抛出 SyntaxError
# 不推荐此形式的链式调用。若一定要链式调用，请在外侧加圆括号

(x
.func()
)
# 以上解析为 x.func();
# 原因：括号未闭合，表达式不完整，不断合并

(
1,
2,
)
# 以上解析为 (1, 2);

if (x == 10) x = 100
else x = 200
# 以上解析为 if (x == 10) { x = 100; } else { x = 200; }

if (x == 10) x = 100;
else x = 200
# 以上解析为 { if (x == 10) x = 100 }; { else x = 200; }
# 两个表达式，第二个以 else 开头，抛出 SyntaxError
```

#### 2.2.2 基本表达式

- 字面量是表达式。
- 标识符是表达式。
- 运算符组成的式是表达式。
- 用花括号 `{}` 定义的**复合表达式** `{ expr1; expr2; ... }` 是表达式。

**注**：`{}` 也用于字典字面量（2.1.4）、函数体（2.2.6）、类体（2.2.7），
判别规则：

- `func` 或 `class` 之后的 `{}` 为函数体/类体；
- 否则若 `{` 后第一个元素以 `**` 开头，或第一个表达式后紧跟 `:`，为字典字面量；
- 都不满足则为复合表达式。

#### 2.2.3 `del` 表达式

`del` 是表达式。

语法：`del target`。其中 `target` 的必须是标识符或属性访问。

#### 2.2.4 `global` 表达式

`global` 是表达式。

语法：`global identifier`，其中 `identifier` 是标识符。

只能在局部作用域中使用。

#### 2.2.5 控制流表达式

##### 2.2.5.1 `if` 表达式

控制流 `if` 是表达式。`if` 可与 `elif` 和 `else` 结合使用。

语法：

```
if (cond1) expr1 ⟦elif (cond2) expr2 ...⟧ ⟦else expr3⟧
```

`elif` 分支可以有 0 个或多个；`else` 可选；`cond`、`expr` 均为表达式。

##### 2.2.5.2 `for` 表达式

控制流 `for` 是表达式。

语法：

1. 步进模式：`for ⟦$⟧ (init cond inc) expr`
2. 迭代模式：`for ⟦$⟧ (identifier : iterable) expr`

其中：

1. `init`, `cond`, `inc` 为表达式或空，三者用 `;` 或至少一个换行无歧义地分割（若某个槽为空，则必须使用 `;`）；
2. `identifier` 为标识符，`iterable` 为表达式；
3. `expr` 为表达式；
4. 带 `$` 为收集模式，不带为计数模式。

##### 2.2.5.3 `while` 表达式

控制流 `while` 是表达式。

语法：`while ⟦$⟧ (cond) expr`

其中 `cond` 和 `expr` 为表达式。

带 `$` 为收集模式，不带为计数模式。

##### 2.2.5.4 `break` 表达式

`break` 是表达式。

语法：`break`。

只能在 `for` 或 `while` 中的 `expr` 部分使用。

##### 2.2.5.5 `continue` 表达式

`continue` 是表达式。

语法：`continue`。

只能在 `for` 或 `while` 中的 `expr` 部分使用。

##### 2.2.5.6 `return` 表达式

`return` 是表达式。

语法：`return ⟦expr⟧`，其中 `expr` 为表达式。

可在全局或函数体内使用。

##### 2.2.5.7 `try` 表达式

控制流 `try` 是表达式。

语法：

```
try expr1 ⟦except (Exception1, ...) expr2 ...⟧ ⟦finally expr3⟧
```

`except` 子句可以有 1 个或多个（上面只写了一个），每个 `except` 内 `Exception` 有 1 个或多个；`finally` 可选；
但 `except` 和 `finally` 不能同时省略。

其中 `expr1`、`expr2`、`expr3`、`Exception` 均为表达式。

##### 2.2.5.8 `raise` 表达式

`raise` 是表达式。

语法：`raise expr`，其中 `expr` 是表达式。

#### 2.2.6 函数表达式

函数的定义 `func` 是表达式。

语法：

```
⟦@decorator ...⟧ func ⟦identifier⟧ ⟦ [ALL_CAPTURE] ⟧ (ALL_PARAM) ⟦: type⟧ { expr1; ... }
```

其中：

1. `decorator` 为表达式；
2. `identifier` 为标识符；
3. `type` 为表达式；
4. `{ expr1; ... }` 称为**函数体**，由 0 个或多个表达式组成。

`[ALL_CAPTURE]` 为**捕获列表**，由 0 个或多个 `ONE_CAPTURE` 组成，语法：

1. `identifier`（**值捕获**，捕获当前作用域内同名标识符的值）
2. `identifier = expr`（**值捕获**，捕获 `expr` 的值，绑定为 `identifier`）
3. `&identifier`（**引用捕获**）

其中 `identifier` 为标识符，`expr` 为表达式。

`ALL_PARAM` 为**形参**部分，由 0 个或多个 `ONE_PARAM` 组成，语法：

1. `identifier ⟦: type⟧ ⟦= expr⟧`
2. `*identifier`（可变长位置形参）
3. `**identifier`（可变长关键字形参）

其中 `identifier` 为标识符，`expr` 和 `type` 均为表达式。

捕获列表、形参列表内部及两者之间的标识符均不可重复，否则抛出 `SyntaxError`。

以上形参若出现，必须遵循以下顺序，否则会抛出 `SyntaxError`：

1. 无默认值的形参；
2. 有默认值的形参、可变长位置形参（这两种之间顺序不限）；
3. 可变长关键字形参。

#### 2.2.7 类表达式

类的定义 `class` 是表达式。

语法：

```
⟦@decorator ...⟧ class ⟦identifier⟧ ⟦(BaseClass1, ...)⟧ { expr1; ... }
```

其中：

1. `decorator` 为表达式；
2. `identifier` 为标识符；
3. `BaseClass` 为表达式；
4. `{ expr1; ... }` 称为**类体**，由 0 个或多个表达式组成。

#### 2.2.8 装饰器表达式

装饰器 `@decorator` 是表达式。

语法：`@decorator expr`，其中 `decorator` 和 `expr` 均为表达式。

**注意**：若 `expr` 紧邻着就是一个函数表达式或类表达式则不适用本节，那属于函数/类表达式的一部分。

## 3 语义

### 3.1 表达式的求值

任何“语句”都是表达式，**求值**即运行某些步骤并得到结果。
故我们将表达式的功能分为两类：**值**和**副作用**。

### 3.2 对象的真值

任意对象的**真值**为 `True` 或 `False`。
对内置类型，`None`、`False`、`0`、`0.0`、`''`、`()`、`[]`、`dict()`、`set()` 的真值为 `False`，其他均为 `True`。

请使用 `bool(x)` 或 `not not x` 或 `if (x) True else False` 来获取对象的真值。

### 3.3 求值顺序

为了避免 C/C++ 中臭名昭著的 UB，SL 中求值顺序是确定的，遵循如下规则：

1. 一个表达式的所有副作用，都会在该表达式完全求值完毕（其值被确定）之前发生;
2. 对元组 `(expr1, expr2, ...)` 和列表 `[expr1, expr2, ...]`
   各元素从前到后逐个求值;
3. 对字典 `{k1: v1, k2: v2, ...}`
   各键值对从前到后逐个求值。即 `k1` -> `v1` -> `k2` -> `v2` -> ... 的顺序;
4. 对运算符：
    1. 对单目运算符（`++x`、`--x` 除外，见下文），先对参数求值，再进行运算符的运算；
    2. 对除了 `and`、`or`、`=` 以及 `+=` 等复合赋值运算符、链式比较（见下第 3 点）外的二目运算符，
       先对左参数求值，再对右参数求值，再进行运算符的运算；
    3. 对链式比较 `a OP1 b OP2 c ...`（`OP` 同属一组：`<`/`<=`/`>`/`>=`/`!=`/`==` 之一，或都是 `is`，可以有 2 个以上算子）：
       先对 `a` 求值，再对 `b` 求值，计算 `t = a OP1 b`；若 `t` 的真值为假，短路，整体的值即为 `t`；
       若真值为真，再对 `c` 求值，计算 `t = b OP2 c`，按同样方式判断、短路；
       除了求值次数不同外，它等价于 `a OP1 b and b OP2 c and c ...`
    4. 对 `and`，先对左参数求值，若其真值成立，则对右参数求值并返回；否则直接返回左参数的值；
    5. 对 `or`，先对左参数求值，若其真值成立，则直接返回左参数的值；否则对右参数求值并返回；
    6. 对赋值运算符（左值定义见 2.1.5）

        1. 简单赋值 `x = expr`
           先对 `expr` 求值，若 `x` 已经引用对象则令 `x` 解除对原对象的引用，再令 `x` 引用结果对象；
        2. 属性赋值 `x.attribute = expr`
           先对 `x` 求值，再对 `expr` 求值，令 `x` 的属性 `attribute` 引用结果对象；
        3. 元素赋值 `x[args, ...] = expr`
           先对 `x` 求值，再对 `args` 逐个求值，再对 `expr` 求值，令 `x` 的元素 `args` 引用结果对象；
        4. 自增自减 `++target`、`--target`，以及复合赋值 `target op= expr`（`target` 为标识符、属性访问或元素访问）
           若 `target` 是属性访问 `x.attr`，`x` 只求值一次；若是元素访问 `x[index, ...]`，`x` 与各 `index` 只各求值一次，
           读、写复用同一次求值结果，不会因为读旧值和写新值这两步而重复求值。
           随后：先读取 `target` 当前值，`op= expr` 情形下再对 `expr` 求值，算出新值，最后写入 `target`
           （标识符的读、写具体落在哪里见 3.10）。
        5. 解构赋值 `(lv1, lv2, ...) = expr` 或 `[lv1, lv2, ...] = expr`
           先对 `expr` 求值，要求其可迭代，否则抛出 `TypeError`；
           再按各 `lv` 从前到后依次对其赋值，每个 `lv` 自身的求值/赋值规则按其类型套用上述 1~4 条
           （`lv` 本身也是解构形式则递归套用本条）。
           无 `*lv` 时要求元素个数与 `lv` 个数完全一致，否则抛出 `ValueError`；
           有 `*lv` 时要求元素个数不少于其余 `lv` 的个数，否则抛出 `ValueError`；`*lv` 收集剩余元素，总是绑定一个 `list`；
    7. 对索引 `x[index1, index2, ...]` 和调用 `x(arg1, arg2, ...)`
       先对 `x` 求值，再从前到后逐个求参数的值，最后求索引或调用函数；
5. 对复合表达式 `{ expr1; expr2; ... }`
   从前到后逐个求每条表达式的值；
6. 对控制流，见 3.4.5 所述。
7. 对函数定义 `func f[ALL_CAPTURE](x: type_1 = default_value_1, y: type_2 = default_value_2, ...): ret_type`
   先从前到后处理 `ALL_CAPTURE` 中各项（具体规则见 3.10.4），
   再从前到后对形参的类型注解和默认值逐个求值，
   即 `type_1` -> `default_value_1` -> `type_2` -> `default_value_2` -> ... 的顺序，
   最后（若有 `: ret_type`）对 `ret_type` 求值；
8. 对类定义 `class identifier(BaseClass1, ...)`
   各基类从前到后逐个求值；
9. 对 `except`，各异常类从前到后逐个求值。

### 3.4 表达式的值

#### 3.4.1 基本表达式的值

- 字面量的值为对应对象；
  其中 `_G`、`_L` 的值为字典，分别表示全局变量和局部变量的字典（实时视图，可读可写）；
- 标识符的值为对应的变量的值；
- 有运算符的表达式值为对应运算符的运算结果；
- 复合表达式的值为块中最后一条表达式的值；空复合表达式 `{}` 的值为 `None`。

#### 3.4.2 运算符表达式的值

以下描述为内置类的实例的语义。自定义类型详见 3.8 所述。

- `x[index, ...]`
    - 对于列表、元组、字符串等，返回下标为 `index` 的元素（此时 `index` 为 int，或 range 对象表示切片）；
    - 对于字典等，返回键为 `index` 所对应的值；
- `x(arguments, ...)`
  调用函数 `x`，传入参数 `arguments`，返回函数的返回值。详见 3.5 所述；
- `x.attribute`
  获取对象 `x` 的属性 `attribute` 的值；
- `x?` 对于类型，生成一个复合类型，表示该类型或 `None`；
- `x!` 对于类型，生成一个复合类型，表示精确该类型（不可以是子类型）；
- `x ** y` 返回 `x` 的 `y` 次幂；
  `x`、`y` 都是 int 时，结果是整数次幂则仍是 int，否则（如 `2 ** -1`）是 float；
  结果不为实数（如负数开偶次方根）抛出 `MathError`；
- `++x`, `--x`，对于 int，表示把 `x` 自增/自减 1，并返回自增/自减后的值；
- `+x`, `-x` 返回正 `x`，负 `x`。
  对于数字，`+x` 等于 `x`，`-x` 等于 `x` 的相反数；
- `~x`
  对于 int，返回 `x` 的按位取反。
- `x * y`
    - 对于均为数字，`x * y` 等于 `x` 乘 `y`；
    - 对于一方是非负的 int 而另一方字符串、元组、列表等，返回把该字符串、元组、列表等重复非负整数次；
- `x / y`, `x // y`, `x % y`
  对于数字，分别返回 `x` 除以 `y` 的精确商、向下取整商、余数；
  `x`、`y` 都是 int 时：`/` 结果总是 float；
  `//`、`%` 结果仍是 int。`y` 为 `0` 一律抛 `MathError`；
- `x + y`
    - 对于均为数字，分别返回 `x` 加 `y`；
    - 对于均为字符串、元组、列表，返回 `x` 和 `y` 的拼接；
- `x - y`
  对于数字、集合，返回 `x` 减 `y` / 差集；
- `x..y`
    - 对于 `x` 和 `y` 为 int、float、None，返回一个 range 对象（称为二元 range 对象），
      分别表示 start、stop，步长默认为 1，等同于 Python 里的 range(x, y)；
    - 对于 `x` 是二元 range 对象且 `y` 是 int、float、None，返回一个 range 对象（称为三元 range 对象），
      表示同 Python 里的 `range(start, stop, step)`；
    - 对于 `x` 是 int、float、None 且 `y` 是二元 range 对象，直接抛出 `TypeError`；
- `x << y`, `x >> y`
  对于 int，分别返回按位左移、按位右移；
- `x ^ y`
    - 对于 int，返回按位异或；
    - 对于 set，返回集合的异或；
- `x & y`, `x | y`
    - 对于 int，分别返回按位与、按位或；
    - 对于 set，返回它们的交集、并集；
- `x < y`, `x <= y`, `x > y`, `x >= y`, `x != y`, `x == y`，分别返回 `x` 小于/小于或等于/大于/大于或等于/不等于/等于 `y`；
- `x is y`，返回 `x` 和 `y` 是否是同一个对象；
- `not x`, `x and y`, `x or y`，分别返回逻辑非、逻辑与、逻辑或：
    - 逻辑非的语义：若 `x` 的真值为 `True`，则返回 `False`，否则返回 `True`（一定是 bool 类型）；
    - 逻辑与的语义：若 `x` 的真值为 `True`，则返回 `y`，否则返回 `x`（一定是 `x` 和 `y` 之一，不一定是 bool 类型）；
    - 逻辑或的语义：若 `x` 的真值为 `True`，则返回 `x`，否则返回 `y`（一定是 `x` 和 `y` 之一，不一定是 bool 类型）；
- `x = expr`，表示令 `x` 引用 `expr` 这个对象，返回 `expr` 的值；
  `x` 为解构形式（左值定义见 2.1.5）时，按 3.3 所述规则对各左值逐个赋值，值同样是 `expr` 的值；
- `x op= expr`（其中 `op` 为支持的复合赋值运算的运算符），把 `旧值 op expr的值` 的结果重新绑定给 `x`；
  **不**等价于 `x = x op expr`（读取旧值的规则不同，见 3.10.3）。

#### 3.4.3 `del` 表达式的值

`del` 有两种目标：

- `del identifier`：在作用域中解除标识符对对象的引用（见 3.10.3）；
- `del expr.attr`：删除对象的属性（见 3.9.1.2）。

两种形式的值均为 `None`。

#### 3.4.4 `global` 表达式的值

只能在局部作用域（函数体或类体）中使用，表示将标识符指定为全局变量。详细内容请见 3.10.2 所述。

`global identifier` 的值为 `None`。

#### 3.4.5 控制流表达式的值

##### 3.4.5.1 `if` 表达式的值

从前到后依次对每个 `cond`（`if` 的 `cond1`、各 `elif` 的 `cond2` ...）求值，
一旦某个真值成立，就求对应 `expr` 的值并返回，后面的 `cond`/`expr` 都不再求值；
若全部为假，有 `else` 则求 `expr3` 的值并返回，否则返回 `None`。

##### 3.4.5.2 `for` 表达式的值

以下两种模式，`$` 都独立起作用：

- 不带 `$` 时，每轮对 `expr` 求值后丢弃，整体的值是 int（实际循环次数）；
- 带 `$` 时，每轮对 `expr` 求值后放入结果列表末尾，整体的值是这个结果列表。

1. 步进模式 `for ⟦$⟧ (init cond inc) expr`：
   若 `init`、`inc` 为空，对其求值实为跳过；若 `cond` 为空，对其求值实为返回 `True`。
   先对 `init` 求值并丢弃，然后不断重复：对 `cond` 求值，真值成立则按上述方式处理 `expr`，再对 `inc` 求值并丢弃；否则跳出循环。
2. 迭代模式 `for ⟦$⟧ (identifier : iterable) expr`：
   要求 `iterable` 具有迭代器协议，否则抛出 `TypeError`。
   不断从 `iterable` 取出一个元素赋给 `identifier`，然后按上述方式处理 `expr`，直到迭代结束。

`expr` 中可含有 `break` 和 `continue`，其行为以及对 `for` 的值的影响详见下文。

**注意**：请不要在无限循环使用收集模式，否则内存占用持续增加。

##### 3.4.5.3 `while` 表达式的值

`while ⟦$⟧ (cond) expr` 等价于 `for ⟦$⟧ (; cond ;) expr`。不再单独定义。

##### 3.4.5.4 `break` 表达式的值

只能在 `for` 或 `while` 中的 `expr` 部分使用。表示跳出包含 `break` 的最内层的循环。

跳出循环，则此次循环**不计入**循环次数（计数模式），此次循环的值**不放入**返回的列表中（收集模式）。

由于对 `break` 求值会退出循环，一切试图利用 `break` 的值的表达式都得不到求值，因此其值毫无意义。
但为了统一，规定其值为 `None`。

##### 3.4.5.5 `continue` 表达式的值

只能在 `for` 或 `while` 中的 `expr` 部分使用。表示跳过当前循环的剩余部分。注意仍然会对 `inc` 求值进而对 `cond` 求值。

跳过当前循环，则此次循环**计入**循环次数（计数模式），此次循环的值**不放入**返回的列表中（收集模式）。

由于对 `continue` 求值会跳过当前循环的剩余部分，一切试图利用 `continue` 的值的表达式都得不到求值，因此其值毫无意义。
但为了统一，规定其值为 `None`。

##### 3.4.5.6 `return` 表达式的值

表示返回指定的值。

裸 `return` 等价于 `return None`。

1. 在函数体内
   立即退出函数，返回指定的值。
2. 在全局
   立即退出当前文件，`return` 后的表达式的值会成为“该文件的返回值”。
    1. 若当前文件作为脚本运行，则要求 `return` 后的表达式的值为 int 且大小在 C++ 中 int 能表示的范围内，或者 `None`。
       该值将作为解释器的退出码，其中 `None` 被视为 0。
    2. 若当前文件作为模块被导入，则 `return` 后的表达式的值会被记入模块对象的 `__return__` 属性。

由于对 `return` 求值会退出程序或函数，一切试图利用 `return` 的值的表达式都得不到求值，因此其值毫无意义。
但为了统一，规定其值为 `None`。

##### 3.4.5.7 `try` 表达式的值

先对 `expr1` 求值：

- 若未发生异常，`expr1` 的值即整个表达式的值；
- 若发生异常，从上到下检查每个 `except` 的 `Exception`（可能不止一个，每个 `except` 内也可能有多个 `Exception`），
  对其求值并检查异常类型是否匹配；一旦匹配，对 `expr2` 求值，其值即整个表达式的值，不再检查后面的 `except`；
  若一个都不匹配（或没有 `except` 子句），异常继续向上传播。

无论上面走到哪一种情况，只要有 `finally` 子句，退出前都会对 `expr3` 求值（丢弃其值）。

跳转限制：
对 `expr3` 求值期间，任何试图跳出这段求值范围的 `return`/`break`/`continue` 都会被拦截并转成 `SyntaxError`；
完整落在 `expr3` 内部的循环、`expr3` 内定义的函数不受影响。
字面写在 `expr3` 里、能在编译期直接查出违规的会提前报错；
查不出来的（例如来自 `eval` 现场编译出的代码，见 4.1.15）由这层运行时拦截兜底，同样抛出 `SyntaxError`。

异常对象的绑定（`__except__`）：

当异常匹配某个 `except`，则将对其 `expr2` 求值前，完成以下：

1. 若当前作用域已存在 `__except__`，先将其原值临时保存（被 shadow）；
2. 将被捕获的异常对象赋给 `__except__`；
3. 在 `expr2` 求值结束、退出该 `except` 块时，恢复 `__except__`：
    - 若进入前已存在，则恢复为其原值；
    - 若进入前不存在，则解除该变量（相当于 `del __except__`）。

借此实现“重新抛出当前异常”：在 `except` 块内写 `raise __except__` 即可。

##### 3.4.5.8 `raise` 表达式的值

`raise expr`，表示在当前函数当前位置抛出异常。`expr` 需要是 `BaseException` （或其子类）的对象，否则抛出 `TypeError`。

若要重新抛出当前正在处理的异常，请在 `except` 块内使用 `raise __except__`。

由于对 `raise` 求值会结束当前块，一切试图利用 `raise` 的值的表达式都得不到求值，因此其值毫无意义。
但为了统一，规定其值为 `None`。

#### 3.4.6 函数表达式的值

省略 `identifier` 定义一个**匿名函数**（**lambda 表达式**），带 `identifier` 定义一个**命名函数**；
该表达式的值都是一个函数对象。

以上两者的区别是：

1. 命名函数的 `__name__` 属性为函数名（字符串），匿名函数不存在 `__name__` 属性。
2. 命名函数会令当前作用域中名称为 `identifier` 的变量引用该函数最终确定的值（见下）。

**文档字符串**：函数体（按 3.11 折叠后）的第一条表达式若是 `str` 字面量，
取其值按以下规则去除缩进后，记为函数对象的 `__doc__` 属性；否则不存在 `__doc__` 属性。

去除缩进规则：

1. 按 `\n` 拆成若干行；
2. 首行单独去掉前导空白（不参与第 3 步的计算）；
3. 其余行中，只看非空白行，取它们前导空白长度的最小值为 `margin`，把这些行统一去掉前 `margin` 个字符；
4. 去掉结果开头和结尾的空行（中间的空行保留）；
5. 重新按 `\n` 拼接。

若函数带有前缀装饰器，建立函数对象之后，按从近到远（离 `func` 最近的先来）依次调用装饰器，整个表达式的值就是最终的值；
若函数是命名的，`identifier` 只在最终值确定后绑定一次，建立过程中不会绑定任何中间值。

要求每个 `decorator` 都是 `Callable`，否则抛出 `TypeError`。

例如：

```
@deco1
@deco2
func f() {}
# 等价于
func f() {};
f = deco1(deco2(f));

@deco func () {}
# 等价于
deco( func () {} )
```

函数建立时会处理 `ALL_CAPTURE`（若有），规则见 3.10.4。

每一个形参可以有以下几种形式：

1. `identifier ⟦: type⟧ ⟦= expr⟧`
    - 都不带：普通形参，调用时必须传入实参；
    - 带 `= expr`：默认值；
      默认值的求值在函数建立时，所以也可能有副作用；
      调用时未传入参数则使用默认值；

      若默认值为可变对象，可能会使该对象在不同调用之间共享。例如：
      ```
      func f(x, ls = []) { ls.append(x); print(ls); }
      f(1) # 输出 [1]
      f(2) # 输出 [1, 2]
      ```

    - 带 `: type`：类型注解；
      类型注解本身的求值也在函数建立时，所以也可能有副作用；
      定义时检查类型注解是否满足 `isinstance(annotation, type | CompoundType)`，若不满足则抛出 `TypeError`；
      调用时自动进行类型检查，若类型不匹配则抛出 `DispatchError`。
    - `: type` 和 `= expr` 都带：除了各自的行为，还会在函数建立时额外检查默认值是否符合类型注解，不符合则抛出 `TypeError`；
2. `*identifier`
   可变长位置形参，至多一个，会容纳所有多余的位置参数。`identifier` 类型为元组。不支持 `: type`/`= expr`。
3. `**identifier`
   可变长关键字形参，至多一个，会容纳所有多余的关键字参数。`identifier` 类型为字典。不支持 `: type`/`= expr`。

函数建立时，检查返回类型注解是否满足 `isinstance(annotation, type | CompoundType)`，不满足则抛出 `TypeError`。

以上涉及函数定义时检查的地方，都在整个函数表达式的捕获、全部形参的注解与默认值、返回类型全部求值完毕之后统一进行。

实际执行时，没有类型注解的形参/返回值等价于类型注解 `object`。

函数定义并不会执行函数体，只有当函数被调用时才会执行此操作。

函数内需要显式使用 `return` 语句返回值，否则函数运行完毕后自动 `return None`。

有关函数调用的细节，请见 3.5 所述。

#### 3.4.7 类表达式的值

省略 `identifier` 定义一个**匿名类**，带 `identifier` 定义一个**命名类**；
该表达式的值都是一个类对象。

以上两者的区别是：

1. 命名类的 `__name__` 属性为类名（字符串），匿名类不存在 `__name__` 属性。
2. 命名类会令当前作用域中名称为 `identifier` 的变量引用该类最终确定的值（见下）。

**文档字符串**：判断方式与去缩进规则均与函数一致，见 3.4.6。

若类带有前缀装饰器，建立类对象之后，按从近到远（离 `class` 最近的先来）依次调用装饰器，整个表达式的值就是最终的值；
若类是命名的，`identifier` 只在最终值确定后绑定一次，建立过程中不会绑定任何中间值。

要求每个 `decorator` 都是 `Callable`，否则抛出 `TypeError`。

与函数不同，类对象的建立会立即执行类体，过程如下：

1. 从前到后对各基类 `BaseClass` 求值（若有）；若某个 `BaseClass` 的 `__is_final_class__` 为 `True`，抛出 `TypeError`；
2. 新建一个局部帧（即一个局部作用域），压入帧栈；
3. 在该帧中从前到后对类体的各表达式求值；
4. 类体执行完毕后弹出该帧，其局部字典中收集到的每个变量 `v`，按下列规则存为类的**属性**：
    1. 若 `isinstance(v, property)`，直接存入（属性行为由 `property` 自己的 `get`、`set`、`delete` 负责）；
    2. 否则若 `isinstance(v, staticmethod)`，将 `v.func` 存入（纯标签，不是描述器，不参与绑定）；
    3. 否则若 `isinstance(v, classmethod)`，直接存入（绑定 `cls` 由 `classmethod` 自己的 `get` 负责）；
    4. 否则若 `isinstance(v, protocols.Callable)`，将 `MethodDescriptor(v)` 存入；
       `MethodDescriptor` 是 `Descriptor` 的子类；
       get 时返回一个把 `obj` 绑定为第一参数的可调用对象；
    5. 否则原样存入。

属性的读、写、删规则见 3.9.1 所述；`property`、`staticmethod`、`classmethod` 见 4.2。

MRO 的计算：
使用 C3 线性化算法。保证结果是一条确定的线性顺序，且与各基类自身的 MRO、基类声明顺序均不矛盾。
若继承关系本身矛盾无法线性化，则抛出 `TypeError`。

`__abstractmethods__` 的计算：
候选集合为各基类 `__abstractmethods__` 的并集，加上本次新收集的属性中 `__is_abstract_method__` 为 `True` 的名字；
对候选集合中每个名字，按新类自己的 MRO 重新查一次，
查到的结果仍是 `__is_abstract_method__` 则保留，否则（被具体实现覆盖）从集合中去掉；
剩下的即为该类的 `__abstractmethods__`。

`C(x)` 调用时（构造实例）：

1. 若 `C.__abstractmethods__` 非空，抛出 `TypeError`（不能实例化含未实现抽象方法的类）；
2. `obj = C.__new__(C, *args, **kwargs)`；
3. 若 `isinstance(obj, C)`，再调用 `obj.__init__(*args, **kwargs)`；
4. 最终返回 `obj`。

#### 3.4.8 装饰器表达式的值

要求 `decorator` 是 `Callable`，否则抛出 `TypeError`。

`@decorator expr` 的值为 `decorator(expr)`。

例如：

```
func outer() {}
class MyClass {
    not_method = @staticmethod outer # 等价于 not_method = staticmethod(outer)
}
```

### 3.5 函数调用

调用 `x(arg, kwarg=v, ...)` 时，在 `type(x)` 的 MRO 上查找 `__op_call__` 并调用；否则抛出 `TypeError`。

调用中的实参分两组：位置组（位置实参、`*expr` 展开）在前，关键字组（关键字实参、`**expr` 展开）在后；
组内顺序不限，但位置组不能出现在关键字组之后，否则抛出 `SyntaxError`。

对于函数对象的调用，应当给所有形参赋值，或是用传参，或是用默认值。捕获列表中的标识符不是形参，不参与该匹配过程。

实参与形参的对应规则如下：

1. 将定义时所有不可变长形参按顺序排列，形成 N 个槽位；其中出现在可变长位置形参之后的槽位标记为**仅关键字**；
2. 将至多 N 个（设为 p 个）以位置传参的实参从前到后依次填入前 p 个非仅关键字槽位中；
3. 将 k 个所有以关键字传参的实参填入对应的槽位中。若对应槽位已被填入，则抛出 `DispatchError`；
4. 如果有默认值的形参对应的槽位没有被填入，则使用默认值填入对应槽位；
5. 如果仍有槽位未被填入，则抛出 `DispatchError`；
6. 处理多余参数
    1. 若有 `*args` 形参，则多余的以位置传参的实参收集进这个元组。该元组可能为空元组；
       若没有 `*args` 形参且有多余的以位置传参的实参，则抛出 `DispatchError`；
    2. 若有 `**kwargs` 形参，则多余的以关键字传参的实参收集进这个字典。该字典可能为空字典；
       若没有 `**kwargs` 形参且有多余的以关键字传参的实参，则抛出 `DispatchError`。

将实参与形参对应后，会进行类型检查（注意这一步是运行时不是编译期），若类型不匹配，抛出 `DispatchError`；否则运行函数。

对函数族对象的调用，调用时会自上而下依次检查参数是否匹配以及类型是否匹配。

- 若存在匹配的，则运行且只运行第一个匹配的函数；
- 若都不匹配，则抛出 `DispatchError`。

函数返回（无论是 `return` 还是函数运行到最后），都要对返回值进行类型检查。若类型不匹配，则抛出 `TypeError`。

### 3.6 `*` 与 `**` 展开语法

1. `*expr` 只能出现在元组字面量、列表字面量、索引、函数调用中；
2. `**expr` 只能出现在字典字面量、函数调用中。
3. 其他位置会抛出 `SyntaxError`。

运行时检查：

1. `*expr` 要求 `expr` 满足可迭代协议（`protocols.Iterable`）；
2. `**expr` 要求 `expr` 满足映射协议（`protocols.Mapping`）。

例如：

```
a = [1, *mid, 2]       # 列表展开
(a, *b, c) = iterable  # 元组展开，收集多余元素（必须显式写括号）
{**d1, k: v, **d2}     # 字典合并（后边的会覆盖前边的相同键）
f(*args, x=1, **extra) # 调用时展开
```

### 3.7 函数重载

SL 支持函数重载，使用 `FuncGroup` 类显式创建**函数族**（Function Group）对象实现运行时 dispatch，而非通过同名函数定义。

详见 4.2.22 所述。

### 3.8 运算符重载

执行规则：

以 `+` 为例，对于 `a + b`，解释器会进行如下操作：

1. 在 `a` 的类（`type(a)` 及其基类）上查找 `__op_add__`，找到则调用 `type(a).__op_add__(a, b)`；
    - 若类上无 `__op_add__`、或调用返回 `NotImplemented`，则进行第 2 步；
    - 否则返回该调用结果；
2. 在 `b` 的类上查找 `__op_radd__`，找到则调用 `type(b).__op_radd__(b, a)`；
    - 若类上无 `__op_radd__`、或调用返回 `NotImplemented`，则进行第 3 步；
    - 否则返回该调用结果；
3. 抛出 `TypeError`。

若为一元运算符则不做反向重试、参数只有一个操作数。

特殊地，`++x`（`--x` 同理），永远直接调用 `type(x).__op_inc__(x)` 并将结果重新绑定给 `x`。
重载时请注意返回值，通常返回 `None` 与意料不符。

除了 `is`、`and`、`or`、`not`、求属性、`=` 以及所有复合赋值，其他运算符均可重载，对应方法名称如下：

| 运算符                    | 方法名               |
|------------------------|-------------------|
| `x[arg, ...]`          | `__op_index__`    |
| `x(arg, kwarg=v, ...)` | `__op_call__`     |
| `x?`                   | `__op_question__` |
| `x!`                   | `__op_exclam__`   |
| `**`                   | `__op_pow__`      |
| `++x`                  | `__op_inc__`      |
| `--x`                  | `__op_dec__`      |
| `+x`                   | `__op_pos__`      |
| `-x`                   | `__op_neg__`      |
| `~x`                   | `__op_invert__`   |
| `*`                    | `__op_mul__`      |
| `/`                    | `__op_div__`      |
| `//`                   | `__op_floordiv__` |
| `%`                    | `__op_mod__`      |
| `+`                    | `__op_add__`      |
| `-`                    | `__op_sub__`      |
| `..`                   | `__op_range__`    |
| `<<`                   | `__op_lshift__`   |
| `>>`                   | `__op_rshift__`   |
| `&`                    | `__op_and__`      |
| `^`                    | `__op_xor__`      |
| `\|`                   | `__op_or__`       |
| `<`                    | `__op_lt__`       |
| `<=`                   | `__op_le__`       |
| `>`                    | `__op_gt__`       |
| `>=`                   | `__op_ge__`       |
| `!=`                   | `__op_ne__`       |
| `==`                   | `__op_eq__`       |

### 3.9 协议

SL 通过若干**协议**（Protocol）把语言机制开放给对象。
对象只要满足某协议（继承约定的基类并重载其方法，或实现约定的方法），就能参与对应的机制。

本章集中列出这些协议。

**注意**：协议是否成立只按约定的方法是否存在判断，不检查方法能否正确工作。实现有问题时（方法不可调用、
返回值不符合约定等），会在实际用到该方法的地方按一般规则抛出异常（属性不存在 `AttributeError`、
对象不可调用 `TypeError` 等），不在判定协议成立与否的这一步。

#### 3.9.1 属性协议

`obj.attr` 等价于 `getattr(obj, 'attr')`。求属性运算符不可重载，具体行为如下。

##### 3.9.1.1 描述器

**描述器**是满足 `isinstance(o, Descriptor)` 的实例 `o`。
自定义属性行为需继承 `Descriptor` 并重载以下方法：

| 方法                      | 何时调用 | 参数                          |
|-------------------------|------|-----------------------------|
| `get(self, obj)`        | 读属性  | `obj`：经其访问的实例               |
| `set(self, obj, value)` | 写属性  | `obj`：经其访问的实例；`value`：要写入的值 |
| `delete(self, obj)`     | 删属性  | `obj`：经其访问的实例               |

`Descriptor` 把 `get` 标记为 `@abstractmethod`；
`set`、`delete` 则有默认实现，调用即无条件抛出 `AttributeError`，需要可写、可删就重写它们。

##### 3.9.1.2 对属性的操作

读 `o.attr`：

1. 若 `o` 本身是一个类，且它自己的 MRO 上有 `attr` 且是描述器，则返回 `该属性.get(o)`；
2. 否则若 `type(o)` 的 MRO 上有 `attr` 且是描述器，则返回 `该属性.get(o)`；
3. 否则若 `o` 自身属性表中有 `attr`，则返回它；
4. 否则若 `type(o)` 的 MRO 上有 `attr`（非描述器），则返回它；
5. 否则若 `type(o)` 的 MRO 上有 `__getattr__`，则返回 `__getattr__(o, attr)`；
6. 否则 `AttributeError`。

写 `o.attr = v`：

1. 若 `type(o)` 的 MRO 上有 `attr` 且是描述器，则调用 `该属性.set(o, v)`；
2. 否则若 `type(o)` 的 MRO 上有 `__setattr__`，则调用 `__setattr__(o, attr, v)`；
3. 否则写入 `o` 自身属性表（无则新建）。

删 `del o.attr`：

1. 若 `type(o)` 的 MRO 上有 `attr` 且是描述器，则调用 `该属性.delete(o)`；
2. 否则若 `o` 自身属性表中有 `attr`，则从中删除；
3. 否则若 `type(o)` 的 MRO 上有 `__delattr__`，则调用 `__delattr__(o, attr)`；
4. 否则 `AttributeError`。

**属性表**不通过任何属性名暴露，唯一的取得方式是内置函数 `attrs(obj)`（见 4.1.8）。

**注意**：对象对其他对象的引用不止属性表这一种。
解释器内部还会维护一些不通过属性机制暴露的引用，SL 层均访问不到，纯属 C++ 实现细节。但它们是真实的引用，垃圾回收照样要遍历到。

#### 3.9.2 迭代器协议

迭代器协议规定对象如何参与 `for (i : obj)` 及 `*obj` 展开迭代。

`for [$] (i : obj) expr` 等价于

```
{
    iterator = obj.__iter__()
    while [$] (True) {
        i = iterator.__next__()
        if (i is StopIteration) break
        expr
    }
}
```

`*obj` 同理。

##### 3.9.2.1 可迭代对象

实现了 `__iter__` 方法的对象称为可迭代对象。

`__iter__` 的语义为获取对象对应的迭代器。

##### 3.9.2.2 迭代器

实现了 `__iter__` 和 `__next__` 方法的对象称为迭代器。

通常迭代器的 `__iter__` 的返回值为它本身。迭代器一定可迭代。

`__next__` 的语义为从迭代器获取下一个元素；若迭代终止，返回单例 `StopIteration`。

#### 3.9.3 映射协议

映射协议规定对象如何参与 `**obj` 展开、以及按键访问。

同时满足：

1. 可迭代对象；
2. 实现了 `__op_index__` 方法；
3. 实现了 `__items__` 方法；

的对象称为**映射**。

`__iter__(self)` 返回迭代器，该迭代器产出键。
`__items__(self)` 返回迭代器，该迭代器产出 `(k, v)` 元组。

`__iter__`、`__items__` 产出的顺序不作保证。

`**obj` 等价于依次取出 `obj.__items__()` 产出的每个 (k, v)，合并进目标。

### 3.10 作用域

SL 只有 2 种**作用域**：

- 全局（文件）作用域
- 局部（函数体或类体）作用域

`if`、`for`、`while`、`try`、`{ }` 不引入作用域；函数可嵌套调用、类可嵌套定义，故运行期可同时存在多个局部作用域。

#### 3.10.1 帧栈

运行期维护一个帧栈，每次函数调用或类体执行压入一个帧、结束时弹出，栈底为全局帧（文件作用域）。
帧是该次函数调用或类体执行的运行环境，其中包含一个局部变量名到对象的局部字典、**global 标记集**等。
记当前帧（栈顶）的局部字典为 `_L`、全局帧（栈底）的局部字典为 `_G`。
读/写/删一个变量即操作某帧局部字典中的对应项，全局作用域中 `_L` 即 `_G`。

`_L` 只包含存储在当前帧里的名字，包括局部变量、形参、值捕获，不包括引用捕获。

帧弹出（函数调用正常返回、异常传播退出、类体执行完毕）时，把它自己的局部字典置为 `None`。
帧对象本身若仍被某个函数通过引用捕获（3.10.4）持有，会随之继续存在，但只是一个局部字典为 `None` 的空壳，

“确定标识符 `identifier` 操作哪个帧”这个过程称为**作用域确定**，规则见 3.10.3 所述；标识符被捕获时另见 3.10.4。

#### 3.10.2 `global` 声明

对 `global identifier` 求值，要求 `identifier` 是未出现的名称、局部变量之一，而不能是形参、值捕获、引用捕获。

会进行：

1. 若 `identifier` 已经在当前帧的 global 标记集中，则跳过后续步骤；
2. 若 `_L` 中已有该项，则将其删除；
3. 把标识符加入当前帧的 global 标记集。

#### 3.10.3 作用域确定规则

对标识符 `identifier`，若其未被捕获，只涉及当前帧与全局帧，不查找帧栈中的其他帧：

1. 若 `identifier` 在当前帧的 global 标记集中：
    - 读取：返回 `_G` 中该项的值；不存在则抛出 `NameError`。
    - 写入：更新 `_G` 中该项，不存在则在 `_G` 中新建。
    - 删除：删除 `_G` 中该项，不存在则抛出 `NameError`。

2. 否则：
    - 读取：若 `_L` 中有该项，返回它；否则若 `_G` 中有该项，返回它；否则抛出 `NameError`。
    - 写入：更新 `_L` 中该项，不存在则在 `_L` 中新建。
    - 删除：删除 `_L` 中该项，不存在则抛出 `NameError`。

若 `identifier` 被捕获，改为按 3.10.4 的规则处理。

**特例**：`++x`、`--x`、复合赋值 `x op= expr` 内部读取 `x` 旧值这一步，不适用第 2 条读取时退回 `_G` 的部分。
不在 global 标记集时只查 `_L`，找不到直接 `NameError`，以保证这一步的读和随后的写始终落在同一帧。

#### 3.10.4 捕获

函数建立时，可在捕获列表中显式声明它要访问的、定义处所在作用域中的标识符，分两种：

1. **值捕获** `x` 或 `x = expr`：函数建立时，取得一个对象：
    - 写 `x` 则按当时所在作用域的规则读取 `x` 的值；
    - 写 `x = expr` 则对 `expr` 求值。

   将结果对象绑定为该函数的一个“隐藏默认值”，名称即 `x`。
   它不是真正的形参，不参与实参匹配。
   此后函数体内对 `x` 的读、写、删，与普通局部变量/形参完全一致。

2. **引用捕获** `&x`：函数建立时，把 (`x`, 定义帧) 记进该函数自己的**引用捕获表**，不读取也不要求此刻已存在。
   此后函数体内对 `x` 的读、写、删，改为对引用捕获表中 `x` 对应的定义帧按 3.10.3 的规则处理；
   若该定义帧的局部字典已是 `None`（该帧已弹出），则抛出 `NameError`。

**注意**：引用捕获的定义帧只是紧邻的、当前定义处所在的那一帧，不会自动向更外层传递。
若内层函数要跨越多层访问最外层的变量，中间每一层都必须显式转发捕获。

#### 3.10.5 要点与惯用法

**注意**：不被捕获的标识符，读、写都只落在当前帧（或 global 标记集指定的全局帧）。

例如：

```
x = 2
func f() {
    x = 1
    func g() { print(x) }     # g 未捕获 x，x 是全局那个 x
    g()                       # 输出 2
    func k[x]() { print(x) }  # k 值捕获 x，建立时读到 f 局部的 x（此刻为 1）
    func m[&x]() { print(x) } # m 引用捕获 x，绑定到 f 这一帧
    k(); m()                  # 均输出 1
    x = 9
    k(); m()                  # 输出 1 和 9
    return m
}
mm = f()
mm()                          # f 已返回，定义帧局部字典已是 None，NameError
```

引用捕获不会跨层自动传递，中间每一层都要显式转发：

```
func f() {
    n = 0
    func g[&n]() {
        func h[&n]() { n += 1 } # h 的定义帧是 g；n 在 g 自己的引用捕获表里，按 3.10.3/3.10.4 递归处理能继续找到 f 帧
        h()
    }
    g()
    return n     # 1
}
```

值捕获等价于用一个不参与传参的默认值形参捕获对象，支持捕获可变对象：

```
(stack_push, stack_pop) = func () {
    stack = []
    return (
        func [stack](x) { stack.append(x) },
        func [stack]() { return stack.pop() }
    )
} ()
```

**注意**：引用捕获不能用来实现“返回两个共享同一变量的函数”这种逃逸场景，
定义帧一旦返回就不再存活，逃逸出去的函数再访问会直接 `NameError`。
这种场景请仍用值捕获 + 可变对象（如单元素 list 或者自建类当“盒子”），靠盒子本身可变来共享状态：

```
(counter_inc, counter_get) = func () {
    box = [0]
    return (
        func [box]() { box[0] += 1 },
        func [box]() { return box[0] }
    )
} ()
```

引用捕获真正适用的场景是闭包**不逃逸**出定义帧的生存期，例如作为回调在外层函数返回前就被同步调用完：

```
func f() {
    n = 0
    add = func [&n](d) { n += d }
    add(1); add(2)
    return n     # 3。add 只在 f 存活期间被调用，未逃逸
}
```

更推荐的方式仍是使用匿名类显式维护状态，例如：

```
stack = class {
    func __init__(self) { self.data = [] }
    func push(self, x) { self.data.append(x) }
    func pop(self) { return self.data.pop() }
} ()
```

### 3.11 异常

SL 中，`SyntaxError` 在编译期抛出；其他所有异常均在运行时抛出。

该编译期可能包含运行时，因为可以使用 `eval` 函数动态编译。

编译期会尽可能折叠纯字面量子表达式（不做常量传播，一掺变量就不折叠）。
若某步折叠会引发异常，则该步不折叠，留给运行时正常抛出异常。

## 4 内置对象

### 4.1 内置函数

#### 4.1.1 `isinstance(obj, type)`

检查 `obj` 是否为 `type` 类型。其中 `type` 可以是具体类也可以是复合类。

返回 `bool` 类型的值。

具体行为：

1. 若 `type` 有 `__instance_check__` 方法，则返回 `type.__instance_check__(obj)`（若返回值不是 `bool`，抛出 `TypeError`）；
2. 否则返回 `issubclass(type(obj), type)`。

#### 4.1.2 `issubclass(cls, type)`

检查 `cls` 是否为 `type` 的子类（`cls` 本身也算）。其中 `type` 可以是具体类也可以是复合类。

返回 `bool` 类型的值。

具体行为：

1. 若 `type` 有 `__subclass_check__` 方法，则返回 `type.__subclass_check__(cls)`（若返回值不是 `bool`，抛出 `TypeError`）；
2. 否则检查 `cls` 是否为 `type` 或其子类。

#### 4.1.3 `len(obj)`

等价于 `obj.__len__()`，若返回值不是 `int`，抛出 `TypeError`。

#### 4.1.4 `hash(obj)`

等价于 `obj.__hash__()`。若返回值不是 `int`，抛出 `TypeError`。

#### 4.1.5 `getattr(obj, name, ⟦default⟧)`

`name` 为 `str`，等价于 `obj.name`，但属性名在运行时给出。

不带 `default` 时属性不存在则 `AttributeError`；
带 `default` 时属性不存在则返回 `default`。

#### 4.1.6 `setattr(obj, name, value)`

`name` 为 `str`，等价于 `obj.name = value`。

#### 4.1.7 `hasattr(obj, name)`

等价于 `getattr(obj, name)` 不抛 `AttributeError` 则 `True`，否则 `False`。

#### 4.1.8 `attrs(obj)`

返回 `obj` 的自身属性表（见 3.9.1.2），为实时视图，可读可改内容。

#### 4.1.9 `finalclass(cls)`

要求 `cls` 为类，否则抛出 `TypeError`；
若 `cls.__abstractmethods__` 非空，也抛出 `TypeError`。

将 `cls.__is_final_class__` 设为 `True`，返回 `cls` 本身。

#### 4.1.10 `abstractmethod(v)`

将 `v.__is_abstract_method__` 设为 `True`，返回 `v` 本身。

可标在普通方法、`property`、`classmethod` 上；

#### 4.1.11 `unsupported(v)`

将 `v.__is_unsupported__` 设为 `True`，返回 `v` 本身。

用于覆盖继承来的默认协议实现、同时让 `protocols` 模块的鸭子类型检查判定为不满足该协议，见 4.3.2。

#### 4.1.12 `input()`

从标准输入读取一行并返回。

返回 `str` 类型的值。

#### 4.1.13 `print(*args, sep=' ', end='\n')`

除去 `file` 和 `flush` 参数，其与 Python 的 `print` 函数行为完全一致。

返回 `None`。

#### 4.1.14 `import(module_name, lazy=False)`

导入名称为 `module_name` 的模块。

`lazy` 参数表示是否延迟加载模块，若为 `True`，则在首次获取其属性时才会加载模块。
**注意**：延迟加载可能导致异常的延迟发生。

当前仅支持内置模块：`math`, `time`, `numbers`, `protocols`。

返回模块对象。

#### 4.1.15 `eval(code)`

`code` 为 `str`，按 2.2.1 的规则解析为一条或多条表达式。

`eval` 是内联语义，它就在调用处当前帧上直接求值，等价于把 `code` 的内容原样写在调用 `eval` 的位置。因此：

- 读写的 `_L`/`_G`（含 global 标记集）就是调用处当前帧的，`global x` 效果等同写在调用处；
- `code` 中若定义带捕获列表的函数，其定义帧、值捕获取值的作用域，都以调用处当前帧为准；
- `code` 中若有 `return`/`break`/`continue`，其合法性与目标和直接把 `code` 写在调用处完全一致：

`eval` 的值：`code` 的值；若 `code` 中的 `return` 使外层函数返回，则以那次 `return` 的
语义为准（外层函数直接返回，`eval` 这次调用不再产生值）。

#### 4.1.16 `exit(code=0)`

抛出 `SystemExit(code)`（见 4.2.23）。一路传播到解释器顶层无人捕获时，解释器终止，退出码为 `code`。

要求 `code` 为 int 或 `None`，其中 `None` 被视为 0。

### 4.2 内置类

#### 4.2.1 object

所有类的根。可直接实例化（`object()` 得到一个空对象）。
提供各协议的默认实现：
`__op_eq__`、`__hash__`、`__is_final_class__ = False`、`__is_abstract_method__ = False`；`__abstractmethods__ = ()`；
`__new__(cls)` 分配一个 `cls` 的空实例；
`__init__(self)` 什么都不做。

#### 4.2.2 type

唯一元类，`__is_final_class__ = True`；一切类都是 `type` 的实例。

1. `type(x)`：单参数，返回 `x` 的类；
2. `type(name, bases, namespace)`：三参数，动态创建一个类，等价于 `class` 表达式的效果。

#### 4.2.3 NoneType

只有一个实例，即 `None`。

#### 4.2.4 int

表示整数，自带高精度。

继承 `numbers.Real`。

#### 4.2.5 bool

只有两个实例，即 `True` 和 `False`。

继承 int。

#### 4.2.6 float

表示浮点数，底层用 C++ 的 double 实现。

继承 `numbers.Real`。

`float` 不会自动转换为 `int`，即便数值恰好是整数；
反过来 `int` 在某些运算下会自动变成 `float`。

#### 4.2.7 complex

表示复数。

继承 `numbers.Number`。

其中实部和虚部分别用一个 float 存储。

#### 4.2.8 str

表示字符串。严格按 Unicode 码点分割，可迭代且逐码点迭代。

**注意**：str 对象不可变。

#### 4.2.9 tuple

容器类。不可变，可迭代。

包含任意多个对象的引用。

**注意**：“不可变”指的是这些引用关系不可变，不蕴含引用的对象自己不可变。

#### 4.2.10 list

容器类，可变，可迭代。

包含任意多个对象的引用。

#### 4.2.11 dict

可变，键需可哈希。遍历（键、值、键值对）按插入序。满足映射协议（`protocols.Mapping`，见 4.3.2）。

#### 4.2.12 unordered_dict

除不保证遍历顺序外，与 `dict` 接口一致。满足映射协议。

#### 4.2.13 set

容器类，可变，可迭代。

#### 4.2.14 range

可迭代。

1. `range(stop)`
2. `range(start, stop)`
3. `range(start, stop, step)`

#### 4.2.15 SingletonType

包含了 SL 中的部分“单例”：

- Ellipsis
- NotImplemented
- StopIteration

#### 4.2.16 CompoundType

用 `|`, `!`, `?`, `[]` 可以创建**复合类**。

1. `|` 表示两种类型均可。例如 `isinstance(1, int | str)` 为 `True`。
2. `!` 表示精确类（即不允许子类）。例如 `isinstance(1, int!)` 为 `True` 而 `isinstance(True, int!)` 为 `False`。
3. `?` 表示可以为 `None`。例如 `isinstance(None, int?)` 为 `True`。
4. `[]` 对容器类，表示容器中元素的类型。
    1. 对于 `tuple`
        1. 对于 `tuple[int, str]` 可传入多个类型，表示既检查元组长度也检查元素类型
           语义为“元组只能有两个元素且第一个元素是 int 类型且第二个元素是 str 类型”
        2. 对于 `tuple[int, ...]` 要求只能传入两个参数，其中第一个为类型，第二个为 `...`，表示只检查元素类型
           语义为“元组的每个元素都是 int 类型”。注意此种类型检查允许空元组

    2. 对于 `list`，同上

    3. 对于 `dict`
       `dict[str, int]` 表示键的类型均为 `str`，值的类型均为 `int`。注意此种类型检查允许空字典

    4. 对于 `set`
       `set[int]` 表示元素的类型均为 `int`。注意此种类型检查允许空集合

   **注意**：对于此种类型检查，平均时间复杂度至少是 $\mathcal O(n)$ 的，请谨慎使用！
   仔细考虑是否真的需要 `list[int, ...]` 而不是 `list`

以上构造复合类的方式均可嵌套使用，但请**尽量避免嵌套过深**，否则可能会导致性能问题。

以上检查均有短路性，但请不要依赖于此，因为检查的顺序不确定，
例如 `int | str?` 的实际实现*可能*为 `None | int | str` 而非 `int | str | None`。

#### 4.2.17 property

`property(func_get, func_set=None, func_del=None)`，`Descriptor` 的子类。
`func_set`、`func_del` 为 `None` 时对应操作按 `Descriptor` 默认行为抛 `AttributeError`。
`get(self, obj)`：若 `isinstance(obj, type)` 返回 `self`（供内省），否则返回 `func_get(obj)`。

#### 4.2.18 staticmethod

`staticmethod(func)`，`self.func = func`。纯标签，不是描述器，仅在类体收集属性时取出 `v.func` 使用，
本身不会成为类属性。

#### 4.2.19 classmethod

`classmethod(func)`，`Descriptor` 的子类。
`get(self, obj)`：令 `cls = obj if isinstance(obj, type) else type(obj)`，
返回把 `cls` 绑定为第一参数的可调用对象。

#### 4.2.20 Function

`func` 表达式建立的对象的类。实现 `__op_call__`。

#### 4.2.21 super

`super(cls, obj)`。

`super_obj.__getattr__(self, attr)`：

在 `type(obj)` 的 MRO 中找到 `cls` 的位置，从下一个类开始查找 `attr`。
找到且是描述器则 `get(obj)`；
否则原样返回；
全部找不到则 `AttributeError`。

#### 4.2.22 FuncGroup(*functions, name=None)

一个例子足以说明 FuncGroup 的用法：

```
f = FuncGroup(
    func (x: int) { print(1) },
    func (x: str) { print(2) },
    func (x: bool) { print(3) },
    func (x: int, y: int) { print(4) }
)
f(1)    # 输出 1
f('a')  # 输出 2
f(True) # 输出 1
f(1, 2) # 输出 4
f(1.0)  # 抛出 DispatchError
```

#### 4.2.23 异常类

只列全局的一批常用异常，其余更细分的见 4.3.3 `exceptions` 模块。

```
BaseException
├── SystemExit         - exit() 触发，见 4.1.16
├── KeyboardInterrupt  - 用户按下 Ctrl + C
└── Exception
    ├── SyntaxError    - 语法错误。编译期
    ├── TypeError      - 类型错误
    ├── ValueError     - 值不合法
    ├── NameError      - 变量名未找到
    ├── AttributeError - 属性不存在或不支持该操作
    ├── IndexError     - `[]` 下标/键不存在或越界（不再区分序列下标与映射键）
    ├── MathError      - 数学运算错误（除以零、负数开偶次方根、对非正数取对数、对[-1, 1]以外的数取反三角等）
    ├── DispatchError  - 函数调用时参数不匹配
    ├── RecursionError - 递归/调用嵌套过深
    └── IOError        - 输入输出失败
```

### 4.3 内置模块

#### 4.3.1 `numbers`

##### 4.3.1.1 Number

抽象基类。定义数值的公共契约：四则运算与相等比较。`int`、`float`、`complex` 均为其子类。

##### 4.3.1.2 Real

`Number` 的子类，抽象基类。在四则运算之上增加大小比较。`int`、`float` 为其子类；

#### 4.3.2 `protocols`

以下几者的判定规则类似：
`isinstance(obj, X)`/`issubclass(cls, X)` 当且仅当 `type(obj)`/`cls` 的 MRO 上有该协议要求的全部方法，
且没有一个被标记 `__is_unsupported__`（见 4.1.11 `unsupported`）。

##### 4.3.2.1 Callable

抽象基类。要求 `__op_call__`。

##### 4.3.2.2 Indexable

抽象基类。要求 `__op_index__`。

##### 4.3.2.3 Hashable

抽象基类。要求 `__hash__`。

默认按对象身份（同 `is`）计算；可重载 `__hash__(self)` 与 `__op_eq__` 改为按值比较，两者需保持一致
（相等的对象哈希值必须相等）。

`list`、`dict`、`set`、`unordered_dict` 不是 `Hashable`。

##### 4.3.2.4 Iterable

抽象基类。要求 `__iter__`。

##### 4.3.2.5 Iterator

抽象基类。要求 `__iter__` 和 `__next__`。

##### 4.3.2.6 Mapping

抽象基类。

`isinstance(obj, Mapping)` 当且仅当：

1. `isinstance(obj, Iterable)`；
2. `isinstance(obj, Indexable)`；
3. `type(obj)` 的 MRO 上有 `__items__`（未被标记 `__is_unsupported__`）。

`dict`、`unordered_dict` 满足。

#### 4.3.3 `exceptions`

更细分的异常类，用不到就不用 `import`。目前只有：

```
IOError
└── EncodingError - 编码错误，主要在打开文件时
```

以后需要更细分的 IO 异常（如文件不存在、权限不足），继承 `IOError` 加入本模块，不动全局列表。

### 4.4 内置类继承关系图

$$
\text{object}\left\{\begin{array}{l}
\text{NoneType} \\
\text{type} \\
\text{Function} \\
\text{staticmethod} \\
\text{super} \\
\text{SingletonType} \\
\text{FuncGroup} \\
\text{CompoundType} \\
\text{Descriptor}\left\{\begin{array}{l}\text{property} \\ \text{classmethod} \end{array}\right. \\
\text{str} \\ \text{tuple} \\ \text{list} \\ \text{set} \\ \text{range} \\
\text{dict} \\ \text{unordered_dict} \\
\text{numbers.Number}\left\{\begin{array}{l}
\text{complex} \\
\text{numbers.Real}\left\{\begin{array}{l}\text{float} \\ \text{int}\left\{\text{bool}\right.\end{array}\right.
\end{array}\right. \\
\text{BaseException}\left\{\begin{array}{l}
\text{SystemExit} \\ \text{KeyboardInterrupt} \\
\text{Exception}\left\{\begin{array}{l}
\text{SyntaxError} \\ \text{TypeError} \\ \text{ValueError} \\ \text{NameError} \\ \text{AttributeError} \\
\text{IndexError} \\ \text{MathError} \\ \text{DispatchError} \\ \text{RecursionError} \\
\text{IOError} \to \text{exceptions.EncodingError}
\end{array}\right.
\end{array}\right.
\end{array}\right.
$$
