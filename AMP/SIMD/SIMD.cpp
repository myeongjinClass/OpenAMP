// SIMD.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include "timer.h"
#include <conio.h>
#include <vector>

int _tmain(int argc, _TCHAR* argv[])
{
#define MAX_SIZE2	10000000

	std::vector<int> view_a(MAX_SIZE2);
	std::vector<int> view_b(MAX_SIZE2);
	std::vector<int> view_c(MAX_SIZE2);

	Timer total;

	total.Start();
#pragma loop(no_vector)
	for (unsigned int i = 0; i < MAX_SIZE2; i++)
	{
		view_c[i] = view_a[i] + view_b[i];
	}
	total.Stop();
	printf("%-10s \t%f\n", "no_vector", total.Elapsed());


	total.Start();
	for (unsigned int i = 0; i < MAX_SIZE2; i++)
	{
		view_c[i] = view_a[i] + view_b[i];
	}
	total.Stop();
	printf("%-10s \t%f\n", "vector", total.Elapsed());

	_getch();
	return 0;
}

