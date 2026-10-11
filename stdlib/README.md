# Standard library

No VOID standard library is implemented yet. This directory records its intended
boundary: ordinary library behavior will use VOID source once the frontend can
compile it; allocation, tracing and trusted C bridges belong to the runtime.
The architecture proof does not introduce user-facing pointer or string types.
