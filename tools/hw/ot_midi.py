#!/usr/bin/env python3
"""Drive the Octatrack over CoreMIDI from the command line (macOS, no deps).

    python3 tools/hw/ot_midi.py list
    python3 tools/hw/ot_midi.py -p A cc  <ch> <cc> <val>      # e.g. -p A cc 1 40 64
    python3 tools/hw/ot_midi.py -p A note <ch> <note> [vel] [hold_s]
    python3 tools/hw/ot_midi.py -p A raw  B0 28 40             # hex bytes
    python3 tools/hw/ot_midi.py -p A start | stop
    python3 tools/hw/ot_midi.py -p A listen [seconds]          # dump incoming; clock (F8) and
                                                               # active sensing (FE) are counted, not printed

OT map (manual + docs/firmware/MIDI.md appendix A): FX2 params 1-6 = CC 40-45 on the
track's channel (SEND: 40 = -DEL, 41 = -VRB); FX1 = CC 34-39; level CC 7;
mute/solo CC 49/50; crossfader CC 48; sample trig = note 36+track;
chromatic = notes 72-96 (84 = unison).
"""
import ctypes, ctypes.util, sys, time

cm = ctypes.cdll.LoadLibrary('/System/Library/Frameworks/CoreMIDI.framework/CoreMIDI')
cf = ctypes.cdll.LoadLibrary('/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation')
UTF8 = 0x08000100
for f in (cm.MIDIGetNumberOfSources, cm.MIDIGetNumberOfDestinations):
    f.restype = ctypes.c_ulong
for f in (cm.MIDIGetSource, cm.MIDIGetDestination):
    f.restype = ctypes.c_uint32
cf.CFStringCreateWithCString.restype = ctypes.c_void_p
cf.CFStringGetCString.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_long, ctypes.c_uint32]
cm.MIDIObjectGetStringProperty.argtypes = [ctypes.c_uint32, ctypes.c_void_p, ctypes.c_void_p]
cm.MIDIClientCreate.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]
cm.MIDIOutputPortCreate.argtypes = [ctypes.c_uint32, ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]
cm.MIDIPacketListInit.restype = ctypes.c_void_p
cm.MIDIPacketListAdd.restype = ctypes.c_void_p
cm.MIDIPacketListAdd.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_void_p, ctypes.c_uint64, ctypes.c_ulong, ctypes.c_char_p]
cm.MIDISend.argtypes = [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
cm.MIDIInputPortCreate.argtypes = [ctypes.c_uint32, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]
cm.MIDIPortConnectSource.argtypes = [ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
READPROC = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)


def find(kind, name, wait=3.0):
    """The endpoint called `name`, or None. A new process can see an empty endpoint
    list for the first moments (seen 19 Oct 2026, one run in several), so poll."""
    t0 = time.time()
    while True:
        m = [ep for n, ep in endpoints(kind) if n == name]
        if m or time.time() - t0 >= wait:
            return m[0] if m else None
        time.sleep(0.1)


def listen(port_name, seconds):
    """Print incoming events from source `port_name` for `seconds`; F8 and FE are only counted."""
    client = ctypes.c_uint32(); inport = ctypes.c_uint32()
    cm.MIDIClientCreate(cfstr("octabam-in"), None, None, ctypes.byref(client))
    t0 = time.time()
    skipped = {0xF8: 0, 0xFE: 0}

    def cb(pktlist, _a, _b):
        n = ctypes.cast(pktlist, ctypes.POINTER(ctypes.c_uint32))[0]
        p = pktlist + 4
        for _ in range(n):
            length = ctypes.cast(p + 8, ctypes.POINTER(ctypes.c_uint16))[0]
            data = ctypes.string_at(p + 10, length)
            for b in data:
                if b in skipped:
                    skipped[b] += 1
            rest = bytes(b for b in data if b not in skipped)
            if rest:
                print(f"{time.time()-t0:7.3f}  {rest.hex(' ')}", flush=True)
            # packets are packed: 8-byte timestamp, 2-byte length, data. Padding each to
            # 4 bytes (as MIDIPacketNext does on x86) ran off the list after the first
            # packet on an arm64 Mac, 19 Oct 2026, and the rest of the burst printed nothing.
            p += 10 + length
    proc = READPROC(cb)
    cm.MIDIInputPortCreate(client, cfstr("in"), proc, None, ctypes.byref(inport))
    src = find("src", port_name)
    if src is None:
        sys.exit(f"no MIDI source named {port_name!r}")
    cm.MIDIPortConnectSource(inport, src, None)
    cf.CFRunLoopRunInMode.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_bool]
    mode = cfstr("kCFRunLoopDefaultMode")
    while time.time() - t0 < seconds:
        cf.CFRunLoopRunInMode(mode, 0.1, False)
    print(f"not printed: {skipped[0xF8]} clock (F8), {skipped[0xFE]} active sensing (FE)")


def cfstr(s):
    return cf.CFStringCreateWithCString(None, s.encode(), UTF8)


def name_of(ep):
    out = ctypes.c_void_p()
    cm.MIDIObjectGetStringProperty(ep, cfstr("name"), ctypes.byref(out))
    buf = ctypes.create_string_buffer(256)
    cf.CFStringGetCString(out, buf, 256, UTF8)
    return buf.value.decode()


def endpoints(kind):
    n, get = ((cm.MIDIGetNumberOfSources, cm.MIDIGetSource) if kind == "src"
              else (cm.MIDIGetNumberOfDestinations, cm.MIDIGetDestination))
    return [(name_of(get(i)), get(i)) for i in range(n())]


class Out:
    def __init__(self, port_name):
        self.client = ctypes.c_uint32(); self.port = ctypes.c_uint32()
        cm.MIDIClientCreate(cfstr("octabam"), None, None, ctypes.byref(self.client))
        cm.MIDIOutputPortCreate(self.client, cfstr("out"), ctypes.byref(self.port))
        self.dest = find("dst", port_name)
        if self.dest is None:
            sys.exit(f"no MIDI destination named {port_name!r}; have "
                     f"{[n for n, _ in endpoints('dst')]}")

    def send(self, data):
        buf = ctypes.create_string_buffer(1024)
        pkt = cm.MIDIPacketListInit(buf)
        cm.MIDIPacketListAdd(buf, 1024, pkt, 0, len(data), bytes(data))
        cm.MIDISend(self.port, self.dest, buf)


def main():
    a = sys.argv[1:]
    port = "A"
    if a and a[0] == "-p":
        port = a[1]; a = a[2:]
    if not a or a[0] == "list":
        print("sources:     ", [n for n, _ in endpoints("src")])
        print("destinations:", [n for n, _ in endpoints("dst")]); return
    cmd, args = a[0], a[1:]
    if cmd == "listen":
        listen(port, float(args[0]) if args else 10); return
    out = Out(port)
    if cmd == "cc":
        ch, cc, val = map(int, args)
        out.send([0xB0 | (ch - 1), cc, val])
    elif cmd == "note":
        ch, note = int(args[0]), int(args[1])
        vel = int(args[2]) if len(args) > 2 else 100
        hold = float(args[3]) if len(args) > 3 else 0.2
        out.send([0x90 | (ch - 1), note, vel]); time.sleep(hold)
        out.send([0x80 | (ch - 1), note, 0])
    elif cmd == "raw":
        out.send([int(x, 16) for x in args])
    elif cmd == "start":
        out.send([0xFA])
    elif cmd == "stop":
        out.send([0xFC])
    else:
        sys.exit(__doc__)
    time.sleep(0.05)


if __name__ == "__main__":
    main()
