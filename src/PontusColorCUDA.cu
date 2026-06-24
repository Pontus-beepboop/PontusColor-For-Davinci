/* ============================================================================
 *  PontusColor - CUDA backend (Windows / Linux, NVIDIA)
 *  Compiled by nvcc, so __CUDACC__ is defined and PontusColorKernel.h exposes
 *  its device functions.
 * ==========================================================================*/

#include "PontusColorKernel.h"
#include <cuda_runtime.h>

__global__ void PontusColorKernelCUDA(int p_Width, int p_Height, PCparams p_Params,
                                      const float* p_Input, float* p_Output)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    if ((x < p_Width) && (y < p_Height))
    {
        const int i = ((y * p_Width) + x) * 4;
        PCrgb c; c.r = p_Input[i]; c.g = p_Input[i + 1]; c.b = p_Input[i + 2];
        PCrgb o = PontusColorPixel(c, p_Params);
        p_Output[i]     = o.r;
        p_Output[i + 1] = o.g;
        p_Output[i + 2] = o.b;
        p_Output[i + 3] = p_Input[i + 3];
    }
}

void RunCudaKernel(void* p_Stream, int p_Width, int p_Height,
                   const PCparams* p_Params, const float* p_Input, float* p_Output)
{
    dim3 threads(16, 16, 1);
    dim3 blocks(((p_Width  + threads.x - 1) / threads.x),
                ((p_Height + threads.y - 1) / threads.y), 1);
    cudaStream_t stream = static_cast<cudaStream_t>(p_Stream);

    PontusColorKernelCUDA<<<blocks, threads, 0, stream>>>(
        p_Width, p_Height, *p_Params, p_Input, p_Output);
}
