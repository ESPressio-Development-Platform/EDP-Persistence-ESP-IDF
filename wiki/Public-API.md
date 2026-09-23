# Public API

VfsFileStorage implements mandatory file operations plus directory/enumeration, rename, append and ranged-write facilities supported by the concrete contract. NvsKeyValueStorage exposes bounded opaque blob persistence through the KeyValueStorage contract.

NVS keys are constrained by the native 15-byte ASCII identity. Nonrepresentable EDP keys are reported explicitly rather than silently transformed. Empty values are supported naturally by the native blob representation.

Exact declarations remain authoritative in the exported headers.
