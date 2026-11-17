// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include <obs.hpp>

#include <QWidget>

#include <mutex>

namespace selective_recording {

// Draws the recording composition. The display only exists while the widget is visible,
// so a closed preview costs nothing.
class PreviewWidget : public QWidget {
	Q_OBJECT

public:
	explicit PreviewWidget(QWidget *parent = nullptr);
	~PreviewWidget() override;

	void set_canvas(obs_canvas_t *canvas);

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	QPaintEngine *paintEngine() const override { return nullptr; }

private:
	void create_display();
	void destroy_display();
	static void draw(void *param, uint32_t cx, uint32_t cy);

	obs_display_t *display_ = nullptr;
	std::mutex mutex_;
	OBSCanvas canvas_;
};

} // namespace selective_recording
