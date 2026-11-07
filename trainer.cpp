// Moon Sweepers trainer — unlocks the game's built-in developer menu (Pause > Settings > Dev).
// Works by patching two checks in GameAssembly.dll in memory; nothing on disk is modified.
// Usage:  MoonSweepersTrainer.exe          interactive menu
//         MoonSweepersTrainer.exe on|off   apply / remove the patches and exit
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <conio.h>
#include <cstdio>
#include <cstring>
#include <cstdint>

struct Patch {
    const char* name;
    uintptr_t rva;          // offset inside GameAssembly.dll (Steam build 26317497)
    uint8_t original[5];
    uint8_t patched[5];
    size_t len;
};

static Patch g_patches[] = {
    // MoonDevTools.get_Enabled(): "sub rsp,28h; cmp ..." -> "mov al,1; ret"
    { "MoonDevTools.get_Enabled", 0x469b10, {0x48, 0x83, 0xEC}, {0xB0, 0x01, 0xC3}, 3 },
    // MoonPauseMenu.Build(): inlined Debug.isDebugBuild call -> "mov al,1; nop x3"
    { "MoonPauseMenu.Build (page Dev)", 0x46d6cb, {0xE8, 0x80, 0x14, 0x5E, 0x01}, {0xB0, 0x01, 0x90, 0x90, 0x90}, 5 },
};

static DWORD FindPid(const wchar_t* exe) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe{ sizeof(pe) };
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
        if (!_wcsicmp(pe.szExeFile, exe)) { pid = pe.th32ProcessID; break; }
    CloseHandle(snap);
    return pid;
}

static uintptr_t FindModule(HANDLE hp, const wchar_t* name) {
    HMODULE mods[1024]; DWORD need = 0;
    if (!EnumProcessModulesEx(hp, mods, sizeof(mods), &need, LIST_MODULES_64BIT)) return 0;
    for (DWORD i = 0; i < need / sizeof(HMODULE); i++) {
        wchar_t n[MAX_PATH];
        if (GetModuleBaseNameW(hp, mods[i], n, MAX_PATH) && !_wcsicmp(n, name)) return (uintptr_t)mods[i];
    }
    return 0;
}

enum State { ORIGINAL, PATCHED, UNKNOWN };

static State Read(HANDLE hp, uintptr_t base, const Patch& p) {
    uint8_t cur[5] = {};
    SIZE_T got = 0;
    if (!ReadProcessMemory(hp, (void*)(base + p.rva), cur, p.len, &got) || got != p.len) return UNKNOWN;
    if (!memcmp(cur, p.original, p.len)) return ORIGINAL;
    if (!memcmp(cur, p.patched, p.len)) return PATCHED;
    return UNKNOWN;
}

static bool Write(HANDLE hp, uintptr_t base, const Patch& p, const uint8_t* bytes) {
    void* at = (void*)(base + p.rva);
    DWORD old = 0;
    if (!VirtualProtectEx(hp, at, p.len, PAGE_EXECUTE_READWRITE, &old)) return false;
    SIZE_T done = 0;
    bool ok = WriteProcessMemory(hp, at, bytes, p.len, &done) && done == p.len;
    VirtualProtectEx(hp, at, p.len, old, &old);
    FlushInstructionCache(hp, at, p.len);
    return ok;
}

// Applies (enable=true) or removes all patches. Refuses if any byte pattern is unexpected (game updated).
static bool SetAll(HANDLE hp, uintptr_t base, bool enable) {
    for (auto& p : g_patches) {
        if (Read(hp, base, p) == UNKNOWN) {
            printf("  ! %s : octets inattendus a GameAssembly+0x%llx. Le jeu a sans doute ete mis a jour ;\n"
                   "    rien n'a ete modifie.\n", p.name, (unsigned long long)p.rva);
            return false;
        }
    }
    for (auto& p : g_patches) {
        if (!Write(hp, base, p, enable ? p.patched : p.original)) {
            printf("  ! ecriture impossible pour %s (erreur %lu)\n", p.name, GetLastError());
            return false;
        }
    }
    return true;
}

static void PrintStatus(HANDLE hp, uintptr_t base) {
    for (auto& p : g_patches) {
        State s = Read(hp, base, p);
        printf("  %-32s %s\n", p.name, s == PATCHED ? "ACTIF" : s == ORIGINAL ? "inactif" : "?? (version inconnue)");
    }
}

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleA("Moon Sweepers Trainer");
    DWORD pid = FindPid(L"MoonSweepers.exe");
    if (!pid) { printf("Lance Moon Sweepers d'abord.\n"); if (argc < 2) _getch(); return 2; }
    HANDLE hp = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!hp) { printf("Impossible d'ouvrir le processus (erreur %lu).\n", GetLastError()); return 3; }
    uintptr_t base = FindModule(hp, L"GameAssembly.dll");
    if (!base) { printf("GameAssembly.dll introuvable (jeu encore en chargement ?).\n"); return 4; }

    if (argc >= 2) {
        bool on = !_stricmp(argv[1], "on");
        bool ok = SetAll(hp, base, on);
        PrintStatus(hp, base);
        CloseHandle(hp);
        return ok ? 0 : 5;
    }

    printf("=== Moon Sweepers Trainer ===  (pid %lu, GameAssembly @ %p)\n\n", pid, (void*)base);
    for (;;) {
        PrintStatus(hp, base);
        printf("\n  [1] Activer le menu dev   [2] Desactiver   [Q] Quitter (desactive)\n> ");
        int c = _getch(); printf("%c\n", c);
        if (c == '1') {
            if (SetAll(hp, base, true))
                printf("\nOK. Ouvre Pause > Reglages : un onglet \"Dev\" est disponible.\n"
                       "Si l'onglet n'apparait pas, retourne au menu principal puis relance ta partie.\n\n");
        } else if (c == '2') {
            if (SetAll(hp, base, false)) printf("\nPatches retires.\n\n");
        } else if (c == 'q' || c == 'Q') {
            if (FindPid(L"MoonSweepers.exe") == pid) SetAll(hp, base, false);
            break;
        }
    }
    CloseHandle(hp);
    return 0;
}
