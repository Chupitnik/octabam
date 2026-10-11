/* BRAIN -- the brain's run-time layers on the unit (docs/proposals/BRAIN.md).
 *
 * This increment: card-layer FX page defaults, read-only. At each project
 * load (and the power-up's bank load) brain_load() puts every effect's
 * twelve descriptor defaults back to the image's values, reads
 * /BRAIN/card.work (or card.strd, by the pair rule of section 6.2) and
 * writes each valid default record over its effect's descriptor bytes
 * (P+0x5e). The choosers and the new-part initialiser read those bytes when
 * they run (tools/verify/verify_descdefaults.py).
 *
 * A record with a mode writes into MODE DEFAULTS' view table (its
 * MODEDEF_TABLE, 0 when that module is not in the remix): the values of the
 * slots the mode's view already re-defaults. The table is sparse and keeps
 * its size, so a slot the view does not list is skipped and counted.
 *
 * The table brain_fx[] (one entry per effect in the image: store id, fx id,
 * twelve keys, layout hash) is generated per remix (manifest.py fx_inc).
 */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

struct fx_ent {			/* 36 bytes, as fx_inc() emits it */
	u32 layout;
	const char *id;
	u16 keys[12];
	u8 fx_id;
	u8 id_len;
	u16 pad;
};

extern const struct fx_ent brain_fx[];
extern const u32 brain_fx_n;
/* id, mode slot, nviews, then per view: mode, npairs, (slot, value)*;
 * 0xff ends it (modules/mode-defaults/manifest.py table_inc) */
extern u8 *const brain_modedef;		/* MODEDEF_TABLE or 0 (hooks.s) */
#define MODEDEF_TABLE brain_modedef

#define F_OPEN   ((int (*)(void *, const char *, const char *, void *, u32))0x40016864)
#define F_READ   ((int (*)(void *, void *, u32))0x40016564)
#define F_CLOSE  ((int (*)(void *))0x4001677c)
#define FX2_IDS  ((u8 **)0x400d5fdc)
#define FX1_IDS  ((u8 **)0x400d5f58)

#define MAX_FX    32
#define BUF_LEN   16384
#define HDR       32
#define REC_HDR   20
#define K_DEFAULT 2
#define T_BYTE    5
#define NO_MODE   0xffff

/* The platform loads a unit's image and leaves its .bss as the boot left
 * it (tools/remix/platform_build.py): state read before BRAIN first writes
 * it lives in .data. */
#define DATA __attribute__((section(".data")))

/* counters, read by the gates (tools/verify/verify_brain.py); all but
 * C_LOADS describe the last brain_load() */
enum { C_LOADS, C_STATE, C_RECORDS, C_APPLIED, C_VALUES, C_SKIP_ID, C_SKIP_LAYOUT,
       C_SKIP_MODE, C_SKIP_CRC, C_SKIP_DUP, C_SKIP_VALUE, C_SKIP_SLOT, C_N };
/* C_SKIP_MODE: a mode record with no MODE DEFAULTS or no view for the mode;
 * C_SKIP_SLOT: a mode value for a slot the view does not list */
/* C_STATE: 0 fresh, 1 card.work, 2 recovered from card.strd, 3 damaged */
u32 brain_counts[C_N] DATA;

static u8 shadow[MAX_FX][2][12];
#define MAX_TABLE 1024
static u8 table_shadow[MAX_TABLE];
static u32 table_len DATA;
static u8 *desc[MAX_FX][2];
static u32 snapped DATA;

static u8 fobj[24];
static u8 iob[4096];
static u8 buf[BUF_LEN];

static u32 be32(const u8 *p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; }
static u32 be16(const u8 *p) { return ((u32)p[0] << 8) | p[1]; }
static u32 align4(u32 n) { return (n + 3) & ~3u; }

static u32 crc32(const u8 *p, u32 n)
{
	u32 c = 0xffffffffu;
	while (n--) {
		c ^= *p++;
		for (int k = 0; k < 8; k++)
			c = (c >> 1) ^ (0xedb88320u & -(c & 1));
	}
	return ~c;
}

/* The descriptor of entry i in one id table, or 0: a table slot whose
 * descriptor carries another id (NONE, or an effect on one chooser only). */
static u8 *find(const u8 *const *table, u32 id)
{
	u8 *p = (u8 *)table[id];
	return (p && be32(p) == id) ? p : 0;
}

static void snapshot(void)
{
	for (u32 i = 0; i < brain_fx_n && i < MAX_FX; i++) {
		desc[i][0] = find((const u8 *const *)FX2_IDS, brain_fx[i].fx_id);
		desc[i][1] = find((const u8 *const *)FX1_IDS, brain_fx[i].fx_id);
		if (desc[i][1] == desc[i][0])
			desc[i][1] = 0;
		for (int t = 0; t < 2; t++)
			if (desc[i][t])
				for (int s = 0; s < 12; s++)
					shadow[i][t][s] = desc[i][t][0x5e + s];
	}
	if (MODEDEF_TABLE) {
		u32 at = 0;
		while (at < MAX_TABLE && MODEDEF_TABLE[at] != 0xff) {
			u32 nviews = MODEDEF_TABLE[at + 2];
			at += 3;
			for (u32 v = 0; v < nviews; v++)
				at += 2 + 2 * MODEDEF_TABLE[at + 1];
		}
		table_len = at < MAX_TABLE ? at : 0;
		for (u32 k = 0; k < table_len; k++)
			table_shadow[k] = MODEDEF_TABLE[k];
	}
	snapped = 1;
}

/* The (slot, value) pairs of MODE DEFAULTS' view for (fx id, mode): a
 * pointer to the first pair and its count, or 0. */
static u8 *view_of(u32 fx_id, u32 mode, u32 *npairs)
{
	if (!MODEDEF_TABLE || !table_len)
		return 0;
	for (u32 at = 0; at < table_len;) {
		u32 id = MODEDEF_TABLE[at], nviews = MODEDEF_TABLE[at + 2];
		at += 3;
		for (u32 v = 0; v < nviews; v++) {
			u32 n = MODEDEF_TABLE[at + 1];
			if (id == fx_id && MODEDEF_TABLE[at] == mode) {
				*npairs = n;
				return MODEDEF_TABLE + at + 2;
			}
			at += 2 + 2 * n;
		}
	}
	return 0;
}

static void restore(void)
{
	for (u32 i = 0; i < brain_fx_n && i < MAX_FX; i++)
		for (int t = 0; t < 2; t++)
			if (desc[i][t])
				for (int s = 0; s < 12; s++)
					desc[i][t][0x5e + s] = shadow[i][t][s];
	for (u32 k = 0; k < table_len; k++)
		MODEDEF_TABLE[k] = table_shadow[k];
}

/* Read one file whole into buf: its length; 0 when absent; 1 (never a
 * valid length) when present but too long or short to read. */
static u32 slurp(const char *path)
{
	u32 n = 1;
	if (F_OPEN(fobj, path, "r", iob, sizeof iob) < 0)
		return 0;
	if (F_READ(fobj, buf, HDR) == 1) {
		u32 total = be32(buf + 12);
		if (total >= HDR && total <= BUF_LEN
		    && (total == HDR || F_READ(fobj, buf + HDR, total - HDR) == 1))
			n = total;
	}
	F_CLOSE(fobj);
	return n;
}

/* BRAIN.md section 7.1/7.2: 1 when buf[0..n) is a valid file. */
static int valid(u32 n)
{
	if (n < HDR || be32(buf) != 0x4252414eu || be16(buf + 4) != HDR || buf[6] != 1
	    || be32(buf + 12) != n || crc32(buf + 20, n - 20) != be32(buf + 16))
		return 0;
	u32 at = HDR, count = be16(buf + 10);
	for (u32 r = 0; r < count; r++) {
		if (at + REC_HDR > n)
			return 0;
		u32 idlen = buf[at + 1], plen = be32(buf + at + 8), size = be32(buf + at + 16);
		if (idlen < 1 || idlen > 63 || size != REC_HDR + align4(idlen) + align4(plen) || at + size > n)
			return 0;
		at += size;
	}
	return at == n;
}

static int same_id(const u8 *a, u32 alen, const char *b, u32 blen)
{
	if (alen != blen)
		return 0;
	for (u32 k = 0; k < alen; k++)
		if (a[k] != (u8)b[k])
			return 0;
	return 1;
}

/* A default record for target 0: its entry index, or -1. */
static int entry_of(const u8 *rec)
{
	u32 idlen = rec[1];
	for (u32 i = 0; i < brain_fx_n && i < MAX_FX; i++)
		if (same_id(rec + REC_HDR, idlen, brain_fx[i].id, brain_fx[i].id_len))
			return (int)i;
	return -1;
}

static void apply_record(const u8 *rec, int e)
{
	const struct fx_ent *f = &brain_fx[e];
	u32 idlen = rec[1], plen = be32(rec + 8);
	const u8 *pay = rec + REC_HDR + align4(idlen);
	if (be16(rec + 2) != 1 || plen < 8)		/* schema major 1 */
		return;
	u32 mode = be16(pay + 2), npairs = 0;
	u8 *view = 0;
	if (be16(pay) != 0) {
		brain_counts[C_SKIP_MODE]++;
		return;
	}
	if (be32(pay + 4) != f->layout) {
		brain_counts[C_SKIP_LAYOUT]++;
		return;
	}
	if (mode != NO_MODE && !(view = view_of(f->fx_id, mode, &npairs))) {
		brain_counts[C_SKIP_MODE]++;
		return;
	}
	brain_counts[C_APPLIED]++;
	for (u32 at = 8; at + 4 <= plen;) {
		u32 key = be16(pay + at), type = pay[at + 2], n = pay[at + 3], head = 4;
		if (n == 255) {
			if (at + 8 > plen)
				break;
			n = be32(pay + at + 4);
			head = 8;
		}
		if (at + head + align4(n) > plen)
			break;
		if (type == T_BYTE && n == 1 && key && view) {
			u8 v = pay[at + head];
			for (int s = 0; s < 12; s++) {
				if (f->keys[s] != key)
					continue;
				u8 *p = desc[e][0] ? desc[e][0] : desc[e][1];
				u32 k = 0;
				while (k < npairs && view[2 * k] != s)
					k++;
				if (k == npairs)
					brain_counts[C_SKIP_SLOT]++;
				else if (!p || v >= be32(p + 0x9a + 4 * s))
					brain_counts[C_SKIP_VALUE]++;
				else {
					view[2 * k + 1] = v;
					brain_counts[C_VALUES]++;
				}
			}
		} else if (type == T_BYTE && n == 1 && key) {
			u8 v = pay[at + head];
			for (int s = 0; s < 12; s++)
				if (f->keys[s] == key)
					for (int t = 0; t < 2; t++) {
						u8 *p = desc[e][t];
						if (!p)
							continue;
						if (v < be32(p + 0x9a + 4 * s)) {
							p[0x5e + s] = v;
							brain_counts[C_VALUES]++;
						} else {
							brain_counts[C_SKIP_VALUE]++;
						}
					}
		}
		at += head + align4(n);
	}
}

static void apply(u32 n)
{
	u32 count = be16(buf + 10);
	const u8 *rec[64];
	int ent[64];
	u32 m = 0;
	for (u32 r = 0, at = HDR; r < count; r++, at += be32(buf + at + 16)) {
		const u8 *p = buf + at;
		brain_counts[C_RECORDS]++;
		if (p[0] != K_DEFAULT)
			continue;
		u32 plen = be32(p + 8);
		if (crc32(p + REC_HDR + align4(p[1]), plen) != be32(p + 12)) {
			brain_counts[C_SKIP_CRC]++;
			continue;
		}
		int e = entry_of(p);
		if (e < 0) {
			brain_counts[C_SKIP_ID]++;
			continue;
		}
		if (m < 64) {
			rec[m] = p;
			ent[m++] = e;
		}
	}
	(void)n;
	/* BRAIN.md section 7.7 rule 5: two records with one store id and
	 * prefix are neither applied. */
	for (u32 i = 0; i < m; i++) {
		const u8 *pi = rec[i] + REC_HDR + align4(rec[i][1]);
		int dup = 0;
		for (u32 j = 0; j < m; j++) {
			const u8 *pj = rec[j] + REC_HDR + align4(rec[j][1]);
			if (j != i && ent[j] == ent[i] && be32(pi) == be32(pj))
				dup = 1;
		}
		if (dup)
			brain_counts[C_SKIP_DUP]++;
		else
			apply_record(rec[i], ent[i]);
	}
}

/* ---- settings: the run-time value table (BRAIN.md sections 5, 7.7) -------- */

/* One per setting the unit holds (manifest.py settings_inc), in the
 * SETTINGS list's order. brain_values[i] is its value, a long other units
 * read by symbol (remix.brain.value_symbol). */
struct set_ent {
	const char *id;
	u8 id_len, type, apply, nlabels;	/* type 1 Binary, 2 Option, 3 Number */
	u16 key, step;
	int min, max, def;
	const char *const *labels;
	const char *name, *group;
	u32 scope;				/* 0 CARD (card.work), 1 PROJECT (the project's brain.work) */
};
extern const struct set_ent brain_set[];
extern const u32 brain_set_n;
extern int brain_values[];

#define K_SETTINGS 1
#define MAX_SET    48

enum { C_SET_RECORDS = 0, C_SET_VALUES, C_SET_SKIP, C_SET_DUP, C_SET_N };
/* C_SET_SKIP: a value whose type, bounds or step does not fit (the layer
 * below stays); C_SET_DUP: two settings records for one store (neither) */
u32 brain_set_counts[C_SET_N] DATA;

static int value_ok(const struct set_ent *e, int v)
{
	return v >= e->min && v <= e->max && (e->step <= 1 || (v - e->min) % e->step == 0);
}

/* One settings record's values into the table: the known keys of its
 * store whose type and bounds fit. */
static void apply_settings(const u8 *rec, u32 scope)
{
	u32 idlen = rec[1], plen = be32(rec + 8);
	const u8 *pay = rec + REC_HDR + align4(idlen);
	if (be16(rec + 2) != 1)				/* schema major 1 */
		return;
	brain_set_counts[C_SET_RECORDS]++;
	for (u32 at = 0; at + 4 <= plen;) {
		u32 key = be16(pay + at), type = pay[at + 2], n = pay[at + 3], head = 4;
		if (n == 255) {
			if (at + 8 > plen)
				break;
			n = be32(pay + at + 4);
			head = 8;
		}
		if (at + head + align4(n) > plen)
			break;
		const u8 *d = pay + at + head;
		for (u32 i = 0; i < brain_set_n && i < MAX_SET; i++) {
			const struct set_ent *e = &brain_set[i];
			if (e->scope != scope || e->key != key || !same_id(rec + REC_HDR, idlen, e->id, e->id_len))
				continue;
			int v;
			if (type != e->type || (type == 3 ? n != 2 : n != 1)) {
				brain_set_counts[C_SET_SKIP]++;
				continue;
			}
			v = type == 3 ? (int)(short)be16(d) : d[0];
			if (!value_ok(e, v)) {
				brain_set_counts[C_SET_SKIP]++;
				continue;
			}
			brain_values[i] = v;
			brain_set_counts[C_SET_VALUES]++;
		}
		at += head + align4(n);
	}
}

/* The table back to the layers below, the counters to zero (brain_load). */
static void reset_settings(void)
{
	for (u32 i = 0; i < brain_set_n && i < MAX_SET; i++)
		brain_values[i] = brain_set[i].def;
	for (int k = 0; k < C_SET_N; k++)
		brain_set_counts[k] = 0;
}

/* Every settings record of the file in buf, applied to the entries of
 * `scope`: a store with two records is skipped (rule 5). */
static void load_settings(int have, u32 scope)
{
	if (!have)
		return;
	u32 count = be16(buf + 10);
	for (u32 r = 0, at = HDR; r < count; r++, at += be32(buf + at + 16)) {
		const u8 *p = buf + at;
		if (p[0] != K_SETTINGS || crc32(p + REC_HDR + align4(p[1]), be32(p + 8)) != be32(p + 12))
			continue;
		int dup = 0;
		for (u32 r2 = 0, at2 = HDR; r2 < count; r2++, at2 += be32(buf + at2 + 16)) {
			const u8 *q = buf + at2;
			if (q != p && q[0] == K_SETTINGS && same_id(q + REC_HDR, q[1], (const char *)p + REC_HDR, p[1]))
				dup = 1;
		}
		if (dup)
			brain_set_counts[C_SET_DUP]++;
		else
			apply_settings(p, scope);
	}
}

/* ---- templates: the copy brain_load keeps (more below the menu) ---------- */

#define K_TEMPLATE  3
#define TPL_CACHE   4096
#define TPL_MAX     64
#define TPL_NAME    8

static u8 tpl_cache[TPL_CACHE];
static u32 tpl_len DATA;			/* bytes of records in tpl_cache */
/* the image's templates (Remix.templates, compiled at build by
 * tools/remix/brain.py compile_templates; manifest.py fx_inc) */
extern const u8 brain_img_tpl[];
extern const u32 brain_img_tpl_len;

/* the copy: every template record of the file in buf (called by brain_load
 * and after each template write) */
static void cache_templates(int have)
{
	tpl_len = 0;
	if (!have)
		return;
	u32 count = be16(buf + 10);
	for (u32 r = 0, at = HDR; r < count; r++, at += be32(buf + at + 16)) {
		const u8 *p = buf + at;
		u32 size = be32(p + 16);
		if (p[0] != K_TEMPLATE || tpl_len + size > TPL_CACHE)
			continue;
		if (crc32(p + REC_HDR + align4(p[1]), be32(p + 8)) != be32(p + 12))
			continue;
		for (u32 k = 0; k < size; k++)
			tpl_cache[tpl_len + k] = p[k];
		tpl_len += size;
	}
}

/* BRAIN's own settings (manifest.py: store octabam.brain) */
extern int brain_v_octabam_brain_1;		/* MIDI LOG: 1 = the debug log records */
extern int brain_v_octabam_brain_2;		/* CARD DEFAULTS: 1 = card defaults apply */

/* ---- the project pair and the project record (BRAIN.md sections 6.1, 7.5, 8) ---- */

#define SET_PATH   ((const char *)0x100f8480)	/* the current set's path: "/SETNAME" */
#define PROJ_NAME  ((const char *)0x100f8378)	/* the current project's folder name */
#define PATH_HALF  0x100
#define K_PROJECT  4
#define OWN_STORE  "octabam.brain"		/* the store id the project record is filed under */

static char pw[PATH_HALF * 2 + 16], ps[PATH_HALF * 2 + 16];	/* <set>/<project>/brain.work, .strd */

/* Both paths of the current project's pair; 0 when no set or project is
 * named (nothing loaded yet). */
static int proj_paths(void)
{
	const char *set = SET_PATH, *proj = PROJ_NAME;
	u32 n = 0, k;
	for (k = 0; set[k] && k < PATH_HALF; k++)
		pw[n++] = set[k];
	if (!n || !proj[0])
		return 0;
	if (proj[0] != '/')
		pw[n++] = '/';
	for (k = 0; proj[k] && k < PATH_HALF; k++)
		pw[n++] = proj[k];
	pw[n++] = '/';
	for (k = 0; k < n; k++)
		ps[k] = pw[k];
	const char *w = "brain.work", *s = "brain.strd";
	for (k = 0; k < 11; k++) {
		pw[n + k] = w[k];
		ps[n + k] = s[k];
	}
	return 1;
}

/* ---- MIDI knob names (BRAIN.md section 3) -------------------------------- */

/* A name per bank, Part, MIDI track and assignable CC (CC1..CC10): four
 * characters and a NUL in a Name value's six bytes. In the project's
 * brain.work as field blocks (kind 5) of store stock.midi, one per Part
 * that has a name: target bank * 4 + Part, instance 0, a Name value keyed
 * track * 10 + cc + 1. Cleared and read at each project load; drawn on
 * the CTRL1/CTRL2 pages by brain_klabel. */
#define K_FIELD   5
#define T_NAME    6
#define KN_PARTS  64
#define KN_LEN    6
#define KN_CHARS  4
#define KN_STORE  "stock.midi"
static char knames[KN_PARTS][8][10][KN_LEN];

/* bank * 4 + Part of the current Part */
static u32 kn_part(void)
{
	return ((u32)(*(volatile u8 *)0x80000002 & 15) << 2) | (*(volatile u8 *)0x80000003 & 3);
}

static void kn_clear(void)
{
	char *p = &knames[0][0][0][0];
	for (u32 k = 0; k < sizeof knames; k++)
		p[k] = 0;
}

/* the field blocks of the project file in buf */
static void kn_load(void)
{
	u32 count = be16(buf + 10);
	for (u32 r = 0, at = HDR; r < count; r++, at += be32(buf + at + 16)) {
		const u8 *p = buf + at;
		if (p[0] != K_FIELD || !same_id(p + REC_HDR, p[1], KN_STORE, 10))
			continue;
		const u8 *pay = p + REC_HDR + align4(p[1]);
		u32 plen = be32(p + 8);
		if (plen < 4 || crc32(pay, plen) != be32(p + 12) || be16(pay) >= KN_PARTS)
			continue;
		char (*part)[10][KN_LEN] = knames[be16(pay)];
		for (u32 v = 4; v + 4 <= plen && pay[v + 3] != 0xff; v += 4 + align4(pay[v + 3])) {
			u32 key = be16(pay + v);
			if (pay[v + 2] != T_NAME || pay[v + 3] != KN_LEN || key < 1 || key > 80)
				continue;
			char *o = part[(key - 1) / 10][(key - 1) % 10];
			for (u32 c = 0; c < KN_CHARS; c++)
				o[c] = (char)pay[v + 4 + c];
			o[KN_CHARS] = 0;
		}
	}
}

/* The Name values keyed 129..138 of template record p onto MIDI track t's
 * names in the current Part; 1 when it had any. */
static int kn_take(const u8 *p, u32 t)
{
	const u8 *pay = p + REC_HDR + align4(p[1]);
	u32 plen = be32(p + 8);
	int any = 0;
	char (*kn)[KN_LEN] = knames[kn_part()][t & 7];
	for (u32 v = 16; v + 4 <= plen && pay[v + 3] != 0xff; v += 4 + align4(pay[v + 3])) {
		u32 key = be16(pay + v);
		if (pay[v + 2] != T_NAME || pay[v + 3] != KN_LEN || key < 129 || key > 138)
			continue;
		for (u32 c = 0; c < KN_CHARS; c++)
			kn[key - 129][c] = (char)pay[v + 4 + c];
		kn[key - 129][KN_CHARS] = 0;
		any = 1;
	}
	return any;
}

/* The label the CTRL page drawer pushes for slot s of descriptor P (the
 * detour at 0x4004e474, hooks.s brain_klabel_hook): the current MIDI
 * track's name for that CC when it has one, else dflt. CTL1 (P
 * 0x400d42f4): slots 2-5 are CC1-CC4; CTL2 (P 0x400d4486): slots 0-5 are
 * CC5-CC10. Bank, Part and track as the UI's writers read them. */
const char *brain_klabel(u32 P, u32 s, const char *dflt)
{
	int cc = P == 0x400d42f4 ? (int)s - 2 : P == 0x400d4486 ? 4 + (int)s : -1;
	if (cc < 0 || cc > 9 || s > 5)
		return dflt;
	const char *n = knames[kn_part()][*(volatile u8 *)0x80000000 & 7][cc];
	return n[0] ? n : dflt;
}

/* Counters of the last project load, read by the gates. */
enum { PC_LOADS, PC_STATE, PC_RECORD, PC_ENTRIES, PC_SAME, PC_MOVED, PC_LAYOUT, PC_GONE, PC_SKIP,
       PC_N };
/* PC_STATE: as C_STATE (0 no files or no project, 1 brain.work, 2 recovered from
 * brain.strd, 3 damaged); PC_RECORD: 1 when one project record was compared;
 * PC_ENTRIES: its entries. Per entry, one of: PC_SAME (its FX id names the same
 * store and layout), PC_MOVED (its FX id names another store, or none, and the
 * image still has the store), PC_LAYOUT (same store, another layout hash),
 * PC_GONE (the image has no such store). PC_SKIP: project records not
 * compared (CRC, schema major, a second record). */
u32 brain_pcounts[PC_N] DATA;

/* One pair by the rule of section 6.2: 0 neither file, 1 .work valid, 2
 * .strd valid (the .work absent or damaged), 3 damaged; buf holds the file
 * for 1 and 2. */
static int pair_load(const char *w, const char *s, u32 *len)
{
	u32 n = slurp(w);
	if (n && valid(n)) {
		*len = n;
		return 1;
	}
	int work_present = n != 0;
	n = slurp(s);
	if (n && valid(n)) {
		*len = n;
		return 2;
	}
	return (work_present || n) ? 3 : 0;
}

/* The project record of the file in buf against brain_fx[]. */
static void check_project(void)
{
	u32 count = be16(buf + 10), found = 0;
	const u8 *rec = 0;
	for (u32 r = 0, at = HDR; r < count; r++, at += be32(buf + at + 16)) {
		const u8 *p = buf + at;
		if (p[0] != K_PROJECT || !same_id(p + REC_HDR, p[1], OWN_STORE, 13))
			continue;
		if (be16(p + 2) != 1 || crc32(p + REC_HDR + align4(p[1]), be32(p + 8)) != be32(p + 12)) {
			brain_pcounts[PC_SKIP]++;
			continue;
		}
		found++;
		rec = p;
	}
	if (found > 1) {			/* rule 5: neither */
		brain_pcounts[PC_SKIP] += found;
		return;
	}
	if (!rec)
		return;
	brain_pcounts[PC_RECORD] = 1;
	const u8 *pay = rec + REC_HDR + align4(rec[1]);
	u32 plen = be32(rec + 8);
	for (u32 at = 0; at + 8 <= plen;) {
		u32 fx = pay[at], idlen = pay[at + 1], layout = be32(pay + at + 4);
		if (idlen < 1 || idlen > 63 || at + 8 + align4(idlen) > plen)
			break;
		const u8 *id = pay + at + 8;
		at += 8 + align4(idlen);
		brain_pcounts[PC_ENTRIES]++;
		int e = -1, at_fx = -1;
		for (u32 i = 0; i < brain_fx_n && i < MAX_FX; i++) {
			if (same_id(id, idlen, brain_fx[i].id, brain_fx[i].id_len))
				e = (int)i;
			if (brain_fx[i].fx_id == fx)
				at_fx = (int)i;
		}
		if (e < 0)
			brain_pcounts[PC_GONE]++;
		else if (at_fx != e)
			brain_pcounts[PC_MOVED]++;
		else if (brain_fx[e].layout != layout)
			brain_pcounts[PC_LAYOUT]++;
		else
			brain_pcounts[PC_SAME]++;
	}
}

/* The project pair: its PROJECT-scope settings onto the table, its project
 * record against the image. */
static void load_project(void)
{
	for (int k = PC_STATE; k < PC_N; k++)
		brain_pcounts[k] = 0;
	brain_pcounts[PC_LOADS]++;
	u32 n;
	if (!proj_paths())
		return;
	kn_clear();
	int st = pair_load(pw, ps, &n);
	brain_pcounts[PC_STATE] = (u32)st;
	if (st == 1 || st == 2) {
		load_settings(1, 1);
		check_project();
		kn_load();
	}
}

static void load_card(void)
{
	u32 n;
	int st = pair_load("/BRAIN/card.work", "/BRAIN/card.strd", &n);
	brain_counts[C_STATE] = (u32)st;
	load_settings(st == 1 || st == 2, 0);
	cache_templates(st == 1 || st == 2);
	if ((st == 1 || st == 2) && brain_v_octabam_brain_2)
		apply(n);
}

void brain_load(void)
{
	/* the counters after C_LOADS describe the last load */
	for (int k = C_STATE; k < C_N; k++)
		brain_counts[k] = 0;
	brain_counts[C_LOADS]++;
	if (!snapped)
		snapshot();
	restore();
	reset_settings();
	load_card();
	load_project();
}

/* ---- writing the card store ------------------------------------------------ */

#define MKDIR    (*(int (**)(const char *))0x46c8240a)	/* FS vtable: create a directory */
#define F_WRITE  ((int (*)(void *, const void *, u32))0x400166b8)
#define DBPTR    (*(u8 **)0x46c82456)			/* the current bank */
#define CUR_PART (*(volatile u8 *)0x80000003)
#define PART_LEN 6322
#define WORKING  0x8ed80

enum { S_SAVES, S_SAVE_ERR, S_STRD, S_STRD_ERR, S_N };
/* S_SAVE_ERR: last error of brain_save_default (0 none, 1 no effect, 2 card
 * store damaged, 3 too large, 4 write, 5 a template of that name is on the
 * card); S_STRD: card.strd copies written */
u32 brain_save_counts[S_N] DATA;

static u8 out[BUF_LEN];

static void put32(u8 *p, u32 v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void put16(u8 *p, u32 v) { p[0] = v >> 8; p[1] = v; }

static int write_file(const char *path, const u8 *data, u32 n)
{
	int ok = 0;
	if (F_OPEN(fobj, path, "w", iob, sizeof iob) < 0) {
		if (path[1] != 'B')			/* a project's file: its folder exists */
			return -1;
		MKDIR("/BRAIN");			/* the first write on this card */
		if (F_OPEN(fobj, path, "w", iob, sizeof iob) < 0)
			return -1;
	}
	ok = F_WRITE(fobj, data, n) == 1;
	F_CLOSE(fobj);
	return ok ? 0 : -1;
}

/* The content of a pair in buf (BRAIN.md section 6.2): its length, 0 when
 * there is none, -1 when both copies are damaged (no automatic write). */
static int cur(const char *w, const char *s)
{
	u32 n = slurp(w);
	if (n && valid(n))
		return (int)n;
	u32 work_present = n != 0;
	n = slurp(s);
	if (n && valid(n))
		return (int)n;
	return (work_present || n) ? -1 : 0;
}

static int current_store(void)
{
	return cur("/BRAIN/card.work", "/BRAIN/card.strd");
}

/* SAVE AS DEFAULT: track t's FX1 (fx 0) or FX2 (fx 1) page in the current
 * Part becomes that effect's card-layer default. The record replaces the
 * effect's default record (no mode) in card.work; every other record keeps
 * its bytes. The descriptor takes the values at once. 0, or an error code
 * (also in brain_save_counts[S_SAVE_ERR]). */
static int write_default(u32 t, u32 fx, int clear);

/* out[HDR..at) holds `kept` records: the header (file kind 0 card .work, 2
 * project .work), then the file at path. */
static int write_store(const char *path, u32 kind, u32 at, u32 kept)
{
	for (u32 k = 0; k < HDR; k++)
		out[k] = 0;
	put32(out, 0x4252414eu);			/* "BRAN": a brain file */
	put16(out + 4, HDR);
	out[6] = 1;					/* container 1.0 */
	out[8] = (u8)kind;
	put16(out + 10, kept);
	put32(out + 12, at);
	for (u32 k = 0; k < 8 && "BRAIN"[k]; k++)
		out[20 + k] = "BRAIN"[k];
	put32(out + 16, crc32(out + 20, at - 20));
	return write_file(path, out, at);
}

static int write_card(u32 at, u32 kept)
{
	return write_store("/BRAIN/card.work", 0, at, kept);
}

/* The settings records of one scope: scope 0 is card.work, scope 1 the
 * project's brain.work (pw, ps; proj_paths() first). One record per store
 * that has entries of the scope, its values the table's; keys this image
 * does not know are kept from the record it replaces (rule 2), and every
 * other record keeps its bytes. With `project`, the project record
 * (section 7.5) is rebuilt from brain_fx[] and replaces any other one.
 * 0, or an error code as write_default's (5: no project is named). */
static int in_scope(u32 f, u32 scope)
{
	return brain_set[f].scope == scope;
}

static int write_set_file(u32 scope, int project)
{
	if (scope && !proj_paths())
		return 5;
	const char *w = scope ? pw : "/BRAIN/card.work", *sd = scope ? ps : "/BRAIN/card.strd";
	int n = cur(w, sd);
	if (n < 0)
		return 2;
	u32 at = HDR, kept = 0;
	/* the kept records: everything but the settings of the stores we write
	 * and, with `project`, the project records */
	for (u32 r = 0, i = HDR; n && r < be16(buf + 10); r++, i += be32(buf + i + 16)) {
		const u8 *p = buf + i;
		u32 size = be32(p + 16), ours = 0;
		if (p[0] == K_SETTINGS)
			for (u32 e = 0; e < brain_set_n && e < MAX_SET; e++)
				ours |= in_scope(e, scope) && same_id(p + REC_HDR, p[1], brain_set[e].id, brain_set[e].id_len);
		if (p[0] == K_PROJECT && project && same_id(p + REC_HDR, p[1], OWN_STORE, 13))
			ours = 1;
		if (ours)
			continue;
		if (at + size > BUF_LEN)
			return 3;
		for (u32 k = 0; k < size; k++)
			out[at + k] = p[k];
		at += size;
		kept++;
	}
	/* one record per store, in table order */
	for (u32 e = 0; e < brain_set_n && e < MAX_SET; e++) {
		const struct set_ent *s = &brain_set[e];
		if (!in_scope(e, scope))
			continue;
		int first = 1;
		for (u32 f = 0; f < e; f++)
			if (in_scope(f, scope) && same_id((const u8 *)brain_set[f].id, brain_set[f].id_len, s->id, s->id_len))
				first = 0;
		if (!first)
			continue;
		u8 *r = out + at;
		u32 idsz = align4(s->id_len);
		if (at + REC_HDR + idsz > BUF_LEN)
			return 3;
		for (u32 k = 0; k < REC_HDR + idsz; k++)
			r[k] = 0;
		r[0] = K_SETTINGS;
		r[1] = s->id_len;
		put16(r + 2, 1);			/* schema major 1, minor 0 */
		for (u32 k = 0; k < s->id_len; k++)
			r[REC_HDR + k] = s->id[k];
		u8 *v = r + REC_HDR + idsz, *v0 = v;
		for (u32 f = e; f < brain_set_n && f < MAX_SET; f++) {
			const struct set_ent *g = &brain_set[f];
			if (!in_scope(f, scope) || !same_id((const u8 *)g->id, g->id_len, s->id, s->id_len))
				continue;
			if (v + 8 > out + BUF_LEN)
				return 3;
			put16(v, g->key);
			v[2] = g->type;
			v[3] = g->type == 3 ? 2 : 1;
			v[4] = v[5] = v[6] = v[7] = 0;
			if (g->type == 3)
				put16(v + 4, (u32)brain_values[f] & 0xffff);
			else
				v[4] = (u8)brain_values[f];
			v += 8;
		}
		/* unknown keys from the record this one replaces */
		for (u32 rr = 0, i = HDR; n && rr < be16(buf + 10); rr++, i += be32(buf + i + 16)) {
			const u8 *p = buf + i;
			if (p[0] != K_SETTINGS || !same_id(p + REC_HDR, p[1], s->id, s->id_len))
				continue;
			const u8 *pay = p + REC_HDR + align4(p[1]);
			u32 plen = be32(p + 8);
			for (u32 a = 0; a + 4 <= plen;) {
				u32 key = be16(pay + a), len = pay[a + 3], head = 4, known = 0;
				if (len == 255) {
					if (a + 8 > plen)
						break;
					len = be32(pay + a + 4);
					head = 8;
				}
				u32 sz = head + align4(len);
				if (a + sz > plen)
					break;
				for (u32 f = 0; f < brain_set_n && f < MAX_SET; f++)
					known |= in_scope(f, scope) && brain_set[f].key == key
						&& same_id((const u8 *)brain_set[f].id, brain_set[f].id_len, s->id, s->id_len);
				if (!known) {
					if (v + sz > out + BUF_LEN)
						return 3;
					for (u32 k = 0; k < sz; k++)
						v[k] = pay[a + k];
					v += sz;
				}
				a += sz;
			}
			break;
		}
		u32 plen = (u32)(v - v0), size = REC_HDR + idsz + plen;
		put32(r + 8, plen);
		put32(r + 12, crc32(v0, plen));
		put32(r + 16, size);
		at += size;
		kept++;
	}
	if (project) {
		/* the project record: one entry per effect in the image */
		u32 plen = 0;
		for (u32 i = 0; i < brain_fx_n && i < MAX_FX; i++)
			plen += 8 + align4(brain_fx[i].id_len);
		u32 size = REC_HDR + align4(13) + plen;
		if (at + size > BUF_LEN)
			return 3;
		u8 *r = out + at;
		for (u32 k = 0; k < size; k++)
			r[k] = 0;
		r[0] = K_PROJECT;
		r[1] = 13;
		put16(r + 2, 1);
		put32(r + 8, plen);
		put32(r + 16, size);
		for (u32 k = 0; k < 13; k++)
			r[REC_HDR + k] = (u8)OWN_STORE[k];
		u8 *v = r + REC_HDR + align4(13), *v0 = v;
		for (u32 i = 0; i < brain_fx_n && i < MAX_FX; i++) {
			const struct fx_ent *f = &brain_fx[i];
			v[0] = f->fx_id;
			v[1] = f->id_len;
			put32(v + 4, f->layout);
			for (u32 k = 0; k < f->id_len; k++)
				v[8 + k] = (u8)f->id[k];
			v += 8 + align4(f->id_len);
		}
		put32(r + 12, crc32(v0, plen));
		at += size;
		kept++;
	}
	return write_store(w, scope ? 2 : 0, at, kept) < 0 ? 4 : 0;
}

/* SETTINGS edit: card.work, and the project's brain.work when a PROJECT
 * setting exists and a project is named. */
static int write_settings(void)
{
	int err = write_set_file(0, 0), proj = 0;
	for (u32 e = 0; e < brain_set_n && e < MAX_SET; e++)
		proj |= in_scope(e, 1);
	if (proj) {
		int e2 = write_set_file(1, 0);
		if (!err)
			err = e2;
	}
	return err;
}

int brain_save_default(u32 t, u32 fx)
{
	return write_default(t, fx, 0);
}

/* CLEAR DEFAULT: the effect's default record (no mode) leaves card.work and
 * the descriptor goes back to the image's values. */
int brain_clear_default(u32 t, u32 fx)
{
	return write_default(t, fx, 1);
}

static int write_default(u32 t, u32 fx, int clear)
{
	u8 *part = DBPTR + WORKING + (CUR_PART & 3) * PART_LEN;
	u32 id = part[fx ? 0x8 + t : t];
	int e = -1, err = 0;
	brain_save_counts[S_SAVES]++;
	if (!snapped)
		snapshot();
	for (u32 i = 0; i < brain_fx_n && i < MAX_FX; i++)
		if (brain_fx[i].fx_id == id && (desc[i][0] || desc[i][1]))
			e = (int)i;
	if (e < 0 || t > 7) {
		err = 1;
		goto done;
	}
	const struct fx_ent *f = &brain_fx[e];
	u8 vals[12];
	for (int s = 0; s < 6; s++) {
		vals[s] = part[0x11a + t * 24 + (fx ? 18 : 12) + s];
		vals[6 + s] = part[0x2fe + t * 30 + (fx ? 6 : 0) + s];
	}
	int n = current_store();
	if (n < 0) {
		err = 2;
		goto done;
	}
	/* the file: the kept records, then the new one */
	u32 at = HDR, kept = 0;
	for (u32 r = 0, i = HDR; n && r < be16(buf + 10); r++, i += be32(buf + i + 16)) {
		const u8 *p = buf + i;
		u32 size = be32(p + 16);
		const u8 *pay = p + REC_HDR + align4(p[1]);
		if (p[0] == K_DEFAULT && same_id(p + REC_HDR, p[1], f->id, f->id_len)
		    && be32(p + 8) >= 4 && be16(pay) == 0 && be16(pay + 2) == NO_MODE)
			continue;
		if (at + size > BUF_LEN) {
			err = 3;
			goto done;
		}
		for (u32 k = 0; k < size; k++)
			out[at + k] = p[k];
		at += size;
		kept++;
	}
	if (clear)
		goto header;
	u32 nvals = 0;
	for (int s = 0; s < 12; s++)
		nvals += f->keys[s] != 0;
	u32 plen = 8 + 8 * nvals, size = REC_HDR + align4(f->id_len) + plen;
	if (at + size > BUF_LEN) {
		err = 3;
		goto done;
	}
	u8 *r = out + at;
	for (u32 k = 0; k < size; k++)
		r[k] = 0;
	r[0] = K_DEFAULT;
	r[1] = f->id_len;
	put16(r + 2, 1);				/* schema major 1, minor 0 */
	put32(r + 8, plen);
	put32(r + 16, size);
	for (u32 k = 0; k < f->id_len; k++)
		r[REC_HDR + k] = f->id[k];
	u8 *pay = r + REC_HDR + align4(f->id_len);
	put16(pay + 2, NO_MODE);			/* target 0, no mode */
	put32(pay + 4, f->layout);
	u8 *v = pay + 8;
	for (int s = 0; s < 12; s++)
		if (f->keys[s]) {
			put16(v, f->keys[s]);
			v[2] = T_BYTE;
			v[3] = 1;
			v[4] = vals[s];
			v += 8;
		}
	put32(r + 12, crc32(pay, plen));
	at += size;
	kept++;
header:
	if (write_card(at, kept) < 0) {
		err = 4;
		goto done;
	}
	/* the descriptor takes the values now (the next load re-applies the file) */
	if (clear) {
		for (int k = 0; k < 2; k++)
			if (desc[e][k])
				for (int s = 0; s < 12; s++)
					desc[e][k][0x5e + s] = shadow[e][k][s];
		goto done;
	}
	for (int s = 0; s < 12; s++)
		if (f->keys[s])
			for (int k = 0; k < 2; k++) {
				u8 *p = desc[e][k];
				if (p && vals[s] < be32(p + 0x9a + 4 * s))
					p[0x5e + s] = vals[s];
			}
done:
	brain_save_counts[S_SAVE_ERR] = (u32)err;
	return -err;
}

/* SAVE PROJECT (the project store), in the engine task: card.work becomes
 * card.strd, the recovery copy (BRAIN.md section 6.2; a damaged or absent
 * card.work is not copied); then the project's brain.work is written (its
 * PROJECT settings and the project record) and becomes brain.strd. */
enum { PS_SAVES, PS_ERR, PS_STRD, PS_STRD_ERR, PS_N };
/* PS_ERR: the last write_set_file error (0 none, 2 both files damaged, 3 too
 * large, 4 write, 5 no project named) */
u32 brain_proj_counts[PS_N] DATA;

void brain_card_store(void)
{
	u32 n = slurp("/BRAIN/card.work");
	if (n && valid(n)) {
		buf[8] = 1;				/* file kind: card .strd (outside the CRC) */
		if (write_file("/BRAIN/card.strd", buf, n) < 0)
			brain_save_counts[S_STRD_ERR]++;
		else
			brain_save_counts[S_STRD]++;
	}
	brain_proj_counts[PS_SAVES]++;
	int err = write_set_file(1, 1);
	brain_proj_counts[PS_ERR] = (u32)err;
	if (err)
		return;
	n = slurp(pw);
	if (!n || !valid(n))
		return;
	buf[8] = 3;					/* project .strd */
	if (write_file(ps, buf, n) < 0)
		brain_proj_counts[PS_STRD_ERR]++;
	else
		brain_proj_counts[PS_STRD]++;
}

/* ---- the engine job: SAVE AS DEFAULT off the UI task ----------------------- */

#define Q_SEND   ((int (*)(void *, void *))0x40000c3c)	/* post a message to a queue */
#define ENGINE_Q ((void *)0x460d17ce)			/* the engine task's queue */
#define JOB_SAVE 0x41					/* above stock's 0..45 */

static u32 job_msg[4];		/* type byte first, as stock's records */
static u32 job_args;

/* From any task: SAVE AS DEFAULT for track t's FX1 (0) or FX2 (1), run by
 * the engine task, where the stock writes the project's files. One job is
 * pending at a time; a second post before the engine runs replaces it. */
void brain_post_save(u32 t, u32 fx)
{
	job_args = (t & 7) | (fx ? 0x100 : 0);
	((u8 *)job_msg)[0] = JOB_SAVE;
	Q_SEND(ENGINE_Q, job_msg);
}

/* From any task: card.work copied to card.strd by the engine task (what
 * SAVE PROJECT's project store does in that task through brain_on_store). */
void brain_post_store(void)
{
	job_args = 0x10000;
	((u8 *)job_msg)[0] = JOB_SAVE;
	Q_SEND(ENGINE_Q, job_msg);
}

/* From any task: CLEAR DEFAULT for track t's FX1 (0) or FX2 (1). */
void brain_post_clear(u32 t, u32 fx)
{
	job_args = (t & 7) | (fx ? 0x100 : 0) | 0x200;
	((u8 *)job_msg)[0] = JOB_SAVE;
	Q_SEND(ENGINE_Q, job_msg);
}

/* The engine task, at a JOB_SAVE message (hooks.s brain_on_job). */
static void write_debug(void);

void brain_post_settings(void)
{
	job_args = 0x40000;
	((u8 *)job_msg)[0] = JOB_SAVE;
	Q_SEND(ENGINE_Q, job_msg);
}

enum { T_SAVE, T_DELETE, T_RENAME, T_COPY };
static int write_template(int del, u32 t, u32 kind, u32 e, u32 i);
static int write_names(void);
static int write_pset(int clear);

void brain_job(void)
{
	if (job_args & 0x3000000)
		brain_save_counts[S_SAVE_ERR] = (u32)write_pset(!!(job_args & 0x2000000));
	else if (job_args & 0x800000)
		brain_save_counts[S_SAVE_ERR] = (u32)write_names();
	else if (job_args & 0x600000)
		brain_save_counts[S_SAVE_ERR] = (u32)write_template(job_args & 0x200000 ? T_RENAME : T_COPY, 0, 0,
								     (job_args >> 8) & 0xff, job_args & 0xff);
	else if (job_args & 0x100000)
		brain_save_counts[S_SAVE_ERR] = (u32)write_template(T_DELETE, 0, 0, (job_args >> 8) & 0xff, job_args & 0xff);
	else if (job_args & 0x80000)
		brain_save_counts[S_SAVE_ERR] = (u32)write_template(T_SAVE, job_args & 7, (job_args >> 8) & 7, 0, 0);
	else if (job_args & 0x40000)
		brain_save_counts[S_SAVE_ERR] = (u32)write_settings();
	else if (job_args & 0x20000)
		write_debug();
	else if (job_args & 0x10000)
		brain_card_store();
	else
		write_default(job_args & 7, (job_args >> 8) & 1, (job_args >> 9) & 1);
}

/* ---- the BRAIN menu ------------------------------------------------------ */

#define POPUP     ((void (*)(const char *, u32, const char *const *, u32, u32))0x4006d57c)
#define CUR_TRACK (*(volatile u8 *)0x80000000)
#define MIDI_MODE (*(volatile u32 *)0x80000012)	/* nonzero: MIDI tracks (a long; the MIDI key stores 1) */
#define PAGE_KIND (*(volatile u32 *)0x460d1684)	/* 0 SRC, 1 LFO, 2 AMP, 3 FX1, 4 FX2 */

static const char *msg[2];
static char line1[16];

/* The page the menu was opened over: 0 FX1, 1 FX2, or -1 with the reason
 * in msg[1]. */
static int menu_target(void)
{
	u32 kind = PAGE_KIND;
	if (MIDI_MODE || (kind != 3 && kind != 4)) {
		msg[0] = "OPEN AN FX PAGE";
		msg[1] = "OF AN AUDIO TRACK";
		return -1;
	}
	u32 fx = kind == 4, t = CUR_TRACK & 7;
	u8 *part = DBPTR + WORKING + (CUR_PART & 3) * PART_LEN;
	u32 id = part[fx ? 0x8 + t : t];
	for (u32 i = 0; i < brain_fx_n && i < MAX_FX; i++)
		if (brain_fx[i].fx_id == id) {
			const char *s = fx ? "FX2 TRACK " : "FX1 TRACK ";
			int k = 0;
			while (s[k]) {
				line1[k] = s[k];
				k++;
			}
			line1[k++] = '1' + t;
			line1[k] = 0;
			msg[0] = line1;
			return (int)fx;
		}
	msg[0] = "THIS EFFECT HAS";
	msg[1] = "NO STORED DEFAULT";
	return -1;
}

/* Row actions, called by the menu with one argument (0). */
void brain_menu_save(u32 unused)
{
	int fx = menu_target();
	(void)unused;
	if (fx >= 0) {
		brain_post_save(CUR_TRACK & 7, (u32)fx);
		msg[1] = "SAVED AS DEFAULT";
	}
	POPUP("SAVE AS DEFAULT", 2, msg, 0, 0);
}

void brain_menu_clear(u32 unused)
{
	int fx = menu_target();
	(void)unused;
	if (fx >= 0) {
		brain_post_clear(CUR_TRACK & 7, (u32)fx);
		msg[1] = "DEFAULT CLEARED";
	}
	POPUP("CLEAR DEFAULT", 2, msg, 0, 0);
}

/* TOOLS > PROJECT CHECK: what the last load found when it compared the
 * project's saved record with this image (brain_pcounts). */
static char pc_line[20];
static u32 put_s(char *o, const char *s);

static u32 put_num(char *o, u32 v)
{
	char d[10];
	u32 n = 0, k = 0;
	do
		d[n++] = (char)('0' + v % 10);
	while ((v /= 10) && n < 10);
	while (n)
		o[k++] = d[--n];
	return k;
}

void brain_menu_pcheck(u32 unused)
{
	(void)unused;
	u32 changed = brain_pcounts[PC_MOVED] + brain_pcounts[PC_LAYOUT] + brain_pcounts[PC_GONE];
	if (brain_pcounts[PC_STATE] == 3) {
		msg[0] = "BRAIN FILES OF";
		msg[1] = "THIS PROJECT BAD";
	} else if (!brain_pcounts[PC_RECORD]) {
		msg[0] = "NO RECORD OF A";
		msg[1] = "BRAIN SAVE";
	} else if (!changed) {
		msg[0] = "SAVED WITH";
		msg[1] = "THIS IMAGE";
	} else {
		u32 k = put_num(pc_line, changed);
		k += put_s(pc_line + k, changed == 1 ? " EFFECT CHANGED" : " EFFECTS CHANGED");
		pc_line[k] = 0;
		msg[0] = pc_line;
		msg[1] = "SINCE THE SAVE";
	}
	POPUP("PROJECT CHECK", 2, msg, 0, 0);
}

/* ---- the debug log: MIDI messages and what the firmware did with them ------ */

/* hooks.s brain_on_midi wraps the MIDI thread's call to its handler (table
 * 0x400d6474, docs/firmware/MIDI.md appendix B) and calls brain_midi_log
 * after the handler returns, with the message (status, data) and the state
 * it left: the sync flags (0x80000028), the playing pattern (0x800065be)
 * and the byte after it (0x800065c0). Clock and active sensing are not
 * logged. WRITE DEBUG LOG has the engine task write the ring to
 * /BRAIN/debug.txt. */

#define LOG_N    256

struct dbg_ent { u16 seq; u8 st, d1, d2, sync, pnow, pnext; };
static struct dbg_ent dbg_ring[LOG_N];
static u32 dbg_seq DATA;			/* messages logged since the boot */

void brain_midi_log(const u8 *m)
{
	if (!brain_v_octabam_brain_1)			/* SETTINGS > MIDI LOG off */
		return;
	u8 st = m[0];
	if (st == 0xf8 || st == 0xfe)
		return;
	struct dbg_ent *e = &dbg_ring[dbg_seq % LOG_N];
	e->seq = (u16)dbg_seq;
	e->st = st;
	e->d1 = m[1];
	e->d2 = m[2];
	e->sync = *(volatile u8 *)0x80000028;
	e->pnow = *(volatile u8 *)0x800065be;
	e->pnext = *(volatile u8 *)0x800065c0;
	dbg_seq++;
}

/* Text without stock's sprintf, which does not take %02X or field widths. */
static u32 put_s(char *o, const char *s) { u32 n = 0; while (s[n]) { o[n] = s[n]; n++; } return n; }
static u32 put_hex2(char *o, u32 v) { const char *h = "0123456789ABCDEF"; o[0] = h[(v >> 4) & 15]; o[1] = h[v & 15]; return 2; }
static u32 put_dec(char *o, u32 v, u32 width)
{
	char d[10];
	u32 n = 0, k = 0;
	do { d[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 10);
	while (k + n < width) o[k++] = ' ';
	while (n) o[k++] = d[--n];
	return k;
}

/* The engine task: the ring, oldest first, to /BRAIN/debug.txt. */
static void write_debug(void)
{
	u32 n = dbg_seq < LOG_N ? dbg_seq : LOG_N, at = 0;
	char *o = (char *)out;
	at += put_s(o + at, "BRAIN debug: ");
	at += put_dec(o + at, dbg_seq, 0);
	at += put_s(o + at, " MIDI messages since the boot, the last ");
	at += put_dec(o + at, n, 0);
	at += put_s(o + at, " below (clock and active sensing not logged)\r\n"
		       "  seq status d1 d2 sync pattern next\r\n");
	for (u32 k = dbg_seq - n; k < dbg_seq && at < BUF_LEN - 64; k++) {
		const struct dbg_ent *e = &dbg_ring[k % LOG_N];
		at += put_dec(o + at, e->seq, 5);
		at += put_s(o + at, "   ");
		at += put_hex2(o + at, e->st);
		at += put_s(o + at, "    ");
		at += put_hex2(o + at, e->d1);
		at += put_s(o + at, " ");
		at += put_hex2(o + at, e->d2);
		at += put_s(o + at, "  ");
		at += put_hex2(o + at, e->sync);
		at += put_dec(o + at, e->pnow, 6);
		at += put_dec(o + at, e->pnext, 7);
		at += put_s(o + at, "\r\n");
	}
	if (write_file("/BRAIN/debug.txt", out, at) < 0)
		brain_save_counts[S_SAVE_ERR] = 4;
}

void brain_post_log(void)
{
	job_args = 0x20000;
	((u8 *)job_msg)[0] = JOB_SAVE;
	Q_SEND(ENGINE_Q, job_msg);
}

void brain_menu_log(u32 unused)
{
	(void)unused;
	brain_post_log();
	msg[0] = "WRITING THE LOG TO";
	msg[1] = "/BRAIN/DEBUG.TXT";
	POPUP("DEBUG LOG", 2, msg, 0, 0);
}

/* ---- the pane's sub-lists ------------------------------------------------ */

/* The stock menu engine is two levels deep: LEFT always focuses the root,
 * and YES on a list row runs its action (0x40064e64; docs/firmware/
 * MAINMENU.md section 4). So BRAIN's top rows each
 * point brain_list at another row array, and the redraw that follows every
 * key (0x40064d7c) shows it. Each sub-list's first row goes back to the
 * top; MAIN MENU's opening (REMIX SWITCH's osw_menu) resets to the top too. */

struct mrow { const char *label; void *window; void (*action)(u32); u32 getter; void *child; u32 id; };
struct mlist { u32 count, scroll, cursor, sel, visible, count2; struct mrow *rows; };

extern struct mlist brain_list;
extern struct mrow brain_top_rows[], brain_def_rows[], brain_tool_rows[], brain_rmx_rows[],
	brain_set_rows[];
void brain_post_settings(void);
extern u32 brain_rmx_n;			/* REMIXES' rows, the back row included (switch.s) */

static u32 brain_from DATA;		/* the top row the open sub-list came from */

static void show(struct mrow *rows, u32 n, u32 at)
{
	brain_list.rows = rows;
	brain_list.count = brain_list.count2 = n;
	brain_list.scroll = at >= brain_list.visible ? at - brain_list.visible + 1 : 0;
	brain_list.cursor = at - brain_list.scroll;
	brain_list.sel = at;
}

#define TOP_N   5				/* SETTINGS, DEFAULTS, TEMPLATES, REMIXES, TOOLS */
#define PANE_W  15				/* characters the list pane shows */
#define NOOP    ((void (*)(u32))0x400648f8)	/* stock's shared row action, `rts` */

void brain_menu_top(void)
{
	show(brain_top_rows, TOP_N, 0);
}

void brain_menu_back(u32 unused)
{
	(void)unused;
	show(brain_top_rows, TOP_N, brain_from);
}

void brain_open_defaults(u32 unused)
{
	(void)unused;
	brain_from = 1;
	show(brain_def_rows, 5, 1);
}

void brain_open_remixes(u32 unused)
{
	(void)unused;
	brain_from = 3;
	show(brain_rmx_rows, brain_rmx_n, brain_rmx_n > 1 ? 1 : 0);
}

void brain_open_tools(u32 unused)
{
	(void)unused;
	brain_from = 4;
	show(brain_tool_rows, 5, 1);
}

/* SETTINGS: the back row, then per module a heading (its key, no action)
 * and its settings, each label "NAME" with the value at the pane's right
 * edge. YES steps the value (Binary and Option wrap; a Number by its step,
 * from max back to min) and has the engine task write card.work. */

static char set_lbl[MAX_SET][PANE_W + 1];
static u8 set_row_ent[MAX_SET * 2 + 2];		/* row -> entry, 0xff: not a setting */
static char set_none[] = "NO SETTINGS";

static void put_row(struct mrow *r, const char *label, void (*action)(u32))
{
	r->label = label;
	r->window = 0;
	r->action = action;
	r->getter = 0;
	r->child = 0;
	r->id = 0;
}

static u32 slen(const char *s)
{
	u32 n = 0;
	while (s[n])
		n++;
	return n;
}

static void set_label(u32 i)
{
	const struct set_ent *e = &brain_set[i];
	char val[12];
	const char *vs = val;
	int v = brain_values[i];
	if (e->type == 1)
		vs = v ? "ON" : "OFF";
	else if (e->type == 2)
		vs = (u32)v < e->nlabels ? e->labels[v] : "?";
	else {
		u32 k = 0, a = (u32)(v < 0 ? -v : v);
		char tmp[8];
		do
			tmp[k++] = (char)('0' + a % 10);
		while ((a /= 10) && k < 7);
		u32 o = 0;
		if (v < 0)
			val[o++] = '-';
		while (k)
			val[o++] = tmp[--k];
		val[o] = 0;
	}
	u32 vl = slen(vs), nl = slen(e->name);
	if (vl > PANE_W - 2)
		vl = PANE_W - 2;
	if (nl > PANE_W - 1 - vl)
		nl = PANE_W - 1 - vl;
	char *o = set_lbl[i];
	u32 k = 0;
	for (; k < nl; k++)
		o[k] = e->name[k];
	for (; k < PANE_W - vl; k++)
		o[k] = ' ';
	for (u32 j = 0; j < vl; j++)
		o[k++] = vs[j];
	o[k] = 0;
}

void brain_set_row(u32 unused)
{
	(void)unused;
	u32 r = brain_list.sel;
	if (r >= sizeof set_row_ent || set_row_ent[r] == 0xff)
		return;
	u32 i = set_row_ent[r];
	const struct set_ent *e = &brain_set[i];
	int v = brain_values[i] + (e->type == 3 ? e->step : 1);
	if (v > e->max)
		v = e->min;
	brain_values[i] = v;
	set_label(i);
	brain_post_settings();
}

void brain_open_settings(u32 unused)
{
	(void)unused;
	struct mrow *row = brain_set_rows + 1;	/* row 0: the back row (manifest) */
	u32 n = 1, first = 0;
	const char *group = 0;
	for (u32 i = 0; i < sizeof set_row_ent; i++)
		set_row_ent[i] = 0xff;
	for (u32 i = 0; i < brain_set_n && i < MAX_SET; i++) {
		const struct set_ent *e = &brain_set[i];
		if (e->group != group) {
			group = e->group;
			put_row(row++, group, 0);
			n++;
		}
		set_label(i);
		put_row(row++, set_lbl[i], brain_set_row);
		set_row_ent[n] = (u8)i;
		if (!first)
			first = n;
		n++;
	}
	if (n == 1) {
		put_row(row, set_none, NOOP);
		n = 2;
		first = 1;
	}
	brain_from = 0;
	show(brain_set_rows, n, first);
}

/* ---- templates: lists, apply, save and delete (BRAIN.md sections 5.2, 9) ---- */

/* A template's target is one view: track t's FX1 or FX2 page (an effect of
 * brain_fx[], entry e < MAX_FX), its AMP or LFO page, a whole MIDI track
 * (the stock targets, entry ST_BASE + kind - V_AMP), or its SRC page, one
 * target per machine type (entry ST_BASE + 3 + machine). */
enum { V_FX1, V_FX2, V_AMP, V_LFO, V_MIDI, V_SRC };
#define ST_BASE  0x40
#define NSLOT    72
#define SKIP     0xfe		/* flat[]: a slot with no parameter */
#define NOFLAT   0xff		/* flat[]: a slot the page-1 writer does not reach */

struct view {
	u32 kind, t, e, n, machine;
	u16 key[NSLOT];			/* 0: the slot is not stored */
	u32 at[NSLOT];			/* Part offset from DBPTR + part * PART_LEN */
	u8 lo[NSLOT], hi[NSLOT];
	u8 flat[NSLOT];			/* page-1 writer index, NOFLAT or SKIP */
	u8 cur[NSLOT];
};

struct st_ent {
	const char *id;
	u32 id_len, layout;
};
#define ST_N     8
static const struct st_ent st_ent[ST_N] = {
	{ "stock.amp", 9, 0x414d5031 },		/* "AMP1" */
	{ "stock.lfo", 9, 0x4c464f31 },		/* "LFO1" */
	{ "stock.midi", 10, 0x4d494431 },	/* "MID1" */
	/* the SRC page by machine type (PARAM_PAGES.md section 3, entries 0-4) */
	{ "stock.static", 12, 0x53544131 },	/* "STA1" */
	{ "stock.flex", 10, 0x464c5831 },	/* "FLX1" */
	{ "stock.thru", 10, 0x54485231 },	/* "THR1" */
	{ "stock.neighbor", 14, 0x4e425231 },	/* "NBR1" */
	{ "stock.pickup", 12, 0x50434b31 },	/* "PCK1" */
};
#define D_PLAYBACK 0x400d2fe4			/* PLAYBACK entry 0; entry m at + m * 0x192 */

/* The stock page descriptors (PARAM_PAGES.md section 3), entry start E:
 * minimum per slot at E + 0xa2, count at E + 0xd2, enable nibbles at
 * E + 0x1c6 (slots 0-7) and E + 0x1c2 (8-11), section 3b. */
#define D_LFO   0x400d37be
#define D_AMP   0x400d3950
#define D_NOTE  0x400d3e06
#define D_ARP   0x400d3f98
#define D_MLFO  0x400d412a
#define D_CTL1  0x400d42bc
#define D_CTL2  0x400d444e
static const u32 midi_desc[5] = { D_NOTE, D_MLFO, D_ARP, D_CTL1, D_CTL2 };

static void ent_id(u32 e, const char **id, u32 *len, u32 *layout)
{
	if (e >= ST_BASE) {
		const struct st_ent *s = &st_ent[e - ST_BASE];
		*id = s->id;
		*len = s->id_len;
		*layout = s->layout;
	} else {
		*id = brain_fx[e].id;
		*len = brain_fx[e].id_len;
		*layout = brain_fx[e].layout;
	}
}

/* One slot: its key, Part offset, page-1 index and range from descriptor
 * E's slot s (a raw byte when E is 0). A slot whose enable nibble has bit
 * 0 clear is neither stored nor written: the stock writers refuse it
 * (0x40054cd8 at 0x40054d20), and the stock MIDI pages' empty setup slots
 * and AMP's XVOL are those. */
static void slot(struct view *v, u32 key, u32 at, u32 flat, u32 E, u32 s)
{
	u32 i = v->n++;
	v->key[i] = (u16)key;
	v->at[i] = at;
	v->flat[i] = (u8)flat;
	v->lo[i] = 0;
	v->hi[i] = 255;
	if (E) {
		int min = (int)be32((const u8 *)E + 0xa2 + 4 * s), cnt = (int)be32((const u8 *)E + 0xd2 + 4 * s);
		v->lo[i] = (u8)min;
		v->hi[i] = (u8)(min + cnt - 1);
		u32 en = s < 8 ? be32((const u8 *)E + 0x1c6) >> (4 * s) : be32((const u8 *)E + 0x1c2) >> (4 * (s - 8));
		if (cnt <= 0 || !(en & 1)) {
			v->key[i] = 0;
			v->flat[i] = SKIP;
		}
	}
	v->cur[i] = (DBPTR + (CUR_PART & 3) * PART_LEN)[at];
}

/* View kind of track t: 0, or 1 with msg[] set when the view cannot hold a
 * template. An FX view needs its effect in brain_fx[] and a descriptor. */
static int view_fill(struct view *v, u32 kind, u32 t)
{
	v->kind = kind;
	v->t = t & 7;
	v->n = 0;
	t = v->t;
	if (kind <= V_FX2) {
		u8 *part = DBPTR + WORKING + (CUR_PART & 3) * PART_LEN;
		u32 id = part[kind ? 0x8 + t : t];
		v->e = MAX_FX;
		for (u32 i = 0; i < brain_fx_n && i < MAX_FX; i++)
			if (brain_fx[i].fx_id == id)
				v->e = i;
		if (v->e == MAX_FX) {
			msg[0] = "THIS EFFECT HAS";
			msg[1] = "NO TEMPLATES";
			return 0;
		}
		if (!snapped)
			snapshot();
		const u8 *d = desc[v->e][kind] ? desc[v->e][kind] : desc[v->e][kind ^ 1];
		if (!d)
			return 0;
		for (u32 s = 0; s < 12; s++) {
			u32 at = s < 6 ? 0x8ee9a + t * 24 + (kind ? 18 : 12) + s
				       : (kind ? 0x8f084 : 0x8f07e) + t * 30 + s - 6;
			slot(v, brain_fx[v->e].keys[s], at, s < 6 ? (kind ? 24 : 18) + s : NOFLAT,
			     (u32)d - 0x38, s);
		}
		return 1;
	}
	if (kind == V_SRC) {
		/* the machine byte, then its six page-1 and six page-2 bytes */
		u32 m = (DBPTR + (CUR_PART & 3) * PART_LEN)[0x8eda2 + t];
		if (m > 4)
			return 0;
		v->machine = m;
		v->e = ST_BASE + 3 + m;
		for (u32 s = 0; s < 12; s++)
			slot(v, 1 + s, (s < 6 ? 0x8edaa : 0x8ef5a - 6) + t * 30 + m * 6 + s, s < 6 ? s : NOFLAT,
			     D_PLAYBACK + m * 0x192, s);
		return 1;
	}
	v->e = ST_BASE + kind - V_AMP;
	if (kind == V_AMP)
		for (u32 s = 0; s < 12; s++)
			slot(v, 1 + s, s < 6 ? 0x8eea0 + t * 24 + s : 0x8f078 + t * 30 + s - 6,
			     s < 6 ? 12 + s : NOFLAT, D_AMP, s);
	else if (kind == V_LFO)
		for (u32 s = 0; s < 18; s++)
			slot(v, 1 + s, s < 6 ? 0x8ee9a + t * 24 + s
					     : s < 12 ? 0x8f072 + t * 30 + s - 6 : 0x8f08a + t * 30 + s - 12,
			     s < 6 ? 6 + s : NOFLAT, D_LFO, s < 6 ? s : 6 + (s - 6) / 3);
	else {
		for (u32 f = 0; f < 32; f++)	/* page 1: 30 values, CTRL1/CTRL2 enable bitmaps */
			slot(v, 64 + f, 0x8f162 + t * 32 + f, f < 30 ? f : NOFLAT,
			     f < 30 ? midi_desc[f / 6] : 0, f % 6);
		for (u32 o = 0; o < 36; o++)	/* LFO PMTR x3, WAVE x3 at 6-11; MULT x3, TRIG x3 at 30-35 */
			slot(v, 1 + o, 0x8f262 + t * 36 + o, NOFLAT, o < 30 ? midi_desc[o / 6] : D_MLFO,
			     o >= 30 ? 8 + (o - 30) / 3 : o / 6 == 1 ? 6 + (o - 6) / 3 : 6 + o % 6);
	}
	return 1;
}

/* The view in front of the page shortcut: 0, or 1 with msg[] set. */
static int view_here(struct view *v)
{
	u32 kind = PAGE_KIND;
	if (MIDI_MODE)
		return view_fill(v, V_MIDI, CUR_TRACK);
	if (kind <= 4)
		return view_fill(v, kind == 0 ? V_SRC : kind == 1 ? V_LFO : kind == 2 ? V_AMP : kind - 3, CUR_TRACK);
	msg[0] = "OPEN A TRACK PAGE";
	msg[1] = "";
	return 0;
}

/* The i-th template record of entry e among the records at recs[0..len),
 * or 0. */
static const u8 *nth_in(const u8 *recs, u32 len, u32 e, u32 i)
{
	const char *id;
	u32 id_len, layout;
	ent_id(e, &id, &id_len, &layout);
	for (u32 at = 0; at < len; at += be32(recs + at + 16)) {
		const u8 *p = recs + at;
		const u8 *pay = p + REC_HDR + align4(p[1]);
		if (!same_id(p + REC_HDR, p[1], id, id_len) || be32(p + 8) < 16 || be32(pay + 4) != layout)
			continue;
		if (p[0] != K_TEMPLATE)
			continue;
		if (i-- == 0)
			return p;
	}
	return 0;
}

static u32 img_count(u32 e)
{
	u32 n = 0;
	while (n < TPL_MAX && nth_in(brain_img_tpl, brain_img_tpl_len, e, n))
		n++;
	return n;
}

/* The i-th template of entry e: the image's first, then the card's. */
static const u8 *tpl_nth(u32 e, u32 i)
{
	u32 k = img_count(e);
	return i < k ? nth_in(brain_img_tpl, brain_img_tpl_len, e, i) : nth_in(tpl_cache, tpl_len, e, i - k);
}

static const u8 *card_nth(u32 e, u32 i)
{
	return nth_in(tpl_cache, tpl_len, e, i);
}

/* ---- apply: each slot through the stock paths ----------------------------- */

#define P1WRITE  ((void (*)(u32, u32, u32))0x40054cd8)	/* page-1 writer (track, flat, value) */
#define LIVEB    0x80000810				/* live lane: track*72 */
#define LIVEM    0x46c76dc0				/* MIDI live copy: track*0x44 */
#define CS1      0x1001614e				/* CS1 mirror of the bank: + Part offset */
#define MIDI_INVAL ((void (*)(void))0x4009a3e0)		/* resend bank and program */
#define MIDI_PROG  ((void (*)(u32, u32))0x4009ec70)	/* (bank, part): send bank and program */
#define CUR_BANK (*(volatile u8 *)0x80000002)

/* View v takes vals[]: page-1 slots through the stock writer, the others as
 * the editors store them (Part, CS1 mirror, live copy, the four dirty
 * flags; MIDI_TRACKS.md section 5, modules/cc-map), each value clamped to
 * its descriptor's range. A MIDI track sends its bank and program after
 * its setup, then its page-1 values. */
static void apply_view(const struct view *v, const u8 *vals)
{
	u32 part = CUR_PART & 3, t = v->t, chan = 0;
	u8 *pb = DBPTR + part * PART_LEN;
	u8 c[NSLOT];
	for (u32 i = 0; i < v->n; i++) {
		u32 x = vals[i];
		c[i] = (u8)(x < v->lo[i] ? v->lo[i] : x > v->hi[i] ? v->hi[i] : x);
		if (v->flat[i] != NOFLAT)
			continue;
		u32 at = v->at[i];
		if (at == 0x8f262 + t * 36 && pb[at] != c[i])
			chan = 1;
		pb[at] = c[i];
		*(volatile u8 *)(CS1 + at + part * PART_LEN) = c[i];
		if (v->kind == V_MIDI)
			*(volatile u8 *)(LIVEM + t * 0x44 + (at - 0x8f162 - t * 32 < 32 ? at - 0x8f162 - t * 32
							  : 0x20 + at - 0x8f262 - t * 36)) = c[i];
		else
			*(volatile u8 *)(LIVEB + t * 72 + (v->kind == V_SRC ? 0x20 + at - 0x8ef5a - t * 30 - v->machine * 6
								       : 0x26 + at - 0x8f072 - t * 30)) = c[i];
	}
	DBPTR[0x95048] |= (u8)(1 << part);
	*(volatile u8 *)0x100b145e |= (u8)(1 << part);
	*(volatile u32 *)(DBPTR + 0x9b332) = 1;
	*(volatile u32 *)0x100f8598 = 1;
	if (v->kind == V_MIDI) {
		if (chan)
			MIDI_INVAL();
		MIDI_PROG(CUR_BANK, part);
	}
	for (u32 i = 0; i < v->n; i++)
		if (v->flat[i] < SKIP)
			P1WRITE(v->kind == V_MIDI ? 8 + t : t, v->flat[i], c[i]);
}

/* The values of template record p for view v: its bytes by key; a slot it
 * has no value for keeps the current byte. */
static void tpl_values(const u8 *p, const struct view *v, u8 *vals)
{
	for (u32 i = 0; i < v->n; i++)
		vals[i] = v->cur[i];
	const u8 *pay = p + REC_HDR + align4(p[1]);
	u32 plen = be32(p + 8);
	for (u32 at = 16; at + 4 <= plen && pay[at + 3] != 0xff; at += 4 + align4(pay[at + 3])) {
		u32 key = be16(pay + at);
		if (pay[at + 2] != T_BYTE || pay[at + 3] != 1 || !key)
			continue;
		for (u32 i = 0; i < v->n; i++)
			if (v->key[i] == key)
				vals[i] = pay[at + 4];
	}
}

/* ---- the engine task: a template saved, deleted, renamed or copied ------- */

/* op T_SAVE: track t's view kind becomes a new template of its target,
 * named "TPL nn" with the lowest free nn. T_DELETE: the i-th card template
 * of entry e leaves card.work. T_RENAME: it takes the name in tpl_newname.
 * T_COPY: the i-th image template of entry e is added to card.work (error
 * 5 when a card template of the target has its name). Every other record
 * keeps its bytes. The copy is read back after the write. 0, or an error
 * code as write_default's. */
static struct view job_view;
static char tpl_newname[TPL_NAME + 1];

static int same_name(const u8 *rec, const u8 *nm)
{
	const u8 *a = rec + REC_HDR + align4(rec[1]) + 8;
	for (u32 k = 0; k < TPL_NAME; k++)
		if (a[k] != nm[k])
			return 0;
	return 1;
}

static int write_template(int del, u32 t, u32 kind, u32 e, u32 i)
{
	int n = current_store();
	if (n < 0)
		return 2;
	struct view *v = &job_view;
	const u8 *add = 0;			/* T_COPY: the image record */
	if (del == T_COPY) {
		add = nth_in(brain_img_tpl, brain_img_tpl_len, e, i);
		if (!add)
			return 1;
		for (u32 j = 0; n && j < TPL_MAX; j++) {
			const u8 *q = nth_in(buf + HDR, (u32)n - HDR, e, j);
			if (!q)
				break;
			if (same_name(q, add + REC_HDR + align4(add[1]) + 8))
				return 5;
		}
	} else if (!del) {
		if (kind > V_SRC || t > 7 || !view_fill(v, kind, t))
			return 1;
		e = v->e;
	}
	const char *id;
	u32 id_len, layout;
	ent_id(e, &id, &id_len, &layout);
	const u8 *drop = ((del == T_DELETE || del == T_RENAME) && n) ? nth_in(buf + HDR, (u32)n - HDR, e, i) : 0;
	if ((del == T_DELETE || del == T_RENAME) && !drop)
		return 1;
	u32 at = HDR, kept = 0;
	for (u32 r = 0, k = HDR; n && r < be16(buf + 10); r++, k += be32(buf + k + 16)) {
		const u8 *p = buf + k;
		u32 size = be32(p + 16);
		if (p == drop && del == T_DELETE)
			continue;
		if (at + size > BUF_LEN)
			return 3;
		for (u32 b = 0; b < size; b++)
			out[at + b] = p[b];
		if (p == drop) {		/* T_RENAME: the name, then the payload CRC */
			u8 *pay = out + at + REC_HDR + align4(p[1]);
			for (u32 c = 0; c < TPL_NAME; c++)
				pay[8 + c] = (u8)tpl_newname[c];
			put32(out + at + 12, crc32(pay, be32(p + 8)));
		}
		at += size;
		kept++;
	}
	if (add) {
		u32 size = be32(add + 16);
		if (at + size > BUF_LEN)
			return 3;
		for (u32 b = 0; b < size; b++)
			out[at + b] = add[b];
		at += size;
		kept++;
	}
	if (del == T_SAVE) {
		/* the lowest free number among this target's names */
		u32 num = 1;
		for (int again = 1; again && num < 100;) {
			again = 0;
			for (u32 j = 0; j < TPL_MAX; j++) {
				const u8 *q = nth_in(buf + HDR, n ? (u32)n - HDR : 0, e, j);
				if (!q)
					break;
				const u8 *nm = q + REC_HDR + align4(q[1]) + 8;
				if (nm[0] == 'T' && nm[1] == 'P' && nm[2] == 'L' && nm[3] == ' '
				    && (u32)((nm[4] - '0') * 10 + (nm[5] - '0')) == num) {
					num++;
					again = 1;
				}
			}
		}
		u32 nvals = 0, nnames = 0;
		for (u32 s = 0; s < v->n; s++)
			nvals += v->key[s] != 0;
		/* a MIDI track carries its knob names: Name values keyed 129 + cc */
		const char (*kn)[KN_LEN] = knames[kn_part()][t & 7];
		for (u32 c = 0; kind == V_MIDI && c < 10; c++)
			nnames += kn[c][0] != 0;
		u32 plen = 16 + 8 * nvals + 12 * nnames, size = REC_HDR + align4(id_len) + plen;
		if (at + size > BUF_LEN)
			return 3;
		u8 *r = out + at;
		for (u32 b = 0; b < size; b++)
			r[b] = 0;
		r[0] = K_TEMPLATE;
		r[1] = (u8)id_len;
		put16(r + 2, 1);			/* schema major 1, minor 0 */
		put32(r + 8, plen);
		put32(r + 16, size);
		for (u32 b = 0; b < id_len; b++)
			r[REC_HDR + b] = (u8)id[b];
		u8 *pay = r + REC_HDR + align4(id_len);
		put16(pay + 2, NO_MODE);		/* target 0, no mode */
		put32(pay + 4, layout);
		pay[8] = 'T'; pay[9] = 'P'; pay[10] = 'L'; pay[11] = ' ';
		pay[12] = (u8)('0' + num / 10);
		pay[13] = (u8)('0' + num % 10);
		u8 *w = pay + 16;
		for (u32 s = 0; s < v->n; s++)
			if (v->key[s]) {
				put16(w, v->key[s]);
				w[2] = T_BYTE;
				w[3] = 1;
				w[4] = v->cur[s];
				w += 8;
			}
		for (u32 c = 0; nnames && c < 10; c++)
			if (kn[c][0]) {
				put16(w, 129 + c);
				w[2] = T_NAME;
				w[3] = KN_LEN;
				for (u32 k = 0; k < KN_LEN; k++)
					w[4 + k] = (u8)kn[c][k];
				w += 12;
			}
		put32(r + 12, crc32(pay, plen));
		at += size;
		kept++;
	}
	if (write_card(at, kept) < 0)
		return 4;
	n = current_store();
	cache_templates(n > 0);
	return 0;
}

void brain_post_template(u32 args)
{
	job_args = args;
	((u8 *)job_msg)[0] = JOB_SAVE;
	Q_SEND(ENGINE_Q, job_msg);
}

/* ---- the page shortcut and the template lists ---------------------------- */

/* Hold a page key (SRC, AMP, LFO, FX1, FX2) and press FUNC: the record
 * manifest.py adds to the page-held key layer (0x400bab6e's keys) calls
 * brain_page_func, which opens the stock list popup (0x4006d94c(count,
 * sel, &sel_out, labels, handlers); YES stores the row in sel_out, closes
 * it and calls that row's handler, no argument). */
#define LISTPOP ((void (*)(u32, u32, u32 *, const char *const *, void *))0x4006d94c)

void brain_tpl_save(u32 unused);
void brain_tpl_load(u32 unused);
void brain_tpl_delete(u32 unused);

static u32 pop_sel;
void brain_kn_list(u32 unused);
static const char *const page_lbl[6] = { "SAVE AS DEFAULT", "CLEAR DEFAULT", "SAVE TEMPLATE",
					 "LOAD TEMPLATE", "DELETE TEMPLATE", "NAME KNOB" };
static void (*const page_fn[6])(u32) = { brain_menu_save, brain_menu_clear, brain_tpl_save,
					 brain_tpl_load, brain_tpl_delete, brain_kn_list };

/* NAME KNOB: on a MIDI track only */
void brain_page_func(void)
{
	pop_sel = 0;
	LISTPOP(MIDI_MODE ? 6 : 5, 0, &pop_sel, page_lbl, (void *)page_fn);
}

static char tpl_lbl[TPL_MAX][TPL_NAME + 1];
static const char *tpl_lp[TPL_MAX];
static void (*tpl_fp[TPL_MAX])(u32);
static u32 tpl_sel;
static struct view tpl_view;		/* the view the lists were opened over */

/* The names of the templates of the view in front (the image's and the
 * card's, or the card's alone), each row calling fn; their count (0: msg[]
 * says why, the popup shown). */
static u32 tpl_list(const char *title, void (*fn)(u32), int card)
{
	if (!view_here(&tpl_view)) {
		POPUP(title, 2, msg, 0, 0);
		return 0;
	}
	u32 n = 0;
	for (; n < TPL_MAX; n++) {
		const u8 *p = card ? card_nth(tpl_view.e, n) : tpl_nth(tpl_view.e, n);
		if (!p)
			break;
		const u8 *nm = p + REC_HDR + align4(p[1]) + 8;
		for (u32 k = 0; k < TPL_NAME; k++)
			tpl_lbl[n][k] = (char)nm[k];
		tpl_lbl[n][TPL_NAME] = 0;
		tpl_lp[n] = tpl_lbl[n];
		tpl_fp[n] = fn;
	}
	if (!n) {
		msg[0] = "NO TEMPLATES";
		msg[1] = "FOR THIS PAGE";
		POPUP(title, 2, msg, 0, 0);
	}
	return n;
}

static void tpl_apply_sel(u32 unused)
{
	(void)unused;
	const u8 *p = tpl_nth(tpl_view.e, tpl_sel);
	if (!p)
		return;
	u8 vals[NSLOT];
	tpl_values(p, &tpl_view, vals);
	apply_view(&tpl_view, vals);
	if (tpl_view.kind == V_MIDI && kn_take(p, tpl_view.t))
		brain_post_template(0x800000);
}

static void tpl_delete_sel(u32 unused)
{
	(void)unused;
	brain_post_template(0x100000 | (tpl_view.e << 8) | (tpl_sel & 0xff));
	msg[0] = tpl_lbl[tpl_sel < TPL_MAX ? tpl_sel : 0];
	msg[1] = "DELETED";
	POPUP("DELETE TEMPLATE", 2, msg, 0, 0);
}

void brain_tpl_load(u32 unused)
{
	(void)unused;
	u32 n = tpl_list("LOAD TEMPLATE", tpl_apply_sel, 0);
	if (n) {
		tpl_sel = 0;
		LISTPOP(n, 0, &tpl_sel, tpl_lp, (void *)tpl_fp);
	}
}

void brain_tpl_delete(u32 unused)
{
	(void)unused;
	u32 n = tpl_list("DELETE TEMPLATE", tpl_delete_sel, 1);
	if (n) {
		tpl_sel = 0;
		LISTPOP(n, 0, &tpl_sel, tpl_lp, (void *)tpl_fp);
	}
}

void brain_tpl_save(u32 unused)
{
	(void)unused;
	if (!view_here(&tpl_view)) {
		POPUP("SAVE TEMPLATE", 2, msg, 0, 0);
		return;
	}
	brain_post_template(0x80000 | tpl_view.t | (tpl_view.kind << 8));
	static const char *const kname[6] = { "FX1 TRACK ", "FX2 TRACK ", "AMP TRACK ", "LFO TRACK ",
					      "MIDI TRACK ", "SRC TRACK " };
	int k = 0;
	for (const char *c = kname[tpl_view.kind]; *c; c++)
		line1[k++] = *c;
	line1[k++] = (char)('1' + tpl_view.t);
	line1[k] = 0;
	msg[0] = line1;
	msg[1] = "TEMPLATE SAVED";
	POPUP("SAVE TEMPLATE", 2, msg, 0, 0);
}

/* ---- BRAIN > TEMPLATES ----------------------------------------------------- */

/* The back row, then per target with templates a heading (its store id
 * after the first '.') and its templates, the image's first, each marked
 * IMG. YES on a row opens the stock list popup: a card template takes
 * RENAME (the stock name editor, 0x4007e664(max, title, buffer, 1,
 * done(confirmed)), KITS' SAVE KIT call) and DELETE, an image template
 * COPY TO CARD. Each posts an engine job and returns the pane to the top,
 * so no row built from the old file is acted on. */

#define NAMEED   ((void (*)(u32, const char *, char *, u32, void (*)(u32)))0x4007e664)
#define TROWS    160				/* brain_tpl_rows after its back row (manifest.py) */

extern struct mrow brain_tpl_rows[];
static char trow_lbl[TROWS][PANE_W + 1];
static u16 trow_ref[TROWS + 1];			/* row -> e << 8 | i, bit 7 of i: image; 0xffff none */
static u32 trow_ref_sel;
static char tname[16];

static void trow_name(char *o, const u8 *rec, int img)
{
	const u8 *nm = rec + REC_HDR + align4(rec[1]) + 8;
	u32 k = 0;
	for (; k < TPL_NAME && nm[k]; k++)
		o[k] = (char)nm[k];
	if (img) {
		for (; k < PANE_W - 3; k++)
			o[k] = ' ';
		o[k++] = 'I'; o[k++] = 'M'; o[k++] = 'G';
	}
	o[k] = 0;
}

void brain_tpl_row(u32 unused);

void brain_open_templates(u32 unused)
{
	(void)unused;
	u32 n = 1, first = 0;
	for (u32 i = 0; i <= TROWS; i++)
		trow_ref[i] = 0xffff;
	for (u32 e = 0; e < ST_BASE + ST_N && n < TROWS; e++) {
		if (e >= brain_fx_n && e < ST_BASE)
			continue;
		u32 ki = img_count(e), kc = 0;
		while (kc < TPL_MAX && card_nth(e, kc))
			kc++;
		if (!ki && !kc)
			continue;
		const char *id;
		u32 id_len, layout;
		ent_id(e, &id, &id_len, &layout);
		u32 d = 0;
		while (d < id_len && id[d] != '.')
			d++;
		d = d < id_len ? d + 1 : 0;
		char *h = trow_lbl[n - 1];
		u32 k = 0;
		for (; d < id_len && k < PANE_W; d++, k++)
			h[k] = (char)(id[d] >= 'a' && id[d] <= 'z' ? id[d] - 32 : id[d]);
		h[k] = 0;
		put_row(brain_tpl_rows + n++, h, 0);
		for (u32 j = 0; j < ki + kc && n < TROWS; j++) {
			int img = j < ki;
			const u8 *rec = img ? nth_in(brain_img_tpl, brain_img_tpl_len, e, j) : card_nth(e, j - ki);
			trow_name(trow_lbl[n - 1], rec, img);
			trow_ref[n] = (u16)(e << 8 | (img ? 0x80 | j : j - ki));
			put_row(brain_tpl_rows + n, trow_lbl[n - 1], brain_tpl_row);
			if (!first)
				first = n;
			n++;
		}
	}
	if (n == 1) {
		put_row(brain_tpl_rows + 1, "NO TEMPLATES", 0);
		n = 2;
	}
	brain_from = 2;
	show(brain_tpl_rows, n, first ? first : 1);
}

static void tpl_job(u32 args, const char *done)
{
	brain_post_template(args);
	msg[0] = tname;
	msg[1] = done;
	POPUP("TEMPLATES", 2, msg, 0, 0);
	brain_menu_back(0);
}

static void tpl_renamed(u32 confirmed)
{
	if (!confirmed)
		return;
	for (u32 k = 0; k <= TPL_NAME; k++)
		tpl_newname[k] = 0;
	for (u32 k = 0; k < TPL_NAME && tname[k]; k++)
		tpl_newname[k] = tname[k];
	if (!tpl_newname[0])
		return;
	tpl_job(0x200000 | trow_ref_sel, "RENAMED");
}

static void trow_rename(u32 unused)
{
	(void)unused;
	NAMEED(TPL_NAME, "TEMPLATE NAME", tname, 1, tpl_renamed);
}

static void trow_delete(u32 unused)
{
	(void)unused;
	tpl_job(0x100000 | trow_ref_sel, "DELETED");
}

static void trow_copy(u32 unused)
{
	(void)unused;
	tpl_job(0x400000 | (trow_ref_sel & 0xff7f), "COPIED TO CARD");
}

static const char *const trow_card_lbl[2] = { "RENAME", "DELETE" };
static void (*const trow_card_fn[2])(u32) = { trow_rename, trow_delete };
static const char *const trow_img_lbl[1] = { "COPY TO CARD" };
static void (*const trow_img_fn[1])(u32) = { trow_copy };

void brain_tpl_row(u32 unused)
{
	(void)unused;
	u32 r = brain_list.sel;
	if (r > TROWS || trow_ref[r] == 0xffff)
		return;
	trow_ref_sel = trow_ref[r];
	u32 e = trow_ref_sel >> 8, i = trow_ref_sel & 0x7f;
	const u8 *rec = trow_ref_sel & 0x80 ? nth_in(brain_img_tpl, brain_img_tpl_len, e, i) : card_nth(e, i);
	if (!rec)
		return;
	for (u32 k = 0; k < sizeof tname; k++)
		tname[k] = 0;
	const u8 *nm = rec + REC_HDR + align4(rec[1]) + 8;
	for (u32 k = 0; k < TPL_NAME && nm[k]; k++)
		tname[k] = (char)nm[k];
	pop_sel = 0;
	if (trow_ref_sel & 0x80)
		LISTPOP(1, 0, &pop_sel, trow_img_lbl, (void *)trow_img_fn);
	else
		LISTPOP(2, 0, &pop_sel, trow_card_lbl, (void *)trow_card_fn);
}

/* ---- NAME KNOB: the page shortcut on a MIDI track ------------------------- */

/* The current MIDI track's ten CCs, each "CCn  NAME" (---- without one); YES
 * opens the name editor (four characters) on that CC's name, and on
 * confirm the engine task rewrites the project's knob-name field blocks
 * (write_names). An empty name clears it. */
static char kn_lbl[10][16];
static const char *kn_lp[10];
static void (*kn_fp[10])(u32);
static u32 kn_sel, kn_t, kn_bp;
static char kn_buf[16];

static void kn_named(u32 confirmed)
{
	if (!confirmed || kn_sel > 9)
		return;
	char *o = knames[kn_bp][kn_t][kn_sel];
	u32 k = 0;
	for (; k < KN_CHARS && kn_buf[k]; k++)
		o[k] = kn_buf[k];
	while (k > 0 && o[k - 1] == ' ')
		k--;
	for (; k < KN_LEN; k++)
		o[k] = 0;
	brain_post_template(0x800000);
}

static void kn_pick(u32 unused)
{
	(void)unused;
	if (kn_sel > 9)
		return;
	for (u32 k = 0; k < sizeof kn_buf; k++)
		kn_buf[k] = 0;
	const char *n = knames[kn_bp][kn_t][kn_sel];
	for (u32 k = 0; k < KN_CHARS && n[k]; k++)
		kn_buf[k] = n[k];
	NAMEED(KN_CHARS, "KNOB NAME", kn_buf, 1, kn_named);
}

void brain_kn_list(u32 unused)
{
	(void)unused;
	if (!MIDI_MODE)
		return;
	kn_t = CUR_TRACK & 7;
	kn_bp = kn_part();
	for (u32 c = 0; c < 10; c++) {
		char *o = kn_lbl[c];
		u32 k = 0;
		o[k++] = 'C'; o[k++] = 'C';
		if (c == 9) {
			o[k++] = '1'; o[k++] = '0';
		} else
			o[k++] = (char)('1' + c);
		while (k < 6)
			o[k++] = ' ';
		const char *n = knames[kn_bp][kn_t][c];
		for (u32 j = 0; j < KN_CHARS; j++)
			o[k++] = n[0] ? (n[j] ? n[j] : ' ') : '-';
		o[k] = 0;
		kn_lp[c] = o;
		kn_fp[c] = kn_pick;
	}
	kn_sel = 0;
	LISTPOP(10, 0, &kn_sel, kn_lp, (void *)kn_fp);
}

/* The engine task: the project's brain.work with one stock.midi field
 * block per Part that has a name, every other record kept. 0, or an error
 * code as write_set_file's. */
static int write_names(void)
{
	if (!proj_paths())
		return 5;
	int n = cur(pw, ps);
	if (n < 0)
		return 2;
	u32 at = HDR, kept = 0;
	for (u32 r = 0, i = HDR; n && r < be16(buf + 10); r++, i += be32(buf + i + 16)) {
		const u8 *p = buf + i;
		u32 size = be32(p + 16);
		if (p[0] == K_FIELD && same_id(p + REC_HDR, p[1], KN_STORE, 10))
			continue;
		if (at + size > BUF_LEN)
			return 3;
		for (u32 b = 0; b < size; b++)
			out[at + b] = p[b];
		at += size;
		kept++;
	}
	for (u32 bp = 0; bp < KN_PARTS; bp++) {
		u32 nn = 0;
		for (u32 t = 0; t < 8; t++)
			for (u32 c = 0; c < 10; c++)
				nn += knames[bp][t][c][0] != 0;
		if (!nn)
			continue;
		u32 plen = 4 + 12 * nn, size = REC_HDR + align4(10) + plen;
		if (at + size > BUF_LEN)
			return 3;
		u8 *r = out + at;
		for (u32 b = 0; b < size; b++)
			r[b] = 0;
		r[0] = K_FIELD;
		r[1] = 10;
		put16(r + 2, 1);
		put32(r + 8, plen);
		put32(r + 16, size);
		for (u32 b = 0; b < 10; b++)
			r[REC_HDR + b] = (u8)KN_STORE[b];
		u8 *pay = r + REC_HDR + align4(10);
		put16(pay, bp);
		u8 *w = pay + 4;
		for (u32 t = 0; t < 8; t++)
			for (u32 c = 0; c < 10; c++)
				if (knames[bp][t][c][0]) {
					put16(w, t * 10 + c + 1);
					w[2] = T_NAME;
					w[3] = KN_LEN;
					for (u32 k = 0; k < KN_LEN; k++)
						w[4 + k] = (u8)knames[bp][t][c][k];
					w += 12;
				}
		put32(r + 12, crc32(pay, plen));
		at += size;
		kept++;
	}
	return write_store(pw, 2, at, kept) < 0 ? 4 : 0;
}

/* ---- project settings for new projects (BRAIN.md section 3) -------------- */

/* The stock PROJECT menu's bytes (docs/firmware/STORAGE.md section 5:
 * bytes from 0x80000000, each mirrored in CS1 at 0x100b1460 + the same
 * offset), TEMPOx24 left out: SAVE PROJ SETUP stores them as a card-layer
 * default of store stock.project (keys 1 + offset, raw bytes); CREATE
 * EMPTY PROJECT applies it right after the empty project's RAM is set up
 * (0x400909d8; the detour at 0x40085176, hooks.s brain_on_newproj), so the
 * new project.work is written with them. */
#define PS_STORE   "stock.project"
#define PS_LAYOUT  0x50524a31u			/* "PRJ1" */
#define PS_RAM     0x80000000u
#define PS_CS1     0x100b1460u
static int ps_byte(u32 off)
{
	return off == 0x24 || (off >= 0x28 && off <= 0x37) || (off >= 0x3f && off <= 0x54)
	       || (off >= 0x56 && off <= 0x6a);
}

/* NP_RUNS: new projects seen; NP_VALUES: bytes the last one took */
enum { NP_RUNS, NP_VALUES, NP_N };
u32 brain_np_counts[NP_N] DATA;

/* engine task: SAVE PROJ SETUP (clear 0) or CLEAR PROJ SETUP (clear 1) */
static int write_pset(int clear)
{
	int n = current_store();
	if (n < 0)
		return 2;
	u32 at = HDR, kept = 0;
	for (u32 r = 0, i = HDR; n && r < be16(buf + 10); r++, i += be32(buf + i + 16)) {
		const u8 *p = buf + i;
		u32 size = be32(p + 16);
		if (p[0] == K_DEFAULT && same_id(p + REC_HDR, p[1], PS_STORE, 13))
			continue;
		if (at + size > BUF_LEN)
			return 3;
		for (u32 b = 0; b < size; b++)
			out[at + b] = p[b];
		at += size;
		kept++;
	}
	if (!clear) {
		u32 nv = 0;
		for (u32 off = 0; off < 0x70; off++)
			nv += ps_byte(off);
		u32 plen = 8 + 8 * nv, size = REC_HDR + align4(13) + plen;
		if (at + size > BUF_LEN)
			return 3;
		u8 *r = out + at;
		for (u32 b = 0; b < size; b++)
			r[b] = 0;
		r[0] = K_DEFAULT;
		r[1] = 13;
		put16(r + 2, 1);
		put32(r + 8, plen);
		put32(r + 16, size);
		for (u32 b = 0; b < 13; b++)
			r[REC_HDR + b] = (u8)PS_STORE[b];
		u8 *pay = r + REC_HDR + align4(13);
		put16(pay + 2, NO_MODE);
		put32(pay + 4, PS_LAYOUT);
		u8 *w = pay + 8;
		for (u32 off = 0; off < 0x70; off++)
			if (ps_byte(off)) {
				put16(w, 1 + off);
				w[2] = T_BYTE;
				w[3] = 1;
				w[4] = *(volatile u8 *)(PS_RAM + off);
				w += 8;
			}
		put32(r + 12, crc32(pay, plen));
		at += size;
		kept++;
	}
	return write_card(at, kept) < 0 ? 4 : 0;
}

/* engine task, inside CREATE EMPTY PROJECT: the card's stock.project
 * default onto the settings bytes and their CS1 mirrors */
void brain_new_project(void)
{
	u32 n;
	brain_np_counts[NP_RUNS]++;
	brain_np_counts[NP_VALUES] = 0;
	int st = pair_load("/BRAIN/card.work", "/BRAIN/card.strd", &n);
	if (st != 1 && st != 2)
		return;
	u32 count = be16(buf + 10);
	for (u32 r = 0, at = HDR; r < count; r++, at += be32(buf + at + 16)) {
		const u8 *p = buf + at;
		const u8 *pay = p + REC_HDR + align4(p[1]);
		u32 plen = be32(p + 8);
		if (p[0] != K_DEFAULT || !same_id(p + REC_HDR, p[1], PS_STORE, 13) || plen < 8
		    || crc32(pay, plen) != be32(p + 12) || be32(pay + 4) != PS_LAYOUT)
			continue;
		for (u32 v = 8; v + 4 <= plen && pay[v + 3] != 0xff; v += 4 + align4(pay[v + 3])) {
			u32 key = be16(pay + v);
			if (pay[v + 2] != T_BYTE || pay[v + 3] != 1 || key < 1 || !ps_byte(key - 1))
				continue;
			*(volatile u8 *)(PS_RAM + key - 1) = pay[v + 4];
			*(volatile u8 *)(PS_CS1 + key - 1) = pay[v + 4];
			brain_np_counts[NP_VALUES]++;
		}
		return;
	}
}

void brain_menu_psave(u32 unused)
{
	(void)unused;
	brain_post_template(0x1000000);
	msg[0] = "PROJECT SETTINGS";
	msg[1] = "SAVED FOR NEW PROJ";
	POPUP("DEFAULTS", 2, msg, 0, 0);
}

void brain_menu_pclear(u32 unused)
{
	(void)unused;
	brain_post_template(0x2000000);
	msg[0] = "PROJECT SETTINGS";
	msg[1] = "CLEARED";
	POPUP("DEFAULTS", 2, msg, 0, 0);
}

/* ---- live popups: TOOLS > MIDI MONITOR and TOOLS > LOAD ------------------- */

/* The stock dialog (0x4006d57c) with three lines, redrawn every half
 * second by a soft timer (0x40031a0c, the sys task's ~120 Hz tick): the
 * tick writes the dialog's copies of its lines (0x460e5d28, 31 bytes
 * apart) and redraws it (0x4006d128), REMIX SWITCH's boot picker's
 * sequence (PR #542). The tick stops itself when the dialog closes or
 * another replaces it, by clearing its slot's function (the service
 * re-links a node only while that word is nonzero); the answer handler
 * stops it with UNTIMER, which is only called from outside the tick. */

#define TIMER     ((int (*)(u32, void (*)(void)))0x40031a0c)
#define UNTIMER   ((void (*)(int))0x40031abc)
#define TIMERS    0x460d5a28			/* 32 slots, 16 bytes: {period, due, fn, next} */
#define DLG_OPEN  (*(volatile u32 *)0x460e5cd0)	/* the dialog's window, 0 = none */
#define DLG_LINES ((char *)0x460e5d28)
#define DLG_DRAW  ((void (*)(void))0x4006d128)
#define LIVE_TICKS 60				/* about 0.5 s */
#define LIVE_W    22				/* characters a dialog line shows */

enum { LIVE_MIDI = 1, LIVE_LOAD = 2 };

static int live_timer DATA = -1;
static u32 live_kind DATA;
static u32 live_win DATA;			/* the dialog window ours opened as */
static char live_lbuf[3][LIVE_W + 1];
static const char *live_lp[3] = { live_lbuf[0], live_lbuf[1], live_lbuf[2] };

/* LOAD METER's window (modules/load-meter: seq, window, sum, count,
 * longest, in DMA timer 3 counts at 132 MHz), or 0 without that module
 * (hooks.s brain_lm, the LM_STATS defsym). */
extern volatile u32 *const brain_lm;
static u32 load_peak_pct DATA, load_peak_us DATA;

static void line_set(u32 i, const char *a, u32 an)
{
	u32 k = 0;
	for (; k < an && k < LIVE_W; k++)
		live_lbuf[i][k] = a[k];
	live_lbuf[i][k] = 0;
}

static void fill_load(void)
{
	char t[LIVE_W + 8];
	u32 n;
	if (!brain_lm) {
		line_set(0, "NO LOAD METER", 13);
		line_set(1, "IN THIS REMIX", 13);
		line_set(2, "", 0);
		return;
	}
	u32 seq = brain_lm[0], win = brain_lm[1], sum = brain_lm[2], cnt = brain_lm[3], max = brain_lm[4];
	if (!seq || !win || !cnt) {
		line_set(0, "MEASURING", 9);
		line_set(1, "", 0);
		line_set(2, "", 0);
		return;
	}
	u32 pct = sum / (win / 100 ? win / 100 : 1), us = max / 132, avg = sum / cnt / 132;
	u32 ms = win / 132000;					/* the window in ms (no 64-bit maths here) */
	u32 rate = ms ? cnt * 1000 / ms : 0;
	if (pct > load_peak_pct)
		load_peak_pct = pct;
	if (us > load_peak_us)
		load_peak_us = us;
	n = put_s(t, "FRAME IRQ ");				/* the interrupt's share */
	n += put_dec(t + n, pct, 3);
	n += put_s(t + n, "% PK ");
	n += put_dec(t + n, load_peak_pct, 3);
	n += put_s(t + n, "%");
	line_set(0, t, n);
	n = put_s(t, "AVG ");					/* the run, average and longest */
	n += put_dec(t + n, avg, 3);
	n += put_s(t + n, "US MAX ");
	n += put_dec(t + n, us, 3);
	n += put_s(t + n, "US");
	line_set(1, t, n);
	n = put_dec(t, rate, 0);
	n += put_s(t + n, " /S  PK ");
	n += put_dec(t + n, load_peak_us, 0);
	n += put_s(t + n, "US");
	line_set(2, t, n);
}

/* One logged message as text: "CH01 CC 007=020", "CH10 NOTE 036 100",
 * "START" ... */
static u32 midi_text(char *t, const struct dbg_ent *e)
{
	u32 st = e->st, n = 0;
	static const char *const sys[16] = { "SYSEX", "MTC", "SPP", "SONG", "", "", "TUNE", "EOX",
					     "CLOCK", "", "START", "CONT", "STOP", "", "SENSE", "RESET" };
	if (st >= 0xf0)
		return put_s(t, sys[st & 15]);
	n = put_s(t, "CH");
	n += put_dec(t + n, (st & 15) + 1, 2);
	if (t[2] == ' ')
		t[2] = '0';
	switch (st >> 4) {
	case 0x8: n += put_s(t + n, " OFF "); break;
	case 0x9: n += put_s(t + n, " NOTE "); break;
	case 0xa: n += put_s(t + n, " PAT "); break;
	case 0xb: n += put_s(t + n, " CC "); break;
	case 0xc: n += put_s(t + n, " PC "); n += put_dec(t + n, e->d1, 3);
		  n += put_s(t + n, " P"); return n + put_dec(t + n, e->pnext + 1, 0);
	case 0xd: n += put_s(t + n, " CP "); return n + put_dec(t + n, e->d1, 3);
	case 0xe: n += put_s(t + n, " PB "); return n + put_dec(t + n, ((u32)e->d2 << 7) | e->d1, 5);
	default: return n;
	}
	n += put_dec(t + n, e->d1, 3);
	t[n++] = (st >> 4) == 0xb ? '=' : ' ';
	return n + put_dec(t + n, e->d2, 3);
}

static void fill_midi(void)
{
	char t[LIVE_W + 8];
	if (!brain_v_octabam_brain_1) {
		line_set(0, "MIDI LOG IS OFF", 15);
		line_set(1, "SETTINGS > MIDI LOG", 19);
		line_set(2, "", 0);
		return;
	}
	for (u32 i = 0; i < 3; i++) {
		u32 back = 3 - i;			/* the newest last */
		if (dbg_seq < back) {
			line_set(i, i == 0 && !dbg_seq ? "NO MESSAGES YET" : "", i == 0 && !dbg_seq ? 15 : 0);
			continue;
		}
		const struct dbg_ent *e = &dbg_ring[(dbg_seq - back) % LOG_N];
		line_set(i, t, midi_text(t, e));
	}
}

static void live_fill(void)
{
	if (live_kind == LIVE_LOAD)
		fill_load();
	else
		fill_midi();
}

void brain_live_tick(void)
{
	if (live_timer < 0)
		return;
	if (!DLG_OPEN || DLG_OPEN != live_win) {
		*(volatile u32 *)(TIMERS + (u32)live_timer * 16 + 8) = 0;	/* not re-linked */
		live_timer = -1;
		return;
	}
	live_fill();
	for (u32 i = 0; i < 3; i++) {
		char *d = DLG_LINES + 31 * i;
		u32 k = 0;
		for (; live_lbuf[i][k] && k < 30; k++)
			d[k] = live_lbuf[i][k];
		d[k] = 0;
	}
	DLG_DRAW();
}

void brain_live_answer(u32 answer)
{
	(void)answer;
	if (live_timer >= 0) {
		UNTIMER(live_timer);
		live_timer = -1;
	}
}

static void live_open(u32 kind, const char *title)
{
	brain_live_answer(0);
	live_kind = kind;
	if (kind == LIVE_LOAD)
		load_peak_pct = load_peak_us = 0;
	live_fill();
	POPUP(title, 3, live_lp, 3, (u32)brain_live_answer);
	live_win = DLG_OPEN;
	live_timer = TIMER(LIVE_TICKS, brain_live_tick);
}

void brain_menu_midimon(u32 unused)
{
	(void)unused;
	live_open(LIVE_MIDI, "MIDI MONITOR");
}

void brain_menu_load(u32 unused)
{
	(void)unused;
	live_open(LIVE_LOAD, "LOAD");
}

/* FUNC + LEVEL push on an MKII: the record manifest.py adds to the FUNC
 * layer's keys; the record's press handler. */
void brain_load_chord(void)
{
	brain_menu_load(0);
}
