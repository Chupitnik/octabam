# Build and Install

Builds one of the three official remixes ([octabam-core](remix-core.md),
[octabam-inserts](remix-inserts.md), [octabam-bus](remix-bus.md)) from your
own copy of Octatrack OS 1.40C, in this browser tab. The OS file is read
locally and nothing is uploaded.

!!! tip "Back up your projects"
    Back up your CF card before installing, or start new projects after
    installing. Remixes with KITS write `kits.work` into each project they
    load, and some change effect parameter layouts or default track
    assignments. A project created on the unit after installation needs no
    preparation; for older projects see the preparation steps on the
    [remix page](choosing.md).

## What you need

- `OCTATRACK_OS1.40C_dist.zip` from Elektron's Octatrack MKI or MKII
  download page, or the `OCTATRACK_OS1.40C.syx` inside it.
- A current desktop browser. Tested: Chrome.
- A CompactFlash card in the unit for the card install, or a 5-pin DIN MIDI
  interface into the Octatrack's MIDI IN for the MIDI install.

## Build

<div id="octabam-builder">This page needs JavaScript.</div>
<script type="module" src="builder/page.js"></script>

The build takes a few seconds. The page offers three files:

| file | use |
|---|---|
| `OCTATRACK_OCTABAM<tag>.bin` | card install: copy to the root of the CF card, then PROJECT → SYSTEM → OS UPGRADE. |
| `OCTATRACK_OS1.40C_OCTABAM<tag>.syx` | DIN MIDI install. |
| `<REMIX> <tag>.RMX` | REMIX SWITCH: copy to `/BRAIN/REMIXES/` on the card. |

The files contain Elektron's OS, so keep them to yourself.

## Install the image

### From the CF card

Use this method when the Octatrack boots normally:

1. On the unit, select **PROJECT → SYSTEM → USB DISK MODE → YES**.
2. Copy the `.bin` file to the root of the mounted CF card, not into a
   folder.
3. Eject the card from the computer, then leave USB DISK MODE on the unit.
   This ensures the copy has finished writing.
4. Select **PROJECT → SYSTEM → OS UPGRADE → YES**, then confirm.
5. Wait for the unit to finish and restart. Power-cycle it once more before
   testing; an OS upgrade does not clear DSP RAM.

### Over DIN MIDI

Use this method if the unit cannot boot far enough to use USB DISK MODE, or
if you prefer a DIN MIDI transfer:

1. Connect the MIDI interface's **MIDI OUT** to the Octatrack's **MIDI IN**.
2. Power off. Hold **FUNC** while powering on to open the STARTUP MENU.
3. Press **TRIG 3** for MIDI UPGRADE and wait for “READY TO RECEIVE MIDI
   UPGRADE”.
4. Send the `.syx` file with a SysEx app. Do not merge other MIDI data or
   clock into the upgrade port. If the unit reports a checksum error,
   re-enter the Startup Menu and resend at a slower rate.
5. Wait through PREPARING FLASH and UPDATING FLASH. Do not power off or
   disconnect the cable during either phase.
6. Let the unit finish booting, then power-cycle once more.

## Confirm the installation

Open **SYSTEM STATUS → OS VERSION** or check the boot screen. It shows
`OCTABAM` followed by the build tag in the file name. If it shows `1.40C`,
the stock OS is still installed.

To go back to stock, install Elektron's stock `.bin` from the CF card with
the same steps, or see [Safety and recovery](safety.md).

## How it works

The page holds a patch for each remix: octabam's bytes, plus instructions to
copy ranges from your 1.40C. It applies the patch to the MAIN OS section of
your file, rebuilds the firmware container with Elektron's container tool
compiled to WebAssembly, and wraps it for the card. It refuses a file whose
MAIN OS is not the 1.40C the patches were made against.

The output is byte-identical to `make image` for the same commit and build
tag. The page shows the commit the patches were built from. To build from
source or choose modules, see the
[build guide](https://github.com/sambanks/octabam/blob/main/docs/guide/BUILDING.md)
and [Create your own remix](own-remix.md).
