# About

## Why octabam exists

!!! note "To be written"
    The author's own account goes here: what started the project, what the
    Octatrack is to them, and what they want it to become. Suggested
    headings, to keep or replace:

    - Where it started
    - What was missing on the unit
    - What changed when other people's work came in
    - What it is for

## What it is

octabam is an unofficial community remixer for the Elektron Octatrack's
operating system. It is not affiliated with Elektron. A **module** adds one
feature; a **remix** is a named set of modules built into one image from
your own copy of OS 1.40C. The [three official remixes](choosing.md) cover
the common choices, and [Create your own remix](own-remix.md) covers
the rest.

## How it is made

- Every module is contributed by its author and credited on its page and in
  [Credits](reference/credits.md).
- A module ported from an existing mod is checked byte for byte against the
  author's own build.
- The build refuses to combine modules that claim the same memory, hook or
  effect id, and names the clash.
- Images are derived from your own OS 1.40C on your own computer, in the
  browser or from source. The built file contains Elektron's OS and stays
  with you.
- Source, issues and pull requests are in the
  [repository](https://github.com/sambanks/octabam). The licence is MIT for
  the repository's own code and documentation; Elektron's firmware is not
  distributed.

## Status and testing

Hardware testing varies by feature and remix. Emulator checks cover the
code under the port. Each [remix page](choosing.md) lists where its image
has run on a unit.

## Get involved

[GitHub Issues](https://github.com/sambanks/octabam/issues) take bug
reports, module proposals and hardware test reports. Pull requests are
accepted for a module, a port of an existing mod, a fix, or a doc
correction; [CONTRIBUTING](https://github.com/sambanks/octabam/blob/main/CONTRIBUTING.md)
has the rules.
