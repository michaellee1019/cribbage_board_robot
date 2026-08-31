# Generic JSON Printer Integration

## Summary

Extend the smarthome printer add-on with a generic JSON endpoint that converts ordered text into a 62 mm label and initiates printing. The endpoint will provide durable, at-most-once idempotency so clients such as the leaderboard can safely retry after losing an HTTP response.

The add-on will remain domain-neutral: it will not understand games, players, scores, turns, or winners. The caller supplies all printable content.

## Public API

Add `POST /text/print` with:

- `Content-Type: application/json`
- Required `Idempotency-Key` header containing 1–200 printable ASCII characters.
- This versioned request body:

```json
{
  "version": 1,
  "filename": "scorebot-game-42.png",
  "title": "CRIBBAGE",
  "lines": [
    "RED    121",
    "BLUE    96",
    "GREEN   80",
    "WHITE   67",
    "TURN: RED"
  ],
  "footer": "Printed {{Timestamp}}"
}
```

Validation rules:

- `version` must equal `1`.
- `filename`, `title`, and `footer` are optional.
- `lines` is required and contains one to six ordered strings.
- Each text field is limited to 256 Unicode characters; total text is limited to 2,000 characters.
- Preserve internal spaces but trim surrounding whitespace.
- Reject embedded newlines, control characters, unknown fields, unsupported versions, and content that cannot fit.
- Reject unknown `{{Variable}}` placeholders.

Provide these service-owned, case-sensitive variables:

- `{{Timestamp}}`: local time as `YYYY-MM-DD HH:MM:SS Z`
- `{{Date}}`: local date as `YYYY-MM-DD`
- `{{Time}}`: local time as `HH:MM:SS Z`

Resolve variables once on the first execution. An idempotent replay must retain the original rendered timestamp.

A successful response will preserve the existing printer success payload and add:

```json
{
  "status": "sent",
  "idempotency_key": "request-key",
  "idempotent_replay": false,
  "rendered_at": "timezone-aware ISO-8601 timestamp",
  "printed_label": {},
  "metrics": {}
}
```

## Implementation Changes

- Render black text on a white 720×390 monochrome canvas using the existing font, Pillow, `PNG_LABEL_SPEC`, label-analysis, archive, and dispatch infrastructure.
- Center and emphasize the optional title, render body lines uniformly in their supplied order, and render the footer smaller.
- Auto-fit using measured bounds. Shrink fonts within defined minimums; never truncate or clip.
- Archive the rendered PNG through `PrintedLabelStore` before dispatching it through the existing printer backend.
- Keep `/png`, template printing, archive management, and existing clients unchanged. Do not add a browser page.

Implement durable idempotency with SQLite in the add-on data directory:

- Validate and render before reserving a key so invalid input does not consume it.
- Hash the canonical validated request before variable substitution.
- Reserve the key atomically using a unique primary key and transaction.
- Persist the payload hash, state, timestamps, resolved render time, archived-label ID, HTTP status, and response body.
- Mark the operation as dispatching before calling the physical printer.
- Persist terminal state before returning the HTTP response.

Idempotency behavior:

- A new key archives and dispatches exactly once.
- The same key and payload after success returns the stored result with `idempotent_replay: true`.
- The same key with a different payload returns `409 idempotency_conflict`.
- A concurrent duplicate returns `409 in_progress` and never dispatches.
- A failure after dispatch begins is stored as `outcome_unknown`; later requests return that result and never print automatically.
- A stale reserved or dispatching record found after restart becomes `outcome_unknown`.
- A definite failure before physical dispatch may release the reservation and permit a safe retry.
- Retain successful records for at least 30 days and uncertain records indefinitely.

This provides at-most-once dispatch, not guaranteed exactly-once physical delivery. Uncertain outcomes favor a missing label over a duplicate.

## Test Plan

Add tests for:

- Request validation, limits, unsupported versions, unknown fields, and unknown variables.
- Deterministic timestamp substitution using an injected clock.
- Rendering at exactly 720×390 with ordered lines and preserved internal spaces.
- Auto-fitting and rejection when content cannot fit.
- A first request producing one archive entry and one dispatch.
- Identical replay returning the stored result without rendering, archiving, or dispatching again.
- Conflicting payloads, concurrent requests, and persistence across app reconstruction.
- Lost client responses followed by safe retries.
- Printer failures and stale operations becoming non-retriable `outcome_unknown` records.
- Safe retry after definite pre-dispatch failure.
- All existing printer routes and tests remaining unchanged.

Update the printer README with the request contract, variables, response behavior, at-most-once limitation, and a `curl` example.

## Assumptions

- Idempotency applies only to `/text/print`.
- SQLite is independent of MongoDB and stored alongside persistent add-on data.
- The existing direct-port LAN trust model remains unchanged.
- The leaderboard firmware client and its print trigger are follow-up work and are not implemented as part of this plan.
