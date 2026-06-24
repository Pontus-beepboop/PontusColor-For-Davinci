/* ============================================================================
 *  PontusColor - OpenFX plugin for the DaVinci Resolve Color page
 *  ---------------------------------------------------------------------------
 *  Appears in Effects -> OpenFX -> PontusColor.  Float RGBA, GPU accelerated
 *  (Metal on macOS, CUDA / OpenCL on Windows) with a multi-threaded CPU
 *  fallback.  All look math lives in PontusColorKernel.h.
 * ==========================================================================*/

#include "PontusColorKernel.h"

#include <cstring>
#include <cmath>
#include <string>
#include <memory>

#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"
#include "ofxsProcessing.h"
#include "ofxsLog.h"

#define kPluginName        "PontusColor"
#define kPluginGrouping    "PontusColor"
#define kPluginDescription "PontusColor - effortless filmic looks with subtractive saturation, " \
                           "log/Rec.709 input handling and creamy highlight rolloff. " \
                           "Drop it on a clip and it already looks like film."
#define kPluginIdentifier  "com.pontusmagnusson.PontusColor"
#define kPluginVersionMajor 1
#define kPluginVersionMinor 0

#define kSupportsTiles              true
#define kSupportsMultiResolution    true
#define kSupportsMultipleClipPARs   false

/* ---- GPU launch entry points (defined in the per-backend files) ---------- */
#ifdef PONTUS_WITH_CUDA
extern void RunCudaKernel(void* p_Stream, int p_Width, int p_Height,
                          const PCparams* p_Params, const float* p_Input, float* p_Output);
#endif
#ifndef __APPLE__
extern void RunOpenCLKernel(void* p_CmdQ, int p_Width, int p_Height,
                            const PCparams* p_Params, const float* p_Input, float* p_Output);
#endif
#ifdef __APPLE__
extern void RunMetalKernel(void* p_CmdQ, int p_Width, int p_Height,
                           const PCparams* p_Params, const float* p_Input, float* p_Output);
#endif

////////////////////////////////////////////////////////////////////////////////
//  Processor
////////////////////////////////////////////////////////////////////////////////
class PontusColorProcessor : public OFX::ImageProcessor
{
public:
    explicit PontusColorProcessor(OFX::ImageEffect& p_Instance) : OFX::ImageProcessor(p_Instance) {}

    virtual void processImagesCuda();
    virtual void processImagesOpenCL();
    virtual void processImagesMetal();
    virtual void multiThreadProcessImages(OfxRectI p_ProcWindow);

    void setSrcImg(OFX::Image* p_SrcImg) { _srcImg = p_SrcImg; }
    void setParams(const PCparams& p)    { _params = p; }

private:
    OFX::Image* _srcImg;
    PCparams    _params;
};

void PontusColorProcessor::processImagesCuda()
{
#ifdef PONTUS_WITH_CUDA
    const OfxRectI& bounds = _srcImg->getBounds();
    const int width  = bounds.x2 - bounds.x1;
    const int height = bounds.y2 - bounds.y1;
    float* input  = static_cast<float*>(_srcImg->getPixelData());
    float* output = static_cast<float*>(_dstImg->getPixelData());
    RunCudaKernel(_pCudaStream, width, height, &_params, input, output);
#endif
}

void PontusColorProcessor::processImagesOpenCL()
{
#ifndef __APPLE__
    const OfxRectI& bounds = _srcImg->getBounds();
    const int width  = bounds.x2 - bounds.x1;
    const int height = bounds.y2 - bounds.y1;
    float* input  = static_cast<float*>(_srcImg->getPixelData());
    float* output = static_cast<float*>(_dstImg->getPixelData());
    RunOpenCLKernel(_pOpenCLCmdQ, width, height, &_params, input, output);
#endif
}

void PontusColorProcessor::processImagesMetal()
{
#ifdef __APPLE__
    const OfxRectI& bounds = _srcImg->getBounds();
    const int width  = bounds.x2 - bounds.x1;
    const int height = bounds.y2 - bounds.y1;
    float* input  = static_cast<float*>(_srcImg->getPixelData());
    float* output = static_cast<float*>(_dstImg->getPixelData());
    RunMetalKernel(_pMetalCmdQ, width, height, &_params, input, output);
#endif
}

void PontusColorProcessor::multiThreadProcessImages(OfxRectI p_ProcWindow)
{
    for (int y = p_ProcWindow.y1; y < p_ProcWindow.y2; ++y)
    {
        if (_effect.abort()) break;
        float* dstPix = static_cast<float*>(_dstImg->getPixelAddress(p_ProcWindow.x1, y));
        for (int x = p_ProcWindow.x1; x < p_ProcWindow.x2; ++x)
        {
            float* srcPix = static_cast<float*>(_srcImg ? _srcImg->getPixelAddress(x, y) : 0);
            if (srcPix)
            {
                PCrgb in; in.r = srcPix[0]; in.g = srcPix[1]; in.b = srcPix[2];
                PCrgb out = PontusColorPixel(in, _params);
                dstPix[0] = out.r; dstPix[1] = out.g; dstPix[2] = out.b;
                dstPix[3] = srcPix[3];
            }
            else
            {
                dstPix[0] = dstPix[1] = dstPix[2] = dstPix[3] = 0.0f;
            }
            dstPix += 4;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//  Plugin instance
////////////////////////////////////////////////////////////////////////////////
class PontusColorPlugin : public OFX::ImageEffect
{
public:
    explicit PontusColorPlugin(OfxImageEffectHandle p_Handle);

    virtual void render(const OFX::RenderArguments& p_Args);
    virtual bool isIdentity(const OFX::IsIdentityArguments& p_Args,
                            OFX::Clip*& p_IdentityClip, double& p_IdentityTime);

    void setupAndProcess(PontusColorProcessor& p_Processor, const OFX::RenderArguments& p_Args);
    PCparams gatherParams(double t);

private:
    OFX::Clip* m_DstClip;
    OFX::Clip* m_SrcClip;

    OFX::ChoiceParam* m_InputSpace;
    OFX::ChoiceParam* m_OutputSpace;
    OFX::ChoiceParam* m_Look;
    OFX::DoubleParam* m_Exposure;
    OFX::DoubleParam* m_Temperature;
    OFX::DoubleParam* m_Tint;
    OFX::DoubleParam* m_Contrast;
    OFX::DoubleParam* m_Pivot;
    OFX::DoubleParam* m_HighlightRolloff;
    OFX::DoubleParam* m_Toe;
    OFX::DoubleParam* m_SubSaturation;
    OFX::DoubleParam* m_Saturation;
    OFX::DoubleParam* m_HighlightDesat;
    OFX::DoubleParam* m_ShadowDesat;
    OFX::DoubleParam* m_FilmWarmth;
    OFX::DoubleParam* m_PrintDensity;
    OFX::DoubleParam* m_LookIntensity;
};

PontusColorPlugin::PontusColorPlugin(OfxImageEffectHandle p_Handle)
    : ImageEffect(p_Handle)
{
    m_DstClip = fetchClip(kOfxImageEffectOutputClipName);
    m_SrcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);

    m_InputSpace       = fetchChoiceParam("inputSpace");
    m_OutputSpace      = fetchChoiceParam("outputSpace");
    m_Look             = fetchChoiceParam("look");
    m_Exposure         = fetchDoubleParam("exposure");
    m_Temperature      = fetchDoubleParam("temperature");
    m_Tint             = fetchDoubleParam("tint");
    m_Contrast         = fetchDoubleParam("contrast");
    m_Pivot            = fetchDoubleParam("pivot");
    m_HighlightRolloff = fetchDoubleParam("highlightRolloff");
    m_Toe              = fetchDoubleParam("toe");
    m_SubSaturation    = fetchDoubleParam("subSaturation");
    m_Saturation       = fetchDoubleParam("saturation");
    m_HighlightDesat   = fetchDoubleParam("highlightDesat");
    m_ShadowDesat      = fetchDoubleParam("shadowDesat");
    m_FilmWarmth       = fetchDoubleParam("filmWarmth");
    m_PrintDensity     = fetchDoubleParam("printDensity");
    m_LookIntensity    = fetchDoubleParam("lookIntensity");
}

PCparams PontusColorPlugin::gatherParams(double t)
{
    PCparams p;
    int iv;
    m_InputSpace->getValueAtTime(t, iv);  p.inputSpace  = iv;
    m_OutputSpace->getValueAtTime(t, iv); p.outputSpace = iv;
    m_Look->getValueAtTime(t, iv);        p.look        = iv;

    double d;
    m_Exposure->getValueAtTime(t, d);         p.exposure         = (float)d;
    m_Temperature->getValueAtTime(t, d);      p.temperature      = (float)d;
    m_Tint->getValueAtTime(t, d);             p.tint             = (float)d;
    m_Contrast->getValueAtTime(t, d);         p.contrast         = (float)d;
    m_Pivot->getValueAtTime(t, d);            p.pivot            = (float)d;
    m_HighlightRolloff->getValueAtTime(t, d); p.highlightRolloff = (float)d;
    m_Toe->getValueAtTime(t, d);              p.toe              = (float)d;
    m_SubSaturation->getValueAtTime(t, d);    p.subSaturation    = (float)d;
    m_Saturation->getValueAtTime(t, d);       p.saturation       = (float)d;
    m_HighlightDesat->getValueAtTime(t, d);   p.highlightDesat   = (float)d;
    m_ShadowDesat->getValueAtTime(t, d);      p.shadowDesat      = (float)d;
    m_FilmWarmth->getValueAtTime(t, d);       p.filmWarmth       = (float)d;
    m_PrintDensity->getValueAtTime(t, d);     p.printDensity     = (float)d;
    m_LookIntensity->getValueAtTime(t, d);    p.lookIntensity    = (float)d;
    return p;
}

void PontusColorPlugin::render(const OFX::RenderArguments& p_Args)
{
    if ((m_DstClip->getPixelDepth() == OFX::eBitDepthFloat) &&
        (m_DstClip->getPixelComponents() == OFX::ePixelComponentRGBA))
    {
        PontusColorProcessor processor(*this);
        setupAndProcess(processor, p_Args);
    }
    else
    {
        OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
    }
}

bool PontusColorPlugin::isIdentity(const OFX::IsIdentityArguments& p_Args,
                                   OFX::Clip*& p_IdentityClip, double& p_IdentityTime)
{
    double mix;
    m_LookIntensity->getValueAtTime(p_Args.time, mix);
    if (mix <= 0.0)
    {
        p_IdentityClip = m_SrcClip;
        p_IdentityTime = p_Args.time;
        return true;
    }
    return false;
}

void PontusColorPlugin::setupAndProcess(PontusColorProcessor& p_Processor,
                                        const OFX::RenderArguments& p_Args)
{
    std::unique_ptr<OFX::Image> dst(m_DstClip->fetchImage(p_Args.time));
    std::unique_ptr<OFX::Image> src(m_SrcClip->fetchImage(p_Args.time));

    p_Processor.setDstImg(dst.get());
    p_Processor.setSrcImg(src.get());
    p_Processor.setGPURenderArgs(p_Args);
    p_Processor.setRenderWindow(p_Args.renderWindow);
    p_Processor.setParams(gatherParams(p_Args.time));
    p_Processor.process();
}

////////////////////////////////////////////////////////////////////////////////
//  Factory
////////////////////////////////////////////////////////////////////////////////
using namespace OFX;

class PontusColorPluginFactory : public OFX::PluginFactoryHelper<PontusColorPluginFactory>
{
public:
    PontusColorPluginFactory()
        : OFX::PluginFactoryHelper<PontusColorPluginFactory>(
              kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor) {}

    virtual void describe(OFX::ImageEffectDescriptor& p_Desc);
    virtual void describeInContext(OFX::ImageEffectDescriptor& p_Desc, OFX::ContextEnum p_Context);
    virtual ImageEffect* createInstance(OfxImageEffectHandle p_Handle, OFX::ContextEnum p_Context);
};

void PontusColorPluginFactory::describe(OFX::ImageEffectDescriptor& p_Desc)
{
    p_Desc.setLabels(kPluginName, kPluginName, kPluginName);
    p_Desc.setPluginGrouping(kPluginGrouping);
    p_Desc.setPluginDescription(kPluginDescription);

    p_Desc.addSupportedContext(eContextFilter);
    p_Desc.addSupportedContext(eContextGeneral);
    p_Desc.addSupportedBitDepth(eBitDepthFloat);

    p_Desc.setSingleInstance(false);
    p_Desc.setHostFrameThreading(false);
    p_Desc.setSupportsMultiResolution(kSupportsMultiResolution);
    p_Desc.setSupportsTiles(kSupportsTiles);
    p_Desc.setTemporalClipAccess(false);
    p_Desc.setRenderTwiceAlways(false);
    p_Desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);

    /* GPU rendering support */
#ifdef __APPLE__
    p_Desc.setSupportsMetalRender(true);
#else
    p_Desc.setSupportsOpenCLBuffersRender(true);
  #ifdef PONTUS_WITH_CUDA
    p_Desc.setSupportsCudaRender(true);
  #endif
#endif
}

static DoubleParamDescriptor* defineSlider(
    OFX::ImageEffectDescriptor& desc, PageParamDescriptor* page, GroupParamDescriptor* group,
    const std::string& name, const std::string& label, const std::string& hint,
    double def, double lo, double hi, double dlo, double dhi, double inc)
{
    DoubleParamDescriptor* p = desc.defineDoubleParam(name);
    p->setLabel(label);
    p->setHint(hint);
    p->setDefault(def);
    p->setRange(lo, hi);
    p->setDisplayRange(dlo, dhi);
    p->setIncrement(inc);
    p->setDoubleType(eDoubleTypePlain);
    if (group) p->setParent(*group);
    if (page)  page->addChild(*p);
    return p;
}

void PontusColorPluginFactory::describeInContext(OFX::ImageEffectDescriptor& p_Desc,
                                                 OFX::ContextEnum /*p_Context*/)
{
    ClipDescriptor* srcClip = p_Desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setIsMask(false);

    ClipDescriptor* dstClip = p_Desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->setSupportsTiles(kSupportsTiles);

    PageParamDescriptor* page = p_Desc.definePageParam("Controls");

    /* ---- Color management ---- */
    GroupParamDescriptor* gCM = p_Desc.defineGroupParam("grpCM");
    gCM->setLabel("Color Management");
    {
        ChoiceParamDescriptor* c = p_Desc.defineChoiceParam("inputSpace");
        c->setLabel("Input Color Space");
        c->setHint("Color space / gamma of the incoming image. Default Rec.709. "
                   "Pick your camera's log to decode it correctly before the look.");
        c->appendOption("Rec.709 (Gamma 2.4)");      // 0
        c->appendOption("Linear (Rec.709)");         // 1
        c->appendOption("Sony S-Log3 / S-Gamut3.Cine"); // 2
        c->appendOption("Sony S-Log3 / S-Gamut3");   // 3
        c->appendOption("RED Log3G10 / REDWideGamutRGB"); // 4
        c->appendOption("ARRI LogC3 / Wide Gamut 3"); // 5
        c->appendOption("ARRI LogC4 / Wide Gamut 4"); // 6
        c->appendOption("Panasonic V-Log / V-Gamut"); // 7
        c->appendOption("Blackmagic Film Gen5 / WG Gen5"); // 8
        c->appendOption("DJI D-Log / D-Gamut");      // 9
        c->appendOption("Canon Log3 / Cinema Gamut"); // 10
        c->appendOption("Fujifilm F-Log2 / F-Gamut"); // 11
        c->appendOption("Cineon (Log)");             // 12
        c->setDefault(PC_IN_REC709);
        c->setParent(*gCM); page->addChild(*c);

        ChoiceParamDescriptor* o = p_Desc.defineChoiceParam("outputSpace");
        o->setLabel("Output Color Space");
        o->setHint("Encoding of the result. Rec.709 gives a display-ready image. "
                   "Use 'Scene Linear (passthrough)' if a Color Space Transform follows.");
        o->appendOption("Rec.709 (Gamma 2.4)");      // 0
        o->appendOption("sRGB");                     // 1
        o->appendOption("Linear (Rec.709)");         // 2
        o->appendOption("Scene Linear (passthrough)"); // 3
        o->setDefault(PC_OUT_REC709);
        o->setParent(*gCM); page->addChild(*o);
    }

    /* ---- Look preset ---- */
    {
        ChoiceParamDescriptor* l = p_Desc.defineChoiceParam("look");
        l->setLabel("Base Look");
        l->setHint("Starting point the sliders layer on top of.");
        l->appendOption("Neutral");          // 0
        l->appendOption("Filmic Subtractive"); // 1
        l->appendOption("Bleach Bypass");    // 2
        l->appendOption("Warm Print");       // 3
        l->appendOption("Cool Modern");      // 4
        l->appendOption("Vintage");          // 5
        l->setDefault(PC_LOOK_FILMIC);
        page->addChild(*l);
    }

    /* ---- Exposure & balance ---- */
    GroupParamDescriptor* gB = p_Desc.defineGroupParam("grpBalance");
    gB->setLabel("Exposure & Balance");
    defineSlider(p_Desc, page, gB, "exposure", "Exposure", "Exposure in stops.",
                 0.0, -6.0, 6.0, -3.0, 3.0, 0.01);
    defineSlider(p_Desc, page, gB, "temperature", "Temperature",
                 "White balance warm (+) / cool (-). Luminance preserving.",
                 0.0, -1.0, 1.0, -1.0, 1.0, 0.01);
    defineSlider(p_Desc, page, gB, "tint", "Tint", "Magenta (+) / green (-).",
                 0.0, -1.0, 1.0, -1.0, 1.0, 0.01);

    /* ---- Tone ---- */
    GroupParamDescriptor* gT = p_Desc.defineGroupParam("grpTone");
    gT->setLabel("Tone");
    defineSlider(p_Desc, page, gT, "contrast", "Contrast",
                 "Log-pivot S-curve contrast. 1 = neutral.", 1.08, 0.0, 2.5, 0.5, 1.8, 0.01);
    defineSlider(p_Desc, page, gT, "pivot", "Contrast Pivot",
                 "Middle-grey the contrast rotates around.", 0.18, 0.05, 0.5, 0.08, 0.30, 0.005);
    defineSlider(p_Desc, page, gT, "highlightRolloff", "Highlight Rolloff",
                 "Filmic shoulder. Higher = softer, creamier highlights.", 0.55, 0.0, 1.0, 0.0, 1.0, 0.01);
    defineSlider(p_Desc, page, gT, "toe", "Toe / Black Soften",
                 "Softens and gently lifts the deepest blacks.", 0.15, 0.0, 1.0, 0.0, 1.0, 0.01);

    /* ---- Color / saturation ---- */
    GroupParamDescriptor* gS = p_Desc.defineGroupParam("grpColor");
    gS->setLabel("Color");
    defineSlider(p_Desc, page, gS, "subSaturation", "Subtractive Saturation",
                 "Film-dye saturation. Deepens colour without going electric. 1 = neutral.",
                 1.15, 0.0, 2.0, 0.0, 2.0, 0.01);
    defineSlider(p_Desc, page, gS, "saturation", "Saturation",
                 "Overall perceptual saturation. 1 = neutral.", 0.95, 0.0, 2.0, 0.0, 2.0, 0.01);
    defineSlider(p_Desc, page, gS, "highlightDesat", "Highlight Desaturation",
                 "Bleeds colour out of highlights (classic film). ", 0.35, 0.0, 1.0, 0.0, 1.0, 0.01);
    defineSlider(p_Desc, page, gS, "shadowDesat", "Shadow Desaturation",
                 "Bleeds colour out of shadows.", 0.15, 0.0, 1.0, 0.0, 1.0, 0.01);

    /* ---- Print ---- */
    GroupParamDescriptor* gP = p_Desc.defineGroupParam("grpPrint");
    gP->setLabel("Print Emulation");
    defineSlider(p_Desc, page, gP, "filmWarmth", "Print Warmth",
                 "Warm shadows / cooler highlights split-tone.", 0.12, -1.0, 1.0, -1.0, 1.0, 0.01);
    defineSlider(p_Desc, page, gP, "printDensity", "Print Density",
                 "Overall richness of blacks. 1 = neutral.", 1.05, 0.0, 2.0, 0.5, 1.6, 0.01);

    /* ---- Master ---- */
    defineSlider(p_Desc, page, 0, "lookIntensity", "Look Intensity",
                 "Dry/wet mix of the whole look against the original.", 1.0, 0.0, 1.0, 0.0, 1.0, 0.01);
}

ImageEffect* PontusColorPluginFactory::createInstance(OfxImageEffectHandle p_Handle,
                                                      OFX::ContextEnum /*p_Context*/)
{
    return new PontusColorPlugin(p_Handle);
}

void OFX::Plugin::getPluginIDs(PluginFactoryArray& p_FactoryArray)
{
    static PontusColorPluginFactory factory;
    p_FactoryArray.push_back(&factory);
}
