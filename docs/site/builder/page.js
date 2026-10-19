// The web builder page: pick a remix, give it your own OCTATRACK_OS1.40C
// zip, download the card image. Everything runs in this browser tab.
import { build } from "./builder.js";
import { makeEft } from "./eft-run.js";

const root = document.getElementById("octabam-builder");
const el = (tag, attrs = {}, ...kids) => {
  const n = Object.assign(document.createElement(tag), attrs);
  n.append(...kids);
  return n;
};
const here = (p) => new URL(p, import.meta.url);

async function main() {
  const manifest = await (await fetch(here("manifest.json"))).json();

  const pick = el("select", { id: "ob-remix" },
    ...manifest.remixes.map((r) => el("option", { value: r.name, textContent: r.title })));
  const file = el("input", { type: "file", id: "ob-file", accept: ".zip,.syx" });
  const go = el("button", { id: "ob-go", textContent: "Build", disabled: true });
  const status = el("p", { id: "ob-status", role: "status" });
  const links = el("p", { id: "ob-links" });
  const built = el("p", { className: "ob-meta" },
    `Patches built from commit ${manifest.commit.slice(0, 8)}, ${manifest.date}.`);

  root.replaceChildren(
    el("p", {}, el("label", { htmlFor: "ob-remix" }, "Remix: "), pick),
    el("p", {}, el("label", { htmlFor: "ob-file" }, "Your OCTATRACK_OS1.40C_dist.zip: "), file),
    el("p", {}, go), status, links, built);

  file.addEventListener("change", () => { go.disabled = !file.files.length; });

  go.addEventListener("click", async () => {
    go.disabled = true;
    links.replaceChildren();
    try {
      const meta = manifest.remixes.find((r) => r.name === pick.value);
      const bytes = new Uint8Array(await file.files[0].arrayBuffer());
      const patch = new Uint8Array(await (await fetch(here(meta.patch))).arrayBuffer());
      const createModule = (await import("./eft.mjs")).default;
      const r = await build({
        file: bytes, patch, meta, eft: makeEft(createModule),
        onStep: (s) => { status.textContent = s + "..."; },
      });
      status.textContent = "Done. Nothing was uploaded; the files below exist only in this tab.";
      const dl = (data, name, label) => el("a", {
        href: URL.createObjectURL(new Blob([data], { type: "application/octet-stream" })),
        download: name, textContent: label,
      });
      links.append(
        dl(r.bin, r.names.bin, `${r.names.bin} (card)`), " · ",
        dl(r.syx, r.names.syx, `${r.names.syx} (MIDI)`), " · ",
        dl(r.rmx, r.names.rmx, `${r.names.rmx} (REMIX SWITCH)`));
    } catch (e) {
      status.textContent = `Failed: ${e.message}`;
    } finally {
      go.disabled = !file.files.length;
    }
  });
}

main().catch((e) => { root.textContent = `The builder could not start: ${e.message}`; });
