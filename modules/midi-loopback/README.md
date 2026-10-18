# MIDI Loopback

Routes MIDI-track output into the Octatrack's own MIDI receiver, with optional
DIN/USB output. Use it to sequence audio-track controls without a loopback cable.

## Use

1. Open a MIDI track's **NOTE SETUP** with **FUNC + SRC**.
2. Set **A: CHAN**, then press A to apply the channel and open **ROUTING**.
3. Turn **A: DEST**:

| DEST | Output |
| --- | --- |
| EXT | DIN/USB only; the default. |
| INT | Internal receiver only. |
| BOTH | One internal copy plus DIN/USB output. |

Edits apply immediately. Press A, YES or NO to close. A track key closes
the window and selects that track. A page key closes the window first; press it
again to change page. CHAN shows **I** for INT
or **B** for BOTH. CHAN OFF disables sequencer output.

For example, MIDI T1 on channel 1 with INT and CC 34 can control audio T1's
FILTER BASE when that track receives channel 1 and audio CC reception is enabled.

## Routing behavior

- Covers sequencer/arp notes, live trig keys, preview, track CCs and LFOs,
  pitch bend, channel pressure, Part bank/program messages and STOP panic.
- Internal events reach audio listeners and the audio-mode auto-channel.
  They do not feed MIDI-track forwarding or recording back into the route.
- Audio receive channels, receive switches and note maps still determine the
  response. Normal DIN/USB input retains its native routing.
- Clock, transport, SysEx and project-wide program-change sync are unchanged.
  Audio-knob feedback and CC Direct Connect are separate paths.
- Changing DEST or replacing a Part releases held notes through their original
  routes. Internal queue overload can drop new events; BOTH still sends externally.

### Feedback and held notes

No selectable channel, auto-channel or audio/MIDI-mode combination lets an
internally delivered event retrigger a MIDI track, including its own source.
The internal receiver uses a private channel map that removes every MIDI-track
listener and the MIDI-mode auto-channel. Audio listeners and the audio-mode
auto-channel remain available; external DIN/USB input keeps its native map.
A MIDI cable or external echo device can return BOTH's external copy as fresh
external input: the internal guard cannot identify that physical round trip.
Use INT for internal-only control and avoid external echo loops.

Internal note-ons with no eligible audio listener are rejected before native
held-note bookkeeping. This prevents a held count for a note with no recipient. A negative-control
test confirms that disabling this guard creates such a count until the matching
release; it does not demonstrate a permanently stuck audible note. Route/Part changes also release
owned notes through their original destinations.

**Avoid overlapping internal and external notes on the same channel and pitch.**
The native receiver merges them: the first note-off ends the shared note even
if the other source still holds it. Tests of both arrival/release orders and
both note-off encodings find no residual held count after either release;
fresh notes work from both sources. This is an early-release limitation, not
a note stuck until the next message. Separate receive channels/pitches avoid
it. Independent sustain would require a separate receiver-ownership design;
this module does not change native voice ownership.

## Storage and composition

DEST is stored per MIDI track in each native Part. Part save/reload/clear,
copy/paste and project save/load carry it; fresh Parts use EXT. Playback uses
each MIDI track's applied Part, independently of retained audio voices and
the Part selected for editing.

Requires **MIDI Part State**. Combining it with MIDI Scenes needs a separate
producer bridge, which is not included in this contribution; that combination
is currently refused. KITS is not required.

### Returning to stock 1.40C

MIDI Part State changes the allowed-value count at `0x400d4082` from 1 to
`0x10`: the Part's MIDI SETUP flags byte accepts 0–15 instead of only zero.
DEST occupies bits 2–3 (INT = 4, BOTH = 8); EXT is zero. This uses an existing
Part field and does not enlarge the file format.

Stock 1.40C loads such a Part but clamps the entire flags byte to zero, in
**both working and saved Part copies**. INT and BOTH therefore become EXT:
MIDI-track output goes to the normal external destination. Saving the current
bank/project on stock writes that reset into its `.work` and `.strd` bank
files. Returning to Loopback cannot recover those saved routes. Loading alone
does not rewrite the other bank files; this is not a promise that later edits
or saves will preserve their flags.

Keep a separate project backup before returning to stock. The regression
executes candidate → stock → candidate native project loads and saves on a
virtual card: all 16 banks, four working plus four saved Parts, eight MIDI
tracks, all 256 input flag values, and explicit INT/BOTH in the saved bank.
This proves the cold disk-load/save behavior in the emulator, not retained-RAM
startup or a physical stock-firmware round trip.

[Implementation, provenance and verification](DEVELOPMENT.md).
