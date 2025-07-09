#include <getopt.h>
#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  int time; // seconds
} Config;

typedef struct {
  GtkLabel *label;
  int seconds_remaining;
  GtkApplication *app;
} TimerData;

const char *format_time_label(int seconds) {
  static char buffer[64];

  snprintf(buffer, sizeof(buffer), "%02d:%02d", seconds / 60, seconds % 60);
  return buffer;
}

static gboolean update_label(gpointer user_data) {
  TimerData *data = (TimerData *)user_data;

  if (data->seconds_remaining <= 0) {
    g_application_quit(G_APPLICATION(data->app));
    return G_SOURCE_REMOVE;
  }

  gtk_label_set_text(data->label, format_time_label(data->seconds_remaining));
  data->seconds_remaining--;

  return G_SOURCE_CONTINUE;
}

static gboolean on_key_press(GtkEventControllerKey *self, guint keyval,
                             guint keycode, GdkModifierType state,
                             gpointer user_data) {
  if (keyval == GDK_KEY_Escape) {
    gtk_window_close(GTK_WINDOW(user_data));
  }
  return FALSE;
}

inline void window_setup(GtkWindow *window) {
  gtk_window_set_title(window, "gui");
  // gtk_window_set_default_size(window, 400, 500);

  gtk_layer_init_for_window(GTK_WINDOW(window));
  gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
  gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
  gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
}

static void activate(GtkApplication *app, gpointer user_data) {
  GtkWindow *window;
  GtkWidget *label;
  GtkWidget *box;

  Config *config = (Config *)user_data;

  window = GTK_WINDOW(gtk_application_window_new(app));
  window_setup(window);

  box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_window_set_child(window, box);

  label = gtk_label_new(format_time_label(config->time));
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_string(
      provider, "* { background-color: rgba(0, 0, 0, 0.3); }"
                "label { font-size: 72px; color: #fcefd4; }");

  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  // Применяем класс к label
  gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
  gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top(label, 10);
  gtk_widget_set_margin_bottom(label, 10);
  gtk_widget_set_margin_start(label, 20);
  gtk_widget_set_margin_end(label, 20);

  gtk_box_append(GTK_BOX(box), label);

  TimerData *timer_data = g_malloc(sizeof(TimerData));
  timer_data->label = GTK_LABEL(label);
  timer_data->seconds_remaining = config->time - 1;
  timer_data->app = app;

  g_timeout_add_seconds(1, update_label, timer_data);

  gtk_window_present(window);
}

void print_usage() {
  printf("Usage: cclock [options]\n");
  printf("Simple countdown timer with GTK overlay window.\n\n");
  printf("Options:\n");
  printf("  -h, --help               Show this help message and exit\n");
  printf("  -s, --seconds <num>      Set countdown seconds (e.g., -s 30)\n");
  printf("  -m, --minutes <num>      Set countdown minutes (e.g., -m 5)\n");
  printf("  -H, --hours <num>        Set countdown hours (e.g., -H 1)\n");
  printf("\n");
  printf("Examples:\n");
  printf("  cclock -H 1 -m 5 -s 30    Countdown for 1 hour, 5 minutes and 30 "
         "seconds\n");
}

int main(int argc, char **argv) {
  Config config;
  int opt, opt_index = 0;
  GtkApplication *app;
  int status;

  if (argc == 1) {
    fprintf(stderr, "Error: you must specify at least the time.\n");
    print_usage();
    return 1;
  }

  struct option options[] = {
      {"help", no_argument, 0, 'h'},
      {"seconds", required_argument, 0, 's'},
      {"minutes", required_argument, 0, 'm'},
      {"hours", required_argument, 0, 'H'},
  };

  while ((opt = getopt_long(argc, argv, "hs:m:H:", options, &opt_index)) !=
         -1) {
    switch (opt) {
    case 'h':
      print_usage();
      return 0;
    case 's':
      config.time += atoi(optarg);
      break;
    case 'm':
      config.time += atoi(optarg) * 60;
      break;
    case 'H':
      config.time += atoi(optarg) * 60 * 60;
      break;
    default:
      print_usage();
      return 1;
    }
  }

  app = gtk_application_new("org.gtk.test", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), &config);
  status = g_application_run(G_APPLICATION(app), 0, NULL);
  g_object_unref(app);

  return status;
}
