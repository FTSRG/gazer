//@expect fail

#include <assert.h>

float __VERIFIER_nondet_float(void);

int main(void)
{
    double x = __VERIFIER_nondet_float();

    assert(x != 150.0);

    return 0;
}
