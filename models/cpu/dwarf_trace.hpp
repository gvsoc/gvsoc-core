/*
 * Copyright (C) 2024 GreenWaves Technologies, SAS, ETH Zurich and
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

// Self-contained ELF + DWARF .debug_line reader for resolving an instruction
// address to its function name and source file:line. No external dependency
// (no libdwarf, no libdw/libelf, no <elf.h>), so it builds identically on
// Linux, macOS and anywhere else.
//
//   * function name  <- the ELF symbol table.
//   * file and line  <- the DWARF .debug_line line-number program (DWARF 2-5).
//
// Both ELF32/ELF64 and both byte orders are handled. The whole binary is parsed
// once (eagerly) into two address-sorted tables; resolution is then a pair of
// binary searches. Output matches what the former libdwfl path produced
// (validated to byte parity on real RISC-V/PULP binaries).

#ifndef __CPU_DWARF_TRACE_HPP__
#define __CPU_DWARF_TRACE_HPP__

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

namespace dwarf_trace
{

// A function symbol (high == low means size 0, e.g. an assembly label).
struct SymEntry { uint64_t low, high; uint32_t idx; std::string name; };
// One line-table row (end == true marks the end_sequence boundary).
struct LineRow { uint64_t addr; int line; std::string file; bool end; };

// ---------------------------------------------------------------------------
// ELF + .debug_line parser. All state is local to an instance, so this header
// can be included into several translation units without ODR/global issues.
// ---------------------------------------------------------------------------
class Parser
{
public:
    // Read the binary and fill the (unsorted) symbol and line-row tables.
    bool parse(const char *path, std::vector<SymEntry> &syms,
        std::vector<LineRow> &lines)
    {
        FILE *f = fopen(path, "rb");
        if (!f)
            return false;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz < 64) { fclose(f); return false; }
        this->buf.resize(sz);
        if (fread(this->buf.data(), 1, sz, f) != (size_t)sz) { fclose(f); return false; }
        fclose(f);

        if (!this->parse_sections())
            return false;
        this->parse_symbols(syms);
        this->parse_lines(lines);
        return true;
    }

private:
    std::vector<uint8_t> buf;
    bool le = true;
    bool is64 = true;
    // .debug_line and the string sections its DWARF5 file tables may reference.
    const uint8_t *line = nullptr, *line_str = nullptr, *dstr = nullptr;
    uint64_t line_sz = 0, line_str_sz = 0, dstr_sz = 0;
    // Symbol table + its string table.
    uint64_t sym_off = 0, sym_sz = 0, sym_entsz = 0, symstr_off = 0, symstr_sz = 0;

    uint16_t r16(const uint8_t *p) const
    { return le ? (uint16_t)(p[0] | p[1] << 8) : (uint16_t)(p[1] | p[0] << 8); }
    uint32_t r32(const uint8_t *p) const
    {
        return le ? ((uint32_t)p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24)
                  : ((uint32_t)p[3] | p[2] << 8 | p[1] << 16 | (uint32_t)p[0] << 24);
    }
    uint64_t r64(const uint8_t *p) const
    { uint64_t a = r32(p), b = r32(p + 4); return le ? (a | (b << 32)) : ((a << 32) | b); }

    // Locate the symbol table and the debug sections by name.
    bool parse_sections()
    {
        const uint8_t *b = this->buf.data();
        if (memcmp(b, "\177ELF", 4) != 0)
            return false;
        this->is64 = b[4] == 2;
        this->le = b[5] != 2;
        uint64_t sz = this->buf.size();

        uint64_t shoff   = is64 ? r64(b + 0x28) : r32(b + 0x20);
        uint16_t shentsz = is64 ? r16(b + 0x3a) : r16(b + 0x2e);
        uint16_t shnum   = is64 ? r16(b + 0x3c) : r16(b + 0x30);
        uint16_t shstrndx = is64 ? r16(b + 0x3e) : r16(b + 0x32);
        if (shoff == 0 || shoff + (uint64_t)shentsz * shnum > sz)
            return false;

        const uint8_t *shstr = b + shoff + (uint64_t)shstrndx * shentsz;
        uint64_t shstr_off = is64 ? r64(shstr + 0x18) : r32(shstr + 0x10);

        const uint32_t SHT_SYMTAB = 2;
        for (uint16_t i = 0; i < shnum; i++)
        {
            const uint8_t *sh = b + shoff + (uint64_t)i * shentsz;
            uint32_t type = r32(sh + 4);
            uint64_t off  = is64 ? r64(sh + 0x18) : r32(sh + 0x10);
            uint64_t size = is64 ? r64(sh + 0x20) : r32(sh + 0x14);
            const char *nm = (const char *)(b + shstr_off + r32(sh));
            if (off + size > sz)
                continue;

            if (type == SHT_SYMTAB)
            {
                uint32_t sh_link = is64 ? r32(sh + 0x28) : r32(sh + 0x18);
                sym_off = off; sym_sz = size;
                sym_entsz = is64 ? r64(sh + 0x38) : r32(sh + 0x24);
                const uint8_t *strsh = b + shoff + (uint64_t)sh_link * shentsz;
                symstr_off = is64 ? r64(strsh + 0x18) : r32(strsh + 0x10);
                symstr_sz  = is64 ? r64(strsh + 0x20) : r32(strsh + 0x14);
            }
            else if (!strcmp(nm, ".debug_line"))     { line = b + off; line_sz = size; }
            else if (!strcmp(nm, ".debug_line_str")) { line_str = b + off; line_str_sz = size; }
            else if (!strcmp(nm, ".debug_str"))      { dstr = b + off; dstr_sz = size; }
        }
        return true;
    }

    // FUNC, GNU_IFUNC and NOTYPE symbols name code addresses (as libdwfl did).
    void parse_symbols(std::vector<SymEntry> &out)
    {
        if (sym_off == 0 || sym_entsz == 0)
            return;
        const uint8_t *b = this->buf.data();
        const uint32_t STT_NOTYPE = 0, STT_FUNC = 2, STT_GNU_IFUNC = 10;
        uint32_t idx = 0;
        for (uint64_t o = 0; o + sym_entsz <= sym_sz; o += sym_entsz, idx++)
        {
            const uint8_t *s = b + sym_off + o;
            uint32_t st_name; uint8_t st_info; uint64_t st_value, st_size;
            if (is64) { st_name = r32(s); st_info = s[4]; st_value = r64(s + 8); st_size = r64(s + 16); }
            else      { st_name = r32(s); st_value = r32(s + 4); st_size = r32(s + 8); st_info = s[12]; }
            uint32_t type = st_info & 0xf;
            if ((type != STT_FUNC && type != STT_NOTYPE && type != STT_GNU_IFUNC)
                || st_value == 0 || st_name >= symstr_sz)
                continue;
            const char *nm = (const char *)(b + symstr_off + st_name);
            if (!*nm)
                continue;
            out.push_back({ st_value, st_value + st_size, idx, std::string(nm) });
        }
    }

    // ---- DWARF .debug_line ----
    // A little cursor with LEB128 over a [p, end) byte range.
    struct Cur
    {
        const Parser *par; const uint8_t *p; const uint8_t *end;
        uint8_t  u8_()  { return *p++; }
        uint16_t u16_() { uint16_t v = par->r16(p); p += 2; return v; }
        uint32_t u32_() { uint32_t v = par->r32(p); p += 4; return v; }
        uint64_t u64_() { uint64_t v = par->r64(p); p += 8; return v; }
        uint64_t uleb() { uint64_t r = 0; int s = 0; uint8_t b; do { b = *p++; r |= (uint64_t)(b & 0x7f) << s; s += 7; } while (b & 0x80); return r; }
        int64_t sleb()  { int64_t r = 0; int s = 0; uint8_t b; do { b = *p++; r |= (int64_t)(b & 0x7f) << s; s += 7; } while (b & 0x80); if (s < 64 && (b & 0x40)) r |= -(int64_t)1 << s; return r; }
        const char *cstr() { const char *s = (const char *)p; p += strlen(s) + 1; return s; }
        uint64_t off(bool d64) { return d64 ? u64_() : u32_(); }
    };

    // Read a DWARF5 dir/file-table form value; capture path strings and dir indices.
    void read_form(Cur &c, uint64_t form, bool d64, std::string *str, uint64_t *num)
    {
        enum { STRING = 0x08, STRP = 0x0e, LINE_STRP = 0x1f, STRX = 0x1a,
               STRX1 = 0x25, STRX2 = 0x26, STRX4 = 0x28, DATA1 = 0x0b, DATA2 = 0x05,
               DATA4 = 0x06, DATA8 = 0x07, DATA16 = 0x1e, UDATA = 0x0f, BLOCK = 0x09 };
        switch (form)
        {
            case STRING:    { const char *s = c.cstr(); if (str) *str = s; break; }
            case LINE_STRP: { uint64_t o = c.off(d64); if (str && line_str && o < line_str_sz) *str = (const char *)(line_str + o); break; }
            case STRP:      { uint64_t o = c.off(d64); if (str && dstr && o < dstr_sz) *str = (const char *)(dstr + o); break; }
            case UDATA:     { uint64_t v = c.uleb(); if (num) *num = v; break; }
            case DATA1:     { uint64_t v = c.u8_();  if (num) *num = v; break; }
            case DATA2:     { uint64_t v = c.u16_(); if (num) *num = v; break; }
            case DATA4:     { uint64_t v = c.u32_(); if (num) *num = v; break; }
            case DATA8:     { uint64_t v = c.u64_(); if (num) *num = v; break; }
            case DATA16:    { c.p += 16; break; }
            case STRX:      { uint64_t v = c.uleb(); if (num) *num = v; break; }
            case STRX1:     { uint64_t v = c.u8_();  if (num) *num = v; break; }
            case STRX2:     { uint64_t v = c.u16_(); if (num) *num = v; break; }
            case STRX4:     { uint64_t v = c.u32_(); if (num) *num = v; break; }
            case BLOCK:     { uint64_t n = c.uleb(); c.p += n; break; }
            default: break;
        }
    }

    static std::string join(const std::string &dir, const std::string &name)
    {
        if (name.empty() || name[0] == '/') return name;
        if (dir.empty()) return name;
        return dir + "/" + name;
    }

    void parse_lines(std::vector<LineRow> &out)
    {
        if (!line)
            return;
        const uint8_t *p = line, *end = line + line_sz;
        while (p < end)
            this->parse_unit(&p, end, out);
    }

    void parse_unit(const uint8_t **pp, const uint8_t *secend, std::vector<LineRow> &out)
    {
        Cur c{ this, *pp, secend };
        if (c.p + 4 > secend) { *pp = secend; return; }
        uint64_t ulen = c.u32_();
        bool d64 = false;
        if (ulen == 0xffffffff) { if (c.p + 8 > secend) { *pp = secend; return; } ulen = c.u64_(); d64 = true; }
        const uint8_t *unit_end = c.p + ulen;
        if (ulen == 0 || unit_end > secend || unit_end < c.p) unit_end = secend;
        // Guarantee forward progress: a zero/garbage unit_length must never leave
        // *pp where it started, or parse_lines() would spin forever.
        if (unit_end <= *pp) { *pp = secend; return; }
        if (c.p + 2 > unit_end) { *pp = unit_end; return; }
        uint16_t ver = c.u16_();
        // Only DWARF 2..5 line programs are understood. Anything else means a
        // corrupt or alien .debug_line (e.g. a concatenated/relinked binary);
        // stop rather than misread it into a runaway allocation. This matches
        // libdwfl, which reported no line info instead of crashing.
        if (ver < 2 || ver > 5) { *pp = secend; return; }
        if (ver >= 5) { c.u8_(); c.u8_(); }            // address_size, segment_selector_size
        uint64_t hlen = c.off(d64);
        const uint8_t *prog = c.p + hlen;
        uint8_t min_inst = c.u8_();
        if (ver >= 4) c.u8_();                          // maximum_operations_per_instruction (assume 1)
        c.u8_();                                        // default_is_stmt
        int8_t line_base = (int8_t)c.u8_();
        uint8_t line_range = c.u8_();
        uint8_t opcode_base = c.u8_();
        uint8_t std_len[256] = { 0 };
        for (int i = 1; i < opcode_base && i < 256; i++) std_len[i] = c.u8_();

        std::vector<std::string> dirs, files;

        if (ver <= 4)
        {
            dirs.push_back("");                          // [0] = comp_dir (implicit, unknown here)
            while (c.p < unit_end && *c.p) dirs.push_back(c.cstr());
            if (c.p < unit_end) c.p++;
            files.push_back("");                         // [0] reserved
            while (c.p < unit_end && *c.p)
            {
                const char *nm = c.cstr();
                uint64_t di = c.uleb(); c.uleb(); c.uleb();
                std::string dir = (di < dirs.size()) ? dirs[di] : std::string();
                files.push_back(join(dir, nm));
            }
            if (c.p < unit_end) c.p++;
        }
        else
        {
            uint8_t dfc = c.u8_();
            std::vector<std::pair<uint64_t, uint64_t>> dfmt(dfc);
            for (int i = 0; i < dfc; i++) { dfmt[i].first = c.uleb(); dfmt[i].second = c.uleb(); }
            uint64_t dcount = c.uleb();
            for (uint64_t i = 0; i < dcount && c.p < unit_end; i++)
            {
                std::string path;
                for (auto &f : dfmt) { std::string s; uint64_t n = 0; read_form(c, f.second, d64, &s, &n); if (f.first == 1) path = s; }
                dirs.push_back(path);
            }
            uint8_t ffc = c.u8_();
            std::vector<std::pair<uint64_t, uint64_t>> ffmt(ffc);
            for (int i = 0; i < ffc; i++) { ffmt[i].first = c.uleb(); ffmt[i].second = c.uleb(); }
            uint64_t fcount = c.uleb();
            for (uint64_t i = 0; i < fcount && c.p < unit_end; i++)
            {
                std::string name; uint64_t di = 0;
                for (auto &f : ffmt)
                {
                    std::string s; uint64_t n = 0; read_form(c, f.second, d64, &s, &n);
                    if (f.first == 1) name = s; else if (f.first == 2) di = n;
                }
                std::string dir = (di < dirs.size()) ? dirs[di] : std::string();
                files.push_back(join(dir, name));
            }
        }

        // Run the line-number program.
        c.p = prog;
        uint64_t address = 0; int file = 1; int lineno = 1;
        auto emit = [&](bool endseq) {
            std::string fp = (file >= 0 && (size_t)file < files.size()) ? files[file] : std::string();
            if (fp.empty()) fp = "-";
            out.push_back({ address, lineno, endseq ? std::string("-") : fp, endseq });
        };
        auto reset = [&]() { address = 0; file = 1; lineno = 1; };
        while (c.p < unit_end)
        {
            uint8_t op = c.u8_();
            if (op == 0)                                 // extended opcode
            {
                uint64_t len = c.uleb(); const uint8_t *es = c.p; uint8_t sub = c.u8_();
                if (sub == 1) { emit(true); reset(); }   // DW_LNE_end_sequence
                else if (sub == 2)                       // DW_LNE_set_address (len-1 address bytes)
                {
                    uint64_t n = len - 1, a = 0;
                    if (le) { for (uint64_t i = 0; i < n; i++) a |= (uint64_t)c.p[i] << (8 * i); }
                    else    { for (uint64_t i = 0; i < n; i++) a = (a << 8) | c.p[i]; }
                    address = a;
                }
                c.p = es + len;
            }
            else if (op < opcode_base)                   // standard opcode
            {
                switch (op)
                {
                    case 1: emit(false); break;                                   // copy
                    case 2: address += min_inst * c.uleb(); break;                // advance_pc
                    case 3: lineno += (int)c.sleb(); break;                       // advance_line
                    case 4: file = (int)c.uleb(); break;                          // set_file
                    case 5: c.uleb(); break;                                      // set_column
                    case 6: break;                                                // negate_stmt
                    case 7: break;                                                // set_basic_block
                    case 8: address += min_inst * ((255 - opcode_base) / line_range); break; // const_add_pc
                    case 9: address += c.u16_(); break;                           // fixed_advance_pc
                    case 10: case 11: break;                                      // prologue_end / epilogue_begin
                    case 12: c.uleb(); break;                                     // set_isa
                    default: for (int i = 0; i < std_len[op]; i++) c.uleb(); break;
                }
            }
            else                                         // special opcode
            {
                int adj = op - opcode_base;
                address += min_inst * (adj / line_range);
                lineno += line_base + (adj % line_range);
                emit(false);
            }
        }
        *pp = unit_end;
    }
};

// Sort both tables and collapse same-address line rows. Multiple rows can share
// an address (DWARF "views"); the state machine leaves the LAST row's registers
// active for [addr, next), which is what libdwfl reported -- so keep the last
// non-end row at each address (the stable sort preserves program order).
inline void finalize(std::vector<SymEntry> &syms, std::vector<LineRow> &lines)
{
    std::sort(syms.begin(), syms.end(), [](const SymEntry &a, const SymEntry &b) {
        return a.low != b.low ? a.low < b.low : a.idx < b.idx; });

    std::stable_sort(lines.begin(), lines.end(),
        [](const LineRow &a, const LineRow &b) { return a.addr < b.addr; });
    std::vector<LineRow> collapsed;
    collapsed.reserve(lines.size());
    for (size_t i = 0; i < lines.size(); )
    {
        size_t j = i, pick = i;
        while (j < lines.size() && lines[j].addr == lines[i].addr)
        {
            if (!lines[j].end) pick = j;
            j++;
        }
        collapsed.push_back(lines[pick]);
        i = j;
    }
    lines.swap(collapsed);
}

// Parse a binary into sorted symbol and line tables. Returns false if it cannot
// be read as ELF.
inline bool load(const char *path, std::vector<SymEntry> &syms, std::vector<LineRow> &lines)
{
    Parser p;
    if (!p.parse(path, syms, lines))
        return false;
    finalize(syms, lines);
    return true;
}

// Function name for addr: the nearest *sized* symbol that contains addr,
// otherwise the nearest preceding symbol (covers zero-sized assembly labels).
// Matches libdwfl's dwfl_module_addrname selection.
inline const char *func_for(const std::vector<SymEntry> &syms, uint64_t addr)
{
    auto it = std::upper_bound(syms.begin(), syms.end(), addr,
        [](uint64_t a, const SymEntry &e) { return a < e.low; });
    if (it == syms.begin())
        return nullptr;
    // Walk back from the nearest preceding symbol. A sized symbol that contains
    // addr wins (this also lets an enclosing function beat an inner zero-sized
    // label). Hitting a sized symbol that ends before addr means addr is in a
    // gap -> no function. A zero-sized symbol owns everything up to the next
    // symbol, so it is the fallback.
    const SymEntry *zero_fallback = nullptr;
    for (auto j = it; j != syms.begin(); )
    {
        --j;
        if (j->high > j->low)                       // sized symbol
        {
            if (addr < j->high) return j->name.c_str();
            break;                                  // gap past a sized symbol
        }
        if (!zero_fallback) zero_fallback = &*j;     // zero-sized: owns forward
    }
    return zero_fallback ? zero_fallback->name.c_str() : nullptr;
}

// file:line for addr: the last non-end row with addr <= pc.
inline bool line_for(const std::vector<LineRow> &lines, uint64_t addr,
    const char **file, int *line)
{
    auto it = std::upper_bound(lines.begin(), lines.end(), addr,
        [](uint64_t a, const LineRow &e) { return a < e.addr; });
    if (it == lines.begin())
        return false;
    --it;
    if (it->end)
        return false;
    *file = it->file.c_str();
    *line = it->line;
    return true;
}

} // namespace dwarf_trace

#endif
