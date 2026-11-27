// Turbulence's standalone Windows launcher. The game module is built locally.
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
enum ControlId { Select = 100, Play, Log, Help };
constexpr UINT_PTR PollTimer = 1;

fs::path package, library, module, selected;
HWND window, status, select_button, play_button, log_button;
HFONT title_font, body_font;
HANDLE child, library_lock;
bool building;
int dpi;

int Scale(int pixels)
{
  return MulDiv(pixels, dpi, 96);
}

std::wstring Quote(const std::wstring& argument)
{
  // Match CommandLineToArgvW rules, including backslashes before a quote.
  std::wstring result = L"\"";
  size_t backslashes = 0;
  for (const wchar_t character : argument)
  {
    if (character == L'\\')
    {
      ++backslashes;
      continue;
    }
    result.append(character == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
    result += character;
    backslashes = 0;
  }
  result.append(backslashes * 2, L'\\');
  return result + L'"';
}

bool Installed()
{
  std::error_code error;
  return fs::is_regular_file(library / L"game/sys/main.dol", error);
}

bool Ready()
{
  std::error_code error;
  return Installed() && fs::is_regular_file(module, error);
}

void Refresh(const wchar_t* message = nullptr)
{
  const bool ready = Ready();
  const bool imported = Installed();
  const bool can_retry = imported || !selected.empty();
  SetWindowTextW(status, message ? message : ready ? L"Sonic Riders is ready to play."
      : imported ? L"Build the game once for this launcher version."
                 : L"Choose or drop your Sonic Riders USA revision 0 disc image.");
  EnableWindow(select_button, !child && !imported);
  EnableWindow(play_button, !child && can_retry);
  SetWindowTextW(play_button, ready ? L"Play" : L"Build Game");
  std::error_code error;
  EnableWindow(log_button, fs::is_regular_file(library / L"launcher.log", error));
}

HWND AddControl(const wchar_t* type, const wchar_t* label, DWORD style,
                int x, int y, int width, int height, int id = 0)
{
  HWND control = CreateWindowExW(0, type, label, WS_VISIBLE | WS_CHILD | style, Scale(x),
      Scale(y), Scale(width), Scale(height), window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
      GetModuleHandleW(nullptr), nullptr);
  SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(body_font), TRUE);
  return control;
}

fs::path FindPython(bool& uses_launcher)
{
  std::array<wchar_t, 32768> result{};
  const std::array candidates{L"py.exe", L"python.exe"};
  for (const auto* name : candidates)
  {
    const DWORD length = SearchPathW(nullptr, name, nullptr, static_cast<DWORD>(result.size()),
                                    result.data(), nullptr);
    if (length && length < result.size())
    {
      uses_launcher = name == candidates.front();
      return result.data();
    }
  }
  return {};
}

DWORD Launch(const fs::path& executable, const std::vector<std::wstring>& arguments)
{
  std::wstring command = Quote(executable.native());
  for (const auto& argument : arguments)
    command += L" " + Quote(argument);

  SECURITY_ATTRIBUTES inheritable{sizeof(inheritable), nullptr, TRUE};
  const auto log = library / L"launcher.log";
  HANDLE output = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              &inheritable, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (output == INVALID_HANDLE_VALUE)
    return GetLastError();
  HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             &inheritable, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (input == INVALID_HANDLE_VALUE)
  {
    const DWORD error = GetLastError();
    CloseHandle(output);
    return error;
  }
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = input;
  startup.hStdOutput = startup.hStdError = output;
  PROCESS_INFORMATION process{};
  const BOOL started = CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
      CREATE_NO_WINDOW, nullptr, package.c_str(), &startup, &process);
  const DWORD error = started ? ERROR_SUCCESS : GetLastError();
  CloseHandle(input);
  CloseHandle(output);
  if (started)
  {
    CloseHandle(process.hThread);
    child = process.hProcess;
    SetTimer(window, PollTimer, 500, nullptr);
  }
  return error;
}

void Start()
{
  if (child)
    return;
  building = !Ready();
  DWORD error;
  if (building)
  {
    if (!Installed() && selected.empty())
      return;
    bool uses_launcher = false;
    const fs::path python = FindPython(uses_launcher);
    if (python.empty())
    {
      Refresh(L"Install Python 3 and enable its launcher or Add Python to PATH, then try again.");
      return;
    }
    std::vector<std::wstring> args;
    if (uses_launcher)
      args.emplace_back(L"-3");
    args.insert(args.end(), {L"-X", L"utf8", L"-u", (package / L"setup-game.py").native(), library.native()});
    if (!Installed())
      args.push_back(selected.native());
    error = Launch(python, args);
  }
  else
  {
    error = Launch(package / L"moderngekko-run.exe",
                   {L"--game", (library / L"game").native(), L"--module", module.native(),
                    L"--user-dir", (library / L"user").native()});
  }
  if (error)
  {
    const auto message = L"Could not start " + std::wstring(building ? L"setup" : L"the game") +
                         L" (Windows error " + std::to_wstring(error) + L").";
    Refresh(message.c_str());
    return;
  }
  Refresh(building ? L"Importing and building the game. This can take several minutes."
                   : L"Game running.");
}

void Accept(fs::path file)
{
  if (child || Installed())
    return;
  const std::wstring extension = file.extension().native();
  if (_wcsicmp(extension.c_str(), L".iso") && _wcsicmp(extension.c_str(), L".gcm") &&
      _wcsicmp(extension.c_str(), L".ciso"))
  {
    Refresh(L"Choose an ISO, GCM or GameCube CISO image.");
    return;
  }
  selected = std::move(file);
  Start();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  switch (message)
  {
  case WM_COMMAND:
    if (LOWORD(wparam) == Select && !child)
    {
      std::array<wchar_t, 32768> path{};
      OPENFILENAMEW picker{};
      picker.lStructSize = sizeof(picker);
      picker.hwndOwner = hwnd;
      picker.lpstrFilter = L"GameCube disc images\0*.iso;*.gcm;*.ciso\0\0";
      picker.lpstrFile = path.data();
      picker.nMaxFile = static_cast<DWORD>(path.size());
      picker.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
      if (GetOpenFileNameW(&picker))
        Accept(path.data());
    }
    else if (LOWORD(wparam) == Play)
      Start();
    else if (LOWORD(wparam) == Log)
    {
      const auto log = Quote((library / L"launcher.log").native());
      ShellExecuteW(hwnd, L"open", L"notepad.exe", log.c_str(), nullptr, SW_SHOWNORMAL);
    }
    else if (LOWORD(wparam) == Help)
      ShellExecuteW(hwnd, L"open", (package / L"README.txt").c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return 0;
  case WM_DROPFILES:
  {
    const auto drop = reinterpret_cast<HDROP>(wparam);
    const UINT length = DragQueryFileW(drop, 0, nullptr, 0);
    if (length)
    {
      std::wstring path(length + 1, L'\0');
      DragQueryFileW(drop, 0, path.data(), length + 1);
      path.resize(length);
      Accept(path);
    }
    DragFinish(drop);
    return 0;
  }
  case WM_TIMER:
    if (child && wparam == PollTimer && WaitForSingleObject(child, 0) == WAIT_OBJECT_0)
    {
      DWORD code = 1;
      GetExitCodeProcess(child, &code);
      CloseHandle(child);
      child = nullptr;
      KillTimer(hwnd, PollTimer);
      if (code || (building && !Ready()))
        Refresh(building ? L"Setup failed. Open the log for details and try again."
                         : L"The game closed with an error. Open the log for details.");
      else
        Refresh();
    }
    return 0;
  case WM_CLOSE:
    if (child)
    {
      MessageBoxW(hwnd, building ? L"Wait for setup to finish before closing the launcher."
                                : L"Close the game before closing the launcher.",
                  L"Turbulence", MB_OK | MB_ICONINFORMATION);
      return 0;
    }
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
  try
  {
    std::array<wchar_t, 32768> exe{};
    const DWORD length = GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size()));
    if (!length || length >= exe.size())
      throw std::runtime_error("Cannot find the launcher folder.");
    package = fs::path(exe.data()).parent_path();
    std::ifstream identity(package / L"build-id.txt");
    std::string id;
    identity >> id;
    if (id.size() != 64 || id.find_first_not_of("0123456789abcdef") != std::string::npos)
      throw std::runtime_error("Extract all files from the Turbulence ZIP before launching.");

    PWSTR app_data = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &app_data)))
      throw std::runtime_error("Cannot open the local application data folder.");
    library = fs::path(app_data) / L"Turbulence";
    CoTaskMemFree(app_data);
    fs::create_directories(library);
    module = library / L"modules" / id / L"gGXEE8P_recomp.dll";
    library_lock = CreateFileW((library / L"launcher.lock").c_str(), GENERIC_WRITE, 0, nullptr,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (library_lock == INVALID_HANDLE_VALUE)
      throw std::runtime_error("The Turbulence launcher is already open or its library is inaccessible.");

    SetProcessDPIAware();
    dpi = GetDpiForSystem();
    const wchar_t* class_name = L"TurbulenceLauncher";
    WNDCLASSW type{};
    type.lpfnWndProc = WindowProc;
    type.lpszClassName = class_name;
    type.hInstance = instance;
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    RegisterClassW(&type);
    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT frame{0, 0, Scale(510), Scale(208)};
    AdjustWindowRectEx(&frame, style, FALSE, 0);
    window = CreateWindowExW(0, class_name, L"Turbulence", style, CW_USEDEFAULT, CW_USEDEFAULT,
        frame.right - frame.left, frame.bottom - frame.top, nullptr, nullptr, instance, nullptr);
    if (!window)
      throw std::runtime_error("Cannot create the launcher window.");

    title_font = CreateFontW(-Scale(28), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    body_font = CreateFontW(-Scale(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HWND title = AddControl(L"STATIC", L"Turbulence", 0, 24, 22, 460, 38);
    SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font), TRUE);
    status = AddControl(L"STATIC", L"", 0, 24, 72, 460, 53);
    select_button = AddControl(L"BUTTON", L"Choose Disc…", WS_TABSTOP, 24, 143, 126, 34, Select);
    play_button = AddControl(L"BUTTON", L"Play", WS_TABSTOP, 160, 143, 96, 34, Play);
    log_button = AddControl(L"BUTTON", L"Open Log", WS_TABSTOP, 288, 143, 96, 34, Log);
    AddControl(L"BUTTON", L"Setup Help", WS_TABSTOP, 394, 143, 92, 34, Help);

    DragAcceptFiles(window, TRUE);
    Refresh();
    ShowWindow(window, show);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
      if (!IsDialogMessageW(window, &message))
      {
        TranslateMessage(&message);
        DispatchMessageW(&message);
      }
    }
    DeleteObject(body_font);
    DeleteObject(title_font);
    CloseHandle(library_lock);
    return 0;
  }
  catch (const std::exception& error)
  {
    MessageBoxA(nullptr, error.what(), "Turbulence", MB_OK | MB_ICONERROR);
    return 1;
  }
}
