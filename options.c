#define _GNU_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <popt.h>

#include "options.h"
#include "config.h"

#define COLORS \
	X("tb", "Title background color", title_background_color, BM_COLOR_TITLE_BG) \
	X("tf", "Title foreground color", title_foreground_color, BM_COLOR_TITLE_FG) \
	X("fb", "Filter background color", filter_background_color, BM_COLOR_FILTER_BG) \
	X("ff", "Filter foreground color", filter_foreground_color, BM_COLOR_FILTER_FG) \
	X("cb", "Cursor background. color", cursor_background_color, BM_COLOR_CURSOR_BG) \
	X("cf", "Cursor foreground color", cursor_foreground_color, BM_COLOR_CURSOR_FG) \
	X("nb", "Normal background color", normal_background_color, BM_COLOR_ITEM_BG) \
	X("nf", "Normal foreground color", normal_foreground_color, BM_COLOR_ITEM_FG) \
	X("hb", "Highlighted background color", highlighted_background_color, BM_COLOR_HIGHLIGHTED_BG) \
	X("hf", "Highlighted foreground color", highlighted_foreground_color, BM_COLOR_HIGHLIGHTED_FG) \
	X("fbb", "Feedback background color", feedback_background_color, BM_COLOR_SELECTED_BG) \
	X("fbf", "Feedback foreground color", feedback_foreground_color, BM_COLOR_SELECTED_FG) \
	X("sb", "Selected background color", selected_background_color, BM_COLOR_SELECTED_BG) \
	X("sf", "Selected foreground color", selected_foreground_color, BM_COLOR_SELECTED_FG) \
	X("ab", "Alternating background. color", alternating_background_color, BM_COLOR_ALTERNATE_BG) \
	X("af", "Alternating foreground color", alternating_foreground_color, BM_COLOR_ALTERNATE_FG) \
	X("scb", "Scrollbar background color", scollbar_background_color, BM_COLOR_SCROLLBAR_BG) \
	X("scf", "Scrollbar foreground color", scollbar_foreground_color, BM_COLOR_SCROLLBAR_FG) \

static int debug;
#if HAVE_BEMENU_SET_BOTTOM || HAVE_BEMENU_SET_ALIGN
static int bottom;
#endif
#if HAVE_BEMENU_SET_ALIGN
static int center;
#endif
static int no_overlap;
static char *monitor;
#ifdef HAVE_BEMENU_PASSWORD_INDICATOR
static char *password;
#endif
static int height;
static char *font;
static char *display;
static int ignore_case;
static int list = 3;
static int wrap;
#define X(option, help, var, setting) static char *var;
COLORS
#undef X

static struct poptOption optionsTable[] = {
	{ "debug", '\0', POPT_ARG_NONE, &debug, 0, NULL, NULL },
#if HAVE_BEMENU_SET_BOTTOM || HAVE_BEMENU_SET_ALIGN
	{ "bottom", 'b', POPT_ARG_NONE, &bottom, 0, NULL, NULL },
#endif
#if HAVE_BEMENU_SET_ALIGN
	{ "center", 'c', POPT_ARG_NONE, &center, 0, NULL, NULL },
#endif
	{ "no-overlap", 'n', POPT_ARG_NONE, &no_overlap, 0, NULL, NULL },
	{ "monitor", 'm', POPT_ARG_STRING, &monitor, 0, "Index of the monitor to use, or \"focused\" or \"all\"", NULL },
#ifdef HAVE_BEMENU_PASSWORD_INDICATOR
	{ "password", 'x', POPT_ARG_STRING, &password, 0, "Handling of input characters: \"none\", or \"hide\" or \"indicator\"", NULL },
#endif
	{ "line-height", 'H', POPT_ARG_INT, &height, 0, "Height for each menu line", NULL },
	{ "fn", '\0', POPT_ARG_STRING, &font, 0, "Font", NULL },
	{ "display", 'D', POPT_ARG_STRING, &display, 0, "Set the X display", NULL },
	{ "ignorecase", 'i', POPT_ARG_NONE, &ignore_case, 0, "Match items case insensitively", NULL },
	{ "list", 'l', POPT_ARG_INT, &list, 0, "List items vertically with the given number of lines", NULL },
	{ "wrap", 'w', POPT_ARG_NONE, &wrap, 0, "Wraps cursor selection", NULL },
#define X(option, help, var, setting) \
	{ option, '\0', POPT_ARG_STRING, &var, 0, help, "#RRGGBB" },
	COLORS
#undef X
	POPT_AUTOHELP
	POPT_TABLEEND
};

void validate_option_parsing(int rc, poptContext ctx) {
	if (rc != -1) {
		fprintf(stderr, PROJECT_NAME " %s: %s\n",
				poptBadOption(ctx, POPT_BADOPTION_NOALIAS),
				poptStrerror(rc));
	}
}

void parse_environment_options(void) {
	const char *opts = getenv("BEMENU_OPTS");

	if (!opts || !opts[0])
		return;

	int env_argc;
	const char **env_argv;
	int rc = poptParseArgvString(opts, &env_argc, &env_argv);
	poptContext ctx = poptGetContext(NULL,
			env_argc, env_argv,
			optionsTable, POPT_CONTEXT_KEEP_FIRST);
	rc = poptGetNextOpt(ctx);
	validate_option_parsing(rc, ctx);
	poptFreeContext(ctx);
	free(env_argv);
}

void parse_options(int argc, const char **argv) {
	parse_environment_options();

	poptContext ctx = poptGetContext(NULL, argc, argv, optionsTable, 0);

	int rc = poptGetNextOpt(ctx);
	validate_option_parsing(rc, ctx);

	poptFreeContext(ctx);
}

void free_options(void) {
	free(font);
#define X(option, help, var, setting) free(var);
	COLORS
#undef X
}

bool is_debug(void) {
	return debug;
}

void apply_global_options() {
	if (display) {
		setenv("DISPLAY", display, 1);
	}
}

static int get_monitor_idx(const char *monitor) {
	if (!monitor) {
		return -1;
	}

	char *end;
	long idx = strtol(monitor, &end, 10);
	if (end && (*end == '\0')) {
		return (int)idx;
	}

	if (strcmp("all", monitor) == 0) {
		return -2;
	}

	// for "focused" and invalid input, fall back to default behavior
	return -1;
}

void apply_options(struct bm_menu *menu) {
	assert(menu);

#if HAVE_BEMENU_SET_BOTTOM
	bm_menu_set_bottom(menu, bottom);
#endif
#if HAVE_BEMENU_SET_ALIGN
	enum bm_align align = BM_ALIGN_TOP;
	if (bottom) align = BM_ALIGN_BOTTOM;
	if (center) align = BM_ALIGN_CENTER;
	bm_menu_set_align(menu, align);
#endif
	bm_menu_set_panel_overlap(menu, !no_overlap);
	bm_menu_set_monitor(menu, get_monitor_idx(monitor));
#ifdef HAVE_BEMENU_PASSWORD_INDICATOR
	if (!password || !strcmp(password, "hide"))
		bm_menu_set_password(menu, BM_PASSWORD_HIDE);
	else if (!strcmp(password, "none"))
		bm_menu_set_password(menu, BM_PASSWORD_NONE);
	else if (!strcmp(password, "indicator"))
		bm_menu_set_password(menu, BM_PASSWORD_INDICATOR);
	else
		bm_menu_set_password(menu, BM_PASSWORD_HIDE);
#else
	bm_menu_set_password(menu, true);
#endif
	bm_menu_set_line_height(menu, height);
	bm_menu_set_font(menu, font);
	if (ignore_case) {
		bm_menu_set_filter_mode(menu, BM_FILTER_MODE_DMENU_CASE_INSENSITIVE);
	}
	bm_menu_set_lines(menu, list);
	bm_menu_set_wrap(menu, wrap);
#define X(option, help, var, setting) \
	if (var) \
		bm_menu_set_color(menu, setting, var);
	COLORS
#undef X
}
