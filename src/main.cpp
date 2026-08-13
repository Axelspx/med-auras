#include "resource.h"
#include "storage.h"
#include "startup.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cwchar>
#include <filesystem>
#include <optional>
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

std::vector<Medication> medications;
std::filesystem::path medications_path;
bool enable_startup_after_save{};

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

INT_PTR CALLBACK medication_editor_procedure(
    const HWND dialog, const UINT message, const WPARAM w_param, const LPARAM l_param) {
    if (message == WM_INITDIALOG) {
        auto* medication = reinterpret_cast<Medication*>(l_param);
        SetWindowLongPtr(dialog, GWLP_USERDATA, l_param);
        SetWindowText(dialog, medication->id.empty() ? L"Add medication" : L"Edit medication");
        SetDlgItemText(dialog, IDC_MEDICATION_NAME, medication->name.c_str());
        SetDlgItemText(dialog, IDC_MEDICATION_DOSE, medication->dose.c_str());
        SetDlgItemText(
            dialog, IDC_MEDICATION_INTERVAL, std::to_wstring(medication->interval.count()).c_str());
        SetFocus(GetDlgItem(dialog, IDC_MEDICATION_NAME));
        return FALSE;
    }
    if (message != WM_COMMAND) return FALSE;
    if (LOWORD(w_param) == IDCANCEL) {
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    if (LOWORD(w_param) != IDOK) return FALSE;

    auto* medication = reinterpret_cast<Medication*>(GetWindowLongPtr(dialog, GWLP_USERDATA));
    std::array<wchar_t, 256> name{};
    std::array<wchar_t, 128> dose{};
    std::array<wchar_t, 32> interval_text{};
    GetDlgItemText(dialog, IDC_MEDICATION_NAME, name.data(), static_cast<int>(name.size()));
    GetDlgItemText(dialog, IDC_MEDICATION_DOSE, dose.data(), static_cast<int>(dose.size()));
    GetDlgItemText(
        dialog, IDC_MEDICATION_INTERVAL, interval_text.data(), static_cast<int>(interval_text.size()));

    wchar_t* interval_end{};
    const long long interval = std::wcstoll(interval_text.data(), &interval_end, 10);
    if (name[0] == L'\0' || interval <= 0 || interval_end == interval_text.data() || *interval_end != L'\0') {
        MessageBox(
            dialog, L"Enter a medication name and a positive interval in minutes.", L"Medication",
            MB_OK | MB_ICONWARNING);
        return TRUE;
    }

    medication->name = name.data();
    medication->dose = dose.data();
    medication->interval = std::chrono::minutes{interval};
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
    const auto now = std::chrono::system_clock::now();
    if (std::ranges::any_of(medications, [now](const Medication& medication) {
            return medication.enabled && medication.remaining_at(now) > std::chrono::minutes::zero();
        })) {
        SetTimer(window, refresh_timer, 60'000, nullptr);
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
    if (!create_taken_buttons(window)) return false;
    SetWindowPos(
        window, nullptr, 0, 0, 420, widget_height(), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
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

        const HBRUSH icon = CreateSolidBrush(medication.enabled ? RGB(66, 82, 110) : RGB(72, 74, 78));
        const RECT icon_bounds{22, top + 14, 70, top + 62};
        FillRect(device, &icon_bounds, icon);
        DeleteObject(icon);

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
    switch (message) {
    case WM_CREATE:
        return create_taken_buttons(window) ? 0 : -1;
    case WM_COMMAND: {
        const int index = LOWORD(w_param) - first_taken_button_id;
        if (HIWORD(w_param) == BN_CLICKED && index >= 0 && static_cast<std::size_t>(index) < medications.size()) {
            Medication& medication = medications[static_cast<std::size_t>(index)];
            const auto previous = medication.last_taken_at;
            medication.mark_taken(std::chrono::system_clock::now());
            try {
                save_medications(medications_path, medications);
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
                    save_medications(medications_path, medications);
                    enable_startup_if_needed(window);
                } catch (const std::exception&) {
                    medications[*index] = previous;
                    MessageBox(
                        window, L"The medication changes could not be saved.", L"Medication Cooldown Widget",
                        MB_OK | MB_ICONERROR);
                }
                schedule_refresh(window);
                InvalidateRect(window, nullptr, FALSE);
            }
        } else if (command == add_medication_command) {
            Medication added{.interval = std::chrono::hours{24}};
            if (edit_medication(window, added)) {
                added.id = new_medication_id();
                medications.push_back(std::move(added));
                try {
                    save_medications(medications_path, medications);
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
                save_medications(medications_path, medications);
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
                    save_medications(medications_path, medications);
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
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProc(window, message, w_param, l_param);
}
}

int WINAPI wWinMain(const HINSTANCE instance, HINSTANCE, PWSTR, const int show_command) {
    bool configuration_missing{};
    try {
        medications_path = local_medications_path();
        configuration_missing = !std::filesystem::exists(medications_path);
        medications = load_medications(medications_path);
        enable_startup_after_save = configuration_missing || medications.empty();
    } catch (const std::exception&) {
        MessageBox(nullptr, L"Saved medications could not be loaded.", L"Medication Cooldown Widget", MB_OK | MB_ICONERROR);
        return 1;
    }
    if (medications.empty() && configuration_missing) medications.push_back(example_medication());

    const WNDCLASS window_definition{
        .lpfnWndProc = window_procedure,
        .hInstance = instance,
        .hCursor = LoadCursor(nullptr, IDC_ARROW),
        .lpszClassName = window_class,
    };
    if (!RegisterClass(&window_definition)) return 1;

    const HWND window = CreateWindowEx(
        WS_EX_TOOLWINDOW, window_class, L"Medication Cooldown Widget", WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, 420,
        widget_height(), nullptr, nullptr, instance, nullptr);
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
