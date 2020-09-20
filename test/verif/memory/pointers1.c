// RUN: %bmc -bound 1 -memory=flat "%s" | FileCheck "%s"
// RUN: %bmc -memory=simple -bound 1 "%s" | FileCheck "%s"

// CHECK: Verification {{(SUCCESSFUL|BOUND REACHED)}}

int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));

void make_symbolic(void* ptr);

int a, b;

int main(void)
{
    int* pA = &a;
    int* pB = &b;

    a = __VERIFIER_nondet_int();
    b = a;

    if (*pA != *pB) {
        __VERIFIER_error();
    }

    return 0;
}
