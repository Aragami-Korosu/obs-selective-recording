# Architecture Decision Records

Each record captures one significant design decision: the context, what was decided, and the consequences. Records are never deleted. To reverse a decision, write a new record that supersedes the old one.

| ADR | Title | Status |
| --- | --- | --- |
| [0001](0001-obs-view-over-canvas-api.md) | Use `obs_view` instead of the Canvas API | Accepted, partly superseded by 0005 |
| [0002](0002-private-scene-mirror.md) | Mirror scenes privately while sharing sources | Accepted |
| [0003](0003-stream-exclusion.md) | How `recording_only` removes a source from the stream | **Proposed** |
| [0004](0004-independent-audio-mix.md) | Build an independent audio mix instead of using OBS tracks | Accepted, pending POC 2 |
| [0005](0005-multi-canvas-support.md) | Support multiple canvases, including Aitum Vertical Canvas | Accepted |

## Template

```markdown
# NNNN. Title

- **Status:** Proposed | Accepted | Superseded by NNNN
- **Date:** YYYY-MM-DD

## Context
## Decision
## Consequences
```
