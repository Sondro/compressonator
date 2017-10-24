// AMD AMDUtils code
// 
// Copyright(c) 2017 Advanced Micro Devices, Inc.All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#pragma once
#include "Ring.h"

// This class, on creation allocates a number of command lists. Using a ring buffer
// these commandLists are recycled when they are no longer used by the GPU. See the 
// 'ring.h' for more details on allocation and recycling 
//
class CommandListRingDX12
{
    DWORD m_memTotalSize;
    RingWithTabs m_mem;

    ID3D12CommandAllocator        **m_ppCommandAllocator;
    ID3D12GraphicsCommandList    **m_ppCommandList;            

public:
    void OnCreate(ID3D12Device* pDevice, ID3D12CommandQueue *pQueue, DWORD numberOfBackBuffers, DWORD memTotalSize, UINT nodeMask=0);
    void OnDestroy();
    ID3D12GraphicsCommandList *GetNewCommandList();
    void OnBeginFrame();
};

