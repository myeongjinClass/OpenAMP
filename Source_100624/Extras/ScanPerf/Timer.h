//===============================================================================
//
// Microsoft Press
// C++ AMP: Accelerated Massive Parallelism with Microsoft Visual C++
//
//===============================================================================
// Copyright (c) 2012 Ade Miller & Kate Gregory.  All rights reserved.
// This code released under the terms of the 
// Microsoft Public License (Ms-PL), http://ampbook.codeplex.com/license.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//===============================================================================

#pragma once

#include <amp.h>

using namespace concurrency;

inline double ElapsedTime(const LARGE_INTEGER& start, const LARGE_INTEGER& end)
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (double(end.QuadPart) - double(start.QuadPart)) * 1000.0 / double(freq.QuadPart);
}

//  Note that this version of timer.h is different from the other versions include in the samples.
//  It has an additional requirement. The case study needs to be able to call use TimeFunc to time
//  subsections of the calculation without forcing a JIT. TimeFunc is always called within further 
//  down the call stack from JitAndTimeFunc with the same function to JIT'ing has already happened.

template <typename Func>
double JitAndTimeFunc(accelerator_view& view, Func f)
{
    //  Ensure that the C++ AMP runtime is initialized.
    //  Ensure that the C++ AMP kernel has been JITed.
    f();

    return TimeFunc(view, f);
}

template <typename Func>
double TimeFunc(accelerator_view& view, Func f)
{
    //  Wait for all previous accelerator work to end.
    view.wait();

    LARGE_INTEGER start, end;
    QueryPerformanceCounter(&start);

    f();

    //  Wait for all accelerator work to end.
    view.wait();
    QueryPerformanceCounter(&end);
    return ElapsedTime(start, end);
}
