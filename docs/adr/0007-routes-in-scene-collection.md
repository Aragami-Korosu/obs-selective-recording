# 0007. Store routes in the scene collection

- **Status:** Accepted
- **Date:** 2026-11-17

## Context

Routes are keyed by source UUID, and UUIDs belong to a scene collection. If the plugin kept routes in its own config file, it would have to track which collection each route came from, and routes would go stale when collections are renamed, duplicated or imported.

## Decision

- **Routing profiles** live in the scene collection, under the `selective-recording` key, through `obs_frontend_add_save_callback`. They're saved and loaded with everything else in the collection.
- **Recording settings** (encoders, folders, resolution, warnings) and hotkeys are about the machine rather than the scenes, so they stay in the plugin's config folder.

## Consequences

- Duplicating or exporting a scene collection carries its routes with it.
- Switching collections switches routes automatically. The integration tests check that routes come back after a switch while recording.
- After a route change, the plugin asks OBS to save the collection three seconds later, so a crash doesn't lose recent changes.
