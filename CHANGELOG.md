# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Project scaffolding based on the official OBS plugin template.
- An empty plugin module that loads and unloads cleanly and writes to the OBS log.
- Build configuration for Windows x64 with Qt6 and the OBS frontend API enabled.
- Project documentation: specification, architecture, roadmap, configuration schema, build guide and decision records.
- Multi-canvas design for dual output with Aitum Vertical Canvas 1.6.0+ (ADR-0005).
- Logging of every detected canvas (main, Aitum Vertical and others) on load and whenever canvases change.
- ADR-0003 accepted: the plugin never modifies a live composition, and keeping a source out of a stream is done by the user in OBS.
- Non-functional requirements NFR-001 to NFR-010 (stream protection, failure handling per canvas, warnings, runtime canvas changes) and recovery requirements (FR-009).
