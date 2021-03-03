#!/bin/bash

assert() {
    expected="$1"
    input="$2"

    ./k8cc "$input" > tmp.s
    gcc -o tmp tmp.s
    ./tmp
    actual="$?"

    if [ "$actual" = "$expected" ]; then
      echo "$input => $actual"
    else
      echo "$input => $expected, but got $actual"
      exit 1
    fi
}

# テストケース
# if statement のテストケース
assert 3 "if (0) return 2; return 3;"
assert 3 "if (1-1) return 2; return 3;"
assert 2 "if (1) return 2; return 3;"
assert 2 "if (2-1) return 2; return 3;"

assert 0 "return 0;"
assert 42 "return 42;"

assert 21 "return 5+20-4;"

assert 41 "return 12 + 34 - 5;"

assert 47 "return 5+6*7;"
assert 15 "return 5*(9-6);"
assert 4 "return (3+5)/2;"

assert 10 "return -10+20;"
assert 10 "return - -10;"
assert 10 "return - - +10;"

# 比較演算子
assert 0 "return 0==1;"
assert 1 "return 42==42;"
assert 1 "return 0!=1;"
assert 0 "return 42!=42;"

assert 1 "return 0<1;"
assert 0 "return 1<1;"
assert 0 "return 2<1;"
assert 1 "return 0<=1;"
assert 1 "return 1<=1;"
assert 0 "return 2<=1;"

assert 1 "return 1>0;"
assert 0 "return 1>1;"
assert 0 "return 1>2;"
assert 1 "return 1>=0;"
assert 1 "return 1>=1;"
assert 0 "return 1>=2;"

# 必ず return 文をつける必要がある
assert 1 "return 1;"
# assert 1 "1;" # ダメなテストケース
assert 3 "1; 2; return 3;"
assert 1 "return 1; 2; 3;"
assert 3 "return 3;"
assert 3 "1 + 2 + 3; 2; return 3;"
# アセンブラの一番初めの ret が評価されてる
assert 2 "1; return 2; 3;"
assert 2 "1; return 2; return 3;"

# 一文字の変数を用いたテストケース
assert 3 "a=3; return a;"
assert 8 "a=3; z=5; return a+z;"
assert 14 "a = 3; b = 5 * 6 - 8; return a + b / 2;"

# 複数文字のローカル変数を用いたテストケース
assert 3 "foo=3; return foo;"
assert 8 "foo123=3; bar=5; return foo123+bar;"

echo OK
