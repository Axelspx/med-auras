#pragma once

#include <windows.h>
#include <inspectable.h>

// Minimal hand-declared subset of the public Windows.UI.Composition ABI.
//
// The Windows SDK and CLion's bundled MinGW ship very different generations of these headers:
// MinGW's windows.ui.composition.h predates ICompositor2 (CreateBackdropBrush), mask brushes, and
// the desktop interop entirely, and it has no windows.graphics.effects.interop.h at all. Rather
// than branch on the toolchain, the interfaces this widget actually calls are declared once here
// and used identically by both.
//
// Everything below is a documented public interface. Vtable order and IIDs are transcribed from
// the Windows SDK headers; parameters for methods the widget never calls are declared only
// accurately enough to keep the slot layout correct. No undocumented DWM structure or offset is
// involved.

namespace composition_abi {

struct Vector2 {
    float x;
    float y;
};

struct Vector3 {
    float x;
    float y;
    float z;
};

struct Quaternion {
    float x;
    float y;
    float z;
    float w;
};

struct Matrix4x4 {
    float values[16];
};

struct WinSize {
    float width;
    float height;
};

// Windows.UI.Color is stored A, R, G, B.
struct WinColor {
    BYTE a;
    BYTE r;
    BYTE g;
    BYTE b;
};

constexpr int directx_pixel_format_b8g8r8a8 = 87;
constexpr int directx_alpha_mode_premultiplied = 1;

// Windows.Graphics.Effects

struct IGraphicsEffectSource : IInspectable {};

struct IGraphicsEffect : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Name(HSTRING* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Name(HSTRING value) = 0;
};

enum GraphicsEffectPropertyMapping {
    graphics_effect_property_mapping_unknown = 0,
    graphics_effect_property_mapping_direct = 1,
};

struct IGraphicsEffectD2D1Interop : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetEffectId(GUID* id) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetNamedPropertyMapping(
        LPCWSTR name, UINT* index, GraphicsEffectPropertyMapping* mapping) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyCount(UINT* count) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProperty(UINT index, IInspectable** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSource(UINT index, IGraphicsEffectSource** source) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSourceCount(UINT* count) = 0;
};

// Windows.Foundation

struct IPropertyValueStatics : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE CreateEmpty(IInspectable**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateUInt8(BYTE, IInspectable**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateInt16(INT16, IInspectable**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateUInt16(UINT16, IInspectable**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateInt32(INT32, IInspectable**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateUInt32(UINT32 value, IInspectable** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateInt64(INT64, IInspectable**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateUInt64(UINT64, IInspectable**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateSingle(float value, IInspectable** result) = 0;
};

// Windows.UI.Composition

struct ICompositionBrush : IInspectable {};
struct ICompositionSurface : IInspectable {};
struct ICompositionClip : IInspectable {};
struct ICompositionBackdropBrush : IInspectable {};
struct ICompositionEffectSourceParameter : IInspectable {};

struct ICompositionEffectSourceParameterFactory : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE Create(
        HSTRING name, ICompositionEffectSourceParameter** value) = 0;
};

struct ICompositionColorBrush : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Color(WinColor* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Color(WinColor value) = 0;
};

struct ICompositionEffectBrush : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE GetSourceParameter(HSTRING name, ICompositionBrush** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetSourceParameter(HSTRING name, ICompositionBrush* source) = 0;
};

struct ICompositionEffectFactory : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE CreateBrush(ICompositionEffectBrush** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ExtendedError(HRESULT* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_LoadStatus(int* value) = 0;
};

struct ICompositionMaskBrush : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Mask(ICompositionBrush** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Mask(ICompositionBrush* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Source(ICompositionBrush** value) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Source(ICompositionBrush* value) = 0;
};

struct ICompositionSurfaceBrush : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_BitmapInterpolationMode(int*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_BitmapInterpolationMode(int) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_HorizontalAlignmentRatio(float*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_HorizontalAlignmentRatio(float) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Stretch(int*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Stretch(int value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Surface(ICompositionSurface**) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Surface(ICompositionSurface* value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_VerticalAlignmentRatio(float*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_VerticalAlignmentRatio(float) = 0;
};

struct ICompositionDrawingSurface : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_AlphaMode(int*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_PixelFormat(int*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Size(WinSize*) = 0;
};

struct ICompositionGraphicsDevice : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE CreateDrawingSurface(
        WinSize size_pixels, int pixel_format, int alpha_mode,
        ICompositionDrawingSurface** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_RenderingDeviceReplaced(void*, UINT64*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_RenderingDeviceReplaced(UINT64) = 0;
};

struct IVisual;

struct IContainerVisual : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Children(void** value) = 0;
};

struct IVisual : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_AnchorPoint(Vector2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AnchorPoint(Vector2) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_BackfaceVisibility(int*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_BackfaceVisibility(int) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_BorderMode(int*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_BorderMode(int) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CenterPoint(Vector3*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_CenterPoint(Vector3) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Clip(ICompositionClip**) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Clip(ICompositionClip*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CompositeMode(int*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_CompositeMode(int) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsVisible(unsigned char*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsVisible(unsigned char) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Offset(Vector3*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Offset(Vector3 value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Opacity(float*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Opacity(float value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Orientation(Quaternion*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Orientation(Quaternion) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Parent(IContainerVisual**) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_RotationAngle(float*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_RotationAngle(float) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_RotationAngleInDegrees(float*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_RotationAngleInDegrees(float) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_RotationAxis(Vector3*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_RotationAxis(Vector3) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Scale(Vector3*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Scale(Vector3) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Size(Vector2*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Size(Vector2 value) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_TransformMatrix(Matrix4x4*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_TransformMatrix(Matrix4x4) = 0;
};

struct IVisualCollection : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Count(INT32*) = 0;
    virtual HRESULT STDMETHODCALLTYPE InsertAbove(IVisual*, IVisual*) = 0;
    virtual HRESULT STDMETHODCALLTYPE InsertAtBottom(IVisual*) = 0;
    virtual HRESULT STDMETHODCALLTYPE InsertAtTop(IVisual* new_child) = 0;
    virtual HRESULT STDMETHODCALLTYPE InsertBelow(IVisual*, IVisual*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Remove(IVisual*) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveAll() = 0;
};

struct ISpriteVisual : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Brush(ICompositionBrush**) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Brush(ICompositionBrush* value) = 0;
};

struct ICompositionTarget : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Root(IVisual**) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Root(IVisual* value) = 0;
};

struct ICompositor : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE CreateColorKeyFrameAnimation(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateColorBrush(ICompositionColorBrush**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateColorBrushWithColor(
        WinColor color, ICompositionColorBrush** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateContainerVisual(IContainerVisual** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateCubicBezierEasingFunction(Vector2, Vector2, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateEffectFactory(
        IGraphicsEffect* effect, ICompositionEffectFactory** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateEffectFactoryWithProperties(
        IGraphicsEffect*, void*, ICompositionEffectFactory**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateExpressionAnimation(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateExpressionAnimationWithExpression(HSTRING, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateInsetClip(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateInsetClipWithInsets(float, float, float, float, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateLinearEasingFunction(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreatePropertySet(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateQuaternionKeyFrameAnimation(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateScalarKeyFrameAnimation(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateScopedBatch(int, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateSpriteVisual(ISpriteVisual** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateSurfaceBrush(ICompositionSurfaceBrush**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateSurfaceBrushWithSurface(
        ICompositionSurface* surface, ICompositionSurfaceBrush** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateTargetForCurrentView(ICompositionTarget**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateVector2KeyFrameAnimation(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateVector3KeyFrameAnimation(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateVector4KeyFrameAnimation(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCommitBatch(int, void**) = 0;
};

struct ICompositor2 : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE CreateAmbientLight(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateAnimationGroup(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateBackdropBrush(ICompositionBackdropBrush** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateDistantLight(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateDropShadow(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateImplicitAnimationCollection(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateLayerVisual(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateMaskBrush(ICompositionMaskBrush** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateNineGridBrush(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreatePointLight(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateSpotLight(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateStepEasingFunction(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateStepEasingFunctionWithStepCount(INT32, void**) = 0;
};

// Composition interop (IUnknown-derived, not IInspectable)

struct ICompositorInterop : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateCompositionSurfaceForHandle(HANDLE, ICompositionSurface**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateCompositionSurfaceForSwapChain(IUnknown*, ICompositionSurface**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateGraphicsDevice(
        IUnknown* rendering_device, ICompositionGraphicsDevice** result) = 0;
};

struct ICompositorDesktopInterop : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateDesktopWindowTarget(
        HWND target, BOOL is_topmost, IUnknown** result) = 0;
    virtual HRESULT STDMETHODCALLTYPE EnsureOnThread(DWORD thread_id) = 0;
};

struct ICompositionDrawingSurfaceInterop : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE BeginDraw(
        const RECT* update_rect, REFIID iid, void** update_object, POINT* update_offset) = 0;
    virtual HRESULT STDMETHODCALLTYPE EndDraw() = 0;
    virtual HRESULT STDMETHODCALLTYPE Resize(SIZE size_pixels) = 0;
    virtual HRESULT STDMETHODCALLTYPE Scroll(const RECT*, const RECT*, int, int) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResumeDraw() = 0;
    virtual HRESULT STDMETHODCALLTYPE SuspendDraw() = 0;
};

// IIDs, transcribed from the Windows SDK headers.
constexpr GUID iid_graphics_effect{
    0xcb51c0ce, 0x8fe6, 0x4636, {0xb2, 0x02, 0x86, 0x1f, 0xaa, 0x07, 0xd8, 0xf3}};
constexpr GUID iid_graphics_effect_source{
    0x2d8f9ddc, 0x4339, 0x4eb9, {0x92, 0x16, 0xf9, 0xde, 0xb7, 0x56, 0x58, 0xa2}};
constexpr GUID iid_graphics_effect_d2d1_interop{
    0x2fc57384, 0xa068, 0x44d7, {0xa3, 0x31, 0x30, 0x98, 0x2f, 0xcf, 0x71, 0x77}};
constexpr GUID iid_property_value_statics{
    0x629bdbc8, 0xd932, 0x4ff4, {0x96, 0xb9, 0x8d, 0x96, 0xc5, 0xc1, 0xe8, 0x58}};
constexpr GUID iid_compositor{
    0xb403ca50, 0x7f8c, 0x4e83, {0x98, 0x5f, 0xcc, 0x45, 0x06, 0x00, 0x36, 0xd8}};
constexpr GUID iid_compositor2{
    0x735081dc, 0x5e24, 0x45da, {0xa3, 0x8f, 0xe3, 0x2c, 0xc3, 0x49, 0xa9, 0xa0}};
constexpr GUID iid_compositor_interop{
    0x25297d5c, 0x3ad4, 0x4c9c, {0xb5, 0xcf, 0xe3, 0x6a, 0x38, 0x51, 0x23, 0x30}};
constexpr GUID iid_compositor_desktop_interop{
    0x29e691fa, 0x4567, 0x4dca, {0xb3, 0x19, 0xd0, 0xf2, 0x07, 0xeb, 0x68, 0x07}};
constexpr GUID iid_composition_target{
    0xa1bea8ba, 0xd726, 0x4663, {0x81, 0x29, 0x6b, 0x5e, 0x79, 0x27, 0xff, 0xa6}};
constexpr GUID iid_visual{
    0x117e202d, 0xa859, 0x4c89, {0x87, 0x3b, 0xc2, 0xaa, 0x56, 0x67, 0x88, 0xe3}};
constexpr GUID iid_container_visual{
    0x02f6bc74, 0xed20, 0x4773, {0xaf, 0xe6, 0xd4, 0x9b, 0x4a, 0x93, 0xdb, 0x32}};
constexpr GUID iid_sprite_visual{
    0x08e05581, 0x1ad1, 0x4f97, {0x97, 0x57, 0x40, 0x2d, 0x76, 0xe4, 0x23, 0x3b}};
constexpr GUID iid_visual_collection{
    0x8b745505, 0xfd3e, 0x4a98, {0x84, 0xa8, 0xe9, 0x49, 0x46, 0x8c, 0x6b, 0xcb}};
constexpr GUID iid_composition_brush{
    0xab0d7608, 0x30c0, 0x40e9, {0xb5, 0x68, 0xb6, 0x0a, 0x6b, 0xd1, 0xfb, 0x46}};
constexpr GUID iid_composition_surface{
    0x1527540d, 0x42c7, 0x47a6, {0xa4, 0x08, 0x66, 0x8f, 0x79, 0xa9, 0x0d, 0xfb}};
constexpr GUID iid_composition_effect_source_parameter_factory{
    0xb3d9f276, 0xaba3, 0x4724, {0xac, 0xf3, 0xd0, 0x39, 0x74, 0x64, 0xdb, 0x1c}};
constexpr GUID iid_composition_drawing_surface_interop{
    0xfd04e6e3, 0xfe0c, 0x4c3c, {0xab, 0x19, 0xa0, 0x76, 0x01, 0xa5, 0x76, 0xee}};

}  // namespace composition_abi
