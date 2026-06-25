/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and
 *                    University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
 */

#include <string.h>
#include <algorithm>
#include <vector>
#include <vp/controller.hpp>

// Resolve trace PC -> symbol at runtime from the ELF binary. This block is the
// portable replacement for the former libdwfl/elfutils dependency, which has no
// supported macOS build. It splits the job the way libdwfl did internally:
//
//   * function name  <- ELF symbol table (covers assembly labels and compiler
//                       .isra/.constprop clones that have no DWARF DIE). Parsed
//                       once at registration with a tiny self-contained reader
//                       (no libelf / no <elf.h>, so it builds anywhere).
//   * file and line  <- DWARF .debug_line, via libdwarf (which bundles its own
//                       object reader, so neither libelf nor libdw is needed).
//
// Registration stays cheap: it reads the symbol table and each compilation
// unit's address range (the CU root DIE only), but defers every CU's line
// program until the first PC actually lands in it. A run that never resolves a
// symbol, or that only touches part of a big binary, pays nothing for the rest
// -- mirroring how libdwfl deferred and per-CU-cached its parsing.
//
// The block is excluded for the 32-bit model variant, which has no libdwarf
// available and therefore no trace symbols.
#if !defined(__M32_MODE__)
#include <map>
#include <string>
#include <libdwarf.h>
#include <dwarf.h>

// A function symbol from the ELF symbol table (high == low means size 0).
struct iss_sym_entry { uint64_t low, high; uint32_t idx; std::string name; };
struct iss_line_entry { uint64_t addr; int line; std::string file; bool end; };

// One compilation unit: known by DIE offset, its line table parsed lazily.
struct iss_cu_info
{
    Dwarf_Off off = 0;
    bool parsed = false;
    std::vector<iss_line_entry> lines;   // sorted by addr (filled on parse)
};

// One contiguous address range -> the CU that owns it (CU root DW_AT_low/high).
struct iss_arange { uint64_t low, high; Dwarf_Off cu_off; };

// One registered binary: symbol table (for names) + lazy DWARF line index.
struct iss_binary_info
{
    Dwarf_Debug dbg = NULL;
    std::vector<iss_sym_entry> syms;            // sorted by (low, idx)
    std::vector<iss_arange> aranges;            // sorted by low
    std::map<Dwarf_Off, iss_cu_info> cus;       // keyed by CU DIE offset
    // Non-contiguous (DW_AT_ranges) CUs are parsed eagerly here at registration,
    // since a single range cannot index them; their line rows go in all_lines.
    std::vector<iss_line_entry> all_lines;      // sorted by addr
};

static std::vector<iss_binary_info> iss_dw_binaries;
#endif

Trace::Trace(Iss &iss)
    : iss(iss)
{
    this->iss.traces.new_trace("insn", &this->insn_trace, vp::DEBUG);
    iss_trace_init(&this->iss);

#if !defined(__M32_MODE__)
    // Register the ELF binaries; trace symbols are resolved lazily on demand.
    js::Config *binaries_config = this->iss.get_js_config()->get("**/binaries");
    if (binaries_config != NULL)
    {
        for (auto x : binaries_config->get_elems())
        {
            iss_register_debug_elf(&this->iss, x->get_str().c_str());
        }
    }
#endif

    this->first_entry = NULL;

    this->iss.traces.new_trace_event_string("asm", &insn_trace_event);
    this->iss.traces.new_trace_event_string("func", &func_trace_event);
    this->iss.traces.new_trace_event_string("inline_func", &inline_trace_event);
    this->iss.traces.new_trace_event_string("file", &file_trace_event);
    this->iss.traces.new_trace_event("line", &line_trace_event, 32);
    this->iss.traces.new_trace_event_string("binaries", &binaries_trace_event);
    this->iss.traces.new_trace_event_string("user_func", &user_func_trace_event);
    this->iss.traces.new_trace_event_string("user_inline_func", &user_inline_trace_event);
    this->iss.traces.new_trace_event_string("user_file", &user_file_trace_event);
    this->iss.traces.new_trace_event("user_line", &user_line_trace_event, 32);

}

TraceEntry *Trace::get_entry()
{
    if (!this->first_entry)
    {
        this->first_entry = new TraceEntry();
        this->first_entry->next = NULL;
    }

    return this->first_entry;
}

void Trace::release_entry(TraceEntry *entry)
{
    entry->next = this->first_entry;
    this->first_entry = entry;
}

void Trace::reset(bool active)
{
    if (active)
    {
        if (this->declare_binaries)
        {
            if (this->iss.get_js_config()->get("**/binaries") != NULL)
            {
                std::string binaries = "static_enable";
                for (auto x : this->iss.get_js_config()->get("**/binaries")->get_elems())
                {
                    binaries += ":" + x->get_str();
                    // Alongside the GUI-facing "binaries" trace event, tell any connected proxy
                    // front-end (e.g. the console) about each binary so it can auto-load its
                    // symbols. Done at reset (not in the Trace ctor) so the component is linked
                    // into the hierarchy and get_launcher() can reach the real launcher.
                    this->iss.get_launcher()->declare_binary(x->get_str());
                }

                this->binaries_trace_event.event_string(binaries.c_str(), true);
            }
            this->declare_binaries = false;
        }
    }
}

#define PC_INFO_ARRAY_SIZE (64 * 1024)

#define MAX_DEBUG_INFO_WIDTH 32

class iss_pc_info
{
public:
    unsigned int base;
    char *func;
    char *inline_func;
    char *file;
    int line;
    // False for a negative-cache entry: the PC was looked up but no symbol was
    // found. Stored so a fruitless libdw lookup is not repeated on every hit.
    bool valid;
    iss_pc_info *next;
};

static bool pc_infos_is_init = false;
static iss_pc_info *pc_infos[PC_INFO_ARRAY_SIZE];
static std::vector<std::string> binaries;

static iss_pc_info *get_pc_info(unsigned int base)
{
    int index = base & (PC_INFO_ARRAY_SIZE - 1);

    iss_pc_info *pc_info = pc_infos[index];

    while (pc_info && pc_info->base != base)
    {
        pc_info = pc_info->next;
    }

    return pc_info;
}

#if !defined(__M32_MODE__)
static iss_pc_info *add_pc_info(unsigned int base, const char *func, const char *inline_func,
    const char *file, int line, bool valid)
{
    iss_pc_info *pc_info = new iss_pc_info();

    int index = base & (PC_INFO_ARRAY_SIZE - 1);
    pc_info->next = pc_infos[index];
    pc_infos[index] = pc_info;

    pc_info->base = base;
    pc_info->func = strdup(func);
    pc_info->inline_func = strdup(inline_func);
    pc_info->file = strdup(file);
    pc_info->line = line;
    pc_info->valid = valid;

    return pc_info;
}

// Forward decl: parse one CU's line table on demand.
static void iss_dw_parse_cu(iss_binary_info &b, iss_cu_info &cu);

// Function name for addr from the ELF symbol table, matching libdwfl's lookup:
// prefer the nearest *sized* symbol that actually contains addr; otherwise fall
// back to the nearest preceding symbol (covers zero-sized assembly labels).
static const char *iss_sym_func(const std::vector<iss_sym_entry> &syms, iss_addr_t addr)
{
    auto it = std::upper_bound(syms.begin(), syms.end(), (uint64_t)addr,
        [](uint64_t a, const iss_sym_entry &e) { return a < e.low; });
    if (it == syms.begin())
        return NULL;

    const iss_sym_entry *fallback = NULL;
    for (auto j = it; j != syms.begin(); )
    {
        --j;
        if (!fallback)
            fallback = &*j;                            // nearest preceding symbol
        if (j->high > j->low && addr < j->high)
            return j->name.c_str();                    // nearest sized container
        if (j->high > j->low && j->high <= addr)
            break;                                     // a sized symbol ends before addr
    }
    return fallback ? fallback->name.c_str() : NULL;
}

// file:line for addr from a CU's (already sorted) line table. The last entry
// with addr <= pc wins; an end_sequence marker means "no line".
static bool iss_dw_fileline(const std::vector<iss_line_entry> &lines, iss_addr_t addr,
    const char **file, int *line)
{
    auto lit = std::upper_bound(lines.begin(), lines.end(), (uint64_t)addr,
        [](uint64_t a, const iss_line_entry &e) { return a < e.addr; });
    if (lit == lines.begin())
        return false;
    --lit;
    if (lit->end)
        return false;
    *file = lit->file.c_str();
    *line = lit->line;
    return true;
}

// Resolve a PC across the registered binaries -- function from the symbol table,
// file:line from DWARF (parsing the owning CU on first touch) -- then store the
// result (positive or negative) in the pc_info hash so it acts as a cache.
static iss_pc_info *iss_libdw_resolve(iss_addr_t addr)
{
    const char *func = NULL;
    const char *file = NULL;
    int line = 0;

    for (auto &b : iss_dw_binaries)
    {
        const char *bf = iss_sym_func(b.syms, addr);

        // Lazy path: the contiguous CU owning addr, its line table parsed on
        // first touch.
        bool got_line = false;
        auto it = std::upper_bound(b.aranges.begin(), b.aranges.end(), (uint64_t)addr,
            [](uint64_t a, const iss_arange &e) { return a < e.low; });
        if (it != b.aranges.begin())
        {
            --it;
            if (addr >= it->low && addr < it->high)
            {
                iss_cu_info &cu = b.cus[it->cu_off];
                iss_dw_parse_cu(b, cu);
                got_line = iss_dw_fileline(cu.lines, addr, &file, &line);
            }
        }
        // Eager fallback: line rows of non-contiguous (DW_AT_ranges) CUs.
        if (!got_line)
            got_line = iss_dw_fileline(b.all_lines, addr, &file, &line);

        if (bf != NULL || got_line)
        {
            func = bf;
            break;
        }
    }

    if (func != NULL || file != NULL)
    {
        // inline_func mirrors func (no separate inline-frame info).
        return add_pc_info(addr, func ? func : "-", func ? func : "-",
            file ? file : "-", line, true);
    }

    return add_pc_info(addr, "-", "-", "-", 0, false);
}
#endif

// Cache lookup with, in libdw mode, a lazy DWARF resolution on miss.
static iss_pc_info *iss_pc_info_get(iss_addr_t addr)
{
    iss_pc_info *info = get_pc_info(addr);
    if (info != NULL)
    {
        return info;
    }

#if !defined(__M32_MODE__)
    return iss_libdw_resolve(addr);
#else
    return NULL;
#endif
}

int iss_trace_pc_info(iss_addr_t addr, const char **func, const char **inline_func, const char **file, int *line)
{
    iss_pc_info *info = iss_pc_info_get(addr);
    if (info == NULL || !info->valid)
        return -1;

    *func = info->func;
    *inline_func = info->inline_func;
    *file = info->file;
    *line = info->line;

    return 0;
}

#if !defined(__M32_MODE__)
// --- ELF symbol table reader (function names) -------------------------------
//
// A tiny, self-contained reader: no libelf and no <elf.h>, parsing the symtab
// with explicit field offsets so it builds anywhere (incl. macOS). It handles
// ELF32/ELF64 and both byte orders. libdwfl named addresses from the symbol
// table, so this is what gives parity for assembly labels (STT_NOTYPE) and
// compiler .isra/.constprop clones that have no DWARF subprogram DIE.

static bool iss_elf_le = true;   // target byte order of the binary being read
static uint16_t iss_rd16(const uint8_t *p)
{
    return iss_elf_le ? (uint16_t)(p[0] | p[1] << 8) : (uint16_t)(p[1] | p[0] << 8);
}
static uint32_t iss_rd32(const uint8_t *p)
{
    return iss_elf_le ? ((uint32_t)p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24)
                      : ((uint32_t)p[3] | p[2] << 8 | p[1] << 16 | (uint32_t)p[0] << 24);
}
static uint64_t iss_rd64(const uint8_t *p)
{
    uint64_t a = iss_rd32(p), b = iss_rd32(p + 4);
    return iss_elf_le ? (a | (b << 32)) : ((a << 32) | b);
}

// Parse STT_FUNC/STT_NOTYPE/STT_GNU_IFUNC symbols into out (sorted by low,idx).
static void iss_load_symbols(const char *binary, std::vector<iss_sym_entry> &out)
{
    FILE *f = fopen(binary, "rb");
    if (!f)
        return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 64) { fclose(f); return; }
    std::vector<uint8_t> buf(sz);
    if (fread(buf.data(), 1, sz, f) != (size_t)sz) { fclose(f); return; }
    fclose(f);

    const uint8_t *b = buf.data();
    if (memcmp(b, "\177ELF", 4) != 0)
        return;
    bool is64 = b[4] == 2;       // EI_CLASS: 1=32, 2=64
    iss_elf_le = b[5] != 2;      // EI_DATA:  1=LE, 2=BE

    uint64_t shoff   = is64 ? iss_rd64(b + 0x28) : iss_rd32(b + 0x20);
    uint16_t shentsz = is64 ? iss_rd16(b + 0x3a) : iss_rd16(b + 0x2e);
    uint16_t shnum   = is64 ? iss_rd16(b + 0x3c) : iss_rd16(b + 0x30);
    if (shoff == 0 || shoff + (uint64_t)shentsz * shnum > (uint64_t)sz)
        return;

    // Find SHT_SYMTAB (type 2); its sh_link names the associated string table.
    const uint32_t SHT_SYMTAB = 2;
    uint64_t sym_off = 0, sym_size = 0, sym_entsz = 0, str_off = 0, str_size = 0;
    for (uint16_t i = 0; i < shnum; i++)
    {
        const uint8_t *sh = b + shoff + (uint64_t)i * shentsz;
        if (iss_rd32(sh + 4) != SHT_SYMTAB)
            continue;
        uint32_t sh_link = is64 ? iss_rd32(sh + 0x28) : iss_rd32(sh + 0x18);
        sym_off   = is64 ? iss_rd64(sh + 0x18) : iss_rd32(sh + 0x10);
        sym_size  = is64 ? iss_rd64(sh + 0x20) : iss_rd32(sh + 0x14);
        sym_entsz = is64 ? iss_rd64(sh + 0x38) : iss_rd32(sh + 0x24);
        const uint8_t *strsh = b + shoff + (uint64_t)sh_link * shentsz;
        str_off  = is64 ? iss_rd64(strsh + 0x18) : iss_rd32(strsh + 0x10);
        str_size = is64 ? iss_rd64(strsh + 0x20) : iss_rd32(strsh + 0x14);
        break;
    }
    if (sym_off == 0 || sym_entsz == 0)
        return;
    if (sym_off + sym_size > (uint64_t)sz || str_off + str_size > (uint64_t)sz)
        return;

    // FUNC, GNU_IFUNC and NOTYPE name code addresses (libdwfl does the same);
    // OBJECT/SECTION/FILE are skipped.
    const uint32_t STT_NOTYPE = 0, STT_FUNC = 2, STT_GNU_IFUNC = 10;
    uint32_t idx = 0;
    for (uint64_t o = 0; o + sym_entsz <= sym_size; o += sym_entsz, idx++)
    {
        const uint8_t *s = b + sym_off + o;
        uint32_t st_name;
        uint8_t st_info;
        uint64_t st_value, st_sz;
        if (is64)
        {
            st_name = iss_rd32(s + 0); st_info = s[4];
            st_value = iss_rd64(s + 8); st_sz = iss_rd64(s + 16);
        }
        else
        {
            st_name = iss_rd32(s + 0); st_value = iss_rd32(s + 4);
            st_sz = iss_rd32(s + 8);   st_info = s[12];
        }
        uint32_t type = st_info & 0xf;
        if ((type != STT_FUNC && type != STT_NOTYPE && type != STT_GNU_IFUNC)
            || st_value == 0 || st_name >= str_size)
            continue;
        const char *nm = (const char *)(b + str_off + st_name);
        if (!*nm)
            continue;
        out.push_back({ st_value, st_value + st_sz, idx, std::string(nm) });
    }
    // Sort by address; ties broken by symtab index so the lookup reproduces
    // libdwfl's choice among coincident symbols (the later-defined one).
    std::sort(out.begin(), out.end(),
        [](const iss_sym_entry &a, const iss_sym_entry &c) {
            return a.low != c.low ? a.low < c.low : a.idx < c.idx; });
}

// --- DWARF line table (file:line) -------------------------------------------

// Flatten one compilation unit's line-number program into out.
static void iss_dw_collect_lines(Dwarf_Debug dbg, Dwarf_Die cu_die,
    std::vector<iss_line_entry> &out)
{
    Dwarf_Unsigned version = 0;
    Dwarf_Small table_count = 0;
    Dwarf_Line_Context ctx = NULL;
    if (dwarf_srclines_b(cu_die, &version, &table_count, &ctx, NULL) != DW_DLV_OK)
        return;

    Dwarf_Line *lines = NULL;
    Dwarf_Signed count = 0;
    if (dwarf_srclines_from_linecontext(ctx, &lines, &count, NULL) == DW_DLV_OK)
    {
        for (Dwarf_Signed i = 0; i < count; i++)
        {
            Dwarf_Addr addr = 0;
            dwarf_lineaddr(lines[i], &addr, NULL);
            Dwarf_Bool is_end = 0;
            dwarf_lineendsequence(lines[i], &is_end, NULL);
            int lineno = 0;
            std::string file = "-";
            if (!is_end)
            {
                Dwarf_Unsigned ln = 0;
                dwarf_lineno(lines[i], &ln, NULL);
                lineno = (int)ln;
                char *src = NULL;
                if (dwarf_linesrc(lines[i], &src, NULL) == DW_DLV_OK && src)
                {
                    file = src;
                    dwarf_dealloc(dbg, src, DW_DLA_STRING);
                }
            }
            out.push_back({ (uint64_t)addr, lineno, file, (bool)is_end });
        }
    }
    dwarf_srclines_dealloc_b(ctx);
}

// Sort a CU's line table and collapse same-address rows. A single address can
// carry several rows (DWARF "views"); the line-number state machine leaves the
// LAST row's registers active for [addr, next_addr), and libdwfl reports that
// one -- so keep the last non-end row at each address (the stable sort
// preserves program order for the tie-break).
static void iss_dw_finalize(std::vector<iss_line_entry> &lines)
{
    std::stable_sort(lines.begin(), lines.end(),
        [](const iss_line_entry &a, const iss_line_entry &b) { return a.addr < b.addr; });

    std::vector<iss_line_entry> collapsed;
    collapsed.reserve(lines.size());
    for (size_t i = 0; i < lines.size(); )
    {
        size_t j = i, pick = i;
        while (j < lines.size() && lines[j].addr == lines[i].addr)
        {
            if (!lines[j].end)
                pick = j;
            j++;
        }
        collapsed.push_back(lines[pick]);
        i = j;
    }
    lines.swap(collapsed);
}

// Parse one CU's line table on first touch: re-fetch its DIE by offset, flatten
// the line program, finalize. This is the lazy work registration deferred.
static void iss_dw_parse_cu(iss_binary_info &b, iss_cu_info &cu)
{
    if (cu.parsed)
        return;
    cu.parsed = true;

    Dwarf_Die cu_die = NULL;
    if (dwarf_offdie_b(b.dbg, cu.off, /*is_info*/ 1, &cu_die, NULL) != DW_DLV_OK)
        return;

    iss_dw_collect_lines(b.dbg, cu_die, cu.lines);
    dwarf_dealloc(b.dbg, cu_die, DW_DLA_DIE);

    iss_dw_finalize(cu.lines);
}
#endif

void iss_register_debug_elf(Iss *iss, const char *binary)
{
#if !defined(__M32_MODE__)
    if (std::find(binaries.begin(), binaries.end(), std::string(binary)) != binaries.end())
        return;

    binaries.push_back(std::string(binary));

    Dwarf_Debug dbg = NULL;
    Dwarf_Error err = NULL;
    int res = dwarf_init_path(binary, NULL, 0, DW_GROUPNUMBER_ANY, NULL, NULL, &dbg, &err);
    if (res != DW_DLV_OK)
    {
        fprintf(stderr, "Unable to load debug info from binary: %s (%s)\n",
            binary, res == DW_DLV_ERROR ? dwarf_errmsg(err) : "no DWARF info");
        if (res == DW_DLV_ERROR)
            dwarf_dealloc_error(dbg, err);
        return;
    }

    iss_dw_binaries.push_back({});
    iss_binary_info &b = iss_dw_binaries.back();
    b.dbg = dbg;

    // Function names come from the symbol table (parsed once, cheaply).
    iss_load_symbols(binary, b.syms);

    // For file:line, index each CU's address range from its root DIE. Bare-metal
    // executables use native (link-time) addresses, so no bias is applied. A CU
    // with a contiguous DW_AT_low/high_pc is indexed and its line program parsed
    // lazily on first hit (iss_dw_parse_cu). A non-contiguous CU (DW_AT_ranges,
    // e.g. Snitch/LLVM output) cannot be indexed by a single range, so its line
    // program is parsed eagerly here into all_lines -- such CUs are the
    // exception; the common GCC case is all-contiguous and stays fully lazy.
    Dwarf_Unsigned cu_len, abbrev, next_cu, typeoff;
    Dwarf_Half ver, addr_sz, len_sz, ext_sz, cu_type;
    Dwarf_Sig8 sig;
    while (dwarf_next_cu_header_d(dbg, /*is_info*/ 1, &cu_len, &ver, &abbrev,
        &addr_sz, &len_sz, &ext_sz, &sig, &typeoff, &next_cu, &cu_type, NULL) == DW_DLV_OK)
    {
        Dwarf_Die cu_die = NULL;
        if (dwarf_siblingof_b(dbg, NULL, /*is_info*/ 1, &cu_die, NULL) != DW_DLV_OK)
            continue;

        Dwarf_Off off = 0;
        dwarf_dieoffset(cu_die, &off, NULL);

        Dwarf_Addr low = 0, high = 0;
        Dwarf_Half form = 0;
        enum Dwarf_Form_Class cls = DW_FORM_CLASS_UNKNOWN;
        bool contiguous = dwarf_lowpc(cu_die, &low, NULL) == DW_DLV_OK &&
            dwarf_highpc_b(cu_die, &high, &form, &cls, NULL) == DW_DLV_OK;
        if (contiguous)
        {
            // DW_AT_high_pc is an absolute address or, since DWARF4, an offset
            // (constant class) from low_pc.
            uint64_t high_abs = (cls == DW_FORM_CLASS_CONSTANT) ? low + high : high;
            b.aranges.push_back({ (uint64_t)low, high_abs, off });
            b.cus[off].off = off;            // create the (unparsed) CU slot
        }
        else
        {
            iss_dw_collect_lines(dbg, cu_die, b.all_lines);   // eager: scattered CU
        }
        dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
    }

    std::sort(b.aranges.begin(), b.aranges.end(),
        [](const iss_arange &a, const iss_arange &c) { return a.low < c.low; });
    iss_dw_finalize(b.all_lines);
#endif
}

static inline char iss_trace_get_mode(int mode)
{
    switch (mode)
    {
    case 0:
        return 'U';
    case 1:
        return 'S';
    case 2:
        return 'H';
    case 3:
        return 'M';
    }
    return ' ';
}

static inline int iss_trace_dump_reg(Iss *iss, iss_insn_t *insn, iss_decoder_arg_t *arg, char *buff, unsigned int reg, bool is_long = true)
{
    if (is_long)
    {
        if (arg->flags & ISS_DECODER_ARG_FLAG_VREG)
        {
            return sprintf(buff, "v%d", reg);
        }
        else
        {
            if (iss->regfile.is_freg(reg))
            {
                return sprintf(buff, "f%d", iss->regfile.get_reg_lid(reg));
            }
            else
            {
                if (reg == 0)
                {
                    return sprintf(buff, "0");
                }
                else if (reg == 1)
                {
                    return sprintf(buff, "ra");
                }
                else if (reg == 2)
                {
                    return sprintf(buff, "sp");
                }
                else if (reg >= 8 && reg <= 9)
                {
                    return sprintf(buff, "s%d", reg - 8);
                }
                else if (reg >= 18 && reg <= 27)
                {
                    return sprintf(buff, "s%d", reg - 16);
                }
                else if (reg == 4)
                {
                    return sprintf(buff, "tp");
                }
                else if (reg >= 10 && reg <= 17)
                {
                    return sprintf(buff, "a%d", reg - 10);
                }
                else if (reg >= 5 && reg <= 7)
                {
                    return sprintf(buff, "t%d", reg - 5);
                }
                else if (reg >= 28 && reg <= 31)
                {
                    return sprintf(buff, "t%d", reg - 25);
                }
                else if (reg == 3)
                {
                    return sprintf(buff, "gp");
                }
                else if (reg >= ISS_NB_REGS)
                {
                    return sprintf(buff, "f%d", reg - ISS_NB_REGS);
                }
            }
        }
    }

    return sprintf(buff, "x%d", reg);
}

static char *iss_trace_dump_reg_value_check(Iss *iss, char *buff, int size, uint64_t saved_value, uint64_t check_saved_value)
{
    uint8_t *check = (uint8_t *)&check_saved_value;
    uint8_t *value = (uint8_t *)&saved_value;
    for (int i=size*2-1; i>=0; i--)
    {
        uint8_t check = (check_saved_value >> (i*4)) & 0xF;
        uint8_t value = (saved_value >> (i*4)) & 0xF;
        if (check == 0xF)
        {
            buff += sprintf(buff, "%1.1x", value);
        }
        else
        {
            buff += sprintf(buff, "X");
        }
    }
    buff += sprintf(buff, " ");

    return buff;
}

static char *dump_vector(Iss *iss, char *buff, int reg, bool is_float, uint8_t *saved_arg)
{
#ifdef CONFIG_ISS_HAS_VECTOR
    buff += sprintf(buff, "[");

    int width = iss->vector.sewb;
    unsigned int lmul = iss->vector.lmul;

    for (int i=CONFIG_ISS_VLEN/8/width*lmul - 1; i>=0; i--)
    {
        uint8_t *vreg = &((uint8_t *)saved_arg)[i*width];
        uint64_t value = *(uint64_t *)vreg;

        bool float_hex = iss->traces.get_trace_engine()->get_trace_float_hex();

        if (is_float && !float_hex)
        {
            uint64_t value_64 = LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, value,
                iss->vector.exp, iss->vector.mant, 11, 52, 0);
            buff += sprintf(buff, "%f", *(double *)&value_64);
        }
        else
        {
            uint64_t mask;
            if (width >= 8)
            {
                mask = ~0ULL;
            }
            else
            {
                mask = (1ULL << (width * 8)) - 1;
            }

            buff += sprintf(buff, "%0*llx", width*2, value & mask);
        }
        if (i != 0)
        {
            buff += sprintf(buff, ", ");
        }
    }

    buff += sprintf(buff, "] ");
#endif

    return buff;
}

static char *dump_float_vector(Iss *iss, char *buff, int full_width, int width, int exp, int mant, bool is_vec, iss_freg_t value)
{
    if (!is_vec || full_width == width)
    {
        uint64_t value_64;
        if (width == 64)
        {
            value_64 = value;
        }
        else
        {
            value_64 = LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, value, exp, mant, 11, 52, 0);
        }

        buff += sprintf(buff, "%f ", *(double *)&value_64);
    }
    else
    {
        buff += sprintf(buff, "[");

        for (int i=full_width / width - 1; i>=0; i--)
        {
            uint64_t value_64 = LIB_FF_CALL4(lib_flexfloat_cvt_ff_ff_round, value >> (i*width), exp, mant, 11, 52, 0);
            buff += sprintf(buff, "%f", *(double *)&value_64);
            if (i != 0)
            {
                buff += sprintf(buff, ", ");
            }
        }

        buff += sprintf(buff, "] ");
    }

    return buff;
}

static char *iss_trace_dump_reg_value(Iss *iss, iss_insn_t *insn, char *buff, bool is_out, int reg,
    uint64_t saved_value, uint64_t check_saved_value, iss_decoder_arg_t *arg,
    iss_decoder_arg_t **prev_arg, bool is_long, uint8_t *saved_varg)
{
    char regStr[16];
    iss_trace_dump_reg(iss, insn, arg, regStr, reg, is_long);
    if (is_long)
        buff += sprintf(buff, "%3.3s", regStr);
    else
        buff += sprintf(buff, "%s", regStr);

    if (is_out)
        buff += sprintf(buff, "=");
    else
        buff += sprintf(buff, ":");
    if (arg->flags & ISS_DECODER_ARG_FLAG_REG64)
    {
        // if (iss->traces.get_trace_engine()->is_memcheck_enabled() && (iss_reg_t)check_saved_value != (iss_reg_t)-1)
        // {
        //     buff = iss_trace_dump_reg_value_check(iss, buff, sizeof(iss_reg64_t), saved_value, check_saved_value);
        // }
        // else
        {
            buff += sprintf(buff, "%" PRIxFULLREG64 " ", saved_value);
        }
    }
    else if (arg->flags & ISS_DECODER_ARG_FLAG_VREG)
    {
        buff = dump_vector(iss, buff, reg, arg->flags & ISS_DECODER_ARG_FLAG_FREG, saved_varg);
    }
    else if (arg->flags & ISS_DECODER_ARG_FLAG_FREG)
    {
        bool float_hex = iss->traces.get_trace_engine()->get_trace_float_hex();

        if (!float_hex && arg->flags & ISS_DECODER_ARG_FLAG_ELEM_SEW)
        {
            buff = dump_float_vector(iss, buff, CONFIG_GVSOC_ISS_FP_WIDTH, 64, 11, 52, false, saved_value);
        }
        else if (!float_hex && arg->flags & ISS_DECODER_ARG_FLAG_ELEM_64)
        {
            buff = dump_float_vector(iss, buff, CONFIG_GVSOC_ISS_FP_WIDTH, 64, 11, 52, arg->flags & ISS_DECODER_ARG_FLAG_VEC, saved_value);
        }
        else if (!float_hex && arg->flags & ISS_DECODER_ARG_FLAG_ELEM_32)
        {
            buff = dump_float_vector(iss, buff, CONFIG_GVSOC_ISS_FP_WIDTH, 32, 8, 23, arg->flags & ISS_DECODER_ARG_FLAG_VEC, saved_value);
        }
        else if (!float_hex && arg->flags & ISS_DECODER_ARG_FLAG_ELEM_16)
        {
            buff = dump_float_vector(iss, buff, CONFIG_GVSOC_ISS_FP_WIDTH, 16, 5, 10, arg->flags & ISS_DECODER_ARG_FLAG_VEC, saved_value);
        }
        else if (!float_hex && arg->flags & ISS_DECODER_ARG_FLAG_ELEM_16A)
        {
            buff = dump_float_vector(iss, buff, CONFIG_GVSOC_ISS_FP_WIDTH, 16, 8, 7, arg->flags & ISS_DECODER_ARG_FLAG_VEC, saved_value);
        }
        else if (!float_hex && arg->flags & ISS_DECODER_ARG_FLAG_ELEM_8)
        {
            buff = dump_float_vector(iss, buff, CONFIG_GVSOC_ISS_FP_WIDTH, 8, 5, 2, arg->flags & ISS_DECODER_ARG_FLAG_VEC, saved_value);
        }
        else if (!float_hex && arg->flags & ISS_DECODER_ARG_FLAG_ELEM_8A)
        {
            buff = dump_float_vector(iss, buff, CONFIG_GVSOC_ISS_FP_WIDTH, 8, 4, 3, arg->flags & ISS_DECODER_ARG_FLAG_VEC, saved_value);
        }
        else
        {
            if (iss->decode.has_double)
            {
                buff += sprintf(buff, "%16.16lx ", (uint64_t)saved_value);
            }
            else
            {
                buff += sprintf(buff, "%8.8x ", (uint32_t)saved_value);
            }
        }
    }
    else
    {
        // if (iss->traces.get_trace_engine()->is_memcheck_enabled() && (iss_reg_t)check_saved_value != (iss_reg_t)-1)
        // {
        //     buff = iss_trace_dump_reg_value_check(iss, buff, sizeof(iss_reg_t), saved_value, check_saved_value);
        // }
        // else
        {
            buff += sprintf(buff, "%" PRIxFULLREG " ", (iss_reg_t)saved_value);
        }
    }
    return buff;
}

static char *iss_trace_dump_arg_value(Iss *iss, iss_insn_t *insn, char *buff,
    iss_insn_arg_t *insn_arg, iss_decoder_arg_t *arg, iss_insn_arg_t *saved_arg,
    iss_decoder_arg_t **prev_arg, int dump_out, bool is_long, uint8_t *saved_varg)
{
    if ((arg->type == ISS_DECODER_ARG_TYPE_OUT_REG || arg->type == ISS_DECODER_ARG_TYPE_IN_REG) && (insn_arg->u.reg.index != 0 || arg->flags & ISS_DECODER_ARG_FLAG_FREG || arg->flags & ISS_DECODER_ARG_FLAG_VREG))
    {
        if ((dump_out && arg->type == ISS_DECODER_ARG_TYPE_OUT_REG) || (!dump_out && arg->type == ISS_DECODER_ARG_TYPE_IN_REG))
        {
            buff = iss_trace_dump_reg_value(iss, insn, buff, arg->type == ISS_DECODER_ARG_TYPE_OUT_REG, insn_arg->u.reg.index,
                (arg->flags & ISS_DECODER_ARG_FLAG_REG64) || (arg->flags & ISS_DECODER_ARG_FLAG_FREG) ? saved_arg->u.reg.value_64 : saved_arg->u.reg.value,
                (arg->flags & ISS_DECODER_ARG_FLAG_REG64) ? saved_arg->u.reg.memcheck_value_64 : saved_arg->u.reg.memcheck_value,
                arg, prev_arg, is_long, saved_varg);
        }
    }
    else if (arg->type == ISS_DECODER_ARG_TYPE_INDIRECT_IMM)
    {
        if (!dump_out)
            buff = iss_trace_dump_reg_value(iss, insn, buff, 0, insn_arg->u.indirect_imm.reg_index,
                saved_arg->u.indirect_imm.reg_value, saved_arg->u.indirect_imm.memcheck_reg_value, arg, prev_arg, is_long, saved_varg);
        iss_addr_t addr;
        if (arg->flags & ISS_DECODER_ARG_FLAG_POSTINC)
        {
            addr = saved_arg->u.indirect_imm.reg_value;
            if (dump_out)
                buff = iss_trace_dump_reg_value(iss, insn, buff, 1,
                    insn_arg->u.indirect_imm.reg_index, addr + insn_arg->u.indirect_imm.imm, saved_arg->u.indirect_imm.memcheck_reg_value, arg, prev_arg, is_long, saved_varg);
        }
        else
        {
            addr = saved_arg->u.indirect_imm.reg_value + insn_arg->u.indirect_imm.imm;
        }
        if (!dump_out)
            buff += sprintf(buff, " PA:%" PRIxFULLREG " ", addr);
    }
    else if (arg->type == ISS_DECODER_ARG_TYPE_INDIRECT_REG)
    {
        if (!dump_out)
            buff = iss_trace_dump_reg_value(iss, insn, buff, 0, insn_arg->u.indirect_reg.offset_reg_index,
                saved_arg->u.indirect_reg.offset_reg_value, saved_arg->u.indirect_reg.memcheck_offset_reg_value, arg, prev_arg, is_long, saved_varg);
        if (!dump_out)
            buff = iss_trace_dump_reg_value(iss, insn, buff, 0, insn_arg->u.indirect_reg.base_reg_index,
                saved_arg->u.indirect_reg.base_reg_value, saved_arg->u.indirect_reg.memcheck_base_reg_value, arg, prev_arg, is_long, saved_varg);
        iss_addr_t addr;
        if (arg->flags & ISS_DECODER_ARG_FLAG_POSTINC)
        {
            addr = saved_arg->u.indirect_reg.base_reg_value;
            if (dump_out)
                buff = iss_trace_dump_reg_value(iss, insn, buff, 1,
                    insn_arg->u.indirect_reg.base_reg_index, addr + insn_arg->u.indirect_reg.offset_reg_value, saved_arg->u.indirect_reg.memcheck_offset_reg_value, arg, prev_arg, is_long, saved_varg);
        }
        else
        {
            addr = saved_arg->u.indirect_reg.base_reg_value + saved_arg->u.indirect_reg.offset_reg_value;
        }
        if (!dump_out)
            buff += sprintf(buff, " PA:%" PRIxFULLREG " ", addr);
    }
    *prev_arg = arg;
    return buff;
}

static char *iss_trace_dump_arg(Iss *iss, iss_insn_t *insn, char *buff, iss_insn_arg_t *insn_arg, iss_decoder_arg_t *arg, iss_decoder_arg_t **prev_arg, bool is_long)
{
    if (*prev_arg != NULL && (*prev_arg)->type != ISS_DECODER_ARG_TYPE_NONE && (*prev_arg)->type != ISS_DECODER_ARG_TYPE_FLAG && ((arg->type != ISS_DECODER_ARG_TYPE_IN_REG && arg->type != ISS_DECODER_ARG_TYPE_OUT_REG) || arg->u.reg.dump_name))
    {
        if (is_long)
            buff += sprintf(buff, ", ");
        else
            buff += sprintf(buff, ",");
    }

    if (arg->type != ISS_DECODER_ARG_TYPE_NONE)
    {
        if (arg->type == ISS_DECODER_ARG_TYPE_OUT_REG || arg->type == ISS_DECODER_ARG_TYPE_IN_REG)
        {
            if (arg->u.reg.dump_name)
                buff += iss_trace_dump_reg(iss, insn, arg, buff, insn_arg->u.reg.index, is_long);
        }
        else if (arg->type == ISS_DECODER_ARG_TYPE_UIMM)
        {
            if (insn_arg->flags & ISS_DECODER_ARG_FLAG_DUMP_NAME)
            {
                buff += sprintf(buff, "%s", insn_arg->name);
            }
            else
            {
                buff += sprintf(buff, "0x%" PRIxREG, insn_arg->u.uim.value);
            }
        }
        else if (arg->type == ISS_DECODER_ARG_TYPE_SIMM)
        {
            if (insn_arg->flags & ISS_DECODER_ARG_FLAG_DUMP_NAME)
            {
                buff += sprintf(buff, "%s", insn_arg->name);
            }
            else
            {
                buff += sprintf(buff, "%" PRIdREG, insn_arg->u.sim.value);
            }
        }
        else if (arg->type == ISS_DECODER_ARG_TYPE_INDIRECT_IMM)
        {
            buff += sprintf(buff, "%" PRIdREG "(", insn_arg->u.indirect_imm.imm);
            if (arg->flags & ISS_DECODER_ARG_FLAG_PREINC)
                buff += sprintf(buff, "!");
            buff += iss_trace_dump_reg(iss, insn, arg, buff, insn_arg->u.indirect_imm.reg_index, is_long);
            if (arg->flags & ISS_DECODER_ARG_FLAG_POSTINC)
                buff += sprintf(buff, "!");
            buff += sprintf(buff, ")");
        }
        else if (arg->type == ISS_DECODER_ARG_TYPE_INDIRECT_REG)
        {
            buff += iss_trace_dump_reg(iss, insn, arg, buff, insn_arg->u.indirect_reg.offset_reg_index, is_long);
            buff += sprintf(buff, "(");
            if (arg->flags & ISS_DECODER_ARG_FLAG_PREINC)
                buff += sprintf(buff, "!");
            buff += iss_trace_dump_reg(iss, insn, arg, buff, insn_arg->u.indirect_reg.base_reg_index, is_long);
            if (arg->flags & ISS_DECODER_ARG_FLAG_POSTINC)
                buff += sprintf(buff, "!");
            buff += sprintf(buff, ")");
        }
        *prev_arg = arg;
    }
    return buff;
}

static char *trace_dump_debug(Iss *iss, iss_insn_t *insn, iss_reg_t pc, char *buff)
{
    char *name = (char *)"-";
    char *file = (char *)"-";
    uint32_t line = 0;
    char *inline_func = (char *)"-";
    iss_pc_info *pc_info = iss_pc_info_get(pc);
    if (pc_info && pc_info->valid)
    {
        name = pc_info->func;
        file = pc_info->file;
        line = pc_info->line;
        inline_func = pc_info->inline_func;
    }

    int line_len = sprintf(buff, ":%d", line);
    if (line_len > 5)
        line_len = 5;
    int max_name_len = MAX_DEBUG_INFO_WIDTH - line_len;

    int len = snprintf(buff, max_name_len + 1, "%s", inline_func);
    if (len > max_name_len)
        len = max_name_len;

    len += sprintf(buff + len, ":%d", line);

    char *start_buff = buff;

    if (len > MAX_DEBUG_INFO_WIDTH)
        len = MAX_DEBUG_INFO_WIDTH;

    for (int i = len; i < MAX_DEBUG_INFO_WIDTH + 1; i++)
    {
        sprintf(buff + i, " ");
    }

    return buff + MAX_DEBUG_INFO_WIDTH + 1;
}

static void iss_trace_dump_insn(Iss *iss, iss_insn_t *insn, iss_reg_t pc, char *buff,
    int buffer_size, iss_insn_arg_t *saved_args, bool is_long, int mode, bool is_event, TraceEntry *entry)
{
    char *init_buff = buff;
    static int max_len = 20;
    static int max_arg_len = 17;
    int len;

    if (is_long)
    {
        if (binaries.size())
            buff = trace_dump_debug(iss, insn, pc, buff);
    }

    if (iss->trace.has_reg_dump)
    {
        buff += sprintf(buff, "%" PRIxFULLREG " ", iss->trace.reg_dump);
    }

    if (iss->trace.has_str_dump)
    {
        buff += sprintf(buff, "%s ", iss->trace.str_dump.c_str());
    }

    if (!is_event)
    {
        buff += sprintf(buff, "%c %" PRIxFULLREG " ", iss_trace_get_mode(mode), pc);
    }

    if (!is_long)
    {
        buff += sprintf(buff, "%" PRIxFULLREG " ", insn->opcode);
    }

    char *start_buff = buff;

    buff += sprintf(buff, "%s ", insn->decoder_item->u.insn.label);

    if (is_long)
    {
        len = buff - start_buff;

        if (len > max_len)
            max_len = len;
        else
        {
            memset(buff, ' ', max_len - len);
            buff += max_len - len;
        }
    }

    iss_decoder_arg_t *prev_arg = NULL;
    start_buff = buff;
    int nb_args = insn->decoder_item->u.insn.nb_args;
    for (int i = 0; i < nb_args; i++)
    {
        int arg_id = insn->decoder_item->u.insn.args_order[i];
        buff = iss_trace_dump_arg(iss, insn, buff, &insn->args[arg_id], &insn->decoder_item->u.insn.args[arg_id], &prev_arg, is_long);
    }
    if (nb_args != 0)
        buff += sprintf(buff, " ");

    if (!is_event)
    {
        len = buff - start_buff;

        if (len > max_arg_len)
            max_arg_len = len;
        else
        {
            memset(buff, ' ', max_arg_len - len);
            buff += max_arg_len - len;
        }
    }

    if (!is_event)
    {
        prev_arg = NULL;
        for (int i = 0; i < nb_args; i++)
        {
            int arg_id = insn->decoder_item->u.insn.args_order[i];
#ifdef CONFIG_ISS_HAS_VECTOR
            uint8_t *saved_vargs = entry->saved_vargs[arg_id];
#else
            uint8_t *saved_vargs = NULL;
#endif
            buff = iss_trace_dump_arg_value(iss, insn, buff, &insn->args[arg_id], &insn->decoder_item->u.insn.args[arg_id], &saved_args[arg_id], &prev_arg, 1, is_long, saved_vargs);
        }
        for (int i = 0; i < nb_args; i++)
        {
            int arg_id = insn->decoder_item->u.insn.args_order[i];
#ifdef CONFIG_ISS_HAS_VECTOR
            uint8_t *saved_vargs = entry->saved_vargs[arg_id];
#else
            uint8_t *saved_vargs = NULL;
#endif
            buff = iss_trace_dump_arg_value(iss, insn, buff, &insn->args[arg_id], &insn->decoder_item->u.insn.args[arg_id], &saved_args[arg_id], &prev_arg, 0, is_long, saved_vargs);
        }

        buff += sprintf(buff, "\n");
    }
}

static void iss_trace_save_varg(Iss *iss, iss_insn_t *insn, iss_insn_arg_t *insn_arg, iss_decoder_arg_t *arg, uint8_t *saved_arg, bool save_out)
{
#ifdef CONFIG_ISS_HAS_VECTOR
    int width = iss->vector.sewb;
    unsigned int lmul = iss->vector.lmul;

    if (save_out && arg->type == ISS_DECODER_ARG_TYPE_OUT_REG ||
            !save_out && arg->type == ISS_DECODER_ARG_TYPE_IN_REG)
    {
        memcpy(saved_arg, iss->vector.vregs[insn_arg->u.reg.index], CONFIG_ISS_VLEN/8*lmul);
    }
#endif
}

static void iss_trace_save_arg(Iss *iss, iss_insn_t *insn, iss_insn_arg_t *insn_arg, iss_decoder_arg_t *arg, iss_insn_arg_t *saved_arg, bool save_out)
{
    if (arg->type == ISS_DECODER_ARG_TYPE_OUT_REG || arg->type == ISS_DECODER_ARG_TYPE_IN_REG)
    {
        if (save_out && arg->type == ISS_DECODER_ARG_TYPE_OUT_REG ||
            !save_out && arg->type == ISS_DECODER_ARG_TYPE_IN_REG)
        {
            if (arg->flags & ISS_DECODER_ARG_FLAG_REG64)
            {
                saved_arg->u.reg.value_64 = iss->regfile.get_reg_pair_untimed(insn_arg->u.reg.index);
                // saved_arg->u.reg.memcheck_value_64 = iss->regfile.get_memcheck_reg64(insn_arg->u.reg.index);
            }
            else if (arg->flags & ISS_DECODER_ARG_FLAG_FREG)
            {
                saved_arg->u.reg.value_64 = iss->regfile.get_freg_untimed(insn_arg->u.reg.index);
            }
            else
            {
                saved_arg->u.reg.value = iss->regfile.get_reg_untimed(insn_arg->u.reg.index);
                // saved_arg->u.reg.memcheck_value = iss->regfile.regs_memcheck[insn_arg->u.reg.index];
            }
        }
    }
    else if (arg->type == ISS_DECODER_ARG_TYPE_INDIRECT_IMM)
    {
        if (save_out)
            return;
        saved_arg->u.indirect_imm.reg_value = iss->regfile.get_reg_untimed(insn_arg->u.indirect_imm.reg_index);
        // saved_arg->u.indirect_imm.memcheck_reg_value = iss->regfile.regs_memcheck[insn_arg->u.indirect_imm.reg_index];
    }
    // else if (arg->type == TRACE_TYPE_FLAG)
    //   {
    //     *savedValue = cpu->flag;
    //   }
    else if (arg->type == ISS_DECODER_ARG_TYPE_INDIRECT_REG)
    {
        if (save_out)
            return;
        saved_arg->u.indirect_reg.base_reg_value = iss->regfile.get_reg_untimed(insn_arg->u.indirect_reg.base_reg_index);
        // saved_arg->u.indirect_reg.memcheck_base_reg_value = iss->regfile.regs_memcheck[insn_arg->u.indirect_reg.base_reg_index];
        saved_arg->u.indirect_reg.offset_reg_value = iss->regfile.get_reg_untimed(insn_arg->u.indirect_reg.offset_reg_index);
        // saved_arg->u.indirect_reg.memcheck_offset_reg_value = iss->regfile.regs_memcheck[insn_arg->u.indirect_reg.offset_reg_index];
    }
    // else
    //   {
    //     *savedValue = arg->val;
    //   }
}

void iss_trace_save_args(Iss *iss, iss_insn_t *insn, bool save_out, TraceEntry *entry)
{
    if (insn->decoder_item)
    {
        for (int i = 0; i < insn->decoder_item->u.insn.nb_args; i++)
        {
            iss_decoder_arg_t *arg = &insn->decoder_item->u.insn.args[i];
            if (arg->flags & ISS_DECODER_ARG_FLAG_VREG)
            {
                // Only dump vector registers if they are not dumped already by the pipeline
#ifndef CONFIG_GVSIC_ISS_V2
    #ifdef CONFIG_ISS_HAS_VECTOR
                iss_trace_save_varg(iss, insn, &insn->args[i], arg, entry->saved_vargs[i], save_out);
    #endif
#endif
            }
            else
            {
                iss_trace_save_arg(iss, insn, &insn->args[i], arg, &entry->saved_args[i], save_out);
            }
        }
    }
}

void iss_trace_dump(Iss *iss, iss_insn_t *insn, iss_reg_t pc, TraceEntry *entry)
{
    if (!insn->is_macro_op || iss->traces.get_trace_engine()->get_format() == TRACE_FORMAT_LONG)
    {
        char buffer[32*1024];

        iss_trace_save_args(iss, insn, true, entry);

        iss_trace_dump_insn(iss, insn, pc, buffer, 1024, entry->saved_args,
            iss->traces.get_trace_engine()->get_format() == TRACE_FORMAT_LONG, iss->trace.priv_mode, 0,
            entry);

        iss->trace.insn_trace.msg(buffer);
    }
}

void Trace::event_dump(iss_insn_t *insn, iss_reg_t pc)
{
    char buffer[1024];

    iss_trace_dump_insn(&this->iss, insn, pc, buffer, 1024, NULL, false, this->priv_mode, 1, NULL);

    char *current = buffer;
    while (*current)
    {
        if (*current == ' ')
            *current = '_';

        current++;
    }

    this->insn_trace_event.event_string(buffer, true);
}

void Trace::insn_trace_start(iss_insn_t *insn, iss_reg_t pc)
{
    iss_reg_t next_insn;


    if (this->insn_trace_event.get_event_active())
    {
        this->event_dump(insn, pc);
    }

    iss.trace.priv_mode = iss.core.mode_get();

    iss_trace_save_args(&this->iss, insn, false, this->get_entry());
}

void Trace::insn_trace_dump(iss_insn_t *insn, iss_reg_t pc, TraceEntry *entry)
{
    iss_trace_dump(&this->iss, insn, pc, entry ? entry : this->get_entry());
    if (entry)
    {
        this->release_entry(entry);
    }
}

void iss_trace_init(Iss *iss)
{
    if (!pc_infos_is_init)
    {
        pc_infos_is_init = true;
        memset(pc_infos, 0, sizeof(pc_infos));
    }
}

void Trace::dump_debug_traces()
{
    const char *func, *inline_func, *file;
    int line;

    if (!iss_trace_pc_info(this->iss.exec.current_insn, &func, &inline_func, &file, &line))
    {
        this->func_trace_event.event_string(func, false);
        this->inline_trace_event.event_string(inline_func, false);
        this->file_trace_event.event_string(file, false);
        this->line_trace_event.event((uint8_t *)&line);
    }
}
