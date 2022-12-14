//--------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corp. 
//
// File: timer.cpp
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this 
// file except in compliance with the License. You may obtain a copy of the License at 
// http://www.apache.org/licenses/LICENSE-2.0  
//  
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, 
// EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED WARRANTIES OR 
// CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT. 
//  
// See the Apache Version 2.0 License for specific language governing permissions and 
// limitations under the License.
//--------------------------------------------------------------------------------------
#include "stdafx.h"
#include "timer.h"

// Initialize the resolution of the timer
LARGE_INTEGER Timer::m_freq = \
          (QueryPerformanceFrequency(&Timer::m_freq), Timer::m_freq);

// Calculate the overhead of the timer
LONGLONG Timer::m_overhead = Timer::GetOverhead();