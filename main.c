// ==============================================================================
// AION 2 BOT - FLUXO AUTÔNOMO DE 4 FASES & HUB DE CONFIGURAÇÃO EM TEMPO REAL
// Repositório Oficial: https://github.com/IuryAlmeidaDev/aion2-bot
// Arquitetura Nativa Win32 C99 Puro - Obsidian HUD + Anti-Stuck Unreal Engine
// ==============================================================================
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifndef WS_EX_LAYERED
#define WS_EX_LAYERED 0x00080000
#endif

#ifndef WS_EX_NOACTIVATE
#define WS_EX_NOACTIVATE 0x08000000L
#endif

#ifndef LWA_ALPHA
#define LWA_ALPHA 0x00000002
#endif

#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#endif

#ifndef KEYEVENTF_SCANCODE
#define KEYEVENTF_SCANCODE 0x0008
#endif

#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC 0
#endif

#ifndef SPI_SETFOREGROUNDLOCKTIMEOUT
#define SPI_SETFOREGROUNDLOCKTIMEOUT 0x2001
#endif

#ifndef ASFW_ANY
#define ASFW_ANY ((DWORD)-1)
#endif

#define WM_APP_UNSTICK (WM_APP + 42)

typedef BOOL (WINAPI *pfn_SetLayeredWindowAttributes)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
static pfn_SetLayeredWindowAttributes fn_SetLayeredWindowAttributes = NULL;

static WCHAR g_iniPath[MAX_PATH] = {0};
static char  g_logPath[MAX_PATH] = {0};

static void init_paths(void) {
    GetModuleFileNameW(NULL, g_iniPath, MAX_PATH);
    WCHAR *lastSlashW = wcsrchr(g_iniPath, L'\\');
    if (lastSlashW) {
        *(lastSlashW + 1) = L'\0';
        wcscat(g_iniPath, L"config.ini");
    } else {
        wcscpy(g_iniPath, L"config.ini");
    }

    GetModuleFileNameA(NULL, g_logPath, MAX_PATH);
    char *lastSlashA = strrchr(g_logPath, '\\');
    if (lastSlashA) {
        *(lastSlashA + 1) = '\0';
        strcat(g_logPath, "debug.log");
    } else {
        strcpy(g_logPath, "debug.log");
    }
}

static void log_debug(const char *msg) {
    if (!g_logPath[0]) init_paths();
    FILE *f = fopen(g_logPath, "a");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(f, "[%02d:%02d:%02d.%03d] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, msg);
        fclose(f);
    }
}

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS *pExceptionInfo) {
    char buf[128];
    snprintf(buf, sizeof(buf), "CRASH! Code: 0x%08X at %p",
             (unsigned int)pExceptionInfo->ExceptionRecord->ExceptionCode,
             pExceptionInfo->ExceptionRecord->ExceptionAddress);
    log_debug(buf);
    return EXCEPTION_EXECUTE_HANDLER;
}

// ==============================================================================
// CORES OBSIDIAN HUD (Paleta GhostHUD)
// ==============================================================================
#define COLOR_BG          RGB(15, 17, 21)      // #0F1115
#define COLOR_BORDER      RGB(42, 50, 64)      // #2A3240
#define COLOR_SLOT_BG     RGB(22, 27, 36)      // #161B24
#define COLOR_BTN_BG      RGB(28, 36, 48)      // #1C2430
#define COLOR_TEXT_DIM    RGB(124, 138, 156)   // #7C8A9C
#define COLOR_TEXT_WHITE  RGB(220, 226, 236)   // #DCE2EC
#define COLOR_CYAN        RGB(0, 240, 255)     // #00F0FF
#define COLOR_GREEN       RGB(46, 213, 115)    // #2ED573
#define COLOR_RED         RGB(255, 71, 87)     // #FF4757
#define COLOR_AMBER       RGB(255, 190, 118)   // #FFBE76
#define COLOR_ORANGE      RGB(255, 165, 2)     // #FFA502
#define COLOR_BADGE_BG    RGB(19, 29, 46)      // #131D2E
#define COLOR_BADGE_BD    RGB(31, 51, 82)      // #1F3352
#define COLOR_ACTIVE_BG   RGB(14, 37, 25)      // #0E2519
#define COLOR_COMBAT_BG   RGB(26, 21, 46)      // #1A152E
#define COLOR_SEARCH_BG   RGB(38, 32, 19)      // #262013

#define HUD_WIDTH            760
#define HUD_BAR_HEIGHT       42
#define HUD_EXPANDED_HEIGHT  176

// ==============================================================================
// CONFIGURAÇÕES EM TEMPO REAL & PERSISTÊNCIA CONFIG.INI
// ==============================================================================
static volatile int  g_rmb_hold_ms       = 6000;
static volatile int  g_wait_after_rmb_ms = 2000;
static volatile int  g_lmb_hold_ms       = 4000;
static volatile int  g_wait_after_lmb_ms = 3000;
static volatile int  g_cd_heal           = 3500;
static volatile int  g_lootPressCount    = 2;
static volatile WORD g_keyHeal           = 'Q';
static volatile WORD g_keyLoot           = 'F';
static int           g_iniPosX           = -1;
static int           g_iniPosY           = -1;

typedef enum {
    PHASE_RMB = 0,       // Fase 1: Segurando Botão Direito (Macro)
    PHASE_WAIT_RMB = 1,  // Fase 2: Pausa após RMB
    PHASE_LMB = 2,       // Fase 3: Segurando Botão Esquerdo (Auto-Ataque)
    PHASE_WAIT_LMB = 3   // Fase 4: Pausa final / Coleta de Loot
} CombatPhase;

static volatile CombatPhase g_currentPhase     = PHASE_RMB;
static volatile DWORD       g_phaseRemainingMs = 6000;

// ==============================================================================
// ESTADO GLOBAL, PAINEL DE CONFIGURAÇÃO & TELEMETRIA
// ==============================================================================
static HWND  g_hWnd        = NULL;
static HWND  g_hAionWnd    = NULL;
static HFONT g_hFontBold   = NULL;
static HFONT g_hFontSmall  = NULL;

static volatile BOOL g_running       = TRUE;
static volatile BOOL g_botActive     = FALSE;
static volatile BOOL g_autoHeal      = TRUE;
static volatile BOOL g_autoLoot      = TRUE;
static volatile BOOL g_windowGuard   = TRUE;
static volatile BOOL g_isForeground  = TRUE;
static volatile BOOL g_hotkeyF8Ok    = FALSE;
static volatile BOOL g_configOpen    = FALSE;
static volatile BOOL g_unsticking    = FALSE;
static volatile int  g_bindingTarget = 0; // 0 = nenhum, 1 = aguardando tecla Cura, 2 = aguardando tecla Loot
static BOOL          g_shiftedUp     = FALSE;

static volatile int  g_mobsKilled    = 0;
static DWORD         g_sessionStartTime = 0;

// Declarações prévias
static HWND find_aion_window(void);
static void focus_aion_window(void);
static void force_stop_aion_action(void);
static void save_config(void);

static void write_ini_int(const WCHAR *section, const WCHAR *key, int val) {
    WCHAR buf[32];
    wsprintfW(buf, L"%d", val);
    WritePrivateProfileStringW(section, key, buf, g_iniPath);
}

static void write_ini_key(const WCHAR *section, const WCHAR *key, WORD vk) {
    WCHAR buf[8] = { (WCHAR)vk, L'\0' };
    WritePrivateProfileStringW(section, key, buf, g_iniPath);
}

static void save_config(void) {
    if (!g_iniPath[0]) init_paths();
    write_ini_int(L"Fluxo",   L"RMB_HoldMs",      g_rmb_hold_ms);
    write_ini_int(L"Fluxo",   L"WaitAfterRMB_Ms", g_wait_after_rmb_ms);
    write_ini_int(L"Fluxo",   L"LMB_HoldMs",      g_lmb_hold_ms);
    write_ini_int(L"Fluxo",   L"WaitAfterLMB_Ms", g_wait_after_lmb_ms);
    write_ini_int(L"Combate", L"AutoHeal",        g_autoHeal ? 1 : 0);
    write_ini_int(L"Combate", L"Heal_CooldownMs", g_cd_heal);
    write_ini_int(L"Combate", L"AutoLoot",        g_autoLoot ? 1 : 0);
    write_ini_int(L"Combate", L"LootPressCount",  g_lootPressCount);
    write_ini_key(L"Teclas",  L"KeyHeal",         g_keyHeal);
    write_ini_key(L"Teclas",  L"KeyLoot",         g_keyLoot);
    write_ini_int(L"Overlay", L"WindowGuard",     g_windowGuard ? 1 : 0);
}

static void load_or_create_config(void) {
    init_paths();
    DWORD attr = GetFileAttributesW(g_iniPath);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        FILE *f = _wfopen(g_iniPath, L"w");
        if (f) {
            fwprintf(f,
                L"; ==============================================================================\n"
                L"; AION 2 BOT - CONFIG.INI (TAMBÉM CONFIGURÁVEL EM TEMPO REAL NO HUD [CFG])\n"
                L"; ==============================================================================\n\n"
                L"[Fluxo]\n"
                L"RMB_HoldMs=6000\n"
                L"WaitAfterRMB_Ms=2000\n"
                L"LMB_HoldMs=4000\n"
                L"WaitAfterLMB_Ms=3000\n\n"
                L"[Combate]\n"
                L"AutoHeal=1\n"
                L"Heal_CooldownMs=3500\n"
                L"AutoLoot=1\n"
                L"LootPressCount=2\n\n"
                L"[Teclas]\n"
                L"KeyHeal=Q\n"
                L"KeyLoot=F\n\n"
                L"[Overlay]\n"
                L"WindowGuard=1\n"
                L"PosX=-1\n"
                L"PosY=-1\n"
            );
            fclose(f);
            log_debug("config.ini criado com valores padrao");
        }
    }

    g_rmb_hold_ms       = GetPrivateProfileIntW(L"Fluxo", L"RMB_HoldMs", 6000, g_iniPath);
    g_wait_after_rmb_ms = GetPrivateProfileIntW(L"Fluxo", L"WaitAfterRMB_Ms", 2000, g_iniPath);
    g_lmb_hold_ms       = GetPrivateProfileIntW(L"Fluxo", L"LMB_HoldMs", 4000, g_iniPath);
    g_wait_after_lmb_ms = GetPrivateProfileIntW(L"Fluxo", L"WaitAfterLMB_Ms", 3000, g_iniPath);
    g_autoHeal          = GetPrivateProfileIntW(L"Combate", L"AutoHeal", 1, g_iniPath);
    g_cd_heal           = GetPrivateProfileIntW(L"Combate", L"Heal_CooldownMs", 3500, g_iniPath);
    g_autoLoot          = GetPrivateProfileIntW(L"Combate", L"AutoLoot", 1, g_iniPath);
    g_lootPressCount    = GetPrivateProfileIntW(L"Combate", L"LootPressCount", 2, g_iniPath);
    g_windowGuard       = GetPrivateProfileIntW(L"Overlay", L"WindowGuard", 1, g_iniPath);
    g_iniPosX           = GetPrivateProfileIntW(L"Overlay", L"PosX", -1, g_iniPath);
    g_iniPosY           = GetPrivateProfileIntW(L"Overlay", L"PosY", -1, g_iniPath);

    if (g_rmb_hold_ms < 500) g_rmb_hold_ms = 6000;
    if (g_wait_after_rmb_ms < 200) g_wait_after_rmb_ms = 2000;
    if (g_lmb_hold_ms < 500) g_lmb_hold_ms = 4000;
    if (g_wait_after_lmb_ms < 200) g_wait_after_lmb_ms = 3000;
    if (g_cd_heal < 500) g_cd_heal = 3500;
    if (g_lootPressCount < 1) g_lootPressCount = 1;

    WCHAR buf[16] = {0};
    GetPrivateProfileStringW(L"Teclas", L"KeyHeal", L"Q", buf, 16, g_iniPath);
    if (buf[0]) g_keyHeal = (WORD)towupper(buf[0]);

    GetPrivateProfileStringW(L"Teclas", L"KeyLoot", L"F", buf, 16, g_iniPath);
    if (buf[0]) g_keyLoot = (WORD)towupper(buf[0]);

    g_phaseRemainingMs = (DWORD)g_rmb_hold_ms;
}

// ==============================================================================
// DESCOBERTA E FOCO DA JANELA VISÍVEL DO AION 2 (UNREALWINDOW)
// ==============================================================================
static BOOL CALLBACK enum_aion_window_cb(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd) || hwnd == g_hWnd) return TRUE;

    WCHAR clsW[128] = {0};
    if (GetClassNameW(hwnd, clsW, 128) > 0) {
        if (wcsstr(clsW, L"UnrealWindow") != NULL) {
            *(HWND*)lParam = hwnd;
            return FALSE;
        }
    }

    WCHAR titleW[256] = {0};
    if (GetWindowTextW(hwnd, titleW, 256) > 0) {
        if (wcsstr(titleW, L"AION") != NULL || wcsstr(titleW, L"Aion") != NULL) {
            *(HWND*)lParam = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

static HWND find_aion_window(void) {
    if (g_hAionWnd && IsWindow(g_hAionWnd) && IsWindowVisible(g_hAionWnd) && g_hAionWnd != g_hWnd) {
        return g_hAionWnd;
    }
    HWND hFound = NULL;
    EnumWindows(enum_aion_window_cb, (LPARAM)&hFound);
    if (hFound) {
        g_hAionWnd = hFound;
        return hFound;
    }
    HWND hUnreal = FindWindowW(L"UnrealWindow", NULL);
    if (hUnreal && IsWindow(hUnreal)) {
        g_hAionWnd = hUnreal;
        return hUnreal;
    }
    return NULL;
}

static void focus_aion_window(void) {
    HWND hAion = find_aion_window();
    if (hAion && GetForegroundWindow() != hAion) {
        DWORD aionTid = GetWindowThreadProcessId(hAion, NULL);
        DWORD myTid   = GetCurrentThreadId();
        BOOL attached = FALSE;
        if (aionTid && aionTid != myTid) {
            attached = AttachThreadInput(myTid, aionTid, TRUE);
        }
        AllowSetForegroundWindow(ASFW_ANY);
        BringWindowToTop(hAion);
        SetForegroundWindow(hAion);
        SetActiveWindow(hAion);
        SetFocus(hAion);
        if (attached) {
            AttachThreadInput(myTid, aionTid, FALSE);
        }
    }
}

static BOOL is_aion2_foreground(void) {
    if (g_unsticking) return TRUE;

    HWND fg = GetForegroundWindow();
    if (!fg) return FALSE;

    if (fg == g_hWnd) {
        if (!g_configOpen) {
            focus_aion_window();
        }
        return TRUE;
    }

    if (!g_windowGuard) {
        find_aion_window();
        return TRUE;
    }

    WCHAR clsW[128] = {0};
    if (GetClassNameW(fg, clsW, 128) > 0) {
        if (wcsstr(clsW, L"UnrealWindow") != NULL) {
            g_hAionWnd = fg;
            return TRUE;
        }
    }

    WCHAR titleW[512] = {0};
    if (GetWindowTextW(fg, titleW, 512) > 0) {
        for (int i = 0; titleW[i] != L'\0'; i++) {
            WCHAR c = titleW[i];
            if ((c == L'a' || c == L'A') &&
                (titleW[i+1] == L'i' || titleW[i+1] == L'I') &&
                (titleW[i+2] == L'o' || titleW[i+2] == L'O') &&
                (titleW[i+3] == L'n' || titleW[i+3] == L'N')) {
                g_hAionWnd = fg;
                return TRUE;
            }
        }
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (pid != 0) {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProc) {
            typedef BOOL (WINAPI *pfn_QueryFullProcessImageNameW)(HANDLE, DWORD, LPWSTR, PDWORD);
            static pfn_QueryFullProcessImageNameW fnQueryW = NULL;
            static BOOL resolvedW = FALSE;
            if (!resolvedW) {
                HMODULE hKernel = GetModuleHandleA("kernel32.dll");
                if (hKernel) fnQueryW = (pfn_QueryFullProcessImageNameW)GetProcAddress(hKernel, "QueryFullProcessImageNameW");
                resolvedW = TRUE;
            }
            if (fnQueryW) {
                WCHAR pathW[1024] = {0};
                DWORD sz = 1024;
                if (fnQueryW(hProc, 0, pathW, &sz)) {
                    for (int i = 0; pathW[i] != L'\0'; i++) {
                        WCHAR c = pathW[i];
                        if ((c == L'a' || c == L'A') &&
                            (pathW[i+1] == L'i' || pathW[i+1] == L'I') &&
                            (pathW[i+2] == L'o' || pathW[i+2] == L'O') &&
                            (pathW[i+3] == L'n' || pathW[i+3] == L'N')) {
                            CloseHandle(hProc);
                            g_hAionWnd = fg;
                            return TRUE;
                        }
                    }
                }
            }
            CloseHandle(hProc);
        }
    }

    return FALSE;
}

// ==============================================================================
// SIMULAÇÃO DE INPUTS NATIVOS + ANTI-STUCK UNREAL ENGINE 5 (FLUSHPRESSEDKEYS)
// ==============================================================================
static void move_cursor_to_game_center(BOOL force_center) {
    if (g_configOpen) return;

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int targetX = sw / 2;
    int targetY = (sh / 2) - 85; // Acima do personagem, longe da barra de skills, loot e HUD

    HWND hAion = find_aion_window();
    if (hAion && IsWindow(hAion)) {
        RECT rcAion;
        if (GetWindowRect(hAion, &rcAion) && (rcAion.right > rcAion.left + 200)) {
            targetX = rcAion.left + (rcAion.right - rcAion.left) / 2;
            targetY = rcAion.top + (rcAion.bottom - rcAion.top) / 2 - 85;
        }
    }

    if (force_center) {
        SetCursorPos(targetX, targetY);
        return;
    }

    POINT pt;
    if (GetCursorPos(&pt)) {
        // Se o cursor estiver sobre o HUD ou na faixa inferior das barras de skill do jogo
        if (g_hWnd && IsWindow(g_hWnd)) {
            RECT rcHud;
            if (GetWindowRect(g_hWnd, &rcHud)) {
                if (pt.x >= rcHud.left - 12 && pt.x <= rcHud.right + 12 &&
                    pt.y >= rcHud.top - 12 && pt.y <= rcHud.bottom + 12) {
                    SetCursorPos(targetX, targetY);
                    return;
                }
            }
        }
        if (pt.y >= sh - 170) {
            SetCursorPos(targetX, targetY);
        }
    }
}

static LPARAM get_aion_center_lparam(HWND hAion) {
    if (hAion && IsWindow(hAion)) {
        RECT rc;
        if (GetClientRect(hAion, &rc) && rc.right > 0 && rc.bottom > 0) {
            int cx = (rc.right - rc.left) / 2;
            int cy = (rc.bottom - rc.top) / 2 - 85;
            if (cy < 50) cy = (rc.bottom - rc.top) / 2;
            return MAKELPARAM(cx, cy);
        }
    }
    return MAKELPARAM(960, 455);
}

static void set_mouse_left(BOOL down) {
    if (down) move_cursor_to_game_center(FALSE);

    INPUT inp;
    ZeroMemory(&inp, sizeof(INPUT));
    inp.type = INPUT_MOUSE;
    inp.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    SendInput(1, &inp, sizeof(INPUT));

    HWND hAion = find_aion_window();
    if (hAion && IsWindow(hAion)) {
        LPARAM lp = get_aion_center_lparam(hAion);
        PostMessageW(hAion, down ? WM_LBUTTONDOWN : WM_LBUTTONUP, down ? MK_LBUTTON : 0, lp);
    }
}

static void set_mouse_right(BOOL down) {
    if (down) move_cursor_to_game_center(FALSE);

    INPUT inp;
    ZeroMemory(&inp, sizeof(INPUT));
    inp.type = INPUT_MOUSE;
    inp.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    SendInput(1, &inp, sizeof(INPUT));

    HWND hAion = find_aion_window();
    if (hAion && IsWindow(hAion)) {
        LPARAM lp = get_aion_center_lparam(hAion);
        PostMessageW(hAion, down ? WM_RBUTTONDOWN : WM_RBUTTONUP, down ? MK_RBUTTON : 0, lp);
    }
}

static void raw_mouse_up_all(void) {
    INPUT inps[2];
    ZeroMemory(inps, sizeof(inps));
    inps[0].type = INPUT_MOUSE;
    inps[0].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    inps[1].type = INPUT_MOUSE;
    inps[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    SendInput(2, inps, sizeof(INPUT));

    HWND hAion = find_aion_window();
    if (hAion && IsWindow(hAion)) {
        LPARAM lp = get_aion_center_lparam(hAion);
        PostMessageW(hAion, WM_LBUTTONUP, 0, lp);
        PostMessageW(hAion, WM_RBUTTONUP, 0, lp);
    }
}

// Pulso rápido de ALT (VK_MENU): no Aion 2 Action Mode, o Alt alterna brevemente o cursor,
// quebrando a trava de botão segurado da câmera de ação e liberando SetForegroundWindow no Windows
static void tap_alt_key_briefly(void) {
    UINT scan = MapVirtualKeyW(VK_MENU, MAPVK_VK_TO_VSC);
    INPUT inp;
    ZeroMemory(&inp, sizeof(INPUT));
    inp.type = INPUT_KEYBOARD;
    inp.ki.wVk = VK_MENU;
    inp.ki.wScan = (WORD)scan;
    inp.ki.dwFlags = KEYEVENTF_SCANCODE;
    SendInput(1, &inp, sizeof(INPUT));

    Sleep(25);

    inp.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    SendInput(1, &inp, sizeof(INPUT));
}

// Destrava completamente o estado de botão pressionado / macro da Unreal Engine (AION2.exe)
// reproduzindo o efeito exato de limpeza de buffer do Alt-Tab (FlushPressedKeys) em 75ms
static void force_stop_aion_action(void) {
    g_unsticking = TRUE;

    // 1. Solta imediatamente LMB e RMB via SendInput + PostMessage
    raw_mouse_up_all();

    HWND hAion = find_aion_window();
    if (hAion && IsWindow(hAion)) {
        LPARAM lp = get_aion_center_lparam(hAion);
        PostMessageW(hAion, WM_RBUTTONUP, 0, lp);
        PostMessageW(hAion, WM_LBUTTONUP, 0, lp);
        PostMessageW(hAion, WM_CANCELMODE, 0, 0);
        PostMessageW(hAion, WM_CAPTURECHANGED, 0, 0);
    }

    // 2. Pulso rápido de ALT para quebrar trava do Action Mode
    tap_alt_key_briefly();

    // 3. Executa o Micro-Focus Reset na thread do HUD (simula o Alt-Tab por 75ms sem minimizar o jogo)
    if (g_hWnd && IsWindow(g_hWnd)) {
        DWORD_PTR res = 0;
        SendMessageTimeoutW(g_hWnd, WM_APP_UNSTICK, 0, 0, SMTO_NORMAL, 400, &res);
    }

    // 4. Reforço final de soltura após o retorno do foco
    raw_mouse_up_all();
    Sleep(15);
    raw_mouse_up_all();

    g_unsticking = FALSE;
}

// Re-captura o viewport 3D do Aion 2 após um reset de foco (evita que o 1º clique seja consumido só pelo foco)
static void wake_aion_viewport_for_attack(BOOL right_button) {
    focus_aion_window();
    move_cursor_to_game_center(TRUE);
    Sleep(15);

    // Clique rápido de 25ms no centro 3D do mundo para travar a mira Action Mode
    if (right_button) {
        set_mouse_right(TRUE);
        Sleep(25);
        set_mouse_right(FALSE);
    } else {
        set_mouse_left(TRUE);
        Sleep(25);
        set_mouse_left(FALSE);
    }
    Sleep(25);
}

static void send_game_key_ex(HWND hwnd, WORD vk, int hold_ms) {
    UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    if (hold_ms <= 0) hold_ms = 60;

    LPARAM lpDown = 1 | ((scan & 0xFF) << 16);
    LPARAM lpUp   = 1 | ((scan & 0xFF) << 16) | (1 << 30) | (1 << 31);

    if (hwnd && IsWindow(hwnd)) {
        PostMessageW(hwnd, WM_KEYDOWN, vk, lpDown);
    }

    INPUT inp;
    ZeroMemory(&inp, sizeof(INPUT));
    inp.type = INPUT_KEYBOARD;
    inp.ki.wVk = vk;
    inp.ki.wScan = (WORD)scan;
    inp.ki.dwFlags = KEYEVENTF_SCANCODE;
    SendInput(1, &inp, sizeof(INPUT));

    Sleep(hold_ms);

    if (hwnd && IsWindow(hwnd)) {
        PostMessageW(hwnd, WM_KEYUP, vk, lpUp);
    }
    inp.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    SendInput(1, &inp, sizeof(INPUT));
}

// ==============================================================================
// TOGGLES & PAINEL DE CONFIGURAÇÃO EM TEMPO REAL
// ==============================================================================
static void toggle_bot(void) {
    g_botActive = !g_botActive;
    if (g_botActive) {
        g_currentPhase = PHASE_RMB;
        g_phaseRemainingMs = (DWORD)g_rmb_hold_ms;
        log_debug("toggle_bot: ATIVADO [F8]");
        focus_aion_window();
        move_cursor_to_game_center(TRUE);
        Beep(1200, 90);
    } else {
        g_currentPhase = PHASE_RMB;
        g_phaseRemainingMs = (DWORD)g_rmb_hold_ms;
        force_stop_aion_action();
        log_debug("toggle_bot: PAUSADO [F8] - Acao e foco destravados imediatamente");
        Beep(600, 130);
    }
    if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
}

static void toggle_heal(void) {
    g_autoHeal = !g_autoHeal;
    save_config();
    log_debug("toggle_heal: ALTERNADO [F6]");
    focus_aion_window();
    Beep(900, 75);
    if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
}

static void toggle_loot(void) {
    g_autoLoot = !g_autoLoot;
    save_config();
    if (g_autoLoot) {
        log_debug("toggle_loot: ATIVADO [F7]");
        Beep(1100, 75);
    } else {
        log_debug("toggle_loot: PAUSADO [F7]");
        Beep(550, 95);
    }
    focus_aion_window();
    if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
}

static void toggle_guard(void) {
    g_windowGuard = !g_windowGuard;
    save_config();
    log_debug("toggle_guard: ALTERNADO [F9]");
    focus_aion_window();
    Beep(1000, 75);
    if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
}

static void toggle_config_panel(void) {
    if (!g_hWnd) return;
    g_configOpen = !g_configOpen;
    g_bindingTarget = 0;

    RECT rc;
    GetWindowRect(g_hWnd, &rc);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int deltaH = HUD_EXPANDED_HEIGHT - HUD_BAR_HEIGHT;

    if (g_configOpen) {
        int newY = rc.top;
        if (rc.top + HUD_EXPANDED_HEIGHT > sh - 45) {
            newY = rc.top - deltaH;
            if (newY < 0) newY = 0;
            g_shiftedUp = TRUE;
        } else {
            g_shiftedUp = FALSE;
        }
        SetWindowPos(g_hWnd, HWND_TOPMOST, rc.left, newY, HUD_WIDTH, HUD_EXPANDED_HEIGHT, SWP_NOACTIVATE);
    } else {
        int newY = g_shiftedUp ? (rc.top + deltaH) : rc.top;
        g_shiftedUp = FALSE;
        SetWindowPos(g_hWnd, HWND_TOPMOST, rc.left, newY, HUD_WIDTH, HUD_BAR_HEIGHT, SWP_NOACTIVATE);
        focus_aion_window();
    }

    Beep(1050, 50);
    InvalidateRect(g_hWnd, NULL, FALSE);
}

// ==============================================================================
// THREAD PRINCIPAL DO BOT (FLUXO DE 4 FASES COM ANTI-STUCK ENTRE ESTÁGIOS)
// ==============================================================================
static DWORD WINAPI bot_worker_thread(LPVOID lpParam) {
    (void)lpParam;
    SetUnhandledExceptionFilter(CrashHandler);
    log_debug("bot_worker_thread: INICIADA (Fluxo 4 Fases + Anti-Stuck)");

    DWORD last_heal = 0;
    DWORD phase_start = 0;
    DWORD last_pulse = 0;
    CombatPhase phase = PHASE_RMB;
    BOOL is_rmb_down = FALSE;
    BOOL is_lmb_down = FALSE;
    BOOL loot_done_in_phase = FALSE;
    BOOL safety_flush_done = FALSE;
    int  unfocused_ticks = 0;

    while (g_running) {
        // Captura de tecla em tempo real quando o usuário clica em "Mudar Tecla" no painel [CFG]
        if (g_bindingTarget != 0) {
            for (int vk = '0'; vk <= 'Z'; vk++) {
                if ((vk > '9' && vk < 'A')) continue;
                if (GetAsyncKeyState(vk) & 0x8000) {
                    if (g_bindingTarget == 1) {
                        g_keyHeal = (WORD)vk;
                    } else if (g_bindingTarget == 2) {
                        g_keyLoot = (WORD)vk;
                    }
                    g_bindingTarget = 0;
                    save_config();
                    Beep(1350, 80);
                    if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
                    Sleep(200);
                    break;
                }
            }
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                g_bindingTarget = 0;
                if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
                Sleep(200);
            }
        }

        // Fallback caso RegisterHotKey não tenha capturado F8
        if (!g_hotkeyF8Ok) {
            if (GetAsyncKeyState(VK_F8) & 0x8000) {
                toggle_bot();
                Sleep(250);
            }
        }

        // Se pausado, garante botões 100% soltos e reseta para início do fluxo
        if (!g_botActive) {
            if (is_rmb_down || is_lmb_down) {
                force_stop_aion_action();
                is_rmb_down = FALSE;
                is_lmb_down = FALSE;
            }
            phase = PHASE_RMB;
            phase_start = 0;
            last_pulse = 0;
            loot_done_in_phase = FALSE;
            safety_flush_done = FALSE;
            unfocused_ticks = 0;
            g_currentPhase = PHASE_RMB;
            g_phaseRemainingMs = (DWORD)g_rmb_hold_ms;
            Sleep(25);
            continue;
        }

        // Checagem de segurança da janela em foco
        BOOL fg_ok = is_aion2_foreground();
        if (!fg_ok) {
            unfocused_ticks++;
            if (unfocused_ticks >= 6) {
                g_isForeground = FALSE;
                if (is_rmb_down || is_lmb_down) {
                    raw_mouse_up_all();
                    is_rmb_down = FALSE;
                    is_lmb_down = FALSE;
                }
                if (phase_start > 0) {
                    phase_start += 30;
                }
                Sleep(30);
                continue;
            }
        } else {
            unfocused_ticks = 0;
            g_isForeground = TRUE;
        }

        DWORD now = GetTickCount();
        if (phase_start == 0) {
            wake_aion_viewport_for_attack(TRUE);
            now = GetTickCount();
            phase_start = now;
            last_pulse = now;
            last_heal = now; // Inicia contagem de cura sem interromper o primeiro segundo
            if (g_sessionStartTime == 0) {
                g_sessionStartTime = now;
            }
            set_mouse_right(TRUE);
            is_rmb_down = TRUE;
            log_debug("FLUXO -> FASE 1: Segurando Botao Direito (Macro RMB)");
        }

        DWORD elapsed = now - phase_start;
        g_currentPhase = phase;

        // Auto-Cura [Q]: dispara APENAS durante as fases ativas de ataque (FASE 1 ou FASE 3),
        // nunca durante os intervalos de pausa (FASE 2 ou FASE 4)
        if (g_autoHeal && (phase == PHASE_RMB || phase == PHASE_LMB) && (now - last_heal >= (DWORD)g_cd_heal)) {
            if (is_rmb_down) {
                set_mouse_right(FALSE);
                Sleep(25);
                send_game_key_ex(g_hAionWnd, g_keyHeal, 55);
                Sleep(25);
                if (g_botActive && phase == PHASE_RMB) {
                    set_mouse_right(TRUE);
                    last_pulse = GetTickCount();
                } else {
                    is_rmb_down = FALSE;
                }
            } else if (is_lmb_down) {
                set_mouse_left(FALSE);
                Sleep(25);
                send_game_key_ex(g_hAionWnd, g_keyHeal, 55);
                Sleep(25);
                if (g_botActive && phase == PHASE_LMB) {
                    set_mouse_left(TRUE);
                    last_pulse = GetTickCount();
                } else {
                    is_lmb_down = FALSE;
                }
            }
            last_heal = GetTickCount();
            now = last_heal;
            elapsed = now - phase_start;
        }

        switch (phase) {
            // ==================================================================
            // ESTÁGIO 1: SEGURANDO BOTÃO DIREITO (MACRO RMB)
            // ==================================================================
            case PHASE_RMB: {
                DWORD duration = (DWORD)g_rmb_hold_ms;
                g_phaseRemainingMs = (elapsed < duration) ? (duration - elapsed) : 0;

                if (elapsed >= duration) {
                    is_rmb_down = FALSE;
                    is_lmb_down = FALSE;
                    log_debug("FLUXO -> Fim da FASE 1: Executando Anti-Stuck para liberar RMB no Aion 2...");
                    force_stop_aion_action();

                    phase = PHASE_WAIT_RMB;
                    phase_start = GetTickCount();
                    safety_flush_done = FALSE;
                    g_currentPhase = PHASE_WAIT_RMB;
                    g_phaseRemainingMs = (DWORD)g_wait_after_rmb_ms;
                    log_debug("FLUXO -> FASE 2: Pausa apos RMB (Botao Direito destravado)");
                    break;
                }

                if (!is_rmb_down) {
                    if (is_lmb_down) { set_mouse_left(FALSE); is_lmb_down = FALSE; }
                    set_mouse_right(TRUE);
                    is_rmb_down = TRUE;
                    last_pulse = now;
                } else if (now - last_pulse >= 300) {
                    // Micro-pulso a cada 300ms: aciona a busca de mob mais próximo do Aion 2
                    // e impede a Unreal Engine de travar o estado contínuo do RMB
                    set_mouse_right(FALSE);
                    Sleep(30);
                    if (g_botActive && phase == PHASE_RMB) {
                        set_mouse_right(TRUE);
                        is_rmb_down = TRUE;
                    }
                    last_pulse = GetTickCount();
                }
                break;
            }

            // ==================================================================
            // ESTÁGIO 2: INTERVALO DE PAUSA APÓS BOTÃO DIREITO (NADA PRESSIONADO)
            // ==================================================================
            case PHASE_WAIT_RMB: {
                DWORD duration = (DWORD)g_wait_after_rmb_ms;
                g_phaseRemainingMs = (elapsed < duration) ? (duration - elapsed) : 0;

                if (is_rmb_down || is_lmb_down) {
                    raw_mouse_up_all();
                    is_rmb_down = FALSE;
                    is_lmb_down = FALSE;
                }

                // Reforço silencioso de soltura aos 250ms de pausa
                if (!safety_flush_done && elapsed >= 250) {
                    safety_flush_done = TRUE;
                    raw_mouse_up_all();
                }

                if (elapsed >= duration) {
                    log_debug("FLUXO -> FASE 3: Ativando viewport e segurando Botao Esquerdo (Auto-Ataque LMB)");
                    wake_aion_viewport_for_attack(FALSE);

                    phase = PHASE_LMB;
                    phase_start = GetTickCount();
                    last_pulse = phase_start;
                    g_currentPhase = PHASE_LMB;
                    g_phaseRemainingMs = (DWORD)g_lmb_hold_ms;

                    set_mouse_left(TRUE);
                    is_lmb_down = TRUE;
                    is_rmb_down = FALSE;
                }
                break;
            }

            // ==================================================================
            // ESTÁGIO 3: SEGURANDO BOTÃO ESQUERDO (AUTO-ATAQUE LMB)
            // ==================================================================
            case PHASE_LMB: {
                DWORD duration = (DWORD)g_lmb_hold_ms;
                g_phaseRemainingMs = (elapsed < duration) ? (duration - elapsed) : 0;

                if (elapsed >= duration) {
                    is_lmb_down = FALSE;
                    is_rmb_down = FALSE;
                    log_debug("FLUXO -> Fim da FASE 3: Executando Anti-Stuck para liberar LMB no Aion 2...");
                    force_stop_aion_action();

                    phase = PHASE_WAIT_LMB;
                    phase_start = GetTickCount();
                    loot_done_in_phase = FALSE;
                    safety_flush_done = FALSE;
                    g_currentPhase = PHASE_WAIT_LMB;
                    g_phaseRemainingMs = (DWORD)g_wait_after_lmb_ms;
                    log_debug("FLUXO -> FASE 4: Pausa final + Coleta de Loot");
                    break;
                }

                if (!is_lmb_down) {
                    if (is_rmb_down) { set_mouse_right(FALSE); is_rmb_down = FALSE; }
                    set_mouse_left(TRUE);
                    is_lmb_down = TRUE;
                    last_pulse = now;
                } else if (now - last_pulse >= 260) {
                    // Micro-pulso a cada 260ms: garante seleção de mob mais próximo e combo contínuo de auto-ataque
                    set_mouse_left(FALSE);
                    Sleep(25);
                    if (g_botActive && phase == PHASE_LMB) {
                        set_mouse_left(TRUE);
                        is_lmb_down = TRUE;
                    }
                    last_pulse = GetTickCount();
                }
                break;
            }

            // ==================================================================
            // ESTÁGIO 4: ESPERA FINAL + COLETA DE LOOT [F]
            // ==================================================================
            case PHASE_WAIT_LMB: {
                DWORD duration = (DWORD)g_wait_after_lmb_ms;
                g_phaseRemainingMs = (elapsed < duration) ? (duration - elapsed) : 0;

                if (is_rmb_down || is_lmb_down) {
                    raw_mouse_up_all();
                    is_rmb_down = FALSE;
                    is_lmb_down = FALSE;
                }

                if (!safety_flush_done && elapsed >= 200) {
                    safety_flush_done = TRUE;
                    raw_mouse_up_all();
                }

                if (g_autoLoot && !loot_done_in_phase && elapsed >= 350) {
                    loot_done_in_phase = TRUE;
                    for (int lp = 0; lp < g_lootPressCount; lp++) {
                        if (!g_botActive || !g_running) break;
                        send_game_key_ex(g_hAionWnd, g_keyLoot, 60);
                        Sleep(75);
                    }
                }

                if (elapsed >= duration) {
                    g_mobsKilled++;
                    log_debug("FLUXO -> Reiniciando Ciclo: FASE 1 (Macro RMB)");
                    wake_aion_viewport_for_attack(TRUE);

                    phase = PHASE_RMB;
                    phase_start = GetTickCount();
                    last_pulse = phase_start;
                    loot_done_in_phase = FALSE;
                    safety_flush_done = FALSE;
                    g_currentPhase = PHASE_RMB;
                    g_phaseRemainingMs = (DWORD)g_rmb_hold_ms;

                    set_mouse_right(TRUE);
                    is_rmb_down = TRUE;
                    is_lmb_down = FALSE;
                }
                break;
            }
        }

        Sleep(20);
    }

    force_stop_aion_action();
    log_debug("bot_worker_thread: FINALIZADA");
    return 0;
}

// ==============================================================================
// RENDERIZAÇÃO GDI DO OVERLAY HUD + PAINEL DE CONFIGURAÇÃO EM TEMPO REAL
// ==============================================================================
static void draw_pill(HDC hdc, int x, int y, int w, int h, COLORREF bg, COLORREF border, const WCHAR *text, COLORREF text_color, HFONT font) {
    HBRUSH hBr = CreateSolidBrush(bg);
    HPEN hPen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBr = SelectObject(hdc, hBr);
    HGDIOBJ oldPen = SelectObject(hdc, hPen);

    RoundRect(hdc, x, y, x + w, y + h, 6, 6);

    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, text_color);
    HGDIOBJ oldFont = SelectObject(hdc, font);

    RECT rc = { x, y, x + w, y + h };
    DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);
}

static void draw_stepper_row(HDC hdc, int x, int y, const WCHAR *label, int ms_or_val, BOOL is_ms) {
    draw_pill(hdc, x, y, 230, 24, COLOR_SLOT_BG, COLOR_BORDER, label, COLOR_TEXT_WHITE, g_hFontSmall);
    draw_pill(hdc, x + 234, y, 28, 24, COLOR_BTN_BG, COLOR_BADGE_BD, L"−", COLOR_CYAN, g_hFontBold);
    WCHAR valStr[32];
    if (is_ms) {
        wsprintfW(valStr, L"%d.%ds", ms_or_val / 1000, (ms_or_val % 1000) / 100);
    } else {
        wsprintfW(valStr, L"%dx", ms_or_val);
    }
    draw_pill(hdc, x + 266, y, 62, 24, COLOR_BG, COLOR_BORDER, valStr, COLOR_GREEN, g_hFontBold);
    draw_pill(hdc, x + 332, y, 28, 24, COLOR_BTN_BG, COLOR_BADGE_BD, L"+", COLOR_CYAN, g_hFontBold);
}

static void on_paint(HWND hWnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    RECT rc;
    GetClientRect(hWnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    HBRUSH bgBrush = CreateSolidBrush(COLOR_BG);
    HPEN borderPen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
    HGDIOBJ ob1 = SelectObject(memDC, bgBrush);
    HGDIOBJ op1 = SelectObject(memDC, borderPen);

    RoundRect(memDC, 0, 0, width, height, 8, 8);

    SelectObject(memDC, ob1);
    SelectObject(memDC, op1);
    DeleteObject(bgBrush);
    DeleteObject(borderPen);

    // --------------------------------------------------------------------------
    // BARRA SUPERIOR PRINCIPAL (y = 7..35)
    // --------------------------------------------------------------------------
    if (g_botActive) {
        draw_pill(memDC, 8, 7, 138, 28, COLOR_ACTIVE_BG, COLOR_GREEN, L"COMBATE: ATIVO [F8]", COLOR_GREEN, g_hFontBold);
    } else {
        draw_pill(memDC, 8, 7, 138, 28, COLOR_SLOT_BG, COLOR_BORDER, L"PAUSADO [F8]", COLOR_RED, g_hFontBold);
    }

    if (!g_botActive) {
        draw_pill(memDC, 152, 7, 124, 28, COLOR_SLOT_BG, COLOR_BORDER, L"FLUXO: PAUSA", COLOR_TEXT_DIM, g_hFontSmall);
    } else if (!g_isForeground) {
        draw_pill(memDC, 152, 7, 124, 28, COLOR_SEARCH_BG, COLOR_AMBER, L"⏸ FORA DE FOCO", COLOR_AMBER, g_hFontSmall);
    } else {
        DWORD rem = g_phaseRemainingMs;
        int secWhole = (int)(rem / 1000);
        int secTenths = (int)((rem % 1000) / 100);
        WCHAR phaseStr[48];

        switch (g_currentPhase) {
            case PHASE_RMB:
                wsprintfW(phaseStr, L"▶ [DIR] %d.%ds", secWhole, secTenths);
                draw_pill(memDC, 152, 7, 124, 28, COLOR_ACTIVE_BG, COLOR_GREEN, phaseStr, COLOR_GREEN, g_hFontBold);
                break;
            case PHASE_WAIT_RMB:
                wsprintfW(phaseStr, L"⏸ PAUSA %d.%ds", secWhole, secTenths);
                draw_pill(memDC, 152, 7, 124, 28, COLOR_SLOT_BG, COLOR_AMBER, phaseStr, COLOR_AMBER, g_hFontSmall);
                break;
            case PHASE_LMB:
                wsprintfW(phaseStr, L"▶ [ESQ] %d.%ds", secWhole, secTenths);
                draw_pill(memDC, 152, 7, 124, 28, COLOR_COMBAT_BG, COLOR_CYAN, phaseStr, COLOR_CYAN, g_hFontBold);
                break;
            case PHASE_WAIT_LMB:
                wsprintfW(phaseStr, L"✦ ESPERA %d.%ds", secWhole, secTenths);
                draw_pill(memDC, 152, 7, 124, 28, COLOR_SEARCH_BG, COLOR_AMBER, phaseStr, COLOR_AMBER, g_hFontSmall);
                break;
            default:
                draw_pill(memDC, 152, 7, 124, 28, COLOR_SLOT_BG, COLOR_BORDER, L"EM EXECUÇÃO", COLOR_CYAN, g_hFontSmall);
                break;
        }
    }

    DWORD now = GetTickCount();
    DWORD elapsedSec = (g_sessionStartTime > 0) ? ((now - g_sessionStartTime) / 1000) : 0;
    int mobsPerHour = 0;
    if (elapsedSec > 10) {
        mobsPerHour = (int)((g_mobsKilled * 3600ULL) / elapsedSec);
    }
    WCHAR farmStr[48];
    wsprintfW(farmStr, L"⚔ %d MOBS (%d/h)", g_mobsKilled, mobsPerHour);
    draw_pill(memDC, 282, 7, 122, 28, COLOR_SLOT_BG, COLOR_BORDER, farmStr, COLOR_TEXT_WHITE, g_hFontSmall);

    int minutes = (int)(elapsedSec / 60);
    int seconds = (int)(elapsedSec % 60);
    int hours   = minutes / 60;
    minutes = minutes % 60;
    WCHAR timeStr[32];
    if (hours > 0) {
        wsprintfW(timeStr, L"⏱ %02d:%02d:%02d", hours, minutes, seconds);
    } else {
        wsprintfW(timeStr, L"⏱ %02d:%02d", minutes, seconds);
    }
    draw_pill(memDC, 410, 7, 68, 28, COLOR_SLOT_BG, COLOR_BORDER, timeStr, COLOR_CYAN, g_hFontSmall);

    WCHAR healStr[32];
    wsprintfW(healStr, L"CURA [%c]: %s", (WCHAR)g_keyHeal, g_autoHeal ? L"ON" : L"OFF");
    draw_pill(memDC, 484, 7, 92, 28, COLOR_SLOT_BG, COLOR_BORDER, healStr, g_autoHeal ? COLOR_GREEN : COLOR_TEXT_DIM, g_hFontSmall);

    WCHAR lootStr[32];
    wsprintfW(lootStr, L"LOOT [%c]: %s", (WCHAR)g_keyLoot, g_autoLoot ? L"ON" : L"OFF");
    draw_pill(memDC, 582, 7, 92, 28, COLOR_SLOT_BG, COLOR_BORDER, lootStr, g_autoLoot ? COLOR_GREEN : COLOR_TEXT_DIM, g_hFontSmall);

    if (g_configOpen) {
        draw_pill(memDC, 680, 7, 46, 28, COLOR_BADGE_BG, COLOR_CYAN, L"⚙ CFG", COLOR_CYAN, g_hFontBold);
    } else {
        draw_pill(memDC, 680, 7, 46, 28, COLOR_SLOT_BG, COLOR_BADGE_BD, L"⚙ CFG", COLOR_CYAN, g_hFontSmall);
    }

    draw_pill(memDC, 730, 7, 22, 28, COLOR_SLOT_BG, COLOR_BORDER, L"✕", COLOR_TEXT_DIM, g_hFontBold);

    // --------------------------------------------------------------------------
    // PAINEL EXPANSÍVEL DE CONFIGURAÇÕES EM TEMPO REAL (y = 44..170)
    // --------------------------------------------------------------------------
    if (g_configOpen) {
        HPEN divPen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
        HGDIOBJ oldDiv = SelectObject(memDC, divPen);
        MoveToEx(memDC, 8, 42, NULL);
        LineTo(memDC, width - 8, 42);
        SelectObject(memDC, oldDiv);
        DeleteObject(divPen);

        draw_stepper_row(memDC, 10, 48,  L"1. Botão Direito (Macro RMB)", g_rmb_hold_ms, TRUE);
        draw_stepper_row(memDC, 10, 78,  L"2. Pausa após Botão Direito",  g_wait_after_rmb_ms, TRUE);
        draw_stepper_row(memDC, 10, 108, L"3. Botão Esquerdo (Ataque LMB)", g_lmb_hold_ms, TRUE);
        draw_stepper_row(memDC, 10, 138, L"4. Espera Final + Coleta Loot", g_wait_after_lmb_ms, TRUE);

        draw_stepper_row(memDC, 390, 48, L"Intervalo Auto-Cura (Cooldown)", g_cd_heal, TRUE);
        draw_stepper_row(memDC, 390, 78, L"Toques da Tecla de Coleta",      g_lootPressCount, FALSE);

        WCHAR bindHealStr[48];
        if (g_bindingTarget == 1) {
            wsprintfW(bindHealStr, L"▶ PRESSIONE TECLA...");
            draw_pill(memDC, 390, 108, 176, 24, COLOR_SEARCH_BG, COLOR_AMBER, bindHealStr, COLOR_AMBER, g_hFontBold);
        } else {
            wsprintfW(bindHealStr, L"TECLA CURA: [%c] (Mudar)", (WCHAR)g_keyHeal);
            draw_pill(memDC, 390, 108, 176, 24, COLOR_BTN_BG, COLOR_BADGE_BD, bindHealStr, COLOR_TEXT_WHITE, g_hFontSmall);
        }

        WCHAR bindLootStr[48];
        if (g_bindingTarget == 2) {
            wsprintfW(bindLootStr, L"▶ PRESSIONE TECLA...");
            draw_pill(memDC, 574, 108, 176, 24, COLOR_SEARCH_BG, COLOR_AMBER, bindLootStr, COLOR_AMBER, g_hFontBold);
        } else {
            wsprintfW(bindLootStr, L"TECLA LOOT: [%c] (Mudar)", (WCHAR)g_keyLoot);
            draw_pill(memDC, 574, 108, 176, 24, COLOR_BTN_BG, COLOR_BADGE_BD, bindLootStr, COLOR_TEXT_WHITE, g_hFontSmall);
        }

        if (g_windowGuard) {
            draw_pill(memDC, 390, 138, 216, 24, COLOR_BADGE_BG, COLOR_CYAN, L"FOCO: SÓ NO AION 2 [F9]", COLOR_CYAN, g_hFontSmall);
        } else {
            draw_pill(memDC, 390, 138, 216, 24, COLOR_SEARCH_BG, COLOR_AMBER, L"FOCO: MODO LIVRE [F9]", COLOR_AMBER, g_hFontSmall);
        }

        draw_pill(memDC, 614, 138, 136, 24, COLOR_SLOT_BG, COLOR_BORDER, L"↺ ZERAR CONTADOR", COLOR_TEXT_DIM, g_hFontSmall);
    }

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hWnd, &ps);
}

// ==============================================================================
// WNDPROC & INTERAÇÃO COM O MOUSE / HOTKEYS / MICRO-FOCUS RESET (WM_APP_UNSTICK)
// ==============================================================================
static BOOL handle_stepper_click(int clickX, int clickY, int rowX, int rowY, volatile int *val, int step, int minVal, int maxVal) {
    if (clickY < rowY || clickY > rowY + 24) return FALSE;
    if (clickX >= rowX + 234 && clickX <= rowX + 262) {
        int v = *val - step;
        if (v < minVal) v = minVal;
        *val = v;
        save_config();
        Beep(850, 40);
        return TRUE;
    }
    if (clickX >= rowX + 332 && clickX <= rowX + 360) {
        int v = *val + step;
        if (v > maxVal) v = maxVal;
        *val = v;
        save_config();
        Beep(1150, 40);
        return TRUE;
    }
    return FALSE;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            log_debug("WndProc: WM_CREATE recebido");
            g_hWnd = hWnd;
            HMODULE hUser32 = GetModuleHandleA("user32.dll");
            if (hUser32) {
                fn_SetLayeredWindowAttributes = (pfn_SetLayeredWindowAttributes)GetProcAddress(hUser32, "SetLayeredWindowAttributes");
                if (fn_SetLayeredWindowAttributes) {
                    fn_SetLayeredWindowAttributes(hWnd, 0, (BYTE)(255 * 0.94f), LWA_ALPHA);
                }
            }

            g_hotkeyF8Ok = RegisterHotKey(hWnd, 108, 0, VK_F8);
            RegisterHotKey(hWnd, 106, 0, VK_F6);
            RegisterHotKey(hWnd, 107, 0, VK_F7);
            RegisterHotKey(hWnd, 109, 0, VK_F9);
            RegisterHotKey(hWnd, 110, 0, VK_F10);
            RegisterHotKey(hWnd, 112, 0, VK_F12);

            SetTimer(hWnd, 1, 50, NULL);
            return 0;
        }

        case WM_MOUSEACTIVATE: {
            return MA_NOACTIVATE;
        }

        // Micro-Focus Reset: reproduz exatamente o evento de desativação de janela do Alt-Tab
        // por 75ms (indo para o nosso HUD e voltando ao Aion 2), forçando a Unreal Engine 5
        // a chamar FlushPressedKeys() e destravar o botão direito / macro!
        case WM_APP_UNSTICK: {
            HWND fg = GetForegroundWindow();
            HWND hAion = find_aion_window();
            if (hAion && IsWindow(hAion)) {
                LPARAM lp = get_aion_center_lparam(hAion);
                PostMessageW(hAion, WM_RBUTTONUP, 0, lp);
                PostMessageW(hAion, WM_LBUTTONUP, 0, lp);
                PostMessageW(hAion, WM_CANCELMODE, 0, 0);
                PostMessageW(hAion, WM_CAPTURECHANGED, 0, 0);
                PostMessageW(hAion, WM_KILLFOCUS, (WPARAM)hWnd, 0);
                PostMessageW(hAion, WM_ACTIVATE, WA_INACTIVE, (LPARAM)hWnd);
                PostMessageW(hAion, WM_ACTIVATEAPP, FALSE, GetCurrentThreadId());

                if (fg == hAion || fg == hWnd) {
                    LONG_PTR exStyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
                    SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle & ~WS_EX_NOACTIVATE);

                    DWORD aionTid = GetWindowThreadProcessId(hAion, NULL);
                    DWORD myTid   = GetCurrentThreadId();
                    BOOL attached = FALSE;
                    if (aionTid && aionTid != myTid) {
                        attached = AttachThreadInput(myTid, aionTid, TRUE);
                    }

                    AllowSetForegroundWindow(ASFW_ANY);
                    BringWindowToTop(hWnd);
                    SetForegroundWindow(hWnd);
                    SetActiveWindow(hWnd);
                    SetFocus(hWnd);

                    // 75ms (~4 frames em 60fps) garante que o loop principal do AION2.exe processe
                    // a perda de foco e limpe todos os botões travados (FlushPressedKeys)
                    Sleep(75);
                    raw_mouse_up_all();

                    SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle | WS_EX_NOACTIVATE);
                    BringWindowToTop(hAion);
                    SetForegroundWindow(hAion);
                    SetActiveWindow(hAion);
                    SetFocus(hAion);

                    if (attached) {
                        AttachThreadInput(myTid, aionTid, FALSE);
                    }

                    PostMessageW(hAion, WM_ACTIVATEAPP, TRUE, aionTid);
                    PostMessageW(hAion, WM_ACTIVATE, WA_ACTIVE, (LPARAM)hWnd);
                    PostMessageW(hAion, WM_SETFOCUS, (WPARAM)hWnd, 0);
                }
            }
            return 0;
        }

        case WM_HOTKEY: {
            if (wParam == 108) toggle_bot();
            else if (wParam == 106) toggle_heal();
            else if (wParam == 107) toggle_loot();
            else if (wParam == 109) toggle_guard();
            else if (wParam == 110) toggle_config_panel();
            else if (wParam == 112) {
                log_debug("WM_HOTKEY: F12 pressionado! Fechando...");
                PostMessage(hWnd, WM_CLOSE, 0, 0);
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 1) {
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            if (y >= 7 && y <= 35) {
                if (x >= 730 && x <= 752) {
                    log_debug("Botao [X] clicado!");
                    PostMessage(hWnd, WM_CLOSE, 0, 0);
                    return 0;
                }
                if (x >= 680 && x <= 726) {
                    toggle_config_panel();
                    return 0;
                }
                if (x >= 8 && x <= 146) {
                    toggle_bot();
                    return 0;
                }
                if (x >= 484 && x <= 576) {
                    toggle_heal();
                    return 0;
                }
                if (x >= 582 && x <= 674) {
                    toggle_loot();
                    return 0;
                }
            }

            if (g_configOpen && y >= 45) {
                if (handle_stepper_click(x, y, 10, 48,  &g_rmb_hold_ms,       500, 500, 60000) ||
                    handle_stepper_click(x, y, 10, 78,  &g_wait_after_rmb_ms, 500, 200, 30000) ||
                    handle_stepper_click(x, y, 10, 108, &g_lmb_hold_ms,       500, 500, 60000) ||
                    handle_stepper_click(x, y, 10, 138, &g_wait_after_lmb_ms, 500, 200, 30000) ||
                    handle_stepper_click(x, y, 390, 48, &g_cd_heal,           500, 500, 60000) ||
                    handle_stepper_click(x, y, 390, 78, &g_lootPressCount,    1,   1,   15)) {
                    InvalidateRect(hWnd, NULL, FALSE);
                    focus_aion_window();
                    return 0;
                }

                if (x >= 390 && x <= 566 && y >= 108 && y <= 132) {
                    g_bindingTarget = (g_bindingTarget == 1) ? 0 : 1;
                    Beep(1000, 50);
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }

                if (x >= 574 && x <= 750 && y >= 108 && y <= 132) {
                    g_bindingTarget = (g_bindingTarget == 2) ? 0 : 2;
                    Beep(1000, 50);
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }

                if (x >= 390 && x <= 606 && y >= 138 && y <= 162) {
                    toggle_guard();
                    return 0;
                }

                if (x >= 614 && x <= 750 && y >= 138 && y <= 162) {
                    g_mobsKilled = 0;
                    g_sessionStartTime = GetTickCount();
                    Beep(950, 60);
                    InvalidateRect(hWnd, NULL, FALSE);
                    focus_aion_window();
                    return 0;
                }
            }

            if (!g_botActive) {
                ReleaseCapture();
                SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
            return 0;
        }

        case WM_PAINT: {
            on_paint(hWnd);
            return 0;
        }

        case WM_CLOSE: {
            log_debug("WndProc: WM_CLOSE recebido");
            DestroyWindow(hWnd);
            return 0;
        }

        case WM_DESTROY: {
            log_debug("WndProc: WM_DESTROY recebido");
            UnregisterHotKey(hWnd, 108);
            UnregisterHotKey(hWnd, 106);
            UnregisterHotKey(hWnd, 107);
            UnregisterHotKey(hWnd, 109);
            UnregisterHotKey(hWnd, 110);
            UnregisterHotKey(hWnd, 112);
            g_running = FALSE;
            raw_mouse_up_all();
            PostQuitMessage(0);
            return 0;
        }

        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

// ==============================================================================
// ENTRY POINT (WINMAIN) COM BLOQUEIO ESTRITO DE MÚLTIPLOS PROCESSOS
// ==============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    SetUnhandledExceptionFilter(CrashHandler);
    init_paths();

    // Remove o bloqueio de tempo de foco do Windows para permitir o Micro-Focus Reset instantâneo
    SystemParametersInfoW(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (PVOID)0, SPIF_SENDCHANGE);
    AllowSetForegroundWindow(ASFW_ANY);

    // Proibição estrita de múltiplos processos: apenas 1 bot pode rodar!
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Global\\Aion2_Bot_SingleInstance_Mutex");
    if (!hMutex) {
        hMutex = CreateMutexW(NULL, TRUE, L"Aion2_Bot_SingleInstance_Mutex");
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS || FindWindowW(L"Aion2BotOverlay", NULL) != NULL) {
        Beep(500, 150);
        MessageBoxW(
            NULL,
            L"O Aion 2 Bot já está aberto e em execução!\n\nApenas 1 processo do bot pode rodar por vez.",
            L"Aion 2 Bot - Processo Único",
            MB_ICONWARNING | MB_OK | MB_TOPMOST
        );
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    log_debug("WinMain: Iniciado (Instancia Unica + Anti-Stuck Ativo)");
    load_or_create_config();
    g_sessionStartTime = GetTickCount();

    g_hFontBold = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    g_hFontSmall = CreateFontW(12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"Aion2BotOverlay";

    if (!RegisterClassExW(&wc)) {
        log_debug("RegisterClassExW falhou!");
        return 1;
    }

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int posX = (g_iniPosX >= 0) ? g_iniPosX : (sw - HUD_WIDTH - 25);
    int posY = (g_iniPosY >= 0) ? g_iniPosY : (sh - HUD_BAR_HEIGHT - 65);

    HWND hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"Aion2BotOverlay",
        L"AION 2 Combat Overlay",
        WS_POPUP,
        posX, posY, HUD_WIDTH, HUD_BAR_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) {
        log_debug("CreateWindowExW falhou!");
        return 1;
    }

    ShowWindow(hWnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hWnd);

    HANDLE hThread = CreateThread(NULL, 0, bot_worker_thread, NULL, 0, NULL);
    if (hThread) {
        CloseHandle(hThread);
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    RECT finalRc;
    if (GetWindowRect(hWnd, &finalRc)) {
        int saveY = g_shiftedUp ? (finalRc.top + (HUD_EXPANDED_HEIGHT - HUD_BAR_HEIGHT)) : finalRc.top;
        write_ini_int(L"Overlay", L"PosX", finalRc.left);
        write_ini_int(L"Overlay", L"PosY", saveY);
    }

    if (g_hFontBold) DeleteObject(g_hFontBold);
    if (g_hFontSmall) DeleteObject(g_hFontSmall);

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    log_debug("WinMain: Encerrado normalmente");
    return (int)msg.wParam;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return WinMain(GetModuleHandleW(NULL), NULL, GetCommandLineA(), SW_SHOW);
}
