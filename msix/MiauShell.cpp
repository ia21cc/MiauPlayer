// MiauShell.dll - comando de menu contextual (IExplorerCommand) para el paquete MSIX de MiauPlayer.
// Explorer le entrega TODA la seleccion de una vez (sin el limite de 15 elementos de los verbos clasicos) y este
// codigo la envia a MiauPlayer para anadirla a la lista de reproduccion (mismo protocolo que "MiauPlayer.exe /add").
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <new>
#include <string>
#include <vector>

// {5B0C6E2A-8D3F-4E71-9C54-2A7F1B6D8E30}  (debe coincidir con el manifiesto AppxManifest.xml)
static const CLSID CLSID_MiauAddCommand = { 0x5b0c6e2a, 0x8d3f, 0x4e71, { 0x9c, 0x54, 0x2a, 0x7f, 0x1b, 0x6d, 0x8e, 0x30 } };

static HMODULE g_mod = nullptr;
static volatile LONG g_objs = 0;
static volatile LONG g_locks = 0;

static const wchar_t* title() {
    const LANGID l = PRIMARYLANGID(GetUserDefaultUILanguage());
    if (l == LANG_SPANISH) return L"Añadir a la lista de MiauPlayer";
    if (l == LANG_PORTUGUESE) return L"Adicionar à lista do MiauPlayer";
    return L"Add to MiauPlayer playlist";
}

static std::wstring installDir() {
    wchar_t p[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(g_mod, p, MAX_PATH * 2);
    if (n == 0 || n >= MAX_PATH * 2) return L"";
    PathRemoveFileSpecW(p);
    return p;
}

static void sendPaths(HWND w, const std::vector<std::wstring>& paths, size_t from) {
    if (!w || from >= paths.size()) return;
    // en tandas, para no mandar un mensaje gigante si la seleccion es enorme
    const size_t kBatch = 400;
    for (size_t i = from; i < paths.size(); i += kBatch) {
        std::wstring joined;
        for (size_t k = i; k < paths.size() && k < i + kBatch; k++) joined += paths[k] + L"\n";
        COPYDATASTRUCT cd = {};
        cd.dwData = 2;   // 2 = anadir a la lista
        cd.cbData = (DWORD)((joined.size() + 1) * sizeof(wchar_t));
        cd.lpData = (PVOID)joined.c_str();
        DWORD_PTR res = 0;
        SendMessageTimeoutW(w, WM_COPYDATA, 0, (LPARAM)&cd, SMTO_ABORTIFHUNG, 20000, &res);
    }
}

class AddCommand : public IExplorerCommand, public IObjectWithSite {
public:
    AddCommand() : m_ref(1), m_site(nullptr) { InterlockedIncrement(&g_objs); }
    ~AddCommand() { if (m_site) m_site->Release(); InterlockedDecrement(&g_objs); }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == IID_IExplorerCommand) *ppv = static_cast<IExplorerCommand*>(this);
        else if (riid == IID_IObjectWithSite) *ppv = static_cast<IObjectWithSite*>(this);
        else return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
    IFACEMETHODIMP_(ULONG) Release() override {
        const ULONG r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }

    // IExplorerCommand
    IFACEMETHODIMP GetTitle(IShellItemArray*, PWSTR* name) override { return SHStrDupW(title(), name); }
    IFACEMETHODIMP GetIcon(IShellItemArray*, PWSTR* icon) override {
        const std::wstring d = installDir();
        if (d.empty()) { *icon = nullptr; return E_NOTIMPL; }
        return SHStrDupW((d + L"\\MiauPlayer.exe,0").c_str(), icon);
    }
    IFACEMETHODIMP GetToolTip(IShellItemArray*, PWSTR* tip) override { *tip = nullptr; return E_NOTIMPL; }
    IFACEMETHODIMP GetCanonicalName(GUID* g) override { *g = CLSID_MiauAddCommand; return S_OK; }
    IFACEMETHODIMP GetState(IShellItemArray*, BOOL, EXPCMDSTATE* st) override { *st = ECS_ENABLED; return S_OK; }
    IFACEMETHODIMP GetFlags(EXPCMDFLAGS* f) override { *f = ECF_DEFAULT; return S_OK; }
    IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** e) override { *e = nullptr; return E_NOTIMPL; }

    IFACEMETHODIMP Invoke(IShellItemArray* items, IBindCtx*) override {
        if (!items) return S_OK;
        DWORD n = 0;
        if (FAILED(items->GetCount(&n)) || n == 0) return S_OK;
        std::vector<std::wstring> paths;
        paths.reserve(n);
        for (DWORD i = 0; i < n; i++) {
            IShellItem* it = nullptr;
            if (FAILED(items->GetItemAt(i, &it)) || !it) continue;
            PWSTR p = nullptr;
            if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) { paths.push_back(p); CoTaskMemFree(p); }
            it->Release();
        }
        if (paths.empty()) return S_OK;

        HWND w = FindWindowW(L"MiauPlayer", nullptr);
        size_t from = 0;
        if (!w) {
            // MiauPlayer no esta abierto: se inicia con la primera pista y el resto se envia cuando su ventana exista
            const std::wstring dir = installDir();
            if (dir.empty()) return E_FAIL;
            const std::wstring exe = dir + L"\\MiauPlayer.exe";
            std::wstring cmd = L"\"" + exe + L"\" /add \"" + paths[0] + L"\"";
            STARTUPINFOW si = {};
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi = {};
            if (!CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi))
                return HRESULT_FROM_WIN32(GetLastError());
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            from = 1;
            for (int i = 0; i < 200 && !w; i++) {   // hasta ~10 s
                Sleep(50);
                w = FindWindowW(L"MiauPlayer", nullptr);
            }
        }
        sendPaths(w, paths, from);
        return S_OK;
    }

    // IObjectWithSite
    IFACEMETHODIMP SetSite(IUnknown* s) override {
        if (m_site) m_site->Release();
        m_site = s;
        if (m_site) m_site->AddRef();
        return S_OK;
    }
    IFACEMETHODIMP GetSite(REFIID riid, void** ppv) override {
        if (!m_site) { if (ppv) *ppv = nullptr; return E_FAIL; }
        return m_site->QueryInterface(riid, ppv);
    }

private:
    LONG m_ref;
    IUnknown* m_site;
};

class Factory : public IClassFactory {
public:
    Factory() : m_ref(1) { InterlockedIncrement(&g_objs); }
    ~Factory() { InterlockedDecrement(&g_objs); }
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) { *ppv = static_cast<IClassFactory*>(this); AddRef(); return S_OK; }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_ref); }
    IFACEMETHODIMP_(ULONG) Release() override {
        const ULONG r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }
    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        AddCommand* c = new (std::nothrow) AddCommand();
        if (!c) return E_OUTOFMEMORY;
        const HRESULT hr = c->QueryInterface(riid, ppv);
        c->Release();
        return hr;
    }
    IFACEMETHODIMP LockServer(BOOL lock) override {
        if (lock) InterlockedIncrement(&g_locks); else InterlockedDecrement(&g_locks);
        return S_OK;
    }
private:
    LONG m_ref;
};

extern "C" {
__declspec(dllexport) HRESULT STDAPICALLTYPE DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (clsid != CLSID_MiauAddCommand) return CLASS_E_CLASSNOTAVAILABLE;
    Factory* f = new (std::nothrow) Factory();
    if (!f) return E_OUTOFMEMORY;
    const HRESULT hr = f->QueryInterface(riid, ppv);
    f->Release();
    return hr;
}
__declspec(dllexport) HRESULT STDAPICALLTYPE DllCanUnloadNow() {
    return (g_objs == 0 && g_locks == 0) ? S_OK : S_FALSE;
}
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { g_mod = h; DisableThreadLibraryCalls(h); }
    return TRUE;
}
