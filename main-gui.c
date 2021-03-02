/* 
*  ==================================================================
*  main-gui.c v0.1 for smvp-csr
*  ==================================================================
*/

#define MAJOR_VER 0
#define MINOR_VER 3
#define REVISION_VER 0
#define SMVP_CSR_DEBUG 0

#define _POSIX_C_SOURCE 200809L // Required to utilize HPET for execution time calculations (via CLOCK_MONOTONIC)

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>

// GTK3 Object Initialization
GtkWidget *g_main_fcbtn_input;
GtkWidget *g_main_entry_output;
GtkWidget *g_main_btn_fcwdgt;
GtkWidget *g_main_sw_csr;
GtkWidget *g_main_sw_tjds;
GtkWidget *g_main_spinbtn_iter;
GtkWidget *g_main_btn_runalgs;
GtkWidget *g_main_pgbar;
GtkWidget *g_main_pgspin;
GtkWidget *g_fc_btn_choose;
GtkWidget *g_fc_button_cancel;

G_MODULE_EXPORT void fc_button_cancel_clicked_cb()
{
}

G_MODULE_EXPORT void fc_btn_choose_clicked_cb()
{
}

G_MODULE_EXPORT void main_btn_fcwdgt_clicked_cb()
{
}

G_MODULE_EXPORT void main_btn_runalgs_clicked_cb()
{
}

G_MODULE_EXPORT void appwindow_main_destroy_cb()
{
    gtk_main_quit();
}

int main(int argc, char *argv[])
{

    // Initialize GTK framework
    GtkBuilder *builder;
    GtkWidget *window;

    gtk_init(&argc, &argv);

    builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "glade/smvp-tbx-main.glade", NULL);
    window = GTK_WIDGET(gtk_builder_get_object(builder, "appwindow_main"));

    gtk_builder_add_callback_symbols(builder,
                                     "fc_button_cancel_clicked_cb", G_CALLBACK(fc_button_cancel_clicked_cb),
                                     "fc_btn_choose_clicked_cb", G_CALLBACK(fc_btn_choose_clicked_cb),
                                     "main_btn_fcwdgt_clicked_cb", G_CALLBACK(main_btn_fcwdgt_clicked_cb),
                                     "main_btn_runalgs_clicked_cb", G_CALLBACK(main_btn_runalgs_clicked_cb),
                                     "appwindow_main_destroy_cb", G_CALLBACK(appwindow_main_destroy_cb),
                                     NULL);

    gtk_builder_connect_signals(builder, NULL);

    // Get pointer addresses to GTK window objects
    g_main_fcbtn_input = GTK_WIDGET(gtk_builder_get_object(builder, "main_fcbtn_input"));
    g_main_entry_output = GTK_WIDGET(gtk_builder_get_object(builder, "main_entry_output"));
    g_main_btn_fcwdgt = GTK_WIDGET(gtk_builder_get_object(builder, "main_btn_fcwdgt"));
    g_main_sw_csr = GTK_WIDGET(gtk_builder_get_object(builder, "main_sw_csr"));
    g_main_sw_tjds = GTK_WIDGET(gtk_builder_get_object(builder, "main_sw_tjds"));
    g_main_spinbtn_iter = GTK_WIDGET(gtk_builder_get_object(builder, "main_spinbtn_iter"));
    g_main_btn_runalgs = GTK_WIDGET(gtk_builder_get_object(builder, "main_btn_runalgs"));
    g_main_pgbar = GTK_WIDGET(gtk_builder_get_object(builder, "main_pgbar"));
    g_main_pgspin = GTK_WIDGET(gtk_builder_get_object(builder, "main_pgspin"));
    g_fc_btn_choose = GTK_WIDGET(gtk_builder_get_object(builder, "fc_btn_choose"));
    g_fc_button_cancel = GTK_WIDGET(gtk_builder_get_object(builder, "fc_button_cancel"));

    g_object_unref(builder);

    gtk_widget_show(window);
    gtk_main();

    return 0;
}
