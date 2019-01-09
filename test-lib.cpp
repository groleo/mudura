#include "test-lib.h"
#include <stdio.h>

void
__attribute__((noinline))
fun_3()
{
	printf("called fun_3\n");
}

void
__attribute__((noinline))
fun_2()
{
	printf("called fun_2\n");
	fun_3();
}


void
__attribute__((noinline))
fun_1()
{
	printf("called fun_1\n");
	fun_2();
}
