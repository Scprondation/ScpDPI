# Core API

Public headers are under `src/core/include/scpdpi`. Link `scpdpi_core` for platform providers, or inject test providers into `CoreEngine`.

```cpp
auto providers = scpdpi::make_platform_providers();
scpdpi::CoreEngine engine(providers.interfaces, providers.routes);
auto snapshot = engine.scan();
if (!snapshot) {
    // snapshot.error().message and native_code contain the failure
}
```

`EngineSnapshot` contains interfaces, routes, VPN-like interfaces, diagnostics, and aggregate telemetry. `CoreEngine::classify(snapshot, destination)` returns `Vpn`, `Direct` or `Unknown` for a destination address. The classifier uses longest-prefix route selection and metric as a tie-breaker; its route view is not process-specific.

`PolicyEngine::rules_for_profile(name)` supplies built-in 0.1.0 rules. `evaluate` returns a `PolicyDecision` for the classification. In the absence of an active match, it returns Bypass. These are observation/policy decisions only; no packet enforcement is implemented.

`ConfigStore` loads and saves version `0.1.0` JSON. It rejects unsupported schema versions, invalid scan intervals, unknown profiles, and any unknown-route action other than `bypass`. `parse_ipc_request` accepts only the fixed IPC command allowlist and validates profile names.

All APIs return `Result<T>` for expected provider, parsing, and storage errors. Exceptions are reserved for programmer misuse of `Result::value()` and unexpected library failures handled at the executable boundary.
