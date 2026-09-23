# Private Implementation

VFS success at PowerLoss retention may require file-content synchronization before close, while directory/metadata durability still depends on the mounted backend. The provider must not claim stronger semantics than that backend can deliver.

NVS uses fixed scratch storage for bounded/truncated reads and does not expose native handles as part of the EDP contract.
