# Resources, Lifecycle and Concurrency

VFS operations use caller-owned buffers plus bounded provider scratch as needed. NVS blob support is explicitly bounded. The providers do not add locks to manufacture a concurrency guarantee; advertised InvocationConcurrency must be true of the full native binding configuration. No general ISR-safety guarantee is made.
