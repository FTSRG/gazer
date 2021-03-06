// RUN: %bmc -bound 1 -no-inline-globals "%s" | FileCheck "%s"

// CHECK: Verification {{(SUCCESSFUL|BOUND REACHED)}}
#include <limits.h>

int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));
void __VERIFIER_assume(int expression);

int b = 1;
int c = 2;

int main(void)
{
    int a = __VERIFIER_nondet_int();
    int d = 3;
    int* ptr;

    if (a == 0) {
        ptr = &b;
    } else {
        ptr = &c;
    }

    if (*ptr > d) {
        __VERIFIER_error();
    }

    return 0;
}