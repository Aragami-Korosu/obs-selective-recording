# 0001. Use `obs_view` instead of the Canvas API

- **Status:** Accepted
- **Date:** 2026-11-17

## Context

The recording needs its own video mix, separate from OBS's main canvas. libobs offers two ways to do this:

- **`obs_view` with `obs_view_add2()`**: an established API. It adds a view to the render loop with its own `obs_video_info` and returns a `video_t` that encoders can connect to.
- **Canvas API** (`obs_canvas_*`): newer and more capable. However, its documentation describes it as early-stage and warns that it may change without notice.

## Decision

Build the video pipeline on `obs_view_add2()`, and keep it behind the `VideoPipeline` interface. Revisit the Canvas API once it's documented as stable.

## Consequences

- We depend only on stable libobs APIs.
- Because `VideoPipeline` isolates the rendering backend, switching to the Canvas API later should only affect that one component.
- We manage the view's lifecycle ourselves: `obs_view_remove()` has to run before the view is destroyed, and before the module unloads.
