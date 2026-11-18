# Architecture Decision Records

Each record captures one significant design decision: the context, what was decided, and the consequences. Records are never deleted. To reverse a decision, write a new record that supersedes the old one.

| ADR | Title | Status |
| --- | --- | --- |
| [0001](0001-obs-view-over-canvas-api.md) | Use `obs_view` instead of the Canvas API | Superseded by 0006 |
| [0002](0002-private-scene-mirror.md) | Mirror scenes privately while sharing sources | Accepted |
| [0003](0003-stream-exclusion.md) | Preserve the live composition; stream exclusion is user-managed | Accepted, amended by 0008 |
| [0004](0004-independent-audio-mix.md) | Build an independent audio mix instead of using OBS tracks | Accepted |
| [0005](0005-multi-canvas-support.md) | Support multiple canvases, including Aitum Vertical Canvas | Accepted |
| [0006](0006-private-canvas-rendering.md) | Render through a private canvas | Accepted |
| [0007](0007-routes-in-scene-collection.md) | Store routes in the scene collection | Accepted |
| [0008](0008-streamlabs-style-placements.md) | Streamlabs-style buttons set placements and hide Recording only from the stream | Accepted |
| [0009](0009-record-button.md) | OBS's Record button records with Selective Recording | Accepted |

## Template

```markdown
# NNNN. Title

- **Status:** Proposed | Accepted | Superseded by NNNN
- **Date:** YYYY-MM-DD

## Context
## Decision
## Consequences
```
