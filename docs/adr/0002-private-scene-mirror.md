# 0002. Mirror scenes privately while sharing sources

- **Status:** Accepted
- **Date:** 2026-11-17

## Context

The recording composition has to look like the live scene, with some items hidden. There are three ways to build it:

1. **Duplicate the scene collection.** This creates a second game capture, a second webcam, a second browser session and so on. Some devices can't be opened twice, and doubling every capture wastes resources.
2. **Toggle visibility on the live scene.** This changes what viewers see.
3. **Mirror the scenes privately.** Build private scenes (`obs_scene_create_private`) that contain the *same* source objects as the originals, with copied item properties.

## Decision

Use option 3. The plugin owns the private scene containers and scene items. It shares the capture sources with OBS by reference. Nested scenes and groups are mirrored recursively.

## Consequences

- No capture source is duplicated, and capture devices are opened only once.
- Source filters come along automatically, because they belong to the shared source. Scene filters have to be copied onto the private scene.
- The mirror has to track every structural change through scene signals. Phase 3 covers this.
- Private scenes don't appear in the OBS UI and aren't saved with the scene collection, so the mirror is rebuilt on every collection load.
- Shared sources that render differently depending on context, such as browser sources and sources that react to being shown, need dedicated testing.
