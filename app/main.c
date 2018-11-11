
#include <stdlib.h>
#include <stdio.h>

int base_func(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
	int a = 0;

	a = arg1 + arg3;

	return a;
}


struct student {
	int age;
	int num;
};

int comp_func(int arg1, struct student* std)
{
	std->age = arg1;

	return arg1;
    
}

int main()
{
	int a = 0;
	int b = 1;
	int c = 2;
	int d = 3;
	int e = 4;
	int f = 5;
	struct student std;

	a = base_func(a, b, c, d, e, f);
	c = comp_func(a, &std);

   return 0;
}

