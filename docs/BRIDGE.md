# Bridge protocol `zalonative.bridge/1`

Synchronous JSON calls, usable over QWebChannel (`BridgeObject::requestJson`)
or directly in C++ (`BridgeRouter::dispatch`).

## Envelope

```jsonc
// request
{"id": "web-1", "kind": "call", "path": "state.publish", "args": [{...}]}
// success
{"id": "web-1", "ok": true, "value": {...}}
// failure
{"id": "web-1", "ok": false, "error": {"code": "unknown_method", "message": "..."}}
```

- `id` is echoed when present.
- `kind` is `call` (`invoke` accepted as an alias); anything else is rejected.
- Requests are capped at 1 MiB of compact JSON.

## Methods

| Path            | Args                       | Returns                          |
|-----------------|----------------------------|----------------------------------|
| `app.ping`      | —                          | `{pong, time}`                   |
| `app.info`      | —                          | `{protocol, role}`               |
| `runtime.info`  | —                          | manifest JSON or `{}`            |
| `state.publish` | one object: `{conversations[], messages{}}` | `{accepted, conversations}` |
| `state.snapshot`| —                          | last published state or `{}`     |
| `log.write`     | one non-empty string (≤2000 chars logged) | `{written: true}`    |

Everything else → `unknown_method`. The renderer can never spawn processes,
read files, or open sockets through this bridge.

## Events (renderer → native is calls; native → renderer is events)

`BridgeObject.event(json)` carries `{kind:"event", path:"state.changed", value}`.
`stub.js` subscribes via QWebChannel when available.

## Error codes

`invalid_json`, `invalid_request`, `invalid_args`, `unknown_method`,
`unsupported_kind`, `payload_too_large`.
