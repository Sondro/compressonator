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

#include "stdafx.h"
#include "Texture.h"
#include "dxgi.h"
#include <assert.h>
#include <string>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "Misc.h"

PluginManager          g_pluginManager;
bool                   g_bAbortCompression = false;
CMIPS*                 g_CMIPS;

using namespace std;
//--------------------------------------------------------------------------------------
// Constructor of the Texture class
// initializes all members
//--------------------------------------------------------------------------------------
Texture::Texture()
{
    m_pTexture2D = NULL;
}
//--------------------------------------------------------------------------------------
// Destructor of the Texture class
//--------------------------------------------------------------------------------------
Texture::~Texture()
{
}

INT32 Texture::OnDestroy()
{
    if (m_pTexture2D != NULL)
    {
        m_pTexture2D->Release();
        m_pTexture2D = NULL;
    }
    return 0;
}

//--------------------------------------------------------------------------------------
// retrieve the GetDxGiFormat from a DDS_PIXELFORMAT
// based on http://msdn.microsoft.com/en-us/library/windows/desktop/bb943991(v=vs.85).aspx
//--------------------------------------------------------------------------------------
UINT32 Texture::GetDxGiFormat(Texture::DDS_PIXELFORMAT pixelFmt) const
{
    if(pixelFmt.flags & 0x00000004)   //DDPF_FOURCC
    {
        // Check for D3DFORMAT enums being set here
        switch(pixelFmt.fourCC)
        {
        case '1TXD':
            return DXGI_FORMAT_BC1_UNORM;
        case '3TXD':
            return DXGI_FORMAT_BC2_UNORM;
        case '5TXD':
            return DXGI_FORMAT_BC3_UNORM;
        case 'U4CB':
            return DXGI_FORMAT_BC4_UNORM;
        case 'A4CB':
            return DXGI_FORMAT_BC4_SNORM;
        case '2ITA':
            return DXGI_FORMAT_BC5_UNORM;
        case 'S5CB':
            return DXGI_FORMAT_BC5_SNORM;
        case 'GBGR':
            return DXGI_FORMAT_R8G8_B8G8_UNORM;
        case 'BGRG':
            return DXGI_FORMAT_G8R8_G8B8_UNORM;
        case 36:
            return DXGI_FORMAT_R16G16B16A16_UNORM;
        case 110:
            return DXGI_FORMAT_R16G16B16A16_SNORM;
        case 111:
            return DXGI_FORMAT_R16_FLOAT;
        case 112:
            return DXGI_FORMAT_R16G16_FLOAT;
        case 113:
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case 114:
            return DXGI_FORMAT_R32_FLOAT;
        case 115:
            return DXGI_FORMAT_R32G32_FLOAT;
        case 116:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        default:
            return 0;
        }
    }
    else
    {
        {
            switch(pixelFmt.bitMaskR)
            {
            case 0xff:
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            case 0x00ff0000:
                return DXGI_FORMAT_B8G8R8A8_UNORM;
            case 0xffff:
                return DXGI_FORMAT_R16G16_UNORM;
            case 0x3ff:
                return DXGI_FORMAT_R10G10B10A2_UNORM;
            case 0x7c00:
                return DXGI_FORMAT_B5G5R5A1_UNORM;
            case 0xf800:
                return DXGI_FORMAT_B5G6R5_UNORM;
            case 0:
                return DXGI_FORMAT_A8_UNORM;
            default:
                return 0;
            };
        }
    }
}

bool Texture::isDXT(DXGI_FORMAT format) const
{
    return (format >= DXGI_FORMAT_BC1_TYPELESS) && (format <= DXGI_FORMAT_BC5_SNORM);
}


//--------------------------------------------------------------------------------------
// return the byte size of a pixel (or block if block compressed)
//--------------------------------------------------------------------------------------
UINT32 Texture::GetPixelSize(DXGI_FORMAT fmt) const
{
    switch(fmt)
    {
    case(DXGI_FORMAT_BC1_TYPELESS) :
    case(DXGI_FORMAT_BC1_UNORM) :
    case(DXGI_FORMAT_BC1_UNORM_SRGB) :
    case(DXGI_FORMAT_BC4_TYPELESS) :
    case(DXGI_FORMAT_BC4_UNORM) :
    case(DXGI_FORMAT_BC4_SNORM) :
        return 8;

    case(DXGI_FORMAT_BC2_TYPELESS) :
    case(DXGI_FORMAT_BC2_UNORM) :
    case(DXGI_FORMAT_BC2_UNORM_SRGB) :
    case(DXGI_FORMAT_BC3_TYPELESS) :
    case(DXGI_FORMAT_BC3_UNORM) :
    case(DXGI_FORMAT_BC3_UNORM_SRGB) :
    case(DXGI_FORMAT_BC5_TYPELESS) :
    case(DXGI_FORMAT_BC5_UNORM) :
    case(DXGI_FORMAT_BC5_SNORM) :
    case(DXGI_FORMAT_BC6H_TYPELESS) :
    case(DXGI_FORMAT_BC6H_UF16) :
    case(DXGI_FORMAT_BC6H_SF16) :
    case(DXGI_FORMAT_BC7_TYPELESS) :
    case(DXGI_FORMAT_BC7_UNORM) :
    case(DXGI_FORMAT_BC7_UNORM_SRGB) :
        return 16;

    default:
        break;
    }
    return 0;
}

void Texture::PatchFmt24To32Bit(unsigned char *pDst, unsigned char *pSrc, UINT32 pixelCount)
{
    // copy pixel data, interleave with A
    for(unsigned int i = 0; i < pixelCount; ++i)
    {
        pDst[0] = pSrc[0];
        pDst[1] = pSrc[1];
        pDst[2] = pSrc[2];
        pDst[3] = 0xFF;
        pDst += 4;
        pSrc += 3;
    }
}

bool Texture::isCubemap() const
{
    return m_header.arraySize == 6;
}

void Texture::InitDebugTexture(ID3D12Device* pDevice, UploadHeapDX12* pUploadHeap)
{
    CD3DX12_RESOURCE_DESC RDescs;
    RDescs = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 32, 32, 1, 1);
    RDescs.Flags |= D3D12_RESOURCE_FLAG_NONE;

    UINT64 DefHeapOffset = 0;

    auto RAInfo = pDevice->GetResourceAllocationInfo(1, 1, &RDescs);

    DefHeapOffset = Align(DefHeapOffset, RAInfo.Alignment);

    pDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT, pUploadHeap->GetNode(), pUploadHeap->GetNodeMask()),
        D3D12_HEAP_FLAG_NONE,
        &RDescs,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_pTexture2D)
        );

    UINT64 UplHeapSize;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedTex2D = { 0 };
    pDevice->GetCopyableFootprints(&RDescs, 0, 1, 0, &placedTex2D, NULL, NULL, &UplHeapSize);

    DefHeapOffset += RAInfo.SizeInBytes;

    UINT8* pixels = pUploadHeap->Suballocate(SIZE_T(UplHeapSize), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    placedTex2D.Offset += UINT64(pixels - pUploadHeap->BasePtr());

    // prepare a pBitmap in memory, with bitmapWidth, bitmapHeight, and pixel format of DXGI_FORMAT_B8G8R8A8_UNORM ...
    for (UINT8 r = 0; r < 32; ++r)
    {
        for (UINT8 g = 0; g < 32; ++g)
        {
            pixels[r*placedTex2D.Footprint.RowPitch + g * 4 + 0] = r * 7;
            pixels[r*placedTex2D.Footprint.RowPitch + g * 4 + 1] = g * 7;
            pixels[r*placedTex2D.Footprint.RowPitch + g * 4 + 2] = 0;
            pixels[r*placedTex2D.Footprint.RowPitch + g * 4 + 3] = 0xFF;
        }
    }

    CD3DX12_TEXTURE_COPY_LOCATION Dst(m_pTexture2D, 0);
    CD3DX12_TEXTURE_COPY_LOCATION Src(pUploadHeap->GetResource(), placedTex2D);
    pUploadHeap->GetCommandList()->CopyTextureRegion(
        &Dst,
        0, 0, 0,
        &Src,
        NULL
        );

    D3D12_RESOURCE_BARRIER RBDesc;
    ZeroMemory(&RBDesc, sizeof(RBDesc));
    RBDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    RBDesc.Transition.pResource = m_pTexture2D;
    RBDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    RBDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    RBDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    pUploadHeap->GetCommandList()->ResourceBarrier(1, &RBDesc);
}

INT32 Texture::InitRendertarget(ID3D12Device* pDevice, UINT width, UINT height, bool bUAV, UINT node, UINT nodemask)
{
    CD3DX12_RESOURCE_DESC RDesc;
    RDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
    RDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if (bUAV)
        RDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    RDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_RESOURCE_ALLOCATION_INFO RAInfo = pDevice->GetResourceAllocationInfo(1, 1, &RDesc);

    // Performance tip: Tell the runtime at resource creation the desired clear value.
    D3D12_CLEAR_VALUE clearValue;
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 1.0f;

    pDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT, node, nodemask),
        D3D12_HEAP_FLAG_NONE,
        &RDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(&m_pTexture2D));

    return 0;
}

void Texture::CreateRTV(DWORD index, RTV *pRV)
{
    if (!m_pTexture2D) return;

    ID3D12Device* pDevice;

    m_pTexture2D->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));
    pDevice->CreateRenderTargetView(m_pTexture2D, nullptr, pRV->GetCPU(index));

    pDevice->Release();
}

void Texture::CreateUAV(DWORD index, CBV_SRV_UAV *pRV)
{
    if (!m_pTexture2D) return;

    ID3D12Device* pDevice;
    m_pTexture2D->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));
    D3D12_RESOURCE_DESC texDesc = m_pTexture2D->GetDesc();

    D3D12_UNORDERED_ACCESS_VIEW_DESC UAViewDesc = {};
    ZeroMemory(&UAViewDesc, sizeof(UAViewDesc));
    UAViewDesc.Format = texDesc.Format;
    UAViewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    pDevice->CreateUnorderedAccessView(m_pTexture2D, NULL, &UAViewDesc, pRV->GetCPU(index));

    pDevice->Release();
}

void Texture::CreateSRV(DWORD index, CBV_SRV_UAV *pRV)
{
    if (!m_pTexture2D) return;

    ID3D12Device* pDevice;
    m_pTexture2D->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));

    D3D12_RESOURCE_DESC texDesc = m_pTexture2D->GetDesc();
    if (texDesc.Format == DXGI_FORMAT_R32_TYPELESS)
    {
        //special case for the depth buffer
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;// m_header.mipMapCount;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        pDevice->CreateShaderResourceView(m_pTexture2D, &srvDesc, pRV->GetCPU(index));
    }
    else
    {
        D3D12_RESOURCE_DESC desc = m_pTexture2D->GetDesc();
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = m_header.mipMapCount;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        pDevice->CreateShaderResourceView(m_pTexture2D, &srvDesc, pRV->GetCPU(index));
    }

    pDevice->Release();
}

void Texture::CreateCubeSRV(DWORD index, CBV_SRV_UAV *pRV)
{
    if (!m_pTexture2D) return;

    ID3D12Device* pDevice;
    m_pTexture2D->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));

    D3D12_RESOURCE_DESC texDesc = m_pTexture2D->GetDesc();
    D3D12_RESOURCE_DESC desc = m_pTexture2D->GetDesc();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.Texture2DArray.ArraySize = m_header.arraySize;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.MipLevels = m_header.mipMapCount;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    pDevice->CreateShaderResourceView(m_pTexture2D, &srvDesc, pRV->GetCPU(index));

    pDevice->Release();
}

void Texture::CreateDSV(DWORD index, DSV *pRV)
{
    if (!m_pTexture2D) return;

    ID3D12Device* pDevice;
    m_pTexture2D->GetDevice(__uuidof(*pDevice), reinterpret_cast<void**>(&pDevice));

    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
    depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    depthStencilViewDesc.Texture2D.MipSlice = 0;
    pDevice->CreateDepthStencilView(m_pTexture2D, &depthStencilViewDesc, pRV->GetCPU(index));

    pDevice->Release();
}

INT32 Texture::InitDepthStencil(ID3D12Device* pDevice, UINT width, UINT height, UINT node, UINT nodemask)
{
    CD3DX12_RESOURCE_DESC RDesc;
    RDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32_TYPELESS, width, height, 1, 1);
    RDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    RDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_RESOURCE_ALLOCATION_INFO RAInfo = pDevice->GetResourceAllocationInfo(1, 1, &RDesc);

    // Performance tip: Tell the runtime at resource creation the desired clear value.
    D3D12_CLEAR_VALUE clearValue;
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    pDevice->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT, node, nodemask),
    D3D12_HEAP_FLAG_NONE,
    &RDesc,
    D3D12_RESOURCE_STATE_DEPTH_WRITE,
    &clearValue,
    IID_PPV_ARGS(&m_pTexture2D));

    return 0;
}

DXGI_FORMAT CMP2DXGIFormat(CMP_FORMAT cmp_format)
{
    DXGI_FORMAT dxgi_format;

    switch (cmp_format)
    {
        // Compression formats ----------
    case CMP_FORMAT_BC1:
    case CMP_FORMAT_DXT1:
        dxgi_format = DXGI_FORMAT_BC1_UNORM;
        break;
    case CMP_FORMAT_BC2:
    case CMP_FORMAT_DXT3:
        dxgi_format = DXGI_FORMAT_BC2_UNORM;
        break;
    case CMP_FORMAT_BC3:
    case CMP_FORMAT_DXT5:
        dxgi_format = DXGI_FORMAT_BC3_UNORM;
        break;
    case CMP_FORMAT_BC4:
    case CMP_FORMAT_ATI1N:
        dxgi_format = DXGI_FORMAT_BC4_UNORM;
        break;
    case CMP_FORMAT_BC5:
    case CMP_FORMAT_ATI2N:
    case CMP_FORMAT_ATI2N_XY:
    case CMP_FORMAT_ATI2N_DXT5:
        dxgi_format = DXGI_FORMAT_BC5_UNORM;
        break;
    case CMP_FORMAT_BC6H:
        dxgi_format = DXGI_FORMAT_BC6H_UF16;
        break;
    case CMP_FORMAT_BC6H_SF:
        dxgi_format = DXGI_FORMAT_BC6H_SF16;
        break;
    case CMP_FORMAT_BC7:
        dxgi_format = DXGI_FORMAT_BC7_UNORM;
        break;
    //uncompressed format
    case CMP_FORMAT_ARGB_8888:
    case CMP_FORMAT_RGBA_8888:
        dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    case CMP_FORMAT_ABGR_8888:
    case CMP_FORMAT_BGRA_8888:
        dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;
        break;
    case CMP_FORMAT_RGB_888:
        dxgi_format = DXGI_FORMAT_B8G8R8A8_UNORM;
        break;
    case CMP_FORMAT_BGR_888:
        dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        break;
    case CMP_FORMAT_RG_8:
        dxgi_format = DXGI_FORMAT_R8_UNORM ;
        break;
    case CMP_FORMAT_R_8:
        dxgi_format = DXGI_FORMAT_R8_UNORM;
        break;
    case CMP_FORMAT_ARGB_2101010:
        dxgi_format = DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM;
        break;
    case CMP_FORMAT_R_16:
        dxgi_format = DXGI_FORMAT_R16_UNORM;
        break;
    case CMP_FORMAT_RGBE_32F:
        dxgi_format = DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
        break;
    case CMP_FORMAT_ARGB_16F:
    case CMP_FORMAT_ABGR_16F:
    case CMP_FORMAT_RGBA_16F:
    case CMP_FORMAT_BGRA_16F:
        dxgi_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        break;
    case CMP_FORMAT_R_16F:
        dxgi_format = DXGI_FORMAT_R16_FLOAT;
        break;
    case CMP_FORMAT_ARGB_32F:
    case CMP_FORMAT_ABGR_32F:
    case CMP_FORMAT_RGBA_32F:
    case CMP_FORMAT_BGRA_32F:
        dxgi_format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        break;
    case CMP_FORMAT_RGB_32F:
    case CMP_FORMAT_BGR_32F:
        dxgi_format = DXGI_FORMAT_R32G32B32_FLOAT;
        break;
    case CMP_FORMAT_RG_32F:
        dxgi_format = DXGI_FORMAT_R32G32_FLOAT;
        break;
    case CMP_FORMAT_R_32F:
        dxgi_format = DXGI_FORMAT_R32_FLOAT;
        break;
        // Unknown compression mapping to Direct X
    case CMP_FORMAT_ASTC:
    case CMP_FORMAT_ATC_RGB:
    case CMP_FORMAT_ATC_RGBA_Explicit:
    case CMP_FORMAT_ATC_RGBA_Interpolated:
    case CMP_FORMAT_DXT5_xGBR:
    case CMP_FORMAT_DXT5_RxBG:
    case CMP_FORMAT_DXT5_RBxG:
    case CMP_FORMAT_DXT5_xRBG:
    case CMP_FORMAT_DXT5_RGxB:
    case CMP_FORMAT_DXT5_xGxR:
    case CMP_FORMAT_ETC_RGB:
    case CMP_FORMAT_ETC2_RGB:
    case CMP_FORMAT_GT:
        // -----------------------------------
    case CMP_FORMAT_Unknown:
    default:
        dxgi_format = DXGI_FORMAT_UNKNOWN;
        break;
    }

    return dxgi_format;
}


//--------------------------------------------------------------------------------------
// entry function to initialize an image from a .DDS texture
//--------------------------------------------------------------------------------------
INT32 Texture::InitFromFile(ID3D12Device* pDevice, UploadHeapDX12* pUploadHeap, void *pluginManager, void *msghandler, const WCHAR *pFilename)
{
    OnDestroy();

    typedef enum RESOURCE_DIMENSION
    {
        RESOURCE_DIMENSION_UNKNOWN = 0,
        RESOURCE_DIMENSION_BUFFER = 1,
        RESOURCE_DIMENSION_TEXTURE1D = 2,
        RESOURCE_DIMENSION_TEXTURE2D = 3,
        RESOURCE_DIMENSION_TEXTURE3D = 4
    } RESOURCE_DIMENSION;

    typedef struct
    {
        UINT32           dxgiFormat;
        RESOURCE_DIMENSION  resourceDimension;
        UINT32           miscFlag;
        UINT32           arraySize;
        UINT32           reserved;
    } DDS_HEADER_DXT10;


    if (PrintStatusLine == NULL) {
        PrintStatusLine = (void(*)(char*))(msghandler);
    }

#ifdef AMDLOAD
    // get the ext and load image with amd compressonator image plugin
    char *fileExt;
    wstring ws(pFilename);
    string sFilename(ws.begin(), ws.end());
    size_t dot = sFilename.find_last_of('.');
    std::string temp;

    if (dot != std::string::npos) {
        temp = (sFilename.substr(dot+1, sFilename.size() - dot));
        std::transform(temp.begin(), temp.end(), temp.begin(), toupper);
        fileExt = (char*)temp.data();
    }
    pMipSet = NULL;
    pMipSet = new MipSet();
    if (pMipSet == NULL) {
        OutputDebugString(TEXT(__FUNCTION__) TEXT(" failed.\n"));
        return -1;
    }
    memset(pMipSet, 0, sizeof(MipSet));

    if (AMDLoadMIPSTextureImage(sFilename.c_str(), pMipSet, false, pluginManager) != 0)
    {
        PrintInfo("Error: reading image, data type not supported.\n");
        return -1;
    }
    
    if (pMipSet)
    {
        if (pMipSet->m_format == CMP_FORMAT_Unknown)
        {
            pMipSet->m_format = GetFormat(pMipSet);
        }

        pMipSet->m_swizzle = KeepSwizzle(pMipSet->m_format);

        if (pMipSet->m_compressed || (pMipSet->m_ChannelFormat == CF_Compressed))
        {
            pMipSet->m_compressed = true;
            Config configsetting;
            configsetting.swizzle = pMipSet->m_swizzle;
            pMipSet = DecompressMIPSet(pMipSet, GPUDecode_INVALID, &configsetting, NULL);
            if (pMipSet == NULL) 
            {
                PrintInfo("Error: reading compressed image.\n");
                return -1;
            }
        }
        if (pMipSet->m_swizzle)
            SwizzleMipMap(pMipSet);
    }
    else
    {
        PrintInfo("Error: reading image, data type not supported.\n");
        return -1;
    }

 
    CMIPS localcMIPS;
    
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedTex2D[6][D3D12_REQ_MIP_LEVELS] = { 0 };
    UINT num_rows[D3D12_REQ_MIP_LEVELS] = { 0 };
    UINT64 row_sizes_in_bytes[D3D12_REQ_MIP_LEVELS] = { 0 };
    UINT32 arraySize = (pMipSet->m_TextureType == TT_CubeMap) ? 6 : 1;
    DXGI_FORMAT dxgiFormat = CMP2DXGIFormat(pMipSet->m_format);
    DDS_HEADER_INFO dx10header =
    {
        pMipSet->m_nWidth,//header->dwWidth,
        pMipSet->m_nHeight, //header->dwHeight,
        1,//header->dwDepth ? header->dwDepth : 1,
        arraySize,
        pMipSet->m_nMipLevels,
        dxgiFormat
    };
    m_header = dx10header;

    CD3DX12_RESOURCE_DESC RDescs;
    RDescs = CD3DX12_RESOURCE_DESC::Tex2D((DXGI_FORMAT)m_header.format, m_header.width, m_header.height, m_header.arraySize, m_header.mipMapCount);

    pDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT, pUploadHeap->GetNode(), pUploadHeap->GetNodeMask()),
        D3D12_HEAP_FLAG_NONE,
        &RDescs,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&m_pTexture2D)
    );


    int offset = 0;

    for (int a = 0; a < m_header.arraySize; a++)
    {
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT *pPlacedTex2D = placedTex2D[a];

        // allocate memory from upload heap
        UINT64 UplHeapSize;
        pDevice->GetCopyableFootprints(&RDescs, 0, m_header.mipMapCount, 0, pPlacedTex2D, num_rows, row_sizes_in_bytes, &UplHeapSize);
        UINT8* pixels = pUploadHeap->Suballocate(SIZE_T(UplHeapSize), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

        UINT32 mipMapCount = m_header.mipMapCount;

        for (UINT mip = 0; mip < mipMapCount; ++mip)
        {
            // skip mipmaps that are smaller than blocksize
            if (mip > 0)
            {
                UINT w0 = pPlacedTex2D[mip - 1].Footprint.Width;
                UINT w1 = pPlacedTex2D[mip].Footprint.Width;
                UINT h0 = pPlacedTex2D[mip - 1].Footprint.Height;
                UINT h1 = pPlacedTex2D[mip].Footprint.Height;
                if ((w0 == w1) && (h0 == h1))
                {
                    --mipMapCount;
                    --mip;
                    continue;
                }
            }
            UINT w = isDXT((DXGI_FORMAT)m_header.format) ? pPlacedTex2D[mip].Footprint.Width / 4 : pPlacedTex2D[mip].Footprint.Width;
            UINT h = isDXT((DXGI_FORMAT)m_header.format) ? pPlacedTex2D[mip].Footprint.Height / 4 : pPlacedTex2D[mip].Footprint.Height;
            UINT bytePP =  4;
            if (pMipSet->m_compressed)
                bytePP = GetPixelSize(dxgiFormat);

            pPlacedTex2D[mip].Footprint.RowPitch = static_cast<UINT>(Align(pPlacedTex2D[mip].Footprint.RowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
            
            MipLevel* mipLevel = localcMIPS.GetMipLevel(pMipSet, mip);
            if (mipLevel->m_pbData == NULL) return -1;

            // read compressonator mipset buffer
            unsigned char *pData = &pixels[pPlacedTex2D[mip].Offset];
            for (UINT y = 0; y < h; ++y)
            {
                UINT8* dest_row_begin = (pData + pPlacedTex2D[mip].Offset) + (y * pPlacedTex2D[mip].Footprint.RowPitch);
                memcpy(&pData[pPlacedTex2D[mip].Footprint.RowPitch*y], &(mipLevel->m_pbData[pPlacedTex2D[mip].Footprint.RowPitch*y]), row_sizes_in_bytes[mip]);
            }
            pPlacedTex2D[mip].Offset += UINT64(pixels - pUploadHeap->BasePtr());
        }
    }

    // copy upload texture to texture heap
    for (int a = 0; a < m_header.arraySize; a++)
    {
        for (UINT mip = 0; mip < m_header.mipMapCount; ++mip)
        {
            D3D12_RESOURCE_DESC texDesc = m_pTexture2D->GetDesc();
            CD3DX12_TEXTURE_COPY_LOCATION Dst(m_pTexture2D, a*m_header.mipMapCount + mip);
            CD3DX12_TEXTURE_COPY_LOCATION Src(pUploadHeap->GetResource(), placedTex2D[a][mip]);
            pUploadHeap->GetCommandList()->CopyTextureRegion(&Dst, 0, 0, 0, &Src, NULL);
        }
    }

    // prepare to shader read
    D3D12_RESOURCE_BARRIER RBDesc;
    ZeroMemory(&RBDesc, sizeof(RBDesc));
    RBDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    RBDesc.Transition.pResource = m_pTexture2D;
    RBDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    RBDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    RBDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    pUploadHeap->GetCommandList()->ResourceBarrier(1, &RBDesc);

    //result = 0;
    if (pMipSet)
        delete pMipSet;
    return 0;

    //======================================================================================
#else
    size_t len = wcslen(pFilename);
    char ext[5] = { 0 };
    size_t numConverted = 0;
    wcstombs_s(&numConverted, ext, 5, &pFilename[len - 4], 4);
    for(int i = 0; i < 4; ++i)
    {
        ext[i] = tolower(ext[i]);
    }

    // check if the extension is known
    UINT32 ext4CC = *reinterpret_cast<const UINT32 *>(ext);
    switch(ext4CC)
    {
    case 'sdd.':
    {
        INT32 result = -1;

        if(GetFileAttributes(pFilename) != 0xFFFFFFFF)
        {
            HANDLE hFile = CreateFile(pFilename,             // file to open
                                      GENERIC_READ,          // open for reading
                                      FILE_SHARE_READ,       // share for reading
                                      NULL,                  // default security
                                      OPEN_EXISTING,         // existing file only
                                      FILE_ATTRIBUTE_NORMAL, // normal file
                                      NULL);                 // no attr. template
            if(hFile != INVALID_HANDLE_VALUE)
            {
                D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedTex2D[6][D3D12_REQ_MIP_LEVELS] = { 0 };
                UINT num_rows[D3D12_REQ_MIP_LEVELS] = { 0 };
                UINT64 row_sizes_in_bytes[D3D12_REQ_MIP_LEVELS] = { 0 };
                UINT64 allocation_offset = 0;

                LARGE_INTEGER largeFileSize;
                GetFileSizeEx(hFile, &largeFileSize);
                assert(0 == largeFileSize.HighPart);
                UINT32 fileSize = largeFileSize.LowPart;
                UINT32 rawTextureSize = fileSize;

                // read the header
                char headerData[4 + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10) ];
                DWORD dwBytesRead = 0;
                if (ReadFile(hFile, headerData, 4 + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10), &dwBytesRead,
                    NULL))
                {
                    char *pByteData = headerData;
                    UINT32 dwMagic = *reinterpret_cast<UINT32 *>(pByteData);
                    pByteData += 4;
                    rawTextureSize -= 4;

                    DDS_HEADER        *header = reinterpret_cast<DDS_HEADER *>(pByteData);
                    pByteData += sizeof(DDS_HEADER);
                    rawTextureSize -= sizeof(DDS_HEADER);

                    DDS_HEADER_DXT10 *header10 = NULL;
                    if (dwMagic == '01XD')   // "DX10"
                    {
                        header10 = reinterpret_cast<DDS_HEADER_DXT10 *>(&pByteData[4]);
                        pByteData += sizeof(DDS_HEADER_DXT10);
                        rawTextureSize -= sizeof(DDS_HEADER_DXT10);

                        DDS_HEADER_INFO dx10header =
                        {
                            header->dwWidth,
                            header->dwHeight,
                            header->dwDepth,
                            header10->arraySize,
                            header->dwMipMapCount,
                            header10->dxgiFormat
                        };
                        m_header = dx10header;
                    }
                    else if (dwMagic == ' SDD')   // "DDS "
                    {
                        // DXGI
                        UINT32 arraySize = (header->dwCubemapFlags == 0xfe00) ? 6 : 1;
                        UINT32 dxgiFormat = GetDxGiFormat(header->ddspf);
                        UINT32 mipMapCount = header->dwMipMapCount ? header->dwMipMapCount : 1;

                        DDS_HEADER_INFO dx10header =
                        {
                            header->dwWidth,
                            header->dwHeight,
                            header->dwDepth ? header->dwDepth : 1,
                            arraySize,
                            mipMapCount,
                            dxgiFormat
                        };
                        m_header = dx10header;
                    }
                    else
                    {
                        return -1;
                    }


                    CD3DX12_RESOURCE_DESC RDescs;
                    RDescs = CD3DX12_RESOURCE_DESC::Tex2D((DXGI_FORMAT)m_header.format, m_header.width, m_header.height, m_header.arraySize, m_header.mipMapCount);

                    pDevice->CreateCommittedResource(
                        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT, pUploadHeap->GetNode(), pUploadHeap->GetNodeMask()),
                        D3D12_HEAP_FLAG_NONE,
                        &RDescs,
                        D3D12_RESOURCE_STATE_COMMON,
                        nullptr,
                        IID_PPV_ARGS(&m_pTexture2D)
                    );

                    if (header->ddspf.bitCount == 24)
                    {
                        // alloc CPU memory & read DDS
                        unsigned char *fileMemory = new unsigned char[rawTextureSize];
                        unsigned char *pFile = fileMemory;
                        SetFilePointer(hFile, fileSize - rawTextureSize, 0, FILE_BEGIN);
                        ReadFile(hFile, fileMemory, rawTextureSize, &dwBytesRead, NULL);

                        for (int a = 0; a < m_header.arraySize; a++)
                        {
                            D3D12_PLACED_SUBRESOURCE_FOOTPRINT *pPlacedTex2D = placedTex2D[a];

                            // allocate memory from upload heap
                            UINT64 UplHeapSize;
                            pDevice->GetCopyableFootprints(&RDescs, 0, m_header.mipMapCount, 0, pPlacedTex2D, num_rows, row_sizes_in_bytes, &UplHeapSize);
                            UINT8* pixels = pUploadHeap->Suballocate(SIZE_T(UplHeapSize), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

                            for (UINT mip = 0; mip < m_header.mipMapCount; ++mip)
                            {
                                // convert DDS to 32 bit
                                unsigned char *pData = &pixels[pPlacedTex2D[mip].Offset];
                                for (UINT y = 0; y < pPlacedTex2D[mip].Footprint.Height; ++y)
                                {
                                    PatchFmt24To32Bit(&pData[pPlacedTex2D[mip].Footprint.RowPitch*y], pFile, pPlacedTex2D[mip].Footprint.Width);
                                    pFile += pPlacedTex2D[mip].Footprint.Width * 3;
                                }

                                pPlacedTex2D[mip].Offset += UINT64(pixels - pUploadHeap->BasePtr());
                            }
                        }
                        delete[] fileMemory;
                    }
                    else
                    {
                        SetFilePointer(hFile, fileSize - rawTextureSize, 0, FILE_BEGIN);

                        int offset = 0;

                        for (int a = 0; a < m_header.arraySize; a++)
                        {
                            D3D12_PLACED_SUBRESOURCE_FOOTPRINT *pPlacedTex2D = placedTex2D[a];

                            // allocate memory from upload heap
                            UINT64 UplHeapSize;
                            pDevice->GetCopyableFootprints(&RDescs, 0, m_header.mipMapCount, 0, pPlacedTex2D, num_rows, row_sizes_in_bytes, &UplHeapSize);
                            UINT8* pixels = pUploadHeap->Suballocate(SIZE_T(UplHeapSize), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

                            UINT32 mipMapCount = m_header.mipMapCount;

                            for (UINT mip = 0; mip < mipMapCount; ++mip)
                            {
                                // skip mipmaps that are smaller than blocksize
                                if (mip > 0)
                                {
                                    UINT w0 = pPlacedTex2D[mip - 1].Footprint.Width;
                                    UINT w1 = pPlacedTex2D[mip].Footprint.Width;
                                    UINT h0 = pPlacedTex2D[mip - 1].Footprint.Height;
                                    UINT h1 = pPlacedTex2D[mip].Footprint.Height;
                                    if ((w0 == w1) && (h0 == h1))
                                    {
                                        --mipMapCount;
                                        --mip;
                                        continue;
                                    }
                                }
                                UINT w = isDXT((DXGI_FORMAT)m_header.format) ? pPlacedTex2D[mip].Footprint.Width / 4 : pPlacedTex2D[mip].Footprint.Width;
                                UINT h = isDXT((DXGI_FORMAT)m_header.format) ? pPlacedTex2D[mip].Footprint.Height / 4 : pPlacedTex2D[mip].Footprint.Height;
                                UINT bytePP = header->ddspf.bitCount != 0 ? header->ddspf.bitCount / 8 : GetPixelSize((DXGI_FORMAT)m_header.format);

                                pPlacedTex2D[mip].Footprint.RowPitch = static_cast<UINT>(Align(pPlacedTex2D[mip].Footprint.RowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));

                                // read DDS
                                unsigned char *pData = &pixels[pPlacedTex2D[mip].Offset];
                                for (UINT y = 0; y < h; ++y)
                                {
                                    UINT8* dest_row_begin = (pData + pPlacedTex2D[mip].Offset) + (y * pPlacedTex2D[mip].Footprint.RowPitch);

                                    ReadFile(hFile, &pData[pPlacedTex2D[mip].Footprint.RowPitch*y], (DWORD)row_sizes_in_bytes[mip], &dwBytesRead, NULL);
                                }
                                pPlacedTex2D[mip].Offset += UINT64(pixels - pUploadHeap->BasePtr());
                            }
                        }
                    }
                }
                CloseHandle(hFile);

                // copy upload texture to texture heap
                for (int a = 0; a < m_header.arraySize; a++)
                {
                    for (UINT mip = 0; mip < m_header.mipMapCount; ++mip)
                    {
                        D3D12_RESOURCE_DESC texDesc = m_pTexture2D->GetDesc();
                        CD3DX12_TEXTURE_COPY_LOCATION Dst(m_pTexture2D, a*m_header.mipMapCount + mip);
                        CD3DX12_TEXTURE_COPY_LOCATION Src(pUploadHeap->GetResource(), placedTex2D[a][mip]);
                        pUploadHeap->GetCommandList()->CopyTextureRegion(&Dst, 0, 0, 0, &Src, NULL);
                    }
                }

                // prepare to shader read
                D3D12_RESOURCE_BARRIER RBDesc;
                ZeroMemory(&RBDesc, sizeof(RBDesc));
                RBDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                RBDesc.Transition.pResource = m_pTexture2D;
                RBDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                RBDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                RBDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

                pUploadHeap->GetCommandList()->ResourceBarrier(1, &RBDesc);

                result = 0;
            }
        }
        if(result != 0)
        {
            OutputDebugString(TEXT(__FUNCTION__) TEXT(" failed.\n"));
            return -1;
        }
        
        return 0;
    }
    default:
        return -2;
    }
#endif
}
