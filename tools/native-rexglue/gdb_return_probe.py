"""Read-only x86-64 return probes without GDB thread-scoped finish breakpoints.

FinishBreakpoint hit GDB breakpoint.c:9182, thr != nullptr, while ACV
worker threads exited. These probes match the native return PC, thread ptid
and stack pointer explicitly; GDB never owns a thread-scoped breakpoint.
Source only at a native function's first instruction (before its prologue).
"""
import gdb

_sites = {}
_pending_limit = 2048

def _register(name):
    return int(gdb.selected_frame().read_register(name))

def _key(stack):
    thread = gdb.selected_thread()
    return (thread.inferior.num, thread.ptid, stack)

class _Site(gdb.Breakpoint):
    def __init__(self, address):
        super().__init__(f"*{address:#x}", internal=True)
        self.outstanding = {}
    def stop(self):
        probes = self.outstanding.pop(_key(_register("rsp")), [])
        for probe in probes:
            probe.stop()
        if not self.outstanding:
            self.enabled = False
        return False

class ReturnProbe:
    """Subclass stop(); creation captures the current native entry return."""
    def __init__(self, *, internal=True):
        # ACV Linux is x86-64; do not infer a PPC guest return from this stack.
        frame = gdb.selected_frame()
        if frame.architecture().name() != "i386:x86-64":
            raise RuntimeError("ReturnProbe requires an x86-64 native entry")
        stack = _register("rsp")
        address = int.from_bytes(gdb.selected_inferior().read_memory(stack, 8), "little")
        count = sum(len(probes) for site in _sites.values() for probes in site.outstanding.values())
        if count >= _pending_limit:
            raise RuntimeError("ReturnProbe pending limit exceeded")
        site = _sites.get(address)
        if site is None:
            site = _sites[address] = _Site(address)
        key = _key(stack + 8)
        site.outstanding.setdefault(key, []).append(self)
        site.enabled = True
    def stop(self):
        return False

def _thread_exited(event):
    thread = event.inferior_thread
    prefix = (thread.inferior.num, thread.ptid)
    for site in _sites.values():
        for key in list(site.outstanding):
            if key[:2] == prefix:
                del site.outstanding[key]
        if not site.outstanding:
            site.enabled = False

gdb.events.thread_exited.connect(_thread_exited)
