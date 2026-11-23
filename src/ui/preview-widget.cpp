// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "ui/preview-widget.hpp"

#include <QWindow>

#if !defined(_WIN32) && !defined(__APPLE__)
#include <obs-nix-platform.h>
#endif

#include <algorithm>

namespace selective_recording {

namespace {

bool to_gs_window(QWindow *window, gs_window &gs)
{
	if (!window)
		return false;
#ifdef _WIN32
	gs.hwnd = (void *)window->winId();
#elif defined(__APPLE__)
	gs.view = (id)window->winId();
#else
	if (obs_get_nix_platform() != OBS_NIX_PLATFORM_X11_EGL)
		return false;
	gs.id = (uint32_t)window->winId();
	gs.display = obs_get_nix_platform_display();
#endif
	return true;
}

} // namespace

PreviewWidget::PreviewWidget(QWidget *parent) : QWidget(parent)
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);
	setMinimumHeight(120);
}

PreviewWidget::~PreviewWidget()
{
	destroy_display();
}

void PreviewWidget::set_canvas(obs_canvas_t *canvas)
{
	std::lock_guard lock(mutex_);
	canvas_ = canvas;
}

void PreviewWidget::create_display()
{
	if (display_ || !isVisible() || !windowHandle())
		return;

	const qreal ratio = devicePixelRatioF();
	gs_init_data info = {};
	info.cx = (uint32_t)std::max(1.0, width() * ratio);
	info.cy = (uint32_t)std::max(1.0, height() * ratio);
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;
	if (!to_gs_window(windowHandle(), info.window))
		return;

	display_ = obs_display_create(&info, 0x101010);
	if (display_)
		obs_display_add_draw_callback(display_, draw, this);
}

void PreviewWidget::destroy_display()
{
	if (!display_)
		return;
	obs_display_remove_draw_callback(display_, draw, this);
	obs_display_destroy(display_);
	display_ = nullptr;
}

void PreviewWidget::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	create_display();
}

void PreviewWidget::hideEvent(QHideEvent *event)
{
	destroy_display();
	QWidget::hideEvent(event);
}

void PreviewWidget::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	if (!display_) {
		create_display();
		return;
	}
	const qreal ratio = devicePixelRatioF();
	obs_display_resize(display_, (uint32_t)std::max(1.0, width() * ratio),
			   (uint32_t)std::max(1.0, height() * ratio));
}

void PreviewWidget::paintEvent(QPaintEvent *)
{
	create_display();
}

void PreviewWidget::draw(void *param, uint32_t cx, uint32_t cy)
{
	auto *self = static_cast<PreviewWidget *>(param);

	// Held while drawing so the last canvas reference is always released on the UI thread.
	std::lock_guard lock(self->mutex_);
	obs_canvas_t *canvas = self->canvas_;
	if (!canvas)
		return;

	obs_video_info ovi = {};
	if (!obs_canvas_get_video_info(canvas, &ovi) || !ovi.base_width || !ovi.base_height)
		return;

	// Letterbox the canvas into the widget.
	const float scale = std::min((float)cx / (float)ovi.base_width, (float)cy / (float)ovi.base_height);
	const int w = (int)((float)ovi.base_width * scale);
	const int h = (int)((float)ovi.base_height * scale);
	const int x = ((int)cx - w) / 2;
	const int y = ((int)cy - h) / 2;

	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0.0f, (float)ovi.base_width, 0.0f, (float)ovi.base_height, -100.0f, 100.0f);
	gs_set_viewport(x, y, w, h);
	obs_render_canvas_texture(canvas);
	gs_projection_pop();
	gs_viewport_pop();
}

} // namespace selective_recording
