#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WS_EX_LAYERED
#define WS_EX_LAYERED 0x00080000
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

typedef BOOL (WINAPI *pfn_SetLayeredWindowAttributes)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
static pfn_SetLayeredWindowAttributes fn_SetLayeredWindowAttributes = NULL;

static void log_debug(const char *msg) {
    FILE *f = fopen("C:\\Users\\iuryc\\aion2-bot\\debug.log", "a");
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

#define HUD_WIDTH   600
#define HUD_HEIGHT  42

// ==============================================================================
// ESTADO GLOBAL
// ==============================================================================
static HWND g_hWnd = NULL;
static HWND g_hAionWnd = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontSmall = NULL;

static volatile BOOL g_running = TRUE;
static volatile BOOL g_botActive = FALSE;
static volatile BOOL g_autoHeal = TRUE;
static volatile BOOL g_windowGuard = TRUE;     // TRUE = só no Aion2, FALSE = Livre
static volatile BOOL g_targetLocked = FALSE;
static volatile BOOL g_isForeground = FALSE;

// ==============================================================================
// TOGGLES COM FEEDBACK SONORO
// ==============================================================================
static void toggle_bot(void) {
    g_botActive = !g_botActive;
    if (g_botActive) {
        log_debug("toggle_bot: ATIVADO");
        Beep(1200, 100);
    } else {
        log_debug("toggle_bot: PAUSADO");
        Beep(600, 150);
    }
    if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
}

static void toggle_heal(void) {
    g_autoHeal = !g_autoHeal;
    log_debug("toggle_heal: ALTERNADO");
    Beep(900, 80);
    if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
}

static void toggle_guard(void) {
    g_windowGuard = !g_windowGuard;
    if (g_windowGuard) {
        log_debug("toggle_guard: PROTECAO AION2 ATIVADA");
        Beep(1000, 80);
    } else {
        log_debug("toggle_guard: MODO LIVRE ATIVADO");
        Beep(800, 100);
    }
    if (g_hWnd) InvalidateRect(g_hWnd, NULL, FALSE);
}

// ==============================================================================
// VERIFICAÇÃO ROBUSTA DE JANELA (AION 2 EM FOCO)
// ==============================================================================
static BOOL is_aion2_foreground(void) {
    HWND fg = GetForegroundWindow();
    if (!fg) return FALSE;
    if (fg == g_hWnd) return TRUE;

    if (!g_windowGuard) {
        g_hAionWnd = fg;
        return TRUE;
    }

    // 1. Checa título Unicode da janela
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

    // 2. Checa processo via QueryFullProcessImageNameW
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

            // Fallback: psapi GetProcessImageFileNameA
            typedef DWORD (WINAPI *pfn_GetProcessImageFileNameA)(HANDLE, LPSTR, DWORD);
            static pfn_GetProcessImageFileNameA fnImgA = NULL;
            static BOOL resolvedImg = FALSE;
            if (!resolvedImg) {
                HMODULE hPsapi = LoadLibraryA("psapi.dll");
                if (hPsapi) fnImgA = (pfn_GetProcessImageFileNameA)GetProcAddress(hPsapi, "GetProcessImageFileNameA");
                resolvedImg = TRUE;
            }
            if (fnImgA) {
                char devPath[1024] = {0};
                if (fnImgA(hProc, devPath, sizeof(devPath)) > 0) {
                    for (int i = 0; devPath[i] != '\0'; i++) {
                        char c = devPath[i];
                        if ((c == 'a' || c == 'A') &&
                            (devPath[i+1] == 'i' || devPath[i+1] == 'I') &&
                            (devPath[i+2] == 'o' || devPath[i+2] == 'O') &&
                            (devPath[i+3] == 'n' || devPath[i+3] == 'N')) {
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
// DETECÇÃO ULTRA-RÁPIDA DE ALVO (BITBLT MEMORY DC CACHEADO <0.5ms)
// ==============================================================================
static BOOL is_target_locked(void) {
    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return FALSE;

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // Em 1080p, a barra carmesim do alvo fica em Y = 56..60, centralizada em X = 960
    int startY = (int)(screenH * 0.052f + 0.5f); // ~56px
    int midX   = screenW / 2;                    // ~960px
    int startX = midX - 120;                     // ~840px
    int width  = 240;
    int height = 5;

    HDC memDC = CreateCompatibleDC(hdcScreen);
    if (!memDC) {
        ReleaseDC(NULL, hdcScreen);
        return FALSE;
    }
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, width, height);
    if (!hBmp) {
        DeleteDC(memDC);
        ReleaseDC(NULL, hdcScreen);
        return FALSE;
    }
    HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

    // Captura apenas a faixa relevante de uma vez
    BitBlt(memDC, 0, 0, width, height, hdcScreen, startX, startY, SRCCOPY);

    // Amostra 5 pontos ao longo da barra em memória RAM (instantâneo)
    int sampleXs[5] = { 20, 60, 100, 140, 180 };
    int redMatches = 0;
    for (int i = 0; i < 5; i++) {
        for (int y = 0; y < height; y++) {
            COLORREF c = GetPixel(memDC, sampleXs[i], y);
            int r = GetRValue(c);
            int g = GetGValue(c);
            int b = GetBValue(c);

            // Vermelho carmesim vivo da barra de vida do alvo (R > 150, G < 40, B < 60)
            if (r > 150 && g < 40 && b < 60) {
                redMatches++;
                break;
            }
        }
    }

    SelectObject(memDC, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);
    ReleaseDC(NULL, hdcScreen);

    return (redMatches >= 2);
}

// ==============================================================================
// SIMULAÇÃO DE INPUTS (MOUSE NATIVO + POSTMESSAGE / SCAN CODE HYBRID)
// ==============================================================================
static void send_click_left(void) {
    INPUT inps[2];
    ZeroMemory(inps, sizeof(inps));

    inps[0].type = INPUT_MOUSE;
    inps[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;

    inps[1].type = INPUT_MOUSE;
    inps[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    SendInput(1, &inps[0], sizeof(INPUT));
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(35);
    SendInput(1, &inps[1], sizeof(INPUT));
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
}

static void send_click_right(void) {
    INPUT inps[2];
    ZeroMemory(inps, sizeof(inps));

    inps[0].type = INPUT_MOUSE;
    inps[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;

    inps[1].type = INPUT_MOUSE;
    inps[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;

    SendInput(1, &inps[0], sizeof(INPUT));
    mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
    Sleep(35);
    SendInput(1, &inps[1], sizeof(INPUT));
    mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
}

static void send_mouse_move(int dx, int dy) {
    INPUT inp;
    ZeroMemory(&inp, sizeof(INPUT));
    inp.type = INPUT_MOUSE;
    inp.mi.dx = dx;
    inp.mi.dy = dy;
    inp.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &inp, sizeof(INPUT));
    mouse_event(MOUSEEVENTF_MOVE, dx, dy, 0, 0);
}

// Envia tecla com PostMessage direto (ignora WH_KEYBOARD_LL) E SendInput com hardware scancode
static void send_game_key(HWND hwnd, WORD vk) {
    UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
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

    Sleep(50);

    if (hwnd && IsWindow(hwnd)) {
        PostMessageW(hwnd, WM_KEYUP, vk, lpUp);
    }
    inp.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    SendInput(1, &inp, sizeof(INPUT));
}

// Inicia caminhada para frente com 'W' para aproximar do alvo
static void start_walk_forward(HWND hwnd) {
    UINT scan = MapVirtualKeyW('W', MAPVK_VK_TO_VSC);
    LPARAM lpDown = 1 | ((scan & 0xFF) << 16);
    if (hwnd && IsWindow(hwnd)) {
        PostMessageW(hwnd, WM_KEYDOWN, 'W', lpDown);
    }
    INPUT inp;
    ZeroMemory(&inp, sizeof(INPUT));
    inp.type = INPUT_KEYBOARD;
    inp.ki.wVk = 'W';
    inp.ki.wScan = (WORD)scan;
    inp.ki.dwFlags = KEYEVENTF_SCANCODE;
    SendInput(1, &inp, sizeof(INPUT));
}

// Para de andar para frente
static void stop_walk_forward(HWND hwnd) {
    UINT scan = MapVirtualKeyW('W', MAPVK_VK_TO_VSC);
    LPARAM lpUp = 1 | ((scan & 0xFF) << 16) | (1 << 30) | (1 << 31);
    if (hwnd && IsWindow(hwnd)) {
        PostMessageW(hwnd, WM_KEYUP, 'W', lpUp);
    }
    INPUT inp;
    ZeroMemory(&inp, sizeof(INPUT));
    inp.type = INPUT_KEYBOARD;
    inp.ki.wVk = 'W';
    inp.ki.wScan = (WORD)scan;
    inp.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    SendInput(1, &inp, sizeof(INPUT));
}

// ==============================================================================
// THREAD PRINCIPAL DO BOT
// ==============================================================================
static DWORD WINAPI bot_worker_thread(LPVOID lpParam) {
    SetUnhandledExceptionFilter(CrashHandler);
    log_debug("bot_worker_thread: INICIADA");
    DWORD last_heal = 0;
    DWORD last_skill_e = 0;
    DWORD last_rmb = 0;
    DWORD combat_start = 0;
    BOOL is_walking = FALSE;
    int lost_counter = 0;

    while (g_running) {
        // Atalhos globais
        if (GetAsyncKeyState(VK_F8) & 0x8000) {
            toggle_bot();
            Sleep(250);
        }
        if ((GetAsyncKeyState(VK_F6) & 0x8000) || (GetAsyncKeyState(VK_F7) & 0x8000)) {
            toggle_heal();
            Sleep(250);
        }
        if (GetAsyncKeyState(VK_F9) & 0x8000) {
            toggle_guard();
            Sleep(250);
        }
        if (GetAsyncKeyState(VK_F12) & 0x8000) {
            log_debug("bot_worker_thread: VK_F12 pressionado! Fechando...");
            PostMessage(g_hWnd, WM_CLOSE, 0, 0);
            break;
        }

        // Se pausado, interrompe caminhada e aguarda
        if (!g_botActive) {
            if (is_walking) {
                stop_walk_forward(g_hAionWnd);
                is_walking = FALSE;
            }
            g_targetLocked = FALSE;
            Sleep(30);
            continue;
        }

        // Checagem de segurança da janela em foco
        g_isForeground = is_aion2_foreground();
        if (!g_isForeground) {
            if (is_walking) {
                stop_walk_forward(g_hAionWnd);
                is_walking = FALSE;
            }
            Sleep(40);
            continue;
        }

        // Se já tivermos um alvo travado na tela, atualiza estado imediatamente
        if (is_target_locked()) {
            if (!g_targetLocked) {
                combat_start = GetTickCount();
                g_targetLocked = TRUE;
                lost_counter = 0;
                log_debug("Alvo travado com sucesso!");
                Beep(1400, 70);
            }
        }

        DWORD now = GetTickCount();

        if (!g_targetLocked) {
            // ==================================================================
            // ESTADO 1: VARREDURA SUAVE & BUSCA DE ALVO
            // ==================================================================
            if (is_walking) {
                stop_walk_forward(g_hAionWnd);
                is_walking = FALSE;
            }

            // 1. Gira a câmera suavemente SEM segurar botão nenhum (elimina o erro "Selecione um alvo")
            for (int step = 0; step < 4; step++) {
                send_mouse_move(14, 0); // ~14px suaves por passo
                Sleep(12);
            }

            // 2. Aperta TAB com entrega híbrida para selecionar mob em frente
            send_game_key(g_hAionWnd, VK_TAB);
            Sleep(45); // Aguarda engine atualizar frame

            // 3. Checagem ultra-rápida de trava (<1ms)
            if (is_target_locked()) {
                g_targetLocked = TRUE;
                lost_counter = 0;
                combat_start = GetTickCount();
                log_debug("Alvo travado com sucesso!");
                Beep(1400, 70);
            }
            Sleep(25);
        } else {
            // ==================================================================
            // ESTADO 2: MOB TRAVADO! COMBATE AUTOMÁTICO & APROXIMAÇÃO
            // ==================================================================
            // Verifica se o mob ainda está vivo
            if (!is_target_locked()) {
                lost_counter++;
                // Se a barra de vida sumiu por 2 checagens seguidas (~200ms), mob morreu!
                if (lost_counter >= 2) {
                    g_targetLocked = FALSE;
                    lost_counter = 0;
                    if (is_walking) {
                        stop_walk_forward(g_hAionWnd);
                        is_walking = FALSE;
                    }
                    log_debug("Mob derrotado ou alvo perdido. Retornando para busca.");
                    Sleep(80);
                    continue;
                }
            } else {
                lost_counter = 0;
            }

            // 1. APROXIMAÇÃO / DISTÂNCIA:
            // Nos primeiros 1.4s após travar alvo (para fechar os ~20m até o alcance melee),
            // ou pulsos de 'W' a cada 2s para se manter colado no mob.
            if (now - combat_start < 1400) {
                if (!is_walking) {
                    start_walk_forward(g_hAionWnd);
                    is_walking = TRUE;
                }
            } else {
                if (((now / 800) % 3) == 0) {
                    if (!is_walking) {
                        start_walk_forward(g_hAionWnd);
                        is_walking = TRUE;
                    }
                } else {
                    if (is_walking) {
                        stop_walk_forward(g_hAionWnd);
                        is_walking = FALSE;
                    }
                }
            }

            // 2. AUTO-CURA [Q] (a cada 3.5s se habilitado - Slot 1 do usuário)
            if (g_autoHeal && (now - last_heal > 3500)) {
                send_game_key(g_hAionWnd, 'Q');
                last_heal = now;
                Sleep(35);
            }

            // 3. SKILL DE DANO [E] (a cada 2.0s - Slot 2 do usuário)
            if (now - last_skill_e > 2000) {
                send_game_key(g_hAionWnd, 'E');
                last_skill_e = now;
                Sleep(35);
            }

            // 4. SKILL SECUNDÁRIA [RMB] (a cada 1.4s - Slot 4 do usuário)
            if (now - last_rmb > 1400) {
                send_click_right();
                last_rmb = now;
                Sleep(35);
            }

            // 5. AUTO-ATAQUE BÁSICO CONTÍNUO [LMB] (Slot 3 do usuário)
            send_click_left();
            Sleep(80);
        }
    }

    if (is_walking) {
        stop_walk_forward(g_hAionWnd);
    }
    log_debug("bot_worker_thread: FINALIZADA");
    return 0;
}

// ==============================================================================
// RENDERIZAÇÃO GDI DO OVERLAY HUD
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

    // 1. Badge Logo: AION 2
    draw_pill(memDC, 8, 7, 68, 28, COLOR_BADGE_BG, COLOR_BADGE_BD, L"AION 2", COLOR_CYAN, g_hFontBold);

    // 2. Slot Status: ATIVO [F8] / PAUSADO [F8]
    if (g_botActive) {
        draw_pill(memDC, 82, 7, 138, 28, COLOR_ACTIVE_BG, COLOR_GREEN, L"COMBATE: ATIVO [F8]", COLOR_GREEN, g_hFontBold);
    } else {
        draw_pill(memDC, 82, 7, 138, 28, COLOR_SLOT_BG, COLOR_BORDER, L"PAUSADO [F8]", COLOR_RED, g_hFontBold);
    }

    // 3. Slot Target: BUSCANDO / TRAVADO
    if (!g_botActive) {
        draw_pill(memDC, 226, 7, 122, 28, COLOR_SLOT_BG, COLOR_BORDER, L"ALVO: ESPERA", COLOR_TEXT_DIM, g_hFontSmall);
    } else if (g_targetLocked) {
        draw_pill(memDC, 226, 7, 122, 28, COLOR_COMBAT_BG, COLOR_GREEN, L"⚔ MOB TRAVADO", COLOR_GREEN, g_hFontBold);
    } else {
        draw_pill(memDC, 226, 7, 122, 28, COLOR_SEARCH_BG, COLOR_AMBER, L"↻ BUSCANDO ALVO", COLOR_AMBER, g_hFontSmall);
    }

    // 4. Slot Cura Q: ON [F6] / OFF [F6]
    if (g_autoHeal) {
        draw_pill(memDC, 354, 7, 102, 28, COLOR_SLOT_BG, COLOR_BORDER, L"CURA [Q]: ON [F6]", COLOR_GREEN, g_hFontSmall);
    } else {
        draw_pill(memDC, 354, 7, 102, 28, COLOR_SLOT_BG, COLOR_BORDER, L"CURA [Q]: OFF", COLOR_TEXT_DIM, g_hFontSmall);
    }

    // 5. Slot Janela: AION2 [OK] / ALT-TAB / LIVRE [F9]
    if (!g_windowGuard) {
        draw_pill(memDC, 462, 7, 98, 28, COLOR_COMBAT_BG, COLOR_AMBER, L"LIVRE [F9]", COLOR_AMBER, g_hFontSmall);
    } else if (g_isForeground) {
        draw_pill(memDC, 462, 7, 98, 28, COLOR_SLOT_BG, COLOR_BORDER, L"AION2 [OK]", COLOR_CYAN, g_hFontSmall);
    } else {
        draw_pill(memDC, 462, 7, 98, 28, COLOR_SLOT_BG, COLOR_BORDER, L"ALT-TAB", COLOR_ORANGE, g_hFontSmall);
    }

    // 6. Botão Fechar [X]
    draw_pill(memDC, 566, 7, 24, 28, COLOR_SLOT_BG, COLOR_BORDER, L"✕", COLOR_TEXT_DIM, g_hFontBold);

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hWnd, &ps);
}

// ==============================================================================
// WNDPROC & INTERAÇÃO COM O MOUSE
// ==============================================================================
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            log_debug("WndProc: WM_CREATE recebido");
            g_hWnd = hWnd;
            HMODULE hUser32 = GetModuleHandleA("user32.dll");
            if (hUser32) {
                fn_SetLayeredWindowAttributes = (pfn_SetLayeredWindowAttributes)GetProcAddress(hUser32, "SetLayeredWindowAttributes");
                if (fn_SetLayeredWindowAttributes) {
                    fn_SetLayeredWindowAttributes(hWnd, 0, (BYTE)(255 * 0.93f), LWA_ALPHA);
                }
            }
            SetTimer(hWnd, 1, 50, NULL);
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

            // Botão Fechar [X]
            if (x >= 566 && x <= 592 && y >= 7 && y <= 35) {
                log_debug("Botao [X] clicado!");
                PostMessage(hWnd, WM_CLOSE, 0, 0);
                return 0;
            }

            // Clique no slot de Status (toggle bot)
            if (x >= 82 && x <= 220 && y >= 7 && y <= 35) {
                toggle_bot();
                return 0;
            }

            // Clique no slot de Cura (toggle heal)
            if (x >= 354 && x <= 456 && y >= 7 && y <= 35) {
                toggle_heal();
                return 0;
            }

            // Clique no slot de Janela (toggle guard)
            if (x >= 462 && x <= 560 && y >= 7 && y <= 35) {
                toggle_guard();
                return 0;
            }

            // Arrasta janela em qualquer outra parte
            ReleaseCapture();
            SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
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
            g_running = FALSE;
            stop_walk_forward(g_hAionWnd);
            PostQuitMessage(0);
            return 0;
        }

        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

// ==============================================================================
// ENTRY POINT (WINMAIN)
// ==============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SetUnhandledExceptionFilter(CrashHandler);
    log_debug("WinMain: Iniciado");

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
    int posX = sw - HUD_WIDTH - 25;
    int posY = sh - HUD_HEIGHT - 65;

    HWND hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        L"Aion2BotOverlay",
        L"AION 2 Combat Overlay",
        WS_POPUP,
        posX, posY, HUD_WIDTH, HUD_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) {
        log_debug("CreateWindowExW falhou!");
        return 1;
    }

    log_debug("Janela criada com sucesso");
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    HANDLE hThread = CreateThread(NULL, 0, bot_worker_thread, NULL, 0, NULL);
    if (hThread) {
        log_debug("CreateThread bem sucedido");
        CloseHandle(hThread);
    } else {
        log_debug("CreateThread falhou!");
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    log_debug("WinMain: Saindo do message loop");

    if (g_hFontBold) DeleteObject(g_hFontBold);
    if (g_hFontSmall) DeleteObject(g_hFontSmall);

    return (int)msg.wParam;
}

int main(int argc, char **argv) {
    return WinMain(GetModuleHandleW(NULL), NULL, GetCommandLineA(), SW_SHOW);
}
