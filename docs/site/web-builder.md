# Build in the Browser

Builds one of the three official remixes ([octabam-core](remix-core.md),
[octabam-inserts](remix-inserts.md), [octabam-bus](remix-bus.md)) from your
own copy of Octatrack OS 1.40C, in this browser tab. The OS file is read
locally and nothing is uploaded.

!!! warning "Read before flashing"
    octabam is community software, not supported by Elektron. A failed flash
    can leave the unit unable to boot until stock OS is reinstalled. Follow
    [Safety and recovery](safety.md) before proceeding.

    Do not share the files this page produces. They contain Elektron's OS.

## What you need

- `OCTATRACK_OS1.40C_dist.zip` from Elektron's Octatrack MKI or MKII
  download page, or the `OCTATRACK_OS1.40C.syx` inside it.
- A current desktop browser. Tested: Chrome.

## Build

<div id="octabam-builder">This page needs JavaScript.</div>
<script type="module" src="builder/page.js"></script>

The build takes a few seconds. The page offers three files:

| file | use |
|---|---|
| `OCTATRACK_OCTABAM<n>.bin` | card install: copy to the root of the CF card, then PROJECT → SYSTEM → OS UPGRADE. |
| `OCTATRACK_OS1.40C_OCTABAM<n>.syx` | DIN MIDI install, and recovery. |
| `<REMIX> <n>.RMX` | REMIX SWITCH: copy to `/BRAIN/REMIXES/` on the card. |

Install steps are in [Build and Install](build-flash.md#4-install-the-image).

## How it works

The page holds a patch for each remix: octabam's bytes, plus instructions to
copy ranges from your 1.40C. It applies the patch to the MAIN OS section of
your file, rebuilds the firmware container with Elektron's container tool
compiled to WebAssembly, and wraps it for the card. It refuses a file whose
MAIN OS is not the 1.40C the patches were made against.

The output is byte-identical to `make image` for the same commit and build
number. The page shows the commit the patches were built from. To build from
source or choose modules, see [Build and Install](build-flash.md).
