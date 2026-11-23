# Development

## Layout

| Path | What it is |
| :--- | :--- |
| `a1s-sendspin-source.yaml` | The example ESPHome configuration |
| `components/sendspin/__init__.py` | The component schema, code generation and ESP-IDF options |
| `components/sendspin/sendspin_hub.*` | The hub: client lifecycle, NVS persistence, pairing and trust |
| `components/sendspin/sendspin_source.*` | The audio source: capture, timestamps, line sensing and statistics |
| `.github/workflows/release.yml` | Tags and releases `master` |
| `AGENTS.md` | Rules for AI coding agents working in the repository |

## Iterating locally

To test component changes without pushing, point `external_components` at the local copy:

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [sendspin]
```

Then build with `esphome run a1s-sendspin-source.yaml` from a clone. Change it back to the GitHub source before committing.

## Rules to keep

The repository's `AGENTS.md` sets these, and they apply to anyone changing the code:

- Don't change the ES8388 register overrides unless you're diagnosing the codec. See [ES8388 registers](reference/registers.md).
- Keep the firmware capture-only. Don't add a speaker or media player on the I²S bus.
- The microphone data callback runs in the real-time task: no blocking I/O, large allocations or unthrottled logging. Do periodic work and logging in `loop()` or the streaming callbacks.
- Put large buffers in PSRAM (`task_stack_in_psram`, `MemoryLocation::PREFER_EXTERNAL`). The Opus state is the one deliberate exception, and is set in `__init__.py`.
- Keep logs throttled. Streaming statistics log every five seconds, and the `Pairing Token` sensor returns nothing when the token hasn't changed.
- Keep the README in step with configuration or code changes.

## Releases

Every push to `master` runs the Release workflow. It reads the Conventional Commits since the last `vX.Y.Z` tag and publishes a tagged GitHub release.

| Commits since the last tag | Bump |
| :--- | :--- |
| `feat!:` or `fix!:`, or a line starting `BREAKING CHANGE:` in the body | major |
| `feat:` | minor |
| `fix:` or `perf:` | patch |
| only `docs:`, `chore:`, `refactor:`, `ci:`, `test:`, `style:` | no release |

The format is `<type>(<optional scope>): <imperative summary>`. Scopes in use are `source`, `hub`, `codegen`, `config`, `deps`, `opus`, `audio`, `stream` and `api`. Pick the type by its effect on someone flashing the firmware, make one logical change per commit, and don't create tags by hand.

You can force a bump from **Actions → Release → Run workflow** (`auto`, `patch`, `minor` or `major`).
