#include <stdio.h>
#include <stdlib.h>

void func1() {
	printf("FUNC1!!\n");
}

void func2() {
	printf("FUNC2!!\n");
}

void func3() {
	printf("FUNC3!!\n");
}

void func4() {
	printf("FUNC4!!\n");
}

static int d1 = 0;
int func_d1() {
    if (!d1) d1 = 100;
    printf("func_d1: %d\n", d1);
    d1--;
    return d1;
}

static int d2 = 0;
int func_d2() {
    if (!d2) d2 = 25;
    printf("func_d2: %d\n", d2);
    d2--;
    return d2;
}

static int a = 0;
void func_always(char* buf, int size) {
    // printf("func_always: %d\n", a);
    a++;
    snprintf(buf, size, "func_always: %d", a);
}

int main() {
	return 0;
}
