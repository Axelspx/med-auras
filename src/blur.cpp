#include "blur.h"

#include "composition_abi.h"

#include <d2d1_1.h>
#include <d2d1effects.h>
#include <d3d11.h>
#include <dxgi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <vector>

using namespace composition_abi;

#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif

namespace {

// MinGW's d2d1effects.h predates the Gaussian blur optimization enum. The values are the
// documented D2D1_GAUSSIANBLUR_OPTIMIZATION / D2D1_BORDER_MODE ones.
constexpr UINT32 gaussian_blur_optimization_speed = 0;
constexpr UINT32 border_mode_hard = 1;

struct DispatcherQueueOptions {
    DWORD size;
    int thread_type;
    int apartment_type;
};

void log_failure(const wchar_t* what, const HRESULT result) {
#ifndef NDEBUG
    wchar_t message[256];
    swprintf(message, 256, L"MedAuras blur unavailable: %ls (0x%08lX)\n", what, static_cast<unsigned long>(result));
    OutputDebugStringW(message);
#else
    static_cast<void>(what);
    static_cast<void>(result);
#endif
}

template <typename T>
void release(T*& value) {
    if (!value) return;
    value->Release();
    value = nullptr;
}

// The Windows Runtime and CoreMessaging entry points are resolved at run time. A machine without
// them simply reports no blur instead of failing to start the process.
struct RuntimeEntryPoints {
    HRESULT(WINAPI* create_string)(PCWSTR, UINT32, HSTRING*){};
    HRESULT(WINAPI* delete_string)(HSTRING){};
    HRESULT(WINAPI* activate_instance)(HSTRING, IInspectable**){};
    HRESULT(WINAPI* get_activation_factory)(HSTRING, REFIID, void**){};
    HRESULT(WINAPI* create_dispatcher_queue)(DispatcherQueueOptions, void**){};
    bool loaded{};
};

RuntimeEntryPoints runtime;

bool load_runtime() {
    if (runtime.loaded) return true;
    const HMODULE combase = LoadLibraryW(L"combase.dll");
    const HMODULE messaging = LoadLibraryW(L"CoreMessaging.dll");
    if (!combase || !messaging) return false;
    const auto load = [](const HMODULE module, const char* name) {
        return reinterpret_cast<void*>(GetProcAddress(module, name));
    };
    runtime.create_string =
        reinterpret_cast<decltype(runtime.create_string)>(load(combase, "WindowsCreateString"));
    runtime.delete_string =
        reinterpret_cast<decltype(runtime.delete_string)>(load(combase, "WindowsDeleteString"));
    runtime.activate_instance =
        reinterpret_cast<decltype(runtime.activate_instance)>(load(combase, "RoActivateInstance"));
    runtime.get_activation_factory = reinterpret_cast<decltype(runtime.get_activation_factory)>(
        load(combase, "RoGetActivationFactory"));
    runtime.create_dispatcher_queue = reinterpret_cast<decltype(runtime.create_dispatcher_queue)>(
        load(messaging, "CreateDispatcherQueueController"));
    runtime.loaded = runtime.create_string && runtime.delete_string && runtime.activate_instance &&
                     runtime.get_activation_factory && runtime.create_dispatcher_queue;
    return runtime.loaded;
}

// Owns one HSTRING for the duration of a call.
class ScopedString {
public:
    explicit ScopedString(const wchar_t* text) {
        if (runtime.create_string) {
            runtime.create_string(text, static_cast<UINT32>(wcslen(text)), &value_);
        }
    }
    ~ScopedString() {
        if (value_ && runtime.delete_string) runtime.delete_string(value_);
    }
    ScopedString(const ScopedString&) = delete;
    ScopedString& operator=(const ScopedString&) = delete;
    HSTRING get() const { return value_; }

private:
    HSTRING value_{};
};

// The Gaussian blur node of the effect graph.
//
// Windows Composition consumes an effect description through IGraphicsEffectD2D1Interop: it asks
// for a D2D effect CLSID, its property values, and its sources, then builds and owns the real
// pipeline. Implementing that interface is the documented way to describe an effect without
// pulling in Win2D, and it is the whole of the graph here -- backdrop in, blurred pixels out. The
// dark tint is a separate composition brush rather than another node, so the widget's own
// foreground pixels stay at full alpha.
class GaussianBlurEffect final : public IGraphicsEffect, public IGraphicsEffectD2D1Interop {
public:
    GaussianBlurEffect(IGraphicsEffectSource* source, const float deviation)
        : source_{source}, deviation_{deviation} {
        if (source_) source_->AddRef();
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_POINTER;
        if (iid == IID_IUnknown || iid == __uuidof(IInspectable) || iid == iid_graphics_effect ||
            iid == iid_graphics_effect_source) {
            // IGraphicsEffectSource adds no slots of its own, so the IGraphicsEffect vtable
            // satisfies it.
            *object = static_cast<IGraphicsEffect*>(this);
        } else if (iid == iid_graphics_effect_d2d1_interop) {
            *object = static_cast<IGraphicsEffectD2D1Interop*>(this);
        } else {
            *object = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = --references_;
        if (remaining == 0) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE GetIids(ULONG* count, IID** iids) override {
        if (count) *count = 0;
        if (iids) *iids = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING* name) override {
        if (!name || !runtime.create_string) return E_POINTER;
        const wchar_t* text = L"Windows.UI.Composition.Effects.GaussianBlurEffect";
        return runtime.create_string(text, static_cast<UINT32>(wcslen(text)), name);
    }

    HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel* level) override {
        if (level) *level = BaseTrust;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_Name(HSTRING* value) override {
        if (!value || !runtime.create_string) return E_POINTER;
        return runtime.create_string(L"Blur", 4, value);
    }

    HRESULT STDMETHODCALLTYPE put_Name(HSTRING) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE GetEffectId(GUID* id) override {
        if (!id) return E_POINTER;
        *id = CLSID_D2D1GaussianBlur;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetNamedPropertyMapping(
        LPCWSTR name, UINT* index, GraphicsEffectPropertyMapping* mapping) override {
        if (!name || !index || !mapping) return E_POINTER;
        if (_wcsicmp(name, L"StandardDeviation") != 0) return E_INVALIDARG;
        *index = D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION;
        *mapping = graphics_effect_property_mapping_direct;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPropertyCount(UINT* count) override {
        if (!count) return E_POINTER;
        *count = 3;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetProperty(const UINT index, IInspectable** value) override {
        if (!value) return E_POINTER;
        *value = nullptr;
        IPropertyValueStatics* statics{};
        const ScopedString class_name{L"Windows.Foundation.PropertyValue"};
        if (!class_name.get()) return E_FAIL;
        HRESULT result = runtime.get_activation_factory(
            class_name.get(), iid_property_value_statics, reinterpret_cast<void**>(&statics));
        if (FAILED(result)) return result;
        switch (index) {
        case D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION:
            result = statics->CreateSingle(deviation_, value);
            break;
        case D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION:
            result = statics->CreateUInt32(gaussian_blur_optimization_speed, value);
            break;
        case D2D1_GAUSSIANBLUR_PROP_BORDER_MODE:
            result = statics->CreateUInt32(border_mode_hard, value);
            break;
        default:
            result = E_INVALIDARG;
            break;
        }
        statics->Release();
        return result;
    }

    HRESULT STDMETHODCALLTYPE GetSource(const UINT index, IGraphicsEffectSource** source) override {
        if (!source) return E_POINTER;
        if (index != 0) return E_INVALIDARG;
        *source = source_;
        if (source_) source_->AddRef();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetSourceCount(UINT* count) override {
        if (!count) return E_POINTER;
        *count = 1;
        return S_OK;
    }

private:
    ~GaussianBlurEffect() { release(source_); }

    ULONG references_{1};
    IGraphicsEffectSource* source_{};
    float deviation_{};
};

struct CardGeometry {
    std::vector<POINT> origins;
    int width{};
    int height{};
    float radius{};

    bool operator==(const CardGeometry& other) const {
        return width == other.width && height == other.height && radius == other.radius &&
               origins.size() == other.origins.size() &&
               std::equal(
                   origins.begin(), origins.end(), other.origins.begin(),
                   [](const POINT& a, const POINT& b) { return a.x == b.x && a.y == b.y; });
    }
};

struct State {
    bool ready{};
    blur::Tokens tokens{};

    ID3D11Device* d3d_device{};
    ID2D1Factory1* d2d_factory{};
    ID2D1Device* d2d_device{};
    ID2D1DeviceContext* d2d_context{};

    ICompositor* compositor{};
    ICompositor2* compositor2{};
    ICompositorInterop* compositor_interop{};
    ICompositorDesktopInterop* desktop_interop{};
    ICompositionGraphicsDevice* graphics_device{};

    IUnknown* window_target{};
    ICompositionTarget* target{};
    IVisual* root{};
    IVisualCollection* children{};

    // Shared across every card: one blurred-backdrop brush and one tint brush, each presented
    // through the same rounded-card alpha mask.
    ICompositionBrush* card_blur_brush{};
    ICompositionBrush* card_tint_brush{};
    ICompositionDrawingSurface* mask_surface{};

    ICompositionDrawingSurface* content_surface{};
    IVisual* content_visual{};
    ID2D1Bitmap1* staging{};
    int content_width{};
    int content_height{};

    CardGeometry geometry;
    std::vector<IVisual*> card_visuals;
};

State state;

bool create_render_device() {
    const D3D_FEATURE_LEVEL levels[]{
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,
    };
    HRESULT result = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels,
        ARRAYSIZE(levels), D3D11_SDK_VERSION, &state.d3d_device, nullptr, nullptr);
    if (FAILED(result)) {
        result = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels,
            ARRAYSIZE(levels), D3D11_SDK_VERSION, &state.d3d_device, nullptr, nullptr);
    }
    if (FAILED(result)) {
        log_failure(L"no Direct3D device", result);
        return false;
    }

    IDXGIDevice* dxgi_device{};
    result = state.d3d_device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgi_device));
    if (SUCCEEDED(result)) {
        const D2D1_FACTORY_OPTIONS options{D2D1_DEBUG_LEVEL_NONE};
        result = D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options,
            reinterpret_cast<void**>(&state.d2d_factory));
    }
    if (SUCCEEDED(result)) {
        result = state.d2d_factory->CreateDevice(dxgi_device, &state.d2d_device);
    }
    if (SUCCEEDED(result)) {
        result = state.d2d_device->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &state.d2d_context);
    }
    release(dxgi_device);
    if (FAILED(result)) log_failure(L"no Direct2D device", result);
    return SUCCEEDED(result);
}

bool create_compositor() {
    // Windows Composition requires a dispatcher queue on the calling thread. Bound to the current
    // thread, it is serviced by the widget's existing message loop, so no extra thread or pump
    // appears.
    void* controller{};
    const DispatcherQueueOptions options{
        sizeof(DispatcherQueueOptions), 2 /* DQTYPE_THREAD_CURRENT */, 0 /* DQTAT_COM_NONE */};
    const HRESULT queue_result = runtime.create_dispatcher_queue(options, &controller);
    if (FAILED(queue_result)) {
        log_failure(L"no dispatcher queue", queue_result);
        return false;
    }

    const ScopedString class_name{L"Windows.UI.Composition.Compositor"};
    IInspectable* instance{};
    HRESULT result = class_name.get() ? runtime.activate_instance(class_name.get(), &instance) : E_FAIL;
    if (SUCCEEDED(result)) {
        result = instance->QueryInterface(iid_compositor, reinterpret_cast<void**>(&state.compositor));
    }
    if (SUCCEEDED(result)) {
        result = instance->QueryInterface(iid_compositor2, reinterpret_cast<void**>(&state.compositor2));
    }
    if (SUCCEEDED(result)) {
        result = instance->QueryInterface(
            iid_compositor_interop, reinterpret_cast<void**>(&state.compositor_interop));
    }
    if (SUCCEEDED(result)) {
        result = instance->QueryInterface(
            iid_compositor_desktop_interop, reinterpret_cast<void**>(&state.desktop_interop));
    }
    if (SUCCEEDED(result)) {
        result = state.compositor_interop->CreateGraphicsDevice(state.d2d_device, &state.graphics_device);
    }
    release(instance);
    if (FAILED(result)) log_failure(L"no compositor", result);
    return SUCCEEDED(result);
}

// Backdrop -> Gaussian blur -> effect brush. The compositor keeps the backdrop live on its own.
bool create_card_brushes() {
    ICompositionBackdropBrush* backdrop{};
    HRESULT result = state.compositor2->CreateBackdropBrush(&backdrop);
    ICompositionBrush* backdrop_brush{};
    if (SUCCEEDED(result)) {
        result = backdrop->QueryInterface(iid_composition_brush, reinterpret_cast<void**>(&backdrop_brush));
    }
    release(backdrop);

    ICompositionEffectSourceParameter* parameter{};
    IGraphicsEffectSource* parameter_source{};
    if (SUCCEEDED(result)) {
        const ScopedString class_name{L"Windows.UI.Composition.CompositionEffectSourceParameter"};
        ICompositionEffectSourceParameterFactory* factory{};
        result = class_name.get() ? runtime.get_activation_factory(
                                        class_name.get(),
                                        iid_composition_effect_source_parameter_factory,
                                        reinterpret_cast<void**>(&factory))
                                  : E_FAIL;
        if (SUCCEEDED(result)) {
            const ScopedString name{L"Backdrop"};
            result = factory->Create(name.get(), &parameter);
        }
        if (SUCCEEDED(result)) {
            result = parameter->QueryInterface(
                iid_graphics_effect_source, reinterpret_cast<void**>(&parameter_source));
        }
        release(factory);
    }
    release(parameter);

    ICompositionEffectFactory* effect_factory{};
    ICompositionEffectBrush* effect_brush{};
    if (SUCCEEDED(result)) {
        auto* effect = new GaussianBlurEffect{parameter_source, state.tokens.blur_amount};
        result = state.compositor->CreateEffectFactory(effect, &effect_factory);
        effect->Release();
    }
    if (SUCCEEDED(result)) result = effect_factory->CreateBrush(&effect_brush);
    if (SUCCEEDED(result)) {
        const ScopedString name{L"Backdrop"};
        result = effect_brush->SetSourceParameter(name.get(), backdrop_brush);
    }
    release(effect_factory);
    release(parameter_source);
    release(backdrop_brush);

    ICompositionBrush* blurred{};
    if (SUCCEEDED(result)) {
        result = effect_brush->QueryInterface(iid_composition_brush, reinterpret_cast<void**>(&blurred));
    }
    release(effect_brush);

    // The tint is a plain colour brush laid over the blur inside the same card mask, rather than
    // another node in the effect graph. Visually identical, and it keeps the card's text and icons
    // out of any translucency: only these two composition layers are see-through.
    ICompositionColorBrush* colour{};
    ICompositionBrush* tint{};
    if (SUCCEEDED(result)) {
        const WinColor value{
            static_cast<BYTE>(std::clamp(state.tokens.tint_opacity, 0.0F, 1.0F) * 255.0F + 0.5F),
            state.tokens.tint_red, state.tokens.tint_green, state.tokens.tint_blue};
        result = state.compositor->CreateColorBrushWithColor(value, &colour);
    }
    if (SUCCEEDED(result)) {
        result = colour->QueryInterface(iid_composition_brush, reinterpret_cast<void**>(&tint));
    }
    release(colour);

    if (FAILED(result)) {
        release(blurred);
        release(tint);
        log_failure(L"no backdrop effect brush", result);
        return false;
    }
    state.card_blur_brush = blurred;
    state.card_tint_brush = tint;
    return true;
}

ICompositionDrawingSurface* create_surface(const int width, const int height) {
    ICompositionDrawingSurface* surface{};
    const WinSize size{static_cast<float>(std::max(1, width)), static_cast<float>(std::max(1, height))};
    const HRESULT result = state.graphics_device->CreateDrawingSurface(
        size, directx_pixel_format_b8g8r8a8, directx_alpha_mode_premultiplied, &surface);
    if (FAILED(result)) {
        log_failure(L"no drawing surface", result);
        return nullptr;
    }
    return surface;
}

// BeginDraw hands back a device context plus the offset of this surface inside a shared atlas, so
// every drawing call has to be translated by it.
ID2D1DeviceContext* begin_surface_draw(ICompositionDrawingSurface* surface, POINT& offset) {
    ICompositionDrawingSurfaceInterop* interop{};
    if (FAILED(surface->QueryInterface(
            iid_composition_drawing_surface_interop, reinterpret_cast<void**>(&interop)))) {
        return nullptr;
    }
    ID2D1DeviceContext* context{};
    const HRESULT result = interop->BeginDraw(
        nullptr, __uuidof(ID2D1DeviceContext), reinterpret_cast<void**>(&context), &offset);
    interop->Release();
    if (FAILED(result)) return nullptr;
    context->SetDpi(96.0F, 96.0F);
    return context;
}

void end_surface_draw(ICompositionDrawingSurface* surface, ID2D1DeviceContext*& context) {
    release(context);
    ICompositionDrawingSurfaceInterop* interop{};
    if (SUCCEEDED(surface->QueryInterface(
            iid_composition_drawing_surface_interop, reinterpret_cast<void**>(&interop)))) {
        interop->EndDraw();
        interop->Release();
    }
}

// The rounded card silhouette, drawn once per layout change. Direct2D's per-primitive
// anti-aliasing supplies the corner quality; a composition clip would give binary edges, which is
// exactly what the layered-window work removed.
bool draw_card_mask(const int width, const int height, const float radius) {
    release(state.mask_surface);
    state.mask_surface = create_surface(width, height);
    if (!state.mask_surface) return false;

    POINT offset{};
    ID2D1DeviceContext* context = begin_surface_draw(state.mask_surface, offset);
    if (!context) return false;
    context->SetTransform(
        D2D1::Matrix3x2F::Translation(static_cast<float>(offset.x), static_cast<float>(offset.y)));
    context->Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
    ID2D1SolidColorBrush* brush{};
    if (SUCCEEDED(context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 1.0F), &brush))) {
        context->FillRoundedRectangle(
            D2D1::RoundedRect(
                D2D1::RectF(0.0F, 0.0F, static_cast<float>(width), static_cast<float>(height)),
                radius, radius),
            brush);
        release(brush);
    }
    end_surface_draw(state.mask_surface, context);
    return true;
}

// One card layer: the shared brush shown through the shared card mask.
IVisual* create_card_layer(ICompositionBrush* source, const POINT origin, const int width, const int height) {
    ICompositionSurfaceBrush* mask_brush{};
    ICompositionSurface* mask{};
    HRESULT result = state.mask_surface->QueryInterface(
        iid_composition_surface, reinterpret_cast<void**>(&mask));
    if (SUCCEEDED(result)) result = state.compositor->CreateSurfaceBrushWithSurface(mask, &mask_brush);
    release(mask);

    ICompositionBrush* mask_as_brush{};
    if (SUCCEEDED(result)) {
        result = mask_brush->QueryInterface(iid_composition_brush, reinterpret_cast<void**>(&mask_as_brush));
    }
    release(mask_brush);

    ICompositionMaskBrush* masked{};
    ICompositionBrush* masked_as_brush{};
    if (SUCCEEDED(result)) result = state.compositor2->CreateMaskBrush(&masked);
    if (SUCCEEDED(result)) result = masked->put_Source(source);
    if (SUCCEEDED(result)) result = masked->put_Mask(mask_as_brush);
    if (SUCCEEDED(result)) {
        result = masked->QueryInterface(iid_composition_brush, reinterpret_cast<void**>(&masked_as_brush));
    }
    release(masked);
    release(mask_as_brush);

    ISpriteVisual* sprite{};
    IVisual* visual{};
    if (SUCCEEDED(result)) result = state.compositor->CreateSpriteVisual(&sprite);
    if (SUCCEEDED(result)) result = sprite->put_Brush(masked_as_brush);
    if (SUCCEEDED(result)) result = sprite->QueryInterface(iid_visual, reinterpret_cast<void**>(&visual));
    release(sprite);
    release(masked_as_brush);
    if (FAILED(result)) {
        release(visual);
        return nullptr;
    }

    visual->put_Size(Vector2{static_cast<float>(width), static_cast<float>(height)});
    visual->put_Offset(Vector3{static_cast<float>(origin.x), static_cast<float>(origin.y), 0.0F});
    return visual;
}

// Card layers first, the widget's own frame on top. Nothing else lives in the tree, so the gaps
// between cards stay genuinely empty.
void rebuild_tree() {
    if (!state.children) return;
    state.children->RemoveAll();
    for (IVisual* visual : state.card_visuals) release(visual);
    state.card_visuals.clear();

    for (const POINT origin : state.geometry.origins) {
        for (ICompositionBrush* source : {state.card_blur_brush, state.card_tint_brush}) {
            IVisual* layer =
                create_card_layer(source, origin, state.geometry.width, state.geometry.height);
            if (!layer) continue;
            state.children->InsertAtTop(layer);
            state.card_visuals.push_back(layer);
        }
    }
    if (state.content_visual) state.children->InsertAtTop(state.content_visual);
}

bool ensure_content_surface(const int width, const int height) {
    if (state.content_surface && state.content_width == width && state.content_height == height) {
        return true;
    }
    release(state.staging);
    release(state.content_surface);
    state.content_surface = create_surface(width, height);
    if (!state.content_surface) return false;
    state.content_width = width;
    state.content_height = height;

    const D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_NONE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0F, 96.0F);
    if (FAILED(state.d2d_context->CreateBitmap(
            D2D1::SizeU(static_cast<UINT32>(width), static_cast<UINT32>(height)), nullptr, 0,
            properties, &state.staging))) {
        release(state.content_surface);
        return false;
    }

    if (!state.content_visual) {
        ISpriteVisual* sprite{};
        if (FAILED(state.compositor->CreateSpriteVisual(&sprite))) return false;
        HRESULT result =
            sprite->QueryInterface(iid_visual, reinterpret_cast<void**>(&state.content_visual));
        release(sprite);
        if (FAILED(result)) return false;
    }

    ICompositionSurface* surface{};
    ICompositionSurfaceBrush* brush{};
    ICompositionBrush* as_brush{};
    ISpriteVisual* sprite{};
    HRESULT result = state.content_surface->QueryInterface(
        iid_composition_surface, reinterpret_cast<void**>(&surface));
    if (SUCCEEDED(result)) result = state.compositor->CreateSurfaceBrushWithSurface(surface, &brush);
    if (SUCCEEDED(result)) {
        result = brush->QueryInterface(iid_composition_brush, reinterpret_cast<void**>(&as_brush));
    }
    if (SUCCEEDED(result)) {
        result = state.content_visual->QueryInterface(
            iid_sprite_visual, reinterpret_cast<void**>(&sprite));
    }
    if (SUCCEEDED(result)) result = sprite->put_Brush(as_brush);
    release(sprite);
    release(as_brush);
    release(brush);
    release(surface);
    if (FAILED(result)) return false;

    state.content_visual->put_Size(Vector2{static_cast<float>(width), static_cast<float>(height)});
    return true;
}

}  // namespace

namespace blur {

bool initialize(const Tokens& tokens) {
    if (state.ready) return true;
    state.tokens = tokens;
    if (!load_runtime()) {
        log_failure(L"no Windows Runtime entry points", E_NOINTERFACE);
        shutdown();
        return false;
    }
    if (!create_render_device() || !create_compositor() || !create_card_brushes()) {
        shutdown();
        return false;
    }
    state.ready = true;
    return true;
}

bool attach(const HWND window) {
    if (!state.ready || state.target) return state.ready && state.target != nullptr;

    HRESULT result = state.desktop_interop->CreateDesktopWindowTarget(window, TRUE, &state.window_target);
    if (SUCCEEDED(result)) {
        result = state.window_target->QueryInterface(
            iid_composition_target, reinterpret_cast<void**>(&state.target));
    }

    IContainerVisual* container{};
    if (SUCCEEDED(result)) result = state.compositor->CreateContainerVisual(&container);
    if (SUCCEEDED(result)) {
        result = container->QueryInterface(iid_visual, reinterpret_cast<void**>(&state.root));
    }
    if (SUCCEEDED(result)) {
        void* children{};
        result = container->get_Children(&children);
        state.children = static_cast<IVisualCollection*>(children);
    }
    if (SUCCEEDED(result)) {
        RECT client{};
        GetClientRect(window, &client);
        state.root->put_Size(Vector2{
            static_cast<float>(client.right - client.left),
            static_cast<float>(client.bottom - client.top)});
        result = state.target->put_Root(state.root);
    }

    release(container);

    if (FAILED(result)) {
        log_failure(L"no desktop window target", result);
        shutdown();
        return false;
    }
    return true;
}

bool active() {
    return state.ready && state.target != nullptr;
}

void set_cards(
    const POINT* origins, const std::size_t count, const int card_width, const int card_height,
    const float corner_radius) {
    if (!active()) return;
    CardGeometry next;
    next.origins.assign(origins, origins + count);
    next.width = card_width;
    next.height = card_height;
    next.radius = corner_radius;
    if (next == state.geometry && state.mask_surface) return;

    const bool mask_changed = !state.mask_surface || state.geometry.width != next.width ||
                              state.geometry.height != next.height || state.geometry.radius != next.radius;
    state.geometry = std::move(next);
    if (mask_changed && !draw_card_mask(card_width, card_height, corner_radius)) return;
    rebuild_tree();
}

bool publish(const void* premultiplied_bgra, const int width, const int height) {
    if (!active() || !premultiplied_bgra || width <= 0 || height <= 0) return false;
    const bool resized = state.content_width != width || state.content_height != height;
    if (!ensure_content_surface(width, height)) return false;
    if (resized) rebuild_tree();

    const D2D1_RECT_U source = D2D1::RectU(0, 0, static_cast<UINT32>(width), static_cast<UINT32>(height));
    if (FAILED(state.staging->CopyFromMemory(
            &source, premultiplied_bgra, static_cast<UINT32>(width) * 4))) {
        return false;
    }

    POINT offset{};
    ID2D1DeviceContext* context = begin_surface_draw(state.content_surface, offset);
    if (!context) return false;
    context->SetTransform(
        D2D1::Matrix3x2F::Translation(static_cast<float>(offset.x), static_cast<float>(offset.y)));
    context->Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
    context->DrawBitmap(
        state.staging,
        D2D1::RectF(0.0F, 0.0F, static_cast<float>(width), static_cast<float>(height)), 1.0F,
        D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    end_surface_draw(state.content_surface, context);
    return true;
}

void shutdown() {
    for (IVisual* visual : state.card_visuals) release(visual);
    state.card_visuals.clear();
    if (state.children) state.children->RemoveAll();
    release(state.children);
    release(state.content_visual);
    release(state.staging);
    release(state.content_surface);
    release(state.mask_surface);
    release(state.card_blur_brush);
    release(state.card_tint_brush);
    release(state.root);
    release(state.target);
    release(state.window_target);
    release(state.graphics_device);
    release(state.desktop_interop);
    release(state.compositor_interop);
    release(state.compositor2);
    release(state.compositor);
    release(state.d2d_context);
    release(state.d2d_device);
    release(state.d2d_factory);
    release(state.d3d_device);
    state.geometry = CardGeometry{};
    state.content_width = 0;
    state.content_height = 0;
    state.ready = false;
}

}  // namespace blur
