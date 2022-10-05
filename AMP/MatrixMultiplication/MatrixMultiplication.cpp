// MatrixMultiplication.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include <amp.h>
#include <ppl.h>
#include <iostream>
#include <assert.h>
#include "timer.h"

using namespace concurrency;

#define DATA_TYPE float

template<typename _type>
void initialize_array(std::vector<_type> &v_data, unsigned size)
{
    for(unsigned i=0; i<size; ++i)
    {
        v_data[i] = (_type)((_type)rand() * 100 / (_type)(RAND_MAX + 1));
    }
}

template<typename _type>
void mxm_single_cpu(int M, int N, int W, const std::vector<_type>& va, const std::vector<_type>& vb, std::vector<_type>& vresult)
{
    for(int k=0; k<M; ++k)
    {
        for(int j=0; j<W; ++j)
        {
            _type result = 0;

            for(int i=0; i<N; ++i)
            {
                int idx_a = k * N + i;
                int idx_b = i * W + j;

                result += va[idx_a] * vb[idx_b];
            }

            vresult[k * W + j] = result;
        }
    }
}

template<typename _type>
void mxm_parall_cpu(int M, int N, int W, const std::vector<_type>& va, const std::vector<_type>& vb, std::vector<_type>& vresult)
{
    for(int k=0; k<M; ++k)
    {
		parallel_for(0, W, [&](int j)
        {
            _type result = 0;

            for(int i=0; i<N; ++i)
            {
                int idx_a = k * N + i;
                int idx_b = i * W + j;

                result += va[idx_a] * vb[idx_b];
            }

            vresult[k * W + j] = result;
        });
	}
}

template<typename _type>
void mxm_amp_simple(int M, int N, int W, const std::vector<_type>& va, const std::vector<_type>& vb, std::vector<_type>& vresult)
{
    extent<2> e_a(M, N), e_b(N, W), e_c(M, W);

    array_view<const _type, 2> av_a(e_a, va); 
    array_view<const _type, 2> av_b(e_b, vb); 
    array_view<_type, 2> av_c(e_c, vresult);
    av_c.discard_data();

    parallel_for_each(av_c.extent, [=](index<2> idx) restrict(amp)
        {
            _type result = 0;

            for(int i = 0; i < av_a.extent[1]; ++i)
            {
                index<2> idx_a(idx[0], i);
                index<2> idx_b(i, idx[1]);

                result += av_a[idx_a] * av_b[idx_b];
            }

            av_c[idx] = result;
        });
    //av_c.synchronize();
}

int _tmain(int argc, _TCHAR* argv[])
{
    srand(2012);

	for (int input = 0; input < 3; input++)
	{
		const int M = 256 + 100 * input;
		const int N = 256 + 100 * input;
		const int W = 256 + 100 * input;
		double time_serial;
		double time_parallel;
		double time_gpu;
		Timer total;

		std::vector<DATA_TYPE> v_a(M * N);
		std::vector<DATA_TYPE> v_b(N * W);
		std::vector<DATA_TYPE> v_c_simple(M * W);
		std::vector<DATA_TYPE> v_c_tiled(M * W);
		std::vector<DATA_TYPE> v_ref(M * W);

		initialize_array(v_a, M * N);
		initialize_array(v_b, N * W);

		// Title
	    printf("C(%d x %d) = A(%d x %d) * B(%d x %d)\n", M, W, M, N, N, W);
		printf("-------------------------------------------\n");
		printf("%-14s %12s %10s\n", "Version", "Time(s)", "Speedup");
		printf("-------------------------------------------\n");

		// Single
		printf("%-15s", "CPU(serial)");
		total.Start();
		mxm_single_cpu(M, N, W, v_a, v_b, v_ref);
		total.Stop();
		time_serial = total.Elapsed();
		printf("%12.6f %10.3f\n", time_serial, 1.0);

		// Parallel
		printf("%-15s", "CPU(parall) ");
		total.Start();
		mxm_parall_cpu(M, N, W, v_a, v_b, v_ref);
		total.Stop();
		time_parallel = total.Elapsed();
		printf("%12.6f %10.3f\n", time_parallel, time_serial / time_parallel);

		// GPU
		printf("%-15s", "GPU");
		total.Start();
		mxm_amp_simple(M, N, W, v_a, v_b, v_c_simple);
		total.Stop();
		time_gpu = total.Elapsed();
		printf("%12.6f %10.3f\n\n", time_gpu, time_serial / time_gpu);
	}
    return 0;
}
