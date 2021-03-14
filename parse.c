#include "k8cc.h"

VarList *locals;

// 関数の循環参照のためのプロトタイプ宣言
Function *function();
Node *declaration();
Node *stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *unary();
Node *primary();

// 構文解析
Node *new_node(NodeKind kind, Token *tkn) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->token = tkn;
    return node;
}

Node *new_node_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tkn) {
    Node *node = new_node(kind, tkn);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node *new_node_num(int value, Token *tkn) {
    Node *node = new_node(NODE_NUM, tkn);
    node->value = value;
    return node;
}

Node *new_unary(NodeKind kind, Node *lhs, Token *tkn) {
    Node *node = new_node(kind, tkn);
    node->lhs = lhs;
    return node;
}

Node *new_var(Var *var, Token *tkn) {
    Node *node = new_node(NODE_VAR, tkn);
    node->var = var;
    return node;
}

Var *find_var(Token *tkn) {
    for (VarList *l = locals; l; l=l->next) {
        Var *var = l->var;
        if (
            strlen(var->name) == tkn->len &&
            !memcmp(tkn->string, var->name, tkn->len)
        ) {
            return var;
        }
    }
    return NULL;
}

Var *push_var(char *name, Type *ty) {
    Var *var = calloc(1, sizeof(Var));
    var->name = name;
    var->ty = ty;

    VarList *vl = calloc(1, sizeof(VarList));
    vl->var = var;
    vl->next = locals;
    locals = vl;
    return var;
}

// Type に関する処理
Type *basetype() {
    // 現時点では int 型のみを実装する
    expect("int");
    Type *ty = int_type();
    while(consume("*")) {
        ty = pointer_to(ty);
    }
    return ty;
}

// function_args = "(" (assign (, assign)*)? ")"
Node *function_args() {
    if (consume(")")) {
        return NULL;
    }

    // 引数が最低一つは存在している前提
    Node *head = assign();
    Node *current_node = head;

    while (consume(",")) {
        // node->args に付け足す処理
        current_node->next = assign();
        current_node = current_node->next;
    }
    expect(")");

    return head;
}

VarList *read_function_param() {
    VarList *vl = calloc(1, sizeof(VarList));
    Type *ty = basetype();
    vl->var = push_var(expect_ident(), ty);
    return vl;
}

// params = ident ("," ident)*
VarList *read_function_params() {
    if (consume(")")) {
        return NULL;
    }

    // 初期化
    // locals に追加しつつ、独自に current_var_list に Var 型のオブジェクトを繋げていっている。
    // なので、f->params を for 文で回しても local 変数は現れず、引数のみ現れる。
    VarList *head = read_function_param();
    VarList *current_var_list = head;

    while (!consume(")")) {
        expect(",");
        current_var_list->next = read_function_param();
        current_var_list = current_var_list->next;
    }

    return head;
}

// program = function*
Function *program() {
    Function head;
    head.next = NULL;
    Function *current_function = &head;

    while (!at_eof()) {
        current_function->next = function();
        current_function = current_function->next;
    }

    return head.next;
}

// function = basetype ident "(" params? ")" "{" stmt* "}"
// params = param ("," param)*
// param = basetype ident
Function *function() {
    locals = NULL;

    Function *function = calloc(1, sizeof(Function));
    basetype();
    function->function_name = expect_ident();
    expect("(");
    // params の処理
    // read_function_params 関数で引数を locals に付け足す実装になっている。
    function->params = read_function_params();
    expect("{");

    // stmt の処理
    Node head;
    head.next = NULL;
    Node *current_node = &head;
    while (!consume("}")) {
        current_node->next = stmt();
        current_node = current_node->next;
    }

    function->node = head.next;
    function->locals = locals;

    return function;
}

// 変数の宣言 ex) int a; int b = 10; などが該当
// declaration = basetype ident ("=" expr) ";"
Node *declaration() {
    Token *tkn = token;
    Type *ty = basetype();
    Var *var = push_var(expect_ident(), ty); // locals には未定義変数が積まれている

    if (consume(";")) {
        return new_node(NODE_NULL, tkn);
    }

    Node *lhs = new_var(var, tkn);
    expect("=");
    Node *rhs = expr();
    Node *node = new_node_binary(NODE_ASSIGN, lhs, rhs, tkn);
    expect(";");
    return new_unary(NODE_EXPR_STMT, node, tkn); // stmt 関数内で呼び出される
}

// stmt = expr ";"
// | "{" stmt* "}"
// | "if" "(" expr ")" stmt ("else" stmt)?
// | "while" "(" expr ")" stmt
// | "for" "(" expr? ";" expr? ";" expr? ")" stmt
// | declaration // 今回の実装では int のみを考慮
// | "return" expr ";"
Node *stmt() {
    Token *tkn;
    if (tkn = consume("return")) {
        Node *node = new_unary(NODE_RETURN, expr(), tkn);
        expect(";");
        return node;
    }

    if (tkn = consume("{")) {
        Node head;
        head.next = NULL;
        Node *current_node = &head;

        while (!consume("}")) {
            current_node->next = stmt();
            current_node = current_node->next;
        }

        Node *node = new_node(NODE_BLOCK, tkn);
        node->body = head.next;
        return node;
    }

    if (tkn = consume("if")) {
        if (consume("(")) {
            Node *node = new_node(NODE_IF, tkn);
            node->condition = expr();
            expect(")");
            node->then = stmt();
            if (consume("else")) {
                node->els = stmt();
            }
            return node;
        }
    }

    if (tkn = consume("while")) {
        if (consume("(")) {
            Node *node = new_node(NODE_WHILE, tkn);
            node->condition = expr();
            expect(")");
            node->then = stmt();
            return node;
        }
    }

    if (tkn = consume("for")) {
        if (consume("(")) {
            Node *node = new_node(NODE_FOR, tkn);
            if (!consume(";")) {
                node->init = expr();
                expect(";");
            }
            if (!consume(";")) {
                node->condition = expr();
                expect(";");
            }
            if (!consume(")")) {
                node->inc = expr();
                expect(")");
            }
            node->then = stmt();
            return node;
        }
    }

    if (tkn = peek("int")) {
        return declaration();
    }

    // Node *node = expr();
    Node *node = new_unary(NODE_EXPR_STMT, expr(), tkn);
    expect(";");
    return node;
}

// expr = assign
Node *expr() { return assign(); }

// assign = equality ("=" assign)?
Node *assign() {
    Node *node = equality();
    Token *tkn;
    if (tkn = consume("=")) {
        node = new_node_binary(NODE_ASSIGN, node, assign(), tkn);
    }
    return node;
}

// equality = relational ("==" relational | "!=" relational)*
Node *equality() {
    Node *node = relational();
    Token *tkn;

    for (;;) {
        if (tkn = consume("==")) {
            node = new_node_binary(NODE_EQ, node, relational(), tkn);
        } else if (tkn = consume("!=")) {
            node = new_node_binary(NODE_NE, node, relational(), tkn);
        } else {
            return node; // 最終的に生成される node
        }
    }
}

// add ("<" add | "<=" add | ">" add | ">=" add )*
Node *relational() {
    Node *node = add();
    Token *tkn;

    for (;;) {
        if (tkn = consume("<")) {
            node = new_node_binary(NODE_LT, node, add(), tkn);
        } else if (tkn = consume("<=")) {
            node = new_node_binary(NODE_LE, node, add(), tkn);
        } else if (tkn = consume(">")) {
            node = new_node_binary(NODE_LT, add(), node, tkn);
        } else if (tkn = consume(">=")) {
            node = new_node_binary(NODE_LE, add(), node, tkn);
        } else {
            return node; // 最終的に生成される node
        }
    }
}

// add ("<" add | "<=" add | ">" add | ">=" add)*
Node *add() {
    Node *node = mul();
    Token *tkn;

    for (;;) {
        if (tkn = consume("+")) {
            node = new_node_binary(NODE_ADD, node, mul(), tkn);
        } else if (tkn = consume("-")) {
            node = new_node_binary(NODE_SUB, node, mul(), tkn);
        } else {
            return node; // 最終的に生成される node
        }
    }
}

// unary ("*" unary | "/" unary)*
Node *mul() {
    Node *node = unary();
    Token *tkn;

    for (;;) {
        if (tkn = consume("*")) {
            node = new_node_binary(NODE_MUL, node, unary(), tkn);
        } else if (tkn = consume("/")) {
            node = new_node_binary(NODE_DIV, node, unary(), tkn);
        } else {
            return node; // 最終的に生成される node
        }
    }
}

// unary = "+"? primary
// "-"? primary
// "*" unary
// "&" unary
Node *unary() {
    Token *tkn;
    if (tkn = consume("*")) {
        return new_unary(NODE_DEREF, unary(), tkn);
    }
    if (tkn = consume("&")) {
        return new_unary(NODE_ADDRESS, unary(), tkn);
    }
    if (consume("+")) {
        return primary();
    }
    if (tkn = consume("-")) {
        return new_node_binary(NODE_SUB, new_node_num(0, tkn), unary(), tkn);
    }
    return primary();
}

// function_args = "(" (assign (, assign)*)? ")"
// primary = num
// | ident function_args?
// | "(" expr ")"
Node *primary() {
    if (consume("(")) {
        Node *node = expr();
        expect(")");
        return node; // この時点で token は ) の次の token を指している
    }

    // 変数に関する処理
    Token *tkn = consume_ident();
    if (tkn) {
        // トークンが関数で使われているかを検証
        if (consume("(")) {
            Node *node = new_node(NODE_FUNCALL, tkn);
            node->function_name = strndup(tkn->string, tkn->len);
            node->args = function_args();
            return node;
        }
        // token が存在にあるか確認する処理
        Var *var = find_var(tkn);
        if (!var) {
            // locals に変数名が積まれていない時 (変数が宣言されていない時) は、エラーを吐く
            error_token(tkn, "undefined variable.");
        }
        return new_var(var, tkn);
    }

    tkn = token;
    if (tkn->kind != TK_NUM) {
        error_token(tkn, "Invalid token.");
    }

    return new_node_num(expect_number(), tkn);  // この時点で token は数字の次の token を指している
}
