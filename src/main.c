#include <getopt.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gtk4-layer-shell.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  int time; // seconds
  char *task;
} Config;

typedef struct {
  GtkLabel *label;
  int seconds_remaining;
  GtkApplication *app;
} TimerData;

typedef struct {
  char *text;
  double offset;
  double text_width;
  double widget_width;
  GtkWidget *area;
} MarqueeData;

static void marquee_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width,
                         int height, gpointer data) {
  (void)drawing_area;
  MarqueeData *md = (MarqueeData *)data;
  md->widget_width = width;

  // Measure text
  PangoLayout *layout = pango_cairo_create_layout(cr);
  pango_layout_set_text(layout, md->text, -1);
  PangoFontDescription *desc = pango_font_description_new();
  pango_font_description_set_absolute_size(desc, 36 * PANGO_SCALE);
  pango_layout_set_font_description(layout, desc);
  pango_font_description_free(desc);

  int tw, th;
  pango_layout_get_pixel_size(layout, &tw, &th);
  md->text_width = tw;

  cairo_set_source_rgba(cr, 252.0 / 255.0, 239.0 / 255.0, 212.0 / 255.0, 1.0);

  int padding = 10;
  double ty = (height - th) / 2.0;
  double tx;

  if (tw <= width - 2 * padding) {
    tx = (width - tw) / 2.0;
  } else {
    tx = (double)width - md->offset;
  }

  // Clip text inside border
  cairo_save(cr);
  cairo_rectangle(cr, padding, 0, width - 2 * padding, height);
  cairo_clip(cr);
  cairo_move_to(cr, tx, ty);
  pango_cairo_show_layout(cr, layout);
  cairo_restore(cr);

  g_object_unref(layout);
}

static gboolean marquee_tick(gpointer data) {
  MarqueeData *md = (MarqueeData *)data;

  if (md->text_width > md->widget_width) {
    md->offset += 1.5;
    if (md->offset > md->widget_width + md->text_width)
      md->offset = 0;
  }

  gtk_widget_queue_draw(md->area);
  return G_SOURCE_CONTINUE;
}

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
  (void)self;
  (void)keycode;
  (void)state;
  if (keyval == GDK_KEY_Escape) {
    gtk_window_close(GTK_WINDOW(user_data));
  }
  return FALSE;
}

static void on_window_realize(GtkWidget *widget, gpointer user_data) {
  (void)user_data;

  // Set empty input region to make window click-through
  GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(widget));
  cairo_region_t *region = cairo_region_create();
  gdk_surface_set_input_region(surface, region);
  cairo_region_destroy(region);
}

static inline void window_setup(GtkWindow *window) {
  gtk_window_set_title(window, "gui");
  // gtk_window_set_default_size(window, 400, 500);

  gtk_layer_init_for_window(GTK_WINDOW(window));
  gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
  gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
  gtk_layer_set_anchor(window, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);

  // Make window click-through (mouse events pass through)
  gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

  // Set up input region after window is realized
  g_signal_connect(window, "realize", G_CALLBACK(on_window_realize), NULL);
}

static void activate(GtkApplication *app, gpointer user_data) {
  GtkWindow *window;
  GtkWidget *label;
  GtkWidget *box;

  Config *config = (Config *)user_data;

  window = GTK_WINDOW(gtk_application_window_new(app));
  window_setup(window);

  box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child(window, box);

  label = gtk_label_new(format_time_label(config->time));
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_string(
      provider, "window { background-color: rgba(0, 0, 0, 0.3); }"
                "label { font-size: 72px; color: #fcefd4; }");

  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
  gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top(label, 10);
  gtk_widget_set_margin_bottom(label, 4);
  gtk_widget_set_margin_start(label, 20);
  gtk_widget_set_margin_end(label, 20);

  gtk_box_append(GTK_BOX(box), label);

  if (config->task) {
    MarqueeData *md = g_malloc(sizeof(MarqueeData));
    md->text = config->task;
    md->offset = 0;
    md->text_width = 0;
    md->widget_width = 0;

    GtkWidget *area = gtk_drawing_area_new();
    md->area = area;
    gtk_widget_set_size_request(area, -1, 55);
    gtk_widget_set_hexpand(area, TRUE);
    gtk_widget_set_margin_bottom(area, 0);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), marquee_draw, md, NULL);
    g_timeout_add(30, marquee_tick, md);
    gtk_box_append(GTK_BOX(box), area);
  }

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
  printf("  -t, --task <text>        Show task label below the timer\n");
  printf("\n");
  printf("Examples:\n");
  printf("  cclock -H 1 -m 5 -s 30    Countdown for 1 hour, 5 minutes and 30 "
         "seconds\n");
}

int main(int argc, char **argv) {
  Config config = {0};
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
      {"task", required_argument, 0, 't'},
  };

  while ((opt = getopt_long(argc, argv, "hs:m:H:t:", options, &opt_index)) !=
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
    case 't':
      config.task = optarg;
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
