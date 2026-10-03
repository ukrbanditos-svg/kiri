/*
 * KiriVN internal compatibility port of wamsoft/layerExImage.
 * Original plugin follows KiriKiri's licensing terms and includes code
 * derived from CxImage under a zlib-style license.
 *
 * This port makes the Windows plugin name "layerExImage.dll" available
 * through Kirikiroid2Yuri's internal plugin loader on Android.
 */

#include "ncbind/ncbind.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>

#define NCB_MODULE_NAME TJS_W("layerExImage.dll")

typedef unsigned char BYTE;
typedef unsigned short WORD;

#ifndef _WIN32
struct RGBQUAD {
    BYTE rgbBlue;
    BYTE rgbGreen;
    BYTE rgbRed;
    BYTE rgbReserved;
};
#endif

struct KiriVNObjectCache {
    typedef iTJSDispatch2* DispatchT;
    typedef tjs_char const* NameT;
    typedef tTVInteger IntegerT;
    typedef ttstr StringT;
    typedef tTJSVariant VariantT;

    KiriVNObjectCache(DispatchT obj, NameT name)
        : _obj(obj), _cache(nullptr), _name(name) {
        tTJSVariant layer;
        TVPExecuteExpression(TJS_W("Layer"), &layer);
        tTJSVariant var;
        if (TJS_SUCCEEDED(layer.AsObjectNoAddRef()->PropGet(
                TJS_IGNOREPROP, name, nullptr, &var, layer.AsObjectNoAddRef()))) {
            _cache = var;
        } else {
            Fail(TJS_W("FAILED: get Layer property object: "));
        }
    }

    ~KiriVNObjectCache() {
        if (_cache) _cache->Release();
    }

    VariantT GetValue() const {
        VariantT var;
        if (TJS_FAILED(_cache->PropGet(0, nullptr, nullptr, &var, _obj)))
            Fail(TJS_W("FAILED: get Layer property value: "));
        return var;
    }

    operator VariantT() const { return GetValue(); }
    operator IntegerT() const { return static_cast<IntegerT>(GetValue()); }

    VariantT operator()(int numparams, VariantT **param) {
        VariantT var;
        if (TJS_FAILED(_cache->FuncCall(
                0, nullptr, nullptr, &var, numparams, param, _obj)))
            Fail(TJS_W("FAILED: call Layer function: "));
        return var;
    }

private:
    DispatchT _obj;
    DispatchT _cache;
    NameT _name;

    void Fail(NameT prefix) const {
        TVPThrowExceptionMessage(prefix, _name);
    }
};

struct KiriVNLayerExBase {
    typedef iTJSDispatch2* DispatchT;
    typedef KiriVNObjectCache ObjectT;
    typedef unsigned char* BufferT;
    typedef tjs_int PitchT;
    typedef tjs_int GeometryT;

    DispatchT _obj;

    KiriVNLayerExBase(DispatchT obj)
        : _obj(obj),
          _pWidth(obj, TJS_W("imageWidth")),
          _pHeight(obj, TJS_W("imageHeight")),
          _pBuffer(obj, TJS_W("mainImageBufferForWrite")),
          _pPitch(obj, TJS_W("mainImageBufferPitch")),
          _pUpdate(obj, TJS_W("update")),
          _pClipLeft(obj, TJS_W("clipLeft")),
          _pClipTop(obj, TJS_W("clipTop")),
          _pClipWidth(obj, TJS_W("clipWidth")),
          _pClipHeight(obj, TJS_W("clipHeight")),
          _width(0), _height(0), _buffer(nullptr), _pitch(0),
          _clipLeft(0), _clipTop(0), _clipWidth(0), _clipHeight(0) {}

    virtual ~KiriVNLayerExBase() {}

    virtual void redraw() {
        tTJSVariant vars[4] = {
            (tjs_int)_clipLeft,
            (tjs_int)_clipTop,
            (tjs_int)_clipWidth,
            (tjs_int)_clipHeight
        };
        tTJSVariant *varsp[4] = { vars, vars + 1, vars + 2, vars + 3 };
        _pUpdate(4, varsp);
    }

    virtual void reset() {
        _width = (GeometryT)(KiriVNObjectCache::IntegerT)_pWidth;
        _height = (GeometryT)(KiriVNObjectCache::IntegerT)_pHeight;
        _buffer = (BufferT)(uintptr_t)(KiriVNObjectCache::IntegerT)_pBuffer;
        _pitch = (PitchT)(KiriVNObjectCache::IntegerT)_pPitch;
        _clipLeft = (GeometryT)(KiriVNObjectCache::IntegerT)_pClipLeft;
        _clipTop = (GeometryT)(KiriVNObjectCache::IntegerT)_pClipTop;
        _clipWidth = (GeometryT)(KiriVNObjectCache::IntegerT)_pClipWidth;
        _clipHeight = (GeometryT)(KiriVNObjectCache::IntegerT)_pClipHeight;
    }

protected:
    ObjectT _pWidth, _pHeight, _pBuffer, _pPitch, _pUpdate;
    GeometryT _width, _height;
    BufferT _buffer;
    PitchT _pitch;
    ObjectT _pClipLeft, _pClipTop, _pClipWidth, _pClipHeight;
    GeometryT _clipLeft, _clipTop, _clipWidth, _clipHeight;
};

class KiriVNLayerExImage : public KiriVNLayerExBase {
public:
    explicit KiriVNLayerExImage(DispatchT obj) : KiriVNLayerExBase(obj) {}

    void reset() override {
        KiriVNLayerExBase::reset();
        _buffer += _clipTop * _pitch + _clipLeft * 4;
        _width = _clipWidth;
        _height = _clipHeight;
    }

    void lut(BYTE *table) {
        BYTE *src = _buffer;
        for (int y = 0; y < _height; ++y) {
            BYTE *p = src;
            for (int x = 0; x < _width; ++x) {
                p[0] = table[p[0]];
                p[1] = table[p[1]];
                p[2] = table[p[2]];
                p += 4;
            }
            src += _pitch;
        }
    }

    void light(int brightness, int contrast) {
        float c = (100 + contrast) / 100.0f;
        brightness += 128;
        BYTE table[256];
        for (int i = 0; i < 256; ++i) {
            table[i] = (BYTE)std::max(
                0, std::min(255, (int)((i - 128) * c + brightness)));
        }
        lut(table);
        redraw();
    }

private:
    static constexpr int HSLMAX = 255;
    static constexpr int RGBMAX = 255;
    static constexpr int HSLUNDEFINED = HSLMAX * 2 / 3;

    static RGBQUAD RGBtoHSL(RGBQUAD in) {
        BYTE R = in.rgbRed, G = in.rgbGreen, B = in.rgbBlue;
        BYTE H, L, S;
        BYTE cMax = std::max(std::max(R, G), B);
        BYTE cMin = std::min(std::min(R, G), B);
        WORD Rdelta, Gdelta, Bdelta;

        L = (BYTE)((((cMax + cMin) * HSLMAX) + RGBMAX) / (2 * RGBMAX));
        if (cMax == cMin) {
            S = 0;
            H = HSLUNDEFINED;
        } else {
            if (L <= HSLMAX / 2)
                S = (BYTE)((((cMax - cMin) * HSLMAX) +
                    ((cMax + cMin) / 2)) / (cMax + cMin));
            else
                S = (BYTE)((((cMax - cMin) * HSLMAX) +
                    ((2 * RGBMAX - cMax - cMin) / 2)) /
                    (2 * RGBMAX - cMax - cMin));

            Rdelta = (WORD)((((cMax - R) * (HSLMAX / 6)) +
                ((cMax - cMin) / 2)) / (cMax - cMin));
            Gdelta = (WORD)((((cMax - G) * (HSLMAX / 6)) +
                ((cMax - cMin) / 2)) / (cMax - cMin));
            Bdelta = (WORD)((((cMax - B) * (HSLMAX / 6)) +
                ((cMax - cMin) / 2)) / (cMax - cMin));

            if (R == cMax) H = (BYTE)(Bdelta - Gdelta);
            else if (G == cMax) H = (BYTE)((HSLMAX / 3) + Rdelta - Bdelta);
            else H = (BYTE)(((2 * HSLMAX) / 3) + Gdelta - Rdelta);
            if (H > HSLMAX) H -= HSLMAX;
        }
        RGBQUAD out = { L, S, H, 0 };
        return out;
    }

    static float HueToRGB(float n1, float n2, float hue) {
        if (hue > 360) hue -= 360;
        else if (hue < 0) hue += 360;

        if (hue < 60) return n1 + (n2 - n1) * hue / 60.0f;
        if (hue < 180) return n2;
        if (hue < 240) return n1 + (n2 - n1) * (240 - hue) / 60.0f;
        return n1;
    }

    static RGBQUAD HSLtoRGB(RGBQUAD hsl) {
        float h = (float)hsl.rgbRed * 360.0f / 255.0f;
        float s = (float)hsl.rgbGreen / 255.0f;
        float l = (float)hsl.rgbBlue / 255.0f;
        float m2 = l <= 0.5f ? l * (1 + s) : l + s - l * s;
        float m1 = 2 * l - m2;

        BYTE r, g, b;
        if (s == 0) {
            r = g = b = (BYTE)(l * 255.0f);
        } else {
            r = (BYTE)(HueToRGB(m1, m2, h + 120) * 255.0f);
            g = (BYTE)(HueToRGB(m1, m2, h) * 255.0f);
            b = (BYTE)(HueToRGB(m1, m2, h - 120) * 255.0f);
        }
        RGBQUAD out = { b, g, r, 0 };
        return out;
    }

    static int hue2rgb(double n1, double n2, double hue) {
        if (hue < 0) hue += 1.0;
        else if (hue > 1.0) hue -= 1.0;

        double color;
        if (hue < 1.0 / 6.0) color = n1 + (n2 - n1) * hue * 6.0;
        else if (hue < 1.0 / 2.0) color = n2;
        else if (hue < 2.0 / 3.0) color = n1 + (n2 - n1) * (2.0 / 3.0 - hue) * 6.0;
        else color = n1;
        return (int)(color * 255.0);
    }

    static void modulatePixel(int &b, int &g, int &r, double h, double s, double l) {
        double red = r / 255.0;
        double green = g / 255.0;
        double blue = b / 255.0;
        double cMax = std::max(std::max(red, green), blue);
        double cMin = std::min(std::min(red, green), blue);
        double delta = cMax - cMin;
        double add = cMax + cMin;
        double luminance = add / 2.0;
        double hue = 0.0;
        double saturation = 0.0;

        if (delta != 0.0) {
            saturation = luminance < 0.5 ? delta / add : delta / (2.0 - add);
            if (red == cMax) hue = (green - blue) / delta;
            else if (green == cMax) hue = 2.0 + (blue - red) / delta;
            else hue = 4.0 + (red - green) / delta;
            hue /= 6.0;
        }

        hue += h;
        while (hue < 0) hue += 1.0;
        while (hue > 1.0) hue -= 1.0;

        if (s > 0) saturation += (1.0 - saturation) * s;
        else saturation += saturation * s;

        if (l > 0) luminance += (1.0 - luminance) * l;
        else luminance += luminance * l;

        if (saturation == 0.0) {
            r = g = b = (int)(luminance * 255.0);
        } else {
            double m2 = luminance <= 0.5
                ? luminance * (1 + saturation)
                : luminance + saturation - luminance * saturation;
            double m1 = 2.0 * luminance - m2;
            r = hue2rgb(m1, m2, hue + 1.0 / 3.0);
            g = hue2rgb(m1, m2, hue);
            b = hue2rgb(m1, m2, hue - 1.0 / 3.0);
        }
    }

public:
    void colorize(int hue, int sat, double blend) {
        if (blend < 0.0) blend = 0.0;
        if (blend > 1.0) blend = 1.0;
        int a0 = (int)(256 * blend);
        int a1 = 256 - a0;
        bool full = blend > 0.999;

        BYTE *src = _buffer;
        for (int y = 0; y < _height; ++y) {
            BYTE *p = src;
            for (int x = 0; x < _width; ++x) {
                RGBQUAD color = { p[0], p[1], p[2], 0 };
                if (full) {
                    color = RGBtoHSL(color);
                    color.rgbRed = (BYTE)hue;
                    color.rgbGreen = (BYTE)sat;
                    color = HSLtoRGB(color);
                } else {
                    RGBQUAD hsl = RGBtoHSL(color);
                    hsl.rgbRed = (BYTE)hue;
                    hsl.rgbGreen = (BYTE)sat;
                    hsl = HSLtoRGB(hsl);
                    color.rgbRed = (BYTE)((hsl.rgbRed * a0 + color.rgbRed * a1) >> 8);
                    color.rgbGreen = (BYTE)((hsl.rgbGreen * a0 + color.rgbGreen * a1) >> 8);
                    color.rgbBlue = (BYTE)((hsl.rgbBlue * a0 + color.rgbBlue * a1) >> 8);
                }
                p[0] = color.rgbBlue;
                p[1] = color.rgbGreen;
                p[2] = color.rgbRed;
                p += 4;
            }
            src += _pitch;
        }
        redraw();
    }

    void modulate(int hue, int saturation, int luminance) {
        double h = hue / 360.0;
        double s = saturation / 100.0;
        double l = luminance / 100.0;

        BYTE *src = _buffer;
        for (int y = 0; y < _height; ++y) {
            BYTE *p = src;
            for (int x = 0; x < _width; ++x) {
                int b = p[0], g = p[1], r = p[2];
                modulatePixel(b, g, r, h, s, l);
                p[0] = (BYTE)b; p[1] = (BYTE)g; p[2] = (BYTE)r;
                p += 4;
            }
            src += _pitch;
        }
        redraw();
    }

    void noise(int level) {
        BYTE *src = _buffer;
        for (int y = 0; y < _height; ++y) {
            BYTE *p = src;
            for (int x = 0; x < _width; ++x) {
                for (int c = 0; c < 3; ++c) {
                    int n = (int)((std::rand() / (float)RAND_MAX - 0.5f) * level);
                    p[c] = (BYTE)std::max(0, std::min(255, (int)p[c] + n));
                }
                p += 4;
            }
            src += _pitch;
        }
        redraw();
    }

    void generateWhiteNoise() {
        BYTE *src = _buffer;
        for (int y = 0; y < _height; ++y) {
            BYTE *p = src;
            for (int x = 0; x < _width; ++x, p += 4) {
                BYTE n = (BYTE)(std::rand() / (RAND_MAX / 255));
                p[0] = p[1] = p[2] = n;
            }
            src += _pitch;
        }
        redraw();
    }

private:
    static std::vector<float> makeGaussianKernel(float radius) {
        radius = std::fabs(0.5f * radius) + 0.25f;
        float stddev = radius;
        float effectRadius = stddev * 2.0f;
        int length = (int)(2 * std::ceil(effectRadius - 0.5f) + 1);
        if (length <= 0) length = 1;
        int mid = length / 2;

        std::vector<float> kernel(length);
        float sum = 0.0f;
        const float denom = 2.0f * stddev * stddev;
        for (int i = 0; i < length; ++i) {
            float x = (float)(i - mid);
            float v = std::exp(-(x * x) / denom);
            kernel[i] = v;
            sum += v;
        }
        if (sum != 0.0f) {
            for (float &v : kernel) v /= sum;
        }
        return kernel;
    }

public:
    void gaussianBlur(float radius) {
        if (_width <= 0 || _height <= 0 || !_buffer) return;
        std::vector<float> kernel = makeGaussianKernel(radius);
        int mid = (int)kernel.size() / 2;
        std::vector<BYTE> temp((size_t)_width * (size_t)_height * 4u);

        // horizontal pass
        for (int y = 0; y < _height; ++y) {
            for (int x = 0; x < _width; ++x) {
                for (int c = 0; c < 4; ++c) {
                    float sum = 0.0f, weight = 0.0f;
                    for (int k = -mid; k <= mid; ++k) {
                        int sx = x + k;
                        if (sx < 0 || sx >= _width) continue;
                        float w = kernel[k + mid];
                        sum += _buffer[y * _pitch + sx * 4 + c] * w;
                        weight += w;
                    }
                    temp[((size_t)y * _width + x) * 4 + c] =
                        (BYTE)std::max(0, std::min(255,
                            (int)(sum / (weight > 0 ? weight : 1.0f) + 0.5f)));
                }
            }
        }

        // vertical pass
        for (int y = 0; y < _height; ++y) {
            for (int x = 0; x < _width; ++x) {
                for (int c = 0; c < 4; ++c) {
                    float sum = 0.0f, weight = 0.0f;
                    for (int k = -mid; k <= mid; ++k) {
                        int sy = y + k;
                        if (sy < 0 || sy >= _height) continue;
                        float w = kernel[k + mid];
                        sum += temp[((size_t)sy * _width + x) * 4 + c] * w;
                        weight += w;
                    }
                    _buffer[y * _pitch + x * 4 + c] =
                        (BYTE)std::max(0, std::min(255,
                            (int)(sum / (weight > 0 ? weight : 1.0f) + 0.5f)));
                }
            }
        }
        redraw();
    }
};

NCB_GET_INSTANCE_HOOK(KiriVNLayerExImage)
{
    NCB_INSTANCE_GETTER(objthis) {
        ClassT *obj = GetNativeInstance(objthis);
        if (!obj) {
            obj = new ClassT(objthis);
            SetNativeInstance(objthis, obj);
        }
        obj->reset();
        return obj;
    }
    ~NCB_GET_INSTANCE_HOOK_CLASS() {}
};

NCB_ATTACH_CLASS_WITH_HOOK(KiriVNLayerExImage, Layer) {
    NCB_METHOD(light);
    NCB_METHOD(colorize);
    NCB_METHOD(modulate);
    NCB_METHOD(noise);
    NCB_METHOD(generateWhiteNoise);
    NCB_METHOD(gaussianBlur);
}
