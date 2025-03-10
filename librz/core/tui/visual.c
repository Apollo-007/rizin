// SPDX-FileCopyrightText: 2009-2020 pancake <pancake@nopcode.org>
// SPDX-License-Identifier: LGPL-3.0-only

#include <rz_core.h>
#include <rz_cons.h>
#include <rz_windows.h>
#include "../core_private.h"
#include "modes.h"

static void visual_refresh(RzCore *core);

#define KEY_ALTQ 0xc5

RZ_IPI RZ_OWN RzCoreVisual *rz_core_visual_new() {
	RzCoreVisual *visual = RZ_NEW0(RzCoreVisual);
	if (!visual) {
		return NULL;
	}
	visual->autoblocksize = true;
	visual->color = 1;
	visual->debug = 1;
	visual->splitPtr = UT64_MAX;
	visual->insertNibble = -1;
	return visual;
}

RZ_IPI void rz_core_visual_free(RZ_NULLABLE RzCoreVisual *visual) {
	if (!visual) {
		return;
	}
	RZ_FREE_CUSTOM(visual->tabs, rz_list_free);
	free(visual->inputing);
	free(visual);
}

RZ_IPI void rz_core_visual_applyHexMode(RzCore *core, int hexMode) {
	RzCoreVisual *visual = core->visual;
	visual->currentFormat = RZ_ABS(hexMode) % PRINT_HEX_FORMATS;
	switch (visual->currentFormat) {
	case 0: /* px */
	case 3: /* prx */
	case 6: /* pxw */
	case 9: /* pxr */
		rz_config_set(core->config, "hex.compact", "false");
		rz_config_set(core->config, "hex.comments", "true");
		break;
	case 1: /* pxa */
	case 4: /* pxb */
	case 7: /* pxq */
		rz_config_set(core->config, "hex.compact", "true");
		rz_config_set(core->config, "hex.comments", "true");
		break;
	case 2: /* pxr */
	case 5: /* pxh */
	case 8: /* pxd */
		rz_config_set(core->config, "hex.compact", "false");
		rz_config_set(core->config, "hex.comments", "false");
		break;
	}
}

RZ_IPI void rz_core_visual_toggle_hints(RzCore *core) {
	if (rz_config_get_b(core->config, "asm.hint.call")) {
		rz_config_toggle(core->config, "asm.hint.call");
		rz_config_set_b(core->config, "asm.hint.jmp", true);
	} else if (rz_config_get_b(core->config, "asm.hint.jmp")) {
		rz_config_toggle(core->config, "asm.hint.jmp");
		rz_config_set_b(core->config, "asm.hint.emu", true);
	} else if (rz_config_get_b(core->config, "asm.hint.emu")) {
		rz_config_toggle(core->config, "asm.hint.emu");
		rz_config_set_b(core->config, "asm.hint.lea", true);
	} else if (rz_config_get_b(core->config, "asm.hint.lea")) {
		rz_config_toggle(core->config, "asm.hint.lea");
		rz_config_set_b(core->config, "asm.hint.call", true);
	} else {
		rz_config_set_b(core->config, "asm.hint.call", true);
	}
}

RZ_IPI void rz_core_visual_toggle_decompiler_disasm(RzCore *core, bool for_graph, bool reset) {
	static RzConfigHold *hold = NULL; // should be a tab-specific var
	if (hold) {
		rz_config_hold_restore(hold);
		rz_config_hold_free(hold);
		hold = NULL;
		return;
	}
	if (reset) {
		return;
	}
	hold = rz_config_hold_new(core->config);
	rz_config_hold_s(hold, "asm.hint.pos", "asm.cmt.col", "asm.offset", "asm.lines",
		"asm.indent", "asm.bytes", "asm.comments", "asm.dwarf", "asm.usercomments", "asm.instr", NULL);
	if (for_graph) {
		rz_config_set(core->config, "asm.hint.pos", "-2");
		rz_config_set(core->config, "asm.lines", "false");
		rz_config_set(core->config, "asm.indent", "false");
	} else {
		rz_config_set(core->config, "asm.hint.pos", "0");
		rz_config_set(core->config, "asm.indent", "true");
		rz_config_set(core->config, "asm.lines", "true");
	}
	rz_config_set(core->config, "asm.cmt.col", "0");
	rz_config_set(core->config, "asm.offset", "false");
	rz_config_set(core->config, "asm.dwarf", "true");
	rz_config_set(core->config, "asm.bytes", "false");
	rz_config_set(core->config, "asm.comments", "false");
	rz_config_set(core->config, "asm.usercomments", "true");
	rz_config_set(core->config, "asm.instr", "false");
}

static void setcursor(RzCore *core, bool cur) {
	int flags = core->print->flags;
	if (core->print->cur_enabled) {
		flags |= RZ_PRINT_FLAGS_CURSOR;
	} else {
		flags &= ~(RZ_PRINT_FLAGS_CURSOR);
	}
	core->print->cur_enabled = cur;
	if (core->print->cur == -1) {
		core->print->cur = 0;
	}
	rz_print_set_flags(core->print, flags);
	core->print->col = core->print->cur_enabled ? 1 : 0;
}

RZ_IPI void rz_core_visual_applyDisMode(RzCore *core, int disMode) {
	RzCoreVisual *visual = core->visual;
	visual->currentFormat = RZ_ABS(disMode) % 5;
	switch (visual->currentFormat) {
	case 0:
		rz_config_set(core->config, "asm.pseudo", "false");
		rz_config_set(core->config, "asm.bytes", "true");
		rz_config_set(core->config, "asm.esil", "false");
		rz_config_set(core->config, "emu.str", "false");
		rz_config_set(core->config, "asm.emu", "false");
		break;
	case 1:
		rz_config_set(core->config, "asm.pseudo", "false");
		rz_config_set(core->config, "asm.bytes", "true");
		rz_config_set(core->config, "asm.esil", "false");
		rz_config_set(core->config, "asm.emu", "false");
		rz_config_set(core->config, "emu.str", "true");
		break;
	case 2:
		rz_config_set(core->config, "asm.pseudo", "true");
		rz_config_set(core->config, "asm.bytes", "true");
		rz_config_set(core->config, "asm.esil", "true");
		rz_config_set(core->config, "emu.str", "true");
		rz_config_set(core->config, "asm.emu", "true");
		break;
	case 3:
		rz_config_set(core->config, "asm.pseudo", "false");
		rz_config_set(core->config, "asm.bytes", "false");
		rz_config_set(core->config, "asm.esil", "false");
		rz_config_set(core->config, "asm.emu", "false");
		rz_config_set(core->config, "emu.str", "true");
		break;
	case 4:
		rz_config_set(core->config, "asm.pseudo", "true");
		rz_config_set(core->config, "asm.bytes", "false");
		rz_config_set(core->config, "asm.esil", "false");
		rz_config_set(core->config, "asm.emu", "false");
		rz_config_set(core->config, "emu.str", "true");
		break;
	}
}

static void nextPrintCommand(RzCore *core) {
	RzCoreVisual *visual = core->visual;
	visual->current0format++;
	visual->current0format %= PRINT_HEX_FORMATS;
	visual->currentFormat = visual->current0format;
}

static void prevPrintCommand(RzCore *core) {
	RzCoreVisual *visual = core->visual;
	visual->current0format--;
	if (visual->current0format < 0) {
		visual->current0format = 0;
	}
	visual->currentFormat = visual->current0format;
}

static const char *stackPrintCommand(RzCore *core) {
	RzCoreVisual *visual = core->visual;
	if (visual->current0format == 0) {
		if (rz_config_get_b(core->config, "dbg.slow")) {
			return "pxr";
		}
		if (rz_config_get_b(core->config, "stack.bytes")) {
			return "px";
		}
		switch (core->rasm->bits) {
		case 64: return "pxq"; break;
		case 32: return "pxw"; break;
		}
		return "px";
	}
	return printHexFormats[visual->current0format % PRINT_HEX_FORMATS];
}

static const char *__core_visual_print_command(RzCore *core) {
	RzCoreVisual *visual = core->visual;
	if (visual->tabs) {
		RzCoreVisualTab *tab = rz_list_get_n(visual->tabs, visual->tab);
		if (tab && tab->name[0] == ':') {
			return tab->name + 1;
		}
	}
	if (rz_config_get_i(core->config, "scr.dumpcols")) {
		free(core->stkcmd);
		core->stkcmd = rz_str_new(stackPrintCommand(core));
		return printfmtColumns[PIDX];
	}
	return printfmtSingle[PIDX];
}

static bool __core_visual_gogo(RzCore *core, int ch) {
	RzIOMap *map;
	switch (ch) {
	case 'g':
		if (core->io->va) {
			RzIOMap *map = rz_io_map_get(core->io, core->offset);
			RzPVector *maps = rz_io_maps(core->io);
			if (!map && !rz_pvector_empty(maps)) {
				map = rz_pvector_at(maps, rz_pvector_len(maps) - 1);
			}
			if (map) {
				rz_core_seek_and_save(core, rz_itv_begin(map->itv), true);
			}
		} else {
			rz_core_seek_and_save(core, 0, true);
		}
		return true;
	case 'G': {
		map = rz_io_map_get(core->io, core->offset);
		RzPVector *maps = rz_io_maps(core->io);
		if (!map && !rz_pvector_empty(maps)) {
			map = rz_pvector_at(maps, 0);
		}
		if (map) {
			RzPrint *p = core->print;
			int scr_rows;
			if (!p->consbind.get_size) {
				break;
			}
			(void)p->consbind.get_size(&scr_rows);
			ut64 scols = rz_config_get_i(core->config, "hex.cols");
			rz_core_seek_and_save(core, rz_itv_end(map->itv) - (scr_rows - 2) * scols, true);
		}
		return true;
	}
	}
	return false;
}

static const char *help_visual[] = {
	"?", "full help",
	"!", "enter panels",
	"a", "code analysis",
	"b", "browse mode",
	"c", "toggle cursor",
	"d", "debugger / emulator",
	"e", "toggle configurations",
	"i", "insert / write",
	"m", "moving around (seeking)",
	"p", "print commands and modes",
	"v", "view management",
	NULL
};

static const char *help_msg_visual[] = {
	"?", "show visual help menu",
	"??", "show this help",
	"$", "set the program counter to the current offset + cursor",
	"&", "rotate asm.bits between 8, 16, 32 and 64 applying hints",
	"%", "in cursor mode finds matching pair, otherwise toggle autoblocksz",
	"^", "seek to the beginning of the function",
	"!", "enter into the visual panels mode",
	"TAB", "switch to the next print mode (or element in cursor mode)",
	"_", "enter the flag/comment/functions/.. hud (same as VF_)",
	"=", "set cmd.vprompt (top row)",
	"|", "set cmd.cprompt (right column)",
	".", "seek to program counter",
	"#", "toggle decompiler comments in disasm (see pdd* from jsdec)",
	"\\", "toggle visual split mode",
	"\"", "toggle the column mode (uses pC..)",
	"/", "in cursor mode search in current block",
	")", "toggle emu.str",
	":cmd", "run rizin command",
	";[-]cmt", "add/remove comment",
	"0", "seek to beginning of current function",
	"[1-9]", "follow jmp/call identified by shortcut (like ;[1])",
	",file", "add a link to the text file",
	"/*+-[]", "change block size, [] = resize hex.cols",
	"<,>", "seek aligned to block size (in cursor slurp or dump files)",
	"a/A", "(a)ssemble code, visual (A)ssembler",
	"b", "browse evals, symbols, flags, evals, classes, ...",
	"B", "toggle breakpoint",
	"c/C", "toggle (c)ursor and (C)olors",
	"d[f?]", "define function, data, code, ..",
	"D", "enter visual diff mode (set diff.from/to)",
	"f/F", "set/unset or browse flags. f- to unset, F to browse, ..",
	"hjkl", "move around (or HJKL) (left-down-up-right)",
	"i", "insert hex or string (in hexdump) use tab to toggle",
	"I", "insert hexpair block ",
	"mK/'K", "mark/go to Key (any key)",
	"n/N", "seek next/prev function/flag/hit (scr.nkey)",
	"g", "go/seek to given offset (g[g/G]<enter> to seek begin/end of file)",
	"O", "toggle asm.pseudo and asm.esil",
	"p/P", "rotate print modes (hex, disasm, debug, words, buf)",
	"q", "back to rizin shell",
	"r", "toggle call/jmp/lea hints",
	"R", "randomize color palette (ecr)",
	"sS", "step / step over",
	"tT", "tt new tab, t[1-9] switch to nth tab, t= name tab, t- close tab",
	"uU", "undo/redo seek",
	"v", "visual function/vars code analysis menu",
	"V", "(V)iew interactive ascii art graph (agfv)",
	"wW", "seek cursor to next/prev word",
	"xX", "show xrefs/refs of current function from/to data/code",
	"yY", "copy and paste selection",
	"Enter", "follow address of jump/call",
	NULL
};

static const char *help_msg_visual_fn[] = {
	"F2", "toggle breakpoint",
	"F4", "run to cursor",
	"F7", "single step",
	"F8", "step over",
	"F9", "continue",
	NULL
};

static void rotateAsmBits(RzCore *core) {
	RzAnalysisHint *hint = rz_analysis_hint_get(core->analysis, core->offset);
	int bits = hint ? hint->bits : rz_config_get_i(core->config, "asm.bits");
	int retries = 4;
	while (retries > 0) {
		int nb = bits == 64 ? 8 : bits == 32 ? 64
			: bits == 16                 ? 32
			: bits == 8                  ? 16
						     : bits;
		if ((core->rasm->cur->bits & nb) == nb) {
			rz_analysis_hint_set_bits(core->analysis, core->offset, nb);
			break;
		}
		bits = nb;
		retries--;
	}
	rz_analysis_hint_free(hint);
}

static const char *rotateAsmemu(RzCore *core) {
	const bool isEmuStr = rz_config_get_b(core->config, "emu.str");
	const bool isEmu = rz_config_get_b(core->config, "asm.emu");
	if (isEmu) {
		if (isEmuStr) {
			rz_config_set(core->config, "emu.str", "false");
		} else {
			rz_config_set(core->config, "asm.emu", "false");
		}
	} else {
		rz_config_set(core->config, "emu.str", "true");
	}
	return "pd";
}

RZ_IPI void rz_core_visual_showcursor(RzCore *core, int x) {
	if (core && core->vmode) {
		rz_cons_show_cursor(x);
		rz_cons_enable_mouse(rz_config_get_b(core->config, "scr.wheel"));
	} else {
		rz_cons_enable_mouse(false);
	}
	rz_cons_flush();
}

static void printFormat(RzCore *core, const int next) {
	RzCoreVisual *visual = core->visual;
	switch (visual->printidx) {
	case RZ_CORE_VISUAL_MODE_PX: // 0 // xc
		visual->hexMode += next;
		rz_core_visual_applyHexMode(core, visual->hexMode);
		printfmtSingle[0] = printHexFormats[RZ_ABS(visual->hexMode) % PRINT_HEX_FORMATS];
		break;
	case RZ_CORE_VISUAL_MODE_PD: // pd
		visual->disMode += next;
		rz_core_visual_applyDisMode(core, visual->disMode);
		printfmtSingle[1] = rotateAsmemu(core);
		break;
	case RZ_CORE_VISUAL_MODE_DB: // debugger
		visual->disMode += next;
		rz_core_visual_applyDisMode(core, visual->disMode);
		printfmtSingle[1] = rotateAsmemu(core);
		visual->current3format += next;
		visual->currentFormat = RZ_ABS(visual->current3format) % PRINT_3_FORMATS;
		printfmtSingle[2] = print3Formats[visual->currentFormat];
		break;
	case RZ_CORE_VISUAL_MODE_OV: // overview
		visual->current4format += next;
		visual->currentFormat = RZ_ABS(visual->current4format) % PRINT_4_FORMATS;
		printfmtSingle[3] = print4Formats[visual->currentFormat];
		break;
	case RZ_CORE_VISUAL_MODE_CD: // code
		visual->current5format += next;
		visual->currentFormat = RZ_ABS(visual->current5format) % PRINT_5_FORMATS;
		printfmtSingle[4] = print5Formats[visual->currentFormat];
		break;
	}
}

static inline void nextPrintFormat(RzCore *core) {
	printFormat(core, 1);
}

static inline void prevPrintFormat(RzCore *core) {
	printFormat(core, -1);
}

RZ_IPI void rz_core_visual_jump(RzCore *core, ut8 ch) {
	char chbuf[2];
	ut64 off;
	chbuf[0] = ch;
	chbuf[1] = '\0';
	off = rz_core_get_asmqjmps(core, chbuf);
	if (off != UT64_MAX) {
		int delta = RZ_ABS((st64)off - (st64)core->offset);
		if (core->print->cur_enabled && delta < 100) {
			core->print->cur = delta;
		} else {
			rz_core_visual_seek_animation(core, off);
			core->print->cur = 0;
		}
		rz_core_block_read(core);
	}
}

RZ_IPI void rz_core_visual_append_help(RzStrBuf *p, const char *title, const char **help) {
	int i, max_length = 0, padding = 0;
	RzConsContext *cons_ctx = rz_cons_singleton()->context;
	const char *pal_args_color = cons_ctx->color_mode ? cons_ctx->pal.args : "",
		   *pal_help_color = cons_ctx->color_mode ? cons_ctx->pal.help : "",
		   *pal_reset = cons_ctx->color_mode ? cons_ctx->pal.reset : "";
	for (i = 0; help[i]; i += 2) {
		max_length = RZ_MAX(max_length, strlen(help[i]));
	}
	rz_strbuf_appendf(p, "|%s:\n", title);

	for (i = 0; help[i]; i += 2) {
		padding = max_length - (strlen(help[i]));
		rz_strbuf_appendf(p, "| %s%s%*s  %s%s%s\n",
			pal_args_color, help[i],
			padding, "",
			pal_help_color, help[i + 1], pal_reset);
	}
}

static int visual_help(RzCore *core) {
	int ret = 0;
	RzStrBuf *p, *q;
repeat:
	p = rz_strbuf_new(NULL);
	q = rz_strbuf_new(NULL);
	if (!p) {
		return 0;
	}
	rz_cons_clear00();
	rz_core_visual_append_help(q, "Visual Help", help_visual);
	rz_cons_printf("%s", rz_strbuf_get(q));
	rz_cons_flush();
	switch (rz_cons_readchar()) {
	case 'q':
		rz_strbuf_free(p);
		rz_strbuf_free(q);
		return ret;
	case '!':
		rz_core_visual_panels_root(core, core->panels_root);
		break;
	case '?':
		rz_core_visual_append_help(p, "Visual mode help", help_msg_visual);
		rz_core_visual_append_help(p, "Function Keys: (See 'e key.'), defaults to", help_msg_visual_fn);
		ret = rz_cons_less_str(rz_strbuf_get(p), "?");
		break;
	case 'v':
		rz_strbuf_appendf(p, "Visual Views:\n\n");
		rz_strbuf_appendf(p,
			" \\     toggle horizonal split mode\n"
			" tt     create a new tab (same as t+)\n"
			" t=     give a name to the current tab\n"
			" t-     close current tab\n"
			" th     select previous tab (same as tj)\n"
			" tl     select next tab (same as tk)\n"
			" t[1-9] select nth tab\n"
			" C   -> rotate scr.color=0,1,2,3\n"
			" R   -> rotate color theme with ecr command which honors scr.randpal\n");
		ret = rz_cons_less_str(rz_strbuf_get(p), "?");
		break;
	case 'p':
		rz_strbuf_appendf(p, "Visual Print Modes:\n\n");
		rz_strbuf_appendf(p,
			" pP  -> change to the next/previous print mode (hex, dis, ..)\n"
			" TAB -> rotate between all the configurations for the current print mode\n");
		ret = rz_cons_less_str(rz_strbuf_get(p), "?");
		break;
	case 'e':
		rz_strbuf_appendf(p, "Visual Evals:\n\n");
		rz_strbuf_appendf(p,
			" E      toggle asm.hint.lea\n"
			" &      rotate asm.bits=16,32,64\n");
		ret = rz_cons_less_str(rz_strbuf_get(p), "?");
		break;
	case 'c':
		setcursor(core, !core->print->cur_enabled);
		rz_strbuf_free(p);
		rz_strbuf_free(q);
		return ret;
	case 'i':
		rz_strbuf_appendf(p, "Visual Insertion Help:\n\n");
		rz_strbuf_appendf(p,
			" i   -> insert bits, bytes or text depending on view\n"
			" a   -> assemble instruction and write the bytes in the current offset\n"
			" A   -> visual assembler\n"
			" +   -> increment value of byte\n"
			" -   -> decrement value of byte\n");
		ret = rz_cons_less_str(rz_strbuf_get(p), "?");
		break;
	case 'd':
		rz_strbuf_appendf(p, "Visual Debugger Help:\n\n");
		rz_strbuf_appendf(p,
			" $   -> set the program counter (PC register)\n"
			" s   -> step in\n"
			" S   -> step over\n"
			" B   -> toggle breakpoint\n"
			" :dc -> continue\n");
		ret = rz_cons_less_str(rz_strbuf_get(p), "?");
		break;
	case 'm':
		rz_strbuf_appendf(p, "Visual Moving Around:\n\n");
		rz_strbuf_appendf(p,
			" g        type flag/offset/register name to seek\n"
			" hl       seek to the next/previous byte\n"
			" jk       seek to the next row (core.offset += hex.cols)\n"
			" JK       seek one page down\n"
			" ^        seek to the beginning of the current map\n"
			" $        seek to the end of the current map\n"
			" c        toggle cursor mode (use hjkl to move and HJKL to select a range)\n"
			" mK/'K    mark/go to Key (any key)\n");
		ret = rz_cons_less_str(rz_strbuf_get(p), "?");
		break;
	case 'a':
		rz_strbuf_appendf(p, "Visual Analysis:\n\n");
		rz_strbuf_appendf(p,
			" df -> define function\n"
			" du -> undefine function\n"
			" dc -> define as code\n"
			" dw -> define as dword (32bit)\n"
			" dw -> define as qword (64bit)\n"
			" dd -> define current block or selected bytes as data\n"
			" V  -> view graph (same as press the 'space' key)\n");
		ret = rz_cons_less_str(rz_strbuf_get(p), "?");
		break;
	}
	rz_strbuf_free(p);
	rz_strbuf_free(q);
	goto repeat;
}

static void prompt_read(const char *p, char *buf, int buflen) {
	if (!buf || buflen < 1) {
		return;
	}
	*buf = 0;
	rz_line_set_prompt(p);
	rz_core_visual_showcursor(NULL, true);
	rz_cons_fgets(buf, buflen, 0, NULL);
	rz_core_visual_showcursor(NULL, false);
}

static void reset_print_cur(RzPrint *p) {
	p->cur = 0;
	p->ocur = -1;
}

static bool __holdMouseState(RzCore *core) {
	bool m = rz_cons_singleton()->mouse;
	rz_cons_enable_mouse(false);
	return m;
}

static void backup_current_addr(RzCore *core, ut64 *addr, ut64 *bsze, ut64 *newaddr, int *cur) {
	*addr = core->offset;
	*bsze = core->blocksize;
	*cur = core->print->cur_enabled ? core->print->cur : 0;
	if (core->print->cur_enabled) {
		if (core->print->ocur != -1) {
			int newsz = core->print->cur - core->print->ocur;
			*newaddr = core->offset + core->print->ocur;
			rz_core_block_size(core, newsz);
		} else {
			*newaddr = core->offset + core->print->cur;
		}
		rz_core_seek(core, *newaddr, true);
		core->print->cur = 0;
	}
}

static void restore_current_addr(RzCore *core, ut64 addr, ut64 bsze, ut64 newaddr, int cur) {
	bool restore_seek = true;
	bool cursor_moved = false;
	if (core->offset != newaddr) {
		// when new address is in the screen bounds, just move
		// the cursor if enabled and restore seek
		if (core->print->cur_enabled && core->print->screen_bounds > 1) {
			if (core->offset >= addr &&
				core->offset < core->print->screen_bounds) {
				core->print->ocur = -1;
				core->print->cur = core->offset - addr;
				cursor_moved = true;
			}
		}

		if (!cursor_moved) {
			restore_seek = false;
			reset_print_cur(core->print);
		}
	}

	if (core->print->cur_enabled) {
		if (restore_seek) {
			rz_core_seek(core, addr, true);
			rz_core_block_size(core, bsze);
			if (!cursor_moved) {
				core->print->cur = cur;
			}
		}
	}
}

RZ_IPI void rz_core_visual_prompt_input(RzCore *core) {
	ut64 addr, bsze, newaddr = 0LL;
	int ret, h, cur;
	(void)rz_cons_get_size(&h);
	bool mouse_state = __holdMouseState(core);
	rz_cons_gotoxy(0, h);
	rz_cons_reset_colors();
	// rz_cons_printf ("\nPress <enter> to return to Visual mode.\n");
	rz_cons_show_cursor(true);
	core->vmode = false;

	backup_current_addr(core, &addr, &bsze, &newaddr, &cur);
	do {
		ret = rz_core_visual_prompt(core);
	} while (ret);
	restore_current_addr(core, addr, bsze, newaddr, cur);

	rz_cons_show_cursor(false);
	core->vmode = true;
	rz_cons_enable_mouse(mouse_state && rz_config_get_b(core->config, "scr.wheel"));
	rz_cons_show_cursor(true);
}

RZ_IPI int rz_core_visual_prompt(RzCore *core) {
	char buf[1024];
	int ret;
	if (PIDX != 2) {
		core->seltab = 0;
	}
#if __UNIX__
	rz_line_set_prompt(Color_RESET ":> ");
#else
	rz_line_set_prompt(":> ");
#endif
	rz_core_visual_showcursor(core, true);
	rz_cons_fgets(buf, sizeof(buf), 0, NULL);
	if (!strcmp(buf, "q")) {
		ret = false;
	} else if (*buf) {
		rz_line_hist_add(buf);
		rz_core_cmd(core, buf, 0);
		rz_cons_echo(NULL);
		rz_cons_flush();
		ret = true;
		if (rz_config_get_b(core->config, "cfg.debug")) {
			rz_core_reg_update_flags(core);
		}
	} else {
		ret = false;
		// rz_cons_any_key (NULL);
		rz_cons_clear00();
		rz_core_visual_showcursor(core, false);
	}
	return ret;
}

static void visual_breakpoint(RzCore *core) {
	rz_core_debug_breakpoint_toggle(core, core->offset);
}

static int visual_nkey(RzCore *core, int ch) {
	const char *cmd;
	ut64 oseek = UT64_MAX;
	if (core->print->ocur == -1) {
		oseek = core->offset;
		rz_core_seek(core, core->offset + core->print->cur, false);
	}

	switch (ch) {
	case RZ_CONS_KEY_F1:
		cmd = rz_config_get(core->config, "key.f1");
		if (cmd && *cmd) {
			ch = rz_core_cmd0(core, cmd);
		} else {
			visual_help(core);
		}
		break;
	case RZ_CONS_KEY_F2:
		cmd = rz_config_get(core->config, "key.f2");
		if (cmd && *cmd) {
			ch = rz_core_cmd0(core, cmd);
		} else {
			visual_breakpoint(core);
		}
		break;
	case RZ_CONS_KEY_F3:
		cmd = rz_config_get(core->config, "key.f3");
		if (cmd && *cmd) {
			ch = rz_core_cmd0(core, cmd);
		}
		break;
	case RZ_CONS_KEY_F4:
		cmd = rz_config_get(core->config, "key.f4");
		if (cmd && *cmd) {
			ch = rz_core_cmd0(core, cmd);
		} else {
			if (core->print->cur_enabled) {
				rz_core_debug_continue_until(core, core->offset, core->offset + core->print->cur);
				core->print->cur_enabled = 0;
			}
		}
		break;
	case RZ_CONS_KEY_F5:
		cmd = rz_config_get(core->config, "key.f5");
		if (cmd && *cmd) {
			ch = rz_core_cmd0(core, cmd);
		}
		break;
	case RZ_CONS_KEY_F6:
		cmd = rz_config_get(core->config, "key.f6");
		if (cmd && *cmd) {
			ch = rz_core_cmd0(core, cmd);
		}
		break;
	case RZ_CONS_KEY_F7:
		cmd = rz_config_get(core->config, "key.f7");
		if (cmd && *cmd) {
			ch = rz_core_cmd0(core, cmd);
		} else {
			rz_core_debug_single_step_in(core);
		}
		break;
	case RZ_CONS_KEY_F8:
		cmd = rz_config_get(core->config, "key.f8");
		if (cmd && *cmd) {
			ch = rz_core_cmd0(core, cmd);
		} else {
			rz_core_debug_single_step_over(core);
		}
		break;
	case RZ_CONS_KEY_F9:
		cmd = rz_config_get(core->config, "key.f9");
		if (cmd && *cmd) {
			ch = rz_core_cmd0(core, cmd);
		} else {
			rz_core_debug_continue(core);
		}
		break;
	case RZ_CONS_KEY_F10:
		cmd = rz_config_get(core->config, "key.f10");
		if (cmd && *cmd) {
			ch = rz_core_cmd0(core, cmd);
		}
		break;
	case RZ_CONS_KEY_F11:
		cmd = rz_config_get(core->config, "key.f11");
		if (cmd && *cmd) {
			ch = rz_core_cmd0(core, cmd);
		}
		break;
	case RZ_CONS_KEY_F12:
		cmd = rz_config_get(core->config, "key.f12");
		if (cmd && *cmd) {
			ch = rz_core_cmd0(core, cmd);
		}
		break;
	}
	if (oseek != UT64_MAX) {
		rz_core_seek(core, oseek, false);
	}
	return ch;
}

static void setdiff(RzCore *core) {
	char from[64], to[64];
	prompt_read("diff from: ", from, sizeof(from));
	rz_config_set(core->config, "diff.from", from);
	prompt_read("diff to: ", to, sizeof(to));
	rz_config_set(core->config, "diff.to", to);
}

static void findPair(RzCore *core) {
	ut8 buf[256];
	int i, len, d = core->print->cur + 1;
	int delta = 0;
	const ut8 *p, *q = NULL;
	const char *keys = "{}[]()<>";
	ut8 ch = core->block[core->print->cur];

	p = (const ut8 *)strchr(keys, ch);
	if (p) {
		char p_1 = 0;
		if ((const char *)p > keys) {
			p_1 = p[-1];
		}
		delta = (size_t)(p - (const ut8 *)keys);
		ch = (delta % 2 && p != (const ut8 *)keys) ? p_1 : p[1];
	}
	len = 1;
	buf[0] = ch;

	if (p && (delta % 2)) {
		for (i = d - 1; i >= 0; i--) {
			if (core->block[i] == ch) {
				q = core->block + i;
				break;
			}
		}
	} else {
		q = rz_mem_mem(core->block + d, core->blocksize - d,
			(const ut8 *)buf, len);
		if (!q) {
			q = rz_mem_mem(core->block, RZ_MIN(core->blocksize, d),
				(const ut8 *)buf, len);
		}
	}
	if (q) {
		core->print->cur = (int)(size_t)(q - core->block);
		core->print->ocur = -1;
		rz_core_visual_showcursor(core, true);
	}
}

static void findNextWord(RzCore *core) {
	int i, d = core->print->cur_enabled ? core->print->cur : 0;
	for (i = d + 1; i < core->blocksize; i++) {
		switch (core->block[i]) {
		case ' ':
		case '.':
		case '\t':
		case '\n':
			if (core->print->cur_enabled) {
				core->print->cur = i + 1;
				core->print->ocur = -1;
				rz_core_visual_showcursor(core, true);
			} else {
				rz_core_seek_and_save(core, core->offset + i + 1, true);
			}
			return;
		}
	}
}

static int isSpace(char ch) {
	switch (ch) {
	case ' ':
	case '.':
	case ',':
	case '\t':
	case '\n':
		return 1;
	}
	return 0;
}

static void findPrevWord(RzCore *core) {
	int i = core->print->cur_enabled ? core->print->cur : 0;
	while (i > 1) {
		if (isSpace(core->block[i])) {
			i--;
		} else if (isSpace(core->block[i - 1])) {
			i -= 2;
		} else {
			break;
		}
	}
	for (; i >= 0; i--) {
		if (isSpace(core->block[i])) {
			if (core->print->cur_enabled) {
				core->print->cur = i + 1;
				core->print->ocur = -1;
				rz_core_visual_showcursor(core, true);
			}
			break;
		}
	}
}

// TODO: integrate in '/' command with search.inblock ?
static void visual_search(RzCore *core) {
	const ut8 *p;
	int len, d = core->print->cur;
	char str[128], buf[sizeof(str) * 2 + 1];

	rz_line_set_prompt("search byte/string in block: ");
	rz_cons_fgets(str, sizeof(str), 0, NULL);
	len = rz_hex_str2bin(str, (ut8 *)buf);
	if (*str == '"') {
		rz_str_ncpy(buf, str + 1, sizeof(buf));
		len = strlen(buf);
		char *e = buf + len - 1;
		if (e > buf && *e == '"') {
			*e = 0;
			len--;
		}
	} else if (len < 1) {
		rz_str_ncpy(buf, str, sizeof(buf));
		len = strlen(buf);
	}
	p = rz_mem_mem(core->block + d, core->blocksize - d,
		(const ut8 *)buf, len);
	if (p) {
		core->print->cur = (int)(size_t)(p - core->block);
		if (len > 1) {
			core->print->ocur = core->print->cur + len - 1;
		} else {
			core->print->ocur = -1;
		}
		rz_core_visual_showcursor(core, true);
		rz_cons_printf("Found in offset 0x%08" PFMT64x " + %d\n", core->offset, core->print->cur);
		rz_cons_any_key(NULL);
	} else {
		RZ_LOG_ERROR("core: Cannot find bytes.\n");
		rz_cons_any_key(NULL);
		rz_cons_clear00();
	}
}

RZ_IPI void rz_core_visual_show_char(RzCore *core, char ch) {
	if (rz_config_get_i(core->config, "scr.feedback") < 2) {
		return;
	}
	if (!IS_PRINTABLE(ch)) {
		return;
	}
	rz_cons_gotoxy(1, 2);
	rz_cons_printf(".---.\n");
	rz_cons_printf("| %c |\n", ch);
	rz_cons_printf("'---'\n");
	rz_cons_flush();
	rz_sys_sleep(1);
}

static void visual_seek_animation(RzCore *core, ut64 addr) {
	if (rz_config_get_i(core->config, "scr.feedback") < 1) {
		return;
	}
	if (core->offset == addr) {
		return;
	}
	rz_cons_gotoxy(1, 2);
	if (addr > core->offset) {
		rz_cons_printf(".----.\n");
		rz_cons_printf("| \\/ |\n");
		rz_cons_printf("'----'\n");
	} else {
		rz_cons_printf(".----.\n");
		rz_cons_printf("| /\\ |\n");
		rz_cons_printf("'----'\n");
	}
	rz_cons_flush();
	rz_sys_usleep(90000);
}

RZ_IPI void rz_core_visual_seek_animation(RzCore *core, ut64 addr) {
	rz_core_seek_and_save(core, addr, true);
	visual_seek_animation(core, addr);
}

RZ_IPI void rz_core_visual_seek_animation_redo(RzCore *core) {
	if (rz_core_seek_redo(core)) {
		visual_seek_animation(core, core->offset);
	}
}

RZ_IPI void rz_core_visual_seek_animation_undo(RzCore *core) {
	if (rz_core_seek_undo(core)) {
		visual_seek_animation(core, core->offset);
	}
}

static void setprintmode(RzCore *core, int n) {
	RzCoreVisual *visual = core->visual;
	rz_config_set_i(core->config, "scr.visual.mode", visual->printidx + n);
	RzAsmOp op;

	switch (visual->printidx) {
	case RZ_CORE_VISUAL_MODE_PD:
	case RZ_CORE_VISUAL_MODE_DB:
		rz_asm_op_init(&op);
		rz_asm_disassemble(core->rasm, &op, core->block, RZ_MIN(32, core->blocksize));
		rz_asm_op_fini(&op);
		break;
	default:
		break;
	}
}

static bool fill_hist_offset(RzCore *core, RzLine *line, RzCoreSeekItem *csi) {
	ut64 off = csi->offset;
	RzFlagItem *f = rz_flag_get_at(core->flags, off, false);
	char *command = NULL;
	if (f && f->offset == off && f->offset > 0) {
		command = rz_str_newf("%s", f->name);
	} else {
		command = rz_str_newf("0x%" PFMT64x, off);
	}
	if (!command) {
		return false;
	}

	strncpy(line->buffer.data, command, RZ_LINE_BUFSIZE - 1);
	line->buffer.index = line->buffer.length = strlen(line->buffer.data);
	free(command);
	return true;
}

RZ_IPI int rz_line_hist_offset_up(RzLine *line) {
	RzCore *core = (RzCore *)line->user;
	RzCoreSeekItem *csi = rz_core_seek_peek(core, line->offset_hist_index - 1);
	if (!csi) {
		return false;
	}

	line->offset_hist_index--;
	bool res = fill_hist_offset(core, line, csi);
	rz_core_seek_item_free(csi);
	return res;
}

RZ_IPI int rz_line_hist_offset_down(RzLine *line) {
	RzCore *core = (RzCore *)line->user;
	RzCoreSeekItem *csi = rz_core_seek_peek(core, line->offset_hist_index + 1);
	if (!csi) {
		return false;
	}
	line->offset_hist_index++;
	bool res = fill_hist_offset(core, line, csi);
	rz_core_seek_item_free(csi);
	return res;
}

RZ_IPI void rz_core_visual_offset(RzCore *core) {
	char buf[256];

	core->cons->line->prompt_type = RZ_LINE_PROMPT_OFFSET;
	rz_line_set_hist_callback(core->cons->line,
		&rz_line_hist_offset_up,
		&rz_line_hist_offset_down);
	rz_line_set_prompt("[offset]> ");
	if (rz_cons_fgets(buf, sizeof(buf) - 1, 0, NULL) > 0) {
		if (!strcmp(buf, "g") || !strcmp(buf, "G")) {
			__core_visual_gogo(core, buf[0]);
		} else {
			if (buf[0] == '.') {
				rz_core_seek_base(core, buf + 1, true);
			} else {
				ut64 addr = rz_num_math(core->num, buf);
				rz_core_seek_and_save(core, addr, true);
			}
		}
		reset_print_cur(core->print);
	}
	rz_line_set_hist_callback(core->cons->line, &rz_line_hist_cmd_up, &rz_line_hist_cmd_down);
	core->cons->line->prompt_type = RZ_LINE_PROMPT_DEFAULT;
}

RZ_IPI int rz_core_visual_prevopsz(RzCore *core, ut64 addr) {
	ut64 prev_addr = rz_core_prevop_addr_heuristic(core, addr);
	return addr - prev_addr;
}

static void add_comment(RzCore *core, ut64 addr, const char *prompt) {
	char buf[1024];
	rz_cons_print(prompt);
	rz_core_visual_showcursor(core, true);
	rz_cons_flush();
	rz_cons_set_raw(false);
	rz_line_set_prompt(":> ");
	rz_cons_enable_mouse(false);
	if (rz_cons_fgets(buf, sizeof(buf), 0, NULL) < 0) {
		buf[0] = '\0';
	}
	if (!strcmp(buf, "-")) {
		rz_meta_del(core->analysis, RZ_META_TYPE_COMMENT, addr, 1);
	} else if (!strcmp(buf, "!")) {
		rz_core_meta_editor(core, RZ_META_TYPE_COMMENT, addr);
	} else {
		rz_core_meta_append(core, buf, RZ_META_TYPE_COMMENT, addr);
	}
	rz_core_visual_showcursor(core, false);
	rz_cons_set_raw(true);
}

static int follow_ref(RzCore *core, RzList /*<RzAnalysisXRef *>*/ *xrefs, int choice, bool xref_to) {
	RzAnalysisXRef *xrefi = rz_list_get_n(xrefs, choice);
	if (xrefi) {
		if (core->print->cur_enabled) {
			core->print->cur = 0;
		}
		ut64 addr = xref_to ? xrefi->from : xrefi->to;
		rz_core_seek_and_save(core, addr, true);
		return 1;
	}
	return 0;
}

RZ_IPI int rz_core_visual_xrefs(RzCore *core, bool xref_to, bool fcnInsteadOfAddr) {
	ut64 cur_ref_addr = UT64_MAX;
	int ret = 0;
	char ch;
	int count = 0;
	RzList *xrefs = NULL;
	RzAnalysisXRef *xrefi;
	RzListIter *iter;
	int skip = 0;
	int idx = 0;
	char cstr[32];
	ut64 addr = core->offset;
	bool xrefsMode = fcnInsteadOfAddr;
	RzCoreVisual *visual = core->visual;
	if (core->print->cur_enabled) {
		addr += core->print->cur;
	}
repeat:
	rz_list_free(xrefs);
	if (xrefsMode) {
		RzAnalysisFunction *fun = rz_analysis_get_fcn_in(core->analysis, addr, RZ_ANALYSIS_FCN_TYPE_NULL);
		if (fun) {
			if (xref_to) { //  function xrefs
				xrefs = rz_analysis_xrefs_get_to(core->analysis, addr);
				// XXX xrefs = rz_analysis_function_get_xrefs_to(core->analysis, fun);
				//  this function is buggy so we must get the xrefs of the addr
			} else { // functon refs
				xrefs = rz_analysis_function_get_xrefs_from(fun);
			}
		} else {
			xrefs = NULL;
		}
	} else {
		if (xref_to) { // address xrefs
			xrefs = rz_analysis_xrefs_get_to(core->analysis, addr);
		} else { // address refs
			xrefs = rz_analysis_xrefs_get_from(core->analysis, addr);
		}
	}

	rz_cons_clear00();
	rz_cons_gotoxy(1, 1);
	{
		char *address = (core->dbg->bits & RZ_SYS_BITS_64)
			? rz_str_newf("0x%016" PFMT64x, addr)
			: rz_str_newf("0x%08" PFMT64x, addr);
		rz_cons_printf("[%s%srefs]> %s # (TAB/jk/q/?) ",
			xrefsMode ? "fcn." : "addr.", xref_to ? "x" : "", address);
		free(address);
	}
	if (!xrefs || rz_list_empty(xrefs)) {
		rz_list_free(xrefs);
		xrefs = NULL;
		rz_cons_printf("\n\n(no %srefs)\n", xref_to ? "x" : "");
	} else {
		int h, w = rz_cons_get_size(&h);
		bool asm_bytes = rz_config_get_b(core->config, "asm.bytes");
		rz_config_set_b(core->config, "asm.bytes", false);
		RzCmdStateOutput state;
		if (!rz_cmd_state_output_init(&state, RZ_OUTPUT_MODE_STANDARD)) {
			return RZ_CMD_STATUS_INVALID;
		}
		rz_core_flag_describe(core, core->offset, false, &state);
		rz_cmd_state_output_fini(&state);
		int maxcount = 9;
		int rows, cols = rz_cons_get_size(&rows);
		count = 0;
		char *dis = NULL;
		rows -= 4;
		idx = 0;
		ut64 curat = UT64_MAX;
		rz_list_foreach (xrefs, iter, xrefi) {
			ut64 xaddr1 = xref_to ? xrefi->from : xrefi->to;
			ut64 xaddr2 = xref_to ? xrefi->to : xrefi->from;
			if (idx - skip > maxcount) {
				rz_cons_printf("...");
				break;
			}
			if (!iter->n && idx < skip) {
				skip = idx;
			}
			if (idx >= skip) {
				if (count > maxcount) {
					strcpy(cstr, "?");
				} else {
					snprintf(cstr, sizeof(cstr), "%d", count);
				}
				if (idx == skip) {
					cur_ref_addr = xaddr1;
				}
				RzAnalysisFunction *fun = rz_analysis_get_fcn_in(core->analysis, xaddr1, RZ_ANALYSIS_FCN_TYPE_NULL);
				char *name;
				if (fun) {
					name = strdup(fun->name);
				} else {
					RzFlagItem *f = rz_flag_get_at(core->flags, xaddr1, true);
					if (f) {
						name = rz_str_newf("%s + %" PFMT64d, f->name, xaddr1 - f->offset);
					} else {
						name = strdup("unk");
					}
				}
				if (w > 45) {
					if (strlen(name) > w - 45) {
						name[w - 45] = 0;
					}
				} else {
					name[0] = 0;
				}

				const char *cmt = rz_meta_get_string(core->analysis, RZ_META_TYPE_COMMENT, xaddr1);
				if (cmt) {
					cmt = rz_str_trim_head_ro(cmt);
					rz_cons_printf(" %d [%s] 0x%08" PFMT64x " 0x%08" PFMT64x " %s %sref (%s) ; %s\n",
						idx, cstr, xaddr2, xaddr1,
						rz_analysis_xrefs_type_tostring(xrefi->type),
						xref_to ? "x" : "", name, cmt);
				}
				free(name);
				if (idx == skip) {
					free(dis);
					curat = xaddr1;
					char *res = rz_core_cmd_strf(core, "pd 4 @ 0x%08" PFMT64x "@e:asm.flags.limit=1", xaddr2);
					// TODO: show disasm with context. not seek addr
					// dis = rz_core_cmd_strf (core, "pd $r-4 @ 0x%08"PFMT64x, xaddr);
					dis = NULL;
					res = rz_str_appendf(res, "; ---------------------------\n");
					switch (visual->printMode) {
					case 0:
						dis = rz_core_cmd_strf(core, "pd $r-4 @ 0x%08" PFMT64x, xaddr1);
						break;
					case 1:
						dis = rz_core_cmd_strf(core, "pd @ 0x%08" PFMT64x "-32", xaddr1);
						break;
					case 2:
						dis = rz_core_cmd_strf(core, "px @ 0x%08" PFMT64x, xaddr1);
						break;
					case 3:
						dis = rz_core_cmd_strf(core, "pds @ 0x%08" PFMT64x, xaddr1);
						break;
					}
					if (dis) {
						res = rz_str_append(res, dis);
						free(dis);
					}
					dis = res;
				}
				if (++count >= rows) {
					rz_cons_printf("...");
					break;
				}
			}
			idx++;
		}
		if (dis) {
			if (count < rows) {
				rz_cons_newline();
			}
			int i = count;
			for (; i < 9; i++) {
				rz_cons_newline();
			}
			/* prepare highlight */
			char *cmd = strdup(rz_config_get(core->config, "scr.highlight"));
			char *ats = rz_str_newf("%" PFMT64x, curat);
			if (ats && !*cmd) {
				(void)rz_config_set(core->config, "scr.highlight", ats);
			}
			/* print disasm */
			char *d = rz_str_ansi_crop(dis, 0, 0, cols, rows - 9);
			if (d) {
				rz_cons_printf("%s", d);
				free(d);
			}
			/* flush and restore highlight */
			rz_cons_flush();
			rz_config_set(core->config, "scr.highlight", cmd);
			free(ats);
			free(cmd);
			free(dis);
			dis = NULL;
		}
		rz_config_set_b(core->config, "asm.bytes", asm_bytes);
	}
	rz_cons_flush();
	rz_cons_enable_mouse(rz_config_get_b(core->config, "scr.wheel"));
	ch = rz_cons_readchar();
	ch = rz_cons_arrow_to_hjkl(ch);
	if (ch == ':') {
		rz_core_visual_prompt_input(core);
		goto repeat;
	} else if (ch == '?') {
		rz_cons_clear00();
		rz_cons_printf("Usage: Visual Xrefs\n"
			       " jk  - select next or previous item (use arrows)\n"
			       " JK  - step 10 rows\n"
			       " pP  - rotate between various print modes\n"
			       " :   - run rizin command\n"
			       " /   - highlight given word\n"
			       " ?   - show this help message\n"
			       " <>  - '<' for xrefs and '>' for refs\n"
			       " TAB - toggle between address and function references\n"
			       " xX  - switch to refs or xrefs\n"
			       " q   - quit this view\n"
			       " \\n  - seek to this xref");
		rz_cons_flush();
		rz_cons_any_key(NULL);
		goto repeat;
	} else if (ch == 9) { // TAB
		xrefsMode = !xrefsMode;
		rz_core_visual_toggle_decompiler_disasm(core, false, true);
		goto repeat;
	} else if (ch == 'p') {
		rz_core_visual_toggle_decompiler_disasm(core, false, true);
		visual->printMode++;
		if (visual->printMode > lastPrintMode) {
			visual->printMode = 0;
		}
		goto repeat;
	} else if (ch == 'P') {
		rz_core_visual_toggle_decompiler_disasm(core, false, true);
		visual->printMode--;
		if (visual->printMode < 0) {
			visual->printMode = lastPrintMode;
		}
		goto repeat;
	} else if (ch == '/') {
		rz_core_cmd0(core, "?i highlight;e scr.highlight=`yp`");
		goto repeat;
	} else if (ch == 'x' || ch == '<') {
		xref_to = true;
		xrefsMode = !xrefsMode;
		goto repeat;
	} else if (ch == 'X' || ch == '>') {
		xref_to = false;
		xrefsMode = !xrefsMode;
		goto repeat;
	} else if (ch == 'J') {
		skip += 10;
		goto repeat;
	} else if (ch == 'g') {
		skip = 0;
		goto repeat;
	} else if (ch == 'G') {
		skip = 9999;
		goto repeat;
	} else if (ch == ';') {
		add_comment(core, cur_ref_addr, "\nEnter comment for reference:\n");
		goto repeat;
	} else if (ch == '.') {
		skip = 0;
		goto repeat;
	} else if (ch == 'j') {
		skip++;
		goto repeat;
	} else if (ch == 'K') {
		skip = (skip < 10) ? 0 : skip - 10;
		goto repeat;
	} else if (ch == 'k') {
		skip--;
		if (skip < 0) {
			skip = 0;
		}
		goto repeat;
	} else if (ch == ' ' || ch == '\n' || ch == '\r' || ch == 'l') {
		ret = follow_ref(core, xrefs, skip, xref_to);
	} else if (IS_DIGIT(ch)) {
		ret = follow_ref(core, xrefs, ch - 0x30, xref_to);
	} else if (ch != 'q' && ch != 'Q' && ch != 'h') {
		goto repeat;
	}
	rz_list_free(xrefs);

	return ret;
}

#if __WINDOWS__
void SetWindow(int Width, int Height) {
	COORD coord;
	coord.X = Width;
	coord.Y = Height;

	SMALL_RECT Rect;
	Rect.Top = 0;
	Rect.Left = 0;
	Rect.Bottom = Height - 1;
	Rect.Right = Width - 1;

	HANDLE Handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleScreenBufferSize(Handle, coord);
	SetConsoleWindowInfo(Handle, TRUE, &Rect);
}
#endif

// unnecesarily public
char *getcommapath(RzCore *core) {
	char *cwd;
	const char *dir = rz_config_get(core->config, "dir.projects");
	const char *prj = rz_config_get(core->config, "prj.name");
	if (dir && *dir && prj && *prj) {
		char *abspath = rz_file_abspath(dir);
		/* use prjdir as base directory for comma-ent files */
		cwd = rz_str_newf("%s" RZ_SYS_DIR "%s.d", abspath, prj);
		free(abspath);
	} else {
		/* use cwd as base directory for comma-ent files */
		cwd = rz_sys_getdir();
	}
	return cwd;
}

static void visual_comma(RzCore *core) {
	bool mouse_state = __holdMouseState(core);
	ut64 addr = core->offset + (core->print->cur_enabled ? core->print->cur : 0);
	char *comment, *cwd, *cmtfile;
	const char *prev_cmt = rz_meta_get_string(core->analysis, RZ_META_TYPE_COMMENT, addr);
	comment = prev_cmt ? strdup(prev_cmt) : NULL;
	cmtfile = rz_str_between(comment, ",(", ")");
	cwd = getcommapath(core);
	if (!cmtfile) {
		char *fn;
		fn = rz_cons_input("<comment-file> ");
		if (fn && *fn) {
			cmtfile = strdup(fn);
			if (!comment || !*comment) {
				comment = rz_str_newf(",(%s)", fn);
				rz_meta_set_string(core->analysis, RZ_META_TYPE_COMMENT, addr, comment);
			} else {
				// append filename in current comment
				char *nc = rz_str_newf("%s ,(%s)", comment, fn);
				rz_meta_set_string(core->analysis, RZ_META_TYPE_COMMENT, addr, nc);
				free(nc);
			}
		}
		free(fn);
	}
	if (cmtfile) {
		char *cwf = rz_str_newf("%s" RZ_SYS_DIR "%s", cwd, cmtfile);
		char *odata = rz_file_slurp(cwf, NULL);
		if (!odata) {
			RZ_LOG_ERROR("core: Could not open '%s'.\n", cwf);
			free(cwf);
			goto beach;
		}
		char *data = rz_core_editor(core, NULL, odata);
		rz_file_dump(cwf, (const ut8 *)data, -1, 0);
		free(data);
		free(odata);
		free(cwf);
	} else {
		RZ_LOG_ERROR("core: No commafile found.\n");
	}
beach:
	free(comment);
	rz_cons_enable_mouse(mouse_state && rz_config_get_b(core->config, "scr.wheel"));
}

static bool isDisasmPrint(int mode) {
	return (mode == RZ_CORE_VISUAL_MODE_PD || mode == RZ_CORE_VISUAL_MODE_DB);
}

static void cursor_ocur(RzCore *core, bool use_ocur) {
	RzPrint *p = core->print;
	if (use_ocur && p->ocur == -1) {
		p->ocur = p->cur;
	} else if (!use_ocur) {
		p->ocur = -1;
	}
}

static void nextOpcode(RzCore *core) {
	RzAnalysisOp *aop = rz_core_analysis_op(core, core->offset + core->print->cur, RZ_ANALYSIS_OP_MASK_BASIC);
	RzPrint *p = core->print;
	if (aop) {
		p->cur += aop->size;
		rz_analysis_op_free(aop);
	} else {
		p->cur += 4;
	}
}

static void prevOpcode(RzCore *core) {
	RzPrint *p = core->print;
	ut64 addr, oaddr = core->offset + core->print->cur;
	if (rz_core_prevop_addr(core, oaddr, 1, &addr)) {
		const int delta = oaddr - addr;
		p->cur -= delta;
	} else {
		p->cur -= 4;
	}
}

static void cursor_nextrow(RzCore *core, bool use_ocur) {
	RzCoreVisual *visual = core->visual;
	RzPrint *p = core->print;
	ut32 roff, next_roff;
	int row, sz, delta;
	RzAsmOp op;

	cursor_ocur(core, use_ocur);
	if (PIDX == 1) { // DISASM
		nextOpcode(core);
		return;
	}

	if (PIDX == 7 || !strcmp("prc", rz_config_get(core->config, "cmd.visual"))) {
		p->cur += rz_config_get_i(core->config, "hex.cols");
		return;
	}
	if (visual->splitView) {
		int w = rz_config_get_i(core->config, "hex.cols");
		if (w < 1) {
			w = 16;
		}
		if (core->seltab == 0) {
			visual->splitPtr += w;
		} else {
			core->offset += w;
		}
		return;
	}
	if (PIDX == RZ_CORE_VISUAL_MODE_DB) {
		const int cols = core->dbg->regcols;
		int w = rz_config_get_i(core->config, "hex.cols");
		switch (core->seltab) {
		case 0:
			if (w < 1) {
				w = 16;
			}
			rz_config_set_i(core->config, "stack.delta",
				rz_config_get_i(core->config, "stack.delta") - w);
			return;
		case 1:
			p->cur += cols > 0 ? cols : 3;
			return;
		default:
			nextOpcode(core);
			return;
		}
	}
	if (p->row_offsets) {
		// FIXME: cache the current row
		row = rz_print_row_at_off(p, p->cur);
		roff = rz_print_rowoff(p, row);
		if (roff == -1) {
			p->cur++;
			return;
		}
		next_roff = rz_print_rowoff(p, row + 1);
		if (next_roff == UT32_MAX) {
			p->cur++;
			return;
		}
		if (next_roff > core->blocksize) {
			p->cur += 32; // XXX workaround to "fix" cursor nextrow far away scrolling issue
			return;
		}
		if (next_roff + 32 < core->blocksize) {
			sz = rz_asm_disassemble(core->rasm, &op,
				core->block + next_roff, 32);
			if (sz < 1) {
				sz = 1;
			}
		} else {
			sz = 1;
		}
		delta = p->cur - roff;
		p->cur = next_roff + RZ_MIN(delta, sz - 1);
	} else {
		p->cur += RZ_MAX(1, p->cols);
	}
}

static void cursor_prevrow(RzCore *core, bool use_ocur) {
	RzCoreVisual *visual = core->visual;
	RzPrint *p = core->print;
	ut32 roff, prev_roff;
	int row;

	cursor_ocur(core, use_ocur);
	if (PIDX == 1) { // DISASM
		prevOpcode(core);
		return;
	}

	if (PIDX == 7 || !strcmp("prc", rz_config_get(core->config, "cmd.visual"))) {
		int cols = rz_config_get_i(core->config, "hex.cols");
		p->cur -= RZ_MAX(cols, 0);
		return;
	}

	if (visual->splitView) {
		int w = rz_config_get_i(core->config, "hex.cols");
		if (w < 1) {
			w = 16;
		}
		if (core->seltab == 0) {
			visual->splitPtr -= w;
		} else {
			core->offset -= w;
		}
		return;
	}
	if (PIDX == RZ_CORE_VISUAL_MODE_DB) {
		switch (core->seltab) {
		case 0: {
			int w = rz_config_get_i(core->config, "hex.cols");
			if (w < 1) {
				w = 16;
			}
			rz_config_set_i(core->config, "stack.delta",
				rz_config_get_i(core->config, "stack.delta") + w);
		}
			return;
		case 1: {
			const int cols = core->dbg->regcols;
			p->cur -= cols > 0 ? cols : 4;
			return;
		}
		default:
			prevOpcode(core);
			return;
		}
	}
	if (p->row_offsets) {
		int delta, prev_sz;

		// FIXME: cache the current row
		row = rz_print_row_at_off(p, p->cur);
		roff = rz_print_rowoff(p, row);
		if (roff == UT32_MAX) {
			p->cur--;
			return;
		}
		prev_roff = row > 0 ? rz_print_rowoff(p, row - 1) : UT32_MAX;
		delta = p->cur - roff;
		if (prev_roff == UT32_MAX) {
			ut64 prev_addr = rz_core_prevop_addr_heuristic(core, core->offset + roff);
			if (prev_addr > core->offset) {
				prev_roff = 0;
				prev_sz = 1;
			} else {
				RzAsmOp op;
				prev_roff = 0;
				rz_core_seek(core, prev_addr, true);
				prev_sz = rz_asm_disassemble(core->rasm, &op,
					core->block, 32);
			}
		} else {
			prev_sz = roff - prev_roff;
		}
		int res = RZ_MIN(delta, prev_sz - 1);
		ut64 cur = prev_roff + res;
		if (cur == p->cur) {
			if (p->cur > 0) {
				p->cur--;
			}
		} else {
			p->cur = prev_roff + delta; // res;
		}
	} else {
		p->cur -= p->cols;
	}
}

static void cursor_left(RzCore *core, bool use_ocur) {
	if (PIDX == 2) {
		if (core->seltab == 1) {
			core->print->cur--;
			return;
		}
	}
	cursor_ocur(core, use_ocur);
	core->print->cur--;
}

static void cursor_right(RzCore *core, bool use_ocur) {
	if (PIDX == 2) {
		if (core->seltab == 1) {
			core->print->cur++;
			return;
		}
	}
	cursor_ocur(core, use_ocur);
	core->print->cur++;
}

static bool fix_cursor(RzCore *core) {
	RzPrint *p = core->print;
	RzCoreVisual *visual = core->visual;
	int offscreen = (core->cons->rows - 3) * p->cols;
	bool res = false;

	if (!core->print->cur_enabled) {
		return false;
	}
	if (core->print->screen_bounds > 1) {
		bool off_is_visible = core->offset < core->print->screen_bounds;
		bool cur_is_visible = core->offset + p->cur < core->print->screen_bounds;
		bool is_close = core->offset + p->cur < core->print->screen_bounds + 32;

		if ((!cur_is_visible && !is_close) || (!cur_is_visible && p->cur == 0)) {
			// when the cursor is not visible and it's far from the
			// last visible byte, just seek there.
			rz_core_seek_delta(core, p->cur, false);
			reset_print_cur(p);
		} else if ((!cur_is_visible && is_close) || !off_is_visible) {
			RzAsmOp op;
			int sz = rz_asm_disassemble(core->rasm,
				&op, core->block, 32);
			if (sz < 1) {
				sz = 1;
			}
			rz_core_seek_delta(core, sz, false);
			p->cur = RZ_MAX(p->cur - sz, 0);
			if (p->ocur != -1) {
				p->ocur = RZ_MAX(p->ocur - sz, 0);
			}
			res |= off_is_visible;
		}
	} else if (core->print->cur >= offscreen) {
		rz_core_seek(core, core->offset + p->cols, true);
		p->cur -= p->cols;
		if (p->ocur != -1) {
			p->ocur -= p->cols;
		}
	}

	if (p->cur < 0) {
		int sz = p->cols;
		if (isDisasmPrint(visual->printidx)) {
			sz = rz_core_visual_prevopsz(core, core->offset + p->cur);
			if (sz < 1) {
				sz = 1;
			}
		}
		rz_core_seek_delta(core, -sz, false);
		p->cur += sz;
		if (p->ocur != -1) {
			p->ocur += sz;
		}
	}
	return res;
}

static bool insert_mode_enabled(RzCore *core) {
	RzCoreVisual *visual = core->visual;
	if (!visual->insertMode) {
		return false;
	}
	char ch = (ut8)rz_cons_readchar();
	if ((ut8)ch == KEY_ALTQ) {
		(void)rz_cons_readchar();
		visual->insertMode = false;
		return true;
	}
	char arrows = rz_cons_arrow_to_hjkl(ch);
	switch (ch) {
	case 127:
		core->print->cur = RZ_MAX(0, core->print->cur - 1);
		return true;
	case 9: // tab "tab" TAB
		core->print->col = core->print->col == 1 ? 2 : 1;
		break;
	}
	if (ch != 'h' && arrows == 'h') {
		core->print->cur = RZ_MAX(0, core->print->cur - 1);
		return true;
	} else if (ch != 'l' && arrows == 'l') {
		core->print->cur = core->print->cur + 1;
		return true;
	} else if (ch != 'j' && arrows == 'j') {
		cursor_nextrow(core, false);
		return true;
	} else if (ch != 'k' && arrows == 'k') {
		cursor_prevrow(core, false);
		return true;
	}
	if (core->print->col == 2) {
		/* ascii column */
		if (IS_PRINTABLE(ch)) {
			ut8 chs[2] = { ch, 0 };
			if (rz_core_write_at(core, core->offset + core->print->cur, chs, 1)) {
				core->print->cur++;
			}
		}
		return true;
	}
	ch = arrows;
	/* hex column */
	switch (ch) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
		if (visual->insertNibble != -1) {
			char hexpair[3] = { visual->insertNibble, ch, 0 };
			rz_core_write_hexpair(core, core->offset + core->print->cur, hexpair);
			core->print->cur++;
			visual->insertNibble = -1;
		} else {
			visual->insertNibble = ch;
		}
		break;
	case 'r': // "r -1"
		rz_core_file_resize_delta(core, -1);
		break;
	case 'R': // "r +1"
		rz_core_file_resize_delta(core, +1);
		break;
	case 'h':
		core->print->cur = RZ_MAX(0, core->print->cur - 1);
		break;
	case 'l':
		core->print->cur = core->print->cur + 1;
		break;
	case 'j':
		cursor_nextrow(core, false);
		break;
	case 'k':
		cursor_prevrow(core, false);
		break;
	case 'Q':
	case 'q':
		visual->insertMode = false;
		break;
	case '?':
		rz_cons_less_str("\nVisual Insert Mode:\n\n"
				 " tab          - toggle between ascii and hex columns\n"
				 " q (or alt-q) - quit insert mode\n"
				 "\nHex column:\n"
				 " r            - remove byte in cursor\n"
				 " R            - insert byte in cursor\n"
				 " [0-9a-f]     - insert hexpairs in hex column\n"
				 " hjkl         - move around\n"
				 "\nAscii column:\n"
				 " arrows       - move around\n"
				 " alt-q        - quit insert mode\n",
			"?");
		break;
	}
	return true;
}

RZ_IPI void rz_core_visual_browse(RzCore *core, const char *input) {
	const char *browsemsg =
		"Browse stuff:\n"
		"-------------\n"
		" _  hud mode (V_)\n"
		" 1  bit editor (vd1)\n"
		" b  blocks\n"
		" a  analysis classes\n"
		" c  classes\n"
		" C  comments\n"
		" d  debug traces\n"
		" e  eval var configurations\n"
		" E  esil debugger mode\n"
		" f  flags\n"
		" F  functions\n"
		" g  graph\n"
		" h  history\n"
		" i  imports\n"
		" m  maps\n"
		" p  pids/threads\n"
		" q  quit\n"
		" r  ROP gadgets\n"
		" s  symbols\n"
		" T  themes\n"
		" v  vars\n"
		" x  xrefs\n"
		" X  refs\n"
		" :  run command\n";
	for (;;) {
		rz_cons_clear00();
		rz_cons_printf("%s\n", browsemsg);
		rz_cons_flush();
		char ch = 0;
		if (input && *input) {
			ch = *input;
			input++;
		} else {
			ch = rz_cons_readchar();
		}
		ch = rz_cons_arrow_to_hjkl(ch);
		switch (ch) {
		case '1':
			rz_core_visual_bit_editor(core);
			break;
		case 'g': // "vbg"
			if (rz_core_visual_view_graph(core)) {
				return;
			}
			break;
		case 'r': // "vbr"
			rz_core_visual_view_rop(core);
			break;
		case 'f': // "vbf"
			rz_core_visual_trackflags(core);
			break;
		case 'F': // "vbF"
			rz_core_visual_analysis(core, NULL);
			break;
		case 'd': // "vbd"
			rz_core_visual_debugtraces(core, NULL);
			break;
		case 'v': // "vbv"
			rz_core_visual_analysis(core, "v");
			break;
		case 'e': // "vbe"
			rz_core_visual_config(core);
			break;
		case 'E': // "vbE"
			rz_core_visual_esil(core);
			break;
		case 'c': // "vbc"
			rz_core_visual_classes(core);
			break;
		case 'a': // "vba"
			rz_core_visual_analysis_classes(core);
			break;
		case 'C': // "vbC"
			rz_core_visual_comments(core);
			break;
		case 'T': // "vbT"
			rz_core_cmd0(core, "eco $(eco~...)");
			break;
		case 'p':
			rz_core_cmd0(core, "dpt=$(dpt~[1-])");
			break;
		case 'b':
			rz_core_cmd0(core, "s $(afb~...)");
			break;
		case 'i':
			// XXX ii shows index first and iiq shows no offset :(
			rz_core_cmd0(core, "s $(ii~...)");
			break;
		case 's':
			rz_core_cmd0(core, "s $(isq~...)");
			break;
		case 'm':
			rz_core_cmd0(core, "s $(dm~...)");
			break;
		case 'x':
			rz_core_visual_xrefs(core, true, true);
			break;
		case 'X':
			rz_core_visual_xrefs(core, false, true);
			break;
		case 'h': // seek history
			rz_core_cmdf(core, "sh~...");
			break;
		case '_':
			rz_core_visual_hudstuff(core);
			break;
		case ':':
			rz_core_visual_prompt_input(core);
			break;
		case 127: // backspace
		case 'q':
			return;
		}
	}
}

static bool isNumber(RzCore *core, int ch) {
	if (ch > '0' && ch <= '9') {
		return true;
	}
	if (core->print->cur_enabled) {
		return ch == '0';
	}
	return false;
}

static char numbuf[32] = { 0 };
static int numbuf_i = 0;

static void numbuf_append(int ch) {
	if (numbuf_i >= sizeof(numbuf) - 1) {
		numbuf_i = 0;
	}
	numbuf[numbuf_i++] = ch;
	numbuf[numbuf_i] = 0;
}

static int numbuf_pull(void) {
	int distance = 1;
	if (numbuf_i) {
		numbuf[numbuf_i] = 0;
		distance = atoi(numbuf);
		if (!distance) {
			distance = 1;
		}
		numbuf_i = 0;
	}
	return distance;
}

static bool canWrite(RzCore *core, ut64 addr) {
	if (rz_config_get_b(core->config, "io.cache")) {
		return true;
	}
	RzIOMap *map = rz_io_map_get(core->io, addr);
	return (map && (map->perm & RZ_PERM_W));
}

RZ_IPI int rz_core_visual_cmd(RzCore *core, const char *arg) {
	ut8 och = arg[0];
	RzAsmOp op;
	ut64 offset = core->offset;
	RzCoreVisual *visual = core->visual;
	char buf[4096];
	const char *key_s;
	int i, cols = core->print->cols;
	int wheelspeed;
	int ch = och;
	if ((ut8)ch == KEY_ALTQ) {
		rz_cons_readchar();
		ch = 'q';
	}
	ch = rz_cons_arrow_to_hjkl(ch);
	ch = visual_nkey(core, ch);
	if (ch < 2) {
		int x, y;
		if (rz_cons_get_click(&x, &y)) {
			if (y == 1) {
				if (x < 15) {
					ch = '_';
				} else if (x < 20) {
					ch = 'p';
				} else if (x < 24) {
					ch = 9;
				}
			} else if (y == 2) {
				if (x < 2) {
					rz_core_visual_closetab(core);
				} else if (x < 5) {
					rz_core_visual_newtab(core);
				} else {
					rz_core_visual_nexttab(core);
				}
				return 1;
			} else {
				ch = 0; //'c';
			}
		} else {
			return 1;
		}
	}
	if (rz_cons_singleton()->mouse_event) {
		wheelspeed = rz_config_get_i(core->config, "scr.wheel.speed");
	} else {
		wheelspeed = 1;
	}

	if (ch == 'l' && och == 6) {
		ch = 'J';
	} else if (ch == 'h' && och == 2) {
		ch = 'K';
	}

	// do we need hotkeys for data references? not only calls?
	// '0' is handled to seek at the beginning of the function
	// unless the cursor is set, then, the 0 is captured here
	if (isNumber(core, ch)) {
		// only in disasm and debug prints..
		if (isDisasmPrint(visual->printidx)) {
			if (rz_config_get_b(core->config, "asm.hints") && (rz_config_get_b(core->config, "asm.hint.jmp") || rz_config_get_b(core->config, "asm.hint.lea") || rz_config_get_b(core->config, "asm.hint.emu") || rz_config_get_b(core->config, "asm.hint.call"))) {
				rz_core_visual_jump(core, ch);
			} else {
				numbuf_append(ch);
			}
		} else {
			numbuf_append(ch);
		}
	} else {
		switch (ch) {
		case 0x0d: // "enter" "\\n" "newline"
		{
			RzAnalysisOp *op;
			bool wheel = rz_config_get_b(core->config, "scr.wheel");
			if (wheel) {
				rz_cons_enable_mouse(true);
			}
			do {
				op = rz_core_analysis_op(core, core->offset + core->print->cur, RZ_ANALYSIS_OP_MASK_BASIC);
				if (op) {
					if (op->type == RZ_ANALYSIS_OP_TYPE_JMP ||
						op->type == RZ_ANALYSIS_OP_TYPE_CJMP ||
						op->type == RZ_ANALYSIS_OP_TYPE_CALL ||
						op->type == RZ_ANALYSIS_OP_TYPE_CCALL) {
						if (core->print->cur_enabled) {
							int delta = RZ_ABS((st64)op->jump - (st64)offset);
							if (op->jump < core->offset || op->jump >= core->print->screen_bounds) {
								rz_core_visual_seek_animation(core, op->jump);
								core->print->cur = 0;
							} else {
								core->print->cur = delta;
							}
						} else {
							rz_core_visual_seek_animation(core, op->jump);
						}
					}
				}
				rz_analysis_op_free(op);
			} while (--wheelspeed > 0);
		} break;
		case 'o': // tab TAB
			nextPrintFormat(core);
			break;
		case 'O': // tab TAB
		case 9: // tab TAB
			rz_core_visual_toggle_decompiler_disasm(core, false, true);
			if (visual->splitView) {
				// this split view is kind of useless imho, we should kill it or merge it into tabs
				core->print->cur = 0;
				core->curtab = 0;
				core->seltab++;
				if (core->seltab > 1) {
					core->seltab = 0;
				}
			} else {
				if (core->print->cur_enabled) {
					core->curtab = 0;
					if (visual->printidx == RZ_CORE_VISUAL_MODE_DB) {
						core->print->cur = 0;
						core->seltab++;
						if (core->seltab > 2) {
							core->seltab = 0;
						}
					} else {
						core->seltab = 0;
						ut64 f = rz_config_get_i(core->config, "diff.from");
						ut64 t = rz_config_get_i(core->config, "diff.to");
						if (f == t && f == 0) {
							core->print->col = core->print->col == 1 ? 2 : 1;
						}
					}
				} else {
					prevPrintFormat(core);
				}
			}
			break;
		case '&':
			rotateAsmBits(core);
			break;
		case 'a': {
			{
				ut64 addr = core->offset;
				if (PIDX == 2) {
					if (core->seltab == 0) {
						addr = rz_debug_reg_get(core->dbg, "SP");
					}
				}
				if (!canWrite(core, addr)) {
					rz_cons_printf("\nFile has been opened in read-only mode. Use -w flag, oo+ or e io.cache=true\n");
					rz_cons_any_key(NULL);
					return true;
				}
			}
			rz_cons_printf("Enter assembler opcodes separated with ';':\n");
			rz_core_visual_showcursor(core, true);
			rz_cons_flush();
			rz_cons_set_raw(false);
			strcpy(buf, "\"wa ");
			rz_line_set_prompt(":> ");
			rz_cons_enable_mouse(false);
			if (rz_cons_fgets(buf + 4, sizeof(buf) - 4, 0, NULL) < 0) {
				buf[0] = '\0';
			}
			strcat(buf, "\"");
			bool wheel = rz_config_get_b(core->config, "scr.wheel");
			if (wheel) {
				rz_cons_enable_mouse(true);
			}
			if (*buf) {
				if (core->print->cur_enabled) {
					int t = core->offset + core->print->cur;
					rz_core_seek(core, t, false);
				}
				rz_core_cmd(core, buf, true);
				if (core->print->cur_enabled) {
					int t = core->offset - core->print->cur;
					rz_core_seek(core, t, true);
				}
			}
			rz_core_visual_showcursor(core, false);
			rz_cons_set_raw(true);
		} break;
		case '=': { // TODO: edit
			rz_core_visual_showcursor(core, true);
			const char *buf = NULL;
#define I core->cons
			const char *cmd = rz_config_get(core->config, "cmd.vprompt");
			rz_line_set_prompt("cmd.vprompt> ");
			I->line->contents = strdup(cmd);
			buf = rz_line_readline();
			I->line->contents = NULL;
			(void)rz_config_set(core->config, "cmd.vprompt", buf);
			rz_core_visual_showcursor(core, false);
		} break;
		case '|': { // TODO: edit
			rz_core_visual_showcursor(core, true);
			const char *buf = NULL;
#define I core->cons
			const char *cmd = rz_config_get(core->config, "cmd.cprompt");
			rz_line_set_prompt("cmd.cprompt> ");
			I->line->contents = strdup(cmd);
			buf = rz_line_readline();
			if (buf && !strcmp(buf, "|")) {
				RZ_FREE(I->line->contents);
				core->print->cur_enabled = true;
				core->print->cur = 0;
				(void)rz_config_set(core->config, "cmd.cprompt", "p=e $r-2");
			} else {
				RZ_FREE(I->line->contents);
				(void)rz_config_set(core->config, "cmd.cprompt", buf ? buf : "");
			}
			rz_core_visual_showcursor(core, false);
		} break;
		case '!':
			rz_core_visual_panels_root(core, core->panels_root);
			break;
		case 'g':
			rz_core_visual_showcursor(core, true);
			rz_core_visual_offset(core);
			rz_core_visual_showcursor(core, false);
			break;
		case 'G':
			__core_visual_gogo(core, 'G');
			break;
		case 'A': {
			const int oce = core->print->cur_enabled;
			const int oco = core->print->ocur;
			const int occ = core->print->cur;
			ut64 off = oce ? core->offset + core->print->cur : core->offset;
			core->print->cur_enabled = 0;
			rz_cons_enable_mouse(false);
			rz_core_visual_asm(core, off);
			core->print->cur_enabled = oce;
			core->print->cur = occ;
			core->print->ocur = oco;
			if (rz_config_get_b(core->config, "scr.wheel")) {
				rz_cons_enable_mouse(true);
			}
		} break;
		case '\\':
			if (visual->splitPtr == UT64_MAX) {
				visual->splitPtr = core->offset;
			}
			visual->splitView = !visual->splitView;
			setcursor(core, visual->splitView);
			break;
		case 'c':
			setcursor(core, !core->print->cur_enabled);
			break;
		case '$':
			if (core->print->cur_enabled) {
				rz_core_reg_set_by_role_or_name(core, "PC", core->offset + core->print->cur);
			} else {
				rz_core_reg_set_by_role_or_name(core, "PC", core->offset);
			}
			break;
		case '@':
			if (core->print->cur_enabled) {
				char buf[128];
				prompt_read("cursor at:", buf, sizeof(buf));
				core->print->cur = (st64)rz_num_math(core->num, buf);
			}
			break;
		case 'C':
			if (++visual->color > 3) {
				visual->color = 0;
			}
			rz_config_set_i(core->config, "scr.color", visual->color);
			break;
		case 'd': {
			bool mouse_state = __holdMouseState(core);
			rz_core_visual_showcursor(core, true);
			int distance = numbuf_pull();
			rz_core_visual_define(core, arg + 1, distance - 1);
			rz_core_visual_showcursor(core, false);
			rz_cons_enable_mouse(mouse_state && rz_config_get_b(core->config, "scr.wheel"));
		} break;
		case 'D':
			setdiff(core);
			break;
		case 'f': {
			bool mouse_state = __holdMouseState(core);
			int range, min, max;
			char name[256], *n;
			rz_line_set_prompt("flag name: ");
			rz_core_visual_showcursor(core, true);
			if (rz_cons_fgets(name, sizeof(name), 0, NULL) >= 0 && *name) {
				n = name;
				rz_str_trim(n);
				if (core->print->ocur != -1) {
					min = RZ_MIN(core->print->cur, core->print->ocur);
					max = RZ_MAX(core->print->cur, core->print->ocur);
				} else {
					min = max = core->print->cur;
				}
				range = max - min + 1;
				if (!strcmp(n, "-")) {
					rz_flag_unset_off(core->flags, core->offset + core->print->cur);
				} else if (*n == '.') {
					RzAnalysisFunction *fcn = rz_analysis_get_fcn_in(core->analysis, core->offset + min, 0);
					if (fcn) {
						if (n[1] == '-') {
							// Unset the local label (flag)
							rz_analysis_function_delete_label(fcn, n + 1);
						} else {
							rz_analysis_function_set_label(fcn, n + 1, core->offset + min);
						}
					} else {
						RZ_LOG_ERROR("core: Cannot find function at 0x%08" PFMT64x "\n", core->offset + min);
					}
				} else if (*n == '-') {
					if (*n) {
						rz_flag_unset_name(core->flags, n + 1);
					}
				} else {
					if (range < 1) {
						range = 1;
					}
					if (*n) {
						rz_flag_set(core->flags, n, core->offset + min, range);
					}
				}
			}
			rz_cons_enable_mouse(mouse_state && rz_config_get_b(core->config, "scr.wheel"));
			rz_core_visual_showcursor(core, false);
		} break;
		case ',':
			visual_comma(core);
			break;
		case 't': {
			rz_cons_gotoxy(0, 0);
			if (visual->tabs) {
				rz_cons_printf("[tnp:=+-] ");
			} else {
				rz_cons_printf("[t] ");
			}
			rz_cons_flush();
			int ch = rz_cons_readchar();
			if (isdigit(ch)) {
				rz_core_visual_nthtab(core, ch - '0' - 1);
			}
			switch (ch) {
			case 'h':
			case 'k':
			case 'p':
				rz_core_visual_prevtab(core);
				break;
			case 9: // t-TAB
			case 'l':
			case 'j':
			case 'n':
				rz_core_visual_nexttab(core);
				break;
			case '=':
				rz_core_visual_tabname_prompt(core);
				break;
			case '-':
				rz_core_visual_closetab(core);
				break;
			case ':': {
				RzCoreVisualTab *tab = rz_core_visual_newtab(core);
				if (tab) {
					tab->name[0] = ':';
					rz_cons_fgets(tab->name + 1, sizeof(tab->name) - 1, 0, NULL);
				}
			} break;
			case '+':
			case 't':
			case 'a':
				rz_core_visual_newtab(core);
				break;
			}
		} break;
		case 'T':
			rz_core_visual_closetab(core);
			break;
		case 'n':
			rz_core_seek_next(core, rz_config_get(core->config, "scr.nkey"), true);
			break;
		case 'N':
			rz_core_seek_prev(core, rz_config_get(core->config, "scr.nkey"), true);
			break;
		case 'i':
		case 'I': {
			ut64 oaddr = core->offset;
			int delta = (core->print->ocur != -1) ? RZ_MIN(core->print->cur, core->print->ocur) : core->print->cur;
			ut64 addr = core->offset + delta;
			if (PIDX == 0) {
				if (strstr(printfmtSingle[0], "pxb")) {
					rz_core_visual_define(core, "1", 1);
					return true;
				}
				if (core->print->ocur == -1) {
					visual->insertMode = true;
					core->print->cur_enabled = true;
					return true;
				}
			} else if (PIDX == 2) {
				if (core->seltab == 0) {
					addr = rz_debug_reg_get(core->dbg, "SP") + delta;
				} else if (core->seltab == 1) {
					// regs, writing in here not implemented
					return true;
				}
			}
			if (!canWrite(core, addr)) {
				rz_cons_printf("\nFile has been opened in read-only mode. Use -w flag, oo+ or e io.cache=true\n");
				rz_cons_any_key(NULL);
				return true;
			}
			rz_core_visual_showcursor(core, true);
			rz_cons_flush();
			rz_cons_set_raw(0);
			if (ch == 'I') {
				strcpy(buf, "wow ");
				rz_line_set_prompt("insert hexpair block: ");
				if (rz_cons_fgets(buf + 4, sizeof(buf) - 4, 0, NULL) < 0) {
					buf[0] = '\0';
				}
				char *p = strdup(buf);
				int cur = core->print->cur;
				if (cur >= core->blocksize) {
					cur = core->print->cur - 1;
				}
				snprintf(buf, sizeof(buf), "%s @ $$0!%i", p,
					core->blocksize - cur);
				rz_core_cmd(core, buf, 0);
				free(p);
				break;
			}
			if (core->print->col == 2) {
				strcpy(buf, "\"w ");
				rz_line_set_prompt("insert string: ");
				if (rz_cons_fgets(buf + 3, sizeof(buf) - 3, 0, NULL) < 0) {
					buf[0] = '\0';
				}
				strcat(buf, "\"");
			} else {
				rz_line_set_prompt("insert hex: ");
				if (core->print->ocur != -1) {
					int bs = RZ_ABS(core->print->cur - core->print->ocur) + 1;
					core->blocksize = bs;
					strcpy(buf, "wow ");
				} else {
					strcpy(buf, "wx ");
				}
				if (rz_cons_fgets(buf + strlen(buf), sizeof(buf) - strlen(buf), 0, NULL) < 0) {
					buf[0] = '\0';
				}
			}
			if (core->print->cur_enabled) {
				rz_core_seek(core, addr, false);
			}
			rz_core_cmd(core, buf, 1);
			if (core->print->cur_enabled) {
				rz_core_seek(core, addr, true);
			}
			rz_cons_set_raw(1);
			rz_core_visual_showcursor(core, false);
			rz_core_seek(core, oaddr, true);
		} break;
		case 'R':
			if (rz_config_get_b(core->config, "scr.randpal")) {
				rz_cons_pal_random();
			} else {
				rz_core_theme_nextpal(core, RZ_CONS_PAL_SEEK_NEXT);
			}
			break;
		case 'e':
			rz_core_visual_config(core);
			break;
		case '^': {
			RzAnalysisFunction *fcn = rz_analysis_get_fcn_in(core->analysis, core->offset, 0);
			if (fcn) {
				rz_core_seek_and_save(core, fcn->addr, false);
			} else {
				__core_visual_gogo(core, 'g');
			}
		} break;
		case 'E':
			rz_core_visual_colors(core);
			break;
		case 'x':
			rz_core_visual_xrefs(core, true, false);
			break;
		case 'X':
			rz_core_visual_xrefs(core, false, false);
			break;
		case 'r':
			// TODO: toggle shortcut hotkeys
			rz_core_visual_toggle_hints(core);
			visual_refresh(core);
			break;
		case ' ':
		case 'V': {
			RzAnalysisFunction *fun = rz_analysis_get_fcn_in(core->analysis, core->offset, RZ_ANALYSIS_FCN_TYPE_NULL);
			int ocolor = rz_config_get_i(core->config, "scr.color");
			if (!fun) {
				rz_cons_message("Not in a function. Type 'df' to define it here");
				break;
			} else if (rz_list_empty(fun->bbs)) {
				rz_cons_message("No basic blocks in this function. You may want to use 'afb+'.");
				break;
			}
			reset_print_cur(core->print);
			eprintf("\rRendering graph...");
			rz_core_visual_graph(core, NULL, NULL, true);
			rz_config_set_i(core->config, "scr.color", ocolor);
		} break;
		case 'v':
			rz_core_visual_analysis(core, NULL);
			break;
		case 'h':
		case 'l': {
			int distance = numbuf_pull();
			if (core->print->cur_enabled) {
				if (ch == 'h') {
					for (i = 0; i < distance; i++) {
						cursor_left(core, false);
					}
				} else {
					for (i = 0; i < distance; i++) {
						cursor_right(core, false);
					}
				}
			} else {
				if (ch == 'h') {
					distance = -distance;
				}
				rz_core_seek_delta(core, distance, false);
			}
		} break;
		case 'L':
		case 'H': {
			int distance = numbuf_pull();
			if (core->print->cur_enabled) {
				if (ch == 'H') {
					for (i = 0; i < distance; i++) {
						cursor_left(core, true);
					}
				} else {
					for (i = 0; i < distance; i++) {
						cursor_right(core, true);
					}
				}
			} else {
				if (ch == 'H') {
					distance = -distance;
				}
				rz_core_seek_delta(core, distance * 2, false);
			}
		} break;
		case 'j':
			if (core->print->cur_enabled) {
				int distance = numbuf_pull();
				for (i = 0; i < distance; i++) {
					cursor_nextrow(core, false);
				}
			} else {
				if (rz_config_get_b(core->config, "scr.wheel.nkey")) {
					int i, distance = numbuf_pull();
					if (distance < 1) {
						distance = 1;
					}
					for (i = 0; i < distance; i++) {
						const char *nkey = rz_config_get(core->config, "scr.nkey");
						rz_core_seek_next(core, nkey, true);
					}
				} else {
					int times = RZ_MAX(1, wheelspeed);
					// Check if we have a data annotation.
					ut64 amisize;
					RzAnalysisMetaItem *ami = rz_meta_get_at(core->analysis, core->offset, RZ_META_TYPE_DATA, &amisize);
					if (!ami) {
						ami = rz_meta_get_at(core->analysis, core->offset, RZ_META_TYPE_STRING, &amisize);
					}
					if (ami) {
						rz_core_seek_delta(core, amisize, false);
					} else {
						int distance = numbuf_pull();
						if (distance > 1) {
							times = distance;
						}
						while (times--) {
							if (isDisasmPrint(visual->printidx)) {
								rz_core_visual_disasm_down(core, &op, &cols);
							} else if (!strcmp(__core_visual_print_command(core),
									   "prc")) {
								cols = rz_config_get_i(core->config, "hex.cols");
							}
							rz_core_seek(core, core->offset + cols, true);
						}
					}
				}
			}
			break;
		case 'J':
			if (core->print->cur_enabled) {
				int distance = numbuf_pull();
				for (i = 0; i < distance; i++) {
					cursor_nextrow(core, true);
				}
			} else {
				if (core->print->screen_bounds > 1 && core->print->screen_bounds >= core->offset) {
					ut64 addr = UT64_MAX;
					if (isDisasmPrint(visual->printidx)) {
						if (core->print->screen_bounds == core->offset) {
							rz_asm_disassemble(core->rasm, &op, core->block, 32);
						}
						if (addr == core->offset || addr == UT64_MAX) {
							addr = core->offset + 48;
						}
					} else {
						int h;
						int hexCols = rz_config_get_i(core->config, "hex.cols");
						if (hexCols < 1) {
							hexCols = 16;
						}
						(void)rz_cons_get_size(&h);
						int delta = hexCols * (h / 4);
						addr = core->offset + delta;
					}
					rz_core_seek(core, addr, true);
				} else {
					rz_core_seek(core, core->offset + visual->obs, true);
				}
			}
			break;
		case 'k':
			if (core->print->cur_enabled) {
				int distance = numbuf_pull();
				for (i = 0; i < distance; i++) {
					cursor_prevrow(core, false);
				}
			} else {
				if (rz_config_get_b(core->config, "scr.wheel.nkey")) {
					int i, distance = numbuf_pull();
					if (distance < 1) {
						distance = 1;
					}
					for (i = 0; i < distance; i++) {
						rz_core_seek_prev(core, rz_config_get(core->config, "scr.nkey"), true);
					}
				} else {
					int times = wheelspeed;
					if (times < 1) {
						times = 1;
					}
					int distance = numbuf_pull();
					if (distance > 1) {
						times = distance;
					}
					while (times--) {
						if (isDisasmPrint(visual->printidx)) {
							rz_core_visual_disasm_up(core, &cols);
						} else if (!strcmp(__core_visual_print_command(core), "prc")) {
							cols = rz_config_get_i(core->config, "hex.cols");
						}
						rz_core_seek_delta(core, -cols, false);
					}
				}
			}
			break;
		case 'K':
			if (core->print->cur_enabled) {
				int distance = numbuf_pull();
				for (i = 0; i < distance; i++) {
					cursor_prevrow(core, true);
				}
			} else {
				if (core->print->screen_bounds > 1 && core->print->screen_bounds > core->offset) {
					int delta = (core->print->screen_bounds - core->offset);
					if (core->offset >= delta) {
						rz_core_seek(core, core->offset - delta, true);
					} else {
						rz_core_seek(core, 0, true);
					}
				} else {
					ut64 at = (core->offset > visual->obs) ? core->offset - visual->obs : 0;
					if (core->offset > visual->obs) {
						rz_core_seek(core, at, true);
					} else {
						rz_core_seek(core, 0, true);
					}
				}
			}
			break;
		case '[':
			// comments column
			if (core->print->cur_enabled &&
				(visual->printidx == RZ_CORE_VISUAL_MODE_PD ||
					(visual->printidx == RZ_CORE_VISUAL_MODE_DB && core->seltab == 2))) {
				int cmtcol = rz_config_get_i(core->config, "asm.cmt.col");
				if (cmtcol > 2) {
					rz_config_set_i(core->config, "asm.cmt.col", cmtcol - 2);
				}
			}
			// hex column
			if ((visual->printidx != RZ_CORE_VISUAL_MODE_PD && visual->printidx != RZ_CORE_VISUAL_MODE_DB) ||
				(visual->printidx == RZ_CORE_VISUAL_MODE_DB && core->seltab != 2)) {
				int scrcols = rz_config_get_i(core->config, "hex.cols");
				if (scrcols > 1) {
					rz_config_set_i(core->config, "hex.cols", scrcols - 1);
				}
			}
			break;
		case ']':
			// comments column
			if (core->print->cur_enabled &&
				(visual->printidx == RZ_CORE_VISUAL_MODE_PD ||
					(visual->printidx == RZ_CORE_VISUAL_MODE_DB && core->seltab == 2))) {
				int cmtcol = rz_config_get_i(core->config, "asm.cmt.col");
				rz_config_set_i(core->config, "asm.cmt.col", cmtcol + 2);
			}
			// hex column
			if ((visual->printidx != RZ_CORE_VISUAL_MODE_PD && visual->printidx != RZ_CORE_VISUAL_MODE_DB) ||
				(visual->printidx == RZ_CORE_VISUAL_MODE_DB && core->seltab != 2)) {
				int scrcols = rz_config_get_i(core->config, "hex.cols");
				rz_config_set_i(core->config, "hex.cols", scrcols + 1);
			}
			break;
		case 's':
			key_s = rz_config_get(core->config, "key.s");
			if (key_s && *key_s) {
				rz_core_cmd0(core, key_s);
			} else {
				rz_core_debug_single_step_in(core);
			}
			break;
		case 'S':
			key_s = rz_config_get(core->config, "key.S");
			if (key_s && *key_s) {
				rz_core_cmd0(core, key_s);
			} else {
				rz_core_debug_single_step_over(core);
			}
			break;
		case '"':
			rz_config_toggle(core->config, "scr.dumpcols");
			break;
		case 'p':
			rz_core_visual_toggle_decompiler_disasm(core, false, true);
			if (visual->printidx == RZ_CORE_VISUAL_MODE_DB && core->print->cur_enabled) {
				nextPrintCommand(core);
			} else {
				setprintmode(core, 1);
			}
			break;
		case 'P':
			if (visual->printidx == RZ_CORE_VISUAL_MODE_DB && core->print->cur_enabled) {
				prevPrintCommand(core);
			} else {
				setprintmode(core, -1);
			}
			break;
		case '%':
			if (core->print->cur_enabled) {
				findPair(core);
			} else {
				/* do nothing? */
				visual->autoblocksize = !visual->autoblocksize;
				if (visual->autoblocksize) {
					visual->obs = core->blocksize;
				} else {
					rz_core_block_size(core, visual->obs);
				}
				rz_cons_clear();
			}
			break;
		case 'w':
			findNextWord(core);
			break;
		case 'W':
			findPrevWord(core);
			break;
		case 'm': {
			rz_cons_gotoxy(0, 0);
			rz_cons_printf(RZ_CONS_CLEAR_LINE "Set shortcut key for 0x%" PFMT64x "\n", core->offset);
			rz_cons_flush();
			int ch = rz_cons_readchar();
			rz_core_visual_mark(core, ch);
		} break;
		case 'M': {
			rz_cons_gotoxy(0, 0);
			if (rz_core_visual_mark_dump(core)) {
				rz_cons_printf(RZ_CONS_CLEAR_LINE "Remove a shortcut key from the list\n");
				rz_cons_flush();
				int ch = rz_cons_readchar();
				rz_core_visual_mark_del(core, ch);
			}
		} break;
		case '\'': {
			rz_cons_gotoxy(0, 0);
			if (rz_core_visual_mark_dump(core)) {
				rz_cons_flush();
				int ch = rz_cons_readchar();
				rz_core_visual_mark_seek(core, ch);
			}
		} break;
		case 'y':
			if (core->print->ocur == -1) {
				rz_core_yank(core, core->offset + core->print->cur, 1);
			} else {
				rz_core_yank(core, core->offset + ((core->print->ocur < core->print->cur) ? core->print->ocur : core->print->cur), RZ_ABS(core->print->cur - core->print->ocur) + 1);
			}
			break;
		case 'Y':
			if (!core->yank_buf) {
				rz_cons_strcat("Cannot paste, clipboard is empty.\n");
				rz_cons_flush();
				rz_cons_any_key(NULL);
				rz_cons_clear00();
			} else {
				rz_core_yank_paste(core, core->offset + core->print->cur, 0);
			}
			break;
		case '0': {
			RzAnalysisFunction *fcn = rz_analysis_get_fcn_in(core->analysis, core->offset, RZ_ANALYSIS_FCN_TYPE_NULL);
			if (fcn) {
				rz_core_seek_and_save(core, fcn->addr, true);
			}
		} break;
		case '-':
			if (core->print->cur_enabled) {
				if (visual->printidx == RZ_CORE_VISUAL_MODE_DB) {
					if (!core->seltab) {
						// stack view
						int w = rz_config_get_i(core->config, "hex.cols");
						rz_config_set_i(core->config, "stack.size",
							rz_config_get_i(core->config, "stack.size") - w);
					}
				} else {
					if (core->print->ocur == -1) {
						sprintf(buf, "wos 01 @ $$+%i @!1", core->print->cur);
					} else {
						sprintf(buf, "wos 01 @ $$+%i @!%i", core->print->cur < core->print->ocur ? core->print->cur : core->print->ocur,
							RZ_ABS(core->print->ocur - core->print->cur) + 1);
					}
					rz_core_cmd(core, buf, 0);
				}
			} else {
				if (!visual->autoblocksize) {
					rz_core_block_size(core, core->blocksize - 1);
				}
			}
			break;
		case '+':
			if (core->print->cur_enabled) {
				if (visual->printidx == RZ_CORE_VISUAL_MODE_DB) {
					if (!core->seltab) {
						// stack view
						int w = rz_config_get_i(core->config, "hex.cols");
						rz_config_set_i(core->config, "stack.size",
							rz_config_get_i(core->config, "stack.size") + w);
					}
				} else {
					if (core->print->ocur == -1) {
						sprintf(buf, "woa 01 @ $$+%i @!1", core->print->cur);
					} else {
						sprintf(buf, "woa 01 @ $$+%i @!%i", core->print->cur < core->print->ocur ? core->print->cur : core->print->ocur,
							RZ_ABS(core->print->ocur - core->print->cur) + 1);
					}
					rz_core_cmd(core, buf, 0);
				}
			} else {
				if (!visual->autoblocksize) {
					rz_core_block_size(core, core->blocksize + 1);
				}
			}
			break;
		case '/': {
			bool mouse_state = __holdMouseState(core);
			if (core->print->cur_enabled) {
				visual_search(core);
			} else {
				if (visual->autoblocksize) {
					rz_core_cmd0(core, "?i highlight;e scr.highlight=`yp`");
				} else {
					rz_core_block_size(core, core->blocksize - cols);
				}
			}
			rz_cons_enable_mouse(mouse_state && rz_config_get_i(core->config, "scr.wheel"));
		} break;
		case ')':
			rotateAsmemu(core);
			break;
		case '#':
			if (visual->printidx == 1) {
				rz_core_visual_toggle_decompiler_disasm(core, false, false);
			} else {
				// do nothing for now :?, px vs pxa?
			}
			break;
		case '*':
			if (core->print->cur_enabled) {
				rz_core_reg_set_by_role_or_name(core, "PC", core->offset + core->print->cur);
			} else if (!visual->autoblocksize) {
				rz_core_block_size(core, core->blocksize + cols);
			}
			break;
		case '>':
			if (core->print->cur_enabled) {
				if (core->print->ocur == -1) {
					RZ_LOG_ERROR("core: No range selected. Use HJKL.\n");
					rz_cons_any_key(NULL);
					break;
				}
				char buf[128];
				// TODO autocomplete filenames
				prompt_read("dump to file: ", buf, sizeof(buf));
				if (buf[0]) {
					ut64 from = core->offset + core->print->ocur;
					ut64 size = RZ_ABS(core->print->cur - core->print->ocur) + 1;
					rz_core_dump(core, buf, from, size, false);
				}
			} else {
				rz_core_seek_and_save(core, core->offset + core->blocksize, false);
			}
			break;
		case '<': // "V<"
			if (core->print->cur_enabled) {
				char buf[128];
				// TODO autocomplete filenames
				prompt_read("load from file: ", buf, sizeof(buf));
				if (buf[0]) {
					size_t sz;
					char *data = rz_file_slurp(buf, &sz);
					if (data) {
						int cur;
						if (core->print->ocur != -1) {
							cur = RZ_MIN(core->print->cur, core->print->ocur);
						} else {
							cur = core->print->cur;
						}
						ut64 from = core->offset + cur;
						ut64 size = RZ_ABS(core->print->cur - core->print->ocur) + 1;
						ut64 s = RZ_MIN(size, (ut64)sz);
						rz_io_write_at(core->io, from, (const ut8 *)data, s);
					}
				}
			} else {
				rz_core_seek_and_save(core, core->offset - core->blocksize, false);
			}
			break;
		case '.': // "V."
			if (core->print->cur_enabled) {
				rz_config_set_i(core->config, "stack.delta", 0);
				rz_core_seek_and_save(core, core->offset + core->print->cur, true);
				core->print->cur = 0;
			} else {
				ut64 addr = rz_debug_reg_get(core->dbg, "PC");
				if (addr && addr != UT64_MAX) {
					rz_core_seek_and_save(core, addr, true);
					rz_core_analysis_set_reg(core, "PC", addr);
				} else {
					ut64 entry = rz_num_get(core->num, "entry0");
					if (!entry || entry == UT64_MAX) {
						RzBinObject *o = rz_bin_cur_object(core->bin);
						RzBinSection *s = o ? rz_bin_get_section_at(o, addr, core->io->va) : NULL;
						if (s) {
							entry = s->vaddr;
						} else {
							RzPVector *maps = rz_io_maps(core->io);
							RzIOMap *map = rz_pvector_pop(maps);
							if (map) {
								entry = map->itv.addr;
							} else {
								entry = rz_config_get_i(core->config, "bin.baddr");
							}
							rz_pvector_push_front(maps, map);
						}
					}
					if (entry != UT64_MAX) {
						rz_core_seek_and_save(core, entry, true);
					}
				}
			}
			break;
		case ':':
			rz_core_visual_prompt_input(core);
			break;
		case '_':
			rz_core_visual_hudstuff(core);
			break;
		case ';':
			rz_cons_gotoxy(0, 0);
			ut64 addr = core->print->cur_enabled ? core->offset + core->print->cur : core->offset;
			add_comment(core, addr, "Enter a comment: ('-' to remove, '!' to use cfg.editor)\n");
			break;
		case 'b':
			rz_core_visual_browse(core, arg + 1);
			break;
		case 'B': {
			ut64 addr = core->print->cur_enabled ? core->offset + core->print->cur : core->offset;
			rz_core_debug_breakpoint_toggle(core, addr);
		} break;
		case 'u':
			rz_core_visual_seek_animation_undo(core);
			break;
		case 'U':
			rz_core_visual_seek_animation_redo(core);
			break;
		case '?':
			if (visual_help(core) == '?') {
				rz_core_visual_hud(core);
			}
			break;
		case 0x1b:
		case 'q':
		case 'Q':
			setcursor(core, false);
			return false;
		}
		numbuf_i = 0;
	}
	rz_core_block_read(core);
	return true;
}

static void visual_flagzone(RzCore *core) {
	const char *a, *b;
	int a_len = 0;
	int w = rz_cons_get_size(NULL);
	rz_flag_zone_around(core->flags, core->offset, &a, &b);
	if (a) {
		rz_cons_printf("[<< %s]", a);
		a_len = strlen(a) + 4;
	}
	int padsize = (w / 2) - a_len;
	int title_size = 12;
	if (a || b) {
		char *title = rz_str_newf("[ 0x%08" PFMT64x " ]", core->offset);
		title_size = strlen(title);
		padsize -= strlen(title) / 2;
		const char *halfpad = rz_str_pad(' ', padsize);
		rz_cons_printf("%s%s", halfpad, title);
		free(title);
	}
	if (b) {
		padsize = (w / 2) - title_size - strlen(b) - 4;
		const char *halfpad = padsize > 1 ? rz_str_pad(' ', padsize) : "";
		rz_cons_printf("%s[%s >>]", halfpad, b);
	}
	if (a || b) {
		rz_cons_newline();
	}
}

RZ_IPI void rz_core_visual_title(RzCore *core, int color) {
	bool showDelta = rz_config_get_b(core->config, "scr.slow");
	static ut64 oldpc = 0;
	const char *BEGIN = core->cons->context->pal.prompt;
	const char *filename;
	char pos[512], bar[512], pcs[32];
	RzCoreVisual *visual = core->visual;
	if (!oldpc) {
		oldpc = rz_debug_reg_get(core->dbg, "PC");
	}
	/* automatic block size */
	int pc, hexcols = rz_config_get_i(core->config, "hex.cols");
	if (visual->autoblocksize) {
		switch (visual->printidx) {
		case RZ_CORE_VISUAL_MODE_PX: // x
			if (visual->currentFormat == 3 || visual->currentFormat == 9 || visual->currentFormat == 5) { // prx
				rz_core_block_size(core, (int)(core->cons->rows * hexcols * 4));
			} else if ((RZ_ABS(visual->hexMode) % 3) == 0) { // prx
				rz_core_block_size(core, (int)(core->cons->rows * hexcols));
			} else {
				rz_core_block_size(core, (int)(core->cons->rows * hexcols * 2));
			}
			break;
		case RZ_CORE_VISUAL_MODE_OV:
		case RZ_CORE_VISUAL_MODE_CD:
			rz_core_block_size(core, (int)(core->cons->rows * hexcols * 2));
			break;
		case RZ_CORE_VISUAL_MODE_PD: // pd
		case RZ_CORE_VISUAL_MODE_DB: // pd+dbg
		{
			int bsize = core->cons->rows * 5;

			if (core->print->screen_bounds > 1) {
				// estimate new blocksize with the size of the last
				// printed instructions
				int new_sz = core->print->screen_bounds - core->offset + 32;
				new_sz = RZ_MIN(new_sz, 16 * 1024);
				if (new_sz > bsize) {
					bsize = new_sz;
				}
			}
			rz_core_block_size(core, bsize);
			break;
		}
		}
	}
	if (rz_config_get_i(core->config, "scr.scrollbar") == 2) {
		visual_flagzone(core);
	}
	if (rz_config_get_b(core->config, "cfg.debug")) {
		ut64 curpc = rz_debug_reg_get(core->dbg, "PC");
		if (curpc && curpc != UT64_MAX && curpc != oldpc) {
			// check dbg.follow here
			int follow = (int)(st64)rz_config_get_i(core->config, "dbg.follow");
			if (follow > 0) {
				if ((curpc < core->offset) || (curpc > (core->offset + follow))) {
					rz_core_seek(core, curpc, true);
				}
			} else if (follow < 0) {
				rz_core_seek(core, curpc + follow, true);
			}
			oldpc = curpc;
		}
	}
	RzIOMap *map = rz_io_map_get(core->io, core->offset);
	RzIODesc *desc = map
		? rz_io_desc_get(core->io, map->fd)
		: core->file ? rz_io_desc_get(core->io, core->file->fd)
			     : NULL;
	filename = desc ? desc->name : "";

	{ /* get flag with delta */
		ut64 addr = core->offset + (core->print->cur_enabled ? core->print->cur : 0);
		/* TODO: we need a helper into rz_flags to do that */
		RzFlagItem *f = NULL;
		if (rz_flag_space_push(core->flags, RZ_FLAGS_FS_SYMBOLS)) {
			f = rz_flag_get_at(core->flags, addr, showDelta);
			rz_flag_space_pop(core->flags);
		}
		if (!f) {
			f = rz_flag_get_at(core->flags, addr, showDelta);
		}
		if (f) {
			if (f->offset == addr || !f->offset) {
				snprintf(pos, sizeof(pos), "@ %s", f->name);
			} else {
				snprintf(pos, sizeof(pos), "@ %s+%d # 0x%" PFMT64x,
					f->name, (int)(addr - f->offset), addr);
			}
		} else {
			RzAnalysisFunction *fcn = rz_analysis_get_fcn_in(core->analysis, addr, 0);
			if (fcn) {
				int delta = addr - fcn->addr;
				if (delta > 0) {
					snprintf(pos, sizeof(pos), "@ %s+%d", fcn->name, delta);
				} else if (delta < 0) {
					snprintf(pos, sizeof(pos), "@ %s%d", fcn->name, delta);
				} else {
					snprintf(pos, sizeof(pos), "@ %s", fcn->name);
				}
			} else {
				pos[0] = 0;
			}
		}
	}

	if (core->print->cur < 0) {
		core->print->cur = 0;
	}

	if (color) {
		rz_cons_strcat(BEGIN);
	}
	const char *cmd_visual = rz_config_get(core->config, "cmd.visual");
	if (cmd_visual && *cmd_visual) {
		rz_str_ncpy(bar, cmd_visual, sizeof(bar) - 1);
		bar[10] = '.'; // chop cmdfmt
		bar[11] = '.'; // chop cmdfmt
		bar[12] = 0; // chop cmdfmt
	} else {
		const char *cmd = __core_visual_print_command(core);
		if (cmd) {
			rz_str_ncpy(bar, cmd, sizeof(bar) - 1);
			bar[10] = '.'; // chop cmdfmt
			bar[11] = '.'; // chop cmdfmt
			bar[12] = 0; // chop cmdfmt
		}
	}
	{
		ut64 sz = rz_io_size(core->io);
		ut64 pa = core->offset;
		{
			RzIOMap *map = rz_io_map_get(core->io, core->offset);
			if (map) {
				pa = map->delta;
			}
		}
		if (sz == UT64_MAX) {
			pcs[0] = 0;
		} else {
			if (!sz || pa > sz) {
				pc = 0;
			} else {
				pc = (pa * 100) / sz;
			}
			sprintf(pcs, "%d%% ", pc);
		}
	}
	{
		char *title;
		char *address = (core->print->wide_offsets && core->dbg->bits & RZ_SYS_BITS_64)
			? rz_str_newf("0x%016" PFMT64x, core->offset)
			: rz_str_newf("0x%08" PFMT64x, core->offset);
		if (visual->insertMode) {
			title = rz_str_newf("[%s + %d> * INSERT MODE *\n",
				address, core->print->cur);
		} else {
			char pm[32] = "[XADVC]";
			int i;
			for (i = 0; i < 6; i++) {
				if (visual->printidx == i) {
					pm[i + 1] = toupper(pm[i + 1]);
				} else {
					pm[i + 1] = tolower(pm[i + 1]);
				}
			}
			if (core->print->cur_enabled) {
				if (core->print->ocur == -1) {
					title = rz_str_newf("[%s *0x%08" PFMT64x " %s%d ($$+0x%x)]> %s %s\n",
						address, core->offset + core->print->cur,
						pm, visual->currentFormat, core->print->cur,
						bar, pos);
				} else {
					title = rz_str_newf("[%s 0x%08" PFMT64x " %s%d [0x%x..0x%x] %d]> %s %s\n",
						address, core->offset + core->print->cur,
						pm, visual->currentFormat, core->print->ocur, core->print->cur,
						RZ_ABS(core->print->cur - core->print->ocur) + 1,
						bar, pos);
				}
			} else {
				title = rz_str_newf("[%s %s%d %s%d %s]> %s %s\n",
					address, pm, visual->currentFormat, pcs, core->blocksize, filename, bar, pos);
			}
		}
		const int tabsCount = rz_core_visual_tab_count(core);
		if (tabsCount > 0) {
			const char *kolor = core->cons->context->pal.prompt;
			char *tabstring = rz_core_visual_tab_string(core, kolor);
			if (tabstring) {
				title = rz_str_append(title, tabstring);
				free(tabstring);
			}
#if 0
			// TODO: add an option to show this tab mode instead?
			const int curTab = core->visual.tab;
			rz_cons_printf ("[");
			int i;
			for (i = 0; i < tabsCount; i++) {
				if (i == curTab) {
					rz_cons_printf ("%d", curTab + 1);
				} else {
					rz_cons_printf (".");
				}
			}
			rz_cons_printf ("]");
			rz_cons_printf ("[tab:%d/%d]", core->visual.tab, tabsCount);
#endif
		}
		rz_cons_print(title);
		free(title);
		free(address);
	}
	if (color) {
		rz_cons_strcat(Color_RESET);
	}
}

static int visual_responsive(RzCore *core) {
	int h, w = rz_cons_get_size(&h);
	if (rz_config_get_b(core->config, "scr.responsive")) {
		if (w < 110) {
			rz_config_set_b(core->config, "asm.cmt.right", false);
		} else {
			rz_config_set_b(core->config, "asm.cmt.right", true);
		}
		if (w < 68) {
			rz_config_set_i(core->config, "hex.cols", (int)(w / 5.2));
		} else {
			rz_config_set_i(core->config, "hex.cols", 16);
		}
		if (w < 25) {
			rz_config_set_b(core->config, "asm.offset", false);
		} else {
			rz_config_set_b(core->config, "asm.offset", true);
		}
		if (w > 80) {
			rz_config_set_i(core->config, "asm.lines.width", 14);
			rz_config_set_i(core->config, "asm.lines.width", w - (int)(w / 1.2));
			rz_config_set_i(core->config, "asm.cmt.col", w - (int)(w / 2.5));
		} else {
			rz_config_set_i(core->config, "asm.lines.width", 7);
		}
		if (w < 70) {
			rz_config_set_i(core->config, "asm.lines.width", 1);
			rz_config_set_b(core->config, "asm.bytes", false);
		} else {
			rz_config_set_b(core->config, "asm.bytes", true);
		}
	}
	return w;
}

// TODO: use colors
RZ_IPI void rz_core_visual_scrollbar(RzCore *core) {
	int i, h, w = rz_cons_get_size(&h);

	int scrollbar = rz_config_get_i(core->config, "scr.scrollbar");
	if (scrollbar == 2) {
		// already handled by the visual_flagzone()
		return;
	}
	if (scrollbar > 2) {
		rz_core_visual_scrollbar_bottom(core);
		return;
	}

	if (w < 10 || h < 3) {
		return;
	}
	ut64 from = 0;
	ut64 to = UT64_MAX;
	if (rz_config_get_b(core->config, "cfg.debug")) {
		from = rz_num_math(core->num, "$D");
		to = rz_num_math(core->num, "$D+$DD");
	} else if (rz_config_get_b(core->config, "io.va")) {
		from = rz_num_math(core->num, "$S");
		to = rz_num_math(core->num, "$S+$SS");
	} else {
		to = rz_num_math(core->num, "$s");
	}
	char *s = rz_str_newf("[0x%08" PFMT64x "]", from);
	rz_cons_gotoxy(w - strlen(s) + 1, 2);
	rz_cons_strcat(s);
	free(s);

	ut64 block = (to - from) / h;

	RzList *words = rz_flag_zone_barlist(core->flags, from, block, h);

	bool hadMatch = false;
	for (i = 0; i < h; i++) {
		const char *word = rz_list_pop_head(words);
		if (word && *word) {
			rz_cons_gotoxy(w - strlen(word) - 1, i + 3);
			rz_cons_printf("%s>", word);
		}
		rz_cons_gotoxy(w, i + 3);
		if (hadMatch) {
			rz_cons_printf("|");
		} else {
			ut64 cur = from + (block * i);
			ut64 nex = from + (block * (i + 1));
			if (RZ_BETWEEN(cur, core->offset, nex)) {
				rz_cons_printf(Color_INVERT "|" Color_RESET);
				hadMatch = true;
			} else {
				rz_cons_printf("|");
			}
		}
	}
	s = rz_str_newf("[0x%08" PFMT64x "]", to);
	if (s) {
		rz_cons_gotoxy(w - strlen(s) + 1, h + 1);
		rz_cons_strcat(s);
		free(s);
	}
	rz_list_free(words);
	rz_cons_flush();
}

RZ_IPI void rz_core_visual_scrollbar_bottom(RzCore *core) {
	int i, h, w = rz_cons_get_size(&h);

	if (w < 10 || h < 4) {
		return;
	}
	ut64 from = 0;
	ut64 to = UT64_MAX;
	if (rz_config_get_b(core->config, "cfg.debug")) {
		from = rz_num_math(core->num, "$D");
		to = rz_num_math(core->num, "$D+$DD");
	} else if (rz_config_get_b(core->config, "io.va")) {
		from = rz_num_math(core->num, "$S");
		to = rz_num_math(core->num, "$S+$SS");
	} else {
		to = rz_num_math(core->num, "$s");
	}
	char *s = rz_str_newf("[0x%08" PFMT64x "]", from);
	int slen = strlen(s) + 1;
	rz_cons_gotoxy(0, h + 1);
	rz_cons_strcat(s);
	free(s);

	int linew = (w - (slen * 2)) + 1;
	ut64 block = (to - from) / linew;

	RzList *words = rz_flag_zone_barlist(core->flags, from, block, h);

	bool hadMatch = false;
	for (i = 0; i < linew + 1; i++) {
		rz_cons_gotoxy(i + slen, h + 1);
		if (hadMatch) {
			rz_cons_strcat("-");
		} else {
			ut64 cur = from + (block * i);
			ut64 nex = from + (block * (i + 2));
			if (RZ_BETWEEN(cur, core->offset, nex)) {
				rz_cons_strcat(Color_INVERT "-" Color_RESET);
				hadMatch = true;
			} else {
				rz_cons_strcat("-");
			}
		}
	}
	for (i = 0; i < linew; i++) {
		const char *word = rz_list_pop_head(words);
		if (word && *word) {
			ut64 cur = from + (block * i);
			ut64 nex = from + (block * (i + strlen(word) + 1));
			rz_cons_gotoxy(i + slen - 1, h);
			if (RZ_BETWEEN(cur, core->offset, nex)) {
				rz_cons_printf(Color_INVERT "{%s}" Color_RESET, word);
			} else {
				rz_cons_printf("{%s}", word);
			}
		}
	}
	s = rz_str_newf("[0x%08" PFMT64x "]", to);
	if (s) {
		rz_cons_gotoxy(linew + slen + 1, h + 1);
		rz_cons_strcat(s);
		free(s);
	}
	rz_list_free(words);
	rz_cons_flush();
}

static void visual_refresh(RzCore *core) {
	static ut64 oseek = UT64_MAX;
	const char *vi, *vcmd, *cmd_str;
	if (!core) {
		return;
	}
	rz_print_set_cursor(core->print, core->print->cur_enabled, core->print->ocur, core->print->cur);
	core->cons->blankline = true;
	RzCoreVisual *visual = core->visual;

	int w = visual_responsive(core);

	if (!visual->autoblocksize) {
		rz_cons_clear();
	}
	rz_cons_goto_origin_reset();
	rz_cons_flush();

	int hex_cols = rz_config_get_i(core->config, "hex.cols");
	int split_w = 12 + 4 + hex_cols + (hex_cols * 3);
	bool ce = core->print->cur_enabled;

	vi = rz_config_get(core->config, "cmd.cprompt");
	bool vsplit = (vi && *vi);

	if (vsplit) {
		// XXX: slow
		core->cons->blankline = false;
		if (split_w > w) {
			// do not show column contents
		} else {
			rz_cons_clear();
			rz_cons_printf("[cmd.cprompt=%s]\n", vi);
			if (oseek != UT64_MAX) {
				rz_core_seek(core, oseek, true);
			}
			rz_core_cmd0(core, vi);
			rz_cons_column(split_w + 1);
			if (!strncmp(vi, "p=", 2) && core->print->cur_enabled) {
				oseek = core->offset;
				core->print->cur_enabled = false;
				rz_core_seek(core, core->num->value, true);
			} else {
				oseek = UT64_MAX;
			}
		}
		rz_cons_gotoxy(0, 0);
	}
	vi = rz_config_get(core->config, "cmd.vprompt");
	if (vi && *vi) {
		rz_core_cmd0(core, vi);
	}
	rz_core_visual_title(core, visual->color);
	vcmd = rz_config_get(core->config, "cmd.visual");
	if (vcmd && *vcmd) {
		// disable screen bounds when it's a user-defined command
		// because it can cause some issues
		core->print->screen_bounds = 0;
		cmd_str = vcmd;
	} else {
		if (visual->splitView) {
			static char debugstr[512];
			const char *pxw = NULL;
			int h = rz_num_get(core->num, "$r");
			int size = (h * 16) / 2;
			switch (visual->printidx) {
			case 1:
				size = (h - 2) / 2;
				pxw = "pd";
				break;
			default:
				pxw = stackPrintCommand(core);
				break;
			}
			snprintf(debugstr, sizeof(debugstr),
				"?0;%s %d @ %" PFMT64d ";cl;"
				"?1;%s %d @ %" PFMT64d ";",
				pxw, size, visual->splitPtr,
				pxw, size, core->offset);
			core->print->screen_bounds = 1LL;
			cmd_str = debugstr;
		} else {
			core->print->screen_bounds = 1LL;
			cmd_str = __core_visual_print_command(core);
		}
	}
	if (cmd_str && *cmd_str) {
		if (vsplit) {
			char *cmd_result = rz_core_cmd_str(core, cmd_str);
			cmd_result = rz_str_ansi_crop(cmd_result, 0, 0, split_w, -1);
			rz_cons_strcat(cmd_result);
		} else {
			rz_core_cmd0(core, cmd_str);
		}
	}
	core->print->cur_enabled = ce;
#if 0
	if (core->print->screen_bounds != 1LL) {
		rz_cons_printf ("[0x%08"PFMT64x "..0x%08"PFMT64x "]\n",
			core->offset, core->print->screen_bounds);
	}
#endif

	/* this is why there's flickering */
	if (core->print->vflush) {
		rz_cons_visual_flush();
	} else {
		rz_cons_reset();
	}
	if (core->scr_gadgets) {
		rz_core_gadget_print(core);
		rz_cons_flush();
	}
	core->cons->blankline = false;
	core->cons->blankline = true;
	core->curtab = 0; // which command are we focusing
	// core->seltab = 0; // user selected tab

	if (rz_config_get_i(core->config, "scr.scrollbar")) {
		rz_core_visual_scrollbar(core);
	}
}

static void visual_refresh_oneshot(RzCore *core) {
	rz_core_task_enqueue_oneshot(&core->tasks, (RzCoreTaskOneShot)visual_refresh, core);
}

RZ_IPI void rz_core_visual_disasm_up(RzCore *core, int *cols) {
	*cols = rz_core_visual_prevopsz(core, core->offset);
}

RZ_IPI void rz_core_visual_disasm_down(RzCore *core, RzAsmOp *op, int *cols) {
	int midflags = rz_config_get_i(core->config, "asm.flags.middle");
	const bool midbb = rz_config_get_b(core->config, "asm.bb.middle");
	op->size = 1;
	rz_asm_set_pc(core->rasm, core->offset);
	*cols = rz_asm_disassemble(core->rasm,
		op, core->block, 32);
	if (midflags || midbb) {
		int skip_bytes_flag = 0, skip_bytes_bb = 0;
		if (midflags >= RZ_MIDFLAGS_REALIGN) {
			skip_bytes_flag = rz_core_flag_in_middle(core, core->offset, *cols, &midflags);
		}
		if (midbb) {
			skip_bytes_bb = rz_core_bb_starts_in_middle(core, core->offset, *cols);
		}
		if (skip_bytes_flag) {
			*cols = skip_bytes_flag;
		}
		if (skip_bytes_bb && skip_bytes_bb < *cols) {
			*cols = skip_bytes_bb;
		}
	}
	if (*cols < 1) {
		*cols = op->size > 1 ? op->size : 1;
	}
}

#ifdef __WINDOWS__

static bool is_mintty(RzCons *cons) {
	return cons->term_pty;
}

static void flush_stdin(void) {
	while (rz_cons_readchar_timeout(1) != -1)
		;
}

#else

static bool is_mintty(RzCons *cons) {
	return false;
}

static void flush_stdin(void) {
	tcflush(STDIN_FILENO, TCIFLUSH);
}

#endif

RZ_IPI int rz_core_visual(RzCore *core, const char *input) {
	const char *teefile;
	ut64 scrseek;
	int flags, ch;
	bool skip;
	char arg[2] = {
		input[0], 0
	};

	RzCoreVisual *visual = core->visual;
	visual->splitPtr = UT64_MAX;

	if (rz_cons_get_size(&ch) < 1 || ch < 1) {
		RZ_LOG_ERROR("core: Cannot create Visual context. Use scr.fix_{columns|rows}\n");
		return 0;
	}
	visual->obs = core->blocksize;
	// rz_cons_set_cup (true);

	core->vmode = false;
	/* honor vim */
	if (!strncmp(input, "im", 2)) {
		char *cmd = rz_str_newf("!v%s", input);
		int ret = rz_core_cmd0(core, cmd);
		free(cmd);
		return ret;
	}
	while (*input) {
		int len = *input == 'd' ? 2 : 1;
		if (!rz_core_visual_cmd(core, input)) {
			return 0;
		}
		input += len;
	}
	core->vmode = true;

	// disable tee in cons
	teefile = rz_cons_singleton()->teefile;
	rz_cons_singleton()->teefile = "";

	static char debugstr[512];
	core->print->flags |= RZ_PRINT_FLAGS_ADDRMOD;
	do {
	dodo:
		rz_core_visual_tab_update(core);
		// update the cursor when it's not visible anymore
		skip = fix_cursor(core);
		rz_cons_show_cursor(false);
		rz_cons_set_raw(1);
		const int ref = rz_config_get_b(core->config, "dbg.slow");
#if 1
		// This is why multiple debug views dont work
		if (visual->printidx == RZ_CORE_VISUAL_MODE_DB) {
			const bool pxa = rz_config_get_b(core->config, "stack.anotated"); // stack.anotated
			const char *reg = rz_config_get(core->config, "stack.reg");
			const int size = rz_config_get_i(core->config, "stack.size");
			const int delta = rz_config_get_i(core->config, "stack.delta");
			const char *cmdvhex = rz_config_get(core->config, "cmd.stack");

			if (cmdvhex && *cmdvhex) {
				rz_strf(debugstr,
					"?0 ; f+ tmp ; sr %s @e: cfg.seek.silent=true ; %s ; ?1 ; %s ; ?1 ; "
					"s tmp @e: cfg.seek.silent=true ; f- tmp ; pd $r",
					reg, cmdvhex,
					ref ? CMD_REGISTERS_REFS : CMD_REGISTERS);
				debugstr[sizeof(debugstr) - 1] = 0;
			} else {
				const char *pxw = stackPrintCommand(core);
				const char sign = (delta < 0) ? '+' : '-';
				const int absdelta = RZ_ABS(delta);
				rz_strf(debugstr,
					"diq ; ?0 ; f+ tmp ; sr %s @e: cfg.seek.silent=true ; %s %d @ $$%c%d;"
					"?1 ; %s;"
					"?1 ; s tmp @e: cfg.seek.silent=true ; f- tmp ; afal ; pd $r",
					reg, pxa ? "pxa" : pxw, size, sign, absdelta,
					ref ? CMD_REGISTERS_REFS : CMD_REGISTERS);
			}
			printfmtSingle[2] = debugstr;
		}
#endif
		rz_cons_enable_mouse(rz_config_get_b(core->config, "scr.wheel"));
		core->cons->event_resize = NULL; // avoid running old event with new data
		core->cons->event_data = core;
		core->cons->event_resize = (RzConsEvent)visual_refresh_oneshot;
		flags = core->print->flags;
		visual->color = rz_config_get_i(core->config, "scr.color");
		if (visual->color) {
			flags |= RZ_PRINT_FLAGS_COLOR;
		}
		visual->debug = rz_config_get_b(core->config, "cfg.debug");
		flags |= RZ_PRINT_FLAGS_ADDRMOD | RZ_PRINT_FLAGS_HEADER;
		rz_print_set_flags(core->print, flags);
		scrseek = rz_num_math(core->num,
			rz_config_get(core->config, "scr.seek"));
		if (scrseek != 0LL) {
			rz_core_seek(core, scrseek, true);
		}
		if (visual->debug) {
			rz_core_reg_update_flags(core);
		}
		core->print->vflush = !skip;
		visual_refresh(core);
		if (insert_mode_enabled(core)) {
			goto dodo;
		}
		if (!skip) {
			ch = rz_cons_readchar();

			if (I->vtmode == RZ_VIRT_TERM_MODE_COMPLETE && !is_mintty(core->cons)) {
				// Prevent runaway scrolling
				if (IS_PRINTABLE(ch) || ch == '\t' || ch == '\n') {
					flush_stdin();
				} else if (ch == 0x1b) {
					char chrs[2];
					int chrs_read = 1;
					chrs[0] = rz_cons_readchar();
					if (chrs[0] == '[') {
						chrs[1] = rz_cons_readchar();
						chrs_read++;
						if (chrs[1] >= 'A' && chrs[1] <= 'D') { // arrow keys
							flush_stdin();
#ifndef __WINDOWS__
							// Following seems to fix an issue where scrolling slows
							// down to a crawl for some terminals after some time
							// mashing the up and down arrow keys
							rz_cons_set_raw(false);
							rz_cons_set_raw(true);
#endif
						}
					}
					(void)rz_cons_readpush(chrs, chrs_read);
				}
			}
			if (rz_cons_is_breaked()) {
				break;
			}
			rz_core_visual_show_char(core, ch);
			if (ch == -1 || ch == 4) {
				break; // error or eof
			}
			arg[0] = ch;
			arg[1] = 0;
		}
	} while (skip || (*arg && rz_core_visual_cmd(core, arg)));

	rz_cons_enable_mouse(false);
	if (visual->color) {
		rz_cons_strcat(Color_RESET);
	}
	rz_config_set_i(core->config, "scr.color", visual->color);
	core->print->cur_enabled = false;
	if (visual->autoblocksize) {
		rz_core_block_size(core, visual->obs);
	}
	rz_cons_singleton()->teefile = teefile;
	rz_cons_set_cup(false);
	rz_cons_clear00();
	core->vmode = false;
	core->cons->event_resize = NULL;
	core->cons->event_data = NULL;
	rz_cons_show_cursor(true);
	return 0;
}
