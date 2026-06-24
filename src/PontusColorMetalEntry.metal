/* Metal entry point. At build time PontusColorKernel.h is prepended to this
 * file (see CMakeLists.txt -> PontusColorKernelSourceMetal), so PCrgb, PCparams
 * and PontusColorPixel() are defined above when the runtime compiles this.
 * The header already does #include <metal_stdlib>; using namespace metal; */

kernel void PontusColorKernelMetal(constant int&      p_Width   [[buffer(0)]],
                                   constant int&      p_Height  [[buffer(1)]],
                                   constant PCparams& p_Params  [[buffer(2)]],
                                   device const float* p_Input  [[buffer(3)]],
                                   device float*       p_Output [[buffer(4)]],
                                   uint2 gid [[thread_position_in_grid]])
{
    if ((int)gid.x < p_Width && (int)gid.y < p_Height)
    {
        const int i = (((int)gid.y * p_Width) + (int)gid.x) * 4;
        PCrgb c; c.r = p_Input[i]; c.g = p_Input[i + 1]; c.b = p_Input[i + 2];
        /* copy into a thread-space value so device functions taking PCparams*
         * (thread) compile cleanly */
        PCparams pp = p_Params;
        PCrgb o = PontusColorPixel(c, pp);
        p_Output[i]     = o.r;
        p_Output[i + 1] = o.g;
        p_Output[i + 2] = o.b;
        p_Output[i + 3] = p_Input[i + 3];
    }
}
