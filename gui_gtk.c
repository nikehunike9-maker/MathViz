#include "gui.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <gdk/gdkkeysyms.h>

static const char default_dsl[] =
    "TITLE: Curtain Model\n"
    "RANGE: -2 12 -2 8\n"
    "POINT: A START: 1 1 COLOR: #333\n"
    "POINT: B START: 9 1 COLOR: #333\n"
    "POINT: C START: 5 6 COLOR: #2ecc71\n"
    "POINT: P START: 1 1 MOVE: 9 1 SPEED: 2 COLOR: #e74c3c\n"
    "LINE: A B LABEL: AB COLOR: #999\n"
    "LINE: A C LABEL: AC COLOR: #3498db\n"
    "LINE: B C LABEL: BC COLOR: #3498db\n"
    "LINE: P C LABEL: PC COLOR: #e74c3c\n"
    "AREA: triangle A P C LABEL: S_APC COLOR: rgba(231,76,60,0.25)\n"
    "AREA: triangle B P C LABEL: S_BPC COLOR: rgba(52,152,219,0.25)\n"
    "OUTPUT: length A P LABEL: AP COLOR: #e74c3c\n"
    "OUTPUT: length P B LABEL: PB COLOR: #3498db\n"
    "OUTPUT: area A P C LABEL: S_APC COLOR: #e74c3c\n"
    "OUTPUT: area B P C LABEL: S_BPC COLOR: #3498db\n"
    "OUTPUT: area A B C LABEL: S_ABC COLOR: #9b59b6\n"
    "    CHECK: P.x > 4 MESSAGE: P 越过中点!\n"
    "KEYPOINT: 4.0 中点\n";

static void on_timeline_seek(GtkRange *range, AppState *state);
static void update_zoom_label(AppState *state);

// ── Draw callback ──
static gboolean on_draw(GtkWidget *widget, cairo_t *cr, AppState *state) {
    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);
    state->drawW = alloc.width;
    state->drawH = alloc.height;
    state->cam.width = alloc.width;
    state->cam.height = alloc.height;

    // Clear background (#f8f9fa)
    cairo_set_source_rgb(cr, 0xf8/255.0, 0xf9/255.0, 0xfa/255.0);
    cairo_paint(cr);

    // Render scene
    if (state->hasData) {
        renderer_render(&state->cam, &state->interp.data,
                       state->interp.positions, state->interp.numPositions,
                       state->interp.time, state->interp.outputValues, state->interp.numOutputs,
                       state->interp.trailPoints, state->interp.trailLen,
                       cr);
    }
    return FALSE;
}

// ── Mouse interaction ──
static gboolean on_draw_button(GtkWidget *widget, GdkEventButton *event, AppState *state) {
    if (event->button == 1) {
        state->dragging = true;
        state->dragStartX = event->x;
        state->dragStartY = event->y;
        state->camStartX = state->cam.camX;
        state->camStartY = state->cam.camY;
        gtk_widget_grab_focus(widget);
    }
    return TRUE;
}

static gboolean on_draw_release(GtkWidget *widget, GdkEventButton *event, AppState *state) {
    if (event->button == 1) state->dragging = false;
    return TRUE;
}

static gboolean on_draw_motion(GtkWidget *widget, GdkEventMotion *event, AppState *state) {
    if (state->dragging) {
        double dx = event->x - state->dragStartX;
        double dy = event->y - state->dragStartY;
        state->cam.camX = state->camStartX - dx / state->cam.zoom;
        state->cam.camY = state->camStartY + dy / state->cam.zoom;
        gtk_widget_queue_draw(state->drawArea);
        update_zoom_label(state);
    }
    return TRUE;
}

static gboolean on_draw_scroll(GtkWidget *widget, GdkEventScroll *event, AppState *state) {
    double factor = (event->direction == GDK_SCROLL_UP) ? 1.1 : 0.9;
    state->cam.zoom = fmax(5, fmin(5000, state->cam.zoom * factor));
    gtk_widget_queue_draw(state->drawArea);
    update_zoom_label(state);
    return TRUE;
}

// ── Measurements panel ──
typedef struct { AppState *s; int idx; } MeasCtx;
static void on_meas_toggled(GtkToggleButton *btn, gpointer data) {
    MeasCtx *p = (MeasCtx*)data;
    p->s->interp.data.outputs[p->idx].showOnCanvas = gtk_toggle_button_get_active(btn);
    gtk_widget_queue_draw(p->s->drawArea);
}
static gboolean on_color_dot_draw(GtkWidget *w, cairo_t *cr, gpointer data) {
    int idx = GPOINTER_TO_INT(data); (void)idx;
    cairo_set_source_rgba(cr, 0.2, 0.2, 0.7, 1);
    cairo_arc(cr, 5, 5, 4, 0, 6.2832);
    cairo_fill(cr);
    return FALSE;
}
static void update_measurements(AppState *state) {
    GList *ch = gtk_container_get_children(GTK_CONTAINER(state->measureBox));
    for (GList *p = ch; p; p = p->next)
        gtk_widget_destroy(GTK_WIDGET(p->data));
    g_list_free(ch);

    if (!state->hasData || state->interp.numOutputs == 0) {
        gtk_widget_hide(state->measureFrame);
        return;
    }
    gtk_widget_show(state->measureFrame);

    for (int i = 0; i < state->interp.numOutputs; i++) {
        double val = state->interp.outputValues[i];
        Output *out = &state->interp.data.outputs[i];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_widget_set_margin_top(row, 1);
        gtk_widget_set_margin_bottom(row, 1);
        gtk_widget_set_margin_start(row, 4);
        gtk_widget_set_margin_end(row, 4);

        // Checkbox to toggle showOnCanvas
        GtkWidget *cb = gtk_check_button_new();
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb), out->showOnCanvas);
        MeasCtx *ud = g_new(MeasCtx, 1);
        ud->s = state; ud->idx = i;
        g_signal_connect_data(cb, "toggled", G_CALLBACK(on_meas_toggled), ud, (GClosureNotify)g_free, 0);
        gtk_box_pack_start(GTK_BOX(row), cb, FALSE, FALSE, 0);

        // Color indicator
        GtkWidget *colorDot = gtk_drawing_area_new();
        gtk_widget_set_size_request(colorDot, 10, 10);
        g_signal_connect(colorDot, "draw", G_CALLBACK(on_color_dot_draw), GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(row), colorDot, FALSE, FALSE, 0);

        // Label
        char buf[128];
        if (isnan(val))
            snprintf(buf, sizeof(buf), "%s = ?", out->label);
        else if (val == (int)val)
            snprintf(buf, sizeof(buf), "%s = %d", out->label, (int)val);
        else
            snprintf(buf, sizeof(buf), "%s = %.2f", out->label, val);
        GtkWidget *lbl = gtk_label_new(buf);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(row), lbl, TRUE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(state->measureBox), row, FALSE, FALSE, 0);
    }
    gtk_widget_show_all(state->measureFrame);
}

// ── Check dialog ──
static void show_check_dialog(AppState *state) {
    if (state->interp.checkMessage[0] == 0) return;

    char buf[1024];
    snprintf(buf, sizeof(buf),
             "检查触发于 t=%.1fs\n\n%s",
             state->interp.time, state->interp.checkMessage);

    // Append recorded values
    if (state->interp.numRecorded > 0) {
        char rec[512] = "\n\n记录值:";
        for (int i = 0; i < state->interp.numRecorded; i++) {
            char line[64];
            if (isnan(state->interp.recordedValues[i]))
                snprintf(line, sizeof(line), "\n  %s = ?",
                         state->interp.recordedLabels[i]);
            else if (state->interp.recordedValues[i] == (int)state->interp.recordedValues[i])
                snprintf(line, sizeof(line), "\n  %s = %d",
                         state->interp.recordedLabels[i],
                         (int)state->interp.recordedValues[i]);
            else
                snprintf(line, sizeof(line), "\n  %s = %.2f",
                         state->interp.recordedLabels[i],
                         state->interp.recordedValues[i]);
            strncat(rec, line, 511 - strlen(rec));
        }
        strncat(buf, rec, 1023 - strlen(buf));
    }

    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(state->window),
        GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
        "%s", buf);
    gtk_window_set_title(GTK_WINDOW(d), "检查事件");
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}

// ── Toast notification ──
static gboolean on_toast_timeout(gpointer data) {
    AppState *s = (AppState*)data;
    gtk_widget_set_visible(s->toastLabel, FALSE);
    s->toastTimerId = 0;
    return G_SOURCE_REMOVE;
}
static void show_toast(AppState *state, const char *msg) {
    if (!msg || !*msg) return;
    gtk_label_set_text(GTK_LABEL(state->toastLabel), msg);
    gtk_widget_set_visible(state->toastLabel, TRUE);
    if (state->toastTimerId > 0) g_source_remove(state->toastTimerId);
    state->toastTimerId = g_timeout_add(3000, on_toast_timeout, state);
}

// ── Update zoom label ──
static void update_zoom_label(AppState *state) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", (int)(state->cam.zoom / 80.0 * 100.0));
    gtk_label_set_text(GTK_LABEL(state->zoomLabel), buf);
}

// ── Animation tick ──
static gboolean animate_tick(AppState *state) {
    if (state->hasData && state->interp.playing) {
        double now = g_get_monotonic_time() / 1000000.0;
        double dt = fmin(now - state->lastTime, 0.1);
        state->lastTime = now;
        state->fps = 1.0 / fmax(dt, 0.001);

        bool triggered = interp_update(&state->interp, dt * state->speed);

        if (state->interp.data.totalTime > 0) {
            double pct = state->interp.time / state->interp.data.totalTime;
            g_signal_handlers_block_by_func(state->timeline, on_timeline_seek, state);
            gtk_range_set_value(GTK_RANGE(state->timeline), pct);
            g_signal_handlers_unblock_by_func(state->timeline, on_timeline_seek, state);
        }

        char buf[64];
        snprintf(buf, sizeof(buf), "时间: %.1fs", state->interp.time);
        gtk_label_set_text(GTK_LABEL(state->timeLabel), buf);
        snprintf(buf, sizeof(buf), "帧率: %.0f", state->fps);
        gtk_label_set_text(GTK_LABEL(state->fpsLabel), buf);

        update_measurements(state);
        gtk_widget_queue_draw(state->drawArea);

        if (triggered) {
            show_check_dialog(state);
            // Show toast
            char toast[128];
            snprintf(toast, sizeof(toast), "✓ t=%.1fs  %s", state->interp.time,
                     state->interp.checkMessage[0] ? state->interp.checkMessage : "检查触发");
            show_toast(state, toast);
            // Add timeline marker
            double totalT = state->interp.data.totalTime > 0 ? state->interp.data.totalTime : 1;
            double pct = state->interp.time / totalT;
            gtk_scale_add_mark(GTK_SCALE(state->timeline), pct, GTK_POS_BOTTOM, NULL);
        }
    }
    return G_SOURCE_CONTINUE;
}

// ── Seek helper ──
static void seek_to(AppState *state, double t) {
    if (!state->hasData) return;
    interp_seek(&state->interp, t);
    state->interp.playing = false;
    gtk_button_set_label(GTK_BUTTON(state->playBtn), "播放 [>]");
    double pct = state->interp.data.totalTime > 0 ? t / state->interp.data.totalTime : 0;
    g_signal_handlers_block_by_func(state->timeline, on_timeline_seek, state);
    gtk_range_set_value(GTK_RANGE(state->timeline), pct);
    g_signal_handlers_unblock_by_func(state->timeline, on_timeline_seek, state);
    char buf[64];
    snprintf(buf, sizeof(buf), "时间: %.1fs", state->interp.time);
    gtk_label_set_text(GTK_LABEL(state->timeLabel), buf);
    update_measurements(state);
    gtk_widget_queue_draw(state->drawArea);
}

// ── Keypoint button callback ──
typedef struct { AppState *state; double time; } KpClick;
static void on_kp_clicked(GtkButton *btn, gpointer data) {
    KpClick *kc = (KpClick*)data;
    seek_to(kc->state, kc->time);
}

// ── Keypoint buttons update ──
static void update_keypoints(AppState *state) {
    GList *ch = gtk_container_get_children(GTK_CONTAINER(state->keypointBox));
    for (GList *p = ch; p; p = p->next)
        gtk_widget_destroy(GTK_WIDGET(p->data));
    g_list_free(ch);

    // Clear existing timeline marks
    gtk_scale_clear_marks(GTK_SCALE(state->timeline));

    if (!state->hasData || state->interp.data.numKeypoints == 0) {
        gtk_widget_hide(state->keypointBox);
        return;
    }
    gtk_widget_show(state->keypointBox);

    double totalT = state->interp.data.totalTime > 0 ? state->interp.data.totalTime : 1;
    for (int i = 0; i < state->interp.data.numKeypoints; i++) {
        Keypoint *kp = &state->interp.data.keypoints[i];
        char label[64];
        if (kp->label[0]) snprintf(label, sizeof(label), "%s", kp->label);
        else snprintf(label, sizeof(label), "%.1fs", kp->time);
        GtkWidget *btn = gtk_button_new_with_label(label);
        gtk_widget_set_size_request(btn, -1, 24);
        KpClick *kc = g_new(KpClick, 1);
        kc->state = state; kc->time = kp->time;
        g_signal_connect_data(btn, "clicked", G_CALLBACK(on_kp_clicked), kc, (GClosureNotify)g_free, 0);
        gtk_box_pack_start(GTK_BOX(state->keypointBox), btn, FALSE, FALSE, 2);

        // Add timeline marker
        double pct = kp->time / totalT;
        gtk_scale_add_mark(GTK_SCALE(state->timeline), pct, GTK_POS_TOP, NULL);
    }
    gtk_widget_show_all(state->keypointBox);
}

// ── DSL execution ──
static void run_dsl(AppState *state) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(state->textBuffer, &start, &end);
    char *text = gtk_text_buffer_get_text(state->textBuffer, &start, &end, FALSE);

    interp_init(&state->interp, text);
    state->hasData = state->interp.data.numPoints > 0;

    if (state->hasData) {
        double rw = state->interp.data.xMax - state->interp.data.xMin;
        double rh = state->interp.data.yMax - state->interp.data.yMin;
        state->cam.camX = (state->interp.data.xMin + state->interp.data.xMax) / 2.0;
        state->cam.camY = (state->interp.data.yMin + state->interp.data.yMax) / 2.0;
        if (state->cam.width > 0 && state->cam.height > 0)
            state->cam.zoom = fmin((double)state->cam.width / rw, (double)state->cam.height / rh) * 0.85;

        g_signal_handlers_block_by_func(state->timeline, on_timeline_seek, state);
        gtk_range_set_value(GTK_RANGE(state->timeline), 0);
        g_signal_handlers_unblock_by_func(state->timeline, on_timeline_seek, state);

        char buf[64];
        snprintf(buf, sizeof(buf), "时间: %.1fs", state->interp.time);
        gtk_label_set_text(GTK_LABEL(state->timeLabel), buf);

        snprintf(buf, sizeof(buf), "已加载 %d 个点", state->interp.data.numPoints);
        guint cid = gtk_statusbar_get_context_id(GTK_STATUSBAR(state->statusBar), "msg");
        gtk_statusbar_push(GTK_STATUSBAR(state->statusBar), cid, buf);

        update_measurements(state);
        update_keypoints(state);
        update_zoom_label(state);
        gtk_widget_queue_draw(state->drawArea);
    }
    g_free(text);
}

// ── Button callbacks ──
static void on_play_clicked(GtkButton *btn, AppState *state) {
    if (!state->hasData) return;
    state->interp.playing = !state->interp.playing;
    if (state->interp.playing && state->interp.time >= state->interp.data.totalTime
        && !state->interp.data.hasInfiniteLoop)
        interp_seek(&state->interp, 0);
    gtk_button_set_label(btn, state->interp.playing ? "暂停 [| |]" : "播放 [>]");
}

static void on_reset_clicked(GtkButton *btn, AppState *state) {
    if (!state->hasData) return;
    interp_seek(&state->interp, 0);
    state->interp.playing = false;
    gtk_button_set_label(GTK_BUTTON(state->playBtn), "播放 [>]");

    g_signal_handlers_block_by_func(state->timeline, on_timeline_seek, state);
    gtk_range_set_value(GTK_RANGE(state->timeline), 0);
    g_signal_handlers_unblock_by_func(state->timeline, on_timeline_seek, state);

    char buf[64];
    snprintf(buf, sizeof(buf), "时间: %.1fs", state->interp.time);
    gtk_label_set_text(GTK_LABEL(state->timeLabel), buf);
    update_measurements(state);
    gtk_widget_queue_draw(state->drawArea);
}

static void on_run_clicked(GtkButton *btn, AppState *state) {
    run_dsl(state);
}

static void on_timeline_seek(GtkRange *range, AppState *state) {
    if (!state->hasData) return;
    double pct = gtk_range_get_value(range);
    interp_seek(&state->interp, pct * state->interp.data.totalTime);

    char buf[64];
    snprintf(buf, sizeof(buf), "时间: %.1fs", state->interp.time);
    gtk_label_set_text(GTK_LABEL(state->timeLabel), buf);
    update_measurements(state);
    gtk_widget_queue_draw(state->drawArea);
}

static void on_speed_changed(GtkComboBox *combo, AppState *state) {
    state->speed = gtk_combo_box_get_active(combo) + 1;
}

// ── Keyboard handler ──
static void show_about(AppState *state) {
    GtkWidget *d = gtk_about_dialog_new();
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(d), "MathViz");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(d), "1.0.0");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(d), "DSL 驱动的数学可视化平台\n\n语法设计者: kevion\nAI 生成项目，仅作娱乐与学习用途");
    const char *authors[] = {"AI (Codeium/DeepSeek)", NULL};
    gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(d), authors);
    gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(d), GTK_LICENSE_GPL_3_0);
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(d), "https://github.com/nikehunike9-maker/MathViz");
    gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(d), "GitHub");
    const char *credits =
        "本软件使用了以下开源项目：\n\n"
        "GTK+3          GNU LGPL 2.1\n"
        "Cairo          LGPL 2.1 / MPL 1.1\n"
        "Pango          GNU LGPL 2.1\n"
        "GLib           GNU LGPL 2.1\n"
        "GDK-Pixbuf     GNU LGPL 2.1\n"
        "ATK            GNU LGPL 2.1\n"
        "FreeType       FTL / GPL 2.0\n"
        "Fontconfig     MIT\n"
        "HarfBuzz       MIT\n"
        "libpng         libpng license\n"
        "zlib           MIT\n\n"
        "感谢以上所有项目的贡献者！";
     gtk_about_dialog_set_license(GTK_ABOUT_DIALOG(d), credits);
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}

static void show_shortcuts(AppState *state) {
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(state->window),
        GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
        "数学可视化平台 - 快捷键\n\n"
        "  Space       播放 / 暂停 (画布聚焦时)\n"
        "  R           重置到起点 (画布聚焦时)\n"
        "  Ctrl+Enter  运行 / 重新解析 DSL\n"
        "  + / -       放大 / 缩小\n"
        "  0           重置视角\n"
        "  方向键      平移画布\n"
        "  1-5         动画速度\n"
        "  N           下一个关键点\n"
        "  H           显示帮助\n"
        "  Ctrl+O      打开 DSL 文件\n"
        "  Ctrl+S      保存 DSL 文件\n"
        "  Ctrl+1~9    跳转到关键点\n"
        "  Q / Esc     退出");
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}

static gboolean on_key_pressed(GtkEventControllerKey *ctrl, guint keyval,
                               guint keycode, GdkModifierType mod, AppState *state) {
    gboolean ctrlHeld = (mod & GDK_CONTROL_MASK) != 0;
    gboolean canvasFocused = gtk_widget_has_focus(state->drawArea);

    // Always respond to Ctrl+Enter
    if ((keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) && ctrlHeld) {
        run_dsl(state);
        return TRUE;
    }

    // Always respond to keypoint shortcuts
    if (state->hasData && keyval >= GDK_KEY_1 && keyval <= GDK_KEY_9) {
        if (ctrlHeld) {
            int kpIdx = keyval - GDK_KEY_1;
            if (kpIdx < state->interp.data.numKeypoints) {
                seek_to(state, state->interp.data.keypoints[kpIdx].time);
            }
            return TRUE;
        }
        if (!ctrlHeld && canvasFocused && keyval >= GDK_KEY_1 && keyval <= GDK_KEY_5) {
            state->speed = keyval - GDK_KEY_0;
            gtk_combo_box_set_active(GTK_COMBO_BOX(state->speedCombo), state->speed - 1);
            return TRUE;
        }
    }
    if (state->hasData && (keyval == GDK_KEY_n || keyval == GDK_KEY_N) && !ctrlHeld) {
        double curT = state->interp.time;
        int nextIdx = -1;
        for (int ki = 0; ki < state->interp.data.numKeypoints; ki++) {
            if (state->interp.data.keypoints[ki].time > curT + 0.01) { nextIdx = ki; break; }
        }
        if (nextIdx < 0 && state->interp.data.numKeypoints > 0) nextIdx = 0;
        if (nextIdx >= 0) seek_to(state, state->interp.data.keypoints[nextIdx].time);
        return TRUE;
    }

    if (!canvasFocused) {
        if (keyval == GDK_KEY_q || keyval == GDK_KEY_Q || keyval == GDK_KEY_Escape) {
            gtk_window_close(GTK_WINDOW(state->window));
            return TRUE;
        }
        return FALSE;
    }

    switch (keyval) {
    case GDK_KEY_space:
        if (state->hasData) {
            state->interp.playing = !state->interp.playing;
            if (state->interp.playing && state->interp.time >= state->interp.data.totalTime
                && !state->interp.data.hasInfiniteLoop)
                interp_seek(&state->interp, 0);
            gtk_button_set_label(GTK_BUTTON(state->playBtn),
                state->interp.playing ? "暂停 [| |]" : "播放 [>]");
        }
        return TRUE;
    case GDK_KEY_r: case GDK_KEY_R:
        if (ctrlHeld) return FALSE;
        on_reset_clicked(NULL, state);
        return TRUE;
    case GDK_KEY_plus: case GDK_KEY_KP_Add:
        state->cam.zoom = fmin(5000, state->cam.zoom * 1.3);
        gtk_widget_queue_draw(state->drawArea);
        update_zoom_label(state);
        return TRUE;
    case GDK_KEY_minus: case GDK_KEY_KP_Subtract:
        state->cam.zoom = fmax(5, state->cam.zoom / 1.3);
        gtk_widget_queue_draw(state->drawArea);
        update_zoom_label(state);
        return TRUE;
    case GDK_KEY_0:
        state->cam.zoom = 80;
        gtk_widget_queue_draw(state->drawArea);
        update_zoom_label(state);
        return TRUE;
    case GDK_KEY_1: case GDK_KEY_2: case GDK_KEY_3: case GDK_KEY_4: case GDK_KEY_5:
    case GDK_KEY_6: case GDK_KEY_7: case GDK_KEY_8: case GDK_KEY_9:
        if (ctrlHeld) {
            // Ctrl+N: jump to Nth keypoint (1-indexed)
            int kpIdx = keyval - GDK_KEY_1;
            if (state->hasData && kpIdx < state->interp.data.numKeypoints) {
                double t = state->interp.data.keypoints[kpIdx].time;
                interp_seek(&state->interp, t);
                state->interp.playing = false;
                gtk_button_set_label(GTK_BUTTON(state->playBtn), "播放 [>]");
                double pct = state->interp.data.totalTime > 0 ? t / state->interp.data.totalTime : 0;
                g_signal_handlers_block_by_func(state->timeline, on_timeline_seek, state);
                gtk_range_set_value(GTK_RANGE(state->timeline), pct);
                g_signal_handlers_unblock_by_func(state->timeline, on_timeline_seek, state);
                char buf[64];
                snprintf(buf, sizeof(buf), "时间: %.1fs", state->interp.time);
                gtk_label_set_text(GTK_LABEL(state->timeLabel), buf);
                update_measurements(state);
                gtk_widget_queue_draw(state->drawArea);
            }
            return TRUE;
        }// fall through to speed keys for 1-5
        if (keyval >= GDK_KEY_1 && keyval <= GDK_KEY_5) {
            state->speed = keyval - GDK_KEY_0;
            gtk_combo_box_set_active(GTK_COMBO_BOX(state->speedCombo), state->speed - 1);
        }
        return TRUE;
    case GDK_KEY_h: case GDK_KEY_H:
        if (ctrlHeld) return FALSE;
        show_shortcuts(state);
        return TRUE;
    case GDK_KEY_n: case GDK_KEY_N: {
        // N = next keypoint
        if (state->hasData && state->interp.data.numKeypoints > 0) {
            double curT = state->interp.time;
            int nextIdx = -1;
            for (int ki = 0; ki < state->interp.data.numKeypoints; ki++) {
                if (state->interp.data.keypoints[ki].time > curT + 0.01) { nextIdx = ki; break; }
            }
            if (nextIdx < 0) nextIdx = 0; // wrap around
            double t = state->interp.data.keypoints[nextIdx].time;
            interp_seek(&state->interp, t);
            state->interp.playing = false;
            gtk_button_set_label(GTK_BUTTON(state->playBtn), "播放 [>]");
            double pct = state->interp.data.totalTime > 0 ? t / state->interp.data.totalTime : 0;
            g_signal_handlers_block_by_func(state->timeline, on_timeline_seek, state);
            gtk_range_set_value(GTK_RANGE(state->timeline), pct);
            g_signal_handlers_unblock_by_func(state->timeline, on_timeline_seek, state);
            char buf[64];
            snprintf(buf, sizeof(buf), "时间: %.1fs", state->interp.time);
            gtk_label_set_text(GTK_LABEL(state->timeLabel), buf);
            update_measurements(state);
            gtk_widget_queue_draw(state->drawArea);
        }
        return TRUE;
    }
    case GDK_KEY_q: case GDK_KEY_Q:
    case GDK_KEY_Escape:
        gtk_window_close(GTK_WINDOW(state->window));
        return TRUE;
    case GDK_KEY_Up:
        state->cam.camY += 30.0 / state->cam.zoom;
        gtk_widget_queue_draw(state->drawArea);
        return TRUE;
    case GDK_KEY_Down:
        state->cam.camY -= 30.0 / state->cam.zoom;
        gtk_widget_queue_draw(state->drawArea);
        return TRUE;
    case GDK_KEY_Left:
        state->cam.camX -= 30.0 / state->cam.zoom;
        gtk_widget_queue_draw(state->drawArea);
        return TRUE;
    case GDK_KEY_Right:
        state->cam.camX += 30.0 / state->cam.zoom;
        gtk_widget_queue_draw(state->drawArea);
        return TRUE;
    default:
        return FALSE;
    }
}

// ── File operations ──
static void on_open(AppState *state) {
    GtkWidget *d = gtk_file_chooser_dialog_new("打开 DSL 文件",
        GTK_WINDOW(state->window), GTK_FILE_CHOOSER_ACTION_OPEN,
        "_取消", GTK_RESPONSE_CANCEL, "_打开", GTK_RESPONSE_ACCEPT, NULL);
    GtkFileFilter *f = gtk_file_filter_new();
    gtk_file_filter_add_pattern(f, "*.dsl");
    gtk_file_filter_add_pattern(f, "*.txt");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(d), f);

    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
        char *content; gsize len;
        if (g_file_get_contents(fn, &content, &len, NULL)) {
            gtk_text_buffer_set_text(state->textBuffer, content, len);
            g_free(content);
            run_dsl(state);
        }
        g_free(fn);
    }
    gtk_widget_destroy(d);
}

static void on_save(AppState *state) {
    GtkWidget *d = gtk_file_chooser_dialog_new("保存 DSL 文件",
        GTK_WINDOW(state->window), GTK_FILE_CHOOSER_ACTION_SAVE,
        "_取消", GTK_RESPONSE_CANCEL, "_保存", GTK_RESPONSE_ACCEPT, NULL);

    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(state->textBuffer, &start, &end);
        char *text = gtk_text_buffer_get_text(state->textBuffer, &start, &end, FALSE);
        g_file_set_contents(fn, text, -1, NULL);
        g_free(text);
        g_free(fn);
    }
    gtk_widget_destroy(d);
}

static void on_new(AppState *state) {
    gtk_text_buffer_set_text(state->textBuffer, state->defaultDsl, -1);
    run_dsl(state);
}

// ── Create GUI ──
AppState* gui_create(GtkApplication *app) {
    AppState *state = g_malloc0(sizeof(AppState));
    state->speed = 1;
    state->cam.zoom = 80;
    state->cam.width = 1;
    state->cam.height = 1;
    state->drawW = 1;
    state->drawH = 1;
    memcpy(state->defaultDsl, default_dsl, sizeof(default_dsl));

    // Window
    state->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(state->window), "数学可视化平台");
    gtk_window_set_default_size(GTK_WINDOW(state->window), 1280, 800);
    g_signal_connect(state->window, "destroy", G_CALLBACK(gui_destroy), state);

    // Keyboard capture (before focused widget)
    GtkEventController *kctrl = gtk_event_controller_key_new(state->window);
    gtk_event_controller_set_propagation_phase(kctrl, GTK_PHASE_CAPTURE);
    g_signal_connect(kctrl, "key-pressed", G_CALLBACK(on_key_pressed), state);

    // Main layout: vbox
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(state->window), vbox);

    // ── Menu bar ──
    GtkWidget *mb = gtk_menu_bar_new();

    GtkWidget *fm = gtk_menu_new();
    GtkWidget *fi = gtk_menu_item_new_with_label("文件");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(fi), fm);

#define ADD_MENU(menu, label, cb) do { \
    GtkWidget *_m = gtk_menu_item_new_with_label(label); \
    g_signal_connect_swapped(_m, "activate", G_CALLBACK(cb), state); \
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), _m); \
} while(0)

    ADD_MENU(fm, "新建",          on_new);
    ADD_MENU(fm, "打开",         on_open);
    ADD_MENU(fm, "保存",         on_save);
    gtk_menu_shell_append(GTK_MENU_SHELL(fm), gtk_separator_menu_item_new());
    { GtkWidget *_m = gtk_menu_item_new_with_label("退出");
      g_signal_connect_swapped(_m, "activate", G_CALLBACK(gtk_window_close), state->window);
      gtk_menu_shell_append(GTK_MENU_SHELL(fm), _m); }
    gtk_menu_shell_append(GTK_MENU_SHELL(mb), fi);

    GtkWidget *hm = gtk_menu_new();
    GtkWidget *hi = gtk_menu_item_new_with_label("帮助");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(hi), hm);
    ADD_MENU(hm, "快捷键",    show_shortcuts);
    gtk_menu_shell_append(GTK_MENU_SHELL(hm), gtk_separator_menu_item_new());
    ADD_MENU(hm, "关于",      show_about);
    gtk_menu_shell_append(GTK_MENU_SHELL(mb), hi);

    gtk_box_pack_start(GTK_BOX(vbox), mb, FALSE, FALSE, 0);
#undef ADD_MENU

    // ── Main paned ──
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), paned, TRUE, TRUE, 0);

    // ── Left: DSL editor ──
    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_paned_pack1(GTK_PANED(paned), sw, FALSE, FALSE);

    state->textView = gtk_text_view_new();
    state->textBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->textView));
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(state->textView), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(state->textView), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(sw), state->textView);
    gtk_widget_set_size_request(state->textView, 300, -1);
    gtk_text_buffer_set_text(state->textBuffer, default_dsl, strlen(default_dsl));

    // ── Right: Canvas + Controls + Measurements ──
    GtkWidget *rightBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_paned_pack2(GTK_PANED(paned), rightBox, TRUE, FALSE);
    gtk_paned_set_position(GTK_PANED(paned), 400);

    // Canvas Overlay (drawing area + keypoint buttons on top)
    state->canvasOverlay = gtk_overlay_new();
    state->drawArea = gtk_drawing_area_new();
    gtk_widget_set_can_focus(state->drawArea, TRUE);
    g_signal_connect(state->drawArea, "draw", G_CALLBACK(on_draw), state);

    gtk_widget_add_events(state->drawArea,
        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
        GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);
    g_signal_connect(state->drawArea, "button-press-event",   G_CALLBACK(on_draw_button),  state);
    g_signal_connect(state->drawArea, "button-release-event", G_CALLBACK(on_draw_release), state);
    g_signal_connect(state->drawArea, "motion-notify-event",  G_CALLBACK(on_draw_motion),  state);
    g_signal_connect(state->drawArea, "scroll-event",         G_CALLBACK(on_draw_scroll),  state);

    gtk_widget_set_size_request(state->drawArea, 400, 300);
    gtk_container_add(GTK_CONTAINER(state->canvasOverlay), state->drawArea);

    // Keypoint button bar (overlayed top-right)
    state->keypointBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_widget_set_halign(state->keypointBox, GTK_ALIGN_END);
    gtk_widget_set_valign(state->keypointBox, GTK_ALIGN_START);
    gtk_widget_set_margin_top(state->keypointBox, 8);
    gtk_widget_set_margin_end(state->keypointBox, 8);
    gtk_overlay_add_overlay(GTK_OVERLAY(state->canvasOverlay), state->keypointBox);

    // Zoom level label (bottom-right)
    state->zoomLabel = gtk_label_new("100%");
    gtk_widget_set_halign(state->zoomLabel, GTK_ALIGN_END);
    gtk_widget_set_valign(state->zoomLabel, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(state->zoomLabel, 6);
    gtk_widget_set_margin_end(state->zoomLabel, 8);
    gtk_widget_set_name(state->zoomLabel, "zoomLabel");
    gtk_overlay_add_overlay(GTK_OVERLAY(state->canvasOverlay), state->zoomLabel);

    // Toast notification (bottom-center)
    state->toastLabel = gtk_label_new("");
    gtk_widget_set_halign(state->toastLabel, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(state->toastLabel, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(state->toastLabel, 12);
    gtk_widget_set_name(state->toastLabel, "toastLabel");
    gtk_widget_set_visible(state->toastLabel, FALSE);
    // Style: green background, white text, rounded
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "#toastLabel { background: rgba(46,204,113,0.92); color: #fff; "
        "padding: 6px 16px; border-radius: 16px; font-size: 14px; font-weight: bold; }"
        "#zoomLabel { background: rgba(255,255,255,0.88); color: #666; "
        "padding: 2px 8px; border-radius: 4px; font-size: 11px; }", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(state->toastLabel),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    gtk_style_context_add_provider(gtk_widget_get_style_context(state->zoomLabel),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
    gtk_overlay_add_overlay(GTK_OVERLAY(state->canvasOverlay), state->toastLabel);

    gtk_box_pack_start(GTK_BOX(rightBox), state->canvasOverlay, TRUE, TRUE, 0);

    // Timeline
    state->timeline = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.001);
    gtk_scale_set_draw_value(GTK_SCALE(state->timeline), FALSE);
    gtk_widget_set_size_request(state->timeline, -1, 20);
    g_signal_connect(state->timeline, "value-changed", G_CALLBACK(on_timeline_seek), state);
    gtk_box_pack_start(GTK_BOX(rightBox), state->timeline, FALSE, FALSE, 0);

    // Controls bar
    GtkWidget *ctrlBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(rightBox), ctrlBox, FALSE, FALSE, 4);

    state->playBtn = gtk_button_new_with_label("播放 [>]");
    g_signal_connect(state->playBtn, "clicked", G_CALLBACK(on_play_clicked), state);
    gtk_box_pack_start(GTK_BOX(ctrlBox), state->playBtn, FALSE, FALSE, 0);

    GtkWidget *runBtn = gtk_button_new_with_label("运行 [>|]");
    g_signal_connect(runBtn, "clicked", G_CALLBACK(on_run_clicked), state);
    gtk_box_pack_start(GTK_BOX(ctrlBox), runBtn, FALSE, FALSE, 0);

    GtkWidget *resetBtn = gtk_button_new_with_label("重置 [0]");
    g_signal_connect(resetBtn, "clicked", G_CALLBACK(on_reset_clicked), state);
    gtk_box_pack_start(GTK_BOX(ctrlBox), resetBtn, FALSE, FALSE, 0);

    GtkWidget *slbl = gtk_label_new("速度:");
    gtk_box_pack_start(GTK_BOX(ctrlBox), slbl, FALSE, FALSE, 4);

    state->speedCombo = gtk_combo_box_text_new();
    for (int i = 1; i <= 5; i++) {
        char b[8]; snprintf(b, sizeof(b), "%dx", i);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state->speedCombo), b);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(state->speedCombo), 0);
    g_signal_connect(state->speedCombo, "changed", G_CALLBACK(on_speed_changed), state);
    gtk_box_pack_start(GTK_BOX(ctrlBox), state->speedCombo, FALSE, FALSE, 0);

    state->timeLabel = gtk_label_new("时间: 0.0s");
    gtk_box_pack_end(GTK_BOX(ctrlBox), state->timeLabel, FALSE, FALSE, 4);

    state->fpsLabel = gtk_label_new("帧率: 0");
    gtk_box_pack_end(GTK_BOX(ctrlBox), state->fpsLabel, FALSE, FALSE, 4);

    // Measurements panel
    state->measureFrame = gtk_frame_new("测量结果");
    GtkWidget *msw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(msw),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    state->measureBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(msw), state->measureBox);
    gtk_container_add(GTK_CONTAINER(state->measureFrame), msw);
    gtk_widget_set_size_request(state->measureFrame, -1, 150);
    gtk_box_pack_start(GTK_BOX(rightBox), state->measureFrame, FALSE, FALSE, 0);

    // Status bar
    state->statusBar = gtk_statusbar_new();
    gtk_box_pack_end(GTK_BOX(vbox), state->statusBar, FALSE, FALSE, 0);

    // ── Show all ──
    gtk_widget_show_all(state->window);

    // ── Animation timer ──
    state->lastTime = g_get_monotonic_time() / 1000000.0;
    state->animTimerId = g_timeout_add(16, (GSourceFunc)animate_tick, state);

    // Parse default DSL
    run_dsl(state);

    return state;
}

void gui_destroy(AppState *state) {
    if (state->animTimerId > 0)
        g_source_remove(state->animTimerId);
    g_free(state);
}
