# Architecture

`VfsFileStorage<TBindingTag,TBindingProfile,TByteOps>` wraps an externally mounted ESP-IDF VFS subtree. `NvsKeyValueStorage<TBindingTag,TByteOps>` wraps an NVS namespace/partition. Mount/partition lifecycle remains outside the provider.

BindingTag creates distinct provider types. BindingProfile statically declares real substrate behaviour such as retention, path limits, case sensitivity, removability and invocation concurrency.
