#include <gtk/gtk.h>
#include <vte/vte.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_SIZE 16
#define LINE_SIZE 512

static GList *terminals = NULL;
static GtkWidget *active_term = NULL;
static gboolean broadcast = FALSE;
static gchar *current_shell = NULL;
static GtkWidget *btn_bcast = NULL;
static gboolean is_flatpak = FALSE;

static gboolean on_term_button_press(GtkWidget *term, GdkEventButton *event, gpointer data) {
    (void)data;
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        active_term = term;
        gtk_widget_grab_focus(term);
        vte_terminal_paste_clipboard(VTE_TERMINAL(term));
        return TRUE;
    }
    return FALSE;
}

static GtkWidget *create_terminal(const char *shell_path);
static void split_pane(GtkOrientation orientation);

static void on_term_focus(GtkWidget *term, GdkEventFocus *event, gpointer data) {
    (void)event; (void)data;
    active_term = term;
}

static void on_term_exit(VteTerminal *vte, gint status, gpointer data) {
    (void)status; (void)data;
    GtkWidget *term = GTK_WIDGET(vte);
    terminals = g_list_remove(terminals, term);

    if (terminals == NULL) {
        gtk_main_quit();
        return;
    }

    GtkWidget *parent = gtk_widget_get_parent(term);
    if (GTK_IS_PANED(parent)) {
        GtkWidget *c1 = gtk_paned_get_child1(GTK_PANED(parent));
        GtkWidget *c2 = gtk_paned_get_child2(GTK_PANED(parent));
        GtkWidget *sibling = (c1 == term) ? c2 : c1;
        GtkWidget *grandparent = gtk_widget_get_parent(parent);

        g_object_ref(sibling);
        gtk_container_remove(GTK_CONTAINER(parent), term);
        gtk_container_remove(GTK_CONTAINER(parent), sibling);

        if (GTK_IS_PANED(grandparent)) {
            gboolean is_c1 = (gtk_paned_get_child1(GTK_PANED(grandparent)) == parent);
            gtk_container_remove(GTK_CONTAINER(grandparent), parent);
            if (is_c1)
                gtk_paned_pack1(GTK_PANED(grandparent), sibling, TRUE, FALSE);
            else
                gtk_paned_pack2(GTK_PANED(grandparent), sibling, TRUE, FALSE);
        } else {
            gtk_container_remove(GTK_CONTAINER(grandparent), parent);
            gtk_box_pack_start(GTK_BOX(grandparent), sibling, TRUE, TRUE, 0);
        }
        g_object_unref(sibling);
        gtk_widget_show_all(grandparent);
        active_term = GTK_WIDGET(g_list_last(terminals)->data);
        gtk_widget_grab_focus(active_term);
    }
}

static gboolean on_term_keypress(GtkWidget *source, GdkEventKey *event, gpointer data) {
    (void)data;
    if (!broadcast || g_list_length(terminals) <= 1) return FALSE;

    char buf[BUF_SIZE] = {0};
    int len = 0;

    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        *buf = '\r'; len = 1;
    } else if (event->keyval == GDK_KEY_BackSpace) {
        *buf = '\x7f'; len = 1;
    } else if (event->keyval == GDK_KEY_Tab) {
        *buf = '\t'; len = 1;
    } else {
        guint32 uc = gdk_keyval_to_unicode(event->keyval);
        if (uc != 0) len = g_unichar_to_utf8(uc, buf);
    }

    if (len > 0) {
        for (GList *l = terminals; l != NULL; l = l->next) {
            if (l->data != source) {
                vte_terminal_feed_child(VTE_TERMINAL(l->data), buf, len);
            }
        }
    }
    return FALSE;
}

static GtkWidget *create_terminal(const char *shell_path) {
    GtkWidget *term = vte_terminal_new();
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(term), 10000);
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(term), TRUE);

    GdkRGBA bg = {0.16, 0.20, 0.25, 1.0};
    GdkRGBA fg = {0.96, 0.95, 0.85, 1.0};
    vte_terminal_set_colors(VTE_TERMINAL(term), &fg, &bg, NULL, 0);
    GdkRGBA cur = {0.98, 0.88, 0.25, 1.0};
    vte_terminal_set_color_cursor(VTE_TERMINAL(term), &cur);

    g_signal_connect(term, "focus-in-event", G_CALLBACK(on_term_focus), NULL);
    g_signal_connect(term, "button-press-event", G_CALLBACK(on_term_button_press), NULL);
    g_signal_connect(term, "key-press-event", G_CALLBACK(on_term_keypress), NULL);
    g_signal_connect(term, "child-exited", G_CALLBACK(on_term_exit), NULL);

    if (is_flatpak) {
        char *argv[] = {"flatpak-spawn", "--host", (char *)shell_path, NULL};
        vte_terminal_spawn_async(
            VTE_TERMINAL(term), VTE_PTY_DEFAULT, g_get_home_dir(),
            argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, -1, NULL, NULL, NULL
        );
    } else {
        char *argv[] = {(char *)shell_path, NULL};
        vte_terminal_spawn_async(
            VTE_TERMINAL(term), VTE_PTY_DEFAULT, g_get_home_dir(),
            argv, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, -1, NULL, NULL, NULL
        );
    }

    terminals = g_list_append(terminals, term);
    active_term = term;
    return term;
}

static void split_pane(GtkOrientation orientation) {
    if (!active_term) return;
    GtkWidget *target = active_term;
    GtkWidget *parent = gtk_widget_get_parent(target);
    if (!parent) return;

    GtkWidget *paned = gtk_paned_new(orientation);
    gtk_paned_set_wide_handle(GTK_PANED(paned), TRUE);

    g_object_ref(target);
    if (GTK_IS_PANED(parent)) {
        gboolean is_c1 = (gtk_paned_get_child1(GTK_PANED(parent)) == target);
        gtk_container_remove(GTK_CONTAINER(parent), target);
        if (is_c1)
            gtk_paned_pack1(GTK_PANED(parent), paned, TRUE, FALSE);
        else
            gtk_paned_pack2(GTK_PANED(parent), paned, TRUE, FALSE);
    } else {
        gtk_container_remove(GTK_CONTAINER(parent), target);
        gtk_box_pack_start(GTK_BOX(parent), paned, TRUE, TRUE, 0);
    }

    GtkWidget *new_term = create_terminal(current_shell);
    gtk_paned_pack1(GTK_PANED(paned), target, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(paned), new_term, TRUE, FALSE);
    g_object_unref(target);

    gtk_widget_show_all(parent);

    GtkAllocation alloc;
    gtk_widget_get_allocation(paned, &alloc);
    int pos = (orientation == GTK_ORIENTATION_VERTICAL) ? (alloc.height / 2) : (alloc.width / 2);
    if (pos > 0) gtk_paned_set_position(GTK_PANED(paned), pos);

    gtk_widget_grab_focus(new_term);
}

static void on_toggle_bcast(GtkToggleButton *btn, gpointer data) {
    (void)data;
    broadcast = gtk_toggle_button_get_active(btn);
    gtk_button_set_label(GTK_BUTTON(btn),
        broadcast ? "Broadcast: ON (Ctrl+Shift+B)" : "Broadcast: OFF (Ctrl+Shift+B)");
}

static void on_shell_changed(GtkComboBoxText *combo, gpointer data) {
    (void)data;
    gchar *text = gtk_combo_box_text_get_active_text(combo);
    if (text) {
        g_free(current_shell);
        current_shell = text;
    }
}

static gboolean on_window_keypress(GtkWidget *w, GdkEventKey *event, gpointer data) {
    (void)w; (void)data;
    gboolean ctrl = (event->state & GDK_CONTROL_MASK) != 0;
    gboolean shift = (event->state & GDK_SHIFT_MASK) != 0;

    if (ctrl && shift) {
        if (event->keyval == GDK_KEY_O) { split_pane(GTK_ORIENTATION_VERTICAL); return TRUE; }
        if (event->keyval == GDK_KEY_E) { split_pane(GTK_ORIENTATION_HORIZONTAL); return TRUE; }
        if (event->keyval == GDK_KEY_B) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn_bcast), !broadcast);
            return TRUE;
        }
        if (event->keyval == GDK_KEY_C && active_term) {
            vte_terminal_copy_clipboard_format(VTE_TERMINAL(active_term), VTE_FORMAT_TEXT);
            return TRUE;
        }
        if (event->keyval == GDK_KEY_V && active_term) {
            vte_terminal_paste_clipboard(VTE_TERMINAL(active_term));
            return TRUE;
        }
    }
    return FALSE;
}

static void populate_shells(GtkComboBoxText *combo) {
    const char *shells_path = is_flatpak ? "/run/host/etc/shells" : "/etc/shells";
    FILE *f = fopen(shells_path, "r");
    char line[LINE_SIZE];
    int count = 0;
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            g_strstrip(line);
            if (*line != '\0' && *line != '#') {
                if (is_flatpak || g_file_test(line, G_FILE_TEST_EXISTS)) {
                    gtk_combo_box_text_append_text(combo, line);
                    if (count == 0) current_shell = g_strdup(line);
                    count++;
                }
            }
        }
        fclose(f);
    }
    if (count == 0) {
        gtk_combo_box_text_append_text(combo, "/bin/bash");
        current_shell = g_strdup("/bin/bash");
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "window, box { background-color: #2c3540; color: #f4f1d0; }"
        "button, combobox button { background: #3e4a59; color: #fef08a; border: 1px solid #facc15; border-radius: 4px; padding: 3px 8px; }"
        "button:checked { background: #facc15; color: #1e293b; font-weight: bold; }"
        "paned > separator { background-color: #facc15; min-width: 3px; min-height: 3px; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    is_flatpak = g_file_test("/.flatpak-info", G_FILE_TEST_EXISTS);

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Robotinator [Bender Edition]");
    gtk_window_set_default_size(GTK_WINDOW(win), 1100, 680);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(win, "key-press-event", G_CALLBACK(on_window_keypress), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(win), vbox);

    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_top(header, 4);
    gtk_widget_set_margin_bottom(header, 4);
    gtk_widget_set_margin_start(header, 6);
    gtk_widget_set_margin_end(header, 6);

    GtkWidget *btn_h = gtk_button_new_with_label("Split Horizontal (Ctrl+Shift+O)");
    g_signal_connect_swapped(btn_h, "clicked", G_CALLBACK(split_pane), GINT_TO_POINTER(GTK_ORIENTATION_VERTICAL));

    GtkWidget *btn_v = gtk_button_new_with_label("Split Vertical (Ctrl+Shift+E)");
    g_signal_connect_swapped(btn_v, "clicked", G_CALLBACK(split_pane), GINT_TO_POINTER(GTK_ORIENTATION_HORIZONTAL));

    btn_bcast = gtk_toggle_button_new_with_label("Broadcast: OFF (Ctrl+Shift+B)");
    g_signal_connect(btn_bcast, "toggled", G_CALLBACK(on_toggle_bcast), NULL);

    GtkWidget *combo = gtk_combo_box_text_new();
    populate_shells(GTK_COMBO_BOX_TEXT(combo));
    g_signal_connect(combo, "changed", G_CALLBACK(on_shell_changed), NULL);

    gtk_box_pack_start(GTK_BOX(header), btn_h, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), btn_v, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), btn_bcast, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header), combo, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header), gtk_label_new("New Pane Shell:"), FALSE, FALSE, 4);

gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);
GtkWidget *root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
gtk_box_pack_start(GTK_BOX(vbox), root_box, TRUE, TRUE, 0);
GtkWidget *first = create_terminal(current_shell);
gtk_box_pack_start(GTK_BOX(root_box), first, TRUE, TRUE, 0);
gtk_widget_show_all(win);
gtk_widget_grab_focus(first);
gtk_main();
return 0;
}
