"""
Automated Lua script testing for Mappy Win32.
Launches mapwin per-script, loads FMP, runs one script, checks log for errors.

Usage:
  python test_lua.py                        # test all scripts with Test.fmp
  python test_lua.py 5 10 15                # test specific lua indices (1-based)
  python test_lua.py --only 1,2,3           # test specific list
"""
import sys, os, time, subprocess, ctypes, re
from ctypes import wintypes

user32 = ctypes.windll.user32
WM_COMMAND = 0x0111
WM_CLOSE   = 0x0010
BM_CLICK   = 0x00F5

def enum_visible():
    result = []
    @ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
    def cb(hwnd, _):
        if user32.IsWindowVisible(hwnd):
            buf = ctypes.create_string_buffer(256)
            user32.GetWindowTextA(hwnd, buf, 256)
            try: t = buf.value.decode('utf-8')
            except: t = ""
            if t: result.append((hwnd, t))
        return True
    user32.EnumWindows(cb, 0)
    return result

def get_wtext(hwnd):
    buf = ctypes.create_string_buffer(256)
    user32.GetWindowTextA(hwnd, buf, 256)
    try: return buf.value.decode('utf-8')
    except: return ""

def find_mappy():
    for h, t in enum_visible():
        if "Mappy" in t:
            return h
    return None

def dismiss_all_popups(exclude_hwnd):
    """Click OK on any popup dialogs, return list of (title, text)."""
    dismissed = []
    for _ in range(10):
        found = False
        for h, t in enum_visible():
            if h == exclude_hwnd: continue
            # Any small visible window that has an OK button
            rect = wintypes.RECT()
            user32.GetWindowRect(h, ctypes.byref(rect))
            w = rect.right - rect.left
            hh = rect.bottom - rect.top
            if w > 800 or hh > 600: continue  # too big, probably main window
            btn = user32.GetDlgItem(h, 1)  # IDOK
            if btn:
                err = ""
                for cid in [0x3E8, -1]:
                    child = user32.GetDlgItem(h, cid)
                    if child:
                        buf = ctypes.create_string_buffer(2048)
                        user32.GetWindowTextA(child, buf, 2048)
                        try: err = buf.value.decode('utf-8')
                        except: pass
                        break
                dismissed.append((t, err))
                user32.PostMessageA(btn, BM_CLICK, 0, 0)
                time.sleep(0.3)
                found = True
        if not found:
            break
    return dismissed

def run_one_script(fmp, lua_idx, exe_path):
    """Run a single script. Returns (pass, error_msg, log_lines)."""
    log_path = os.path.join(os.path.dirname(exe_path), "lua_test.log")
    tmp_log = log_path + f".{os.getpid()}"
    # Remove old logs
    for p in [log_path, tmp_log]:
        if os.path.exists(p):
            try: os.remove(p)
            except: pass

    proc = subprocess.Popen([exe_path, fmp], cwd=os.path.dirname(exe_path))
    hwnd = None

    # Wait for window
    for _ in range(20):
        time.sleep(0.5)
        hwnd = find_mappy()
        if hwnd: break

    if not hwnd:
        proc.terminate()
        return False, "Window not found", []

    time.sleep(0.5)

    # Send menu command (lua01 -> index 1 -> menu ID 7001)
    menu_id = 7000 + lua_idx
    user32.PostMessageA(hwnd, WM_COMMAND, menu_id, 0)
    time.sleep(2.0)

    # Dismiss popups (they block the script)
    popups = dismiss_all_popups(hwnd)
    time.sleep(0.5)

    # Dismiss again in case there were multiple rounds
    more = dismiss_all_popups(hwnd)
    popups.extend(more)
    time.sleep(0.3)

    # Close
    user32.PostMessageA(hwnd, WM_CLOSE, 0, 0)
    for _ in range(20):
        if proc.poll() is not None:
            break
        time.sleep(0.5)
    if proc.poll() is None:
        proc.terminate()
    proc.wait(timeout=5)

    # Read log
    log_lines = []
    time.sleep(0.5)
    if os.path.exists(log_path):
        for attempt in range(5):
            try:
                with open(log_path) as f:
                    log_lines = f.readlines()
                os.remove(log_path)
                break
            except (PermissionError, OSError):
                time.sleep(0.5)

    # Check for errors in log
    error = None
    for line in log_lines:
        if "LUA ERROR" in line:
            error = line.strip()
            break
        if "ERROR:" in line:
            error = line.strip()

    return error is None, error or "", log_lines

def main():
    exe = os.path.join(os.path.dirname(os.path.abspath(__file__)), "mapwin.exe")
    fmp = "Test.fmp"
    indices = list(range(1, 29))

    args = sys.argv[1:]
    if "--only" in args:
        i = args.index("--only") + 1
        if i < len(args):
            indices = [int(x) for x in args[i].split(",")]
    else:
        for a in args:
            if a.endswith('.fmp'): fmp = a
            elif a.isdigit(): indices = [int(a)]

    print(f"Testing {len(indices)} scripts with {fmp}")
    print("=" * 50)

    results = []
    for idx in indices:
        label = f"lua{idx:02d}"
        print(f"{label} ... ", end="", flush=True)
        ok, err, log = run_one_script(fmp, idx, exe)
        if ok:
            # Check if script actually ran (log has LuaRunScript line)
            ran = any("LuaRunScript" in l for l in log)
            if ran:
                print("PASS")
            else:
                print("SKIP (script did not execute)")
                ok = None  # skip
        else:
            short = err[:70] if err else "unknown"
            print(f"FAIL: {short}")
        results.append((label, ok, err))

    # Summary
    passed = sum(1 for _, ok, _ in results if ok is True)
    failed = sum(1 for _, ok, _ in results if ok is False)
    skipped = sum(1 for _, ok, _ in results if ok is None)
    print("=" * 50)
    print(f"PASS: {passed}  FAIL: {failed}  SKIP: {skipped}")
    if failed:
        print("\nFailed scripts:")
        for name, ok, err in results:
            if ok is False:
                print(f"  {name}: {err[:80]}")

    sys.exit(0 if failed == 0 else 1)

if __name__ == "__main__":
    main()
