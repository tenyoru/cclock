#include <getopt.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gtk4-layer-shell.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int time; // seconds
  char *task;
} Config;

typedef struct {
  Config config;
} AppState;

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

typedef struct {
  AppState *state;
  GtkApplication *app;
  GtkWindow *window;
  GtkSpinButton *hours;
  GtkSpinButton *minutes;
  GtkSpinButton *seconds;
  GtkEntry *task;
  GtkLabel *error_label;
} PickerData;

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

static void start_countdown(GtkApplication *app, Config *config) {
  GtkWindow *window;
  GtkWidget *label;
  GtkWidget *box;

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

static void start_picker(GtkApplication *app, AppState *state);

static void on_picker_start(GtkButton *button, gpointer user_data) {
  (void)button;

  PickerData *data = (PickerData *)user_data;

  int hours = gtk_spin_button_get_value_as_int(data->hours);
  int minutes = gtk_spin_button_get_value_as_int(data->minutes);
  int seconds = gtk_spin_button_get_value_as_int(data->seconds);
  int total_seconds = hours * 3600 + minutes * 60 + seconds;

  if (total_seconds <= 0) {
    gtk_label_set_text(data->error_label, "Set at least 1 second.");
    return;
  }

  if (data->state->config.task != NULL) {
    g_free(data->state->config.task);
    data->state->config.task = NULL;
  }

  const char *task_text = gtk_editable_get_text(GTK_EDITABLE(data->task));
  if (task_text != NULL && strlen(task_text) > 0) {
    data->state->config.task = g_strdup(task_text);
  }

  data->state->config.time = total_seconds;
  gtk_window_close(data->window);
  start_countdown(data->app, &data->state->config);
}

static void start_picker(GtkApplication *app, AppState *state) {
  PickerData *data = g_malloc0(sizeof(PickerData));
  data->state = state;
  data->app = app;

  GtkWidget *window = gtk_application_window_new(app);
  data->window = GTK_WINDOW(window);
  gtk_window_set_title(GTK_WINDOW(window), "Set countdown");
  gtk_window_set_default_size(GTK_WINDOW(window), 420, 220);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_top(box, 16);
  gtk_widget_set_margin_bottom(box, 16);
  gtk_widget_set_margin_start(box, 16);
  gtk_widget_set_margin_end(box, 16);
  gtk_window_set_child(GTK_WINDOW(window), box);

  GtkWidget *time_label = gtk_label_new("Custom time");
  gtk_widget_set_halign(time_label, GTK_ALIGN_START);
  gtk_box_append(GTK_BOX(box), time_label);

  GtkWidget *grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
  gtk_box_append(GTK_BOX(box), grid);

  GtkWidget *hours_label = gtk_label_new("Hours");
  gtk_widget_set_halign(hours_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), hours_label, 0, 0, 1, 1);
  GtkWidget *hours_spin = gtk_spin_button_new_with_range(0, 99, 1);
  data->hours = GTK_SPIN_BUTTON(hours_spin);
  gtk_grid_attach(GTK_GRID(grid), hours_spin, 1, 0, 1, 1);

  GtkWidget *minutes_label = gtk_label_new("Minutes");
  gtk_widget_set_halign(minutes_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), minutes_label, 2, 0, 1, 1);
  GtkWidget *minutes_spin = gtk_spin_button_new_with_range(0, 59, 1);
  data->minutes = GTK_SPIN_BUTTON(minutes_spin);
  gtk_grid_attach(GTK_GRID(grid), minutes_spin, 3, 0, 1, 1);

  GtkWidget *seconds_label = gtk_label_new("Seconds");
  gtk_widget_set_halign(seconds_label, GTK_ALIGN_START);
  gtk_grid_attach(GTK_GRID(grid), seconds_label, 4, 0, 1, 1);
  GtkWidget *seconds_spin = gtk_spin_button_new_with_range(0, 59, 1);
  data->seconds = GTK_SPIN_BUTTON(seconds_spin);
  gtk_grid_attach(GTK_GRID(grid), seconds_spin, 5, 0, 1, 1);

  GtkWidget *task_label = gtk_label_new("Task");
  gtk_widget_set_halign(task_label, GTK_ALIGN_START);
  gtk_box_append(GTK_BOX(box), task_label);

  GtkWidget *task_entry = gtk_entry_new();
  data->task = GTK_ENTRY(task_entry);
  if (state->config.task) {
    gtk_editable_set_text(GTK_EDITABLE(task_entry), state->config.task);
  }
  gtk_box_append(GTK_BOX(box), task_entry);

  GtkWidget *error = gtk_label_new("");
  data->error_label = GTK_LABEL(error);
  gtk_widget_add_css_class(error, "error");
  gtk_widget_set_halign(error, GTK_ALIGN_START);
  gtk_box_append(GTK_BOX(box), error);

  GtkWidget *start_button = gtk_button_new_with_label("Start countdown");
  g_signal_connect(start_button, "clicked", G_CALLBACK(on_picker_start), data);
  gtk_box_append(GTK_BOX(box), start_button);

  gtk_window_present(GTK_WINDOW(window));
}

static void activate(GtkApplication *app, gpointer user_data) {
  AppState *state = (AppState *)user_data;

  if (state->config.time > 0) {
    start_countdown(app, &state->config);
    return;
  }

  start_picker(app, state);
}

void print_usage() {
  printf("Usage: cclock [options]\n");
  printf("Simple countdown timer with GTK overlay window.\n");
  printf("If no time is provided, a custom time picker is shown.\n\n");
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
  AppState state = {0};
  int opt, opt_index = 0;
  GtkApplication *app;
  int status;

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
      state.config.time += atoi(optarg);
      break;
    case 'm':
      state.config.time += atoi(optarg) * 60;
      break;
    case 'H':
      state.config.time += atoi(optarg) * 60 * 60;
      break;
    case 't':
      state.config.task = g_strdup(optarg);
      break;
    default:
      print_usage();
      return 1;
    }
  }

  app = gtk_application_new("org.tenyoru.cclock", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), &state);
  status = g_application_run(G_APPLICATION(app), 0, NULL);
  if (state.config.task != NULL) {
    g_free(state.config.task);
  }
  g_object_unref(app);

  return status;
}
