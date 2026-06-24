/* OpenCL entry point. At build time PontusColorKernel.h is prepended to this
 * file (see CMakeLists.txt -> PontusColorKernelSource), so PCrgb, PCparams and
 * PontusColorPixel() are all defined above when the runtime compiles this. */

__kernel void PontusColorKernelOpenCL(const int p_Width, const int p_Height,
                                      PCparams p_Params,
                                      __global const float* p_Input,
                                      __global float* p_Output)
{
    const int x = get_global_id(0);
    const int y = get_global_id(1);
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
