#include <gtk/gtk.h>

GtkWidget *spinButton;

void value_changed_callback(GtkButton *Button, gpointer data) {

    gint value = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinButton));
    GString *text = g_string_new("");
    // g_string_printf(text, "%d", value);
    if (value == 47) {
        g_string_printf(text, "%s", "You are sooo lucky!!!");
    } else {
        g_string_printf(text, "%s", "Bad luck =( ");
    }
    gtk_label_set_text(GTK_LABEL(data), text->str);
}

// static void button_clicked(GtkWidget *button, gpointer data)
// {
//     g_print("Button clicked\n");
// }

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *label;
    GtkWidget *vbox;
    GtkWidget *magicButton;
    GtkWidget *widgetPtr[2] = {spinButton, label};

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "GtkVBox");
    gtk_window_set_default_size(GTK_WINDOW(window), 200, 50);

    spinButton = gtk_spin_button_new_with_range(0.0, 100.0, 1.0);
    label = gtk_label_new(" ");
    vbox = gtk_box_new(TRUE, 5);
    magicButton = gtk_button_new_with_label("Click to see if you are lucky");

    gtk_box_pack_start(GTK_BOX(vbox), spinButton, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), magicButton, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 5);
    
    gtk_container_add(GTK_CONTAINER(window), vbox);

    g_signal_connect(GTK_BUTTON(magicButton), "clicked",
                     G_CALLBACK(value_changed_callback), label);
    g_signal_connect(window, "destroy",
                     G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window);

    gtk_main();

    return 0;
}