#include "gui.h"

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    gui_create(app);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.mathviz.app", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
