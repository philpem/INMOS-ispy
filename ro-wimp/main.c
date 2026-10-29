/* Native RISC OS 3.1 WIMP topology and sparse-memory diagnostic shell. */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "kernel.h"
#include "swis.h"
#include "oslib/os.h"
#include "oslib/wimp.h"
#include "oslib/osspriteop.h"
#include "oslib/wimpreadsysinfo.h"
#include "graph.h"
#include "job.h"
#include "hardware.h"
#include "present.h"

static wimp_t task;
static wimp_w network, information, program_info, ram_count;
static struct hardware hardware[2];
static int current_hardware, pending_action, pending_node = -1, ram_node = -1;
static int ram_keep_open;
static unsigned int ram_ceiling = 8192, service_time, service_gap, services;
static char operation_status[256] = "No network discovered";
static FILE *hardware_log;
static int hardware_cancel_test, hardware_quit_test;
static int hardware_quiet_test;
static const char *logged_stage;
static wimp_i bar_icon;
static struct graph graph;
static struct job job;
static int info_node = -1, menu_node = -1, menu_kind;
enum { MENU_NETWORK, MENU_NODE, MENU_ICONBAR };
static int menu_x, menu_y;
static unsigned int sprite_reloads;
static const char *sprite_file;
static char resource_dir[1024];
static const int messages[] = {message_MENU_WARNING, message_MODE_CHANGE, 0};
static int failed;
static int grid_sample, layered_sample;
static int parallel_sample, smoke_expect_empty;
static int startup_example = -1, shown_example = -1;
static os_error last_error;
static const char *error_call;
static int error_line;
static unsigned int redraws, polls, node_redraws;
static unsigned int info_open_poll;
static unsigned int ram_open_poll, ram_open_time;
static int interaction_checks;
static int plot_origin_x, plot_origin_y;
static int smoke_rendered;
static int smoke_expected_failure;
static char smoke_picture[1024];
static FILE *smoke_log;
static wimp_MENU(6) main_menu;
static wimp_MENU(GRAPH_EXAMPLES) examples_menu;
static wimp_MENU(3) layout_menu;
static wimp_MENU(3) processor_menu;
static wimp_MENU(2) iconbar_menu;
static union { int align; char bytes[16384]; } sprite_buffer;
static osspriteop_area *sprites;
static void smoke_pointer(int node, wimp_mouse_state buttons);
static void smoke_check(int condition, const char *message);
/* Templates contain indirected titles; their storage must outlive the windows. */
static union { int align; char bytes[4096]; } templates[4];
static char template_text[4][4096];

static int checked_at(os_error *error, const char *call, int line_number)
{
    if (error == NULL) return 1;
    if (!failed) {
        last_error = *error; error_call = call; error_line = line_number;
    }
    failed = 1;
    job_quit(&job);
    return 0;
}

#define checked(error) checked_at((error), #error, __LINE__)

static int ticks(unsigned int *time)
{
    int now;
    if (!checked(xos_read_monotonic_time(&now))) return 0;
    *time = (unsigned int)now;
    return 1;
}

static int load_window(int index, const char *name, wimp_w *window)
{
    union { int align; char text[12]; } template_name;
    int context, used, data_used;
    os_error error;
    wimp_window *definition = (wimp_window *)templates[index].bytes;
    memset(&template_name, 0, sizeof(template_name));
    strcpy(template_name.text, name);
    /* LoadTemplate bounds the indirected workspace, but has no end pointer
     * for the window block. Query both sizes before allowing it to copy. */
    if (!checked(xwimp_load_template(NULL, NULL, NULL, (byte *)-1,
                  template_name.text, 0, &used, &data_used, &context))) return 0;
    if (context == 0) {
        error.errnum = 1;
        strcpy(error.errmess, "Ispy window template was not found");
        return checked(&error);
    }
    if (used < 0 || (unsigned int)used < offsetof(wimp_window, icons) ||
        (unsigned int)used > sizeof(templates[index].bytes) ||
        data_used < 0 || (unsigned int)data_used > sizeof(template_text[index])) {
        error.errnum = 1;
        strcpy(error.errmess, "Ispy window template exceeds workspace");
        return checked(&error);
    }
    /* The query overwrites the search name; begin the exact lookup again. */
    memset(&template_name, 0, sizeof(template_name));
    strcpy(template_name.text, name);
    if (!checked(xwimp_load_template(definition, template_text[index],
                  template_text[index] + sizeof(template_text[index]),
                  (byte *)-1, template_name.text, 0,
                  &used, &data_used, &context))) return 0;
    if (context == 0) {
        error.errnum = 1;
        strcpy(error.errmess, "Ispy window template was not found");
        return checked(&error);
    }
    return checked(xwimp_create_window(definition, window));
}

static void open_window(wimp_w window)
{
    wimp_window_state state;
    wimp_open open;
    state.w = window;
    if (!checked(xwimp_get_window_state(&state))) return;
    open.w = window;
    open.visible = state.visible;
    open.xscroll = state.xscroll;
    open.yscroll = state.yscroll;
    open.next = wimp_TOP;
    checked(xwimp_open_window(&open));
}

static void text(int x, int y, int width, const char *string)
{
    wimp_icon icon;
    memset(&icon, 0, sizeof(icon));
    icon.extent.x0 = x;
    icon.extent.y0 = y;
    icon.extent.x1 = x + width;
    icon.extent.y1 = y + 36;
    icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED | wimp_ICON_VCENTRED |
                 ((wimp_icon_flags)wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT);
    icon.data.indirected_text.text = (char *)string;
    icon.data.indirected_text.validation = "";
    icon.data.indirected_text.size = strlen(string) + 1;
    checked(xwimp_plot_icon(&icon));
}

static void line(int x0, int y0, int x1, int y1)
{
    checked(xwimp_set_colour(wimp_COLOUR_DARK_BLUE));
    checked(xos_plot((os_plot_code)4, x0+plot_origin_x, y0+plot_origin_y));
    checked(xos_plot((os_plot_code)5, x1+plot_origin_x, y1+plot_origin_y));
}

/* The bitmap and text are separate native icons, as in a Filer display. */
static void node_icon(int x, int y, const char *name, int selected)
{
    wimp_icon icon;
    memset(&icon, 0, sizeof(icon));
    icon.extent.x0 = x-34; icon.extent.x1 = x+34;
    icon.extent.y0 = y-34; icon.extent.y1 = y+34;
    icon.flags = wimp_ICON_SPRITE | wimp_ICON_INDIRECTED |
                 wimp_ICON_HCENTRED | wimp_ICON_VCENTRED;
    if (selected) icon.flags |= wimp_ICON_SELECTED;
    icon.data.indirected_sprite.id = (osspriteop_id)name;
    icon.data.indirected_sprite.area = sprites;
    icon.data.indirected_sprite.size = strlen(name)+1;
    checked(xwimp_plot_icon(&icon));
}

static void node_label(int x, int y, const char *label, int selected)
{
    wimp_icon icon;
    memset(&icon, 0, sizeof(icon));
    icon.extent.x0 = x; icon.extent.x1 = x+176;
    icon.extent.y0 = y; icon.extent.y1 = y+40;
    icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED |
                 wimp_ICON_HCENTRED | wimp_ICON_VCENTRED | wimp_ICON_FILLED |
                 ((wimp_icon_flags)wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT);
    if (selected) icon.flags |= wimp_ICON_SELECTED;
    icon.data.indirected_text.text = (char *)label;
    icon.data.indirected_text.validation = "";
    icon.data.indirected_text.size = strlen(label)+1;
    checked(xwimp_plot_icon(&icon));
}

static void draw_network(void)
{
    int i, j;
    char label[100];
    text(16, -52, graph.width-32, operation_status);
    if (!graph.node_count) {
        text(16, -132, graph.width-32, "Menu: Discover or Count RAM");
        return;
    }
    node_icon(96, graph.host_y, "host", 0);
    node_label(8, graph.host_y-92, "Host", 0);
    for (i = 0; i < graph.edge_count; ++i) {
        const struct graph_edge *e = &graph.edges[i];
        struct graph_path path;
        graph_edge_path(&graph, i, &path);
        for (j = 1; j < path.count; ++j)
            line(path.points[j-1].x, path.points[j-1].y,
                 path.points[j].x, path.points[j].y);
        if (e->a >= 0) {
            sprintf(label, "%d", e->aport);
            text(path.label_a.x, path.label_a.y, 40, label);
        } else text(path.label_a.x, path.label_a.y, 80, "Host");
        sprintf(label, "%d", e->bport);
        text(path.label_b.x, path.label_b.y, 40, label);
    }
    for (i = 0; i < graph.node_count; ++i) {
        const struct graph_node *n = &graph.nodes[i];
        int cx = (n->box.x0+n->box.x1)/2, cy = graph_node_y(n);
        node_icon(cx, cy, "tpnode", n->selected);
        sprintf(label, "%d: %s", n->number, n->type);
        node_label(n->box.x0, cy-92, label, n->selected);
        node_ram_label(n, label);
        node_label(n->box.x0, cy-132, label, 0);
    }
    text(16, 26-graph.height, graph.width-32, "Select: node   Menu: operations");
}

static void display_field(int row, const char *label, const char *value)
{
    wimp_icon icon;
    int y = -60-row*60;
    memset(&icon, 0, sizeof(icon));
    icon.extent.x0 = 8; icon.extent.x1 = 208;
    icon.extent.y0 = y+6; icon.extent.y1 = y+46;
    icon.flags = wimp_ICON_TEXT | wimp_ICON_INDIRECTED | wimp_ICON_VCENTRED |
        wimp_ICON_RJUSTIFIED |
        ((wimp_icon_flags)wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT);
    icon.data.indirected_text.text = (char *)label;
    icon.data.indirected_text.validation = "";
    icon.data.indirected_text.size = strlen(label)+1;
    checked(xwimp_plot_icon(&icon));
    icon.extent.x0 = 216; icon.extent.x1 = 648;
    icon.extent.y0 = y; icon.extent.y1 = y+52;
    icon.flags &= ~wimp_ICON_RJUSTIFIED;
    icon.flags |= wimp_ICON_HCENTRED | wimp_ICON_BORDER | wimp_ICON_FILLED |
        ((wimp_icon_flags)wimp_COLOUR_VERY_LIGHT_GREY << wimp_ICON_BG_COLOUR_SHIFT);
    icon.data.indirected_text.text = (char *)value;
    icon.data.indirected_text.validation = "R2";
    icon.data.indirected_text.size = strlen(value)+1;
    checked(xwimp_plot_icon(&icon));
}

static void draw_information(void)
{
    char value[80], label[16];
    int port, i;
    const struct graph_node *n;
    if (info_node < 0 || info_node >= graph.node_count) return;
    n = &graph.nodes[info_node];
    if (graph.sample) sprintf(value, "%d (%s, sample)", n->number, n->type);
    else sprintf(value, "%d (%s, %d MHz)", n->number, n->type, n->mhz);
    display_field(0, "Node", value);
    node_ram_value(n, 0, value);
    display_field(1, "Internal RAM", value);
    node_ram_value(n, 1, value);
    display_field(2, n->internal_bytes ? "External RAM" : "Detected RAM", value);
    for (port = 0; port < 4; ++port) {
        strcpy(value, "Not connected");
        for (i = 0; i < graph.edge_count; ++i) {
            const struct graph_edge *e = &graph.edges[i];
            if (e->a == info_node && e->aport == port)
                sprintf(value, "Node %d, link %d", graph.nodes[e->b].number, e->bport);
            else if (e->b == info_node && e->bport == port) {
                if (e->a < 0) strcpy(value, "Host");
                else sprintf(value, "Node %d, link %d", graph.nodes[e->a].number, e->aport);
            }
        }
        sprintf(label, "Link %d", port);
        display_field(3+port, label, value);
    }
}

static void redraw(wimp_w window)
{
    wimp_draw draw;
    osbool more;
    draw.w = window;
    if (!checked(xwimp_redraw_window(&draw, &more))) return;
    ++redraws;
    if (window == information) ++node_redraws;
    while (more) {
        /* Wimp_PlotIcon adds the redraw origin itself. Only OS_Plot needs
         * explicit screen coordinates; keep all layout in work-area units. */
        plot_origin_x = draw.box.x0 - draw.xscroll;
        plot_origin_y = draw.box.y1 - draw.yscroll;
        if (window == network) draw_network();
        else if (window == information) draw_information();
        if (!checked(xwimp_get_rectangle(&draw, &more))) break;
    }
}

static void menu_init(wimp_menu *menu, const char *title,
                      const char **entries, int count)
{
    int i;
    memset(menu, 0, wimp_SIZEOF_MENU(count));
    strcpy(menu->title_data.text, title);
    menu->title_fg = wimp_COLOUR_BLACK;
    menu->title_bg = wimp_COLOUR_LIGHT_GREY;
    menu->work_fg = wimp_COLOUR_BLACK;
    menu->work_bg = wimp_COLOUR_WHITE;
    menu->width = 240; menu->height = 44;
    for (i = 0; i < count; ++i) {
        wimp_menu_entry *entry = &menu->entries[i];
        entry->sub_menu = wimp_NO_SUB_MENU;
        entry->icon_flags = wimp_ICON_TEXT | wimp_ICON_FILLED |
            ((wimp_icon_flags)wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT);
        if(strlen(entries[i])>=sizeof(entry->data.text)) {
            entry->icon_flags |= wimp_ICON_INDIRECTED;
            entry->data.indirected_text.text=(char *)entries[i];
            entry->data.indirected_text.validation="";
            entry->data.indirected_text.size=strlen(entries[i])+1;
        } else strcpy(entry->data.text, entries[i]);
    }
    menu->entries[count-1].menu_flags = wimp_MENU_LAST;
}

static void refresh_network(void)
{
    wimp_window_state state;
    int left=0, bottom=-graph.height, right=graph.width, top=0;
    state.w=network;
    if (!checked(xwimp_get_window_state(&state))) return;
    /* Old pixels can survive outside a replacement graph's smaller bounds. */
    if (state.xscroll<left) left=state.xscroll;
    if (state.xscroll+state.visible.x1-state.visible.x0>right)
        right=state.xscroll+state.visible.x1-state.visible.x0;
    if (state.yscroll>top) top=state.yscroll;
    if (state.yscroll-state.visible.y1+state.visible.y0<bottom)
        bottom=state.yscroll-state.visible.y1+state.visible.y0;
    checked(xwimp_force_redraw(network,left,bottom,right,top));
}

static void queue_action(int action, int node)
{
    if (job.busy || job.quitting || pending_action) return;
    pending_action = action; pending_node = node;
}

static void open_ram(int node, int x, int y, int submenu)
{
    if (job.busy || pending_action || job.quitting) return;
    ram_node = node;
    if (submenu) checked(xwimp_create_sub_menu((wimp_menu *)ram_count, x, y));
    else checked(xwimp_create_menu((wimp_menu *)ram_count, x, y));
    checked(xwimp_set_caret_position(ram_count, 3, 0, 0, -1, -1));
    ram_open_poll = polls;
    ticks(&ram_open_time);
}

/* Standalone entry for diagnostics; still use WIMP's transient-menu rules. */
static void offer_ram(int node)
{
    open_ram(node, 480, 800, 0);
}

static void accept_ram(int adjust)
{
    wimp_icon_state state;
    unsigned long value;
    char *end;
    if (job.busy || pending_action || job.quitting) return;
    state.w = ram_count; state.i = 3;
    if (!checked(xwimp_get_icon_state(&state))) return;
    value = strtoul(state.icon.data.indirected_text.text, &end, 10);
    /* WIMP writable strings may end with any control character. */
    if ((unsigned char)*end >= 32 || value < 4 || value > 262144UL || (value & 1)) {
        os_error error;
        error.errnum = 1;
        strcpy(error.errmess, "Enter an even ceiling from 4 to 262144 KiB");
        xwimp_report_error(&error, wimp_ERROR_BOX_OK_ICON, "Ispy", NULL);
        return;
    }
    ram_ceiling = (unsigned int)value;
    ram_keep_open = adjust;
    if (!adjust) checked(xwimp_create_menu(wimp_CLOSE_MENU, 0, 0));
    queue_action(2, ram_node);
}

/* Wimp menu bodies include a 24-unit band for each separator. The title
 * sits above the y passed to CreateMenu and is not part of this height. */
static int iconbar_menu_y(const wimp_menu *menu)
{
    int i, y = 96;
    for (i = 0; ; ++i) {
        y += menu->height + menu->gap;
        if (menu->entries[i].menu_flags & wimp_MENU_SEPARATE) y += 24;
        if (menu->entries[i].menu_flags & wimp_MENU_LAST) break;
    }
    return y;
}

static void show_menu(int kind, int x, int y)
{
    wimp_menu *menu;
    menu_kind = kind;
    if (kind == MENU_NODE) {
        info_node = menu_node;
        checked(xwimp_force_redraw(information, 0, -428, 664, 0));
        menu = (wimp_menu *)&processor_menu;
        processor_menu.entries[1].icon_flags &= ~wimp_ICON_SHADED;
        if (job.busy || pending_action || graph.sample) processor_menu.entries[1].icon_flags |= wimp_ICON_SHADED;
        processor_menu.entries[2].icon_flags &= ~wimp_ICON_SHADED;
        if (!job.busy) processor_menu.entries[2].icon_flags |= wimp_ICON_SHADED;
    } else if (kind == MENU_ICONBAR) {
        menu = (wimp_menu *)&iconbar_menu;
        y = iconbar_menu_y(menu);
    } else {
        menu = (wimp_menu *)&main_menu;
        main_menu.entries[0].icon_flags &= ~wimp_ICON_SHADED;
        main_menu.entries[1].icon_flags &= ~wimp_ICON_SHADED;
        if (job.busy || pending_action) {
            main_menu.entries[0].icon_flags |= wimp_ICON_SHADED;
            main_menu.entries[1].icon_flags |= wimp_ICON_SHADED;
        }
        {
            int i;
            for (i=4;i<6;++i) {
                main_menu.entries[i].icon_flags &= ~wimp_ICON_SHADED;
                if (job.busy || pending_action) main_menu.entries[i].icon_flags |= wimp_ICON_SHADED;
            }
            for (i=0;i<3;++i) {
                layout_menu.entries[i].menu_flags &= ~wimp_MENU_TICKED;
                if (graph.layout_mode==i) layout_menu.entries[i].menu_flags |= wimp_MENU_TICKED;
            }
            for (i=0;i<GRAPH_EXAMPLES;++i) {
                examples_menu.entries[i].menu_flags &= ~wimp_MENU_TICKED;
                if (graph.sample==4 && shown_example==i) examples_menu.entries[i].menu_flags |= wimp_MENU_TICKED;
            }
        }
        main_menu.entries[2].icon_flags &= ~wimp_ICON_SHADED;
        if (!job.busy) main_menu.entries[2].icon_flags |= wimp_ICON_SHADED;
    }
    menu_x = x; menu_y = y;
    checked(xwimp_create_menu(menu, x, y));
}

static void open_info_submenu(int x, int y)
{
    wimp_w window = menu_kind == MENU_ICONBAR ? program_info : information;
    checked(xwimp_create_sub_menu((wimp_menu *)window, x, y));
    info_open_poll = polls;
}

static void pointer_event(const wimp_pointer *pointer)
{
    int hit = -1;
    if (pointer->w == network) {
        wimp_window_state state;
        state.w = network;
        if (!checked(xwimp_get_window_state(&state))) return;
        hit = graph_pointer(&graph, pointer->pos.x, pointer->pos.y,
                            state.visible.x0, state.visible.y1,
                            state.xscroll, state.yscroll);
    }
    if (pointer->w == ram_count &&
        (pointer->buttons & (wimp_CLICK_SELECT | wimp_CLICK_ADJUST))) {
        int adjust = (pointer->buttons & wimp_CLICK_ADJUST) != 0;
        if (pointer->i == 4) {
            if (!adjust) checked(xwimp_create_menu(wimp_CLOSE_MENU, 0, 0));
            else if (!job.busy && !pending_action) {
                wimp_icon_state state;
                state.w = ram_count; state.i = 3;
                if (checked(xwimp_get_icon_state(&state))) {
                    sprintf(state.icon.data.indirected_text.text, "%u", ram_ceiling);
                    checked(xwimp_set_icon_state(ram_count, 3, 0, 0));
                }
            }
        } else if (pointer->i == 5) accept_ram(adjust);
    } else if (pointer->buttons & wimp_CLICK_MENU) {
        if (pointer->w == network) {
            menu_node = hit;
            show_menu(hit >= 0 ? MENU_NODE : MENU_NETWORK,
                      pointer->pos.x-64, pointer->pos.y);
        } else if (pointer->w == wimp_ICON_BAR && pointer->i == bar_icon) {
            show_menu(MENU_ICONBAR, pointer->pos.x-64, pointer->pos.y);
        }
    } else if (pointer->w == network &&
               (pointer->buttons & (wimp_CLICK_SELECT | wimp_CLICK_ADJUST))) {
        graph_select(&graph, hit, (pointer->buttons & wimp_CLICK_ADJUST) != 0);
        refresh_network();
    } else if (pointer->w == wimp_ICON_BAR && pointer->i == bar_icon &&
               (pointer->buttons & wimp_CLICK_SELECT)) open_window(network);
}

static int load_sprites(void)
{
    char *suffix;
    char path[1024];
    const char *dir = resource_dir;
    os_error *error;
    if (!*dir || strlen(dir) > sizeof(path)-16) return 0;
    if (!checked(xwimpreadsysinfo_sprite_suffix(&suffix))) return 0;
    sprite_file = strcmp(suffix, "22") == 0 ? "Sprites22" : "Sprites";
    sprites = (osspriteop_area *)sprite_buffer.bytes;
    sprites->size = sizeof(sprite_buffer);
    sprites->sprite_count = 0; sprites->first = sprites->used = 16;
    sprintf(path, "%s.%s", dir, sprite_file);
    error = xosspriteop_load_sprite_file(osspriteop_USER_AREA, sprites, path);
    if (error != NULL && strcmp(sprite_file, "Sprites") != 0) {
        sprite_file = "Sprites";
        sprites->sprite_count = 0; sprites->first = sprites->used = 16;
        sprintf(path, "%s.Sprites", dir);
        error = xosspriteop_load_sprite_file(osspriteop_USER_AREA, sprites, path);
    }
    if (!checked(error)) return 0;
    ++sprite_reloads;
    return 1;
}

static int screen_size(int *width, int *height)
{
    int xlimit, ylimit, xeig, yeig;
    if (!checked(xos_read_mode_variable((os_mode)-1, os_MODEVAR_XWIND_LIMIT, &xlimit, NULL)) ||
        !checked(xos_read_mode_variable((os_mode)-1, os_MODEVAR_YWIND_LIMIT, &ylimit, NULL)) ||
        !checked(xos_read_mode_variable((os_mode)-1, os_MODEVAR_XEIG_FACTOR, &xeig, NULL)) ||
        !checked(xos_read_mode_variable((os_mode)-1, os_MODEVAR_YEIG_FACTOR, &yeig, NULL)))
        return 0;
    *width = (xlimit+1) << xeig;
    *height = (ylimit+1) << yeig;
    return 1;
}

static void first_window(void)
{
    int screen_width, screen_height, width, height;
    wimp_open open;
    os_box extent;
    if (!screen_size(&screen_width, &screen_height)) return;
    /* Leave room for furniture within the centred quarter-screen footprint. */
    width = screen_width/2-48;
    height = screen_height/2-48;
    if (width < 320) width = 320;
    if (height < 240) height = 240;
    graph_layout(&graph, width);
    extent.x0 = 0; extent.x1 = graph.width;
    extent.y0 = -(height > graph.height ? height : graph.height); extent.y1 = 0;
    if (!checked(xwimp_set_extent(network, &extent))) return;
    open.w = network;
    open.visible.x0 = (screen_width-width)/2;
    open.visible.x1 = open.visible.x0+width;
    open.visible.y0 = (screen_height-height)/2;
    open.visible.y1 = open.visible.y0+height;
    open.xscroll = 0;open.yscroll = graph.host_y+210;
    if(open.yscroll<height-graph.height)open.yscroll=height-graph.height;
    if(open.yscroll>0)open.yscroll=0;
    open.next = wimp_TOP;
    checked(xwimp_open_window(&open));
}

static void arrange_network(void)
{
    wimp_window_state state;
    wimp_open open;
    os_box extent;
    int w,h;
    state.w=network;
    if(!checked(xwimp_get_window_state(&state)))return;
    w=state.visible.x1-state.visible.x0;h=state.visible.y1-state.visible.y0;
    graph_layout(&graph,w);
    extent.x0=0;extent.x1=graph.width;extent.y0=-(graph.height>h?graph.height:h);extent.y1=0;
    if(!checked(xwimp_set_extent(network,&extent)))return;
    open.w=network;open.visible=state.visible;open.next=wimp_TOP;open.xscroll=0;
    open.yscroll=graph.host_y+210;
    if(open.yscroll<h-graph.height)open.yscroll=h-graph.height;
    if(open.yscroll>0)open.yscroll=0;
    checked(xwimp_open_window(&open));
    if(graph.sample==4) sprintf(operation_status,"Example: %s (synthetic) - %s",
        graph_example_name(shown_example),graph_layout_name(&graph));
    refresh_network();
}

static void choose_example(int example)
{
    if(job.busy || pending_action || job.quitting || example<0 || example>=GRAPH_EXAMPLES)return;
    checked(xwimp_create_menu(wimp_CLOSE_MENU,0,0));
    menu_node=info_node=ram_node=-1;shown_example=example;
    if(graph_example(&graph,example))arrange_network();
}

static void choose_layout(int mode)
{
    if(job.busy || pending_action || job.quitting || mode<0 || mode>2)return;
    checked(xwimp_create_menu(wimp_CLOSE_MENU,0,0));
    graph.layout_mode=mode;arrange_network();
}

static void event(wimp_event_no reason, wimp_block *block)
{
    switch (reason) {
    case wimp_REDRAW_WINDOW_REQUEST: redraw(block->redraw.w); break;
    case wimp_OPEN_WINDOW_REQUEST:
        checked(xwimp_open_window(&block->open)); break;
    case wimp_CLOSE_WINDOW_REQUEST:
        /* Closing a view never tears down a running operation. */
        checked(xwimp_close_window(block->close.w)); break;
    case wimp_MOUSE_CLICK: pointer_event(&block->pointer); break;
    case wimp_MENU_SELECTION:
        if (((menu_kind == MENU_NODE || menu_kind == MENU_ICONBAR) &&
             block->selection.items[0] == 0) ||
            ((menu_kind == MENU_NETWORK || menu_kind == MENU_NODE) &&
             block->selection.items[0] == 1)) {
            /* Arrow traversal is handled by Message_MenuWarning below. */
        } else {
            if (menu_kind == MENU_NODE && block->selection.items[0] == 2)
                job_cancel(&job);
            else if (menu_kind == MENU_NETWORK) {
                if (block->selection.items[0] == 0) queue_action(1, -1);
                else if (block->selection.items[0] == 2) job_cancel(&job);
                else if (block->selection.items[0] == 4) choose_example(block->selection.items[1]);
                else if (block->selection.items[0] == 5) choose_layout(block->selection.items[1]);
                else if (block->selection.items[0] == 3) {
                    graph_select(&graph, -1, 0);
                    refresh_network();
                }
            } else if (menu_kind == MENU_ICONBAR && block->selection.items[0] == 1)
                job_quit(&job);
            /* Keep the menu in place after choosing an item with Adjust. */
            if (!job.quitting) {
                wimp_pointer pointer;
                if (checked(xwimp_get_pointer_info(&pointer)) &&
                    (pointer.buttons & wimp_CLICK_ADJUST))
                    show_menu(menu_kind, menu_x, menu_y);
            }
        }
        break;
    case wimp_KEY_PRESSED:
        if (block->key.w == ram_count && block->key.c == 13) accept_ram(0);
        else if (block->key.w == ram_count && block->key.c == wimp_KEY_ESCAPE)
            checked(xwimp_create_menu(wimp_CLOSE_MENU, 0, 0));
        else if (block->key.c == wimp_KEY_ESCAPE && job.busy) job_cancel(&job);
        else checked(xwimp_process_key(block->key.c));
        break;
    case wimp_USER_MESSAGE:
    case wimp_USER_MESSAGE_RECORDED:
        if (block->message.action == 0) job_quit(&job);
        else if (block->message.action == message_MENU_WARNING) {
            wimp_message_menu_warning *warning =
                (wimp_message_menu_warning *)&block->message.data;
            wimp_w window = menu_kind == MENU_ICONBAR ? program_info : information;
            if ((menu_kind == MENU_ICONBAR || menu_kind == MENU_NODE) &&
                warning->selection.items[0] == 0 && warning->sub_menu == (wimp_menu *)window)
                open_info_submenu(warning->pos.x, warning->pos.y);
            else if ((menu_kind == MENU_NETWORK || menu_kind == MENU_NODE) &&
                warning->selection.items[0] == 1 &&
                warning->sub_menu == (wimp_menu *)ram_count &&
                (menu_kind == MENU_NETWORK ||
                 (!graph.sample && menu_node >= 0 && menu_node < graph.node_count)))
                open_ram(menu_kind == MENU_NETWORK ? -1 : graph.nodes[menu_node].number,
                         warning->pos.x, warning->pos.y, 1);
        } else if (block->message.action == message_MODE_CHANGE) {
            if (load_sprites()) {
                refresh_network();
                checked(xwimp_set_icon_state(wimp_ICON_BAR, bar_icon, 0, 0));
            }
        }
        break;
    }
}

/* Called from link readiness loops. Poll without sleeping: the hardware has
 * work to do. Ownership remains held across every nested event dispatch. */
static int hardware_service(void)
{
    unsigned int now, gap;
    wimp_block block;
    wimp_event_no reason;
    if (smoke_expect_empty) {
        smoke_check(job.busy && !graph.sample && !graph.node_count &&
                    !graph.edge_count && menu_node == -1 && info_node == -1,
                    "Demonstration graph survived into a hardware callback");
        smoke_expect_empty = 0;
    }
    if (hardware_quiet_test && hardware_log && logged_stage != hardware_stage()) {
        logged_stage = hardware_stage();
        fprintf(hardware_log, "hardware_stage_enter=%s\n", logged_stage);
        fflush(hardware_log);
    }
    if (!ticks(&now)) return 1;
    gap = now-service_time;
    if (gap >= 2) {
        if (gap > service_gap) service_gap = gap;
        service_time = now; ++services;
        if (!checked(xwimp_poll_idle(0, &block, (os_t)now, NULL, &reason))) return 1;
        ++polls; event(reason, &block);
        if (hardware_fault_read_blocked && hardware_cancel_test) {
            memset(&block, 0, sizeof(block));
            block.key.w = network; block.key.c = wimp_KEY_ESCAPE;
            event(wimp_KEY_PRESSED, &block);
        }
        if (hardware_fault_read_blocked && hardware_quit_test) {
            memset(&block, 0, sizeof(block));
            event(wimp_USER_MESSAGE, &block);
        }
    }
    return job.cancel || job.quitting || failed;
}

static void run_pending(void)
{
    int action = pending_action, node = pending_node, ok, next = 1-current_hardware;
    const struct hardware *expected = graph.sample || !graph.node_count ?
        NULL : &hardware[current_hardware];
    unsigned int begun = 0, ended = 0;
    if (!action || !job_begin(&job)) return;
    pending_action = 0;
    if (!ram_keep_open) checked(xwimp_create_menu(wimp_CLOSE_MENU, 0, 0));
    else {
        checked(xwimp_set_icon_state(ram_count, 3, wimp_ICON_SHADED, wimp_ICON_SHADED));
        checked(xwimp_set_icon_state(ram_count, 5, wimp_ICON_SHADED, wimp_ICON_SHADED));
    }
    /* Keep keyboard events in this task after closing the writable dialogue.
     * PRM Wimp_SetCaretPosition: height bit 25 makes the caret invisible. */
    checked(xwimp_set_caret_position(network, -1, 0, 0, 1 << 25, 0));
    if (graph.sample) {
        int width = graph.width, mode=graph.layout_mode;
        graph_empty(&graph);graph.layout_mode=mode;shown_example=-1;
        graph_layout(&graph, width);
        info_node = menu_node = -1;
        checked(xwimp_close_window(information));
        arrange_network();
    }
    if (hardware_log) {
        wimp_caret caret;
        fprintf(hardware_log, "hardware_begin action=%d node=%d ceiling=%u quiet=%d\n",
                action, node, ram_ceiling, hardware_quiet_test);
        if (checked(xwimp_get_caret_position(&caret)))
            fprintf(hardware_log, "keyboard_focus=%d\n", caret.w == network);
    }
    strcpy(operation_status, action == 1 ? "Discovering... Escape cancels" : "Counting RAM... Escape cancels");
    refresh_network();
    ticks(&service_time); begun = service_time; service_gap = services = 0;
    ok = hardware_run(&hardware[next], action == 2, node, ram_ceiling,
                      hardware_service, expected);
    job_end(&job, ok && action == 1);
    if (ram_keep_open) {
        wimp_window_state state;
        /* A rejected identity must require a new menu target. A retained
         * dialogue must not silently retry against the replacement map. */
        if (!ok || job.quitting) checked(xwimp_create_menu(wimp_CLOSE_MENU, 0, 0));
        checked(xwimp_set_icon_state(ram_count, 3, 0, wimp_ICON_SHADED));
        checked(xwimp_set_icon_state(ram_count, 5, 0, wimp_ICON_SHADED));
        state.w = ram_count;
        if (checked(xwimp_get_window_state(&state)) && (state.flags & wimp_WINDOW_OPEN))
            checked(xwimp_set_caret_position(ram_count, 3, 0, 0, -1, -1));
        ram_keep_open = 0;
    }
    ticks(&ended);
    if (hardware[next].ready) {
        int i;
        current_hardware = next;
        hardware_graph(&hardware[next], &graph);
        info_node = menu_node = -1;
        /* Adjust can retain the processor menu across fresh discovery.
         * Rebind its target by processor number, not the old array index. */
        if (menu_kind == MENU_NODE && node >= 0) {
            for (i = 0; i < graph.node_count; ++i)
                if (graph.nodes[i].number == node) info_node = menu_node = i;
        }
        checked(xwimp_close_window(information));
        arrange_network();
    }
    if (ok) sprintf(operation_status, "%d processors%s", graph.node_count,
                     action == 2 ? "; RAM count finished" : "; discovered");
    else strcpy(operation_status, hardware[next].error);
    refresh_network();
    if (hardware_log) {
        int i;
        fprintf(hardware_log, "hardware_stage=%s\n", hardware_stage());
        fprintf(hardware_log, "action=%d node=%d ok=%d count=%d edges=%d services=%u max_gap_cs=%u elapsed_cs=%u busy=%d quitting=%d status=%s\n",
            action, node, ok, graph.node_count, graph.edge_count, services, service_gap,
            ended-begun, job.busy, job.quitting, operation_status);
        if (failed) {
            fprintf(hardware_log, "wimp_error=%s error_line=%d error_call=%s\n",
                    last_error.errmess, error_line,
                    error_call == NULL ? "unknown" : error_call);
        }
        for (i = 0; i < graph.node_count; ++i)
            fprintf(hardware_log, "node=%d type=%s mhz=%d bytes=%lu state=%d errors=%d\n",
                graph.nodes[i].number, graph.nodes[i].type, graph.nodes[i].mhz,
                graph.nodes[i].ram_bytes, graph.nodes[i].ram_state, graph.nodes[i].ram_errors);
        for (i = 0; i < graph.node_count; ++i)
            fprintf(hardware_log, "timing_node=%d internal_cycles=%d external_cycles=%d\n",
                graph.nodes[i].number, graph.nodes[i].internal_cycles, graph.nodes[i].external_cycles);
        for (i = 0; i < graph.edge_count; ++i)
            fprintf(hardware_log, "edge=%d:%d-%d:%d\n", graph.edges[i].a, graph.edges[i].aport,
                    graph.edges[i].b, graph.edges[i].bport);
        if (hardware[next].ready && !job.quitting && !hardware_quiet_test) {
            for (i = 0; i < graph.node_count; ++i) {
                smoke_pointer(i, wimp_CLICK_MENU);
                smoke_check(menu_kind == MENU_NODE && menu_node == i && info_node == i,
                            "Menu failed to target a live node after the operation");
            }
            checked(xwimp_create_menu(wimp_CLOSE_MENU, 0, 0));
            fprintf(hardware_log, "live_node_menus=%s\n", failed ? "FAIL" : "OK");
        }
        if (hardware_quiet_test) fprintf(hardware_log, "live_node_menus=SKIPPED\n");
        fflush(hardware_log);
        job_quit(&job);
    } else if (!ok && !job.quitting && smoke_log) {
        if (!smoke_expected_failure) smoke_check(0, hardware[next].error);
    } else if (!ok && !job.quitting) {
        os_error error;
        error.errnum = 1;
        strncpy(error.errmess, operation_status, sizeof(error.errmess)-1);
        error.errmess[sizeof(error.errmess)-1] = 0;
        xwimp_report_error(&error, wimp_ERROR_BOX_OK_ICON, "Ispy", NULL);
    }
}

static int start(void)
{
    wimp_icon_create icon;
    const char *main_entries[] = {"Discover", "Count RAM", "Cancel", "Clear", "Examples", "Layout"};
    const char *layout_entries[] = {"Automatic", "Layered", "Host distance"};
    const char *example_entries[GRAPH_EXAMPLES];
    int example;
    const char *bar_entries[] = {"Info", "Quit"};
    const char *node_entries[] = {"Info", "Count RAM", "Cancel"};
    char path[1024];
    const char *dir = getenv("Ispy$Dir");
    if (dir == NULL || strlen(dir) > sizeof(path)-16) return 0;
    /* Each launch sets a shared system variable. Keep this task's own copy
     * before another installation can change it during a later WIMP poll. */
    strcpy(resource_dir, dir);
    sprintf(path, "%s.Templates", resource_dir);
    if (!checked(xwimp_open_template(path))) return 0;
    if (!load_window(0, "Network", &network) ||
        !load_window(1, "NodeInfo", &information) ||
        !load_window(2, "ProgInfo", &program_info) ||
        !load_window(3, "RAMCount", &ram_count)) {
        checked(xwimp_close_template());
        return 0;
    }
    if (!checked(xwimp_close_template())) return 0;
    menu_init((wimp_menu *)&main_menu, "Network", main_entries, 6);
    for(example=0;example<GRAPH_EXAMPLES;++example)example_entries[example]=graph_example_name(example);
    menu_init((wimp_menu *)&examples_menu,"Examples",example_entries,GRAPH_EXAMPLES);
    menu_init((wimp_menu *)&layout_menu,"Layout",layout_entries,3);
    examples_menu.width=288;layout_menu.width=288;
    main_menu.entries[4].sub_menu=(wimp_menu *)&examples_menu;
    main_menu.entries[4].menu_flags |= wimp_MENU_SEPARATE;
    main_menu.entries[5].sub_menu=(wimp_menu *)&layout_menu;
    main_menu.entries[1].sub_menu = (wimp_menu *)ram_count;
    main_menu.entries[1].menu_flags |= wimp_MENU_GIVE_WARNING;
    menu_init((wimp_menu *)&iconbar_menu, "Ispy", bar_entries, 2);
    iconbar_menu.entries[0].sub_menu = (wimp_menu *)program_info;
    iconbar_menu.entries[0].menu_flags |= wimp_MENU_SEPARATE | wimp_MENU_GIVE_WARNING;
    menu_init((wimp_menu *)&processor_menu, "Processor", node_entries, 3);
    processor_menu.entries[0].sub_menu = (wimp_menu *)information;
    processor_menu.entries[0].menu_flags |= wimp_MENU_SEPARATE | wimp_MENU_GIVE_WARNING;
    processor_menu.entries[1].sub_menu = (wimp_menu *)ram_count;
    processor_menu.entries[1].menu_flags |= wimp_MENU_GIVE_WARNING;
    if (!load_sprites()) return 0;
    memset(&icon, 0, sizeof(icon));
    icon.w = wimp_ICON_BAR_RIGHT;
    icon.icon.extent.x1 = 100;
    icon.icon.extent.y1 = 68;
    icon.icon.flags = wimp_ICON_SPRITE | wimp_ICON_INDIRECTED |
        wimp_ICON_HCENTRED | wimp_ICON_VCENTRED |
        (wimp_BUTTON_CLICK << wimp_ICON_BUTTON_TYPE_SHIFT) |
        ((wimp_icon_flags)wimp_COLOUR_BLACK << wimp_ICON_FG_COLOUR_SHIFT);
    icon.icon.data.indirected_sprite.id = (osspriteop_id)"!ispy";
    icon.icon.data.indirected_sprite.area = (osspriteop_area *)1;
    icon.icon.data.indirected_sprite.size = 6;
    if (!checked(xwimp_create_icon(&icon, &bar_icon))) return 0;
    if (startup_example>=0) {
        graph_empty(&graph);shown_example=startup_example;
        graph_example(&graph,shown_example);
        sprintf(operation_status,"Example: %s (synthetic) - %s",graph_example_name(shown_example),graph_layout_name(&graph));
    } else if (layered_sample) {
        graph_sample_layered(&graph);
        strcpy(operation_status, "Sample automatic layout (not discovered)");
    } else if (grid_sample) {
        graph_sample_grid(&graph);
        strcpy(operation_status, "Sample 3x3 grid (not discovered)");
    } else if (parallel_sample) {
        graph_sample(&graph);
        strcpy(operation_status, "Sample network (not discovered)");
    } else graph_empty(&graph);
    first_window();
    return !failed;
}

/* Exercise real event handlers without requiring remote mouse injection. */
static void smoke_check(int condition, const char *message)
{
    os_error error;
    if (condition) { ++interaction_checks; return; }
    error.errnum = 1;
    strcpy(error.errmess, message);
    checked(&error);
}

static void smoke_pointer(int node, wimp_mouse_state buttons)
{
    wimp_window_state state;
    wimp_pointer pointer;
    int x = 24, y = -80;
    state.w = network;
    if (!checked(xwimp_get_window_state(&state))) return;
    if (node >= 0) {
        const struct graph_box *b = &graph.nodes[node].box;
        x = (b->x0+b->x1)/2; y = (b->y0+b->y1)/2;
    }
    memset(&pointer, 0, sizeof(pointer));
    pointer.w = network;
    pointer.buttons = buttons;
    pointer.pos.x = x+state.visible.x0-state.xscroll;
    pointer.pos.y = y+state.visible.y1-state.yscroll;
    pointer_event(&pointer);
}

static void smoke_open_submenu(int item)
{
    wimp_block block;
    wimp_message_menu_warning *warning;
    memset(&block, 0, sizeof(block));
    block.message.action = message_MENU_WARNING;
    warning = (wimp_message_menu_warning *)&block.message.data;
    warning->sub_menu = (wimp_menu *)(item == 1 ? ram_count :
        (menu_kind == MENU_ICONBAR ? program_info : information));
    warning->pos.x = menu_x+288;
    warning->pos.y = menu_y;
    warning->selection.items[0] = item;
    warning->selection.items[1] = -1;
    block.message.size = sizeof(wimp_full_message_menu_warning);
    checked(xwimp_send_message(wimp_USER_MESSAGE, &block.message, task));
}

static void smoke_open_info(void)
{
    smoke_open_submenu(0);
}

/* Let WIMP deliver redraws and submenu warnings between test actions. */
static void smoke_wait_events(void)
{
    unsigned int begun = 0, now = 0;
    wimp_block block;
    wimp_event_no reason;
    if (!ticks(&begun)) return;
    now = begun;
    while (!failed && !job.quitting && now-begun < 20) {
        if (!checked(xwimp_poll_idle(0, &block, (os_t)(now+1), NULL, &reason))) return;
        ++polls;
        event(reason, &block);
        if (!ticks(&now)) return;
    }
}

/* Exercise a reload after another launch has changed the system variable.
 * Restore it before yielding to WIMP; only our handler sees the test message. */
static void smoke_resource_directory(void)
{
    static const char wrong[] = "IspyPathNotFound:";
    char saved[1024];
    const char *value = getenv("Ispy$Dir");
    unsigned int before = sprite_reloads;
    wimp_block block;
    wimp_window_state state;
    wimp_w windows[4];
    int i, changed_seen;
    if (!value || strlen(value) >= sizeof(saved)) {
        smoke_check(0, "Cannot save the launch variable for the path check");
        return;
    }
    strcpy(saved, value);
    if (!checked(xos_set_var_val("Ispy$Dir", (const byte *)wrong, sizeof(wrong)-1,
                 0, os_VARTYPE_LITERAL_STRING, NULL, NULL))) return;
    value = getenv("Ispy$Dir");
    changed_seen = value != NULL && strcmp(value, wrong) == 0;
    memset(&block, 0, sizeof(block));
    block.message.action = message_MODE_CHANGE;
    event(wimp_USER_MESSAGE, &block);
    /* Always restore, including when the reload reported an error. */
    checked(xos_set_var_val("Ispy$Dir", (const byte *)saved, strlen(saved),
            0, os_VARTYPE_LITERAL_STRING, NULL, NULL));
    value = getenv("Ispy$Dir");
    smoke_check(changed_seen, "Path check did not change the shared variable");
    smoke_check(value != NULL && strcmp(value, saved) == 0,
                "Path check did not restore the shared variable");
    smoke_check(!failed && sprite_reloads == before+1 &&
                strcmp(resource_dir, saved) == 0,
                "Sprite reload followed another installation's path");
    windows[0] = network; windows[1] = information;
    windows[2] = program_info; windows[3] = ram_count;
    for (i = 0; i < 4 && !failed; ++i) {
        state.w = windows[i];
        checked(xwimp_get_window_state(&state));
    }
    fprintf(smoke_log, "resource_dir=%s shared_change_seen=%d reloads=%u restored=%d\n",
            resource_dir, changed_seen, sprite_reloads,
            value != NULL && strcmp(value, saved) == 0);
}

/* Exercise retained-dialogue failure paths using the real operation/event
 * boundary. The changed-route case alters only the expected snapshot, not
 * physical connections; readiness faults are scoped to these test scans. */
static void smoke_failed_counts(void)
{
    int fault;
    for (fault = 0; fault < 3 && !failed; ++fault) {
        int previous_hardware = current_hardware;
        unsigned char route = hardware[previous_hardware].routes[1][0];
        wimp_pointer pointer;
        wimp_window_state state;
        wimp_icon_state icon;
        smoke_pointer(1, wimp_CLICK_MENU);
        smoke_open_submenu(1);
        smoke_wait_events();
        if (failed) return;
        if (fault == 0) hardware[previous_hardware].routes[1][0] ^= 1;
        hardware_fault_stall = fault != 0;
        hardware_cancel_test = fault == 1;
        memset(&pointer, 0, sizeof(pointer));
        pointer.w = ram_count; pointer.i = 5; pointer.buttons = wimp_CLICK_ADJUST;
        pointer_event(&pointer);
        smoke_check(pending_action == 2 && pending_node == 1 && ram_keep_open,
                    "Failed-scan diagnostic did not queue Adjust Count");
        smoke_expected_failure = 1;
        run_pending();
        smoke_expected_failure = hardware_fault_stall = hardware_cancel_test = 0;
        hardware[previous_hardware].routes[1][0] = route;
        if (failed) return;
        fprintf(smoke_log, "retained_fault=%d stage=%s status=%s max_gap_cs=%u\n",
                fault, hardware_stage(), operation_status, service_gap);
        fflush(smoke_log);
        smoke_check(strstr(operation_status, fault == 0 ? "changed" :
                    fault == 1 ? "Cancelled" : "Incomplete RAM reply") != NULL,
                    "Retained-dialogue fault did not produce its expected failure");
        state.w = ram_count;
        if (checked(xwimp_get_window_state(&state)))
            smoke_check(!(state.flags & wimp_WINDOW_OPEN) && !job.busy &&
                        !pending_action && !ram_keep_open,
                        "Failed Adjust Count retained a stale dialogue or owner");
        icon.w = ram_count; icon.i = 5;
        if (checked(xwimp_get_icon_state(&icon)))
            smoke_check(!(icon.icon.flags & wimp_ICON_SHADED),
                        "Failed Adjust Count left Count shaded for the next use");
        smoke_pointer(1, wimp_CLICK_MENU);
        smoke_open_info();
        smoke_wait_events();
        state.w = information;
        if (checked(xwimp_get_window_state(&state)))
            smoke_check((state.flags & wimp_WINDOW_OPEN) && info_node == 1,
                        "Node Info failed after retained-dialogue failure");
        checked(xwimp_create_menu(wimp_CLOSE_MENU, 0, 0));
    }
    /* Establish successful rediscovery/count recovery and restore live data
     * for the ordinary UI checks that follow. */
    if (!failed) {
        queue_action(2, -1);
        run_pending();
        smoke_check(!failed && !job.busy && graph.node_count == 2 &&
                    graph.nodes[0].ram_state == 3 && graph.nodes[1].ram_state == 3,
                    "RAM counting failed to recover after retained-dialogue faults");
    }
}

static void smoke_repeat_counts(void)
{
    int cycle, i;
    queue_action(1, -1);
    run_pending();
    smoke_check(!failed && !graph.sample && graph.node_count == 2,
                "Repeat-count check requires the live two-node network");
    if (failed) return;
    for (cycle = 0; cycle < 3 && !failed; ++cycle) {
        int node = cycle == 1 ? 1 : -1;
        wimp_pointer pointer;
        wimp_window_state state;
        wimp_w windows[4];
        windows[0] = network; windows[1] = information;
        windows[2] = program_info; windows[3] = ram_count;
        fprintf(smoke_log, "repeat_cycle=%d node=%d begin\n", cycle, node);
        fflush(smoke_log);
        smoke_pointer(node, wimp_CLICK_MENU);
        smoke_open_submenu(1);
        smoke_wait_events();
        if (failed) return;
        state.w = ram_count;
        if (checked(xwimp_get_window_state(&state)))
            smoke_check((state.flags & wimp_WINDOW_OPEN) && ram_node == node,
                        "Count RAM submenu opened for the wrong target");
        if (cycle == 0) {
            wimp_block block;
            wimp_caret caret;
            if (checked(xwimp_get_caret_position(&caret)))
                smoke_check(caret.w == ram_count && caret.i == 3,
                            "Count RAM submenu did not focus its writable field");
            memset(&pointer, 0, sizeof(pointer));
            pointer.w = ram_count; pointer.i = 4; pointer.buttons = wimp_CLICK_SELECT;
            pointer_event(&pointer);
            state.w = ram_count;
            if (checked(xwimp_get_window_state(&state)))
                smoke_check(!(state.flags & wimp_WINDOW_OPEN) && !pending_action,
                            "Cancel did not dismiss Count RAM without scanning");
            smoke_pointer(-1, wimp_CLICK_MENU);
            smoke_open_submenu(1);
            smoke_wait_events();
            memset(&block, 0, sizeof(block));
            block.key.w = ram_count; block.key.c = wimp_KEY_ESCAPE;
            event(wimp_KEY_PRESSED, &block);
            state.w = ram_count;
            if (checked(xwimp_get_window_state(&state)))
                smoke_check(!(state.flags & wimp_WINDOW_OPEN) && !pending_action,
                            "Escape did not dismiss Count RAM without scanning");
            smoke_pointer(-1, wimp_CLICK_MENU);
            smoke_open_submenu(1);
            smoke_wait_events();
        }
        memset(&pointer, 0, sizeof(pointer));
        pointer.w = ram_count; pointer.i = 5;
        pointer.buttons = cycle == 1 ? wimp_CLICK_ADJUST : wimp_CLICK_SELECT;
        if (cycle == 2) {
            wimp_block block;
            memset(&block, 0, sizeof(block));
            block.key.w = ram_count; block.key.c = 13;
            event(wimp_KEY_PRESSED, &block);
        } else pointer_event(&pointer);
        smoke_check(pending_action == 2 && pending_node == node && !job.busy,
                    "Count button did not queue the repeat operation");
        if (failed) return;
        run_pending();
        if (failed) return;
        state.w = ram_count;
        if (checked(xwimp_get_window_state(&state)))
            smoke_check(((state.flags & wimp_WINDOW_OPEN) != 0) == (cycle == 1),
                        "Count Select/Adjust did not dismiss/retain its submenu");
        if (cycle == 1) {
            wimp_caret caret;
            if (checked(xwimp_get_caret_position(&caret)))
                smoke_check(caret.w == ram_count && caret.i == 3 &&
                            menu_node >= 0 && graph.nodes[menu_node].number == node,
                            "Retained Count RAM lost keyboard focus or node context");
        }
        checked(xwimp_create_menu(wimp_CLOSE_MENU, 0, 0));
        smoke_check(!job.busy && !job.quitting && graph.node_count == 2,
                    "Repeated scan lost operation ownership or topology");
        for (i = 0; i < graph.node_count; ++i) {
            struct graph_node *n = &graph.nodes[i];
            if (node < 0 || n->number == node)
                smoke_check(n->ram_state == 3 && !n->ram_errors &&
                            node_external_bytes(n) == 4194304UL,
                            "Repeated scan lost a completed 4 MiB RAM result");
            else smoke_check(n->ram_state == 0,
                             "Selected-node count tested another processor");
        }
        for (i = 0; i < 4 && !failed; ++i) {
            state.w = windows[i];
            fprintf(smoke_log, "repeat_window=%d handle=%d\n", i, (int)windows[i]);
            fflush(smoke_log);
            if (!checked(xwimp_get_window_state(&state))) return;
        }
        smoke_wait_events();
        for (i = 0; i < graph.node_count && !failed; ++i) {
            smoke_pointer(i, wimp_CLICK_MENU);
            smoke_check(menu_kind == MENU_NODE && menu_node == i && info_node == i,
                        "Repeated scan broke node Menu targeting");
            smoke_open_info();
            smoke_wait_events();
            state.w = information;
            if (checked(xwimp_get_window_state(&state)))
                smoke_check((state.flags & wimp_WINDOW_OPEN) != 0,
                            "Node Info did not open after a repeated scan");
            checked(xwimp_create_menu(wimp_CLOSE_MENU, 0, 0));
        }
        fprintf(smoke_log, "repeat_cycle=%d complete max_gap_cs=%u\n", cycle, service_gap);
        fflush(smoke_log);
    }
    if (!failed) smoke_failed_counts();
}

static void smoke_geometry(void)
{
    wimp_window_state state;
    wimp_window *definition = (wimp_window *)templates[2].bytes;
    int width, height;
    state.w = network;
    if (!screen_size(&width, &height) || !checked(xwimp_get_window_state(&state))) return;
    smoke_check(abs(state.visible.x0+state.visible.x1-width) <= 8 &&
                abs(state.visible.y0+state.visible.y1-height) <= 8,
                "Initial window is not centred");
    smoke_check(state.visible.x1-state.visible.x0+48 <= width/2+8 &&
                state.visible.y1-state.visible.y0+48 <= height/2+8,
                "Initial window exceeds its quarter-screen footprint");
    smoke_check(definition->extent.x1 == 584 && definition->extent.y0 == -308,
                "ProgInfo template dimensions are wrong");
}

/* Check pixels, not just successful SWI returns: an off-screen PlotIcon can
 * succeed while rendering nothing in the window. Use a blank work-area point
 * as the background reference so the check is independent of screen palette. */
static int smoke_ink(int ox, int oy, int x0, int y0, int x1, int y1)
{
    os_gcol background, colour;
    os_tint background_tint, tint;
    osbool outside;
    int x, y, ink = 0;
    if (!checked(xos_read_point(ox+340, oy-90, &background,
                                &background_tint, &outside)) || outside) return 0;
    for (y = y0; y < y1; y += 4)
        for (x = x0; x < x1; x += 4) {
            if (!checked(xos_read_point(ox+x, oy+y, &colour, &tint, &outside)))
                return 0;
            if (!outside && (colour != background || tint != background_tint)) ++ink;
        }
    return ink;
}

static int smoke_sprite_ink, smoke_text_ink;

static void smoke_capture(wimp_w window, char suffix)
{
    wimp_window_state state;
    osspriteop_area *capture;
    char path[1024];
    int width, height, x0, y0, x1, y1;
    state.w = window;
    if (!screen_size(&width, &height) || !checked(xwimp_get_window_state(&state))) return;
    x0 = state.visible.x0-4; y0 = state.visible.y0-4;
    x1 = state.visible.x1+3; y1 = state.visible.y1+43;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= width) x1 = width-1;
    if (y1 >= height) y1 = height-1;
    capture = (osspriteop_area *)calloc(1, 128*1024);
    if (capture == NULL) { smoke_check(0, "No memory for dialogue evidence"); return; }
    capture->size = 128*1024;
    capture->first = capture->used = 16;
    strcpy(path, smoke_picture);
    path[strlen(path)-1] = suffix;
    if (checked(xosspriteop_get_sprite_user_coords(osspriteop_USER_AREA,
        capture, "dialogue", 1, x0, y0, x1, y1)))
        checked(xosspriteop_save_sprite_file(osspriteop_USER_AREA, capture, path));
    free(capture);
}

static wimp_open grid_restore;
static int grid_target;

static void smoke_grid_begin(void)
{
    wimp_window_state state;
    wimp_open open;
    state.w = network;
    if (!checked(xwimp_get_window_state(&state))) return;
    grid_restore.w = network; grid_restore.visible = state.visible;
    grid_restore.next = wimp_TOP;
    grid_restore.xscroll = state.xscroll; grid_restore.yscroll = state.yscroll;
    open = grid_restore;
    grid_target = 0;
    {
        int i, w = state.visible.x1-state.visible.x0;
        int h = state.visible.y1-state.visible.y0;
        for (i = 1; i < graph.node_count; ++i)
            if (graph_node_y(&graph.nodes[i]) <= graph_node_y(&graph.nodes[grid_target]))
                grid_target = i;
        open.xscroll = graph.nodes[grid_target].box.x0+88-w/2;
        if (open.xscroll > graph.width-w) open.xscroll = graph.width-w;
        if (open.xscroll < 0) open.xscroll = 0;
        open.yscroll = graph_node_y(&graph.nodes[grid_target])+210;
        if (open.yscroll < h-graph.height) open.yscroll = h-graph.height;
        if (open.yscroll > 0) open.yscroll = 0;
    }
    checked(xwimp_open_window(&open));
    refresh_network();
}

static void smoke_grid_end(void)
{
    wimp_window_state state;
    int x, y;
    state.w = network;
    if (!checked(xwimp_get_window_state(&state))) return;
    x = (graph.nodes[grid_target].box.x0+graph.nodes[grid_target].box.x1)/2-state.xscroll;
    y = graph_node_y(&graph.nodes[grid_target])-state.yscroll;
    smoke_check(x >= 34 && x+34 < state.visible.x1-state.visible.x0 &&
                y <= -34 && y-144 > state.visible.y0-state.visible.y1,
                "Scrolled grid node is outside the visible work area");
    smoke_capture(network, 'g');
    smoke_check(graph.node_count == 9 && graph.edge_count == 13,
                "Grid fixture lost nodes or connections");
    smoke_pointer(grid_target, wimp_CLICK_SELECT);
    smoke_check(graph.nodes[grid_target].selected && !graph.nodes[0].selected,
                "Select missed the scrolled grid bottom row");
    smoke_pointer(grid_target, wimp_CLICK_MENU);
    smoke_check(menu_kind == MENU_NODE && menu_node == grid_target && info_node == grid_target,
                "Menu missed the scrolled grid bottom row");
    fprintf(smoke_log, "grid_nodes=%d grid_edges=%d scrolled_node=%d xscroll=%d yscroll=%d\n",
            graph.node_count, graph.edge_count, menu_node, state.xscroll, state.yscroll);
    checked(xwimp_create_menu(wimp_CLOSE_MENU, 0, 0));
    graph_select(&graph, 0, 0);
    checked(xwimp_open_window(&grid_restore));
    refresh_network();
}

static void smoke_render(void)
{
    wimp_window_state state;
    osspriteop_area *capture;
    int ox, oy, cy;
    state.w = network;
    if (!checked(xwimp_get_window_state(&state))) return;
    ox = state.visible.x0-state.xscroll;
    oy = state.visible.y1-state.yscroll;
    cy=graph_node_y(&graph.nodes[1]);
    smoke_sprite_ink = smoke_ink(ox, oy,
        (graph.nodes[1].box.x0+graph.nodes[1].box.x1)/2-34, cy-34,
        (graph.nodes[1].box.x0+graph.nodes[1].box.x1)/2+34, cy+34);
    smoke_text_ink = smoke_ink(ox, oy, 16, -52, graph.width-16, -16);
    smoke_check(smoke_sprite_ink > 20, "Processor sprite is not visible in the network");
    smoke_check(smoke_text_ink > 20, "Network text is not visible in the network");
    if (failed) return;
    capture = (osspriteop_area *)calloc(1, 128*1024);
    if (capture == NULL) { smoke_check(0, "No memory for rendering evidence"); return; }
    capture->size = 128*1024;
    capture->first = capture->used = 16;
    if (checked(xosspriteop_get_sprite_user_coords(osspriteop_USER_AREA,
        capture, "network", 1, state.visible.x0, oy-390,
        state.visible.x1-1, oy-80)))
        checked(xosspriteop_save_sprite_file(osspriteop_USER_AREA, capture, smoke_picture));
    free(capture);
    smoke_rendered = !failed;
}

/* All example/layout changes go through the same MenuSelection handler as the UI. */
static void smoke_network_choice(int item,int subitem)
{
    wimp_block block;
    smoke_pointer(-1,wimp_CLICK_MENU);
    memset(&block,0,sizeof(block));
    block.selection.items[0]=item;block.selection.items[1]=subitem;block.selection.items[2]=-1;
    event(wimp_MENU_SELECTION,&block);
}

static void smoke_examples(void)
{
    int example,mode;
    smoke_check(!graph.node_count && !graph.sample,"Examples test must start empty");
    smoke_check(main_menu.entries[4].sub_menu==(wimp_menu *)&examples_menu &&
        main_menu.entries[5].sub_menu==(wimp_menu *)&layout_menu,"Example/layout submenus missing");
    for(example=0;example<GRAPH_EXAMPLES && !failed;++example) {
        smoke_network_choice(4,example);
        smoke_check(graph.sample==4 && shown_example==example && info_node==-1 && menu_node==-1,
            "Example selection did not replace and reset the view");
        smoke_check(!pending_action && !job.busy && !job.live,"Example selection started hardware work");
        for(mode=0;mode<3 && !failed;++mode) {
            wimp_window_state state;
            wimp_open open;
            int node=graph.node_count-1,w,h;
            graph_select(&graph,0,0);
            smoke_network_choice(5,mode);
            smoke_check(graph.layout_mode==mode && graph.nodes[0].selected,"Layout changed selection or ignored mode");
            state.w=network;if(!checked(xwimp_get_window_state(&state)))return;
            w=state.visible.x1-state.visible.x0;h=state.visible.y1-state.visible.y0;
            open.w=network;open.visible=state.visible;open.next=wimp_TOP;
            open.xscroll=graph.nodes[node].box.x0+88-w/2;
            if(open.xscroll>graph.width-w)open.xscroll=graph.width-w;
            if(open.xscroll<0)open.xscroll=0;
            open.yscroll=graph_node_y(&graph.nodes[node])+h/2;
            if(open.yscroll<h-graph.height)open.yscroll=h-graph.height;
            if(open.yscroll>0)open.yscroll=0;
            checked(xwimp_open_window(&open));refresh_network();smoke_wait_events();
            smoke_pointer(node,wimp_CLICK_SELECT);
            smoke_check(graph.nodes[node].selected && !graph.nodes[0].selected,"Example Select missed scrolled node");
            smoke_pointer(node,wimp_CLICK_MENU);
            smoke_check(menu_node==node && info_node==node,"Example Menu missed scrolled node");
            smoke_open_info();smoke_wait_events();
            state.w=information;
            if(checked(xwimp_get_window_state(&state)))smoke_check((state.flags&wimp_WINDOW_OPEN)!=0,"Example Info did not open");
            checked(xwimp_create_menu(wimp_CLOSE_MENU,0,0));smoke_wait_events();
            if(mode==GRAPH_AUTO)smoke_capture(network,(char)('0'+example));
            fprintf(smoke_log,"example=%s mode=%d layout=%s nodes=%d edges=%d width=%d height=%d\n",
                graph_example_name(example),mode,graph_layout_name(&graph),graph.node_count,graph.edge_count,graph.width,graph.height);
        }
    }
    if(!failed) {
        int saved_example=shown_example,saved_mode=graph.layout_mode;
        job_begin(&job);
        smoke_network_choice(4,0);smoke_network_choice(5,0);
        smoke_check(shown_example==saved_example && graph.layout_mode==saved_mode,"Busy guard allowed layout/example replacement");
        smoke_check((main_menu.entries[4].icon_flags&wimp_ICON_SHADED) &&
            (main_menu.entries[5].icon_flags&wimp_ICON_SHADED),"Busy example/layout controls not shaded");
        job_end(&job,0);
    }
}

/* Exercise the large-example -> real-discovery shrink, including old pixels
 * below the new graph and an old scroll position far from the host. */
static void smoke_transition(void)
{
    int action;
    for(action=1;action<=2 && !failed;++action) {
        wimp_window_state state;
        wimp_open open;
        wimp_pointer pointer;
        int sw,sh,w,h,ox,oy,ink=0,i;
        smoke_network_choice(4,3);
        if(failed || !screen_size(&sw,&sh))return;
        w=sw-180;if(w>900)w=900;
        h=sh-180;if(h>760)h=760;
        state.w=network;
        if(!checked(xwimp_get_window_state(&state)))return;
        open.w=network;open.visible=state.visible;open.next=wimp_TOP;
        open.visible.x0=80;open.visible.x1=80+w;
        open.visible.y0=sh-100-h;open.visible.y1=sh-100;
        open.xscroll=graph.width-w;open.yscroll=h-graph.height;
        checked(xwimp_open_window(&open));refresh_network();smoke_wait_events();
        smoke_capture(network,'g');
        if(failed)return;
        if(action==1)smoke_network_choice(0,-1);
        else {
            offer_ram(-1);smoke_wait_events();
            memset(&pointer,0,sizeof(pointer));pointer.w=ram_count;pointer.i=5;
            pointer.buttons=wimp_CLICK_SELECT;pointer_event(&pointer);
        }
        smoke_check(pending_action==action,"Scan control did not queue the requested operation");
        if(failed)return;
        run_pending();smoke_wait_events();
        if(job.quitting)smoke_check(0,"Transition check was cancelled");
        if(failed)return;
        smoke_check(!graph.sample && graph.node_count==2,"Grid scan did not replace example");
        if(failed)return;
        if(action==2)for(i=0;i<graph.node_count;++i)
            smoke_check(graph.nodes[i].ram_state==3 &&
                node_external_bytes(&graph.nodes[i])==4194304UL,
                "Grid RAM scan did not complete the 4 MiB count");
        state.w=network;
        if(failed || !checked(xwimp_get_window_state(&state)))return;
        smoke_check(state.xscroll==0 && state.yscroll==0,"Scan retained example scroll position");
        ox=state.visible.x0-state.xscroll;oy=state.visible.y1-state.yscroll;
        if(h>graph.height+32) {
            ink=smoke_ink(ox,oy,16,-h+16,w-16,-graph.height);
            smoke_check(ink==0,"Old example pixels survived below the new graph");
        }
        fprintf(smoke_log,"transition_action=%d nodes=%d blank_ink=%d xscroll=%d yscroll=%d status=%s\n",
            action,graph.node_count,ink,state.xscroll,state.yscroll,operation_status);
        if(failed)return;
        smoke_capture(network,action==1?'p':'r');
        smoke_pointer(1,wimp_CLICK_SELECT);
        smoke_check(graph.nodes[1].selected,"Select failed after scan");
        smoke_pointer(1,wimp_CLICK_MENU);
        smoke_check(menu_node==1 && info_node==1,"Node Menu failed after scan");
        smoke_open_info();smoke_wait_events();
        state.w=information;
        if(checked(xwimp_get_window_state(&state)))
            smoke_check((state.flags&wimp_WINDOW_OPEN)!=0,"Info failed after scan");
        checked(xwimp_create_menu(wimp_CLOSE_MENU,0,0));
    }
}

int main(int argc, char **argv)
{
    int transition_smoke=argc==3 && strcmp(argv[1],"--smoke-transition")==0;
    int examples_smoke=argc==3 && strcmp(argv[1],"--smoke-examples")==0;
    int path_smoke = argc == 3 && strcmp(argv[1], "--smoke-path") == 0;
    int empty_smoke = path_smoke || (argc == 3 && strcmp(argv[1], "--smoke-empty") == 0);
    int repeat_smoke = argc == 3 && strcmp(argv[1], "--smoke-repeat") == 0;
    int live_smoke = repeat_smoke || ((argc == 3 || argc == 4) && strcmp(argv[1], "--smoke-live") == 0);
    int layout_smoke = argc == 3 && strcmp(argv[1], "--smoke-layout") == 0;
    int grid_smoke = layout_smoke || ((argc == 3 || argc == 4) && strcmp(argv[1], "--smoke-grid") == 0);
    int ram_smoke = argc == 3 && strcmp(argv[1], "--smoke-ram") == 0;
    int smoke = transition_smoke || examples_smoke || empty_smoke || ram_smoke || grid_smoke || live_smoke || ((argc == 3 || argc == 4) && strcmp(argv[1], "--smoke") == 0);
    int phase = -1;
    unsigned int phase_poll = 0;
    unsigned int begun = 0, previous = 0, now = 0, gap, max_gap = 0;
    wimp_event_no reason;
    wimp_block block;
    layered_sample = layout_smoke || (argc == 3 && strcmp(argv[1], "--sample") == 0 &&
                                         strcmp(argv[2], "layered") == 0);
    grid_sample = grid_smoke || (argc == 3 && strcmp(argv[1], "--sample") == 0 &&
                                    strcmp(argv[2], "grid") == 0);
    parallel_sample = (argc == 3 && strcmp(argv[1], "--sample") == 0 &&
                      strcmp(argv[2], "parallel") == 0) ||
                     (smoke && !examples_smoke && !empty_smoke && !ram_smoke && !grid_smoke && !live_smoke);
    if(argc==3 && strcmp(argv[1],"--example")==0) {
        static const char *slugs[]={"pipeline","ring","grid3","grid4","tree","cube","irregular","parallel"};
        int i;
        for(i=0;i<GRAPH_EXAMPLES;++i)if(strcmp(argv[2],slugs[i])==0)startup_example=i;
        if(startup_example<0){fprintf(stderr,"Unknown example: %s\n",argv[2]);return 1;}
    }
    job_init(&job);
    hardware_quiet_test = argc >= 3 && strcmp(argv[1], "--hardware-quiet") == 0;
    if (argc >= 3 && (strcmp(argv[1], "--hardware") == 0 || hardware_quiet_test)) {
        hardware_log = fopen(argv[2], "w");
        if (!hardware_log) return 1;
        pending_action = argc >= 4 && strcmp(argv[3], "discover") != 0 ? 2 : 1;
        pending_node = argc >= 5 ? atoi(argv[4]) : -1;
        if (argc >= 6) ram_ceiling = (unsigned int)strtoul(argv[5], NULL, 10);
        hardware_cancel_test = argc >= 4 && strcmp(argv[3], "cancel") == 0;
        hardware_quit_test = argc >= 4 && strcmp(argv[3], "quit") == 0;
        hardware_fault_stall = hardware_cancel_test || hardware_quit_test ||
            (argc >= 4 && strcmp(argv[3], "stall") == 0);
    }
    if (smoke) {
        smoke_log = fopen(argv[2], "w");
        if (smoke_log == NULL) return 1;
        if (strlen(argv[2]) > sizeof(smoke_picture)-2) { fclose(smoke_log); return 1; }
        sprintf(smoke_picture, "%sp", argv[2]);
    }
    /* Mode-specific checks verify the user's chosen mode. Never change the
     * shared desktop mode from a test task. */
    if (smoke && argc == 4) {
        _kernel_swi_regs regs;
        int mode = atoi(argv[3]);
        memset(&regs, 0, sizeof(regs));
        regs.r[0] = 135;
        if (!checked((os_error *)_kernel_swi(OS_Byte, &regs, &regs))) goto done;
        if ((mode != 12 && mode != 27) || (regs.r[2] & 255) != mode) {
            smoke_check(0, "Select the requested desktop mode manually before this UI check");
            goto done;
        }
    }
    if (!checked(xwimp_initialise(310, "Ispy", (const wimp_message_list *)messages,
                                  NULL, &task))) goto done;
    if (!start()) {
        if (!failed) {
            last_error.errnum = 1;
            strcpy(last_error.errmess, "Run Ispy using its !Run file (Ispy$Dir is missing)");
            failed = 1;
        }
        goto close;
    }
    if(transition_smoke){smoke_transition();goto close;}
    if(examples_smoke){smoke_examples();goto close;}
    if (smoke) {
        if (empty_smoke) {
            wimp_window_state state;
            if (path_smoke) {
                smoke_resource_directory();
                if (failed) goto close;
            }
            smoke_check(!graph.sample && !graph.node_count && !graph.edge_count &&
                        graph_hit(&graph, 520, -210) == -1,
                        "Normal startup contains demonstration nodes");
            smoke_geometry();
            smoke_pointer(-1, wimp_CLICK_MENU);
            smoke_check(menu_kind == MENU_NETWORK && menu_node == -1,
                        "Empty view did not offer network operations");
            smoke_open_submenu(1);
            smoke_wait_events();
            state.w = ram_count;
            if (checked(xwimp_get_window_state(&state)))
                smoke_check((state.flags & wimp_WINDOW_OPEN) && ram_node == -1,
                            "Empty view did not offer whole-network Count RAM");
            checked(xwimp_create_menu(wimp_CLOSE_MENU, 0, 0));
            smoke_check(!pending_action && !job.busy,
                        "Empty-view browsing started a hardware operation");
            smoke_wait_events();
            smoke_capture(network, 'e');
            goto close;
        }
        if (live_smoke) {
            int i;
            ram_ceiling = 8192;
            smoke_check(!graph.sample && !graph.node_count && !graph.edge_count,
                        "Live startup did not start with an empty graph");
            if (repeat_smoke) {
                int width = graph.width;
                graph_sample_grid(&graph);
                graph.width = width;
                smoke_expect_empty = 1;
            }
            if (repeat_smoke) smoke_repeat_counts();
            else {
                queue_action(2, -1);
                run_pending();
            }
            smoke_check(!graph.sample && graph.node_count == 2 && !failed,
                        "Live UI run did not discover the two-node test network");
            for (i = 0; i < graph.node_count; ++i) {
                char label[80];
                node_ram_label(&graph.nodes[i], label);
                fprintf(smoke_log, "node=%d internal=%lu external=%lu graph_label=%s\n",
                    graph.nodes[i].number, graph.nodes[i].internal_bytes,
                    node_external_bytes(&graph.nodes[i]), label);
            }
            if (failed) goto close;
            redraws = node_redraws = 0;
            refresh_network();
        }
        if (ram_smoke) {
            offer_ram(-1);
            phase = 3;
        } else {
            job_begin(&job);
            smoke_geometry();
            smoke_pointer(0, wimp_CLICK_SELECT);
            smoke_check(graph.nodes[0].selected && !graph.nodes[1].selected,
                        "Select did not select only node 0");
            smoke_pointer(1, wimp_CLICK_ADJUST);
            smoke_check(graph.nodes[0].selected && graph.nodes[1].selected,
                        "Adjust did not extend the selection");
            smoke_pointer(1, wimp_CLICK_ADJUST);
        }
        if (!ticks(&begun)) goto close;
        previous = begun;
    }
    while (!job_can_quit(&job)) {
        if (!ticks(&now)) break;
        if (!checked(xwimp_poll_idle(smoke || job.busy || pending_action ? 0 : wimp_MASK_NULL,
                      &block, (os_t)(now+1), NULL, &reason))) break;
        ++polls;
        event(reason, &block);
        if (!smoke && pending_action && !job.busy && !job.quitting) run_pending();
        if (smoke) {
            if (!ticks(&now)) break;
            gap = now - previous;
            if (gap > max_gap) max_gap = gap;
            previous = now;
            if (phase == -1 && redraws > 0 && now-begun >= 20) {
                smoke_render();
                if (grid_sample) {
                    smoke_grid_begin(); phase = -2; phase_poll = polls;
                } else phase = -3;
            }
            if (phase == -2 && reason == wimp_NULL_REASON_CODE && polls > phase_poll+2) {
                smoke_grid_end(); phase = -3; phase_poll = polls;
            }
            if (phase == -3 && reason == wimp_NULL_REASON_CODE && polls > phase_poll+2) {
                smoke_pointer(1, wimp_CLICK_MENU);
                smoke_check(menu_kind == MENU_NODE && menu_node == 1 && info_node == 1 &&
                            graph.nodes[0].selected && !graph.nodes[1].selected,
                            "Node Menu changed selection or targeted the wrong node");
                smoke_check(processor_menu.entries[0].sub_menu == (wimp_menu *)information,
                            "Node Info is not a dialogue submenu");
                smoke_open_info();
                phase = 0;
            } else if (phase == 0 && node_redraws > 0 && now-begun >= 80) {
                wimp_pointer pointer;
                smoke_capture(information, 'n');
                memset(&pointer, 0, sizeof(pointer));
                pointer.w = wimp_ICON_BAR; pointer.i = bar_icon;
                pointer.buttons = wimp_CLICK_MENU;
                pointer.pos.x = 800; pointer.pos.y = 400;
                pointer_event(&pointer);
                smoke_check(menu_kind == MENU_ICONBAR &&
                    iconbar_menu.entries[0].sub_menu == (wimp_menu *)program_info,
                    "Icon bar Menu has no program Info submenu");
                smoke_check(menu_y == 208 && menu_x == 736,
                    "Icon bar menu base is not 96 OS units above the screen bottom");
                smoke_open_info();
                phase = 1;
            } else if (phase == 1 && polls > info_open_poll && now-begun >= 140) {
                wimp_window_state state;
                state.w = program_info;
                if (checked(xwimp_get_window_state(&state)) &&
                    (state.flags & wimp_WINDOW_OPEN)) {
                    smoke_check(1, "Program Info submenu did not open");
                    smoke_capture(program_info, 'i');
                    smoke_pointer(-1, wimp_CLICK_MENU);
                    smoke_check(menu_kind == MENU_NETWORK && menu_node == -1,
                                "Blank-space Menu did not target the network");
                    checked(xwimp_create_menu(wimp_CLOSE_MENU, 0, 0));
                    smoke_pointer(-1, wimp_CLICK_SELECT);
                    smoke_check(!graph.nodes[0].selected && !graph.nodes[1].selected,
                                "Blank-space Select did not clear selection");
                    phase = 2;
                }
            }
            if (phase == 2 && now-begun >= 200) {
                wimp_icon_state state;
                job_end(&job, 0);
                smoke_pointer(-1, wimp_CLICK_MENU);
                smoke_check(main_menu.entries[1].sub_menu == (wimp_menu *)ram_count &&
                    processor_menu.entries[1].sub_menu == (wimp_menu *)ram_count,
                    "Count RAM is not a submenu on both menus");
                smoke_open_submenu(1);
                state.w = ram_count; state.i = 3;
                if (checked(xwimp_get_icon_state(&state))) {
                    fprintf(smoke_log, "ceiling_size=%d bytes=%u,%u,%u,%u,%u\n",
                        state.icon.data.indirected_text.size,
                        (unsigned char)state.icon.data.indirected_text.text[0],
                        (unsigned char)state.icon.data.indirected_text.text[1],
                        (unsigned char)state.icon.data.indirected_text.text[2],
                        (unsigned char)state.icon.data.indirected_text.text[3],
                        (unsigned char)state.icon.data.indirected_text.text[4]);
                    smoke_check(strncmp(state.icon.data.indirected_text.text, "8192", 4) == 0 &&
                        (unsigned char)state.icon.data.indirected_text.text[4] < 32 &&
                        state.icon.data.indirected_text.size >= 7,
                        "RAM ceiling field has the wrong value or buffer size");
                }
                phase = 3;
            } else if (phase == 3 && polls > ram_open_poll+1 && now-ram_open_time >= 20 &&
                       reason == wimp_NULL_REASON_CODE) {
                wimp_window_state state;
                state.w = ram_count;
                if (checked(xwimp_get_window_state(&state)))
                    smoke_check((state.flags & wimp_WINDOW_OPEN) != 0,
                                "RAM dialogue did not open");
                smoke_capture(ram_count, 'r');
                accept_ram(0);
                smoke_check(pending_action == 2 && pending_node == -1 && ram_ceiling == 8192,
                            "RAM dialogue did not queue the requested network operation");
                pending_action = 0;
                smoke_check(job_begin(&job), "RAM dialogue lost operation ownership");
                phase = 4;
            }
            if (now-begun >= (grid_sample ? 3000U : 2000U) && phase != 4)
                smoke_check(0, "Timed out waiting for submenu messages and redraws");
            if (phase == 4 || failed || job.quitting) {
                job_quit(&job);
                job_end(&job, 0);
            }
        }
    }
close:
    if (smoke && !transition_smoke && !examples_smoke && !ram_smoke && !empty_smoke && !failed)
        smoke_check(redraws >= 2 && node_redraws > 0 && phase == 4 && smoke_rendered,
                    "Smoke run did not complete redraw/interaction checks");
    if (!smoke && failed)
        xwimp_report_error(&last_error, wimp_ERROR_BOX_OK_ICON, "Ispy", NULL);
    checked(xwimp_close_down(task));
done:
    if (hardware_log) fclose(hardware_log);
    if (smoke_log != NULL) {
        fprintf(smoke_log, "status=%s polls=%u redraws=%u max_gap_cs=%u checks=%d sprite_ink=%d text_ink=%d sprites=%s reloads=%u\n",
                failed ? "FAIL" : "OK", polls, redraws, max_gap, interaction_checks,
                smoke_sprite_ink, smoke_text_ink, sprite_file == NULL ? "none" : sprite_file,
                sprite_reloads);
        if (failed) {
            fprintf(smoke_log, "error=%s\n", last_error.errmess);
            fprintf(smoke_log, "error_line=%d error_call=%s phase=%d\n",
                    error_line, error_call == NULL ? "unknown" : error_call, phase);
        }
        /* Publish completion only after Wimp_CloseDown and the log close.
         * Result collection must test this marker before opening any output. */
        if (fclose(smoke_log) == 0) {
            FILE *completion;
            smoke_picture[strlen(smoke_picture)-1] = 'd';
            completion = fopen(smoke_picture, "w");
            if (completion != NULL) {
                fputs(failed ? "FAIL\n" : "OK\n", completion);
                if (fclose(completion) != 0) failed = 1;
            } else failed = 1;
        } else failed = 1;
    } else if (failed) {
        fprintf(stderr, "Ispy: %s\n", last_error.errmess);
    }
    return failed ? 1 : 0;
}
