# Composition

Both providers offer existing Persistence-domain capabilities and require one external EDP-Memory ByteOperations provider. Binding profiles participate in compile-time provider qualification.

No ESP-IDF runtime registry is added; Bootstrap constructs the concrete providers and supplies their native bindings explicitly.
