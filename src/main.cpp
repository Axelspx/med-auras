#include "resource.h"
#include "storage.h"
#include "startup.h"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
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
constexpr int first_taken_button_id = 100;
constexpr int row_top = 12;
constexpr int row_step = 80;
constexpr int row_height = 76;
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
std::vector<HBITMAP> medication_icons;
bool enable_startup_after_save{};
bool tray_icon_added{};
UINT taskbar_created_message{};

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
    if (medications.empty()) return 60;
    return row_top * 2 + static_cast<int>(medications.size()) * row_step - (row_step - row_height);
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

std::wstring remaining_text(const Medication& medication, const std::chrono::system_clock::time_point now) {
    if (!medication.enabled) return L"PAUSED";
    const std::chrono::minutes remaining = medication.remaining_at(now);
    if (remaining == std::chrono::minutes::zero()) return L"READY";
    const auto hours = std::chrono::duration_cast<std::chrono::hours>(remaining);
    const auto minutes = remaining - hours;
    if (hours == std::chrono::hours::zero()) return std::to_wstring(minutes.count()) + L"m";
    return std::to_wstring(hours.count()) + L"h " + std::to_wstring(minutes.count()) + L"m";
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

HBITMAP load_medication_icon(const std::wstring& path) {
    if (!imaging_factory) return nullptr;

    IWICBitmapDecoder* decoder{};
    IWICBitmapFrameDecode* frame{};
    IWICBitmapScaler* scaler{};
    IWICFormatConverter* converter{};
    HBITMAP bitmap{};
    void* pixels{};
    UINT width{};
    UINT height{};
    UINT scaled_width{};
    UINT scaled_height{};

    HRESULT result = imaging_factory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(result)) result = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(result)) result = frame->GetSize(&width, &height);
    if (SUCCEEDED(result) && width > 0 && height > 0) {
        const double scale = std::min(48.0 / width, 48.0 / height);
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
        info.bmiHeader.biWidth = 48;
        info.bmiHeader.biHeight = -48;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        const HDC device = GetDC(nullptr);
        bitmap = CreateDIBSection(device, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        ReleaseDC(nullptr, device);
        if (!bitmap || !pixels) result = E_OUTOFMEMORY;
    }
    if (SUCCEEDED(result)) {
        constexpr UINT stride = 48 * 4;
        std::memset(pixels, 0, 48 * stride);
        auto* destination = static_cast<BYTE*>(pixels) + ((48 - scaled_height) / 2 * 48 +
                                                          (48 - scaled_width) / 2) * 4;
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
        std::clamp(position.x, info.rcWork.left, std::max(info.rcWork.left, info.rcWork.right - 420)),
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

std::optional<std::size_t> medication_at(const HWND window, POINT point) {
    if (point.x == -1 && point.y == -1) {
        if (medications.empty()) return std::nullopt;
        return 0;
    }
    ScreenToClient(window, &point);
    if (point.x < 12 || point.x >= 408 || point.y < row_top) return std::nullopt;
    const int offset = point.y - row_top;
    const std::size_t index = static_cast<std::size_t>(offset / row_step);
    if (offset % row_step >= row_height || index >= medications.size()) return std::nullopt;
    return index;
}

bool create_taken_buttons(const HWND window) {
    for (std::size_t index = 0; index < medications.size(); ++index) {
        const int button_id = first_taken_button_id + static_cast<int>(index);
        const HWND button = CreateWindow(
            L"BUTTON", L"Taken", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 322,
            row_top + static_cast<int>(index * row_step) + 20, 76, 36, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(button_id)), GetModuleHandle(nullptr), nullptr);
        if (!button) return false;
        SendMessage(button, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        EnableWindow(button, medications[index].enabled);
    }
    return true;
}

bool rebuild_widget(const HWND window) {
    while (const HWND child = GetWindow(window, GW_CHILD)) DestroyWindow(child);
    rebuild_medication_icons();
    if (!create_taken_buttons(window)) return false;
    SetWindowPos(
        window, nullptr, 0, 0, 420, widget_height(), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
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

void paint_widget(const HWND window) {
    PAINTSTRUCT paint{};
    const HDC device = BeginPaint(window, &paint);
    RECT client{};
    GetClientRect(window, &client);

    const HBRUSH background = CreateSolidBrush(RGB(18, 21, 27));
    FillRect(device, &client, background);
    DeleteObject(background);

    if (medications.empty()) {
        SetBkMode(device, TRANSPARENT);
        SetTextColor(device, RGB(174, 180, 190));
        SelectObject(device, GetStockObject(DEFAULT_GUI_FONT));
        DrawText(device, L"Right-click to add medication", -1, &client, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    const auto now = std::chrono::system_clock::now();
    for (std::size_t index = 0; index < medications.size(); ++index) {
        const Medication& medication = medications[index];
        const LONG top = row_top + static_cast<LONG>(index * row_step);
        const std::chrono::minutes remaining = medication.remaining_at(now);

        const HBRUSH row = CreateSolidBrush(medication.enabled ? RGB(31, 36, 45) : RGB(37, 39, 43));
        const RECT row_bounds{12, top, 408, top + row_height};
        FillRect(device, &row_bounds, row);
        DeleteObject(row);

        const RECT icon_bounds{22, top + 14, 70, top + 62};
        if (index < medication_icons.size() && medication_icons[index]) {
            const HDC memory = CreateCompatibleDC(device);
            const HGDIOBJ previous = SelectObject(memory, medication_icons[index]);
            const BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
            AlphaBlend(device, icon_bounds.left, icon_bounds.top, 48, 48, memory, 0, 0, 48, 48, blend);
            SelectObject(memory, previous);
            DeleteDC(memory);
        } else {
            const HBRUSH icon = CreateSolidBrush(medication.enabled ? RGB(66, 82, 110) : RGB(72, 74, 78));
            FillRect(device, &icon_bounds, icon);
            DeleteObject(icon);
        }

        SetBkMode(device, TRANSPARENT);
        SetTextColor(device, RGB(239, 242, 247));
        SelectObject(device, GetStockObject(DEFAULT_GUI_FONT));
        RECT name_bounds{82, top + 10, 300, top + 31};
        const std::wstring label = medication.name + (medication.dose.empty() ? L"" : L"  " + medication.dose);
        DrawText(device, label.c_str(), -1, &name_bounds, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        const std::wstring status = remaining_text(medication, now);
        SetTextColor(device, medication.enabled && remaining == std::chrono::minutes::zero() ? RGB(104, 218, 142)
                                                                                             : RGB(205, 211, 221));
        RECT status_bounds{238, top + 10, 312, top + 31};
        DrawText(device, status.c_str(), -1, &status_bounds, DT_RIGHT | DT_SINGLELINE);

        const RECT bar_bounds{82, top + 42, 312, top + 55};
        const HBRUSH bar_background = CreateSolidBrush(RGB(11, 13, 17));
        FillRect(device, &bar_bounds, bar_background);
        DeleteObject(bar_background);

        double progress = 1.0;
        if (medication.enabled && remaining > std::chrono::minutes::zero() && medication.interval.count() > 0) {
            progress = 1.0 - static_cast<double>(remaining.count()) / static_cast<double>(medication.interval.count());
        }
        RECT progress_bounds = bar_bounds;
        progress_bounds.right = progress_bounds.left + static_cast<LONG>(230.0 * std::clamp(progress, 0.0, 1.0));
        const HBRUSH progress_brush = CreateSolidBrush(!medication.enabled                               ? RGB(80, 82, 86)
                                                       : remaining == std::chrono::minutes::zero() ? RGB(49, 145, 83)
                                                                                                   : RGB(55, 104, 178));
        FillRect(device, &progress_bounds, progress_brush);
        DeleteObject(progress_brush);
    }

    EndPaint(window, &paint);
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
        if (!create_taken_buttons(window)) return -1;
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
        const int index = LOWORD(w_param) - first_taken_button_id;
        if (HIWORD(w_param) == BN_CLICKED && index >= 0 && static_cast<std::size_t>(index) < medications.size()) {
            Medication& medication = medications[static_cast<std::size_t>(index)];
            const auto previous = medication.last_taken_at;
            medication.mark_taken(std::chrono::system_clock::now());
            try {
                save_state();
                enable_startup_if_needed(window);
            } catch (const std::exception&) {
                medication.last_taken_at = previous;
                MessageBox(window, L"The medication time could not be saved.", L"Medication Cooldown Widget", MB_OK | MB_ICONERROR);
            }
            schedule_refresh(window);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        break;
    }
    case WM_CONTEXTMENU: {
        POINT point{
            static_cast<LONG>(static_cast<short>(LOWORD(l_param))),
            static_cast<LONG>(static_cast<short>(HIWORD(l_param))),
        };
        const auto index = medication_at(window, point);
        if (point.x == -1 && point.y == -1) {
            RECT bounds{};
            GetWindowRect(window, &bounds);
            point = POINT{bounds.left + 82, bounds.top + row_top + 20};
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
            Medication edited = medications[*index];
            if (edit_medication(window, edited)) {
                const Medication previous = medications[*index];
                medications[*index] = std::move(edited);
                try {
                    save_state();
                    enable_startup_if_needed(window);
                } catch (const std::exception&) {
                    medications[*index] = previous;
                    MessageBox(
                        window, L"The medication changes could not be saved.", L"Medication Cooldown Widget",
                        MB_OK | MB_ICONERROR);
                }
                rebuild_medication_icons();
                schedule_refresh(window);
                InvalidateRect(window, nullptr, FALSE);
            }
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
    case WM_TIMER:
        if (w_param == refresh_timer) {
            schedule_refresh(window);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        break;
    case WM_PAINT:
        paint_widget(window);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        KillTimer(window, refresh_timer);
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
    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    struct ComCleanup {
        bool uninitialize;
        ~ComCleanup() {
            clear_medication_icons();
            if (imaging_factory) imaging_factory->Release();
            if (uninitialize) CoUninitialize();
        }
    } cleanup{SUCCEEDED(com_result)};
    if (SUCCEEDED(com_result) || com_result == RPC_E_CHANGED_MODE) {
        CoCreateInstance(
            CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory,
            reinterpret_cast<void**>(&imaging_factory));
    }

    const INITCOMMONCONTROLSEX common_controls{sizeof(INITCOMMONCONTROLSEX), ICC_DATE_CLASSES};
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
        WS_EX_TOOLWINDOW | (widget_settings.always_on_top ? WS_EX_TOPMOST : 0), window_class,
        L"Medication Cooldown Widget", WS_POPUP, initial_position.x, initial_position.y, 420, widget_height(), nullptr,
        nullptr, instance, nullptr);
    if (!window) return 1;

    ShowWindow(window, show_command);
    schedule_refresh(window);

    MSG message{};
    while (GetMessage(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    return static_cast<int>(message.wParam);
}
