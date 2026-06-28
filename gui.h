#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>
#include <stdbool.h>
#include "interpreter.h"
#include "renderer.h"

typedef struct {
    GtkWidget *window;
    GtkWidget *drawArea;
    GtkWidget *textView;
    GtkTextBuffer *textBuffer;
    GtkWidget *timeLabel;
    GtkWidget *fpsLabel;
    GtkWidget *statusBar;
    GtkWidget *playBtn;
    GtkWidget *speedCombo;
    GtkWidget *timeline;
    GtkWidget *measureBox;
    GtkWidget *measureFrame;
    GtkWidget *keypointBox;
    GtkWidget *canvasOverlay;
    GtkWidget *zoomLabel;
    GtkWidget *toastLabel;
    guint toastTimerId;

    Camera cam;
    InterpState interp;
    bool hasData;
    double lastTime;
    double fps;
    int speed;
    bool dragging;
    double dragStartX, dragStartY;
    double camStartX, camStartY;
    int drawW, drawH;

    guint animTimerId;
    char defaultDsl[4096];
} AppState;

AppState* gui_create(GtkApplication *app);
void gui_destroy(AppState *state);

#endif
