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

//----------------------------------------------------------------------------
// CPU parallel implementation.
//----------------------------------------------------------------------------

#pragma once

#include "IReduce.h"
#include "Timer.h"
#include <vector>
#include <ppl.h>

using namespace concurrency;

class ParallelReduction : public IReduce
{
public:
    int Reduce(accelerator_view& view, const std::vector<int>& source, double& computeTime) const
    {
        int total;
        computeTime = TimeFunc(view, [&]()
        {
            total = parallel_reduce(source.cbegin(), source.cend(), 0, std::plus<int>());
        });
        return total;
    }
};
