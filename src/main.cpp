#include "resource.h"
#include "storage.h"
#include "startup.h"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wincodec.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr wchar_t window_class[] = L"MedAurasWindow";
constexpr UINT_PTR refresh_timer = 1;
constexpr UINT_PTR renderer_release_timer = 2;
constexpr int first_taken_button_id = 100;
constexpr int first_edit_button_id = 200;
struct DesignTokens {
    float widget_width{400.0F};
    float empty_height{60.0F};
    float row_height{80.0F};
    float row_gap{4.0F};
    float card_radius{14.0F};
    float stroke{1.0F};
    float icon_size{48.0F};
    float icon_radius{9.0F};
    float content_left{72.0F};
    float content_right{310.0F};
    float action_panel_left{312.0F};
    float action_panel_right{394.0F};
    float action_panel_top{12.0F};
    float action_panel_bottom{68.0F};
    float action_panel_radius{9.0F};
    float action_button_padding{5.0F};
    float action_button_gap{8.0F};
    float taken_button_size{36.0F};
    float edit_button_size{28.0F};
    float button_inset{1.0F};
    float focus_inset{4.0F};
};
constexpr DesignTokens design;
constexpr UINT toggle_paused_command = 1;
constexpr UINT remove_medication_command = 2;
constexpr UINT edit_medication_command = 3;
constexpr UINT add_medication_command = 4;
constexpr UINT lock_position_command = 5;
constexpr UINT always_on_top_command = 6;
constexpr UINT tray_show_hide_command = 10;
constexpr UINT tray_startup_command = 11;
constexpr UINT tray_exit_command = 12;
constexpr UINT tray_callback_message = WM_APP + 1;
constexpr UINT tray_icon_id = 1;

std::vector<Medication> medications;
std::filesystem::path medications_path;
WidgetSettings widget_settings;
IWICImagingFactory* imaging_factory{};
ID2D1Factory* d2d_factory{};
ID2D1DCRenderTarget* d2d_render_target{};
D2D1_ALPHA_MODE d2d_alpha_mode{D2D1_ALPHA_MODE_UNKNOWN};
std::vector<HBITMAP> medication_icons;
HFONT name_font{};
HFONT dose_font{};
HFONT status_font{};
HFONT countdown_font{};
HFONT initial_font{};
HFONT glyph_font{};
UINT widget_dpi = 96;
bool enable_startup_after_save{};
bool tray_icon_added{};
UINT taskbar_created_message{};
HWND tooltip_window{};
std::optional<std::size_t> hovered_bar;
bool tracking_mouse_leave{};

float dpi_scale() {
    return static_cast<float>(widget_dpi) / 96.0F;
}

int pixels(const float dips) {
    return static_cast<int>(std::lround(dips * dpi_scale()));
}

float dips(const int pixels_value) {
    return static_cast<float>(pixels_value) / dpi_scale();
}

int scaled(const int value) {
    return pixels(static_cast<float>(value));
}

int widget_width() {
    return pixels(design.widget_width);
}

void enable_per_monitor_dpi() {
    using SetDpiAwareness = BOOL(WINAPI*)(HANDLE);
    const auto set_awareness = reinterpret_cast<SetDpiAwareness>(
        GetProcAddress(GetModuleHandle(L"user32.dll"), "SetProcessDpiAwarenessContext"));
    if (set_awareness) set_awareness(reinterpret_cast<HANDLE>(-4));
}

UINT system_dpi() {
    using GetDpi = UINT(WINAPI*)();
    const auto get_dpi =
        reinterpret_cast<GetDpi>(GetProcAddress(GetModuleHandle(L"user32.dll"), "GetDpiForSystem"));
    if (get_dpi) return get_dpi();
    const HDC device = GetDC(nullptr);
    const UINT dpi = device ? static_cast<UINT>(GetDeviceCaps(device, LOGPIXELSX)) : 96;
    if (device) ReleaseDC(nullptr, device);
    return dpi;
}

UINT window_dpi(const HWND window) {
    using GetDpi = UINT(WINAPI*)(HWND);
    const auto get_dpi =
        reinterpret_cast<GetDpi>(GetProcAddress(GetModuleHandle(L"user32.dll"), "GetDpiForWindow"));
    return get_dpi ? get_dpi(window) : system_dpi();
}

std::filesystem::path local_medications_path() {
    PWSTR local_app_data{};
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local_app_data))) {
        throw std::runtime_error("Could not locate the local application data folder");
    }
    const std::filesystem::path path = std::filesystem::path{local_app_data} / L"MedAuras" / L"medications.json";
    CoTaskMemFree(local_app_data);
    return path;
}

Medication example_medication() {
    return Medication{
        .id = L"example-medication",
        .name = L"Example medication",
        .dose = L"40 mg",
        .interval = std::chrono::hours{12},
    };
}

int widget_height() {
    if (medications.empty()) return pixels(design.empty_height);
    const float rows = static_cast<float>(medications.size());
    return pixels(rows * design.row_height + (rows - 1.0F) * design.row_gap);
}

void enable_startup_if_needed(const HWND window) {
    if (!enable_startup_after_save || medications.empty()) return;
    if (enable_startup()) {
        enable_startup_after_save = false;
    } else {
        MessageBox(
            window, L"The medication state was saved, but Windows startup could not be enabled.",
            L"Medication Cooldown Widget", MB_OK | MB_ICONWARNING);
    }
}

std::chrono::system_clock::time_point system_time_to_time_point(const SYSTEMTIME& value) {
    const std::chrono::year_month_day date{
        std::chrono::year{value.wYear}, std::chrono::month{value.wMonth}, std::chrono::day{value.wDay}};
    return std::chrono::sys_days{date} + std::chrono::hours{value.wHour} + std::chrono::minutes{value.wMinute} +
           std::chrono::seconds{value.wSecond};
}

SYSTEMTIME time_point_to_system_time(const std::chrono::system_clock::time_point value) {
    const auto seconds = std::chrono::floor<std::chrono::seconds>(value);
    const auto day = std::chrono::floor<std::chrono::days>(seconds);
    const std::chrono::year_month_day date{day};
    const std::chrono::hh_mm_ss time{seconds - day};
    return SYSTEMTIME{
        .wYear = static_cast<WORD>(static_cast<int>(date.year())),
        .wMonth = static_cast<WORD>(static_cast<unsigned>(date.month())),
        .wDayOfWeek = static_cast<WORD>(std::chrono::weekday{day}.c_encoding()),
        .wDay = static_cast<WORD>(static_cast<unsigned>(date.day())),
        .wHour = static_cast<WORD>(time.hours().count()),
        .wMinute = static_cast<WORD>(time.minutes().count()),
        .wSecond = static_cast<WORD>(time.seconds().count()),
    };
}

SYSTEMTIME local_time_for(const std::chrono::system_clock::time_point value) {
    const SYSTEMTIME utc = time_point_to_system_time(value);
    SYSTEMTIME local{};
    return SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local) ? local : utc;
}

std::optional<std::chrono::system_clock::time_point> utc_time_for(const SYSTEMTIME& local) {
    SYSTEMTIME utc{};
    if (!TzSpecificLocalTimeToSystemTime(nullptr, &local, &utc)) return std::nullopt;
    return system_time_to_time_point(utc);
}

struct DisplayInterval {
    double value;
    IntervalUnit unit;
};

DisplayInterval display_interval(const std::chrono::minutes interval) {
    const auto minutes = interval.count();
    if (minutes >= 10'080 && minutes % 5'040 == 0) {
        return {static_cast<double>(minutes) / 10'080.0, IntervalUnit::weeks};
    }
    if (minutes >= 1'440 && minutes % 720 == 0) {
        return {static_cast<double>(minutes) / 1'440.0, IntervalUnit::days};
    }
    if (minutes >= 60 && minutes % 30 == 0) {
        return {static_cast<double>(minutes) / 60.0, IntervalUnit::hours};
    }
    return {static_cast<double>(minutes), IntervalUnit::minutes};
}

std::wstring interval_text(const double value) {
    std::wostringstream output;
    output << std::setprecision(10) << value;
    return output.str();
}

INT_PTR CALLBACK medication_editor_procedure(
    const HWND dialog, const UINT message, const WPARAM w_param, const LPARAM l_param) {
    if (message == WM_INITDIALOG) {
        auto* medication = reinterpret_cast<Medication*>(l_param);
        SetWindowLongPtr(dialog, GWLP_USERDATA, l_param);
        SetWindowText(dialog, medication->id.empty() ? L"Add medication" : L"Edit medication");
        SetDlgItemText(dialog, IDC_MEDICATION_NAME, medication->name.c_str());
        SetDlgItemText(dialog, IDC_MEDICATION_DOSE, medication->dose.c_str());
        SetDlgItemText(dialog, IDC_MEDICATION_ICON, medication->icon_path ? medication->icon_path->c_str() : L"");
        const DisplayInterval displayed = display_interval(medication->interval);
        SetDlgItemText(dialog, IDC_MEDICATION_INTERVAL, interval_text(displayed.value).c_str());
        const HWND unit = GetDlgItem(dialog, IDC_MEDICATION_UNIT);
        for (const wchar_t* label : {L"minutes", L"hours", L"days", L"weeks"}) {
            SendMessage(unit, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
        }
        SendMessage(unit, CB_SETCURSEL, static_cast<WPARAM>(displayed.unit), 0);

        const SYSTEMTIME start = local_time_for(
            medication->last_taken_at.value_or(std::chrono::system_clock::now()));
        DateTime_SetSystemtime(GetDlgItem(dialog, IDC_MEDICATION_DATE), GDT_VALID, &start);
        DateTime_SetSystemtime(GetDlgItem(dialog, IDC_MEDICATION_TIME), GDT_VALID, &start);
        SetFocus(GetDlgItem(dialog, IDC_MEDICATION_NAME));
        return FALSE;
    }
    if (message != WM_COMMAND) return FALSE;
    if (LOWORD(w_param) == IDCANCEL) {
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    if (LOWORD(w_param) == IDC_BROWSE_ICON) {
        std::array<wchar_t, 32'768> path{};
        GetDlgItemText(dialog, IDC_MEDICATION_ICON, path.data(), static_cast<int>(path.size()));
        OPENFILENAME file{sizeof(OPENFILENAME)};
        file.hwndOwner = dialog;
        file.lpstrFilter = L"Images (*.png;*.jpg;*.jpeg;*.bmp;*.ico)\0*.png;*.jpg;*.jpeg;*.bmp;*.ico\0All files\0*.*\0";
        file.lpstrFile = path.data();
        file.nMaxFile = static_cast<DWORD>(path.size());
        file.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileName(&file)) SetDlgItemText(dialog, IDC_MEDICATION_ICON, path.data());
        return TRUE;
    }
    if (LOWORD(w_param) == IDC_CLEAR_ICON) {
        SetDlgItemText(dialog, IDC_MEDICATION_ICON, L"");
        return TRUE;
    }
    if (LOWORD(w_param) != IDOK) return FALSE;

    auto* medication = reinterpret_cast<Medication*>(GetWindowLongPtr(dialog, GWLP_USERDATA));
    std::array<wchar_t, 256> name{};
    std::array<wchar_t, 128> dose{};
    std::array<wchar_t, 32> interval_text{};
    std::array<wchar_t, 32'768> icon_path{};
    GetDlgItemText(dialog, IDC_MEDICATION_NAME, name.data(), static_cast<int>(name.size()));
    GetDlgItemText(dialog, IDC_MEDICATION_DOSE, dose.data(), static_cast<int>(dose.size()));
    GetDlgItemText(
        dialog, IDC_MEDICATION_INTERVAL, interval_text.data(), static_cast<int>(interval_text.size()));
    GetDlgItemText(dialog, IDC_MEDICATION_ICON, icon_path.data(), static_cast<int>(icon_path.size()));

    wchar_t* interval_end{};
    const double interval_value = std::wcstod(interval_text.data(), &interval_end);
    const LRESULT selected_unit = SendDlgItemMessage(dialog, IDC_MEDICATION_UNIT, CB_GETCURSEL, 0, 0);
    const auto interval = selected_unit >= 0 && selected_unit <= static_cast<LRESULT>(IntervalUnit::weeks)
                              ? interval_in_minutes(interval_value, static_cast<IntervalUnit>(selected_unit))
                              : std::nullopt;
    SYSTEMTIME date{};
    SYSTEMTIME time{};
    DateTime_GetSystemtime(GetDlgItem(dialog, IDC_MEDICATION_DATE), &date);
    DateTime_GetSystemtime(GetDlgItem(dialog, IDC_MEDICATION_TIME), &time);
    date.wHour = time.wHour;
    date.wMinute = time.wMinute;
    date.wSecond = 0;
    date.wMilliseconds = 0;
    const auto start = utc_time_for(date);
    if (name[0] == L'\0' || !interval || !start || interval_end == interval_text.data() || *interval_end != L'\0') {
        MessageBox(
            dialog, L"Enter a medication name, positive interval, unit, and valid start date/time.", L"Medication",
            MB_OK | MB_ICONWARNING);
        return TRUE;
    }

    medication->name = name.data();
    medication->dose = dose.data();
    medication->icon_path = icon_path[0] == L'\0' ? std::nullopt : std::optional<std::wstring>{icon_path.data()};
    medication->interval = *interval;
    medication->last_taken_at = *start;
    EndDialog(dialog, IDOK);
    return TRUE;
}

bool edit_medication(const HWND owner, Medication& medication) {
    return DialogBoxParam(
               GetModuleHandle(nullptr), MAKEINTRESOURCE(IDD_MEDICATION_EDITOR), owner, medication_editor_procedure,
               reinterpret_cast<LPARAM>(&medication)) == IDOK;
}

std::wstring new_medication_id() {
    GUID id{};
    std::array<wchar_t, 40> text{};
    if (SUCCEEDED(CoCreateGuid(&id)) && StringFromGUID2(id, text.data(), static_cast<int>(text.size())) > 0) {
        return text.data();
    }
    return L"medication-" +
           std::to_wstring(std::chrono::system_clock::now().time_since_epoch().count());
}

std::wstring status_text(const Medication& medication, const std::chrono::system_clock::time_point now) {
    if (!medication.enabled) return L"PAUSED";
    const std::chrono::minutes remaining = medication.remaining_at(now);
    if (remaining == std::chrono::minutes::zero()) return L"DUE";
    return medication.is_soon_at(now) ? L"SOON" : L"";
}

std::wstring countdown_text(const Medication& medication, const std::chrono::system_clock::time_point now) {
    if (!medication.enabled) return L"PAUSED";
    const std::chrono::minutes remaining = medication.remaining_at(now);
    if (remaining == std::chrono::minutes::zero()) return L"DUE";
    const auto hours = std::chrono::duration_cast<std::chrono::hours>(remaining);
    const auto minutes = remaining - hours;
    return hours == std::chrono::hours::zero()
               ? std::to_wstring(minutes.count()) + L"m"
               : std::to_wstring(hours.count()) + L"h " + std::to_wstring(minutes.count()) + L"m";
}

std::wstring local_timestamp_text(
    const Medication& medication, const std::chrono::system_clock::time_point now) {
    const auto timestamp = medication.next_available_at();
    if (!timestamp) return countdown_text(medication, now);

    const SYSTEMTIME local = local_time_for(*timestamp);
    std::array<wchar_t, 32> date{};
    std::array<wchar_t, 32> time{};
    if (!GetDateFormatEx(
            LOCALE_NAME_USER_DEFAULT, 0, &local, L"dd MMM", date.data(), static_cast<int>(date.size()), nullptr) ||
        !GetTimeFormatEx(
            LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &local, nullptr, time.data(),
            static_cast<int>(time.size()))) {
        return {};
    }
    const wchar_t* prefix = !medication.enabled                                      ? L"Was due "
                            : medication.remaining_at(now) == std::chrono::minutes::zero() ? L"Since "
                                                                                      : L"";
    return std::wstring{prefix} + date.data() + L" " + time.data();
}

void schedule_refresh(const HWND window) {
    KillTimer(window, refresh_timer);
    if (!IsWindowVisible(window)) return;
    const auto now = std::chrono::system_clock::now();
    if (std::ranges::any_of(medications, [now](const Medication& medication) {
            return medication.enabled && medication.remaining_at(now) > std::chrono::minutes::zero();
        })) {
        SetTimer(window, refresh_timer, 60'000, nullptr);
    }
}

void save_state() {
    save_medications(medications_path, medications, widget_settings);
}

void clear_medication_icons() {
    for (const HBITMAP icon : medication_icons) {
        if (icon) DeleteObject(icon);
    }
    medication_icons.clear();
}

void rebuild_fonts() {
    if (name_font) DeleteObject(name_font);
    if (dose_font) DeleteObject(dose_font);
    if (status_font) DeleteObject(status_font);
    if (countdown_font) DeleteObject(countdown_font);
    if (initial_font) DeleteObject(initial_font);
    if (glyph_font) DeleteObject(glyph_font);
    const auto make_font = [](const int dip, const int weight, const wchar_t* face) {
        return CreateFont(
            -scaled(dip), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
    };
    name_font = make_font(14, FW_SEMIBOLD, L"Segoe UI");
    dose_font = make_font(11, FW_NORMAL, L"Segoe UI");
    status_font = make_font(10, FW_SEMIBOLD, L"Segoe UI");
    countdown_font = make_font(12, FW_SEMIBOLD, L"Segoe UI");
    initial_font = make_font(24, FW_SEMIBOLD, L"Segoe UI");
    glyph_font = make_font(18, FW_NORMAL, L"Segoe Fluent Icons");
}

HBITMAP load_medication_icon(const std::wstring& path) {
    if (!imaging_factory) return nullptr;

    IWICBitmapDecoder* decoder{};
    IWICBitmapFrameDecode* frame{};
    IWICBitmapScaler* scaler{};
    IWICFormatConverter* converter{};
    HBITMAP bitmap{};
    void* bitmap_pixels{};
    UINT width{};
    UINT height{};
    UINT scaled_width{};
    UINT scaled_height{};
    const UINT icon_size = static_cast<UINT>(pixels(design.icon_size));

    HRESULT result = imaging_factory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(result)) result = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(result)) result = frame->GetSize(&width, &height);
    if (SUCCEEDED(result) && width > 0 && height > 0) {
        const double scale = std::min(static_cast<double>(icon_size) / width, static_cast<double>(icon_size) / height);
        scaled_width = std::max(1U, static_cast<UINT>(std::lround(width * scale)));
        scaled_height = std::max(1U, static_cast<UINT>(std::lround(height * scale)));
        result = imaging_factory->CreateBitmapScaler(&scaler);
    } else if (SUCCEEDED(result)) {
        result = E_FAIL;
    }
    if (SUCCEEDED(result)) {
        result = scaler->Initialize(frame, scaled_width, scaled_height, WICBitmapInterpolationModeFant);
    }
    if (SUCCEEDED(result)) result = imaging_factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(result)) {
        result = converter->Initialize(
            scaler, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
            WICBitmapPaletteTypeCustom);
    }
    if (SUCCEEDED(result)) {
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = static_cast<LONG>(icon_size);
        info.bmiHeader.biHeight = -static_cast<LONG>(icon_size);
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        const HDC device = GetDC(nullptr);
        bitmap = CreateDIBSection(device, &info, DIB_RGB_COLORS, &bitmap_pixels, nullptr, 0);
        ReleaseDC(nullptr, device);
        if (!bitmap || !bitmap_pixels) result = E_OUTOFMEMORY;
    }
    if (SUCCEEDED(result)) {
        const UINT stride = icon_size * 4;
        std::memset(bitmap_pixels, 0, icon_size * stride);
        auto* destination = static_cast<BYTE*>(bitmap_pixels) + ((icon_size - scaled_height) / 2 * icon_size +
                                                                 (icon_size - scaled_width) / 2) * 4;
        const UINT buffer_size = stride * (scaled_height - 1) + scaled_width * 4;
        result = converter->CopyPixels(nullptr, stride, buffer_size, destination);
    }

    if (converter) converter->Release();
    if (scaler) scaler->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (FAILED(result) && bitmap) {
        DeleteObject(bitmap);
        bitmap = nullptr;
    }
    return bitmap;
}

void rebuild_medication_icons() {
    clear_medication_icons();
    medication_icons.reserve(medications.size());
    for (const Medication& medication : medications) {
        medication_icons.push_back(medication.icon_path ? load_medication_icon(*medication.icon_path) : nullptr);
    }
}

NOTIFYICONDATA tray_icon_data(const HWND window) {
    NOTIFYICONDATA data{sizeof(NOTIFYICONDATA)};
    data.hWnd = window;
    data.uID = tray_icon_id;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = tray_callback_message;
    data.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    std::copy_n(L"MedAuras", 9, data.szTip);
    return data;
}

bool add_tray_icon(const HWND window) {
    if (tray_icon_added) return true;
    NOTIFYICONDATA data = tray_icon_data(window);
    tray_icon_added = Shell_NotifyIcon(NIM_ADD, &data) != FALSE;
    return tray_icon_added;
}

void remove_tray_icon(const HWND window) {
    if (!tray_icon_added) return;
    NOTIFYICONDATA data = tray_icon_data(window);
    Shell_NotifyIcon(NIM_DELETE, &data);
    tray_icon_added = false;
}

void show_widget(const HWND window) {
    ShowWindow(window, SW_SHOWNORMAL);
    SetForegroundWindow(window);
    schedule_refresh(window);
    InvalidateRect(window, nullptr, FALSE);
}

POINT clamped_widget_position(const POINT position) {
    const HMONITOR monitor = MonitorFromPoint(position, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(MONITORINFO)};
    if (!monitor || !GetMonitorInfo(monitor, &info)) return position;
    return POINT{
        std::clamp(position.x, info.rcWork.left, std::max(info.rcWork.left, info.rcWork.right - widget_width())),
        std::clamp(position.y, info.rcWork.top, std::max(info.rcWork.top, info.rcWork.bottom - widget_height())),
    };
}

void apply_topmost(const HWND window) {
    SetWindowPos(
        window, widget_settings.always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void persist_window_position(const HWND window, const bool restore_on_failure) {
    RECT bounds{};
    if (!GetWindowRect(window, &bounds)) return;
    const auto previous_x = widget_settings.window_x;
    const auto previous_y = widget_settings.window_y;
    widget_settings.window_x = bounds.left;
    widget_settings.window_y = bounds.top;
    try {
        save_state();
    } catch (const std::exception&) {
        widget_settings.window_x = previous_x;
        widget_settings.window_y = previous_y;
        if (restore_on_failure && previous_x && previous_y) {
            SetWindowPos(
                window, nullptr, *previous_x, *previous_y, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        MessageBox(
            window, L"The widget position could not be saved.", L"Medication Cooldown Widget",
            MB_OK | MB_ICONERROR);
    }
}

void show_tray_menu(const HWND window, const POINT point) {
    const HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenu(menu, MF_STRING, tray_show_hide_command, IsWindowVisible(window) ? L"Hide" : L"Show");
    AppendMenu(
        menu, MF_STRING | (is_startup_enabled() ? MF_CHECKED : MF_UNCHECKED), tray_startup_command,
        L"Start with Windows");
    AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(menu, MF_STRING, tray_exit_command, L"Exit");

    SetForegroundWindow(window);
    const UINT command = TrackPopupMenu(
        menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, point.x, point.y, 0, window, nullptr);
    DestroyMenu(menu);
    PostMessage(window, WM_NULL, 0, 0);

    if (command == tray_show_hide_command) {
        if (IsWindowVisible(window)) {
            ShowWindow(window, SW_HIDE);
            KillTimer(window, refresh_timer);
        } else {
            show_widget(window);
        }
    } else if (command == tray_startup_command) {
        const bool enabled = is_startup_enabled();
        if (enabled ? disable_startup() : enable_startup()) {
            enable_startup_after_save = false;
        } else {
            MessageBox(
                window, enabled ? L"Windows startup could not be disabled." : L"Windows startup could not be enabled.",
                L"Medication Cooldown Widget", MB_OK | MB_ICONERROR);
        }
    } else if (command == tray_exit_command) {
        DestroyWindow(window);
    }
}

struct RoundedShape {
    D2D1_RECT_F bounds;
    float radius;
};

RoundedShape aligned_shape(const RoundedShape& shape);

struct RowLayout {
    RoundedShape card;
    RoundedShape icon_tile;
    D2D1_RECT_F icon;
    D2D1_RECT_F name_with_state;
    D2D1_RECT_F name_without_state;
    D2D1_RECT_F dose;
    RoundedShape badge;
    RoundedShape progress_bar;
    D2D1_RECT_F progress_text;
    RoundedShape action_panel;
    RoundedShape taken_button;
    RoundedShape edit_button;
};

D2D1_RECT_F rect(const float left, const float top, const float right, const float bottom) {
    return D2D1::RectF(left, top, right, bottom);
}

RoundedShape rounded_shape(
    const float left, const float top, const float right, const float bottom, const float radius) {
    return RoundedShape{rect(left, top, right, bottom), radius};
}

RoundedShape capsule(const float left, const float top, const float right, const float bottom) {
    return rounded_shape(left, top, right, bottom, (bottom - top) * 0.5F);
}

RowLayout row_layout(const std::size_t index) {
    const float top = static_cast<float>(index) * (design.row_height + design.row_gap);
    const float taken_left = design.action_panel_left + design.action_button_padding;
    const float taken_top = top + (design.row_height - design.taken_button_size) * 0.5F;
    const float edit_left = taken_left + design.taken_button_size + design.action_button_gap;
    const float edit_top = top + (design.row_height - design.edit_button_size) * 0.5F;
    return RowLayout{
        .card = rounded_shape(0.0F, top, design.widget_width, top + design.row_height, design.card_radius),
        .icon_tile = rounded_shape(8.0F, top + 8.0F, 64.0F, top + 72.0F, design.icon_radius),
        .icon = rect(12.0F, top + 16.0F, 60.0F, top + 64.0F),
        .name_with_state = rect(design.content_left, top + 7.0F, 248.0F, top + 27.0F),
        .name_without_state = rect(design.content_left, top + 7.0F, 306.0F, top + 27.0F),
        .dose = rect(design.content_left, top + 27.0F, design.content_right, top + 43.0F),
        .badge = capsule(252.0F, top + 8.0F, design.content_right, top + 28.0F),
        .progress_bar = capsule(design.content_left, top + 45.0F, design.content_right, top + 67.0F),
        .progress_text = rect(80.0F, top + 45.0F, 302.0F, top + 67.0F),
        .action_panel = rounded_shape(
            design.action_panel_left, top + design.action_panel_top,
            design.action_panel_right, top + design.action_panel_bottom,
            design.action_panel_radius),
        .taken_button = capsule(
            taken_left, taken_top, taken_left + design.taken_button_size,
            taken_top + design.taken_button_size),
        .edit_button = capsule(
            edit_left, edit_top, edit_left + design.edit_button_size,
            edit_top + design.edit_button_size),
    };
}

RoundedShape empty_card_layout() {
    return rounded_shape(0.0F, 0.0F, design.widget_width, design.empty_height, design.card_radius);
}

RECT pixel_rect(const D2D1_RECT_F bounds) {
    return RECT{pixels(bounds.left), pixels(bounds.top), pixels(bounds.right), pixels(bounds.bottom)};
}

bool contains(const RoundedShape& shape, const D2D1_POINT_2F point) {
    const RoundedShape aligned = aligned_shape(shape);
    const D2D1_RECT_F bounds = aligned.bounds;
    if (point.x < bounds.left || point.x >= bounds.right || point.y < bounds.top || point.y >= bounds.bottom) {
        return false;
    }
    const float radius = std::min(
        aligned.radius, std::min((bounds.right - bounds.left) * 0.5F, (bounds.bottom - bounds.top) * 0.5F));
    const float center_x = std::clamp(point.x, bounds.left + radius, bounds.right - radius);
    const float center_y = std::clamp(point.y, bounds.top + radius, bounds.bottom - radius);
    const float dx = point.x - center_x;
    const float dy = point.y - center_y;
    return dx * dx + dy * dy <= radius * radius;
}

std::optional<std::size_t> progress_bar_at(const POINT point) {
    const D2D1_POINT_2F dip_point = D2D1::Point2F(dips(point.x), dips(point.y));
    for (std::size_t index = 0; index < medications.size(); ++index) {
        if (contains(row_layout(index).progress_bar, dip_point)) return index;
    }
    return std::nullopt;
}

void invalidate_progress_bar(const HWND window, const std::optional<std::size_t> index) {
    if (!index || *index >= medications.size()) return;
    RECT bounds = pixel_rect(row_layout(*index).progress_bar.bounds);
    InflateRect(&bounds, 1, 1);
    InvalidateRect(window, &bounds, FALSE);
}

std::optional<std::size_t> medication_at(const HWND window, POINT point) {
    if (point.x == -1 && point.y == -1) {
        const int focused_id = GetDlgCtrlID(GetFocus());
        const int taken_index = focused_id - first_taken_button_id;
        const int edit_index = focused_id - first_edit_button_id;
        if (taken_index >= 0 && static_cast<std::size_t>(taken_index) < medications.size()) {
            return static_cast<std::size_t>(taken_index);
        }
        if (edit_index >= 0 && static_cast<std::size_t>(edit_index) < medications.size()) {
            return static_cast<std::size_t>(edit_index);
        }
        return medications.empty() ? std::nullopt : std::optional<std::size_t>{0};
    }
    ScreenToClient(window, &point);
    const D2D1_POINT_2F dip_point = D2D1::Point2F(dips(point.x), dips(point.y));
    for (std::size_t index = 0; index < medications.size(); ++index) {
        if (contains(row_layout(index).card, dip_point)) return index;
    }
    return std::nullopt;
}

bool add_button_tooltip(const HWND button, const wchar_t* text) {
    if (!tooltip_window) return false;
    TOOLINFO info{sizeof(TOOLINFO)};
    info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    info.hwnd = GetParent(button);
    info.uId = reinterpret_cast<UINT_PTR>(button);
    info.lpszText = const_cast<wchar_t*>(text);
    return SendMessage(tooltip_window, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&info)) != FALSE;
}

RoundedShape action_button_shape(const HWND button) {
    RECT client{};
    GetClientRect(button, &client);
    const float width = dips(client.right - client.left);
    const float height = dips(client.bottom - client.top);
    const float inset = design.button_inset;
    return rounded_shape(
        inset, inset, width - inset, height - inset,
        std::max(0.0F, std::min(width, height) * 0.5F - inset));
}

LRESULT CALLBACK action_button_procedure(
    const HWND button, const UINT message, const WPARAM w_param, const LPARAM l_param,
    UINT_PTR, DWORD_PTR) {
    if (message == WM_NCHITTEST) {
        POINT point{
            static_cast<LONG>(static_cast<short>(LOWORD(l_param))),
            static_cast<LONG>(static_cast<short>(HIWORD(l_param))),
        };
        ScreenToClient(button, &point);
        if (!contains(action_button_shape(button), D2D1::Point2F(dips(point.x), dips(point.y)))) {
            return HTTRANSPARENT;
        }
    } else if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(button, action_button_procedure, 1);
    }
    return DefSubclassProc(button, message, w_param, l_param);
}

bool create_action_buttons(const HWND window) {
    tooltip_window = CreateWindowEx(
        WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, window, nullptr,
        GetModuleHandle(nullptr), nullptr);
    for (std::size_t index = 0; index < medications.size(); ++index) {
        const RowLayout layout = row_layout(index);
        const RECT taken = pixel_rect(layout.taken_button.bounds);
        const HWND taken_button = CreateWindow(
            L"BUTTON", L"Taken", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            taken.left, taken.top, taken.right - taken.left, taken.bottom - taken.top, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(first_taken_button_id + static_cast<int>(index))),
            GetModuleHandle(nullptr), nullptr);
        const RECT edit = pixel_rect(layout.edit_button.bounds);
        const HWND edit_button = CreateWindow(
            L"BUTTON", L"Edit medication", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            edit.left, edit.top, edit.right - edit.left, edit.bottom - edit.top, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(first_edit_button_id + static_cast<int>(index))),
            GetModuleHandle(nullptr), nullptr);
        if (!taken_button || !edit_button) return false;
        if (!SetWindowSubclass(taken_button, action_button_procedure, 1, 0) ||
            !SetWindowSubclass(edit_button, action_button_procedure, 1, 0)) {
            return false;
        }
        EnableWindow(taken_button, medications[index].enabled);
        if (tooltip_window) {
            static_cast<void>(add_button_tooltip(taken_button, L"Taken"));
            static_cast<void>(add_button_tooltip(edit_button, L"Edit medication"));
        }
    }
    return true;
}

bool rebuild_widget(const HWND window) {
    while (const HWND child = GetWindow(window, GW_CHILD)) DestroyWindow(child);
    if (tooltip_window) DestroyWindow(tooltip_window);
    tooltip_window = nullptr;
    hovered_bar.reset();
    tracking_mouse_leave = false;
    rebuild_medication_icons();
    if (!create_action_buttons(window)) return false;
    SetWindowPos(
        window, nullptr, 0, 0, widget_width(), widget_height(), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(window, nullptr, TRUE);
    RECT bounds{};
    if (GetWindowRect(window, &bounds)) {
        const POINT clamped = clamped_widget_position({bounds.left, bounds.top});
        if (clamped.x != bounds.left || clamped.y != bounds.top) {
            SetWindowPos(
                window, nullptr, clamped.x, clamped.y, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            persist_window_position(window, false);
        }
    }
    schedule_refresh(window);
    InvalidateRect(window, nullptr, FALSE);
    return true;
}

void mark_medication_taken(const HWND window, const std::size_t index) {
    Medication& medication = medications[index];
    const auto previous = medication.last_taken_at;
    medication.mark_taken(std::chrono::system_clock::now());
    try {
        save_state();
        enable_startup_if_needed(window);
    } catch (const std::exception&) {
        medication.last_taken_at = previous;
        MessageBox(
            window, L"The medication time could not be saved.", L"Medication Cooldown Widget",
            MB_OK | MB_ICONERROR);
    }
    schedule_refresh(window);
    InvalidateRect(window, nullptr, FALSE);
}

void edit_medication_at(const HWND window, const std::size_t index) {
    Medication edited = medications[index];
    if (!edit_medication(window, edited)) return;
    const Medication previous = medications[index];
    medications[index] = std::move(edited);
    try {
        save_state();
        enable_startup_if_needed(window);
    } catch (const std::exception&) {
        medications[index] = previous;
        MessageBox(
            window, L"The medication changes could not be saved.", L"Medication Cooldown Widget",
            MB_OK | MB_ICONERROR);
    }
    rebuild_medication_icons();
    schedule_refresh(window);
    InvalidateRect(window, nullptr, FALSE);
}

template <typename T>
void release(T*& value) {
    if (!value) return;
    value->Release();
    value = nullptr;
}

D2D1_COLOR_F d2d_color(const COLORREF color) {
    return D2D1::ColorF(
        static_cast<float>(GetRValue(color)) / 255.0F,
        static_cast<float>(GetGValue(color)) / 255.0F,
        static_cast<float>(GetBValue(color)) / 255.0F,
        1.0F);
}

RoundedShape aligned_shape(const RoundedShape& shape) {
    return RoundedShape{
        rect(
            dips(pixels(shape.bounds.left)), dips(pixels(shape.bounds.top)),
            dips(pixels(shape.bounds.right)), dips(pixels(shape.bounds.bottom))),
        dips(pixels(shape.radius)),
    };
}

RoundedShape inset_shape(const RoundedShape& shape, const float inset) {
    return RoundedShape{
        rect(
            shape.bounds.left + inset, shape.bounds.top + inset,
            shape.bounds.right - inset, shape.bounds.bottom - inset),
        std::max(0.0F, shape.radius - inset),
    };
}

D2D1_ROUNDED_RECT native_shape(const RoundedShape& shape) {
    return D2D1::RoundedRect(shape.bounds, shape.radius, shape.radius);
}

bool begin_d2d(
    const HDC device, const RECT& bounds, ID2D1SolidColorBrush** brush,
    const D2D1_ALPHA_MODE alpha_mode = D2D1_ALPHA_MODE_IGNORE) {
    if (!d2d_factory && FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory))) {
        return false;
    }
    if (d2d_render_target && d2d_alpha_mode != alpha_mode) release(d2d_render_target);
    if (!d2d_render_target) {
        const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, alpha_mode),
            static_cast<float>(widget_dpi), static_cast<float>(widget_dpi));
        if (FAILED(d2d_factory->CreateDCRenderTarget(&properties, &d2d_render_target))) {
            release(d2d_factory);
            return false;
        }
        d2d_alpha_mode = alpha_mode;
    }
    d2d_render_target->SetDpi(static_cast<float>(widget_dpi), static_cast<float>(widget_dpi));
    if (FAILED(d2d_render_target->BindDC(device, &bounds))) {
        release(d2d_render_target);
        release(d2d_factory);
        return false;
    }
    d2d_render_target->BeginDraw();
    d2d_render_target->SetTransform(D2D1::Matrix3x2F::Identity());
    d2d_render_target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    if (FAILED(d2d_render_target->CreateSolidColorBrush(d2d_color(RGB(255, 255, 255)), brush))) {
        d2d_render_target->EndDraw();
        release(d2d_render_target);
        release(d2d_factory);
        return false;
    }
    return true;
}

void release_renderer() {
    release(d2d_render_target);
    d2d_alpha_mode = D2D1_ALPHA_MODE_UNKNOWN;
    release(d2d_factory);
}

bool end_d2d(const HWND window, ID2D1SolidColorBrush*& brush) {
    release(brush);
    const HRESULT result = d2d_render_target->EndDraw();
    if (result == D2DERR_RECREATE_TARGET) release(d2d_render_target);
    if (!SetTimer(window, renderer_release_timer, 50, nullptr)) release_renderer();
    return SUCCEEDED(result);
}

void fill_shape(
    ID2D1RenderTarget* target, ID2D1SolidColorBrush* brush,
    const RoundedShape& logical_shape, const COLORREF color) {
    const RoundedShape shape = aligned_shape(logical_shape);
    brush->SetColor(d2d_color(color));
    target->FillRoundedRectangle(native_shape(shape), brush);
}

void stroke_shape(
    ID2D1RenderTarget* target, ID2D1SolidColorBrush* brush,
    const RoundedShape& logical_shape, const COLORREF color, const float width = design.stroke) {
    const RoundedShape shape = inset_shape(aligned_shape(logical_shape), width * 0.5F);
    brush->SetColor(d2d_color(color));
    target->DrawRoundedRectangle(native_shape(shape), brush, width);
}

void fill_gradient(
    ID2D1RenderTarget* target, const RoundedShape& logical_shape,
    const COLORREF top, const COLORREF bottom) {
    const RoundedShape shape = aligned_shape(logical_shape);
    const D2D1_GRADIENT_STOP stops[]{
        {0.0F, d2d_color(top)},
        {1.0F, d2d_color(bottom)},
    };
    ID2D1GradientStopCollection* collection{};
    ID2D1LinearGradientBrush* brush{};
    if (SUCCEEDED(target->CreateGradientStopCollection(stops, 2, &collection)) &&
        SUCCEEDED(target->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(
                D2D1::Point2F(shape.bounds.left, shape.bounds.top),
                D2D1::Point2F(shape.bounds.left, shape.bounds.bottom)),
            collection, &brush))) {
        target->FillRoundedRectangle(native_shape(shape), brush);
    }
    release(brush);
    release(collection);
}

struct ButtonVisualState {
    bool disabled;
    bool pressed;
    bool hot;
    bool focused;
};

void draw_action_button_geometry(
    ID2D1RenderTarget* target, ID2D1SolidColorBrush* brush,
    const RoundedShape& circle, const ButtonVisualState state) {
    const bool disabled = state.disabled;
    const bool pressed = state.pressed;
    const bool hot = state.hot;
    const COLORREF fill = disabled ? RGB(45, 49, 56)
                          : pressed ? RGB(53, 59, 70)
                          : hot     ? RGB(69, 76, 89)
                                    : RGB(57, 63, 74);
    fill_shape(target, brush, circle, fill);
    stroke_shape(
        target, brush, circle,
        disabled ? RGB(72, 77, 86) : hot ? RGB(119, 128, 143) : RGB(86, 94, 107));
    if (state.focused) {
        stroke_shape(
            target, brush, inset_shape(circle, design.focus_inset - design.button_inset),
            RGB(218, 224, 235));
    }
}

void draw_action_button(const DRAWITEMSTRUCT& item) {
    const bool taken = item.CtlID >= first_taken_button_id && item.CtlID < first_edit_button_id;
    const ButtonVisualState state{
        .disabled = (item.itemState & ODS_DISABLED) != 0,
        .pressed = (item.itemState & ODS_SELECTED) != 0,
        .hot = (item.itemState & ODS_HOTLIGHT) != 0,
        .focused = (item.itemState & ODS_FOCUS) != 0,
    };
    ID2D1SolidColorBrush* brush{};
    if (begin_d2d(item.hDC, item.rcItem, &brush)) {
        brush->SetColor(d2d_color(RGB(34, 39, 48)));
        d2d_render_target->FillRectangle(
            rect(0.0F, 0.0F, dips(item.rcItem.right - item.rcItem.left), dips(item.rcItem.bottom - item.rcItem.top)),
            brush);
        const RoundedShape circle = action_button_shape(item.hwndItem);
        draw_action_button_geometry(d2d_render_target, brush, circle, state);
        end_d2d(GetAncestor(item.hwndItem, GA_ROOT), brush);
    }

    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, state.disabled ? RGB(115, 120, 130) : RGB(239, 242, 247));
    SelectObject(item.hDC, glyph_font);
    RECT glyph_bounds = item.rcItem;
    const wchar_t* glyph = taken ? L"\xE73E" : L"\xE70F";
    DrawText(item.hDC, glyph, 1, &glyph_bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

ButtonVisualState button_visual_state(const HWND button) {
    const LRESULT state = SendMessage(button, BM_GETSTATE, 0, 0);
    return ButtonVisualState{
        .disabled = !IsWindowEnabled(button),
        .pressed = (state & BST_PUSHED) != 0,
        .hot = (state & BST_HOT) != 0,
        .focused = GetFocus() == button,
    };
}

void render_redesigned_widget(
    const HWND window, const HDC device, RECT client,
    BYTE* const layered_bits, const int surface_width, const int surface_height) {
    const auto now = std::chrono::system_clock::now();
    ID2D1SolidColorBrush* brush{};
    if (begin_d2d(device, client, &brush, D2D1_ALPHA_MODE_PREMULTIPLIED)) {
        d2d_render_target->Clear(D2D1::ColorF(0.0F, 0.0F, 0.0F, 0.0F));
        if (medications.empty()) {
            const RoundedShape card = empty_card_layout();
            fill_gradient(d2d_render_target, card, RGB(40, 46, 56), RGB(24, 29, 37));
            stroke_shape(d2d_render_target, brush, card, RGB(72, 80, 92));
        } else {
            for (std::size_t index = 0; index < medications.size(); ++index) {
                const Medication& medication = medications[index];
                const RowLayout layout = row_layout(index);
                const std::chrono::minutes remaining = medication.remaining_at(now);
                const bool due = medication.enabled && remaining == std::chrono::minutes::zero();
                const bool soon = medication.is_soon_at(now);
                const bool paused = !medication.enabled;

                fill_gradient(
                    d2d_render_target, layout.card,
                    paused ? RGB(47, 49, 54) : RGB(43, 49, 59),
                    paused ? RGB(31, 33, 37) : RGB(24, 29, 37));
                stroke_shape(d2d_render_target, brush, layout.card, RGB(73, 81, 94));
                fill_gradient(
                    d2d_render_target, layout.icon_tile,
                    paused ? RGB(72, 74, 79) : RGB(68, 75, 87),
                    paused ? RGB(51, 53, 57) : RGB(45, 51, 61));
                stroke_shape(d2d_render_target, brush, layout.icon_tile, RGB(91, 99, 112));

                const std::wstring state = status_text(medication, now);
                if (!state.empty()) {
                    const COLORREF state_accent = paused ? RGB(143, 148, 158)
                                                  : due   ? RGB(229, 77, 83)
                                                          : RGB(232, 169, 66);
                    fill_shape(d2d_render_target, brush, layout.badge, RGB(35, 40, 48));
                    stroke_shape(d2d_render_target, brush, layout.badge, state_accent);
                }

                const COLORREF accent = paused ? RGB(111, 116, 126)
                                        : due   ? RGB(218, 64, 71)
                                        : soon  ? RGB(221, 158, 54)
                                                : RGB(207, 214, 224);
                fill_shape(d2d_render_target, brush, layout.progress_bar, RGB(20, 24, 30));
                double progress = paused ? 1.0 : 0.0;
                if (medication.enabled && remaining > std::chrono::minutes::zero() && medication.interval.count() > 0) {
                    progress = static_cast<double>(remaining.count()) /
                               static_cast<double>(medication.interval.count());
                }
                progress = std::clamp(progress, 0.0, 1.0);
                if (progress > 0.0) {
                    RoundedShape progress_fill = layout.progress_bar;
                    progress_fill.bounds.right = progress_fill.bounds.left +
                        (progress_fill.bounds.right - progress_fill.bounds.left) * static_cast<float>(progress);
                    progress_fill.radius = std::min(
                        progress_fill.radius,
                        (progress_fill.bounds.right - progress_fill.bounds.left) * 0.5F);
                    fill_shape(d2d_render_target, brush, progress_fill, accent);
                }
                stroke_shape(
                    d2d_render_target, brush, layout.progress_bar,
                    due ? accent : RGB(81, 89, 102));
                fill_shape(d2d_render_target, brush, layout.action_panel, RGB(34, 39, 48));
                stroke_shape(d2d_render_target, brush, layout.action_panel, RGB(79, 87, 100));
                draw_action_button_geometry(
                    d2d_render_target, brush, layout.taken_button,
                    button_visual_state(GetDlgItem(
                        window, first_taken_button_id + static_cast<int>(index))));
                draw_action_button_geometry(
                    d2d_render_target, brush, layout.edit_button,
                    button_visual_state(GetDlgItem(
                        window, first_edit_button_id + static_cast<int>(index))));
            }
        }
        if (!end_d2d(window, brush)) InvalidateRect(window, nullptr, FALSE);
    }

    std::vector<BYTE> alpha;
    if (layered_bits) {
        alpha.resize(static_cast<std::size_t>(surface_width) * static_cast<std::size_t>(surface_height));
        for (std::size_t index = 0; index < alpha.size(); ++index) alpha[index] = layered_bits[index * 4 + 3];
    }

    SetBkMode(device, TRANSPARENT);

    if (medications.empty()) {
        SetTextColor(device, RGB(174, 180, 190));
        SelectObject(device, dose_font);
        DrawText(device, L"Right-click to add medication", -1, &client, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (layered_bits) {
            for (std::size_t index = 0; index < alpha.size(); ++index) {
                layered_bits[index * 4 + 3] = alpha[index];
            }
        }
        return;
    }

    for (std::size_t index = 0; index < medications.size(); ++index) {
        const Medication& medication = medications[index];
        const RowLayout layout = row_layout(index);
        const std::chrono::minutes remaining = medication.remaining_at(now);
        const bool due = medication.enabled && remaining == std::chrono::minutes::zero();
        const bool paused = !medication.enabled;
        const RECT icon_bounds = pixel_rect(layout.icon);
        const int icon_size = pixels(design.icon_size);
        if (index < medication_icons.size() && medication_icons[index]) {
            const HDC memory = CreateCompatibleDC(device);
            const HGDIOBJ previous = SelectObject(memory, medication_icons[index]);
            const BLENDFUNCTION blend{AC_SRC_OVER, 0, static_cast<BYTE>(paused ? 150 : 255), AC_SRC_ALPHA};
            AlphaBlend(
                device, icon_bounds.left, icon_bounds.top, icon_size, icon_size, memory, 0, 0, icon_size, icon_size,
                blend);
            SelectObject(memory, previous);
            DeleteDC(memory);
        } else if (!medication.name.empty()) {
            SetTextColor(device, paused ? RGB(173, 176, 182) : RGB(239, 242, 247));
            SelectObject(device, initial_font);
            const wchar_t initial[]{medication.name.front(), L'\0'};
            RECT initial_bounds = pixel_rect(layout.icon_tile.bounds);
            DrawText(device, initial, 1, &initial_bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        const std::wstring state = status_text(medication, now);
        RECT name_bounds = pixel_rect(state.empty() ? layout.name_without_state : layout.name_with_state);
        SetTextColor(device, paused ? RGB(191, 195, 202) : RGB(242, 244, 248));
        SelectObject(device, name_font);
        DrawText(device, medication.name.c_str(), -1, &name_bounds, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT dose_bounds = pixel_rect(layout.dose);
        SetTextColor(device, RGB(169, 176, 188));
        SelectObject(device, dose_font);
        DrawText(device, medication.dose.c_str(), -1, &dose_bounds, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        if (!state.empty()) {
            RECT badge = pixel_rect(layout.badge.bounds);
            const COLORREF state_accent = paused ? RGB(143, 148, 158)
                                          : due   ? RGB(229, 77, 83)
                                                  : RGB(232, 169, 66);
            SetTextColor(device, state_accent);
            SelectObject(device, status_font);
            DrawText(device, state.c_str(), -1, &badge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        RECT bar_text = pixel_rect(layout.progress_text);
        const std::wstring text = hovered_bar == index ? local_timestamp_text(medication, now)
                                                       : countdown_text(medication, now);
        SetTextColor(device, due ? RGB(244, 104, 110) : RGB(240, 243, 248));
        SelectObject(device, countdown_font);
        DrawText(
            device, text.c_str(), -1, &bar_text,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        const HWND taken_button = GetDlgItem(window, first_taken_button_id + static_cast<int>(index));
        SetTextColor(device, IsWindowEnabled(taken_button) ? RGB(239, 242, 247) : RGB(115, 120, 130));
        SelectObject(device, glyph_font);
        RECT taken_bounds = pixel_rect(layout.taken_button.bounds);
        DrawText(device, L"\xE73E", 1, &taken_bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SetTextColor(device, RGB(239, 242, 247));
        RECT edit_bounds = pixel_rect(layout.edit_button.bounds);
        DrawText(device, L"\xE70F", 1, &edit_bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    if (layered_bits) {
        for (std::size_t index = 0; index < alpha.size(); ++index) layered_bits[index * 4 + 3] = alpha[index];
    }
}

void paint_redesigned_widget(const HWND window) {
    PAINTSTRUCT paint{};
    // The frame is rendered into an offscreen DIB and published with UpdateLayeredWindow, so the
    // paint device is unused. BeginPaint/EndPaint still has to run to clear the update region.
    static_cast<void>(BeginPaint(window, &paint));
    RECT client{};
    GetClientRect(window, &client);
    EndPaint(window, &paint);

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    BITMAPINFO bitmap_info{};
    bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info.bmiHeader.biWidth = width;
    bitmap_info.bmiHeader.biHeight = -height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    HDC memory = screen ? CreateCompatibleDC(screen) : nullptr;
    void* bits{};
    HBITMAP bitmap = memory
                         ? CreateDIBSection(memory, &bitmap_info, DIB_RGB_COLORS, &bits, nullptr, 0)
                         : nullptr;
    if (!bitmap || !bits) {
        if (bitmap) DeleteObject(bitmap);
        if (memory) DeleteDC(memory);
        if (screen) ReleaseDC(nullptr, screen);
        return;
    }

    const HGDIOBJ previous = SelectObject(memory, bitmap);
    std::memset(bits, 0, static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    render_redesigned_widget(window, memory, client, static_cast<BYTE*>(bits), width, height);

    RECT window_bounds{};
    GetWindowRect(window, &window_bounds);
    POINT destination{window_bounds.left, window_bounds.top};
    POINT source{};
    SIZE size{width, height};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(window, screen, &destination, &size, memory, &source, 0, &blend, ULW_ALPHA);

    SelectObject(memory, previous);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
}

LRESULT CALLBACK window_procedure(const HWND window, const UINT message, const WPARAM w_param, const LPARAM l_param) {
    if (message == taskbar_created_message) {
        tray_icon_added = false;
        if (!add_tray_icon(window)) {
            MessageBox(
                window, L"The system tray icon could not be restored.", L"Medication Cooldown Widget",
                MB_OK | MB_ICONWARNING);
        }
        return 0;
    }

    switch (message) {
    case WM_CREATE:
        if (!create_action_buttons(window)) return -1;
        if (!add_tray_icon(window)) {
            MessageBox(
                window, L"The system tray icon could not be created.", L"Medication Cooldown Widget",
                MB_OK | MB_ICONWARNING);
        }
        return 0;
    case tray_callback_message:
        if (l_param == WM_LBUTTONDBLCLK) {
            show_widget(window);
        } else if (l_param == WM_RBUTTONUP || l_param == WM_CONTEXTMENU) {
            POINT point{};
            if (GetCursorPos(&point)) show_tray_menu(window, point);
        }
        return 0;
    case WM_COMMAND: {
        const int taken_index = LOWORD(w_param) - first_taken_button_id;
        const int edit_index = LOWORD(w_param) - first_edit_button_id;
        if (HIWORD(w_param) == BN_CLICKED && taken_index >= 0 &&
            static_cast<std::size_t>(taken_index) < medications.size()) {
            mark_medication_taken(window, static_cast<std::size_t>(taken_index));
            return 0;
        }
        if (HIWORD(w_param) == BN_CLICKED && edit_index >= 0 &&
            static_cast<std::size_t>(edit_index) < medications.size()) {
            edit_medication_at(window, static_cast<std::size_t>(edit_index));
            return 0;
        }
        break;
    }
    case WM_DRAWITEM:
        if (w_param >= first_taken_button_id &&
            w_param < first_edit_button_id + static_cast<WPARAM>(medications.size())) {
            draw_action_button(*reinterpret_cast<const DRAWITEMSTRUCT*>(l_param));
            return TRUE;
        }
        break;
    case WM_CONTEXTMENU: {
        POINT point{
            static_cast<LONG>(static_cast<short>(LOWORD(l_param))),
            static_cast<LONG>(static_cast<short>(HIWORD(l_param))),
        };
        const auto index = medication_at(window, point);
        if (point.x == -1 && point.y == -1) {
            RECT bounds{};
            GetWindowRect(window, &bounds);
            point = POINT{bounds.left + scaled(82), bounds.top + scaled(20)};
        }

        const HMENU menu = CreatePopupMenu();
        if (!menu) return 0;
        if (index) {
            AppendMenu(menu, MF_STRING, edit_medication_command, L"Edit...");
            AppendMenu(
                menu, MF_STRING, toggle_paused_command, medications[*index].enabled ? L"Pause" : L"Resume");
            AppendMenu(menu, MF_STRING, remove_medication_command, L"Remove");
            AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
        }
        AppendMenu(menu, MF_STRING, add_medication_command, L"Add medication...");
        AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenu(
            menu, MF_STRING | (widget_settings.position_locked ? MF_CHECKED : MF_UNCHECKED),
            lock_position_command, L"Lock position");
        AppendMenu(
            menu, MF_STRING | (widget_settings.always_on_top ? MF_CHECKED : MF_UNCHECKED),
            always_on_top_command, L"Always on top");
        SetForegroundWindow(window);
        const UINT command = TrackPopupMenu(
            menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, point.x, point.y, 0, window, nullptr);
        DestroyMenu(menu);
        if (command == edit_medication_command && index) {
            edit_medication_at(window, *index);
        } else if (command == add_medication_command) {
            Medication added{.interval = std::chrono::hours{24}};
            if (edit_medication(window, added)) {
                added.id = new_medication_id();
                medications.push_back(std::move(added));
                try {
                    save_state();
                    enable_startup_if_needed(window);
                } catch (const std::exception&) {
                    medications.pop_back();
                    MessageBox(
                        window, L"The medication could not be added.", L"Medication Cooldown Widget",
                        MB_OK | MB_ICONERROR);
                    return 0;
                }
                if (!rebuild_widget(window)) DestroyWindow(window);
            }
        } else if (command == toggle_paused_command && index) {
            Medication& medication = medications[*index];
            medication.enabled = !medication.enabled;
            try {
                save_state();
                enable_startup_if_needed(window);
            } catch (const std::exception&) {
                medication.enabled = !medication.enabled;
                MessageBox(
                    window, L"The medication state could not be saved.", L"Medication Cooldown Widget",
                    MB_OK | MB_ICONERROR);
            }
            EnableWindow(GetDlgItem(window, first_taken_button_id + static_cast<int>(*index)), medication.enabled);
            schedule_refresh(window);
            InvalidateRect(window, nullptr, FALSE);
        } else if (command == remove_medication_command && index) {
            const std::wstring prompt = L"Remove " + medications[*index].name + L"?";
            if (MessageBox(
                    window, prompt.c_str(), L"Medication Cooldown Widget", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) ==
                IDYES) {
                const Medication removed = medications[*index];
                medications.erase(medications.begin() + static_cast<std::ptrdiff_t>(*index));
                try {
                    save_state();
                    enable_startup_if_needed(window);
                } catch (const std::exception&) {
                    medications.insert(
                        medications.begin() + static_cast<std::ptrdiff_t>(*index), removed);
                    MessageBox(
                        window, L"The medication could not be removed.", L"Medication Cooldown Widget",
                        MB_OK | MB_ICONERROR);
                    return 0;
                }

                if (!rebuild_widget(window)) {
                    MessageBox(
                        window, L"The medication was removed, but the widget could not be refreshed.",
                        L"Medication Cooldown Widget", MB_OK | MB_ICONERROR);
                    DestroyWindow(window);
                    return 0;
                }
            }
        } else if (command == lock_position_command) {
            widget_settings.position_locked = !widget_settings.position_locked;
            try {
                save_state();
            } catch (const std::exception&) {
                widget_settings.position_locked = !widget_settings.position_locked;
                MessageBox(
                    window, L"The position lock setting could not be saved.", L"Medication Cooldown Widget",
                    MB_OK | MB_ICONERROR);
            }
        } else if (command == always_on_top_command) {
            widget_settings.always_on_top = !widget_settings.always_on_top;
            apply_topmost(window);
            try {
                save_state();
            } catch (const std::exception&) {
                widget_settings.always_on_top = !widget_settings.always_on_top;
                apply_topmost(window);
                MessageBox(
                    window, L"The always-on-top setting could not be saved.", L"Medication Cooldown Widget",
                    MB_OK | MB_ICONERROR);
            }
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
        if (!widget_settings.position_locked) {
            POINT point{
                static_cast<LONG>(static_cast<short>(LOWORD(l_param))),
                static_cast<LONG>(static_cast<short>(HIWORD(l_param))),
            };
            ClientToScreen(window, &point);
            ReleaseCapture();
            SendMessage(window, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
        }
        return 0;
    case WM_MOUSEMOVE: {
        if (!tracking_mouse_leave) {
            TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT), TME_LEAVE, window, 0};
            tracking_mouse_leave = TrackMouseEvent(&tracking) != FALSE;
        }
        const POINT point{
            static_cast<LONG>(static_cast<short>(LOWORD(l_param))),
            static_cast<LONG>(static_cast<short>(HIWORD(l_param))),
        };
        const auto next_hover = progress_bar_at(point);
        if (next_hover != hovered_bar) {
            const auto previous_hover = hovered_bar;
            hovered_bar = next_hover;
            invalidate_progress_bar(window, previous_hover);
            invalidate_progress_bar(window, hovered_bar);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        tracking_mouse_leave = false;
        if (hovered_bar) {
            const auto previous_hover = hovered_bar;
            hovered_bar.reset();
            invalidate_progress_bar(window, previous_hover);
        }
        return 0;
    case WM_KEYDOWN:
        if (w_param == VK_APPS || (w_param == VK_F10 && (GetKeyState(VK_SHIFT) & 0x8000))) {
            SendMessage(window, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(window), static_cast<LPARAM>(-1));
            return 0;
        }
        break;
    case WM_EXITSIZEMOVE:
        persist_window_position(window, true);
        return 0;
    case WM_DISPLAYCHANGE: {
        RECT bounds{};
        if (GetWindowRect(window, &bounds)) {
            const POINT clamped = clamped_widget_position({bounds.left, bounds.top});
            if (clamped.x != bounds.left || clamped.y != bounds.top) {
                SetWindowPos(
                    window, nullptr, clamped.x, clamped.y, 0, 0,
                    SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                persist_window_position(window, false);
            }
        }
        return 0;
    }
    case WM_DPICHANGED: {
        widget_dpi = HIWORD(w_param);
        while (const HWND child = GetWindow(window, GW_CHILD)) DestroyWindow(child);
        if (tooltip_window) DestroyWindow(tooltip_window);
        tooltip_window = nullptr;
        hovered_bar.reset();
        tracking_mouse_leave = false;
        rebuild_fonts();
        rebuild_medication_icons();
        if (!create_action_buttons(window)) {
            DestroyWindow(window);
            return 0;
        }
        const auto* suggested = reinterpret_cast<const RECT*>(l_param);
        const POINT position = clamped_widget_position({suggested->left, suggested->top});
        SetWindowPos(
            window, nullptr, position.x, position.y, widget_width(), widget_height(),
            SWP_NOZORDER | SWP_NOACTIVATE);
        persist_window_position(window, false);
        schedule_refresh(window);
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    case WM_TIMER:
        if (w_param == renderer_release_timer) {
            KillTimer(window, renderer_release_timer);
            release_renderer();
            return 0;
        }
        if (w_param == refresh_timer) {
            schedule_refresh(window);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        break;
    case WM_PAINT:
        paint_redesigned_widget(window);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        KillTimer(window, refresh_timer);
        KillTimer(window, renderer_release_timer);
        release_renderer();
        remove_tray_icon(window);
        clear_medication_icons();
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProc(window, message, w_param, l_param);
}
}

int WINAPI wWinMain(const HINSTANCE instance, HINSTANCE, PWSTR, const int show_command) {
    enable_per_monitor_dpi();
    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    struct ComCleanup {
        bool uninitialize;
        ~ComCleanup() {
            clear_medication_icons();
            release(d2d_render_target);
            release(d2d_factory);
            if (name_font) DeleteObject(name_font);
            if (dose_font) DeleteObject(dose_font);
            if (status_font) DeleteObject(status_font);
            if (countdown_font) DeleteObject(countdown_font);
            if (initial_font) DeleteObject(initial_font);
            if (glyph_font) DeleteObject(glyph_font);
            if (imaging_factory) imaging_factory->Release();
            if (uninitialize) CoUninitialize();
        }
    } cleanup{SUCCEEDED(com_result)};
    widget_dpi = system_dpi();
    rebuild_fonts();
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory))) return 1;
    if (SUCCEEDED(com_result) || com_result == RPC_E_CHANGED_MODE) {
        CoCreateInstance(
            CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory,
            reinterpret_cast<void**>(&imaging_factory));
    }

    const INITCOMMONCONTROLSEX common_controls{
        sizeof(INITCOMMONCONTROLSEX), ICC_DATE_CLASSES | ICC_WIN95_CLASSES};
    if (!InitCommonControlsEx(&common_controls)) return 1;

    bool configuration_missing{};
    try {
        medications_path = local_medications_path();
        configuration_missing = !std::filesystem::exists(medications_path);
        medications = load_medications(medications_path, &widget_settings);
        enable_startup_after_save = configuration_missing;
    } catch (const std::exception&) {
        MessageBox(nullptr, L"Saved medications could not be loaded.", L"Medication Cooldown Widget", MB_OK | MB_ICONERROR);
        return 1;
    }
    if (medications.empty() && configuration_missing) medications.push_back(example_medication());
    rebuild_medication_icons();

    taskbar_created_message = RegisterWindowMessage(L"TaskbarCreated");
    if (taskbar_created_message == 0) {
        MessageBox(
            nullptr, L"System tray integration could not be initialized.", L"Medication Cooldown Widget",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    const WNDCLASS window_definition{
        .lpfnWndProc = window_procedure,
        .hInstance = instance,
        .hCursor = LoadCursor(nullptr, IDC_ARROW),
        .lpszClassName = window_class,
    };
    if (!RegisterClass(&window_definition)) return 1;

    const bool has_saved_position = widget_settings.window_x && widget_settings.window_y;
    const POINT initial_position = has_saved_position
                                       ? clamped_widget_position({*widget_settings.window_x, *widget_settings.window_y})
                                       : POINT{CW_USEDEFAULT, CW_USEDEFAULT};
    const HWND window = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT | WS_EX_LAYERED |
            (widget_settings.always_on_top ? WS_EX_TOPMOST : 0),
        window_class,
        L"Medication Cooldown Widget", WS_POPUP | WS_CLIPCHILDREN, initial_position.x, initial_position.y, widget_width(),
        widget_height(), nullptr, nullptr, instance, nullptr);
    if (!window) return 1;

    const UINT initial_dpi = window_dpi(window);
    if (initial_dpi != widget_dpi) {
        widget_dpi = initial_dpi;
        while (const HWND child = GetWindow(window, GW_CHILD)) DestroyWindow(child);
        rebuild_fonts();
        if (!rebuild_widget(window)) return 1;
    }

    ShowWindow(window, show_command);
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    schedule_refresh(window);

    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessage(window, &message)) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
    }
    return static_cast<int>(message.wParam);
}
