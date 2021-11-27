#include <gtk/gtk.h>

static void print_hello(GtkWidget *widget, gpointer data) {
  // NOTE: g_print is also interesting function
  g_print("Hello World\n");
}

static void print_hello2(GtkWidget *widget, gpointer data) {
  // NOTE: g_print is also interesting function
  g_print("Hello World2\n");
}

static void activate(GtkApplication *app, gpointer user_data) {
  GtkWidget *window;
  GtkWidget *button;
  GtkWidget *button_box;
  GtkWidget *button2;
  GtkWidget *button_box2;

  window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "Window");
  gtk_window_set_default_size(GTK_WINDOW(window), 200, 200);

  button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_container_add(GTK_CONTAINER(window), button_box);
  button_box2 = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_container_add(GTK_CONTAINER(window), button_box2);

  // NOTE: g_signal_connect can be interesting target
  // extract "clicked", "print_hello" from arguments
  g_signal_connect(button, "clicked", G_CALLBACK(print_hello), NULL);
  g_signal_connect(button2, "clicked", G_CALLBACK(print_hello2), NULL);
  /* g_signal_connect_swapped(button, "clicked", G_CALLBACK(gtk_widget_destroy), */
  /*                          window); */
  gtk_container_add(GTK_CONTAINER(button_box), button);
  gtk_container_add(GTK_CONTAINER(button_box), button2);

  gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
  GtkApplication *app;
  int status;

  app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  return status;
}
