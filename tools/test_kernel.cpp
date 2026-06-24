#include <cstdio>
#include "PontusColorKernel.h"

static PCparams defaults(){
    PCparams p;
    p.inputSpace=PC_IN_REC709; p.outputSpace=PC_OUT_REC709; p.look=PC_LOOK_FILMIC;
    p.exposure=0.0f; p.temperature=0.0f; p.tint=0.0f;
    p.contrast=1.05f; p.pivot=0.18f; p.highlightRolloff=0.5f; p.toe=0.15f;
    p.subSaturation=1.15f; p.saturation=0.95f; p.highlightDesat=0.35f; p.shadowDesat=0.15f;
    p.filmWarmth=0.15f; p.printDensity=1.05f; p.lookIntensity=1.0f;
    return p;
}
static void show(const char* n, PCrgb c, PCparams p){
    PCrgb o=PontusColorPixel(c,p);
    printf("%-22s in(%.3f %.3f %.3f) -> out(%.4f %.4f %.4f)\n",n,c.r,c.g,c.b,o.r,o.g,o.b);
}
int main(){
    PCparams p=defaults();
    PCrgb black={0,0,0}, mid={0.18f,0.18f,0.18f}, grey={0.5f,0.5f,0.5f}, white={1,1,1};
    PCrgb red={0.7f,0.1f,0.1f}, skin={0.62f,0.45f,0.38f}, sky={0.25f,0.45f,0.75f};
    printf("=== Default filmic look (Rec709 in/out) ===\n");
    show("black",black,p); show("mid 0.18",mid,p); show("grey 0.5",grey,p);
    show("white",white,p); show("red",red,p); show("skin",skin,p); show("sky",sky,p);

    printf("\n=== Identity check (look=NEUTRAL, all neutral, intensity 1) ===\n");
    PCparams id=defaults(); id.look=PC_LOOK_NEUTRAL; id.contrast=1.0f; id.highlightRolloff=0.0f;
    id.toe=0.0f; id.subSaturation=1.0f; id.saturation=1.0f; id.highlightDesat=0.0f;
    id.shadowDesat=0.0f; id.filmWarmth=0.0f; id.printDensity=1.0f;
    show("mid 0.18",mid,id); show("grey 0.5",grey,id); show("skin",skin,id);

    printf("\n=== Log inputs decode + look (should land in display range) ===\n");
    PCparams ps=defaults();
    ps.inputSpace=PC_IN_SLOG3; PCrgb sl={0.42f,0.40f,0.38f}; show("slog3 midgrey",sl,ps);
    ps.inputSpace=PC_IN_REDLOG3G10; PCrgb rl={0.46f,0.44f,0.42f}; show("redlog3g10",rl,ps);
    ps.inputSpace=PC_IN_LOGC3; PCrgb lc={0.39f,0.38f,0.37f}; show("logc3",lc,ps);
    ps.inputSpace=PC_IN_VLOG; PCrgb vl={0.42f,0.41f,0.40f}; show("vlog",vl,ps);
    ps.inputSpace=PC_IN_LOGC4; PCrgb l4={0.30f,0.29f,0.28f}; show("logc4",l4,ps);

    printf("\n=== White balance warm/cool ===\n");
    PCparams pw=defaults(); pw.temperature=0.6f;  show("warm +0.6",grey,pw);
    pw.temperature=-0.6f; show("cool -0.6",grey,pw);
    pw.temperature=0.0f; pw.tint=0.5f; show("tint +0.5",grey,pw);
    return 0;
}
