CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS ?= -ICompiler/include
AR ?= ar
PYTHON ?= python3
TEST_JOBS ?=

DEBUG_LINE_DUMP := readelf --debug-dump=decodedline
DEBUG_SECTION_DUMP := readelf -S
EXE_SUFFIX :=
BIN := bin/voidc
ifeq ($(OS),Windows_NT)
BIN := bin/voidc.exe
EXE_SUFFIX := .exe
DEBUG_LINE_DUMP := objdump --dwarf=decodedline
DEBUG_SECTION_DUMP := objdump -h
endif
SOURCES := $(wildcard Compiler/src/*.c)
HEADERS := $(wildcard Compiler/include/*.h Compiler/include/*.def)
RUNTIME_HEADERS := $(wildcard Runtime/include/*.h)
RUNTIME_SOURCES := $(wildcard Runtime/src/*.c)
NATIVE_FIXTURE_DIR := Tests/NativeIntegration/native
NATIVE_FIXTURE_SOURCE := $(NATIVE_FIXTURE_DIR)/fixture.c
NATIVE_FIXTURE_OBJECT := $(NATIVE_FIXTURE_DIR)/fixture.o
NATIVE_FIXTURE := $(NATIVE_FIXTURE_DIR)/libvoidc040fixture.a
NATIVE_BUFFER_CALL_DIR := Tests/NativeBufferCallIntegration/native
NATIVE_BUFFER_CALL_SOURCE := $(NATIVE_BUFFER_CALL_DIR)/fixture.c
NATIVE_BUFFER_CALL_OBJECT := $(NATIVE_BUFFER_CALL_DIR)/fixture.o
NATIVE_BUFFER_CALL_FIXTURE := $(NATIVE_BUFFER_CALL_DIR)/libvoidc248fixture.a
NATIVE_FUNCTION_ADDRESS_DIR := Tests/NativeFunctionAddressIndirectCallCompletion/native
NATIVE_FUNCTION_ADDRESS_SOURCE := $(NATIVE_FUNCTION_ADDRESS_DIR)/fixture.c
NATIVE_FUNCTION_ADDRESS_OBJECT := $(NATIVE_FUNCTION_ADDRESS_DIR)/fixture.o
NATIVE_FUNCTION_ADDRESS_FIXTURE := $(NATIVE_FUNCTION_ADDRESS_DIR)/libvoidc256fixture.a
NATIVE_FUNCTION_ABI_DIR := Tests/NativeFunctionPointerAbiIntegration/native
NATIVE_FUNCTION_ABI_SOURCE := $(NATIVE_FUNCTION_ABI_DIR)/fixture.c
NATIVE_FUNCTION_ABI_OBJECT := $(NATIVE_FUNCTION_ABI_DIR)/fixture.o
NATIVE_FUNCTION_ABI_FIXTURE := $(NATIVE_FUNCTION_ABI_DIR)/libvoidc257fixture.a
NATIVE_FUNCTION_GENERIC_DIR := Tests/GenericFunctionPointerUnmanagedStorageIntegration/native
NATIVE_FUNCTION_GENERIC_SOURCE := $(NATIVE_FUNCTION_GENERIC_DIR)/fixture.c
NATIVE_FUNCTION_GENERIC_OBJECT := $(NATIVE_FUNCTION_GENERIC_DIR)/fixture.o
NATIVE_FUNCTION_GENERIC_FIXTURE := $(NATIVE_FUNCTION_GENERIC_DIR)/libvoidc258fixture.a
NATIVE_HEAP_FUNCTION_POINTER_INTEGRATION_TEST := Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/CConsumer/native-heap-function-pointer-integration-test
NATIVE_HEAP_FUNCTION_POINTER_AUDIT_DIR := Tests/NativeHeapFunctionPointerIntegrationAudit/Surface/native
NATIVE_HEAP_FUNCTION_POINTER_AUDIT_SOURCE := $(NATIVE_HEAP_FUNCTION_POINTER_AUDIT_DIR)/fixture.c
NATIVE_HEAP_FUNCTION_POINTER_AUDIT_OBJECT := $(NATIVE_HEAP_FUNCTION_POINTER_AUDIT_DIR)/fixture.o
NATIVE_HEAP_FUNCTION_POINTER_AUDIT_FIXTURE := $(NATIVE_HEAP_FUNCTION_POINTER_AUDIT_DIR)/libvoidc260audit.a
NATIVE_HEAP_FUNCTION_POINTER_AUDIT_TEST := Tests/NativeHeapFunctionPointerIntegrationAudit/CConsumer/native-heap-function-pointer-audit-test
NATIVE_HEAP_FUNCTION_POINTER_AUDIT_MEMORY_TEST := Tests/NativeHeapFunctionPointerIntegrationAudit/native-memory-contract-test
AWAITER_PROTOCOL_SEMANTIC_TEST := Tests/AwaiterProtocolSemanticFoundation/semantic_test
ASYNC_LAMBDA_SEMANTIC_TEST := Tests/AsyncLambdaSyntaxSemanticFoundationModel/semantic_test
AWAIT_USING_SEMANTIC_TEST := Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemanticsModel/semantic_test
NATIVE_CALLBACK_BYREF_DIR := Tests/NativeCallbackByRef/native
NATIVE_CALLBACK_BYREF_SOURCE := $(NATIVE_CALLBACK_BYREF_DIR)/fixture.c
NATIVE_CALLBACK_BYREF_OBJECT := $(NATIVE_CALLBACK_BYREF_DIR)/fixture.o
NATIVE_CALLBACK_BYREF_FIXTURE := $(NATIVE_CALLBACK_BYREF_DIR)/libvoidc098fixture.a
LANGUAGE_EDGE_INTEGRATION_NATIVE_DIR := Tests/LanguageEdgeProjectCompletionIntegration/Consumer/native
LANGUAGE_EDGE_INTEGRATION_NATIVE_SOURCE := $(LANGUAGE_EDGE_INTEGRATION_NATIVE_DIR)/fixture.c
LANGUAGE_EDGE_INTEGRATION_NATIVE_OBJECT := $(LANGUAGE_EDGE_INTEGRATION_NATIVE_DIR)/fixture.o
LANGUAGE_EDGE_INTEGRATION_NATIVE_FIXTURE := $(LANGUAGE_EDGE_INTEGRATION_NATIVE_DIR)/libvoid100fixture.a
UNSAFE_DELEGATE_NATIVE_DIR := Tests/UnsafeDelegates/native
UNSAFE_DELEGATE_NATIVE_SOURCE := $(UNSAFE_DELEGATE_NATIVE_DIR)/fixture.c
UNSAFE_DELEGATE_NATIVE_OBJECT := $(UNSAFE_DELEGATE_NATIVE_DIR)/fixture.o
UNSAFE_DELEGATE_NATIVE_FIXTURE := $(UNSAFE_DELEGATE_NATIVE_DIR)/libvoidc101fixture.a
NATIVE_BOUNDARY_EXCEPTION_DIR := Tests/NativeLibraryBoundaryExceptions/Runtime/native
NATIVE_BOUNDARY_EXCEPTION_SOURCE := $(NATIVE_BOUNDARY_EXCEPTION_DIR)/fixture.c
NATIVE_BOUNDARY_EXCEPTION_OBJECT := $(NATIVE_BOUNDARY_EXCEPTION_DIR)/fixture.o
NATIVE_BOUNDARY_EXCEPTION_FIXTURE := $(NATIVE_BOUNDARY_EXCEPTION_DIR)/libvoidc119fixture.a
QUERY_TEST := Tests/SemanticQueries/query-test
QUERY_TEST_SOURCE := Tests/SemanticQueries/query_test.c
QUERY_TEST_COMPILER_SOURCES := $(filter-out Compiler/src/main.c,$(SOURCES))
DEBUG_INFO_PROJECT := Tests/DebugInfo
DEBUG_INFO_BINARY := $(DEBUG_INFO_PROJECT)/bin/DebugInfo$(EXE_SUFFIX)
DEBUG_INFO_C := $(DEBUG_INFO_PROJECT)/.void/DebugInfo.c
NATIVE_THREAD_RUNTIME_TEST := Tests/NativeThreadRuntimeFoundation/native_thread_test
RUNTIME_CONTEXT_TEST := Tests/PerThreadRuntimeExecutionContext/runtime_context_test
THREAD_SAFE_ALLOCATION_REGISTRY_TEST := Tests/ThreadSafeAllocationRuntimeRegistry/CConsumer/thread-registry-test
COOPERATIVE_STW_GC_TEST := Tests/CooperativeStopTheWorldGc/CConsumer/stw-test
MANAGED_PINNING_RUNTIME_TEST := Tests/ManagedPinningRuntimeFoundation/CConsumer/pin-runtime-test
FIXED_STATEMENT_CLEANUP_TEST := Tests/FixedStatementScopedPointerCleanup/CConsumer/fixed-cleanup-test
PINNING_GC_THREADS_NATIVE_TEST := Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/CConsumer/pin-native-integration-test
UNMANAGED_MEMORY_NATIVE_BUFFER_AUDIT_TEST := Tests/UnmanagedMemoryNativeBufferIntegrationAudit/CConsumer/unmanaged-memory-native-buffer-audit-test
NATIVE_HEAP_RUNTIME_TEST := Tests/NativeHeapRuntimeFoundation/native_heap_test
NATIVE_MONITOR_TEST := Tests/MonitorMutualExclusionFoundation/native_monitor_test
THREAD_EXCEPTION_NATIVE_DIR := Tests/ThreadExceptionNativeBoundaryCleanup/native
THREAD_EXCEPTION_NATIVE_SOURCE := $(THREAD_EXCEPTION_NATIVE_DIR)/fixture.c
THREAD_EXCEPTION_NATIVE_OBJECT := $(THREAD_EXCEPTION_NATIVE_DIR)/fixture.o
THREAD_EXCEPTION_NATIVE_FIXTURE := $(THREAD_EXCEPTION_NATIVE_DIR)/libvoidc168fixture.a
MONITOR_CONDITION_WAIT_RUNTIME_TEST := Tests/MonitorConditionWaitRuntimeFoundation/native_monitor_condition_test
PORTABLE_ATOMIC_RUNTIME_TEST := Tests/PortableAtomicRuntimeFoundation/native_atomic_test
MONOTONIC_TIMED_WAIT_RUNTIME_TEST := Tests/MonotonicTimeTimedWaitRuntimeFoundation/native_timed_wait_test
SYNC_EXCEPTION_NATIVE_DIR := Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/native
SYNC_EXCEPTION_NATIVE_SOURCE := $(SYNC_EXCEPTION_NATIVE_DIR)/fixture.c
SYNC_EXCEPTION_NATIVE_OBJECT := $(SYNC_EXCEPTION_NATIVE_DIR)/fixture.o
SYNC_EXCEPTION_NATIVE_FIXTURE := $(SYNC_EXCEPTION_NATIVE_DIR)/libvoidc178fixture.a
TIMED_CANCELLABLE_NATIVE_DIR := Tests/TimedCancellableSynchronizationIntegration/native
TIMED_CANCELLABLE_NATIVE_SOURCE := $(TIMED_CANCELLABLE_NATIVE_DIR)/fixture.c
TIMED_CANCELLABLE_NATIVE_OBJECT := $(TIMED_CANCELLABLE_NATIVE_DIR)/fixture.o
TIMED_CANCELLABLE_NATIVE_FIXTURE := $(TIMED_CANCELLABLE_NATIVE_DIR)/libvoidc189fixture.a
TIMED_SYNC_AUDIT_NATIVE_DIR := Tests/TimedSynchronizationCancellationIntegrationAudit/native
TIMED_SYNC_AUDIT_NATIVE_SOURCE := $(TIMED_SYNC_AUDIT_NATIVE_DIR)/fixture.c
TIMED_SYNC_AUDIT_NATIVE_OBJECT := $(TIMED_SYNC_AUDIT_NATIVE_DIR)/fixture.o
TIMED_SYNC_AUDIT_NATIVE_FIXTURE := $(TIMED_SYNC_AUDIT_NATIVE_DIR)/libvoidc190fixture.a
TASK_RUN_NATIVE_DIR := Tests/TaskRunThreadPoolSchedulingIntegration/native
TASK_RUN_NATIVE_SOURCE := $(TASK_RUN_NATIVE_DIR)/fixture.c
TASK_RUN_NATIVE_OBJECT := $(TASK_RUN_NATIVE_DIR)/fixture.o
TASK_RUN_NATIVE_FIXTURE := $(TASK_RUN_NATIVE_DIR)/libvoidc196fixture.a
BORROWED_NUL_NATIVE_DIR := Tests/BorrowedNulNativeStringParameters/native
BORROWED_NUL_NATIVE_SOURCE := $(BORROWED_NUL_NATIVE_DIR)/fixture.c
BORROWED_NUL_NATIVE_OBJECT := $(BORROWED_NUL_NATIVE_DIR)/fixture.o
BORROWED_NUL_NATIVE_FIXTURE := $(BORROWED_NUL_NATIVE_DIR)/libvoidc294fixture.a
POINTER_LENGTH_NATIVE_DIR := Tests/PointerLengthNativeStringParameters/native
POINTER_LENGTH_NATIVE_SOURCE := $(POINTER_LENGTH_NATIVE_DIR)/fixture.c
POINTER_LENGTH_NATIVE_OBJECT := $(POINTER_LENGTH_NATIVE_DIR)/fixture.o
POINTER_LENGTH_NATIVE_FIXTURE := $(POINTER_LENGTH_NATIVE_DIR)/libvoidc295fixture.a
STRING_RETURN_NATIVE_DIR := Tests/BorrowedStaticNativeStringReturns/native
STRING_RETURN_NATIVE_SOURCE := $(STRING_RETURN_NATIVE_DIR)/fixture.c
STRING_RETURN_NATIVE_OBJECT := $(STRING_RETURN_NATIVE_DIR)/fixture.o
STRING_RETURN_NATIVE_FIXTURE := $(STRING_RETURN_NATIVE_DIR)/libvoidc296fixture.a
STRING_RETURN_INVALID_NATIVE_DIR := Tests/BorrowedStaticNativeStringReturnsDiagnostics/InvalidPointerLength/native
STRING_RETURN_INVALID_NATIVE_SOURCE := $(STRING_RETURN_INVALID_NATIVE_DIR)/fixture.c
STRING_RETURN_INVALID_NATIVE_OBJECT := $(STRING_RETURN_INVALID_NATIVE_DIR)/fixture.o
STRING_RETURN_INVALID_NATIVE_FIXTURE := $(STRING_RETURN_INVALID_NATIVE_DIR)/libvoidc296invalidfixture.a
OWNED_STRING_RETURN_NATIVE_DIR := Tests/OwnedNativeStringReturns/native
OWNED_STRING_RETURN_NATIVE_SOURCE := $(OWNED_STRING_RETURN_NATIVE_DIR)/fixture.c
OWNED_STRING_RETURN_NATIVE_OBJECT := $(OWNED_STRING_RETURN_NATIVE_DIR)/fixture.o
OWNED_STRING_RETURN_NATIVE_FIXTURE := $(OWNED_STRING_RETURN_NATIVE_DIR)/libvoidc297fixture.a
NATIVE_STRING_BOUNDARY_NATIVE_DIR := Tests/NativeStringLifetimeCallbackBoundaryIntegration/native
NATIVE_STRING_BOUNDARY_NATIVE_SOURCE := $(NATIVE_STRING_BOUNDARY_NATIVE_DIR)/fixture.c
NATIVE_STRING_BOUNDARY_NATIVE_OBJECT := $(NATIVE_STRING_BOUNDARY_NATIVE_DIR)/fixture.o
NATIVE_STRING_BOUNDARY_NATIVE_FIXTURE := $(NATIVE_STRING_BOUNDARY_NATIVE_DIR)/libvoidc299fixture.a
MANAGED_STRING_AUDIT_NATIVE_DIR := Tests/ManagedStringsNativeStringAbiIntegrationAudit/native
MANAGED_STRING_AUDIT_NATIVE_SOURCE := $(MANAGED_STRING_AUDIT_NATIVE_DIR)/fixture.c
MANAGED_STRING_AUDIT_NATIVE_OBJECT := $(MANAGED_STRING_AUDIT_NATIVE_DIR)/fixture.o
MANAGED_STRING_AUDIT_NATIVE_FIXTURE := $(MANAGED_STRING_AUDIT_NATIVE_DIR)/libvoidc300fixture.a
MANAGED_STRING_AUDIT_C_CONSUMER := Tests/ManagedStringsNativeStringAbiIntegrationAudit/CConsumer/consumer
UTF8_VALIDITY_NATIVE_DIR := Tests/Utf8ValidityUnicodeScalarFoundation/native
UTF8_VALIDITY_NATIVE_SOURCE := $(UTF8_VALIDITY_NATIVE_DIR)/fixture.c
UTF8_VALIDITY_NATIVE_OBJECT := $(UTF8_VALIDITY_NATIVE_DIR)/fixture.o
UTF8_VALIDITY_NATIVE_FIXTURE := $(UTF8_VALIDITY_NATIVE_DIR)/libvoidc301fixture.a
UTF8_VALIDITY_DIAGNOSTIC_DIRS := Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidNativeNul Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidOverlongView Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidSurrogateView Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidCallbackView Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidOwnedReturn
UTF8_VALIDITY_DIAGNOSTIC_FIXTURES := $(addsuffix /native/libvoidc301fixture.a,$(UTF8_VALIDITY_DIAGNOSTIC_DIRS))
UTF8_VALIDITY_EXPORT_CONSUMER := Tests/Utf8ValidityUnicodeScalarFoundation/CConsumer/invalid_utf8_consumer
ASYNC_OBJECT_MODEL_NATIVE_DIR := Tests/AsyncObjectModelLibraryBoundaryIntegration/Runtime/native
ASYNC_OBJECT_MODEL_NATIVE_SOURCE := $(ASYNC_OBJECT_MODEL_NATIVE_DIR)/fixture.c
ASYNC_OBJECT_MODEL_NATIVE_OBJECT := $(ASYNC_OBJECT_MODEL_NATIVE_DIR)/fixture.o
ASYNC_OBJECT_MODEL_NATIVE_FIXTURE := $(ASYNC_OBJECT_MODEL_NATIVE_DIR)/libvoidc209fixture.a

.PHONY: all clean test test-diagnostics test-check test-semantic-queries test-debug-info test-lsp test-lsp-diagnostics test-lsp-hover-signature test-lsp-completion test-lsp-navigation test-tooling-integration test-base-constructors test-constructor-delegation test-interface-inheritance test-struct-interfaces test-virtual-properties test-static-events test-interface-events test-delegate-completion test-conditional-expressions test-object-model-integration test-nested-value-structs test-readonly-structs test-jagged-arrays test-multidimensional-arrays test-closure-completion-i test-closure-completion-ii test-iterator-completion test-unsafe-member-completion test-compound-targets test-value-types-closures-control-flow-integration test-static-interface-properties test-static-interface-events test-constructor-returns test-rectangular-array-initializers test-iterator-locals test-receiver-expressions test-generic-method-calls test-native-callback-by-ref test-library-output test-language-edge-project-completion-integration test-unsafe-delegates test-unsafe-pointer-properties test-do-while test-expression-bodied-members test-custom-event-accessors test-null-conditional-arrays test-sized-rectangular-array-initializers test-implicit-rectangular-array-initializers test-generic-constraints test-language-surface-completion-ii-integration test-exception-throw-foundation test-try-catch-completion test-exception-propagation-gc-unwinding test-finally-completion test-structured-control-finally test-nested-handlers-rethrow test-iterator-exception-integration test-delegate-event-exception-integration test-native-library-boundary-exception-integration test-exceptions-runtime-control-flow-integration test-idisposable-iterator-disposal-foundation test-foreach-disposal-completion test-using-statement-completion test-using-declaration-completion test-generic-method-type-inference test-extension-method-completion test-static-interface-method-contracts test-constrained-static-interface-dispatch test-default-interface-methods test-deterministic-lifetime-advanced-abstractions-integration test-declaration-patterns test-constant-null-patterns test-relational-patterns test-logical-patterns test-property-patterns test-recursive-patterns test-pattern-switch-statements test-switch-guards-pattern-ordering test-switch-expressions test-pattern-control-flow-integration test-iterator-pattern-local-capture test-iterator-switch-guard-local-capture test-iterator-captured-lifetime-gc-integration test-iterator-state-machine-integration-audit test-diagnostic-contract-cleanup test-unary-operator-completion test-binary-operator-completion test-static-interface-operator-contracts test-constrained-generic-operator-dispatch test-iterator-lifetime-generic-operator-integration test-standard-conversion-classification-completion test-user-defined-conversion-resolution-completion test-lifted-nullable-conversion-completion test-conversion-site-integration test-static-interface-conversion-contracts test-constrained-generic-implicit-conversion-dispatch test-constrained-generic-explicit-conversion-dispatch test-generic-conversion-provenance-inference-integration test-iterator-closure-gc-conversion-integration test-conversion-semantics-generic-conversion-integration test-native-thread-runtime-foundation test-per-thread-runtime-execution-context test-thread-safe-allocation-runtime-registry test-cooperative-stop-the-world-gc-safepoints test-managed-thread-surface test-monitor-mutual-exclusion-runtime-foundation test-lock-statement-completion test-thread-exception-native-boundary-cleanup-integration test-threaded-closure-delegate-generic-gc-integration test-native-concurrency-synchronization-integration-audit test-monitor-condition-wait-runtime-foundation test-managed-monitor-wait-completion test-monitor-pulse-completion test-portable-atomic-runtime-foundation test-interlocked-integer-operations test-interlocked-managed-reference-operations-gc-safety test-volatile-memory-ordering test-synchronization-exception-native-boundary-cleanup-integration test-threaded-generic-closure-delegate-synchronization-integration test-advanced-synchronization-atomic-integration-audit test-monotonic-time-timed-wait-runtime-foundation test-managed-thread-sleep-timeout-contract test-timed-monitor-wait-completion test-manual-reset-event-completion test-auto-reset-event-completion test-semaphore-completion test-cancellation-token-source-foundation test-cancellation-registration-wakeup-completion test-timed-cancellable-synchronization-integration test-timed-synchronization-cancellation-integration-audit test-timed-synchronization-cancellation-race-stress

all: $(BIN) $(NATIVE_FIXTURE) $(NATIVE_BUFFER_CALL_FIXTURE) $(NATIVE_FUNCTION_ADDRESS_FIXTURE) $(NATIVE_FUNCTION_ABI_FIXTURE) $(NATIVE_CALLBACK_BYREF_FIXTURE) $(LANGUAGE_EDGE_INTEGRATION_NATIVE_FIXTURE) $(UNSAFE_DELEGATE_NATIVE_FIXTURE) $(NATIVE_BOUNDARY_EXCEPTION_FIXTURE) $(THREAD_EXCEPTION_NATIVE_FIXTURE) $(SYNC_EXCEPTION_NATIVE_FIXTURE) $(TIMED_CANCELLABLE_NATIVE_FIXTURE) $(TIMED_SYNC_AUDIT_NATIVE_FIXTURE) $(TASK_RUN_NATIVE_FIXTURE) $(BORROWED_NUL_NATIVE_FIXTURE) $(POINTER_LENGTH_NATIVE_FIXTURE) $(STRING_RETURN_NATIVE_FIXTURE) $(STRING_RETURN_INVALID_NATIVE_FIXTURE) $(OWNED_STRING_RETURN_NATIVE_FIXTURE) $(NATIVE_STRING_BOUNDARY_NATIVE_FIXTURE) $(MANAGED_STRING_AUDIT_NATIVE_FIXTURE) $(UTF8_VALIDITY_NATIVE_FIXTURE)

$(BIN): $(SOURCES) $(HEADERS) $(RUNTIME_HEADERS) $(RUNTIME_SOURCES)
	@mkdir -p bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $(BIN)

$(NATIVE_FIXTURE_OBJECT): $(NATIVE_FIXTURE_SOURCE)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -c $< -o $@

$(NATIVE_FIXTURE): $(NATIVE_FIXTURE_OBJECT)
	$(AR) rcs $@ $<

$(NATIVE_BUFFER_CALL_OBJECT): $(NATIVE_BUFFER_CALL_SOURCE)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -c $< -o $@

$(NATIVE_BUFFER_CALL_FIXTURE): $(NATIVE_BUFFER_CALL_OBJECT)
	$(AR) rcs $@ $<

$(NATIVE_FUNCTION_ADDRESS_OBJECT): $(NATIVE_FUNCTION_ADDRESS_SOURCE)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -c $< -o $@

$(NATIVE_FUNCTION_ADDRESS_FIXTURE): $(NATIVE_FUNCTION_ADDRESS_OBJECT)
	$(AR) rcs $@ $<

$(NATIVE_FUNCTION_ABI_OBJECT): $(NATIVE_FUNCTION_ABI_SOURCE)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -c $< -o $@

$(NATIVE_FUNCTION_ABI_FIXTURE): $(NATIVE_FUNCTION_ABI_OBJECT)
	$(AR) rcs $@ $<

$(NATIVE_FUNCTION_GENERIC_OBJECT): $(NATIVE_FUNCTION_GENERIC_SOURCE)
	$(CC) $(CFLAGS) -c $< -o $@

$(NATIVE_FUNCTION_GENERIC_FIXTURE): $(NATIVE_FUNCTION_GENERIC_OBJECT)
	$(AR) rcs $@ $<

$(NATIVE_HEAP_FUNCTION_POINTER_AUDIT_OBJECT): $(NATIVE_HEAP_FUNCTION_POINTER_AUDIT_SOURCE)
	$(CC) $(CFLAGS) -c $< -o $@

$(NATIVE_HEAP_FUNCTION_POINTER_AUDIT_FIXTURE): $(NATIVE_HEAP_FUNCTION_POINTER_AUDIT_OBJECT)
	$(AR) rcs $@ $<

$(NATIVE_CALLBACK_BYREF_OBJECT): $(NATIVE_CALLBACK_BYREF_SOURCE)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -c $< -o $@

$(NATIVE_CALLBACK_BYREF_FIXTURE): $(NATIVE_CALLBACK_BYREF_OBJECT)
	$(AR) rcs $@ $<

$(LANGUAGE_EDGE_INTEGRATION_NATIVE_OBJECT): $(LANGUAGE_EDGE_INTEGRATION_NATIVE_SOURCE)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -c $< -o $@

$(LANGUAGE_EDGE_INTEGRATION_NATIVE_FIXTURE): $(LANGUAGE_EDGE_INTEGRATION_NATIVE_OBJECT)
	$(AR) rcs $@ $<

$(UNSAFE_DELEGATE_NATIVE_OBJECT): $(UNSAFE_DELEGATE_NATIVE_SOURCE)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -c $< -o $@

$(UNSAFE_DELEGATE_NATIVE_FIXTURE): $(UNSAFE_DELEGATE_NATIVE_OBJECT)
	$(AR) rcs $@ $<

$(NATIVE_BOUNDARY_EXCEPTION_OBJECT): $(NATIVE_BOUNDARY_EXCEPTION_SOURCE)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -c $< -o $@

$(NATIVE_BOUNDARY_EXCEPTION_FIXTURE): $(NATIVE_BOUNDARY_EXCEPTION_OBJECT)
	$(AR) rcs $@ $<

$(THREAD_EXCEPTION_NATIVE_OBJECT): $(THREAD_EXCEPTION_NATIVE_SOURCE)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -c $< -o $@

$(THREAD_EXCEPTION_NATIVE_FIXTURE): $(THREAD_EXCEPTION_NATIVE_OBJECT)
	$(AR) rcs $@ $<

$(SYNC_EXCEPTION_NATIVE_OBJECT): $(SYNC_EXCEPTION_NATIVE_SOURCE)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -c $< -o $@

$(SYNC_EXCEPTION_NATIVE_FIXTURE): $(SYNC_EXCEPTION_NATIVE_OBJECT)
	$(AR) rcs $@ $<

$(TIMED_CANCELLABLE_NATIVE_OBJECT): $(TIMED_CANCELLABLE_NATIVE_SOURCE)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -c $< -o $@

$(TIMED_CANCELLABLE_NATIVE_FIXTURE): $(TIMED_CANCELLABLE_NATIVE_OBJECT)
	$(AR) rcs $@ $<

$(TIMED_SYNC_AUDIT_NATIVE_OBJECT): $(TIMED_SYNC_AUDIT_NATIVE_SOURCE)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -c $< -o $@

$(TIMED_SYNC_AUDIT_NATIVE_FIXTURE): $(TIMED_SYNC_AUDIT_NATIVE_OBJECT)
	$(AR) rcs $@ $<

$(TASK_RUN_NATIVE_OBJECT): $(TASK_RUN_NATIVE_SOURCE)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -c $< -o $@

$(TASK_RUN_NATIVE_FIXTURE): $(TASK_RUN_NATIVE_OBJECT)
	$(AR) rcs $@ $<

$(BORROWED_NUL_NATIVE_OBJECT): $(BORROWED_NUL_NATIVE_SOURCE)
	$(CC) $(CFLAGS) -c $< -o $@

$(BORROWED_NUL_NATIVE_FIXTURE): $(BORROWED_NUL_NATIVE_OBJECT)
	$(AR) rcs $@ $<

$(POINTER_LENGTH_NATIVE_OBJECT): $(POINTER_LENGTH_NATIVE_SOURCE)
	$(CC) $(CFLAGS) -c $< -o $@

$(POINTER_LENGTH_NATIVE_FIXTURE): $(POINTER_LENGTH_NATIVE_OBJECT)
	$(AR) rcs $@ $<

$(STRING_RETURN_NATIVE_OBJECT): $(STRING_RETURN_NATIVE_SOURCE)
	$(CC) $(CFLAGS) -c $< -o $@

$(STRING_RETURN_NATIVE_FIXTURE): $(STRING_RETURN_NATIVE_OBJECT)
	$(AR) rcs $@ $<

$(STRING_RETURN_INVALID_NATIVE_OBJECT): $(STRING_RETURN_INVALID_NATIVE_SOURCE)
	$(CC) $(CFLAGS) -c $< -o $@

$(STRING_RETURN_INVALID_NATIVE_FIXTURE): $(STRING_RETURN_INVALID_NATIVE_OBJECT)
	$(AR) rcs $@ $<

$(OWNED_STRING_RETURN_NATIVE_OBJECT): $(OWNED_STRING_RETURN_NATIVE_SOURCE)
	$(CC) $(CFLAGS) -c $< -o $@

$(OWNED_STRING_RETURN_NATIVE_FIXTURE): $(OWNED_STRING_RETURN_NATIVE_OBJECT)
	$(AR) rcs $@ $<

$(NATIVE_STRING_BOUNDARY_NATIVE_OBJECT): $(NATIVE_STRING_BOUNDARY_NATIVE_SOURCE)
	$(CC) $(CFLAGS) -c $< -o $@

$(NATIVE_STRING_BOUNDARY_NATIVE_FIXTURE): $(NATIVE_STRING_BOUNDARY_NATIVE_OBJECT)
	$(AR) rcs $@ $<

$(MANAGED_STRING_AUDIT_NATIVE_OBJECT): $(MANAGED_STRING_AUDIT_NATIVE_SOURCE)
	$(CC) $(CFLAGS) -c $< -o $@

$(MANAGED_STRING_AUDIT_NATIVE_FIXTURE): $(MANAGED_STRING_AUDIT_NATIVE_OBJECT)
	$(AR) rcs $@ $<

$(UTF8_VALIDITY_NATIVE_OBJECT): $(UTF8_VALIDITY_NATIVE_SOURCE)
	$(CC) $(CFLAGS) -c $< -o $@

$(UTF8_VALIDITY_NATIVE_FIXTURE): $(UTF8_VALIDITY_NATIVE_OBJECT)
	$(AR) rcs $@ $<

$(UTF8_VALIDITY_DIAGNOSTIC_FIXTURES): $(UTF8_VALIDITY_NATIVE_FIXTURE)
	@mkdir -p $(dir $@)
	cp $< $@

$(ASYNC_OBJECT_MODEL_NATIVE_OBJECT): $(ASYNC_OBJECT_MODEL_NATIVE_SOURCE)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -c $< -o $@

$(ASYNC_OBJECT_MODEL_NATIVE_FIXTURE): $(ASYNC_OBJECT_MODEL_NATIVE_OBJECT)
	$(AR) rcs $@ $<

.PHONY: test-synchronous-foreach-suspension-cleanup-completion
.PHONY: test-awaited-switch-expression-pattern-guard-completion
.PHONY: test-async-enumeration-protocol-foundation
.PHONY: test-await-foreach-syntax-lowering-completion
.PHONY: test-async-iterator-method-foundation
.PHONY: test-async-iterator-composition-completion
.PHONY: test-async-iterator-cancellation-integration
.PHONY: test-async-language-asynchronous-iteration-integration-audit
.PHONY: test-by-reference-value-ref-local-foundation
.PHONY: test-ref-assignment-aliasing-completion
.PHONY: test-ref-return-method-completion
.PHONY: test-ref-returning-properties-indexers-completion
.PHONY: test-ref-readonly-scoped-escape-completion
.PHONY: test-ref-struct-foundation
.PHONY: test-ref-struct-escape-completion
.PHONY: test-span-foundation
.PHONY: test-span-conversion-slicing-stackalloc-completion
.PHONY: test-by-reference-span-integration-audit
.PHONY: test-managed-threadpool-work-queue-foundation test-threadpool-worker-lifecycle-gc-coordination test-threadpool-exception-shutdown-completion test-threadpool-exception-shutdown-stress test-task-state-completion-foundation test-generic-task-result-gc-completion test-task-run-threadpool-scheduling-integration test-task-waiting-timeout-fault-propagation test-task-cancellation-integration test-task-continuation-completion-source-foundation test-threadpool-task-integration-audit test-threadpool-task-integration-audit-stress test-async-await-syntax-semantic-foundation test-async-task-state-machine-lowering-foundation test-await-suspension-resumption-completion test-async-exception-catch-finally-integration test-async-cancellation-integration test-async-generics-closures-gc-lifetime-integration test-async-object-model-library-boundary-integration test-async-await-task-runtime-integration-audit test-async-await-task-runtime-integration-audit-stress

clean:
	rm -rf Tests/CorrectnessNativeBackendIntegrationAudit/.void Tests/CorrectnessNativeBackendIntegrationAudit/bin Tests/CorrectnessNativeBackendIntegrationAudit/publish
	rm -rf Tests/CorrectnessBlockStabilization/.void Tests/CorrectnessBlockStabilization/bin Tests/CorrectnessBlockStabilization/publish
	rm -rf Tests/CorrectnessCrossFeatureIntegration/.void Tests/CorrectnessCrossFeatureIntegration/bin Tests/CorrectnessCrossFeatureIntegration/publish
	rm -rf Tests/LiteralHostCBoundaryConsistency/.void Tests/LiteralHostCBoundaryConsistency/bin Tests/LiteralHostCBoundaryConsistency/publish
	rm -rf Tests/SignedArithmeticConversionBoundaryCompletion/.void Tests/SignedArithmeticConversionBoundaryCompletion/bin Tests/SignedArithmeticConversionBoundaryCompletion/publish
	rm -rf Tests/SignedArithmeticConversionBoundaryDiagnostics/*/.void Tests/SignedArithmeticConversionBoundaryDiagnostics/*/bin Tests/SignedArithmeticConversionBoundaryDiagnostics/*/publish
	rm -rf Tests/DefinedSignedIntegerArithmeticFoundation/.void Tests/DefinedSignedIntegerArithmeticFoundation/bin Tests/DefinedSignedIntegerArithmeticFoundation/publish
	rm -rf Tests/UsingAliasSemanticBinding/.void Tests/UsingAliasSemanticBinding/bin Tests/UsingAliasSemanticBinding/publish
	rm -rf Tests/OrdinaryLocalDefiniteAssignment/.void Tests/OrdinaryLocalDefiniteAssignment/bin Tests/OrdinaryLocalDefiniteAssignment/publish
	rm -rf Tests/ImplicitInstanceReceiverBinding/.void Tests/ImplicitInstanceReceiverBinding/bin Tests/ImplicitInstanceReceiverBinding/publish
	rm -rf Tests/ImplicitInstanceReceiverBindingStrict/.void Tests/ImplicitInstanceReceiverBindingStrict/bin Tests/ImplicitInstanceReceiverBindingStrict/publish
	rm -rf Tests/ImplicitInstanceReceiverBindingDiagnostics/*/.void Tests/ImplicitInstanceReceiverBindingDiagnostics/*/bin Tests/ImplicitInstanceReceiverBindingDiagnostics/*/publish
	rm -rf Tests/OutVariableIntegrationAudit/.void Tests/OutVariableIntegrationAudit/bin Tests/OutVariableIntegrationAudit/publish
	rm -rf Tests/RuntimeDiagnosticsApplicationAudit/.void Tests/RuntimeDiagnosticsApplicationAudit/bin Tests/RuntimeDiagnosticsApplicationAudit/publish
	rm -rf Tests/RuntimeDiagnosticsThreadSourceAudit/.void Tests/RuntimeDiagnosticsThreadSourceAudit/bin Tests/RuntimeDiagnosticsThreadSourceAudit/publish
	rm -rf void-runtime-diagnostics-integration-349-* void-source-fallback-348-* void-runtime-presentation-347-* void-compiler-host-* "void host 雪 "*
	rm -rf Tests/RuntimeDiagnosticsCrossFeatureIntegration/.void Tests/RuntimeDiagnosticsCrossFeatureIntegration/bin Tests/RuntimeDiagnosticsCrossFeatureIntegration/publish
	rm -rf Tests/SourceAvailabilityPublishNativeBoundaryFallbacks/.void Tests/SourceAvailabilityPublishNativeBoundaryFallbacks/bin Tests/SourceAvailabilityPublishNativeBoundaryFallbacks/publish
	rm -rf Tests/UnifiedRuntimeDiagnosticPresentation/.void Tests/UnifiedRuntimeDiagnosticPresentation/bin Tests/UnifiedRuntimeDiagnosticPresentation/publish
	rm -rf Tests/ExceptionRethrowRuntimeFailureStackPreservation/.void Tests/ExceptionRethrowRuntimeFailureStackPreservation/bin Tests/ExceptionRethrowRuntimeFailureStackPreservation/publish
	rm -rf Tests/SynchronousVoidCallStackFoundation/.void Tests/SynchronousVoidCallStackFoundation/bin Tests/SynchronousVoidCallStackFoundation/publish
	rm -rf Tests/RuntimeSourceExcerptDiagnosticFoundation/.void Tests/RuntimeSourceExcerptDiagnosticFoundation/bin Tests/RuntimeSourceExcerptDiagnosticFoundation/publish
	rm -rf Tests/OutVariableScopeFlowDiscardCompletion/.void Tests/OutVariableScopeFlowDiscardCompletion/bin Tests/OutVariableScopeFlowDiscardCompletion/publish
	rm -rf Tests/OutVarTypeInferenceOverloadIntegration/.void Tests/OutVarTypeInferenceOverloadIntegration/bin Tests/OutVarTypeInferenceOverloadIntegration/publish
	rm -rf Tests/InlineTypedOutVariableDeclarations/.void Tests/InlineTypedOutVariableDeclarations/bin Tests/InlineTypedOutVariableDeclarations/publish
	rm -rf Tests/StaticApiIntegration/.void Tests/StaticApiIntegration/bin Tests/StaticApiIntegration/publish
	rm -rf Tests/BuiltInStaticValues/.void Tests/BuiltInStaticValues/bin Tests/BuiltInStaticValues/publish
	rm -rf Tests/BuiltInIntegralBoolCharParsing/.void Tests/BuiltInIntegralBoolCharParsing/bin Tests/BuiltInIntegralBoolCharParsing/publish
	rm -rf Tests/BuiltInFloatingDecimalParsing/.void Tests/BuiltInFloatingDecimalParsing/bin Tests/BuiltInFloatingDecimalParsing/publish
	rm -rf Tests/ConstructedGenericStaticMembers/.void Tests/ConstructedGenericStaticMembers/bin Tests/ConstructedGenericStaticMembers/publish
	rm -rf Tests/BuiltInAssociatedMemberFoundation/.void Tests/BuiltInAssociatedMemberFoundation/bin Tests/BuiltInAssociatedMemberFoundation/publish
	rm -rf Tests/StaticCallableTypeReceivers/.void Tests/StaticCallableTypeReceivers/bin Tests/StaticCallableTypeReceivers/publish
	rm -rf Tests/StaticValueTypeReceivers/.void Tests/StaticValueTypeReceivers/bin Tests/StaticValueTypeReceivers/publish
	rm -rf Tests/TypeQualifiedMemberAccessFoundation/.void Tests/TypeQualifiedMemberAccessFoundation/bin Tests/TypeQualifiedMemberAccessFoundation/publish
	rm -rf Tests/EngineTextStreams/.void Tests/EngineTextStreams/bin Tests/EngineNativeFailure/.void Tests/EngineNativeFailure/bin Tests/EngineFilesystemDiagnostics/.void Tests/EngineFilesystemDiagnostics/bin
	rm -rf Tests/InterfaceIndexerDispatch/.void Tests/InterfaceIndexerDispatch/bin Tests/InterfaceIndexerDispatch/publish Tests/InterfaceIndexerDiagnostics/.void Tests/InterfaceIndexerDiagnostics/bin Tests/InterfaceIndexerDiagnostics/publish
	rm -rf Tests/EngineMemoryStreamUnusedVirtual/.void Tests/EngineMemoryStreamUnusedVirtual/bin Tests/EngineMemoryStreamUnusedVirtual/publish
	rm -rf Tests/Utf8ValidityUnicodeScalarFoundation/.void Tests/Utf8ValidityUnicodeScalarFoundation/bin Tests/Utf8ValidityUnicodeScalarFoundation/publish
	rm -rf Tests/Utf8ValidityUnicodeScalarFoundation/Library/.void Tests/Utf8ValidityUnicodeScalarFoundation/Library/bin Tests/Utf8ValidityUnicodeScalarFoundation/Library/publish
	rm -rf Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/*/.void Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/*/bin Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/*/publish
	rm -rf Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/*/native
	rm -f Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidOwnedReturn/release.marker
	rm -rf Tests/StringCoreProperties/.void Tests/StringCoreProperties/bin Tests/StringCoreProperties/publish
	rm -rf Tests/StringCorePropertiesDiagnostics/*/.void Tests/StringCorePropertiesDiagnostics/*/bin Tests/StringCorePropertiesDiagnostics/*/publish
	rm -rf Tests/StringCharacterIndexingEnumeration/.void Tests/StringCharacterIndexingEnumeration/bin Tests/StringCharacterIndexingEnumeration/publish
	rm -rf Tests/StringCharacterIndexingEnumerationDiagnostics/*/.void Tests/StringCharacterIndexingEnumerationDiagnostics/*/bin Tests/StringCharacterIndexingEnumerationDiagnostics/*/publish
	rm -rf Tests/StringRangeSlicingSubstring/.void Tests/StringRangeSlicingSubstring/bin Tests/StringRangeSlicingSubstring/publish
	rm -rf Tests/StringRangeSlicingSubstringDiagnostics/*/.void Tests/StringRangeSlicingSubstringDiagnostics/*/bin Tests/StringRangeSlicingSubstringDiagnostics/*/publish
	rm -rf Tests/StringConcatenationComposition/.void Tests/StringConcatenationComposition/bin Tests/StringConcatenationComposition/publish
	rm -rf Tests/StringConcatenationCompositionDiagnostics/*/.void Tests/StringConcatenationCompositionDiagnostics/*/bin Tests/StringConcatenationCompositionDiagnostics/*/publish
	rm -rf Tests/OrdinalStringSearchComparison/.void Tests/OrdinalStringSearchComparison/bin Tests/OrdinalStringSearchComparison/publish
	rm -rf Tests/OrdinalStringSearchComparisonDiagnostics/*/.void Tests/OrdinalStringSearchComparisonDiagnostics/*/bin Tests/OrdinalStringSearchComparisonDiagnostics/*/publish
	rm -rf Tests/ImmutableStringEditing/.void Tests/ImmutableStringEditing/bin Tests/ImmutableStringEditing/publish
	rm -rf Tests/ImmutableStringEditingDiagnostics/*/.void Tests/ImmutableStringEditingDiagnostics/*/bin Tests/ImmutableStringEditingDiagnostics/*/publish
	rm -rf Tests/StringBuilderFoundation/.void Tests/StringBuilderFoundation/bin Tests/StringBuilderFoundation/publish
	rm -rf Tests/StringBuilderFoundationDiagnostics/*/.void Tests/StringBuilderFoundationDiagnostics/*/bin Tests/StringBuilderFoundationDiagnostics/*/publish
	rm -rf Tests/InterpolatedStringsPrimitiveFormatting/.void Tests/InterpolatedStringsPrimitiveFormatting/bin Tests/InterpolatedStringsPrimitiveFormatting/publish
	rm -rf Tests/InterpolatedStringsPrimitiveFormattingDiagnostics/*/.void Tests/InterpolatedStringsPrimitiveFormattingDiagnostics/*/bin Tests/InterpolatedStringsPrimitiveFormattingDiagnostics/*/publish
	rm -rf Tests/CoreStringsUtf8TextConstructionIntegrationAudit/.void Tests/CoreStringsUtf8TextConstructionIntegrationAudit/bin Tests/CoreStringsUtf8TextConstructionIntegrationAudit/publish
	rm -rf Tests/CoreStringsUtf8TextConstructionIntegrationAuditDiagnostics/*/.void Tests/CoreStringsUtf8TextConstructionIntegrationAuditDiagnostics/*/bin Tests/CoreStringsUtf8TextConstructionIntegrationAuditDiagnostics/*/publish
	rm -f $(UTF8_VALIDITY_EXPORT_CONSUMER)
	rm -f $(UTF8_VALIDITY_NATIVE_OBJECT) $(UTF8_VALIDITY_NATIVE_FIXTURE)
	rm -rf bin
	rm -rf Tests/AsyncTimingCancellationIntegrationAudit/.void Tests/AsyncTimingCancellationIntegrationAudit/bin Tests/AsyncTimingCancellationIntegrationAudit/publish
	rm -rf Tests/AsyncTimingCancellationIntegrationAudit/Library/.void Tests/AsyncTimingCancellationIntegrationAudit/Library/bin Tests/AsyncTimingCancellationIntegrationAudit/Library/publish
	rm -rf Tests/AsyncTimingCancellationIntegrationAuditDiagnostics/*/.void Tests/AsyncTimingCancellationIntegrationAuditDiagnostics/*/bin Tests/AsyncTimingCancellationIntegrationAuditDiagnostics/*/publish
	rm -f Tests/AsyncTimingCancellationIntegrationAudit/CConsumer/async-timing-cancellation-audit-consumer
	rm -rf Tests/AsyncTimingCancellationGcThreadIntegration/.void Tests/AsyncTimingCancellationGcThreadIntegration/bin Tests/AsyncTimingCancellationGcThreadIntegration/publish
	rm -rf Tests/AsyncTimingCancellationGcThreadIntegration/Library/.void Tests/AsyncTimingCancellationGcThreadIntegration/Library/bin Tests/AsyncTimingCancellationGcThreadIntegration/Library/publish
	rm -rf Tests/AsyncTimingCancellationGcThreadIntegrationDiagnostics/*/.void Tests/AsyncTimingCancellationGcThreadIntegrationDiagnostics/*/bin Tests/AsyncTimingCancellationGcThreadIntegrationDiagnostics/*/publish
	rm -f Tests/AsyncTimingCancellationGcThreadIntegration/CConsumer/async-timing-cancellation-integration-consumer
	rm -f Tests/AsyncTimingCancellationGcThreadIntegration/CConsumer/consumer
	rm -rf Tests/SharedMonotonicTimerQueueFoundation/.void Tests/SharedMonotonicTimerQueueFoundation/bin Tests/SharedMonotonicTimerQueueFoundation/publish
	rm -rf Tests/SharedMonotonicTimerQueueFoundationDiagnostics/*/.void Tests/SharedMonotonicTimerQueueFoundationDiagnostics/*/bin Tests/SharedMonotonicTimerQueueFoundationDiagnostics/*/publish
	rm -rf Tests/TaskDelayFoundation/.void Tests/TaskDelayFoundation/bin Tests/TaskDelayFoundation/publish
	rm -rf Tests/TaskDelayFoundationDiagnostics/*/.void Tests/TaskDelayFoundationDiagnostics/*/bin Tests/TaskDelayFoundationDiagnostics/*/publish
	rm -rf Tests/TaskDelayCancellationRaceCompletion/.void Tests/TaskDelayCancellationRaceCompletion/bin Tests/TaskDelayCancellationRaceCompletion/publish
	rm -rf Tests/TaskDelayCancellationRaceCompletionDiagnostics/*/.void Tests/TaskDelayCancellationRaceCompletionDiagnostics/*/bin Tests/TaskDelayCancellationRaceCompletionDiagnostics/*/publish
	rm -rf Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletion/.void Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletion/bin Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletion/publish
	rm -rf Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletionDiagnostics/*/.void Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletionDiagnostics/*/bin Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletionDiagnostics/*/publish
	rm -rf Tests/CancellationTokenSourceCancelAfterCompletion/.void Tests/CancellationTokenSourceCancelAfterCompletion/bin Tests/CancellationTokenSourceCancelAfterCompletion/publish
	rm -rf Tests/CancellationTokenSourceCancelAfterCompletionDiagnostics/*/.void Tests/CancellationTokenSourceCancelAfterCompletionDiagnostics/*/bin Tests/CancellationTokenSourceCancelAfterCompletionDiagnostics/*/publish
	rm -rf Tests/LinkedCancellationTokenSourceCompletion/.void Tests/LinkedCancellationTokenSourceCompletion/bin Tests/LinkedCancellationTokenSourceCompletion/publish
	rm -rf Tests/LinkedCancellationTokenSourceCompletionDiagnostics/*/.void Tests/LinkedCancellationTokenSourceCompletionDiagnostics/*/bin Tests/LinkedCancellationTokenSourceCompletionDiagnostics/*/publish
	rm -rf Tests/TaskWaitAsyncCancellationCompletion/.void Tests/TaskWaitAsyncCancellationCompletion/bin Tests/TaskWaitAsyncCancellationCompletion/publish
	rm -rf Tests/TaskWaitAsyncCancellationCompletionDiagnostics/*/.void Tests/TaskWaitAsyncCancellationCompletionDiagnostics/*/bin Tests/TaskWaitAsyncCancellationCompletionDiagnostics/*/publish
	rm -rf Tests/TaskWaitAsyncTimeoutCombinedCancellationCompletion/.void Tests/TaskWaitAsyncTimeoutCombinedCancellationCompletion/bin Tests/TaskWaitAsyncTimeoutCombinedCancellationCompletion/publish
	rm -rf Tests/TaskWaitAsyncTimeoutCombinedCancellationCompletionDiagnostics/*/.void Tests/TaskWaitAsyncTimeoutCombinedCancellationCompletionDiagnostics/*/bin Tests/TaskWaitAsyncTimeoutCombinedCancellationCompletionDiagnostics/*/publish
	rm -rf Tests/GeneralizedAwaitablesValueTaskIntegrationAudit/.void Tests/GeneralizedAwaitablesValueTaskIntegrationAudit/bin Tests/GeneralizedAwaitablesValueTaskIntegrationAudit/publish
	rm -rf Tests/GeneralizedAwaitablesValueTaskIntegrationAuditDiagnostics/*/.void Tests/GeneralizedAwaitablesValueTaskIntegrationAuditDiagnostics/*/bin Tests/GeneralizedAwaitablesValueTaskIntegrationAuditDiagnostics/*/publish
	rm -rf Tests/AsyncLambdaSyntaxSemanticFoundation/.void Tests/AsyncLambdaSyntaxSemanticFoundation/bin Tests/AsyncLambdaSyntaxSemanticFoundation/publish
	rm -rf Tests/AsyncLambdaSyntaxSemanticFoundationDiagnostics/*/.void Tests/AsyncLambdaSyntaxSemanticFoundationDiagnostics/*/bin Tests/AsyncLambdaSyntaxSemanticFoundationDiagnostics/*/publish
	rm -rf Tests/AsyncLambdaStateMachineLowering/.void Tests/AsyncLambdaStateMachineLowering/bin Tests/AsyncLambdaStateMachineLowering/publish
	rm -rf Tests/AsyncLambdaCaptureGenericGcLifetimeCompletion/.void Tests/AsyncLambdaCaptureGenericGcLifetimeCompletion/bin Tests/AsyncLambdaCaptureGenericGcLifetimeCompletion/publish
	rm -rf Tests/AsyncLambdaCaptureGenericGcLifetimeCompletionDiagnostics/*/.void Tests/AsyncLambdaCaptureGenericGcLifetimeCompletionDiagnostics/*/bin Tests/AsyncLambdaCaptureGenericGcLifetimeCompletionDiagnostics/*/publish
	rm -rf Tests/AsyncLambdaDelegateEventValueTaskIntegration/.void Tests/AsyncLambdaDelegateEventValueTaskIntegration/bin Tests/AsyncLambdaDelegateEventValueTaskIntegration/publish
	rm -rf Tests/AsyncLambdaDelegateEventValueTaskIntegration/Library/.void Tests/AsyncLambdaDelegateEventValueTaskIntegration/Library/bin Tests/AsyncLambdaDelegateEventValueTaskIntegration/Library/publish
	rm -rf Tests/AsyncLambdaDelegateEventValueTaskIntegrationDiagnostics/*/.void Tests/AsyncLambdaDelegateEventValueTaskIntegrationDiagnostics/*/bin Tests/AsyncLambdaDelegateEventValueTaskIntegrationDiagnostics/*/publish
	rm -f Tests/AsyncLambdaDelegateEventValueTaskIntegration/CConsumer/async-lambda-delegate-event-consumer
	rm -rf Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemantics/.void Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemantics/bin Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemantics/publish
	rm -rf Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemanticsDiagnostics/*/.void Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemanticsDiagnostics/*/bin Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemanticsDiagnostics/*/publish
	rm -rf Tests/AwaitUsingStateMachineStructuredCleanupCompletion/.void Tests/AwaitUsingStateMachineStructuredCleanupCompletion/bin Tests/AwaitUsingStateMachineStructuredCleanupCompletion/publish
	rm -rf Tests/TaskCompletionFactoriesAsyncTaskRunUnwrapping/.void Tests/TaskCompletionFactoriesAsyncTaskRunUnwrapping/bin Tests/TaskCompletionFactoriesAsyncTaskRunUnwrapping/publish
	rm -rf Tests/TaskCompletionFactoriesAsyncTaskRunUnwrappingRuntime/*/.void Tests/TaskCompletionFactoriesAsyncTaskRunUnwrappingRuntime/*/bin Tests/TaskCompletionFactoriesAsyncTaskRunUnwrappingRuntime/*/publish
	rm -rf Tests/TaskWhenAllCompletion/.void Tests/TaskWhenAllCompletion/bin Tests/TaskWhenAllCompletion/publish
	rm -rf Tests/TaskWhenAllCompletionRuntime/*/.void Tests/TaskWhenAllCompletionRuntime/*/bin Tests/TaskWhenAllCompletionRuntime/*/publish
	rm -rf Tests/TaskWhenAnyCompletion/.void Tests/TaskWhenAnyCompletion/bin Tests/TaskWhenAnyCompletion/publish
	rm -rf Tests/TaskWhenAnyCompletionRuntime/*/.void Tests/TaskWhenAnyCompletionRuntime/*/bin Tests/TaskWhenAnyCompletionRuntime/*/publish
	rm -rf Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/.void Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/bin Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/publish
	rm -rf Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/Library/.void Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/Library/bin Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/Library/publish
	rm -rf Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAuditDiagnostics/*/.void Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAuditDiagnostics/*/bin Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAuditDiagnostics/*/publish
	rm -f Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/CConsumer/async-composition-audit-consumer
	rm -f $(AWAIT_USING_SEMANTIC_TEST)
	rm -f $(ASYNC_LAMBDA_SEMANTIC_TEST)
	rm -rf Examples/HelloProject/.void Examples/HelloProject/bin Examples/HelloProject/publish
	rm -rf Examples/HelloProjectless/.void Examples/HelloProjectless/bin Examples/HelloProjectless/publish
	rm -rf Tests/GC/.void Tests/GC/bin Tests/GC/publish
	rm -rf Tests/Properties/.void Tests/Properties/bin Tests/Properties/publish
	rm -rf Tests/Inheritance/.void Tests/Inheritance/bin Tests/Inheritance/publish
	rm -rf Tests/InheritanceGC/.void Tests/InheritanceGC/bin Tests/InheritanceGC/publish
	rm -rf Tests/Interfaces/.void Tests/Interfaces/bin Tests/Interfaces/publish
	rm -rf Tests/Generics/.void Tests/Generics/bin Tests/Generics/publish
	rm -rf Tests/Collections/.void Tests/Collections/bin Tests/Collections/publish
	rm -rf Tests/Foreach/.void Tests/Foreach/bin Tests/Foreach/publish
	rm -rf Tests/Yield/.void Tests/Yield/bin Tests/Yield/publish
	rm -rf Tests/Delegates/.void Tests/Delegates/bin Tests/Delegates/publish
	rm -rf Tests/Lambdas/.void Tests/Lambdas/bin Tests/Lambdas/publish
	rm -rf Tests/Events/.void Tests/Events/bin Tests/Events/publish
	rm -rf Tests/QueueStack/.void Tests/QueueStack/bin Tests/QueueStack/publish
	rm -rf Tests/DictionaryHashSet/.void Tests/DictionaryHashSet/bin Tests/DictionaryHashSet/publish
	rm -rf Tests/Attributes/.void Tests/Attributes/bin Tests/Attributes/publish
	rm -rf Tests/NativeInterop/.void Tests/NativeInterop/bin Tests/NativeInterop/publish
	rm -rf Tests/Pointers/.void Tests/Pointers/bin Tests/Pointers/publish
	rm -rf Tests/NativeStructs/.void Tests/NativeStructs/bin Tests/NativeStructs/publish
	rm -rf Tests/NativeByRef/.void Tests/NativeByRef/bin Tests/NativeByRef/publish
	rm -rf Tests/StaticCalls/.void Tests/StaticCalls/bin Tests/StaticCalls/publish
	rm -rf Tests/NativeAliases/.void Tests/NativeAliases/bin Tests/NativeAliases/publish
	rm -rf Tests/NativeLibraries/.void Tests/NativeLibraries/bin Tests/NativeLibraries/publish
	rm -rf Tests/NativeCallbacks/.void Tests/NativeCallbacks/bin Tests/NativeCallbacks/publish
	rm -rf Tests/Memory/.void Tests/Memory/bin Tests/Memory/publish
	rm -rf Tests/StackAlloc/.void Tests/StackAlloc/bin Tests/StackAlloc/publish
	rm -rf Tests/NativeIntegration/.void Tests/NativeIntegration/bin Tests/NativeIntegration/publish
	rm -rf Tests/Enums/.void Tests/Enums/bin Tests/Enums/publish
	rm -rf Tests/Switch/.void Tests/Switch/bin Tests/Switch/publish
	rm -rf Tests/ConstReadonly/.void Tests/ConstReadonly/bin Tests/ConstReadonly/publish
	rm -rf Tests/Casts/.void Tests/Casts/bin Tests/Casts/publish
	rm -rf Tests/NullOperators/.void Tests/NullOperators/bin Tests/NullOperators/publish
	rm -rf Tests/OptionalParams/.void Tests/OptionalParams/bin Tests/OptionalParams/publish
	rm -rf Tests/DefaultValues/.void Tests/DefaultValues/bin Tests/DefaultValues/publish
	rm -rf Tests/TypeMetadata/.void Tests/TypeMetadata/bin Tests/TypeMetadata/publish
	rm -rf Tests/Conversions/.void Tests/Conversions/bin Tests/Conversions/publish
	rm -rf Tests/ErgonomicsIntegration/.void Tests/ErgonomicsIntegration/bin Tests/ErgonomicsIntegration/publish
	rm -rf Tests/NullableValues/.void Tests/NullableValues/bin Tests/NullableValues/publish
	rm -rf Tests/NullableOperators/.void Tests/NullableOperators/bin Tests/NullableOperators/publish
	rm -rf Tests/NamedArguments/.void Tests/NamedArguments/bin Tests/NamedArguments/publish
	rm -rf Tests/TargetTypedDefault/.void Tests/TargetTypedDefault/bin Tests/TargetTypedDefault/publish
	rm -rf Tests/InstanceInitializers/.void Tests/InstanceInitializers/bin Tests/InstanceInitializers/publish
	rm -rf Tests/ObjectInitializers/.void Tests/ObjectInitializers/bin Tests/ObjectInitializers/publish
	rm -rf Tests/CollectionInitializers/.void Tests/CollectionInitializers/bin Tests/CollectionInitializers/publish
	rm -rf Tests/StaticMembers/.void Tests/StaticMembers/bin Tests/StaticMembers/publish
	rm -rf Tests/StaticInitialization/.void Tests/StaticInitialization/bin Tests/StaticInitialization/publish
	rm -rf Tests/LanguageCompletionIntegration/.void Tests/LanguageCompletionIntegration/bin Tests/LanguageCompletionIntegration/publish
	rm -rf Tests/Diagnostics/Semantic/.void Tests/Diagnostics/Semantic/bin Tests/Diagnostics/Semantic/publish
	rm -rf Tests/Check/Valid/.void Tests/Check/Valid/bin Tests/Check/Valid/publish
	rm -rf Tests/Check/Semantic/.void Tests/Check/Semantic/bin Tests/Check/Semantic/publish
	rm -rf Tests/Check/Parser/.void Tests/Check/Parser/bin Tests/Check/Parser/publish
	rm -rf Tests/Check/Projectless/.void Tests/Check/Projectless/bin Tests/Check/Projectless/publish
	rm -rf Tests/SemanticQueries/.void Tests/SemanticQueries/bin Tests/SemanticQueries/publish
	rm -rf Tests/DebugInfo/.void Tests/DebugInfo/bin Tests/DebugInfo/publish
	rm -rf Tests/LspProtocol/.void Tests/LspProtocol/bin Tests/LspProtocol/publish
	rm -rf Tests/LspDiagnostics/.void Tests/LspDiagnostics/bin Tests/LspDiagnostics/publish
	rm -rf Tests/LspHoverSignature/.void Tests/LspHoverSignature/bin Tests/LspHoverSignature/publish
	rm -rf Tests/LspCompletion/.void Tests/LspCompletion/bin Tests/LspCompletion/publish
	rm -rf Tests/LspNavigation/.void Tests/LspNavigation/bin Tests/LspNavigation/publish
	rm -rf Tests/ToolingIntegration/.void Tests/ToolingIntegration/bin Tests/ToolingIntegration/publish
	rm -rf Tests/BaseConstructors/.void Tests/BaseConstructors/bin Tests/BaseConstructors/publish
	rm -rf Tests/BaseConstructorDiagnostics/Missing/.void Tests/BaseConstructorDiagnostics/Missing/bin Tests/BaseConstructorDiagnostics/Missing/publish
	rm -rf Tests/BaseConstructorDiagnostics/Root/.void Tests/BaseConstructorDiagnostics/Root/bin Tests/BaseConstructorDiagnostics/Root/publish
	rm -rf Tests/ConstructorDelegation/.void Tests/ConstructorDelegation/bin Tests/ConstructorDelegation/publish
	rm -rf Tests/ConstructorDelegationDiagnostics/Cycle/.void Tests/ConstructorDelegationDiagnostics/Cycle/bin Tests/ConstructorDelegationDiagnostics/Cycle/publish
	rm -rf Tests/ConstructorDelegationDiagnostics/NoMatch/.void Tests/ConstructorDelegationDiagnostics/NoMatch/bin Tests/ConstructorDelegationDiagnostics/NoMatch/publish
	rm -rf Tests/ConstructorDelegationDiagnostics/ThisAccess/.void Tests/ConstructorDelegationDiagnostics/ThisAccess/bin Tests/ConstructorDelegationDiagnostics/ThisAccess/publish
	rm -rf Tests/InterfaceInheritance/.void Tests/InterfaceInheritance/bin Tests/InterfaceInheritance/publish
	rm -rf Tests/InterfaceInheritanceDiagnostics/Cycle/.void Tests/InterfaceInheritanceDiagnostics/Cycle/bin Tests/InterfaceInheritanceDiagnostics/Cycle/publish
	rm -rf Tests/InterfaceInheritanceDiagnostics/NonInterface/.void Tests/InterfaceInheritanceDiagnostics/NonInterface/bin Tests/InterfaceInheritanceDiagnostics/NonInterface/publish
	rm -rf Tests/InterfaceInheritanceDiagnostics/Conflict/.void Tests/InterfaceInheritanceDiagnostics/Conflict/bin Tests/InterfaceInheritanceDiagnostics/Conflict/publish
	rm -rf Tests/InterfaceInheritanceDiagnostics/PropertyConflict/.void Tests/InterfaceInheritanceDiagnostics/PropertyConflict/bin Tests/InterfaceInheritanceDiagnostics/PropertyConflict/publish
	rm -rf Tests/InterfaceInheritanceDiagnostics/Missing/.void Tests/InterfaceInheritanceDiagnostics/Missing/bin Tests/InterfaceInheritanceDiagnostics/Missing/publish
	rm -rf Tests/InterfaceInheritanceDiagnostics/MissingProperty/.void Tests/InterfaceInheritanceDiagnostics/MissingProperty/bin Tests/InterfaceInheritanceDiagnostics/MissingProperty/publish
	rm -rf Tests/StructInterfaces/.void Tests/StructInterfaces/bin Tests/StructInterfaces/publish
	rm -rf Tests/StructInterfaceDiagnostics/Missing/.void Tests/StructInterfaceDiagnostics/Missing/bin Tests/StructInterfaceDiagnostics/Missing/publish
	rm -rf Tests/StructInterfaceDiagnostics/MissingProperty/.void Tests/StructInterfaceDiagnostics/MissingProperty/bin Tests/StructInterfaceDiagnostics/MissingProperty/publish
	rm -rf Tests/StructInterfaceDiagnostics/NonInterface/.void Tests/StructInterfaceDiagnostics/NonInterface/bin Tests/StructInterfaceDiagnostics/NonInterface/publish
	rm -rf Tests/StructInterfaceDiagnostics/Inaccessible/.void Tests/StructInterfaceDiagnostics/Inaccessible/bin Tests/StructInterfaceDiagnostics/Inaccessible/publish
	rm -rf Tests/VirtualProperties/.void Tests/VirtualProperties/bin Tests/VirtualProperties/publish
	rm -rf Tests/VirtualPropertyDiagnostics/NonVirtual/.void Tests/VirtualPropertyDiagnostics/NonVirtual/bin Tests/VirtualPropertyDiagnostics/NonVirtual/publish
	rm -rf Tests/VirtualPropertyDiagnostics/Missing/.void Tests/VirtualPropertyDiagnostics/Missing/bin Tests/VirtualPropertyDiagnostics/Missing/publish
	rm -rf Tests/VirtualPropertyDiagnostics/Sealed/.void Tests/VirtualPropertyDiagnostics/Sealed/bin Tests/VirtualPropertyDiagnostics/Sealed/publish
	rm -rf Tests/VirtualPropertyDiagnostics/AbstractMissing/.void Tests/VirtualPropertyDiagnostics/AbstractMissing/bin Tests/VirtualPropertyDiagnostics/AbstractMissing/publish
	rm -rf Tests/VirtualPropertyDiagnostics/ReAbstractMissing/.void Tests/VirtualPropertyDiagnostics/ReAbstractMissing/bin Tests/VirtualPropertyDiagnostics/ReAbstractMissing/publish
	rm -rf Tests/VirtualPropertyDiagnostics/TypeMismatch/.void Tests/VirtualPropertyDiagnostics/TypeMismatch/bin Tests/VirtualPropertyDiagnostics/TypeMismatch/publish
	rm -rf Tests/VirtualPropertyDiagnostics/AccessorShape/.void Tests/VirtualPropertyDiagnostics/AccessorShape/bin Tests/VirtualPropertyDiagnostics/AccessorShape/publish
	rm -rf Tests/VirtualPropertyDiagnostics/Static/.void Tests/VirtualPropertyDiagnostics/Static/bin Tests/VirtualPropertyDiagnostics/Static/publish
	rm -rf Tests/VirtualPropertyDiagnostics/AbstractBody/.void Tests/VirtualPropertyDiagnostics/AbstractBody/bin Tests/VirtualPropertyDiagnostics/AbstractBody/publish
	rm -rf Tests/VirtualPropertyDiagnostics/PrivateAccess/.void Tests/VirtualPropertyDiagnostics/PrivateAccess/bin Tests/VirtualPropertyDiagnostics/PrivateAccess/publish
	rm -rf Tests/VirtualPropertyDiagnostics/StructVirtual/.void Tests/VirtualPropertyDiagnostics/StructVirtual/bin Tests/VirtualPropertyDiagnostics/StructVirtual/publish
	rm -rf Tests/StaticEvents/.void Tests/StaticEvents/bin Tests/StaticEvents/publish
	rm -rf Tests/StaticEventDiagnostics/ExternalRead/.void Tests/StaticEventDiagnostics/ExternalRead/bin Tests/StaticEventDiagnostics/ExternalRead/publish
	rm -rf Tests/StaticEventDiagnostics/ExternalInvoke/.void Tests/StaticEventDiagnostics/ExternalInvoke/bin Tests/StaticEventDiagnostics/ExternalInvoke/publish
	rm -rf Tests/StaticEventDiagnostics/ExternalAssign/.void Tests/StaticEventDiagnostics/ExternalAssign/bin Tests/StaticEventDiagnostics/ExternalAssign/publish
	rm -rf Tests/StaticEventDiagnostics/Initializer/.void Tests/StaticEventDiagnostics/Initializer/bin Tests/StaticEventDiagnostics/Initializer/publish
	rm -rf Tests/StaticEventDiagnostics/NonDelegate/.void Tests/StaticEventDiagnostics/NonDelegate/bin Tests/StaticEventDiagnostics/NonDelegate/publish
	rm -rf Tests/InterfaceEvents/.void Tests/InterfaceEvents/bin Tests/InterfaceEvents/publish
	rm -rf Tests/InterfaceEventDiagnostics/Missing/.void Tests/InterfaceEventDiagnostics/Missing/bin Tests/InterfaceEventDiagnostics/Missing/publish
	rm -rf Tests/InterfaceEventDiagnostics/TypeMismatch/.void Tests/InterfaceEventDiagnostics/TypeMismatch/bin Tests/InterfaceEventDiagnostics/TypeMismatch/publish
	rm -rf Tests/InterfaceEventDiagnostics/Inaccessible/.void Tests/InterfaceEventDiagnostics/Inaccessible/bin Tests/InterfaceEventDiagnostics/Inaccessible/publish
	rm -rf Tests/InterfaceEventDiagnostics/InterfaceInitializer/.void Tests/InterfaceEventDiagnostics/InterfaceInitializer/bin Tests/InterfaceEventDiagnostics/InterfaceInitializer/publish
	rm -rf Tests/InterfaceEventDiagnostics/StaticInterface/.void Tests/InterfaceEventDiagnostics/StaticInterface/bin Tests/InterfaceEventDiagnostics/StaticInterface/publish
	rm -rf Tests/InterfaceEventDiagnostics/Conflict/.void Tests/InterfaceEventDiagnostics/Conflict/bin Tests/InterfaceEventDiagnostics/Conflict/publish
	rm -rf Tests/InterfaceEventDiagnostics/ExternalRead/.void Tests/InterfaceEventDiagnostics/ExternalRead/bin Tests/InterfaceEventDiagnostics/ExternalRead/publish
	rm -rf Tests/DelegateCompletion/.void Tests/DelegateCompletion/bin Tests/DelegateCompletion/publish
	rm -rf Tests/DelegateCompletionDiagnostics/MethodGroupMismatch/.void Tests/DelegateCompletionDiagnostics/MethodGroupMismatch/bin Tests/DelegateCompletionDiagnostics/MethodGroupMismatch/publish
	rm -rf Tests/DelegateCompletionDiagnostics/InvokeMissingModifier/.void Tests/DelegateCompletionDiagnostics/InvokeMissingModifier/bin Tests/DelegateCompletionDiagnostics/InvokeMissingModifier/publish
	rm -rf Tests/DelegateCompletionDiagnostics/InvokeWrongModifier/.void Tests/DelegateCompletionDiagnostics/InvokeWrongModifier/bin Tests/DelegateCompletionDiagnostics/InvokeWrongModifier/publish
	rm -rf Tests/DelegateCompletionDiagnostics/EqualityMismatch/.void Tests/DelegateCompletionDiagnostics/EqualityMismatch/bin Tests/DelegateCompletionDiagnostics/EqualityMismatch/publish
	rm -rf Tests/ConditionalExpressions/.void Tests/ConditionalExpressions/bin Tests/ConditionalExpressions/publish
	rm -rf Tests/ConditionalExpressionDiagnostics/NonBool/.void Tests/ConditionalExpressionDiagnostics/NonBool/bin Tests/ConditionalExpressionDiagnostics/NonBool/publish
	rm -rf Tests/ConditionalExpressionDiagnostics/NoCommonType/.void Tests/ConditionalExpressionDiagnostics/NoCommonType/bin Tests/ConditionalExpressionDiagnostics/NoCommonType/publish
	rm -rf Tests/ConditionalExpressionDiagnostics/TargetMismatch/.void Tests/ConditionalExpressionDiagnostics/TargetMismatch/bin Tests/ConditionalExpressionDiagnostics/TargetMismatch/publish
	rm -rf Tests/ConditionalExpressionDiagnostics/NoTarget/.void Tests/ConditionalExpressionDiagnostics/NoTarget/bin Tests/ConditionalExpressionDiagnostics/NoTarget/publish
	rm -rf Tests/ConditionalExpressionDiagnostics/DefiniteAssignment/.void Tests/ConditionalExpressionDiagnostics/DefiniteAssignment/bin Tests/ConditionalExpressionDiagnostics/DefiniteAssignment/publish
	rm -rf Tests/ObjectModelIntegration/.void Tests/ObjectModelIntegration/bin Tests/ObjectModelIntegration/publish
	rm -rf Tests/NestedValueStructs/.void Tests/NestedValueStructs/bin Tests/NestedValueStructs/publish
	rm -rf Tests/NestedValueStructDiagnostics/Direct/.void Tests/NestedValueStructDiagnostics/Direct/bin Tests/NestedValueStructDiagnostics/Direct/publish
	rm -rf Tests/NestedValueStructDiagnostics/Indirect/.void Tests/NestedValueStructDiagnostics/Indirect/bin Tests/NestedValueStructDiagnostics/Indirect/publish
	rm -rf Tests/NestedValueStructDiagnostics/Nullable/.void Tests/NestedValueStructDiagnostics/Nullable/bin Tests/NestedValueStructDiagnostics/Nullable/publish
	rm -rf Tests/NestedValueStructDiagnostics/Property/.void Tests/NestedValueStructDiagnostics/Property/bin Tests/NestedValueStructDiagnostics/Property/publish
	rm -rf Tests/ReadonlyStructs/.void Tests/ReadonlyStructs/bin Tests/ReadonlyStructs/publish
	rm -rf Tests/ReadonlyStructDiagnostics/MutableField/.void Tests/ReadonlyStructDiagnostics/MutableField/bin Tests/ReadonlyStructDiagnostics/MutableField/publish
	rm -rf Tests/ReadonlyStructDiagnostics/MethodMutation/.void Tests/ReadonlyStructDiagnostics/MethodMutation/bin Tests/ReadonlyStructDiagnostics/MethodMutation/publish
	rm -rf Tests/ReadonlyStructDiagnostics/InNonReadonlyCall/.void Tests/ReadonlyStructDiagnostics/InNonReadonlyCall/bin Tests/ReadonlyStructDiagnostics/InNonReadonlyCall/publish
	rm -rf Tests/ReadonlyStructDiagnostics/ReadonlyFieldCall/.void Tests/ReadonlyStructDiagnostics/ReadonlyFieldCall/bin Tests/ReadonlyStructDiagnostics/ReadonlyFieldCall/publish
	rm -rf Tests/ReadonlyStructDiagnostics/RefEscape/.void Tests/ReadonlyStructDiagnostics/RefEscape/bin Tests/ReadonlyStructDiagnostics/RefEscape/publish
	rm -rf Tests/ReadonlyStructDiagnostics/NonReadonlyProperty/.void Tests/ReadonlyStructDiagnostics/NonReadonlyProperty/bin Tests/ReadonlyStructDiagnostics/NonReadonlyProperty/publish
	rm -rf Tests/ReadonlyStructDiagnostics/InvalidReadonlyMethod/.void Tests/ReadonlyStructDiagnostics/InvalidReadonlyMethod/bin Tests/ReadonlyStructDiagnostics/InvalidReadonlyMethod/publish
	rm -rf Tests/ReadonlyStructDiagnostics/AutoSetter/.void Tests/ReadonlyStructDiagnostics/AutoSetter/bin Tests/ReadonlyStructDiagnostics/AutoSetter/publish
	rm -rf Tests/ReadonlyStructDiagnostics/ReadonlySetter/.void Tests/ReadonlyStructDiagnostics/ReadonlySetter/bin Tests/ReadonlyStructDiagnostics/ReadonlySetter/publish
	rm -rf Tests/ReadonlyStructDiagnostics/ReadonlyConstructor/.void Tests/ReadonlyStructDiagnostics/ReadonlyConstructor/bin Tests/ReadonlyStructDiagnostics/ReadonlyConstructor/publish
	rm -rf Tests/ReadonlyStructDiagnostics/ReadonlyThisCall/.void Tests/ReadonlyStructDiagnostics/ReadonlyThisCall/bin Tests/ReadonlyStructDiagnostics/ReadonlyThisCall/publish
	rm -rf Tests/ReadonlyStructDiagnostics/NestedMutation/.void Tests/ReadonlyStructDiagnostics/NestedMutation/bin Tests/ReadonlyStructDiagnostics/NestedMutation/publish
	rm -rf Tests/JaggedArrays/.void Tests/JaggedArrays/bin Tests/JaggedArrays/publish
	rm -rf Tests/JaggedArrayDiagnostics/ElementMismatch/.void Tests/JaggedArrayDiagnostics/ElementMismatch/bin Tests/JaggedArrayDiagnostics/ElementMismatch/publish
	rm -rf Tests/JaggedArrayDiagnostics/DepthMismatch/.void Tests/JaggedArrayDiagnostics/DepthMismatch/bin Tests/JaggedArrayDiagnostics/DepthMismatch/publish
	rm -rf Tests/JaggedArrayDiagnostics/LengthType/.void Tests/JaggedArrayDiagnostics/LengthType/bin Tests/JaggedArrayDiagnostics/LengthType/publish
	rm -rf Tests/JaggedArrayDiagnostics/SizedOrder/.void Tests/JaggedArrayDiagnostics/SizedOrder/bin Tests/JaggedArrayDiagnostics/SizedOrder/publish
	rm -rf Tests/MultidimensionalArrays/.void Tests/MultidimensionalArrays/bin Tests/MultidimensionalArrays/publish
	rm -rf Tests/MultidimensionalArrayDiagnostics/RankIndex/.void Tests/MultidimensionalArrayDiagnostics/RankIndex/bin Tests/MultidimensionalArrayDiagnostics/RankIndex/publish
	rm -rf Tests/MultidimensionalArrayDiagnostics/IndexType/.void Tests/MultidimensionalArrayDiagnostics/IndexType/bin Tests/MultidimensionalArrayDiagnostics/IndexType/publish
	rm -rf Tests/MultidimensionalArrayDiagnostics/LengthType/.void Tests/MultidimensionalArrayDiagnostics/LengthType/bin Tests/MultidimensionalArrayDiagnostics/LengthType/publish
	rm -rf Tests/MultidimensionalArrayDiagnostics/AssignmentRank/.void Tests/MultidimensionalArrayDiagnostics/AssignmentRank/bin Tests/MultidimensionalArrayDiagnostics/AssignmentRank/publish
	rm -rf Tests/MultidimensionalArrayDiagnostics/GetLength/.void Tests/MultidimensionalArrayDiagnostics/GetLength/bin Tests/MultidimensionalArrayDiagnostics/GetLength/publish
	rm -rf Tests/MultidimensionalArrayDiagnostics/MalformedType/.void Tests/MultidimensionalArrayDiagnostics/MalformedType/bin Tests/MultidimensionalArrayDiagnostics/MalformedType/publish
	rm -rf Tests/MultidimensionalArrayDiagnostics/InitializerLiteral/.void Tests/MultidimensionalArrayDiagnostics/InitializerLiteral/bin Tests/MultidimensionalArrayDiagnostics/InitializerLiteral/publish
	rm -rf Tests/MultidimensionalArrayRuntime/IndexBounds/.void Tests/MultidimensionalArrayRuntime/IndexBounds/bin Tests/MultidimensionalArrayRuntime/IndexBounds/publish
	rm -rf Tests/MultidimensionalArrayRuntime/DimensionBounds/.void Tests/MultidimensionalArrayRuntime/DimensionBounds/bin Tests/MultidimensionalArrayRuntime/DimensionBounds/publish
	rm -rf Tests/MultidimensionalArrayRuntime/NegativeLength/.void Tests/MultidimensionalArrayRuntime/NegativeLength/bin Tests/MultidimensionalArrayRuntime/NegativeLength/publish
	rm -rf Tests/ClosureCompletionI/.void Tests/ClosureCompletionI/bin Tests/ClosureCompletionI/publish
	rm -rf Tests/ClosureCompletionII/.void Tests/ClosureCompletionII/bin Tests/ClosureCompletionII/publish
	rm -rf Tests/ClosureCompletionIIDiagnostics/OutMissingAssignment/.void Tests/ClosureCompletionIIDiagnostics/OutMissingAssignment/bin Tests/ClosureCompletionIIDiagnostics/OutMissingAssignment/publish
	rm -rf Tests/ClosureCompletionIIDiagnostics/InMutation/.void Tests/ClosureCompletionIIDiagnostics/InMutation/bin Tests/ClosureCompletionIIDiagnostics/InMutation/publish
	rm -rf Tests/IteratorCompletion/.void Tests/IteratorCompletion/bin Tests/IteratorCompletion/publish
	rm -rf Tests/UnsafeMemberCompletion/.void Tests/UnsafeMemberCompletion/bin Tests/UnsafeMemberCompletion/publish
	rm -rf Tests/UnsafeMemberCompletionDiagnostics/SafeProject/.void Tests/UnsafeMemberCompletionDiagnostics/SafeProject/bin Tests/UnsafeMemberCompletionDiagnostics/SafeProject/publish
	rm -rf Tests/UnsafeMemberCompletionDiagnostics/SafeStructProperty/.void Tests/UnsafeMemberCompletionDiagnostics/SafeStructProperty/bin Tests/UnsafeMemberCompletionDiagnostics/SafeStructProperty/publish
	rm -rf Tests/UnsafeMemberCompletionDiagnostics/ClassProperty/.void Tests/UnsafeMemberCompletionDiagnostics/ClassProperty/bin Tests/UnsafeMemberCompletionDiagnostics/ClassProperty/publish
	rm -rf Tests/UnsafeMemberCompletionDiagnostics/SafeConstructor/.void Tests/UnsafeMemberCompletionDiagnostics/SafeConstructor/bin Tests/UnsafeMemberCompletionDiagnostics/SafeConstructor/publish
	rm -rf Tests/UnsafeMemberCompletionDiagnostics/UnsafeConstructorSafeProject/.void Tests/UnsafeMemberCompletionDiagnostics/UnsafeConstructorSafeProject/bin Tests/UnsafeMemberCompletionDiagnostics/UnsafeConstructorSafeProject/publish
	rm -rf Tests/UnsafeMemberCompletionDiagnostics/UnsupportedProperty/.void Tests/UnsafeMemberCompletionDiagnostics/UnsupportedProperty/bin Tests/UnsafeMemberCompletionDiagnostics/UnsupportedProperty/publish
	rm -rf Tests/UnsafeMemberCompletionDiagnostics/UnsupportedConstructor/.void Tests/UnsafeMemberCompletionDiagnostics/UnsupportedConstructor/bin Tests/UnsafeMemberCompletionDiagnostics/UnsupportedConstructor/publish
	rm -rf Tests/UnsafeMemberCompletionDiagnostics/InterfaceProperty/.void Tests/UnsafeMemberCompletionDiagnostics/InterfaceProperty/bin Tests/UnsafeMemberCompletionDiagnostics/InterfaceProperty/publish
	rm -rf Tests/CompoundTargets/.void Tests/CompoundTargets/bin Tests/CompoundTargets/publish
	rm -rf Tests/CompoundTargetDiagnostics/MissingSetter/.void Tests/CompoundTargetDiagnostics/MissingSetter/bin Tests/CompoundTargetDiagnostics/MissingSetter/publish
	rm -rf Tests/CompoundTargetDiagnostics/MissingIndexerOperator/.void Tests/CompoundTargetDiagnostics/MissingIndexerOperator/bin Tests/CompoundTargetDiagnostics/MissingIndexerOperator/publish
	rm -rf Tests/CompoundTargetDiagnostics/DuplicateSwizzle/.void Tests/CompoundTargetDiagnostics/DuplicateSwizzle/bin Tests/CompoundTargetDiagnostics/DuplicateSwizzle/publish
	rm -rf Tests/CompoundTargetDiagnostics/NonWritableSwizzle/.void Tests/CompoundTargetDiagnostics/NonWritableSwizzle/bin Tests/CompoundTargetDiagnostics/NonWritableSwizzle/publish
	rm -rf Tests/CompoundTargetDiagnostics/MissingSwizzleOperator/.void Tests/CompoundTargetDiagnostics/MissingSwizzleOperator/bin Tests/CompoundTargetDiagnostics/MissingSwizzleOperator/publish
	rm -rf Tests/ValueTypesClosuresControlFlowIntegration/.void Tests/ValueTypesClosuresControlFlowIntegration/bin Tests/ValueTypesClosuresControlFlowIntegration/publish
	rm -rf Tests/StaticInterfaceProperties/.void Tests/StaticInterfaceProperties/bin Tests/StaticInterfaceProperties/publish
	rm -rf Tests/StaticInterfacePropertyDiagnostics/*/.void Tests/StaticInterfacePropertyDiagnostics/*/bin Tests/StaticInterfacePropertyDiagnostics/*/publish
	rm -rf Tests/StaticInterfaceEvents/.void Tests/StaticInterfaceEvents/bin Tests/StaticInterfaceEvents/publish
	rm -rf Tests/StaticInterfaceEventDiagnostics/*/.void Tests/StaticInterfaceEventDiagnostics/*/bin Tests/StaticInterfaceEventDiagnostics/*/publish
	rm -rf Tests/ConstructorReturns/.void Tests/ConstructorReturns/bin Tests/ConstructorReturns/publish
	rm -rf Tests/ConstructorReturnDiagnostics/*/.void Tests/ConstructorReturnDiagnostics/*/bin Tests/ConstructorReturnDiagnostics/*/publish
	rm -rf Tests/RectangularArrayInitializers/.void Tests/RectangularArrayInitializers/bin Tests/RectangularArrayInitializers/publish
	rm -rf Tests/IteratorLocals/.void Tests/IteratorLocals/bin Tests/IteratorLocals/publish
	rm -rf Tests/IteratorLocalDiagnostics/*/.void Tests/IteratorLocalDiagnostics/*/bin Tests/IteratorLocalDiagnostics/*/publish
	rm -rf Tests/ReceiverExpressions/.void Tests/ReceiverExpressions/bin Tests/ReceiverExpressions/publish
	rm -rf Tests/ReceiverExpressionDiagnostics/*/.void Tests/ReceiverExpressionDiagnostics/*/bin Tests/ReceiverExpressionDiagnostics/*/publish
	rm -rf Tests/GenericMethodCalls/.void Tests/GenericMethodCalls/bin Tests/GenericMethodCalls/publish
	rm -rf Tests/GenericMethodCallDiagnostics/*/.void Tests/GenericMethodCallDiagnostics/*/bin Tests/GenericMethodCallDiagnostics/*/publish
	rm -rf Tests/GenericMethodTypeInference/.void Tests/GenericMethodTypeInference/bin Tests/GenericMethodTypeInference/publish
	rm -rf Tests/GenericMethodTypeInferenceDiagnostics/*/.void Tests/GenericMethodTypeInferenceDiagnostics/*/bin Tests/GenericMethodTypeInferenceDiagnostics/*/publish
	rm -rf Tests/ExtensionMethodCompletion/.void Tests/ExtensionMethodCompletion/bin Tests/ExtensionMethodCompletion/publish
	rm -rf Tests/StaticInterfaceMethods/.void Tests/StaticInterfaceMethods/bin Tests/StaticInterfaceMethods/publish
	rm -rf Tests/StaticInterfaceMethodDiagnostics/*/.void Tests/StaticInterfaceMethodDiagnostics/*/bin Tests/StaticInterfaceMethodDiagnostics/*/publish
	rm -rf Tests/ConstrainedStaticInterfaceDispatch/.void Tests/ConstrainedStaticInterfaceDispatch/bin Tests/ConstrainedStaticInterfaceDispatch/publish
	rm -rf Tests/ConstrainedStaticInterfaceDispatchDiagnostics/*/.void Tests/ConstrainedStaticInterfaceDispatchDiagnostics/*/bin Tests/ConstrainedStaticInterfaceDispatchDiagnostics/*/publish
	rm -rf Tests/DefaultInterfaceMethods/.void Tests/DefaultInterfaceMethods/bin Tests/DefaultInterfaceMethods/publish
	rm -rf Tests/DefaultInterfaceMethodDiagnostics/*/.void Tests/DefaultInterfaceMethodDiagnostics/*/bin Tests/DefaultInterfaceMethodDiagnostics/*/publish
	rm -rf Tests/DeterministicLifetimeAdvancedAbstractionsIntegration/.void Tests/DeterministicLifetimeAdvancedAbstractionsIntegration/bin Tests/DeterministicLifetimeAdvancedAbstractionsIntegration/publish
	rm -rf Tests/DeterministicLifetimeAdvancedAbstractionsIntegrationDiagnostics/*/.void Tests/DeterministicLifetimeAdvancedAbstractionsIntegrationDiagnostics/*/bin Tests/DeterministicLifetimeAdvancedAbstractionsIntegrationDiagnostics/*/publish
	rm -rf Tests/DeclarationPatterns/.void Tests/DeclarationPatterns/bin Tests/DeclarationPatterns/publish
	rm -rf Tests/DeclarationPatternDiagnostics/*/.void Tests/DeclarationPatternDiagnostics/*/bin Tests/DeclarationPatternDiagnostics/*/publish
	rm -rf Tests/ConstantNullPatterns/.void Tests/ConstantNullPatterns/bin Tests/ConstantNullPatterns/publish
	rm -rf Tests/ConstantNullPatternDiagnostics/*/.void Tests/ConstantNullPatternDiagnostics/*/bin Tests/ConstantNullPatternDiagnostics/*/publish
	rm -rf Tests/RelationalPatterns/.void Tests/RelationalPatterns/bin Tests/RelationalPatterns/publish
	rm -rf Tests/LogicalPatterns/.void Tests/LogicalPatterns/bin Tests/LogicalPatterns/publish
	rm -rf Tests/LogicalPatternDiagnostics/*/.void Tests/LogicalPatternDiagnostics/*/bin Tests/LogicalPatternDiagnostics/*/publish
	rm -rf Tests/PropertyPatterns/.void Tests/PropertyPatterns/bin Tests/PropertyPatterns/publish
	rm -rf Tests/PropertyPatternDiagnostics/*/.void Tests/PropertyPatternDiagnostics/*/bin Tests/PropertyPatternDiagnostics/*/publish
	rm -rf Tests/RecursivePatterns/.void Tests/RecursivePatterns/bin Tests/RecursivePatterns/publish
	rm -rf Tests/RecursivePatternDiagnostics/*/.void Tests/RecursivePatternDiagnostics/*/bin Tests/RecursivePatternDiagnostics/*/publish
	rm -rf Tests/PatternSwitchStatement/.void Tests/PatternSwitchStatement/bin Tests/PatternSwitchStatement/publish
	rm -rf Tests/PatternSwitchDiagnostics/*/.void Tests/PatternSwitchDiagnostics/*/bin Tests/PatternSwitchDiagnostics/*/publish
	rm -rf Tests/SwitchGuardsPatternOrdering/.void Tests/SwitchGuardsPatternOrdering/bin Tests/SwitchGuardsPatternOrdering/publish
	rm -rf Tests/SwitchGuardDiagnostics/*/.void Tests/SwitchGuardDiagnostics/*/bin Tests/SwitchGuardDiagnostics/*/publish
	rm -rf Tests/SwitchExpressions/.void Tests/SwitchExpressions/bin Tests/SwitchExpressions/publish
	rm -rf Tests/PatternControlFlowIntegration/.void Tests/PatternControlFlowIntegration/bin Tests/PatternControlFlowIntegration/publish
	rm -rf Tests/PatternControlFlowIntegrationDiagnostics/*/.void Tests/PatternControlFlowIntegrationDiagnostics/*/bin Tests/PatternControlFlowIntegrationDiagnostics/*/publish
	rm -rf Tests/IteratorPatternLocalCapture/.void Tests/IteratorPatternLocalCapture/bin Tests/IteratorPatternLocalCapture/publish
	rm -rf Tests/IteratorPatternLocalCaptureDiagnostics/*/.void Tests/IteratorPatternLocalCaptureDiagnostics/*/bin Tests/IteratorPatternLocalCaptureDiagnostics/*/publish
	rm -rf Tests/IteratorSwitchGuardLocalCapture/.void Tests/IteratorSwitchGuardLocalCapture/bin Tests/IteratorSwitchGuardLocalCapture/publish
	rm -rf Tests/IteratorSwitchGuardLocalCaptureDiagnostics/*/.void Tests/IteratorSwitchGuardLocalCaptureDiagnostics/*/bin Tests/IteratorSwitchGuardLocalCaptureDiagnostics/*/publish
	rm -rf Tests/IteratorCapturedLifetimeGcIntegration/.void Tests/IteratorCapturedLifetimeGcIntegration/bin Tests/IteratorCapturedLifetimeGcIntegration/publish
	rm -rf Tests/IteratorStateMachineIntegrationAudit/.void Tests/IteratorStateMachineIntegrationAudit/bin Tests/IteratorStateMachineIntegrationAudit/publish
	rm -rf Tests/DiagnosticContractCleanup/.void Tests/DiagnosticContractCleanup/bin Tests/DiagnosticContractCleanup/publish
	rm -rf Tests/DiagnosticContractCleanupDiagnostics/*/.void Tests/DiagnosticContractCleanupDiagnostics/*/bin Tests/DiagnosticContractCleanupDiagnostics/*/publish
	rm -rf Tests/UnaryOperatorCompletion/.void Tests/UnaryOperatorCompletion/bin Tests/UnaryOperatorCompletion/publish
	rm -rf Tests/UnaryOperatorCompletionDiagnostics/*/.void Tests/UnaryOperatorCompletionDiagnostics/*/bin Tests/UnaryOperatorCompletionDiagnostics/*/publish
	rm -rf Tests/BinaryOperatorCompletion/.void Tests/BinaryOperatorCompletion/bin Tests/BinaryOperatorCompletion/publish
	rm -rf Tests/BinaryOperatorCompletionDiagnostics/*/.void Tests/BinaryOperatorCompletionDiagnostics/*/bin Tests/BinaryOperatorCompletionDiagnostics/*/publish
	rm -rf Tests/StaticInterfaceOperatorContracts/.void Tests/StaticInterfaceOperatorContracts/bin Tests/StaticInterfaceOperatorContracts/publish
	rm -rf Tests/ConstrainedGenericOperatorDispatch/.void Tests/ConstrainedGenericOperatorDispatch/bin Tests/ConstrainedGenericOperatorDispatch/publish
	rm -rf Tests/ConstrainedGenericOperatorDispatchDiagnostics/*/.void Tests/ConstrainedGenericOperatorDispatchDiagnostics/*/bin Tests/ConstrainedGenericOperatorDispatchDiagnostics/*/publish
	rm -rf Tests/IteratorLifetimeGenericOperatorIntegration/.void Tests/IteratorLifetimeGenericOperatorIntegration/bin Tests/IteratorLifetimeGenericOperatorIntegration/publish
	rm -rf Tests/IteratorLifetimeGenericOperatorIntegrationDiagnostics/*/.void Tests/IteratorLifetimeGenericOperatorIntegrationDiagnostics/*/bin Tests/IteratorLifetimeGenericOperatorIntegrationDiagnostics/*/publish
	rm -rf Tests/StandardConversionClassificationCompletion/.void Tests/StandardConversionClassificationCompletion/bin Tests/StandardConversionClassificationCompletion/publish
	rm -rf Tests/StandardConversionClassificationDiagnostics/*/.void Tests/StandardConversionClassificationDiagnostics/*/bin Tests/StandardConversionClassificationDiagnostics/*/publish
	rm -rf Tests/UserDefinedConversionResolutionCompletion/.void Tests/UserDefinedConversionResolutionCompletion/bin Tests/UserDefinedConversionResolutionCompletion/publish
	rm -rf Tests/UserDefinedConversionResolutionDiagnostics/*/.void Tests/UserDefinedConversionResolutionDiagnostics/*/bin Tests/UserDefinedConversionResolutionDiagnostics/*/publish
	rm -rf Tests/LiftedNullableConversionCompletion/.void Tests/LiftedNullableConversionCompletion/bin Tests/LiftedNullableConversionCompletion/publish
	rm -rf Tests/LiftedNullableConversionDiagnostics/*/.void Tests/LiftedNullableConversionDiagnostics/*/bin Tests/LiftedNullableConversionDiagnostics/*/publish
	rm -rf Tests/LiftedNullableConversionRuntime/*/.void Tests/LiftedNullableConversionRuntime/*/bin Tests/LiftedNullableConversionRuntime/*/publish
	rm -rf Tests/ConversionSiteIntegration/.void Tests/ConversionSiteIntegration/bin Tests/ConversionSiteIntegration/publish
	rm -rf Tests/ConversionSiteIntegrationDiagnostics/*/.void Tests/ConversionSiteIntegrationDiagnostics/*/bin Tests/ConversionSiteIntegrationDiagnostics/*/publish
	rm -rf Tests/StaticInterfaceConversionContracts/.void Tests/StaticInterfaceConversionContracts/bin Tests/StaticInterfaceConversionContracts/publish
	rm -rf Tests/StaticInterfaceConversionContractDiagnostics/*/.void Tests/StaticInterfaceConversionContractDiagnostics/*/bin Tests/StaticInterfaceConversionContractDiagnostics/*/publish
	rm -rf Tests/ConstrainedGenericImplicitConversionDispatch/.void Tests/ConstrainedGenericImplicitConversionDispatch/bin Tests/ConstrainedGenericImplicitConversionDispatch/publish
	rm -rf Tests/ConstrainedGenericImplicitConversionDispatchDiagnostics/*/.void Tests/ConstrainedGenericImplicitConversionDispatchDiagnostics/*/bin Tests/ConstrainedGenericImplicitConversionDispatchDiagnostics/*/publish
	rm -rf Tests/ConstrainedGenericExplicitConversionDispatch/.void Tests/ConstrainedGenericExplicitConversionDispatch/bin Tests/ConstrainedGenericExplicitConversionDispatch/publish
	rm -rf Tests/ConstrainedGenericExplicitConversionDispatchDiagnostics/*/.void Tests/ConstrainedGenericExplicitConversionDispatchDiagnostics/*/bin Tests/ConstrainedGenericExplicitConversionDispatchDiagnostics/*/publish
	rm -rf Tests/GenericConversionProvenanceInferenceIntegration/.void Tests/GenericConversionProvenanceInferenceIntegration/bin Tests/GenericConversionProvenanceInferenceIntegration/publish
	rm -rf Tests/GenericConversionProvenanceInferenceIntegrationDiagnostics/*/.void Tests/GenericConversionProvenanceInferenceIntegrationDiagnostics/*/bin Tests/GenericConversionProvenanceInferenceIntegrationDiagnostics/*/publish
	rm -rf Tests/IteratorClosureGcConversionIntegration/.void Tests/IteratorClosureGcConversionIntegration/bin Tests/IteratorClosureGcConversionIntegration/publish
	rm -rf Tests/IteratorClosureGcConversionIntegrationDiagnostics/*/.void Tests/IteratorClosureGcConversionIntegrationDiagnostics/*/bin Tests/IteratorClosureGcConversionIntegrationDiagnostics/*/publish
	rm -rf Tests/ConversionSemanticsGenericConversionIntegration/.void Tests/ConversionSemanticsGenericConversionIntegration/bin Tests/ConversionSemanticsGenericConversionIntegration/publish
	rm -rf Tests/ConversionSemanticsGenericConversionIntegrationDiagnostics/*/.void Tests/ConversionSemanticsGenericConversionIntegrationDiagnostics/*/bin Tests/ConversionSemanticsGenericConversionIntegrationDiagnostics/*/publish
	rm -rf Tests/NativeThreadRuntimeFoundation/.void Tests/NativeThreadRuntimeFoundation/bin Tests/NativeThreadRuntimeFoundation/publish
	rm -f $(NATIVE_THREAD_RUNTIME_TEST)
	rm -rf Tests/PerThreadRuntimeExecutionContext/.void Tests/PerThreadRuntimeExecutionContext/bin Tests/PerThreadRuntimeExecutionContext/publish
	rm -f $(RUNTIME_CONTEXT_TEST)
	rm -rf Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void Tests/ThreadSafeAllocationRuntimeRegistry/Library/bin Tests/ThreadSafeAllocationRuntimeRegistry/Library/publish
	rm -f $(THREAD_SAFE_ALLOCATION_REGISTRY_TEST)
	rm -rf Tests/CooperativeStopTheWorldGc/Library/.void Tests/CooperativeStopTheWorldGc/Library/bin Tests/CooperativeStopTheWorldGc/Library/publish
	rm -f $(COOPERATIVE_STW_GC_TEST)
	rm -rf Tests/ManagedThreadSurface/.void Tests/ManagedThreadSurface/bin Tests/ManagedThreadSurface/publish
	rm -rf Tests/ManagedThreadSurfaceRuntime/*/.void Tests/ManagedThreadSurfaceRuntime/*/bin Tests/ManagedThreadSurfaceRuntime/*/publish
	rm -rf Tests/ManagedThreadSurfaceDiagnostics/*/.void Tests/ManagedThreadSurfaceDiagnostics/*/bin Tests/ManagedThreadSurfaceDiagnostics/*/publish
	rm -rf Tests/MonitorMutualExclusionFoundation/.void Tests/MonitorMutualExclusionFoundation/bin Tests/MonitorMutualExclusionFoundation/publish
	rm -rf Tests/MonitorMutualExclusionFoundationRuntime/*/.void Tests/MonitorMutualExclusionFoundationRuntime/*/bin Tests/MonitorMutualExclusionFoundationRuntime/*/publish
	rm -rf Tests/MonitorMutualExclusionFoundationDiagnostics/*/.void Tests/MonitorMutualExclusionFoundationDiagnostics/*/bin Tests/MonitorMutualExclusionFoundationDiagnostics/*/publish
	rm -f $(NATIVE_MONITOR_TEST)
	rm -rf Tests/LockStatementCompletion/.void Tests/LockStatementCompletion/bin Tests/LockStatementCompletion/publish
	rm -rf Tests/LockStatementCompletionRuntime/*/.void Tests/LockStatementCompletionRuntime/*/bin Tests/LockStatementCompletionRuntime/*/publish
	rm -rf Tests/LockStatementCompletionDiagnostics/*/.void Tests/LockStatementCompletionDiagnostics/*/bin Tests/LockStatementCompletionDiagnostics/*/publish
	rm -rf Tests/ThreadExceptionNativeBoundaryCleanup/.void Tests/ThreadExceptionNativeBoundaryCleanup/bin Tests/ThreadExceptionNativeBoundaryCleanup/publish
	rm -rf Tests/ThreadExceptionNativeBoundaryCleanupRuntime/*/.void Tests/ThreadExceptionNativeBoundaryCleanupRuntime/*/bin Tests/ThreadExceptionNativeBoundaryCleanupRuntime/*/publish
	rm -rf Tests/ThreadedClosureDelegateGenericGcIntegration/.void Tests/ThreadedClosureDelegateGenericGcIntegration/bin Tests/ThreadedClosureDelegateGenericGcIntegration/publish
	rm -rf Tests/NativeConcurrencySynchronizationIntegrationAudit/.void Tests/NativeConcurrencySynchronizationIntegrationAudit/bin Tests/NativeConcurrencySynchronizationIntegrationAudit/publish
	rm -rf Tests/MonitorConditionWaitRuntimeFoundation/.void Tests/MonitorConditionWaitRuntimeFoundation/bin Tests/MonitorConditionWaitRuntimeFoundation/publish
	rm -f $(MONITOR_CONDITION_WAIT_RUNTIME_TEST)
	rm -rf Tests/ManagedMonitorWaitCompletion/.void Tests/ManagedMonitorWaitCompletion/bin Tests/ManagedMonitorWaitCompletion/publish
	rm -rf Tests/ManagedMonitorWaitCompletionRuntime/*/.void Tests/ManagedMonitorWaitCompletionRuntime/*/bin Tests/ManagedMonitorWaitCompletionRuntime/*/publish
	rm -rf Tests/ManagedMonitorWaitCompletionDiagnostics/*/.void Tests/ManagedMonitorWaitCompletionDiagnostics/*/bin Tests/ManagedMonitorWaitCompletionDiagnostics/*/publish
	rm -rf Tests/MonitorPulseCompletion/.void Tests/MonitorPulseCompletion/bin Tests/MonitorPulseCompletion/publish
	rm -rf Tests/MonitorPulseCompletionRuntime/*/.void Tests/MonitorPulseCompletionRuntime/*/bin Tests/MonitorPulseCompletionRuntime/*/publish
	rm -rf Tests/MonitorPulseCompletionDiagnostics/*/.void Tests/MonitorPulseCompletionDiagnostics/*/bin Tests/MonitorPulseCompletionDiagnostics/*/publish
	rm -rf Tests/PortableAtomicRuntimeFoundation/.void Tests/PortableAtomicRuntimeFoundation/bin Tests/PortableAtomicRuntimeFoundation/publish
	rm -f $(PORTABLE_ATOMIC_RUNTIME_TEST)
	rm -rf Tests/InterlockedIntegerOperations/.void Tests/InterlockedIntegerOperations/bin Tests/InterlockedIntegerOperations/publish
	rm -rf Tests/InterlockedIntegerOperationsDiagnostics/*/.void Tests/InterlockedIntegerOperationsDiagnostics/*/bin Tests/InterlockedIntegerOperationsDiagnostics/*/publish
	rm -rf Tests/InterlockedManagedReferenceOperations/.void Tests/InterlockedManagedReferenceOperations/bin Tests/InterlockedManagedReferenceOperations/publish
	rm -rf Tests/InterlockedManagedReferenceOperationsDiagnostics/*/.void Tests/InterlockedManagedReferenceOperationsDiagnostics/*/bin Tests/InterlockedManagedReferenceOperationsDiagnostics/*/publish
	rm -rf Tests/VolatileMemoryOrdering/.void Tests/VolatileMemoryOrdering/bin Tests/VolatileMemoryOrdering/publish
	rm -rf Tests/VolatileMemoryOrderingDiagnostics/*/.void Tests/VolatileMemoryOrderingDiagnostics/*/bin Tests/VolatileMemoryOrderingDiagnostics/*/publish
	rm -rf Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/.void Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/bin Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/publish
	rm -f $(SYNC_EXCEPTION_NATIVE_OBJECT) $(SYNC_EXCEPTION_NATIVE_FIXTURE)
	rm -f $(TIMED_CANCELLABLE_NATIVE_OBJECT) $(TIMED_CANCELLABLE_NATIVE_FIXTURE)
	rm -rf Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/bin Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/publish
	rm -rf Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void Tests/AdvancedSynchronizationAtomicIntegrationAudit/bin Tests/AdvancedSynchronizationAtomicIntegrationAudit/publish
	rm -rf Tests/MonotonicTimeTimedWaitRuntimeFoundation/.void Tests/MonotonicTimeTimedWaitRuntimeFoundation/bin Tests/MonotonicTimeTimedWaitRuntimeFoundation/publish
	rm -f $(MONOTONIC_TIMED_WAIT_RUNTIME_TEST)
	rm -rf Tests/ManagedThreadSleepTimeoutContract/.void Tests/ManagedThreadSleepTimeoutContract/bin Tests/ManagedThreadSleepTimeoutContract/publish
	rm -rf Tests/ManagedThreadSleepTimeoutContractRuntime/*/.void Tests/ManagedThreadSleepTimeoutContractRuntime/*/bin Tests/ManagedThreadSleepTimeoutContractRuntime/*/publish
	rm -rf Tests/ManagedThreadSleepTimeoutContractDiagnostics/*/.void Tests/ManagedThreadSleepTimeoutContractDiagnostics/*/bin Tests/ManagedThreadSleepTimeoutContractDiagnostics/*/publish
	rm -rf Tests/TimedMonitorWaitCompletion/.void Tests/TimedMonitorWaitCompletion/bin Tests/TimedMonitorWaitCompletion/publish
	rm -rf Tests/TimedMonitorWaitCompletionRuntime/*/.void Tests/TimedMonitorWaitCompletionRuntime/*/bin Tests/TimedMonitorWaitCompletionRuntime/*/publish
	rm -rf Tests/TimedMonitorWaitCompletionDiagnostics/*/.void Tests/TimedMonitorWaitCompletionDiagnostics/*/bin Tests/TimedMonitorWaitCompletionDiagnostics/*/publish
	rm -rf Tests/ManualResetEventCompletion/.void Tests/ManualResetEventCompletion/bin Tests/ManualResetEventCompletion/publish
	rm -rf Tests/ManualResetEventCompletionRuntime/*/.void Tests/ManualResetEventCompletionRuntime/*/bin Tests/ManualResetEventCompletionRuntime/*/publish
	rm -rf Tests/AutoResetEventCompletion/.void Tests/AutoResetEventCompletion/bin Tests/AutoResetEventCompletion/publish
	rm -rf Tests/AutoResetEventCompletionRuntime/*/.void Tests/AutoResetEventCompletionRuntime/*/bin Tests/AutoResetEventCompletionRuntime/*/publish
	rm -rf Tests/SemaphoreCompletion/.void Tests/SemaphoreCompletion/bin Tests/SemaphoreCompletion/publish
	rm -rf Tests/SemaphoreCompletionRuntime/*/.void Tests/SemaphoreCompletionRuntime/*/bin Tests/SemaphoreCompletionRuntime/*/publish
	rm -rf Tests/CancellationTokenSourceFoundation/.void Tests/CancellationTokenSourceFoundation/bin Tests/CancellationTokenSourceFoundation/publish
	rm -rf Tests/CancellationTokenSourceRuntime/*/.void Tests/CancellationTokenSourceRuntime/*/bin Tests/CancellationTokenSourceRuntime/*/publish
	rm -rf Tests/CancellationRegistrationWakeupCompletion/.void Tests/CancellationRegistrationWakeupCompletion/bin Tests/CancellationRegistrationWakeupCompletion/publish
	rm -rf Tests/CancellationRegistrationWakeupRuntime/*/.void Tests/CancellationRegistrationWakeupRuntime/*/bin Tests/CancellationRegistrationWakeupRuntime/*/publish
	rm -rf Tests/TimedCancellableSynchronizationIntegration/.void Tests/TimedCancellableSynchronizationIntegration/bin Tests/TimedCancellableSynchronizationIntegration/publish
	rm -rf Tests/TimedCancellableSynchronizationIntegrationDiagnostics/*/.void Tests/TimedCancellableSynchronizationIntegrationDiagnostics/*/bin Tests/TimedCancellableSynchronizationIntegrationDiagnostics/*/publish
	rm -rf Tests/TimedSynchronizationCancellationIntegrationAudit/.void Tests/TimedSynchronizationCancellationIntegrationAudit/bin Tests/TimedSynchronizationCancellationIntegrationAudit/publish
	rm -rf Tests/TimedSynchronizationCancellationIntegrationAuditDiagnostics/*/.void Tests/TimedSynchronizationCancellationIntegrationAuditDiagnostics/*/bin Tests/TimedSynchronizationCancellationIntegrationAuditDiagnostics/*/publish
	rm -rf Tests/TimedSynchronizationCancellationRaceStress/.void Tests/TimedSynchronizationCancellationRaceStress/bin Tests/TimedSynchronizationCancellationRaceStress/publish
	rm -rf Tests/ManagedThreadPoolWorkQueueFoundation/.void Tests/ManagedThreadPoolWorkQueueFoundation/bin Tests/ManagedThreadPoolWorkQueueFoundation/publish
	rm -rf Tests/ManagedThreadPoolWorkQueueRuntime/*/.void Tests/ManagedThreadPoolWorkQueueRuntime/*/bin Tests/ManagedThreadPoolWorkQueueRuntime/*/publish
	rm -rf Tests/ThreadPoolWorkerLifecycleGcCoordination/.void Tests/ThreadPoolWorkerLifecycleGcCoordination/bin Tests/ThreadPoolWorkerLifecycleGcCoordination/publish
	rm -rf Tests/ThreadPoolExceptionShutdownCompletion/.void Tests/ThreadPoolExceptionShutdownCompletion/bin Tests/ThreadPoolExceptionShutdownCompletion/publish
	rm -rf Tests/ThreadPoolExceptionShutdownRuntime/*/.void Tests/ThreadPoolExceptionShutdownRuntime/*/bin Tests/ThreadPoolExceptionShutdownRuntime/*/publish
	rm -rf Tests/ThreadPoolExceptionShutdownStress/.void Tests/ThreadPoolExceptionShutdownStress/bin Tests/ThreadPoolExceptionShutdownStress/publish
	rm -rf Tests/TaskStateCompletionFoundation/.void Tests/TaskStateCompletionFoundation/bin Tests/TaskStateCompletionFoundation/publish
	rm -rf Tests/TaskStateCompletionRuntime/*/.void Tests/TaskStateCompletionRuntime/*/bin Tests/TaskStateCompletionRuntime/*/publish
	rm -rf Tests/GenericTaskResultGcCompletion/.void Tests/GenericTaskResultGcCompletion/bin Tests/GenericTaskResultGcCompletion/publish
	rm -rf Tests/GenericTaskResultRuntime/*/.void Tests/GenericTaskResultRuntime/*/bin Tests/GenericTaskResultRuntime/*/publish
	rm -rf Tests/TaskRunThreadPoolSchedulingIntegration/.void Tests/TaskRunThreadPoolSchedulingIntegration/bin Tests/TaskRunThreadPoolSchedulingIntegration/publish
	rm -rf Tests/TaskRunThreadPoolSchedulingRuntime/*/.void Tests/TaskRunThreadPoolSchedulingRuntime/*/bin Tests/TaskRunThreadPoolSchedulingRuntime/*/publish
	rm -rf Tests/TaskWaitingTimeoutFaultPropagation/.void Tests/TaskWaitingTimeoutFaultPropagation/bin Tests/TaskWaitingTimeoutFaultPropagation/publish
	rm -rf Tests/TaskWaitingTimeoutFaultPropagationRuntime/*/.void Tests/TaskWaitingTimeoutFaultPropagationRuntime/*/bin Tests/TaskWaitingTimeoutFaultPropagationRuntime/*/publish
	rm -rf Tests/TaskCancellationIntegration/.void Tests/TaskCancellationIntegration/bin Tests/TaskCancellationIntegration/publish
	rm -rf Tests/TaskCancellationRuntime/*/.void Tests/TaskCancellationRuntime/*/bin Tests/TaskCancellationRuntime/*/publish
	rm -rf Tests/TaskContinuationCompletionSourceFoundation/.void Tests/TaskContinuationCompletionSourceFoundation/bin Tests/TaskContinuationCompletionSourceFoundation/publish
	rm -rf Tests/TaskContinuationCompletionSourceRuntime/*/.void Tests/TaskContinuationCompletionSourceRuntime/*/bin Tests/TaskContinuationCompletionSourceRuntime/*/publish
	rm -rf Tests/ThreadPoolTaskIntegrationAudit/.void Tests/ThreadPoolTaskIntegrationAudit/bin Tests/ThreadPoolTaskIntegrationAudit/publish
	rm -rf Tests/ThreadPoolTaskIntegrationAuditRuntime/*/.void Tests/ThreadPoolTaskIntegrationAuditRuntime/*/bin Tests/ThreadPoolTaskIntegrationAuditRuntime/*/publish
	rm -rf Tests/AsyncAwaitSyntaxSemanticFoundation/.void Tests/AsyncAwaitSyntaxSemanticFoundation/bin Tests/AsyncAwaitSyntaxSemanticFoundation/publish
	rm -rf Tests/AsyncAwaitSyntaxSemanticDiagnostics/*/.void Tests/AsyncAwaitSyntaxSemanticDiagnostics/*/bin Tests/AsyncAwaitSyntaxSemanticDiagnostics/*/publish
	rm -f $(AWAITER_PROTOCOL_SEMANTIC_TEST)
	rm -rf Tests/AwaiterProtocolSemanticFoundationDiagnostics/*/.void Tests/AwaiterProtocolSemanticFoundationDiagnostics/*/bin Tests/AwaiterProtocolSemanticFoundationDiagnostics/*/publish
	rm -rf Tests/GeneralAwaiterStateMachineLowering/.void Tests/GeneralAwaiterStateMachineLowering/bin Tests/GeneralAwaiterStateMachineLowering/publish
	rm -rf Tests/TaskAwaiterTaskDeSpecialization/.void Tests/TaskAwaiterTaskDeSpecialization/bin Tests/TaskAwaiterTaskDeSpecialization/publish
	rm -rf Tests/GenericStructExtensionAwaiterCompletion/.void Tests/GenericStructExtensionAwaiterCompletion/bin Tests/GenericStructExtensionAwaiterCompletion/publish
	rm -rf Tests/GenericStructExtensionAwaiterCompletionDiagnostics/*/.void Tests/GenericStructExtensionAwaiterCompletionDiagnostics/*/bin Tests/GenericStructExtensionAwaiterCompletionDiagnostics/*/publish
	rm -rf Tests/TaskYieldAllocationFreeYieldAwaitable/.void Tests/TaskYieldAllocationFreeYieldAwaitable/bin Tests/TaskYieldAllocationFreeYieldAwaitable/publish
	rm -rf Tests/TaskYieldAllocationFreeYieldAwaitableRuntime/*/.void Tests/TaskYieldAllocationFreeYieldAwaitableRuntime/*/bin Tests/TaskYieldAllocationFreeYieldAwaitableRuntime/*/publish
	rm -rf Tests/ValueTaskFoundation/.void Tests/ValueTaskFoundation/bin Tests/ValueTaskFoundation/publish
	rm -rf Tests/ValueTaskFoundationRuntime/*/.void Tests/ValueTaskFoundationRuntime/*/bin Tests/ValueTaskFoundationRuntime/*/publish
	rm -rf Tests/GenericValueTaskResultGcCompletion/.void Tests/GenericValueTaskResultGcCompletion/bin Tests/GenericValueTaskResultGcCompletion/publish
	rm -rf Tests/GenericValueTaskResultGcCompletionRuntime/*/.void Tests/GenericValueTaskResultGcCompletionRuntime/*/bin Tests/GenericValueTaskResultGcCompletionRuntime/*/publish
	rm -rf Tests/AsyncValueTaskReturnIntegration/.void Tests/AsyncValueTaskReturnIntegration/bin Tests/AsyncValueTaskReturnIntegration/publish
	rm -rf Tests/AsyncValueTaskReturnIntegrationDiagnostics/*/.void Tests/AsyncValueTaskReturnIntegrationDiagnostics/*/bin Tests/AsyncValueTaskReturnIntegrationDiagnostics/*/publish
	rm -rf Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/.void Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/bin Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/publish
	rm -rf Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/Library/.void Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/Library/bin Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/Library/publish
	rm -rf Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegrationDiagnostics/*/.void Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegrationDiagnostics/*/bin Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegrationDiagnostics/*/publish
	rm -f Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/CConsumer/value-task-library-consumer
	rm -rf Tests/AsyncTaskStateMachineLoweringFoundation/.void Tests/AsyncTaskStateMachineLoweringFoundation/bin Tests/AsyncTaskStateMachineLoweringFoundation/publish
	rm -rf Tests/AwaitSuspensionResumptionCompletion/.void Tests/AwaitSuspensionResumptionCompletion/bin Tests/AwaitSuspensionResumptionCompletion/publish
	rm -rf Tests/AsyncTaskTypedAwaitResults/.void Tests/AsyncTaskTypedAwaitResults/bin Tests/AsyncTaskTypedAwaitResults/publish
	rm -rf Tests/AsyncMultipleAwaitStructuredControlFlow/.void Tests/AsyncMultipleAwaitStructuredControlFlow/bin Tests/AsyncMultipleAwaitStructuredControlFlow/publish
	rm -rf Tests/AsyncMultipleAwaitStructuredControlFlowDiagnostics/*/.void Tests/AsyncMultipleAwaitStructuredControlFlowDiagnostics/*/bin Tests/AsyncMultipleAwaitStructuredControlFlowDiagnostics/*/publish
	rm -rf Tests/AsyncExceptionCatchFinallyIntegration/.void Tests/AsyncExceptionCatchFinallyIntegration/bin Tests/AsyncExceptionCatchFinallyIntegration/publish
	rm -rf Tests/AsyncExceptionCatchFinallyDiagnostics/*/.void Tests/AsyncExceptionCatchFinallyDiagnostics/*/bin Tests/AsyncExceptionCatchFinallyDiagnostics/*/publish
	rm -rf Tests/AsyncCancellationIntegration/.void Tests/AsyncCancellationIntegration/bin Tests/AsyncCancellationIntegration/publish
	rm -rf Tests/AsyncGenericsClosuresGcLifetimeIntegration/.void Tests/AsyncGenericsClosuresGcLifetimeIntegration/bin Tests/AsyncGenericsClosuresGcLifetimeIntegration/publish
	rm -rf Tests/AsyncObjectModelLibraryBoundaryIntegration/Runtime/.void Tests/AsyncObjectModelLibraryBoundaryIntegration/Runtime/bin Tests/AsyncObjectModelLibraryBoundaryIntegration/Runtime/publish
	rm -rf Tests/AsyncObjectModelLibraryBoundaryIntegration/Library/.void Tests/AsyncObjectModelLibraryBoundaryIntegration/Library/bin Tests/AsyncObjectModelLibraryBoundaryIntegration/Library/publish
	rm -rf Tests/AsyncObjectModelLibraryBoundaryIntegration/Consumer/.void Tests/AsyncObjectModelLibraryBoundaryIntegration/Consumer/bin Tests/AsyncObjectModelLibraryBoundaryIntegration/Consumer/publish
	rm -f Tests/AsyncObjectModelLibraryBoundaryIntegration/Consumer/native/libVoid209AsyncLib.a Tests/AsyncObjectModelLibraryBoundaryIntegration/CConsumer/async-library-consumer
	rm -rf Tests/AsyncObjectModelLibraryBoundaryDiagnostics/*/.void Tests/AsyncObjectModelLibraryBoundaryDiagnostics/*/bin Tests/AsyncObjectModelLibraryBoundaryDiagnostics/*/publish
	rm -f $(ASYNC_OBJECT_MODEL_NATIVE_OBJECT) $(ASYNC_OBJECT_MODEL_NATIVE_FIXTURE)
	rm -rf Tests/AsyncAwaitTaskRuntimeIntegrationAudit/.void Tests/AsyncAwaitTaskRuntimeIntegrationAudit/bin Tests/AsyncAwaitTaskRuntimeIntegrationAudit/publish
	rm -rf Tests/AsyncAwaitTaskRuntimeIntegrationAuditRuntime/*/.void Tests/AsyncAwaitTaskRuntimeIntegrationAuditRuntime/*/bin Tests/AsyncAwaitTaskRuntimeIntegrationAuditRuntime/*/publish
	rm -rf Tests/GeneralAwaitExpressionLoweringFoundation/.void Tests/GeneralAwaitExpressionLoweringFoundation/bin Tests/GeneralAwaitExpressionLoweringFoundation/publish
	rm -rf Tests/GeneralAwaitExpressionLoweringDiagnostics/*/.void Tests/GeneralAwaitExpressionLoweringDiagnostics/*/bin Tests/GeneralAwaitExpressionLoweringDiagnostics/*/publish
	rm -rf Tests/AwaitedConditionsExpressionEvaluationCompletion/.void Tests/AwaitedConditionsExpressionEvaluationCompletion/bin Tests/AwaitedConditionsExpressionEvaluationCompletion/publish
	rm -rf Tests/AwaitedConditionsExpressionEvaluationDiagnostics/*/.void Tests/AwaitedConditionsExpressionEvaluationDiagnostics/*/bin Tests/AwaitedConditionsExpressionEvaluationDiagnostics/*/publish
	rm -rf Tests/SynchronousForeachSuspensionCleanupCompletion/.void Tests/SynchronousForeachSuspensionCleanupCompletion/bin Tests/SynchronousForeachSuspensionCleanupCompletion/publish
	rm -rf Tests/SynchronousForeachSuspensionCleanupDiagnostics/*/.void Tests/SynchronousForeachSuspensionCleanupDiagnostics/*/bin Tests/SynchronousForeachSuspensionCleanupDiagnostics/*/publish
	rm -rf Tests/AwaitedSwitchExpressionPatternGuardCompletion/.void Tests/AwaitedSwitchExpressionPatternGuardCompletion/bin Tests/AwaitedSwitchExpressionPatternGuardCompletion/publish
	rm -rf Tests/AwaitedSwitchExpressionPatternGuardDiagnostics/*/.void Tests/AwaitedSwitchExpressionPatternGuardDiagnostics/*/bin Tests/AwaitedSwitchExpressionPatternGuardDiagnostics/*/publish
	rm -rf Tests/AsyncEnumerationProtocolFoundation/.void Tests/AsyncEnumerationProtocolFoundation/bin Tests/AsyncEnumerationProtocolFoundation/publish
	rm -rf Tests/AsyncEnumerationProtocolDiagnostics/*/.void Tests/AsyncEnumerationProtocolDiagnostics/*/bin Tests/AsyncEnumerationProtocolDiagnostics/*/publish
	rm -rf Tests/AwaitForeachSyntaxLoweringCompletion/.void Tests/AwaitForeachSyntaxLoweringCompletion/bin Tests/AwaitForeachSyntaxLoweringCompletion/publish
	rm -rf Tests/AwaitForeachSyntaxLoweringDiagnostics/*/.void Tests/AwaitForeachSyntaxLoweringDiagnostics/*/bin Tests/AwaitForeachSyntaxLoweringDiagnostics/*/publish
	rm -rf Tests/AsyncIteratorMethodFoundation/.void Tests/AsyncIteratorMethodFoundation/bin Tests/AsyncIteratorMethodFoundation/publish
	rm -rf Tests/AsyncIteratorMethodDiagnostics/*/.void Tests/AsyncIteratorMethodDiagnostics/*/bin Tests/AsyncIteratorMethodDiagnostics/*/publish
	rm -rf Tests/AsyncIteratorCompositionCompletion/.void Tests/AsyncIteratorCompositionCompletion/bin Tests/AsyncIteratorCompositionCompletion/publish
	rm -rf Tests/AsyncIteratorCompositionDiagnostics/*/.void Tests/AsyncIteratorCompositionDiagnostics/*/bin Tests/AsyncIteratorCompositionDiagnostics/*/publish
	rm -rf Tests/AsyncIteratorCancellationIntegration/.void Tests/AsyncIteratorCancellationIntegration/bin Tests/AsyncIteratorCancellationIntegration/publish
	rm -rf Tests/AsyncIteratorCancellationDiagnostics/*/.void Tests/AsyncIteratorCancellationDiagnostics/*/bin Tests/AsyncIteratorCancellationDiagnostics/*/publish
	rm -rf Tests/AsyncLanguageAsynchronousIterationIntegrationAudit/.void Tests/AsyncLanguageAsynchronousIterationIntegrationAudit/bin Tests/AsyncLanguageAsynchronousIterationIntegrationAudit/publish
	rm -rf Tests/AsyncLanguageAsynchronousIterationIntegrationDiagnostics/*/.void Tests/AsyncLanguageAsynchronousIterationIntegrationDiagnostics/*/bin Tests/AsyncLanguageAsynchronousIterationIntegrationDiagnostics/*/publish
	rm -rf Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Library/.void Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Library/bin Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Library/publish
	rm -rf Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Consumer/.void Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Consumer/bin Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Consumer/publish
	rm -f Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Consumer/native/libVoid220AsyncStreamLib.a Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/CConsumer/async-stream-consumer
	rm -rf Tests/ByReferenceValueRefLocalFoundation/.void Tests/ByReferenceValueRefLocalFoundation/bin Tests/ByReferenceValueRefLocalFoundation/publish
	rm -rf Tests/ByReferenceValueRefLocalDiagnostics/*/.void Tests/ByReferenceValueRefLocalDiagnostics/*/bin Tests/ByReferenceValueRefLocalDiagnostics/*/publish
	rm -rf Tests/RefAssignmentAliasingCompletion/.void Tests/RefAssignmentAliasingCompletion/bin Tests/RefAssignmentAliasingCompletion/publish
	rm -rf Tests/RefAssignmentAliasingDiagnostics/*/.void Tests/RefAssignmentAliasingDiagnostics/*/bin Tests/RefAssignmentAliasingDiagnostics/*/publish
	rm -rf Tests/RefReturnMethodCompletion/.void Tests/RefReturnMethodCompletion/bin Tests/RefReturnMethodCompletion/publish
	rm -rf Tests/RefReturnMethodDiagnostics/*/.void Tests/RefReturnMethodDiagnostics/*/bin Tests/RefReturnMethodDiagnostics/*/publish
	rm -rf Tests/RefReturningPropertiesIndexersCompletion/.void Tests/RefReturningPropertiesIndexersCompletion/bin Tests/RefReturningPropertiesIndexersCompletion/publish
	rm -rf Tests/RefReturningPropertiesIndexersDiagnostics/*/.void Tests/RefReturningPropertiesIndexersDiagnostics/*/bin Tests/RefReturningPropertiesIndexersDiagnostics/*/publish
	rm -rf Tests/RefReadonlyScopedEscapeCompletion/.void Tests/RefReadonlyScopedEscapeCompletion/bin Tests/RefReadonlyScopedEscapeCompletion/publish
	rm -rf Tests/RefReadonlyScopedEscapeDiagnostics/*/.void Tests/RefReadonlyScopedEscapeDiagnostics/*/bin Tests/RefReadonlyScopedEscapeDiagnostics/*/publish
	rm -rf Tests/RefStructFoundation/.void Tests/RefStructFoundation/bin Tests/RefStructFoundation/publish
	rm -rf Tests/RefStructFoundationDiagnostics/*/.void Tests/RefStructFoundationDiagnostics/*/bin Tests/RefStructFoundationDiagnostics/*/publish
	rm -rf Tests/RefStructEscapeCompletion/.void Tests/RefStructEscapeCompletion/bin Tests/RefStructEscapeCompletion/publish
	rm -rf Tests/RefStructEscapeDiagnostics/*/.void Tests/RefStructEscapeDiagnostics/*/bin Tests/RefStructEscapeDiagnostics/*/publish
	rm -rf Tests/SpanFoundation/.void Tests/SpanFoundation/bin Tests/SpanFoundation/publish
	rm -rf Tests/SpanFoundationDiagnostics/*/.void Tests/SpanFoundationDiagnostics/*/bin Tests/SpanFoundationDiagnostics/*/publish
	rm -rf Tests/SpanFoundationRuntime/*/.void Tests/SpanFoundationRuntime/*/bin Tests/SpanFoundationRuntime/*/publish
	rm -rf Tests/SpanConversionSlicingStackallocCompletion/.void Tests/SpanConversionSlicingStackallocCompletion/bin Tests/SpanConversionSlicingStackallocCompletion/publish
	rm -rf Tests/StrictGeneratedCCleanlinessCompletion/.void Tests/StrictGeneratedCCleanlinessCompletion/bin Tests/StrictGeneratedCCleanlinessCompletion/publish
	rm -rf Tests/SpanConversionSlicingStackallocDiagnostics/*/.void Tests/SpanConversionSlicingStackallocDiagnostics/*/bin Tests/SpanConversionSlicingStackallocDiagnostics/*/publish
	rm -rf Tests/SpanConversionSlicingStackallocRuntime/*/.void Tests/SpanConversionSlicingStackallocRuntime/*/bin Tests/SpanConversionSlicingStackallocRuntime/*/publish
	rm -rf Tests/ByReferenceSpanIntegrationAudit/.void Tests/ByReferenceSpanIntegrationAudit/bin Tests/ByReferenceSpanIntegrationAudit/publish
	rm -rf Tests/ByReferenceSpanIntegrationAuditDiagnostics/*/.void Tests/ByReferenceSpanIntegrationAuditDiagnostics/*/bin Tests/ByReferenceSpanIntegrationAuditDiagnostics/*/publish
	rm -rf Tests/IndexFromEndFoundation/.void Tests/IndexFromEndFoundation/bin Tests/IndexFromEndFoundation/publish
	rm -rf Tests/IndexFromEndNoUsing/.void Tests/IndexFromEndNoUsing/bin Tests/IndexFromEndNoUsing/publish
	rm -rf Tests/IndexFromEndDiagnostics/*/.void Tests/IndexFromEndDiagnostics/*/bin Tests/IndexFromEndDiagnostics/*/publish
	rm -rf Tests/IndexFromEndRuntime/*/.void Tests/IndexFromEndRuntime/*/bin Tests/IndexFromEndRuntime/*/publish
	rm -rf Tests/RangeSyntaxValueFoundation/.void Tests/RangeSyntaxValueFoundation/bin Tests/RangeSyntaxValueFoundation/publish
	rm -rf Tests/RangeSyntaxNoUsing/.void Tests/RangeSyntaxNoUsing/bin Tests/RangeSyntaxNoUsing/publish
	rm -rf Tests/RangeSyntaxDiagnostics/*/.void Tests/RangeSyntaxDiagnostics/*/bin Tests/RangeSyntaxDiagnostics/*/publish
	rm -rf Tests/RangeSyntaxRuntime/*/.void Tests/RangeSyntaxRuntime/*/bin Tests/RangeSyntaxRuntime/*/publish
	rm -rf Tests/GeneralIndexRangeConsumerSemantics/.void Tests/GeneralIndexRangeConsumerSemantics/bin Tests/GeneralIndexRangeConsumerSemantics/publish
	rm -rf Tests/GeneralIndexRangeConsumerSemanticsDiagnostics/*/.void Tests/GeneralIndexRangeConsumerSemanticsDiagnostics/*/bin Tests/GeneralIndexRangeConsumerSemanticsDiagnostics/*/publish
	rm -rf Tests/ArraysSpanIndexRangeCompletion/.void Tests/ArraysSpanIndexRangeCompletion/bin Tests/ArraysSpanIndexRangeCompletion/publish
	rm -rf Tests/ArraysSpanIndexRangeNoUsing/.void Tests/ArraysSpanIndexRangeNoUsing/bin Tests/ArraysSpanIndexRangeNoUsing/publish
	rm -rf Tests/ArraysSpanIndexRangeDiagnostics/*/.void Tests/ArraysSpanIndexRangeDiagnostics/*/bin Tests/ArraysSpanIndexRangeDiagnostics/*/publish
	rm -rf Tests/ArraysSpanIndexRangeRuntime/*/.void Tests/ArraysSpanIndexRangeRuntime/*/bin Tests/ArraysSpanIndexRangeRuntime/*/publish
	rm -rf Tests/ManagedArrayBackedMemoryFoundation/.void Tests/ManagedArrayBackedMemoryFoundation/bin Tests/ManagedArrayBackedMemoryFoundation/publish
	rm -rf Tests/ManagedArrayBackedMemoryFoundationDiagnostics/*/.void Tests/ManagedArrayBackedMemoryFoundationDiagnostics/*/bin Tests/ManagedArrayBackedMemoryFoundationDiagnostics/*/publish
	rm -rf Tests/ManagedArrayBackedMemoryFoundationRuntime/*/.void Tests/ManagedArrayBackedMemoryFoundationRuntime/*/bin Tests/ManagedArrayBackedMemoryFoundationRuntime/*/publish
	rm -rf Tests/MemorySlicingConversionSpanBridge/.void Tests/MemorySlicingConversionSpanBridge/bin Tests/MemorySlicingConversionSpanBridge/publish
	rm -rf Tests/MemorySlicingConversionSpanBridgeDiagnostics/*/.void Tests/MemorySlicingConversionSpanBridgeDiagnostics/*/bin Tests/MemorySlicingConversionSpanBridgeDiagnostics/*/publish
	rm -rf Tests/MemorySlicingConversionSpanBridgeRuntime/*/.void Tests/MemorySlicingConversionSpanBridgeRuntime/*/bin Tests/MemorySlicingConversionSpanBridgeRuntime/*/publish
	rm -rf Tests/MemorySuspensionGcIntegration/.void Tests/MemorySuspensionGcIntegration/bin Tests/MemorySuspensionGcIntegration/publish
	rm -rf Tests/MemorySuspensionGcIntegrationDiagnostics/*/.void Tests/MemorySuspensionGcIntegrationDiagnostics/*/bin Tests/MemorySuspensionGcIntegrationDiagnostics/*/publish
	rm -rf Tests/SpanEnumerationRefForeach/.void Tests/SpanEnumerationRefForeach/bin Tests/SpanEnumerationRefForeach/publish
	rm -rf Tests/SpanEnumerationRefForeachDiagnostics/*/.void Tests/SpanEnumerationRefForeachDiagnostics/*/bin Tests/SpanEnumerationRefForeachDiagnostics/*/publish
	rm -rf Tests/ContiguousMemoryOperationsCompletion/.void Tests/ContiguousMemoryOperationsCompletion/bin Tests/ContiguousMemoryOperationsCompletion/publish
	rm -rf Tests/ContiguousMemoryOperationsDiagnostics/*/.void Tests/ContiguousMemoryOperationsDiagnostics/*/bin Tests/ContiguousMemoryOperationsDiagnostics/*/publish
	rm -rf Tests/ContiguousMemoryOperationsRuntime/*/.void Tests/ContiguousMemoryOperationsRuntime/*/bin Tests/ContiguousMemoryOperationsRuntime/*/publish
	rm -rf Tests/IndexRangeContiguousMemoryIntegrationAudit/.void Tests/IndexRangeContiguousMemoryIntegrationAudit/bin Tests/IndexRangeContiguousMemoryIntegrationAudit/publish
	rm -rf Tests/IndexRangeContiguousMemoryIntegrationAuditDiagnostics/*/.void Tests/IndexRangeContiguousMemoryIntegrationAuditDiagnostics/*/bin Tests/IndexRangeContiguousMemoryIntegrationAuditDiagnostics/*/publish
	rm -rf Tests/AsyncAwaitTaskRuntimeIntegrationStress/.void Tests/AsyncAwaitTaskRuntimeIntegrationStress/bin Tests/AsyncAwaitTaskRuntimeIntegrationStress/publish
	rm -f $(TASK_RUN_NATIVE_OBJECT) $(TASK_RUN_NATIVE_FIXTURE)
	rm -f $(BORROWED_NUL_NATIVE_OBJECT) $(BORROWED_NUL_NATIVE_FIXTURE)
	rm -f $(POINTER_LENGTH_NATIVE_OBJECT) $(POINTER_LENGTH_NATIVE_FIXTURE)
	rm -f $(STRING_RETURN_NATIVE_OBJECT) $(STRING_RETURN_NATIVE_FIXTURE)
	rm -f $(STRING_RETURN_INVALID_NATIVE_OBJECT) $(STRING_RETURN_INVALID_NATIVE_FIXTURE)
	rm -f $(OWNED_STRING_RETURN_NATIVE_OBJECT) $(OWNED_STRING_RETURN_NATIVE_FIXTURE)
	rm -f $(NATIVE_STRING_BOUNDARY_NATIVE_OBJECT) $(NATIVE_STRING_BOUNDARY_NATIVE_FIXTURE)
	rm -f $(MANAGED_STRING_AUDIT_NATIVE_OBJECT) $(MANAGED_STRING_AUDIT_NATIVE_FIXTURE) $(MANAGED_STRING_AUDIT_C_CONSUMER)
	rm -rf Tests/ManagedStringsNativeStringAbiIntegrationAudit/.void Tests/ManagedStringsNativeStringAbiIntegrationAudit/bin Tests/ManagedStringsNativeStringAbiIntegrationAudit/publish
	rm -rf Tests/ManagedStringsNativeStringAbiIntegrationAudit/Library/.void Tests/ManagedStringsNativeStringAbiIntegrationAudit/Library/bin Tests/ManagedStringsNativeStringAbiIntegrationAudit/Library/publish
	rm -rf Tests/NativeStringLifetimeCallbackBoundaryIntegration/.void Tests/NativeStringLifetimeCallbackBoundaryIntegration/bin Tests/NativeStringLifetimeCallbackBoundaryIntegration/publish
	rm -rf Tests/NativeStringLifetimeCallbackBoundaryIntegrationDiagnostics/*/.void Tests/NativeStringLifetimeCallbackBoundaryIntegrationDiagnostics/*/bin Tests/NativeStringLifetimeCallbackBoundaryIntegrationDiagnostics/*/publish
	rm -rf Tests/VoidStringExportAbi/Library/.void Tests/VoidStringExportAbi/Library/bin Tests/VoidStringExportAbi/Library/publish
	rm -f Tests/VoidStringExportAbi/CConsumer/consumer Tests/VoidStringExportAbi/CConsumer/invalid-view
	rm -rf Tests/VoidStringExportAbiDiagnostics/*/.void Tests/VoidStringExportAbiDiagnostics/*/bin Tests/VoidStringExportAbiDiagnostics/*/publish
	rm -f $(TIMED_SYNC_AUDIT_NATIVE_OBJECT) $(TIMED_SYNC_AUDIT_NATIVE_FIXTURE)
	rm -rf Tests/StaticInterfaceOperatorContractDiagnostics/*/.void Tests/StaticInterfaceOperatorContractDiagnostics/*/bin Tests/StaticInterfaceOperatorContractDiagnostics/*/publish
	rm -rf Tests/SwitchExpressionDiagnostics/*/.void Tests/SwitchExpressionDiagnostics/*/bin Tests/SwitchExpressionDiagnostics/*/publish
	rm -rf Tests/RelationalPatternDiagnostics/*/.void Tests/RelationalPatternDiagnostics/*/bin Tests/RelationalPatternDiagnostics/*/publish
	rm -rf Tests/ExtensionMethodDiagnostics/*/.void Tests/ExtensionMethodDiagnostics/*/bin Tests/ExtensionMethodDiagnostics/*/publish
	rm -rf Tests/NativeCallbackByRef/.void Tests/NativeCallbackByRef/bin Tests/NativeCallbackByRef/publish
	rm -rf Tests/NativeCallbackByRefDiagnostics/*/.void Tests/NativeCallbackByRefDiagnostics/*/bin Tests/NativeCallbackByRefDiagnostics/*/publish
	rm -rf Tests/LibraryOutput/Library/.void Tests/LibraryOutput/Library/bin Tests/LibraryOutput/Library/publish
	rm -rf Tests/LibraryOutput/Consumer/.void Tests/LibraryOutput/Consumer/bin Tests/LibraryOutput/Consumer/publish
	rm -f Tests/LibraryOutput/Consumer/native/libVoid099Lib.a Tests/LibraryOutput/CConsumer/library-consumer
	rm -rf Tests/LibraryOutputDiagnostics/*/.void Tests/LibraryOutputDiagnostics/*/bin Tests/LibraryOutputDiagnostics/*/publish
	rm -rf Tests/LanguageEdgeProjectCompletionIntegration/Library/.void Tests/LanguageEdgeProjectCompletionIntegration/Library/bin Tests/LanguageEdgeProjectCompletionIntegration/Library/publish
	rm -rf Tests/LanguageEdgeProjectCompletionIntegration/Consumer/.void Tests/LanguageEdgeProjectCompletionIntegration/Consumer/bin Tests/LanguageEdgeProjectCompletionIntegration/Consumer/publish
	rm -f Tests/LanguageEdgeProjectCompletionIntegration/Consumer/native/libVoid100Lib.a
	rm -rf Tests/UnsafeDelegates/.void Tests/UnsafeDelegates/bin Tests/UnsafeDelegates/publish
	rm -rf Tests/UnsafeDelegateDiagnostics/*/.void Tests/UnsafeDelegateDiagnostics/*/bin Tests/UnsafeDelegateDiagnostics/*/publish
	rm -rf Tests/UnsafePointerProperties/.void Tests/UnsafePointerProperties/bin Tests/UnsafePointerProperties/publish
	rm -rf Tests/UnsafePointerPropertyDiagnostics/*/.void Tests/UnsafePointerPropertyDiagnostics/*/bin Tests/UnsafePointerPropertyDiagnostics/*/publish
	rm -rf Tests/DoWhileCompletion/.void Tests/DoWhileCompletion/bin Tests/DoWhileCompletion/publish
	rm -rf Tests/DoWhileDiagnostics/*/.void Tests/DoWhileDiagnostics/*/bin Tests/DoWhileDiagnostics/*/publish
	rm -rf Tests/ExpressionBodiedMembers/.void Tests/ExpressionBodiedMembers/bin Tests/ExpressionBodiedMembers/publish
	rm -rf Tests/ExpressionBodiedMemberDiagnostics/*/.void Tests/ExpressionBodiedMemberDiagnostics/*/bin Tests/ExpressionBodiedMemberDiagnostics/*/publish
	rm -rf Tests/CustomEventAccessors/.void Tests/CustomEventAccessors/bin Tests/CustomEventAccessors/publish
	rm -rf Tests/NullConditionalArrays/.void Tests/NullConditionalArrays/bin Tests/NullConditionalArrays/publish
	rm -rf Tests/NullConditionalArrayDiagnostics/*/.void Tests/NullConditionalArrayDiagnostics/*/bin Tests/NullConditionalArrayDiagnostics/*/publish
	rm -rf Tests/SizedRectangularArrayInitializers/.void Tests/SizedRectangularArrayInitializers/bin Tests/SizedRectangularArrayInitializers/publish
	rm -rf Tests/SizedRectangularArrayInitializerDiagnostics/*/.void Tests/SizedRectangularArrayInitializerDiagnostics/*/bin Tests/SizedRectangularArrayInitializerDiagnostics/*/publish
	rm -rf Tests/ImplicitRectangularArrayInitializers/.void Tests/ImplicitRectangularArrayInitializers/bin Tests/ImplicitRectangularArrayInitializers/publish
	rm -rf Tests/ImplicitRectangularArrayInitializerDiagnostics/*/.void Tests/ImplicitRectangularArrayInitializerDiagnostics/*/bin Tests/ImplicitRectangularArrayInitializerDiagnostics/*/publish
	rm -rf Tests/GenericConstraints/.void Tests/GenericConstraints/bin Tests/GenericConstraints/publish
	rm -rf Tests/LanguageSurfaceCompletionIIIntegration/.void Tests/LanguageSurfaceCompletionIIIntegration/bin Tests/LanguageSurfaceCompletionIIIntegration/publish
	rm -rf Tests/LanguageSurfaceCompletionIIIntegrationDiagnostics/*/.void Tests/LanguageSurfaceCompletionIIIntegrationDiagnostics/*/bin Tests/LanguageSurfaceCompletionIIIntegrationDiagnostics/*/publish
	rm -rf Tests/ExceptionThrowFoundation/.void Tests/ExceptionThrowFoundation/bin Tests/ExceptionThrowFoundation/publish
	rm -rf Tests/ExceptionThrowUnhandled/.void Tests/ExceptionThrowUnhandled/bin Tests/ExceptionThrowUnhandled/publish
	rm -rf Tests/ExceptionThrowDiagnostics/*/.void Tests/ExceptionThrowDiagnostics/*/bin Tests/ExceptionThrowDiagnostics/*/publish
	rm -rf Tests/TryCatchCompletion/.void Tests/TryCatchCompletion/bin Tests/TryCatchCompletion/publish
	rm -rf Tests/TryCatchUnhandled/.void Tests/TryCatchUnhandled/bin Tests/TryCatchUnhandled/publish
	rm -rf Tests/TryCatchDiagnostics/*/.void Tests/TryCatchDiagnostics/*/bin Tests/TryCatchDiagnostics/*/publish
	rm -rf Tests/ExceptionPropagationGcUnwinding/.void Tests/ExceptionPropagationGcUnwinding/bin Tests/ExceptionPropagationGcUnwinding/publish
	rm -rf Tests/FinallyCompletion/.void Tests/FinallyCompletion/bin Tests/FinallyCompletion/publish
	rm -rf Tests/FinallyDiagnostics/*/.void Tests/FinallyDiagnostics/*/bin Tests/FinallyDiagnostics/*/publish
	rm -rf Tests/StructuredControlFinally/.void Tests/StructuredControlFinally/bin Tests/StructuredControlFinally/publish
	rm -rf Tests/StructuredControlFinallyDiagnostics/*/.void Tests/StructuredControlFinallyDiagnostics/*/bin Tests/StructuredControlFinallyDiagnostics/*/publish
	rm -rf Tests/NestedHandlersRethrow/.void Tests/NestedHandlersRethrow/bin Tests/NestedHandlersRethrow/publish
	rm -rf Tests/NestedHandlersRethrowDiagnostics/*/.void Tests/NestedHandlersRethrowDiagnostics/*/bin Tests/NestedHandlersRethrowDiagnostics/*/publish
	rm -rf Tests/IteratorExceptionIntegration/.void Tests/IteratorExceptionIntegration/bin Tests/IteratorExceptionIntegration/publish
	rm -rf Tests/IteratorExceptionDiagnostics/*/.void Tests/IteratorExceptionDiagnostics/*/bin Tests/IteratorExceptionDiagnostics/*/publish
	rm -rf Tests/DelegateEventExceptionIntegration/.void Tests/DelegateEventExceptionIntegration/bin Tests/DelegateEventExceptionIntegration/publish
	rm -rf Tests/NativeLibraryBoundaryExceptions/Runtime/.void Tests/NativeLibraryBoundaryExceptions/Runtime/bin Tests/NativeLibraryBoundaryExceptions/Runtime/publish
	rm -rf Tests/NativeLibraryBoundaryExceptions/Library/.void Tests/NativeLibraryBoundaryExceptions/Library/bin Tests/NativeLibraryBoundaryExceptions/Library/publish
	rm -rf Tests/ExceptionsRuntimeControlFlowIntegration/.void Tests/ExceptionsRuntimeControlFlowIntegration/bin Tests/ExceptionsRuntimeControlFlowIntegration/publish
	rm -rf Tests/IteratorDisposalFoundation/.void Tests/IteratorDisposalFoundation/bin Tests/IteratorDisposalFoundation/publish
	rm -rf Tests/IteratorDisposalDiagnostics/*/.void Tests/IteratorDisposalDiagnostics/*/bin Tests/IteratorDisposalDiagnostics/*/publish
	rm -rf Tests/ForeachDisposalCompletion/.void Tests/ForeachDisposalCompletion/bin Tests/ForeachDisposalCompletion/publish
	rm -rf Tests/ForeachDisposalDiagnostics/*/.void Tests/ForeachDisposalDiagnostics/*/bin Tests/ForeachDisposalDiagnostics/*/publish
	rm -rf Tests/UsingStatementCompletion/.void Tests/UsingStatementCompletion/bin Tests/UsingStatementCompletion/publish
	rm -rf Tests/UsingStatementDiagnostics/*/.void Tests/UsingStatementDiagnostics/*/bin Tests/UsingStatementDiagnostics/*/publish
	rm -rf Tests/UsingDeclarationCompletion/.void Tests/UsingDeclarationCompletion/bin Tests/UsingDeclarationCompletion/publish
	rm -rf Tests/UsingDeclarationDiagnostics/*/.void Tests/UsingDeclarationDiagnostics/*/bin Tests/UsingDeclarationDiagnostics/*/publish
	rm -f Tests/NativeLibraryBoundaryExceptions/CConsumer/library-ok Tests/NativeLibraryBoundaryExceptions/CConsumer/library-throw
	rm -rf Tests/GenericConstraintDiagnostics/*/.void Tests/GenericConstraintDiagnostics/*/bin Tests/GenericConstraintDiagnostics/*/publish
	rm -rf Tests/CustomEventAccessorDiagnostics/*/.void Tests/CustomEventAccessorDiagnostics/*/bin Tests/CustomEventAccessorDiagnostics/*/publish
	rm -rf Tests/RectangularArrayInitializerDiagnostics/*/.void Tests/RectangularArrayInitializerDiagnostics/*/bin Tests/RectangularArrayInitializerDiagnostics/*/publish
	rm -f $(QUERY_TEST)
	rm -f $(NATIVE_FIXTURE_OBJECT) $(NATIVE_FIXTURE)
	rm -f $(NATIVE_BUFFER_CALL_OBJECT) $(NATIVE_BUFFER_CALL_FIXTURE)
	rm -f $(NATIVE_CALLBACK_BYREF_OBJECT) $(NATIVE_CALLBACK_BYREF_FIXTURE)
	rm -f $(LANGUAGE_EDGE_INTEGRATION_NATIVE_OBJECT) $(LANGUAGE_EDGE_INTEGRATION_NATIVE_FIXTURE)
	rm -f $(UNSAFE_DELEGATE_NATIVE_OBJECT) $(UNSAFE_DELEGATE_NATIVE_FIXTURE)
	rm -f $(NATIVE_BOUNDARY_EXCEPTION_OBJECT) $(NATIVE_BOUNDARY_EXCEPTION_FIXTURE)
	rm -f $(THREAD_EXCEPTION_NATIVE_OBJECT) $(THREAD_EXCEPTION_NATIVE_FIXTURE)
	rm -rf Tests/UnmanagedGenericConstraintFoundation/.void Tests/UnmanagedGenericConstraintFoundation/bin Tests/UnmanagedGenericConstraintFoundation/publish
	rm -rf Tests/UnmanagedGenericConstraintDiagnostics/*/.void Tests/UnmanagedGenericConstraintDiagnostics/*/bin Tests/UnmanagedGenericConstraintDiagnostics/*/publish
	rm -rf Tests/GenericPointerUnmanagedStackStorage/.void Tests/GenericPointerUnmanagedStackStorage/bin Tests/GenericPointerUnmanagedStackStorage/publish
	rm -rf Tests/GenericPointerUnmanagedStackStorageDiagnostics/*/.void Tests/GenericPointerUnmanagedStackStorageDiagnostics/*/bin Tests/GenericPointerUnmanagedStackStorageDiagnostics/*/publish
	rm -rf Tests/GenericPointerUnmanagedStackStorageRuntime/*/.void Tests/GenericPointerUnmanagedStackStorageRuntime/*/bin Tests/GenericPointerUnmanagedStackStorageRuntime/*/publish
	rm -rf Tests/ManagedPinningRuntimeFoundation/Library/.void Tests/ManagedPinningRuntimeFoundation/Library/bin Tests/ManagedPinningRuntimeFoundation/Library/publish
	rm -f $(MANAGED_PINNING_RUNTIME_TEST)
	rm -rf Tests/ManagedPinningRuntimeFoundationDiagnostics/*/.void Tests/ManagedPinningRuntimeFoundationDiagnostics/*/bin Tests/ManagedPinningRuntimeFoundationDiagnostics/*/publish
	rm -rf Tests/FixedStatementScopedPointerSemantics/.void Tests/FixedStatementScopedPointerSemantics/bin Tests/FixedStatementScopedPointerSemantics/publish
	rm -rf Tests/FixedStatementScopedPointerCleanup/Library/.void Tests/FixedStatementScopedPointerCleanup/Library/bin Tests/FixedStatementScopedPointerCleanup/Library/publish
	rm -f $(FIXED_STATEMENT_CLEANUP_TEST)
	rm -rf Tests/FixedStatementScopedPointerDiagnostics/*/.void Tests/FixedStatementScopedPointerDiagnostics/*/bin Tests/FixedStatementScopedPointerDiagnostics/*/publish
	rm -rf Tests/FixedStatementScopedPointerRuntime/*/.void Tests/FixedStatementScopedPointerRuntime/*/bin Tests/FixedStatementScopedPointerRuntime/*/publish
	rm -rf Tests/StructuralPinnableReferenceProtocol/.void Tests/StructuralPinnableReferenceProtocol/bin Tests/StructuralPinnableReferenceProtocol/publish
	rm -rf Tests/StructuralPinnableReferenceProtocolDiagnostics/*/.void Tests/StructuralPinnableReferenceProtocolDiagnostics/*/bin Tests/StructuralPinnableReferenceProtocolDiagnostics/*/publish
	rm -rf Tests/StructuralPinnableReferenceProtocolRuntime/*/.void Tests/StructuralPinnableReferenceProtocolRuntime/*/bin Tests/StructuralPinnableReferenceProtocolRuntime/*/publish
	rm -rf Tests/ArraySpanMemoryPinningCompletion/.void Tests/ArraySpanMemoryPinningCompletion/bin Tests/ArraySpanMemoryPinningCompletion/publish
	rm -rf Tests/ArraySpanMemoryPinningCompletionDiagnostics/*/.void Tests/ArraySpanMemoryPinningCompletionDiagnostics/*/bin Tests/ArraySpanMemoryPinningCompletionDiagnostics/*/publish
	rm -rf Tests/UnmanagedSpanConstructionCompletion/.void Tests/UnmanagedSpanConstructionCompletion/bin Tests/UnmanagedSpanConstructionCompletion/publish
	rm -rf Tests/UnmanagedSpanConstructionDiagnostics/*/.void Tests/UnmanagedSpanConstructionDiagnostics/*/bin Tests/UnmanagedSpanConstructionDiagnostics/*/publish
	rm -rf Tests/UnmanagedSpanConstructionRuntime/*/.void Tests/UnmanagedSpanConstructionRuntime/*/bin Tests/UnmanagedSpanConstructionRuntime/*/publish
	rm -rf Tests/NativeBufferCallIntegration/.void Tests/NativeBufferCallIntegration/bin Tests/NativeBufferCallIntegration/publish
	rm -rf Tests/NativeBufferCallIntegrationDiagnostics/*/.void Tests/NativeBufferCallIntegrationDiagnostics/*/bin Tests/NativeBufferCallIntegrationDiagnostics/*/publish
	rm -rf Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/Library/.void Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/Library/bin Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/Library/publish
	rm -f $(PINNING_GC_THREADS_NATIVE_TEST)
	rm -rf Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Surface/.void Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Surface/bin Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Surface/publish
	rm -rf Tests/UnmanagedMemoryNativeBufferIntegrationAudit/AsyncUnsafe/.void Tests/UnmanagedMemoryNativeBufferIntegrationAudit/AsyncUnsafe/bin Tests/UnmanagedMemoryNativeBufferIntegrationAudit/AsyncUnsafe/publish
	rm -rf Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Library/.void Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Library/bin Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Library/publish
	rm -f $(UNMANAGED_MEMORY_NATIVE_BUFFER_AUDIT_TEST)
	rm -rf Tests/UnmanagedMemoryNativeBufferIntegrationAuditDiagnostics/*/.void Tests/UnmanagedMemoryNativeBufferIntegrationAuditDiagnostics/*/bin Tests/UnmanagedMemoryNativeBufferIntegrationAuditDiagnostics/*/publish
	rm -rf Tests/NativeHeapRuntimeFoundation/.void Tests/NativeHeapRuntimeFoundation/bin Tests/NativeHeapRuntimeFoundation/publish
	rm -f $(NATIVE_HEAP_RUNTIME_TEST)
	rm -rf Tests/NativeMemoryAllocationSurface/.void Tests/NativeMemoryAllocationSurface/bin Tests/NativeMemoryAllocationSurface/publish
	rm -rf Tests/NativeMemoryAllocationSurfaceDiagnostics/*/.void Tests/NativeMemoryAllocationSurfaceDiagnostics/*/bin Tests/NativeMemoryAllocationSurfaceDiagnostics/*/publish
	rm -rf Tests/GenericTypedNativeAllocationCompletion/.void Tests/GenericTypedNativeAllocationCompletion/bin Tests/GenericTypedNativeAllocationCompletion/publish
	rm -rf Tests/GenericTypedNativeAllocationRuntime/*/.void Tests/GenericTypedNativeAllocationRuntime/*/bin Tests/GenericTypedNativeAllocationRuntime/*/publish
	rm -rf Tests/GenericTypedNativeAllocationDiagnostics/*/.void Tests/GenericTypedNativeAllocationDiagnostics/*/bin Tests/GenericTypedNativeAllocationDiagnostics/*/publish
	rm -rf Tests/NativeReallocationAlignmentCompletion/.void Tests/NativeReallocationAlignmentCompletion/bin Tests/NativeReallocationAlignmentCompletion/publish
	rm -f Tests/NativeReallocationAlignmentCompletion/native_memory_contract_test
	rm -rf Tests/NativeReallocationAlignmentRuntime/*/.void Tests/NativeReallocationAlignmentRuntime/*/bin Tests/NativeReallocationAlignmentRuntime/*/publish
	rm -rf Tests/NativeReallocationAlignmentDiagnostics/*/.void Tests/NativeReallocationAlignmentDiagnostics/*/bin Tests/NativeReallocationAlignmentDiagnostics/*/publish
	rm -rf Tests/NativeFunctionPointerTypeFoundation/.void Tests/NativeFunctionPointerTypeFoundation/bin Tests/NativeFunctionPointerTypeFoundation/publish
	rm -rf Tests/NativeFunctionPointerTypeFoundationDiagnostics/*/.void Tests/NativeFunctionPointerTypeFoundationDiagnostics/*/bin Tests/NativeFunctionPointerTypeFoundationDiagnostics/*/publish
	rm -rf Tests/NativeFunctionAddressIndirectCallCompletion/.void Tests/NativeFunctionAddressIndirectCallCompletion/bin Tests/NativeFunctionAddressIndirectCallCompletion/publish
	rm -f $(NATIVE_FUNCTION_ADDRESS_OBJECT) $(NATIVE_FUNCTION_ADDRESS_FIXTURE)
	rm -rf Tests/NativeFunctionAddressIndirectCallRuntime/*/.void Tests/NativeFunctionAddressIndirectCallRuntime/*/bin Tests/NativeFunctionAddressIndirectCallRuntime/*/publish
	rm -rf Tests/NativeFunctionAddressIndirectCallDiagnostics/*/.void Tests/NativeFunctionAddressIndirectCallDiagnostics/*/bin Tests/NativeFunctionAddressIndirectCallDiagnostics/*/publish
	rm -rf Tests/NativeFunctionPointerAbiIntegration/.void Tests/NativeFunctionPointerAbiIntegration/bin Tests/NativeFunctionPointerAbiIntegration/publish
	rm -f $(NATIVE_FUNCTION_ABI_OBJECT) $(NATIVE_FUNCTION_ABI_FIXTURE)
	rm -rf Tests/NativeFunctionPointerAbiIntegrationDiagnostics/*/.void Tests/NativeFunctionPointerAbiIntegrationDiagnostics/*/bin Tests/NativeFunctionPointerAbiIntegrationDiagnostics/*/publish
	rm -rf Tests/GenericFunctionPointerUnmanagedStorageIntegration/.void Tests/GenericFunctionPointerUnmanagedStorageIntegration/bin Tests/GenericFunctionPointerUnmanagedStorageIntegration/publish
	rm -f $(NATIVE_FUNCTION_GENERIC_OBJECT) $(NATIVE_FUNCTION_GENERIC_FIXTURE)
	rm -rf Tests/GenericFunctionPointerUnmanagedStorageIntegrationDiagnostics/*/.void Tests/GenericFunctionPointerUnmanagedStorageIntegrationDiagnostics/*/bin Tests/GenericFunctionPointerUnmanagedStorageIntegrationDiagnostics/*/publish
	rm -rf Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/Library/.void Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/Library/bin Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/Library/publish
	rm -f $(NATIVE_HEAP_FUNCTION_POINTER_INTEGRATION_TEST)
	rm -rf Tests/NativeHeapFunctionPointerIntegrationAudit/Surface/.void Tests/NativeHeapFunctionPointerIntegrationAudit/Surface/bin Tests/NativeHeapFunctionPointerIntegrationAudit/Surface/publish
	rm -f $(NATIVE_HEAP_FUNCTION_POINTER_AUDIT_OBJECT) $(NATIVE_HEAP_FUNCTION_POINTER_AUDIT_FIXTURE)
	rm -rf Tests/NativeHeapFunctionPointerIntegrationAudit/Library/.void Tests/NativeHeapFunctionPointerIntegrationAudit/Library/bin Tests/NativeHeapFunctionPointerIntegrationAudit/Library/publish
	rm -f $(NATIVE_HEAP_FUNCTION_POINTER_AUDIT_TEST) $(NATIVE_HEAP_FUNCTION_POINTER_AUDIT_MEMORY_TEST)
	rm -rf Tests/NativeHeapFunctionPointerIntegrationAuditDiagnostics/*/.void Tests/NativeHeapFunctionPointerIntegrationAuditDiagnostics/*/bin Tests/NativeHeapFunctionPointerIntegrationAuditDiagnostics/*/publish
	rm -rf Tests/ManagedStringRepresentationFoundation/.void Tests/ManagedStringRepresentationFoundation/bin Tests/ManagedStringRepresentationFoundation/publish
	rm -rf Tests/ManagedStringRepresentationFoundationDiagnostics/*/.void Tests/ManagedStringRepresentationFoundationDiagnostics/*/bin Tests/ManagedStringRepresentationFoundationDiagnostics/*/publish
	rm -rf Tests/LengthAwareStringSemantics/.void Tests/LengthAwareStringSemantics/bin Tests/LengthAwareStringSemantics/publish
	rm -rf Tests/LengthAwareStringSemanticsOutput/.void Tests/LengthAwareStringSemanticsOutput/bin Tests/LengthAwareStringSemanticsOutput/publish
	rm -rf Tests/LengthAwareStringSemanticsDiagnostics/*/.void Tests/LengthAwareStringSemanticsDiagnostics/*/bin Tests/LengthAwareStringSemanticsDiagnostics/*/publish
	rm -rf Tests/NativeStringAbiContractMetadataFoundation/.void Tests/NativeStringAbiContractMetadataFoundation/bin Tests/NativeStringAbiContractMetadataFoundation/publish
	rm -rf Tests/NativeStringAbiContractMetadataFoundationDiagnostics/*/.void Tests/NativeStringAbiContractMetadataFoundationDiagnostics/*/bin Tests/NativeStringAbiContractMetadataFoundationDiagnostics/*/publish
	rm -rf Tests/BorrowedNulNativeStringParameters/.void Tests/BorrowedNulNativeStringParameters/bin Tests/BorrowedNulNativeStringParameters/publish
	rm -rf Tests/BorrowedNulNativeStringParametersDiagnostics/*/.void Tests/BorrowedNulNativeStringParametersDiagnostics/*/bin Tests/BorrowedNulNativeStringParametersDiagnostics/*/publish
	rm -rf Tests/PointerLengthNativeStringParameters/.void Tests/PointerLengthNativeStringParameters/bin Tests/PointerLengthNativeStringParameters/publish
	rm -rf Tests/BorrowedStaticNativeStringReturns/.void Tests/BorrowedStaticNativeStringReturns/bin Tests/BorrowedStaticNativeStringReturns/publish
	rm -rf Tests/BorrowedStaticNativeStringReturnsDiagnostics/*/.void Tests/BorrowedStaticNativeStringReturnsDiagnostics/*/bin Tests/BorrowedStaticNativeStringReturnsDiagnostics/*/publish
	rm -rf Tests/OwnedNativeStringReturns/.void Tests/OwnedNativeStringReturns/bin Tests/OwnedNativeStringReturns/publish
	rm -rf Tests/OwnedNativeStringReturnsDiagnostics/*/.void Tests/OwnedNativeStringReturnsDiagnostics/*/bin Tests/OwnedNativeStringReturnsDiagnostics/*/publish
	rm -rf Tests/.test-logs
	rm -rf Tests/__pycache__
	rm -rf Tests/StandardLibraryCompatibilityCleanup/.void Tests/StandardLibraryCompatibilityCleanup/bin Tests/StandardLibraryCompatibilityCleanup/publish
	rm -rf Tests/StandardLibraryCompatibilityCleanupDiagnostics/*/.void Tests/StandardLibraryCompatibilityCleanupDiagnostics/*/bin Tests/StandardLibraryCompatibilityCleanupDiagnostics/*/publish
	rm -rf Tests/StandardLibraryConsoleOutput/.void Tests/StandardLibraryConsoleOutput/bin Tests/StandardLibraryConsoleOutput/publish
	rm -rf Tests/StandardLibraryConsoleWriteFailure/.void Tests/StandardLibraryConsoleWriteFailure/bin Tests/StandardLibraryConsoleWriteFailure/publish
	rm -rf Tests/ConsoleReadLineFoundation/.void Tests/ConsoleReadLineFoundation/bin Tests/ConsoleReadLineFoundation/publish
	rm -rf Tests/ConsoleReadLineFoundationDiagnostics/*/.void Tests/ConsoleReadLineFoundationDiagnostics/*/bin Tests/ConsoleReadLineFoundationDiagnostics/*/publish
	rm -rf Tests/ConsoleReadSharedInputBuffering/.void Tests/ConsoleReadSharedInputBuffering/bin Tests/ConsoleReadSharedInputBuffering/publish
	rm -rf Tests/ConsoleReadSharedInputBufferingDiagnostics/*/.void Tests/ConsoleReadSharedInputBufferingDiagnostics/*/bin Tests/ConsoleReadSharedInputBufferingDiagnostics/*/publish
	rm -rf Tests/ConsoleRedirection/.void Tests/ConsoleRedirection/bin Tests/ConsoleRedirection/publish
	rm -rf Tests/ConsoleRedirectionDiagnostics/*/.void Tests/ConsoleRedirectionDiagnostics/*/bin Tests/ConsoleRedirectionDiagnostics/*/publish
	rm -rf Tests/ConsoleStandardStreams/.void Tests/ConsoleStandardStreams/bin Tests/ConsoleStandardStreams/publish
	rm -rf Tests/ConsoleStandardStreamsDiagnostics/*/.void Tests/ConsoleStandardStreamsDiagnostics/*/bin Tests/ConsoleStandardStreamsDiagnostics/*/publish
	rm -rf Tests/ConsoleEncodingIntegration/.void Tests/ConsoleEncodingIntegration/bin Tests/ConsoleEncodingIntegration/publish
	rm -rf Tests/ConsoleEncodingIntegrationDiagnostics/*/.void Tests/ConsoleEncodingIntegrationDiagnostics/*/bin Tests/ConsoleEncodingIntegrationDiagnostics/*/publish
	rm -rf Tests/ConsoleKeyReadKey/.void Tests/ConsoleKeyReadKey/bin Tests/ConsoleKeyReadKey/publish
	rm -rf Tests/ConsoleKeyReadKeyDiagnostics/*/.void Tests/ConsoleKeyReadKeyDiagnostics/*/bin Tests/ConsoleKeyReadKeyDiagnostics/*/publish
	rm -rf Tests/ConsoleInputControl/.void Tests/ConsoleInputControl/bin Tests/ConsoleInputControl/publish
	rm -rf Tests/ConsoleInputControlDiagnostics/*/.void Tests/ConsoleInputControlDiagnostics/*/bin Tests/ConsoleInputControlDiagnostics/*/publish
	rm -rf Tests/ConsolePresentationBasics/.void Tests/ConsolePresentationBasics/bin Tests/ConsolePresentationBasics/publish
	rm -rf Tests/ConsoleStandardIoTerminalIntegrationAudit/.void Tests/ConsoleStandardIoTerminalIntegrationAudit/bin Tests/ConsoleStandardIoTerminalIntegrationAudit/publish
	rm -rf Tests/ConsoleStandardIoTerminalIntegrationAuditDiagnostics/*/.void Tests/ConsoleStandardIoTerminalIntegrationAuditDiagnostics/*/bin Tests/ConsoleStandardIoTerminalIntegrationAuditDiagnostics/*/publish
	rm -rf Tests/ConsolePresentationBasicsDiagnostics/*/.void Tests/ConsolePresentationBasicsDiagnostics/*/bin Tests/ConsolePresentationBasicsDiagnostics/*/publish
	rm -rf Tests/ConsoleTextReaderWriter/.void Tests/ConsoleTextReaderWriter/bin Tests/ConsoleTextReaderWriter/publish
	rm -rf Tests/ConsoleTextReaderWriterDiagnostics/*/.void Tests/ConsoleTextReaderWriterDiagnostics/*/bin Tests/ConsoleTextReaderWriterDiagnostics/*/publish
	rm -rf Tests/EngineListOperations/.void Tests/EngineListOperations/bin Tests/EngineListOperations/publish
	rm -rf Tests/EngineSetOperations/.void Tests/EngineSetOperations/bin Tests/EngineSetOperations/publish
	rm -rf Tests/EngineMemoryStream/.void Tests/EngineMemoryStream/bin Tests/EngineMemoryStream/publish
	rm -rf Tests/EngineReadinessDiagnostics/*/.void Tests/EngineReadinessDiagnostics/*/bin Tests/EngineReadinessDiagnostics/*/publish

test-diagnostics: $(BIN)
	@output="$$(./$(BIN) lex Tests/Diagnostics/Tokens.void 2>&1)"; \
	printf '%s\n' "$$output" | grep -Fq '1:1-1:7 public'; \
	echo True
	@output="$$(./$(BIN) lex Tests/Diagnostics/Lexer/Program.void 2>&1 || :)"; \
	printf '%s\n' "$$output" | grep -Fq 'Tests/Diagnostics/Lexer/Program.void:5:9-5:10: error: unexpected character'; \
	echo True
	@output="$$(./$(BIN) lex Tests/Diagnostics/Lexer/BlockComment.void 2>&1 || :)"; \
	printf '%s\n' "$$output" | grep -Fqx 'Tests/Diagnostics/Lexer/BlockComment.void:1:1-3:1: error: unterminated block comment'; \
	echo True
	@output="$$(./$(BIN) parse Tests/Diagnostics/Parser/Program.void 2>&1 || :)"; \
	printf '%s\n' "$$output" | grep -Fqx "Tests/Diagnostics/Parser/Program.void:6:5-6:5: error: expected ';', found '}'"; \
	echo True
	@output="$$(./$(BIN) build Tests/Diagnostics/Semantic 2>&1 || :)"; \
	printf '%s\n' "$$output" | grep -Fqx "Tests/Diagnostics/Semantic/Program.void:5:21-5:33: error: unknown identifier 'MissingValue'"; \
	echo True

test-check: $(BIN)
	@rm -rf Tests/Check/Valid/.void Tests/Check/Valid/bin Tests/Check/Valid/publish; \
	output="$$(CC=false ./$(BIN) check Tests/Check/Valid 2>&1)"; \
	printf '%s\n' "$$output" | grep -Fqx 'checked: CheckValid'; \
	echo True
	@test ! -e Tests/Check/Valid/.void; \
	test ! -e Tests/Check/Valid/bin; \
	test ! -e Tests/Check/Valid/publish; \
	echo True
	@output="$$(./$(BIN) check Tests/Check/Semantic 2>&1 || :)"; \
	printf '%s\n' "$$output" | grep -Fqx "Tests/Check/Semantic/Program.void:5:21-5:33: error: unknown identifier 'MissingValue'"; \
	echo True
	@output="$$(./$(BIN) check Tests/Check/Parser 2>&1 || :)"; \
	printf '%s\n' "$$output" | grep -Fqx "Tests/Check/Parser/Program.void:6:5-6:5: error: expected ';', found '}'"; \
	echo True
	@output="$$(CC=false ./$(BIN) check Tests/Check/Projectless 2>&1)"; \
	printf '%s\n' "$$output" | grep -Fqx 'checked: Program'; \
	echo True

test-semantic-queries: $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUERY_TEST_COMPILER_SOURCES) $(QUERY_TEST_SOURCE) -o $(QUERY_TEST)
	@./$(QUERY_TEST)

test-debug-info: $(BIN)
	@set -e; \
	rm -rf $(DEBUG_INFO_PROJECT)/.void $(DEBUG_INFO_PROJECT)/bin $(DEBUG_INFO_PROJECT)/publish; \
	./$(BIN) build $(DEBUG_INFO_PROJECT) >/dev/null; \
	grep -Fq '#line 7 "Tests/DebugInfo/Program.void"' $(DEBUG_INFO_C); \
	echo True
	@set -e; \
	grep -Fq '#line 13 "Tests/DebugInfo/Program.void"' $(DEBUG_INFO_C); \
	echo True
	@set -e; \
	$(DEBUG_LINE_DUMP) $(DEBUG_INFO_BINARY) 2>/dev/null | grep -Eq 'Program\.void[[:space:]]+7[[:space:]]'; \
	echo True
	@set -e; \
	$(DEBUG_LINE_DUMP) $(DEBUG_INFO_BINARY) 2>/dev/null | grep -Eq 'Program\.void[[:space:]]+13[[:space:]]'; \
	echo True
	@set -e; \
	$(DEBUG_SECTION_DUMP) $(DEBUG_INFO_BINARY) 2>/dev/null | grep -Fq '.debug_line'; \
	echo True
	@set -e; \
	output="$$(./$(DEBUG_INFO_BINARY))"; \
	test "$$output" = '5'; \
	echo True

test-lsp: $(BIN)
	@$(PYTHON) Tests/test_lsp_transport.py
	@set -e; \
	msg='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'; \
	output="$$( { printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","id":2,"method":"shutdown","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","method":"exit","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; } | ./$(BIN) lsp )"; \
	printf '%s' "$$output" | grep -Fq '"textDocumentSync":1'; \
	echo True
	@set -e; \
	msg='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'; \
	output="$$( { printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","id":2,"method":"shutdown","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","method":"exit","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; } | ./$(BIN) lsp )"; \
	printf '%s' "$$output" | grep -Fq '"serverInfo":{"name":"voidc","version":"0.0.360"}'; \
	echo True
	@set -e; \
	msg='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'; \
	output="$$( { printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","id":2,"method":"shutdown","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","method":"exit","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; } | ./$(BIN) lsp )"; \
	printf '%s' "$$output" | grep -Fq '{"jsonrpc":"2.0","id":2,"result":null}'; \
	echo True
	@set -e; \
	uri="$$( $(PYTHON) -c 'from pathlib import Path; print(Path("Tests/LspProtocol/Program.void").resolve().as_uri())')"; \
	msg='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'; \
	{ printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","method":"initialized","params":{}}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"%s","languageId":"void","version":1,"text":"public static class Program { }"}}}' "$$uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"%s","version":2},"contentChanges":[{"text":"public static class Program { public static void Main() { } }"}]}}' "$$uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didClose","params":{"textDocument":{"uri":"%s"}}}' "$$uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","id":2,"method":"shutdown","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","method":"exit","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; } | ./$(BIN) lsp >/dev/null; \
	echo True
	@set -e; \
	msg='{"jsonrpc":"2.0","id":7,"method":"voidc/notImplemented","params":{}}'; \
	output="$$( { printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","id":8,"method":"shutdown","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","method":"exit","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; } | ./$(BIN) lsp )"; \
	printf '%s' "$$output" | grep -Fq '"code":-32601'; \
	echo True
	@set +e; \
	msg='{"jsonrpc":"2.0","method":"exit","params":null}'; \
	{ printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; } | ./$(BIN) lsp >/dev/null; \
	status=$$?; set -e; test $$status -eq 1; echo True

test-lsp-diagnostics: $(BIN)
	@set -e; \
	uri="$$( $(PYTHON) -c 'from pathlib import Path; print(Path("Tests/LspDiagnostics/Program.void").resolve().as_uri())')"; \
	msg='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'; \
	output="$$( { printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"%s","languageId":"void","version":1,"text":"public static class Program { public static void Main() { missing; } }"}}}' "$$uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"%s","version":2},"contentChanges":[{"text":"public static class Program { public static void Main() { int value = 1 } }"}]}}' "$$uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"%s","version":3},"contentChanges":[{"text":"public static class Program { public static void Main() { # } }"}]}}' "$$uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"%s","version":4},"contentChanges":[{"text":"public static class Program { public static void Main() { } }"}]}}' "$$uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didClose","params":{"textDocument":{"uri":"%s"}}}' "$$uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","id":2,"method":"shutdown","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","method":"exit","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; } | ./$(BIN) lsp )"; \
	printf '%s' "$$output" | grep -Fq '"severity":1,"source":"voidc","message":"unknown identifier '\''missing'\''"'; \
	echo True; \
	printf '%s' "$$output" | grep -Fq '"version":1,"diagnostics":[{"range":{"start":{"line":0,"character":58},"end":{"line":0,"character":65}}'; \
	echo True; \
	printf '%s' "$$output" | grep -Fq '"version":2,"diagnostics":[{"range":{"start":{"line":0,"character":72},"end":{"line":0,"character":72}},"severity":1,"source":"voidc","message":"expected '\'';'\'', found '\''}'\''"'; \
	echo True; \
	printf '%s' "$$output" | grep -Fq '"version":3,"diagnostics":[{"range":{"start":{"line":0,"character":58},"end":{"line":0,"character":59}},"severity":1,"source":"voidc","message":"unexpected character '\''#'\''"'; \
	echo True; \
	printf '%s' "$$output" | grep -Fq '"version":4,"diagnostics":[]'; \
	echo True; \
	test "$$(printf '%s' "$$output" | grep -Fo '"version":4,"diagnostics":[]' | wc -l)" -eq 2; \
	echo True
	@set -e; \
	uri="$$( $(PYTHON) -c 'from pathlib import Path; print(Path("Tests/LspDiagnostics/Program.void").resolve().as_uri())')"; \
	msg='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'; \
	output="$$( { printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	unicode_comment="$$(printf '\360\237\230\200')"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"%s","languageId":"void","version":1,"text":"public static class Program { public static void Main() { /*%s*/ missing; } }"}}}' "$$uri" "$$unicode_comment")"; printf 'Content-Length: %d\r\n\r\n%s' "$$(printf '%s' "$$msg" | wc -c)" "$$msg"; \
	msg='{"jsonrpc":"2.0","id":2,"method":"shutdown","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","method":"exit","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; } | ./$(BIN) lsp )"; \
	printf '%s' "$$output" | grep -Fq '"range":{"start":{"line":0,"character":65},"end":{"line":0,"character":72}}'; \
	echo True

test-lsp-hover-signature: $(BIN)
	@set -e; \
	uri="$$( $(PYTHON) -c 'from pathlib import Path; print(Path("Tests/LspHoverSignature/Program.void").resolve().as_uri())')"; \
	text="$$(sed ':a;N;$$!ba;s/\\/\\\\/g;s/"/\\"/g;s/\n/\\n/g' Tests/LspHoverSignature/Program.void)"; \
	hover_char="$$(awk 'NR==41{print index($$0,"Mix")-1}' Tests/LspHoverSignature/Program.void)"; \
	method_char="$$(awk 'NR==41{print index($$0,"right: 4")-1+8}' Tests/LspHoverSignature/Program.void)"; \
	constructor_char="$$(awk 'NR==40{print index($$0,"extra: 3")-1+7}' Tests/LspHoverSignature/Program.void)"; \
	params_char="$$(awk 'NR==42{print index($$0,"3);")-1}' Tests/LspHoverSignature/Program.void)"; \
	delegate_char="$$(awk 'NR==44{print index($$0,"bonus: 2")-1+7}' Tests/LspHoverSignature/Program.void)"; \
	msg='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'; \
	output="$$( { printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"%s","languageId":"void","version":1,"text":"%s"}}}' "$$uri" "$$text")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":2,"method":"textDocument/hover","params":{"textDocument":{"uri":"%s"},"position":{"line":40,"character":%s}}}' "$$uri" "$$hover_char")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":3,"method":"textDocument/signatureHelp","params":{"textDocument":{"uri":"%s"},"position":{"line":40,"character":%s}}}' "$$uri" "$$method_char")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":4,"method":"textDocument/signatureHelp","params":{"textDocument":{"uri":"%s"},"position":{"line":39,"character":%s}}}' "$$uri" "$$constructor_char")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":5,"method":"textDocument/signatureHelp","params":{"textDocument":{"uri":"%s"},"position":{"line":41,"character":%s}}}' "$$uri" "$$params_char")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":6,"method":"textDocument/signatureHelp","params":{"textDocument":{"uri":"%s"},"position":{"line":43,"character":%s}}}' "$$uri" "$$delegate_char")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","id":7,"method":"shutdown","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","method":"exit","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; } | ./$(BIN) lsp )"; \
	printf '%s' "$$output" | grep -Fq '"hoverProvider":true'; echo True; \
	printf '%s' "$$output" | grep -Fq '"signatureHelpProvider":{"triggerCharacters":["(",","]}'; echo True; \
	printf '%s' "$$output" | grep -Fq '"value":"method Mix(int left, int right = ...): int"'; echo True; \
	printf '%s' "$$output" | grep -Fq '"label":"Mix(int left, int right = ...): int"'; \
	printf '%s' "$$output" | grep -Fq '"label":"Mix(string text, int count = ...): int"'; \
	printf '%s' "$$output" | grep -Fq '"id":3,"result":{"signatures"'; \
	printf '%s' "$$output" | grep -Fq '"activeSignature":0,"activeParameter":1'; echo True; \
	printf '%s' "$$output" | grep -Fq '"label":"Calculator(int seed, int extra = ...)"'; \
	printf '%s' "$$output" | grep -Fq '"label":"Calculator(string name)"'; echo True; \
	printf '%s' "$$output" | grep -Fq '"label":"Sum(int start, params int[] values): int"'; \
	printf '%s' "$$output" | grep -Fq '"label":"params int[] values"'; echo True; \
	printf '%s' "$$output" | grep -Fq '"label":"Work(int value, int bonus = ...): int"'; echo True

test-lsp-completion: $(BIN)
	@set -e; \
	uri="$$( $(PYTHON) -c 'from pathlib import Path; print(Path("Tests/LspCompletion/Program.void").resolve().as_uri())')"; \
	text="$$(sed ':a;N;$$!ba;s/\\/\\\\/g;s/"/\\"/g;s/\n/\\n/g' Tests/LspCompletion/Program.void)"; \
	instance_char="$$(awk 'NR==51{print index($$0,"Health")-1}' Tests/LspCompletion/Program.void)"; \
	static_char="$$(awk 'NR==52{print index($$0,"Count")-1}' Tests/LspCompletion/Program.void)"; \
	enum_char="$$(awk 'NR==53{print index($$0,"Idle")-1}' Tests/LspCompletion/Program.void)"; \
	general_char="$$(awk 'NR==54{print index($$0,"total")-1}' Tests/LspCompletion/Program.void)"; \
	inside_char="$$(awk 'NR==42{print index($$0,"local")-1}' Tests/LspCompletion/Program.void)"; \
	msg='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'; \
	output="$$( { printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"%s","languageId":"void","version":1,"text":"%s"}}}' "$$uri" "$$text")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":2,"method":"textDocument/completion","params":{"textDocument":{"uri":"%s"},"position":{"line":50,"character":%s}}}' "$$uri" "$$instance_char")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":3,"method":"textDocument/completion","params":{"textDocument":{"uri":"%s"},"position":{"line":51,"character":%s}}}' "$$uri" "$$static_char")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":4,"method":"textDocument/completion","params":{"textDocument":{"uri":"%s"},"position":{"line":52,"character":%s}}}' "$$uri" "$$enum_char")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":5,"method":"textDocument/completion","params":{"textDocument":{"uri":"%s"},"position":{"line":53,"character":%s}}}' "$$uri" "$$general_char")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":6,"method":"textDocument/completion","params":{"textDocument":{"uri":"%s"},"position":{"line":41,"character":%s}}}' "$$uri" "$$inside_char")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","id":7,"method":"shutdown","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","method":"exit","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; } | ./$(BIN) lsp )"; \
	printf '%s' "$$output" | grep -Fq '"completionProvider":{"triggerCharacters":["."]}'; echo True; \
	printf '%s' "$$output" | grep -Fq '"id":2,"result":{"isIncomplete":false,"items":[{"label":"Health","kind":5,"detail":"int"},{"label":"Score","kind":10,"detail":"int"},{"label":"Damage","kind":2,"detail":"void"},{"label":"Inspect","kind":2,"detail":"void"},{"label":"PublicBase","kind":5,"detail":"int"},{"label":"BaseMethod","kind":2,"detail":"void"}]}}'; echo True; \
	printf '%s' "$$output" | grep -Fq '"id":3,"result":{"isIncomplete":false,"items":[{"label":"Count","kind":5,"detail":"int"},{"label":"Max","kind":10,"detail":"int"},{"label":"Reset","kind":2,"detail":"void"}]}}'; echo True; \
	printf '%s' "$$output" | grep -Fq '"id":4,"result":{"isIncomplete":false,"items":[{"label":"Idle","kind":20,"detail":"Mode"},{"label":"Run","kind":20,"detail":"Mode"}]}}'; echo True; \
	printf '%s' "$$output" | grep -Fq '"id":5,"result":{"isIncomplete":false,"items":[{"label":"player","kind":6,"detail":"Player"},{"label":"total","kind":6,"detail":"int"},{"label":"count","kind":6,"detail":"int"},{"label":"mode","kind":6,"detail":"Mode"}'; \
	printf '%s' "$$output" | grep -Fq '{"label":"Player","kind":7,"detail":"Player"}'; \
	printf '%s' "$$output" | grep -Fq '{"label":"Void","kind":9,"detail":"namespace"}'; echo True; \
	printf '%s' "$$output" | grep -Fq '"id":6,"result":{"isIncomplete":false,"items":[{"label":"amount","kind":6,"detail":"int"},{"label":"local","kind":6,"detail":"int"},{"label":"Health"'; echo True; \
	test "$$(printf '%s' "$$output" | grep -Fo '"label":"Secret"' | wc -l)" -eq 1; \
	test "$$(printf '%s' "$$output" | grep -Fo '"label":"ProtectedBase"' | wc -l)" -eq 1; \
	test "$$(printf '%s' "$$output" | grep -Fo '"label":"HiddenBase"' | wc -l)" -eq 0; echo True

test-lsp-navigation: $(BIN)
	@set -e; \
	program_uri="$$( $(PYTHON) -c 'from pathlib import Path; print(Path("Tests/LspNavigation/Program.void").resolve().as_uri())')"; \
	player_uri="$$( $(PYTHON) -c 'from pathlib import Path; print(Path("Tests/LspNavigation/Player.void").resolve().as_uri())')"; \
	program_text="$$(sed ':a;N;$$!ba;s/\\/\\\\/g;s/"/\\"/g;s/\n/\\n/g' Tests/LspNavigation/Program.void)"; \
	player_text="$$(sed ':a;N;$$!ba;s/\\/\\\\/g;s/"/\\"/g;s/\n/\\n/g' Tests/LspNavigation/Player.void)"; \
	damage_char="$$(awk 'NR==8{print index($$0,"Damage")-1}' Tests/LspNavigation/Program.void)"; \
	msg='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'; \
	output="$$( { printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"%s","languageId":"void","version":1,"text":"%s"}}}' "$$program_uri" "$$program_text")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"%s","languageId":"void","version":1,"text":"%s"}}}' "$$player_uri" "$$player_text")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":2,"method":"textDocument/definition","params":{"textDocument":{"uri":"%s"},"position":{"line":7,"character":%s}}}' "$$program_uri" "$$damage_char")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":3,"method":"textDocument/references","params":{"textDocument":{"uri":"%s"},"position":{"line":7,"character":%s},"context":{"includeDeclaration":true}}}' "$$program_uri" "$$damage_char")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":4,"method":"textDocument/references","params":{"textDocument":{"uri":"%s"},"position":{"line":7,"character":%s},"context":{"includeDeclaration":false}}}' "$$program_uri" "$$damage_char")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":5,"method":"textDocument/documentSymbol","params":{"textDocument":{"uri":"%s"}}}' "$$player_uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","id":6,"method":"workspace/symbol","params":{"query":"Damage"}}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","id":7,"method":"workspace/symbol","params":{"query":"MissingNavigationSymbol"}}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","id":8,"method":"shutdown","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","method":"exit","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; } | ./$(BIN) lsp )"; \
	printf '%s' "$$output" | grep -Fq '"definitionProvider":true,"referencesProvider":true,"documentSymbolProvider":true,"workspaceSymbolProvider":true'; echo True; \
	printf '%s' "$$output" | grep -Fq "\"id\":2,\"result\":{\"uri\":\"$$player_uri\",\"range\":{\"start\":{\"line\":4"; echo True; \
	printf '%s' "$$output" | grep -Fq "\"id\":3,\"result\":[{\"uri\":\"$$player_uri\",\"range\":{\"start\":{\"line\":4"; \
	printf '%s' "$$output" | grep -Fq "{\"uri\":\"$$program_uri\",\"range\":{\"start\":{\"line\":7,\"character\":15},\"end\":{\"line\":7,\"character\":21}}},{\"uri\":\"$$program_uri\",\"range\":{\"start\":{\"line\":9,\"character\":15},\"end\":{\"line\":9,\"character\":21}}}]"; echo True; \
	printf '%s' "$$output" | grep -Fq "\"id\":4,\"result\":[{\"uri\":\"$$program_uri\",\"range\":{\"start\":{\"line\":7,\"character\":15},\"end\":{\"line\":7,\"character\":21}}},{\"uri\":\"$$program_uri\",\"range\":{\"start\":{\"line\":9,\"character\":15},\"end\":{\"line\":9,\"character\":21}}}]"; echo True; \
	printf '%s' "$$output" | grep -Fq '"id":5,"result":[{"name":"Player","kind":5'; \
	printf '%s' "$$output" | grep -Fq '"name":"Health","kind":8'; \
	printf '%s' "$$output" | grep -Fq '"name":"Damage","kind":6'; echo True; \
	printf '%s' "$$output" | grep -Fq "\"id\":6,\"result\":[{\"name\":\"Damage\",\"kind\":6,\"location\":{\"uri\":\"$$player_uri\""; echo True; \
	printf '%s' "$$output" | grep -Fq '"id":7,"result":[]'; echo True

test-tooling-integration: $(BIN)
	@set -e; \
	rm -rf Tests/ToolingIntegration/.void Tests/ToolingIntegration/bin Tests/ToolingIntegration/publish; \
	output="$$(CC=false ./$(BIN) check Tests/ToolingIntegration 2>&1)"; \
	printf '%s\n' "$$output" | grep -Fqx 'checked: ToolingIntegration'; \
	test ! -e Tests/ToolingIntegration/.void; \
	test ! -e Tests/ToolingIntegration/bin; \
	test ! -e Tests/ToolingIntegration/publish; \
	echo True
	@set -e; \
	./$(BIN) build Tests/ToolingIntegration >/dev/null; \
	grep -Fq '#line 8 "Tests/ToolingIntegration/Program.void"' Tests/ToolingIntegration/.void/ToolingIntegration.c; \
	$(DEBUG_LINE_DUMP) Tests/ToolingIntegration/bin/ToolingIntegration$(EXE_SUFFIX) 2>/dev/null | grep -Eq 'Program\.void[[:space:]]+8[[:space:]]'; \
	test "$$(./Tests/ToolingIntegration/bin/ToolingIntegration)" = '-5'; \
	echo True
	@set -e; \
	program_uri="$$( $(PYTHON) -c 'from pathlib import Path; print(Path("Tests/ToolingIntegration/Program.void").resolve().as_uri())')"; \
	player_uri="$$( $(PYTHON) -c 'from pathlib import Path; print(Path("Tests/ToolingIntegration/Player.void").resolve().as_uri())')"; \
	program_v1="$$(sed ':a;N;$$!ba;s/\\/\\\\/g;s/"/\\"/g;s/\n/\\n/g' Tests/ToolingIntegration/Program.void)"; \
	player_v1="$$(sed ':a;N;$$!ba;s/\\/\\\\/g;s/"/\\"/g;s/\n/\\n/g' Tests/ToolingIntegration/Player.void)"; \
	program_v2="$$(sed ':a;N;$$!ba;s/\\/\\\\/g;s/"/\\"/g;s/\n/\\n/g' Tests/ToolingIntegration/Edits/ProgramHeal.txt)"; \
	player_v2="$$(sed ':a;N;$$!ba;s/\\/\\\\/g;s/"/\\"/g;s/\n/\\n/g' Tests/ToolingIntegration/Edits/PlayerHeal.txt)"; \
	player_bad="$$(sed ':a;N;$$!ba;s/\\/\\\\/g;s/"/\\"/g;s/\n/\\n/g' Tests/ToolingIntegration/Edits/PlayerBroken.txt)"; \
	msg='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'; \
	output="$$( { printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"%s","languageId":"void","version":1,"text":"%s"}}}' "$$program_uri" "$$program_v1")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"%s","languageId":"void","version":1,"text":"%s"}}}' "$$player_uri" "$$player_v1")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":2,"method":"textDocument/hover","params":{"textDocument":{"uri":"%s"},"position":{"line":7,"character":16}}}' "$$program_uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":3,"method":"textDocument/signatureHelp","params":{"textDocument":{"uri":"%s"},"position":{"line":7,"character":23}}}' "$$program_uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":4,"method":"textDocument/completion","params":{"textDocument":{"uri":"%s"},"position":{"line":7,"character":15}}}' "$$program_uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":5,"method":"textDocument/definition","params":{"textDocument":{"uri":"%s"},"position":{"line":7,"character":16}}}' "$$program_uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":6,"method":"textDocument/references","params":{"textDocument":{"uri":"%s"},"position":{"line":7,"character":16},"context":{"includeDeclaration":true}}}' "$$program_uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":7,"method":"textDocument/documentSymbol","params":{"textDocument":{"uri":"%s"}}}' "$$player_uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","id":8,"method":"workspace/symbol","params":{"query":"Damage"}}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"%s","version":2},"contentChanges":[{"text":"%s"}]}}' "$$program_uri" "$$program_v2")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"%s","version":2},"contentChanges":[{"text":"%s"}]}}' "$$player_uri" "$$player_v2")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":9,"method":"textDocument/hover","params":{"textDocument":{"uri":"%s"},"position":{"line":7,"character":16}}}' "$$program_uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":10,"method":"textDocument/definition","params":{"textDocument":{"uri":"%s"},"position":{"line":7,"character":16}}}' "$$program_uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":11,"method":"textDocument/references","params":{"textDocument":{"uri":"%s"},"position":{"line":7,"character":16},"context":{"includeDeclaration":true}}}' "$$program_uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","id":12,"method":"workspace/symbol","params":{"query":"Heal"}}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"%s","version":3},"contentChanges":[{"text":"%s"}]}}' "$$player_uri" "$$player_bad")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":13,"method":"textDocument/hover","params":{"textDocument":{"uri":"%s"},"position":{"line":7,"character":16}}}' "$$program_uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"%s","version":4},"contentChanges":[{"text":"%s"}]}}' "$$player_uri" "$$player_v2")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":14,"method":"textDocument/hover","params":{"textDocument":{"uri":"%s"},"position":{"line":7,"character":16}}}' "$$program_uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didClose","params":{"textDocument":{"uri":"%s"}}}' "$$player_uri")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"%s","version":3},"contentChanges":[{"text":"%s"}]}}' "$$program_uri" "$$program_v1")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","id":15,"method":"shutdown","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","method":"exit","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; } | ./$(BIN) lsp )"; \
	printf '%s' "$$output" | grep -Fq '"serverInfo":{"name":"voidc","version":"0.0.360"}'; \
	printf '%s' "$$output" | grep -Fq '"hoverProvider":true'; \
	printf '%s' "$$output" | grep -Fq '"completionProvider":{"triggerCharacters":["."]}'; \
	printf '%s' "$$output" | grep -Fq '"definitionProvider":true,"referencesProvider":true,"documentSymbolProvider":true,"workspaceSymbolProvider":true'; \
	echo True; \
	printf '%s' "$$output" | grep -Fq '"id":2,"result":{"contents":{"kind":"plaintext","value":"method Damage(int amount): void"'; \
	printf '%s' "$$output" | grep -Fq '"id":3,"result":{"signatures":[{"label":"Damage(int amount): void"'; \
	echo True; \
	printf '%s' "$$output" | grep -Fq '"id":4,"result":{"isIncomplete":false,"items":[{"label":"Health","kind":5,"detail":"int"},{"label":"Damage","kind":2,"detail":"void"}]}}'; \
	echo True; \
	printf '%s' "$$output" | grep -Fq "\"id\":5,\"result\":{\"uri\":\"$$player_uri\",\"range\":{\"start\":{\"line\":4"; \
	printf '%s' "$$output" | grep -Fq '"id":6,"result":['; \
	printf '%s' "$$output" | grep -Fq '"id":7,"result":[{"name":"Player","kind":5'; \
	printf '%s' "$$output" | grep -Fq '"id":8,"result":[{"name":"Damage","kind":6'; \
	echo True; \
	test "$$(printf '%s' "$$output" | grep -Fo '"message":"type '\''Player'\'' has no matching instance method '\''Heal'\''"' | wc -l)" -eq 2; \
	echo True; \
	printf '%s' "$$output" | grep -Fq '"version":2,"diagnostics":[]'; \
	printf '%s' "$$output" | grep -Fq '"id":9,"result":{"contents":{"kind":"plaintext","value":"method Heal(int amount): void"'; \
	printf '%s' "$$output" | grep -Fq "\"id\":10,\"result\":{\"uri\":\"$$player_uri\",\"range\":{\"start\":{\"line\":9"; \
	printf '%s' "$$output" | grep -Fq '"id":11,"result":['; \
	printf '%s' "$$output" | grep -Fq '"id":12,"result":['; \
	printf '%s' "$$output" | grep -Fq '"name":"Heal","kind":6'; \
	echo True; \
	printf '%s' "$$output" | grep -Fq '"version":3,"diagnostics":[{"range":{"start":{"line":12,"character":4},"end":{"line":12,"character":4}},"severity":1,"source":"voidc","message":"expected '\'';'\'', found '\''}'\''"'; \
	printf '%s' "$$output" | grep -Fq '"id":13,"result":null'; \
	echo True; \
	printf '%s' "$$output" | grep -Fq '"version":4,"diagnostics":[]'; \
	printf '%s' "$$output" | grep -Fq '"id":14,"result":{"contents":{"kind":"plaintext","value":"method Heal(int amount): void"'; \
	echo True; \
	printf '%s' "$$output" | grep -Fq '"version":3,"diagnostics":[]'; \
	printf '%s' "$$output" | grep -Fq '"id":15,"result":null'; \
	echo True

test-base-constructors: $(BIN)
	@./$(BIN) build Tests/BaseConstructors >/dev/null
	@./Tests/BaseConstructors/bin/BaseConstructors
	@set -e; \
	output="$$(./$(BIN) check Tests/BaseConstructorDiagnostics/Missing 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "no matching base constructor for 'Base' was found"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/BaseConstructorDiagnostics/Root 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "constructor 'Root' cannot use a base constructor initializer because the type has no base class"; \
	echo True

test-constructor-delegation: $(BIN)
	@./$(BIN) build Tests/ConstructorDelegation >/dev/null
	@./Tests/ConstructorDelegation/bin/ConstructorDelegation
	@set -e; \
	output="$$(./$(BIN) check Tests/ConstructorDelegationDiagnostics/Cycle 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "constructor delegation cycle detected in 'Cycle'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/ConstructorDelegationDiagnostics/NoMatch 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "no matching delegated constructor for 'NoMatch' was found"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/ConstructorDelegationDiagnostics/ThisAccess 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "unknown identifier 'this'"; \
	echo True

test-interface-inheritance: $(BIN)
	@./$(BIN) build Tests/InterfaceInheritance >/dev/null
	@./Tests/InterfaceInheritance/bin/InterfaceInheritance
	@set -e; \
	output="$$(./$(BIN) check Tests/InterfaceInheritanceDiagnostics/Cycle 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "interface inheritance cycle involving 'IA'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/InterfaceInheritanceDiagnostics/NonInterface 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "interface 'IBad' can inherit only from interfaces"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/InterfaceInheritanceDiagnostics/Conflict 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "interface 'IBad' inherits conflicting method 'Get' return types"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/InterfaceInheritanceDiagnostics/PropertyConflict 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "interface 'IBad' inherits conflicting property 'Code' types"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/InterfaceInheritanceDiagnostics/Missing 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "class 'Broken' does not implement interface method 'IBase.BaseCall'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/InterfaceInheritanceDiagnostics/MissingProperty 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "class 'Broken' does not implement interface property 'IBase.Value'"; \
	echo True
	@set -e; \
	uri="$$( $(PYTHON) -c 'from pathlib import Path; print(Path("Tests/InterfaceInheritance/Program.void").resolve().as_uri())')"; \
	text="$$(sed ':a;N;$$!ba;s/\\/\\\\/g;s/"/\\"/g;s/\n/\\n/g' Tests/InterfaceInheritance/Program.void)"; \
	member_line="$$(awk '/diamond\.Value/{print NR-1; exit}' Tests/InterfaceInheritance/Program.void)"; \
	member_char="$$(awk '/diamond\.Value/{print index($$0,"Value")-1; exit}' Tests/InterfaceInheritance/Program.void)"; \
	msg='{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'; \
	output="$$( { printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"%s","languageId":"void","version":1,"text":"%s"}}}' "$$uri" "$$text")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg="$$(printf '{"jsonrpc":"2.0","id":2,"method":"textDocument/completion","params":{"textDocument":{"uri":"%s"},"position":{"line":%s,"character":%s}}}' "$$uri" "$$member_line" "$$member_char")"; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","id":3,"method":"shutdown","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; \
	msg='{"jsonrpc":"2.0","method":"exit","params":null}'; printf 'Content-Length: %d\r\n\r\n%s' "$${#msg}" "$$msg"; } | ./$(BIN) lsp )"; \
	printf '%s' "$$output" | grep -Fq '"label":"Sum"'; \
	printf '%s' "$$output" | grep -Fq '"label":"Left"'; \
	printf '%s' "$$output" | grep -Fq '"label":"Right"'; \
	printf '%s' "$$output" | grep -Fq '"label":"Value"'; \
	printf '%s' "$$output" | grep -Fq '"label":"Number"'; \
	echo True

test-struct-interfaces: $(BIN)
	@./$(BIN) build Tests/StructInterfaces >/dev/null
	@./Tests/StructInterfaces/bin/StructInterfaces
	@set -e; \
	output="$$(./$(BIN) check Tests/StructInterfaceDiagnostics/Missing 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "struct 'Broken' does not implement interface method 'IValue.Get'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/StructInterfaceDiagnostics/MissingProperty 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "struct 'Broken' does not implement interface property 'IValue.Value'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/StructInterfaceDiagnostics/NonInterface 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "struct 'Broken' can implement only interfaces"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/StructInterfaceDiagnostics/Inaccessible 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "struct 'Broken' does not implement interface method 'IValue.Get'"; \
	echo True

test-virtual-properties: $(BIN)
	@./$(BIN) build Tests/VirtualProperties >/dev/null
	@./Tests/VirtualProperties/bin/VirtualProperties
	@set -e; \
	output="$$(./$(BIN) check Tests/VirtualPropertyDiagnostics/NonVirtual 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "property 'Value' cannot override non-virtual base property"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/VirtualPropertyDiagnostics/Missing 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "property 'Value' is marked override but no matching base property exists"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/VirtualPropertyDiagnostics/Sealed 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "property 'Value' cannot override a sealed base property"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/VirtualPropertyDiagnostics/AbstractMissing 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "non-abstract class 'Broken' does not implement abstract property 'Value'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/VirtualPropertyDiagnostics/ReAbstractMissing 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "non-abstract class 'Broken' does not implement abstract property 'Value'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/VirtualPropertyDiagnostics/TypeMismatch 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "override property 'Value' must use type 'int'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/VirtualPropertyDiagnostics/AccessorShape 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "override property 'Value' must preserve the base accessor shape"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/VirtualPropertyDiagnostics/Static 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "virtual/override/abstract/sealed properties cannot be static"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/VirtualPropertyDiagnostics/AbstractBody 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "abstract property 'Value' accessors cannot have bodies"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/VirtualPropertyDiagnostics/PrivateAccess 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "getter for property 'Value' is inaccessible"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/VirtualPropertyDiagnostics/StructVirtual 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "virtual/override/abstract/sealed properties are supported on classes only"; \
	echo True

test-static-events: $(BIN)
	@./$(BIN) build Tests/StaticEvents >/dev/null
	@./Tests/StaticEvents/bin/StaticEvents
	@set -e; \
	output="$$(./$(BIN) check Tests/StaticEventDiagnostics/ExternalRead 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "event 'Changed' can only be used with += or -= outside its declaring type"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/StaticEventDiagnostics/ExternalInvoke 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "event 'Changed' can only be used with += or -= outside its declaring type"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/StaticEventDiagnostics/ExternalAssign 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "event 'Changed' can only be used with += or -= outside its declaring type"; \
	echo True
	@./$(BIN) check Tests/StaticEventDiagnostics/Initializer >/dev/null
	@echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/StaticEventDiagnostics/NonDelegate 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "event 'Changed' must use a delegate type"; \
	echo True

test-interface-events: $(BIN)
	@./$(BIN) build Tests/InterfaceEvents >/dev/null
	@./Tests/InterfaceEvents/bin/InterfaceEvents
	@set -e; \
	output="$$(./$(BIN) check Tests/InterfaceEventDiagnostics/Missing 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "does not implement interface event 'IEmitter.Changed'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/InterfaceEventDiagnostics/TypeMismatch 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "does not implement interface event 'IEmitter.Changed'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/InterfaceEventDiagnostics/Inaccessible 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "does not implement interface event 'IEmitter.Changed'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/InterfaceEventDiagnostics/InterfaceInitializer 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "interface event 'Changed' must be an instance signature ending in ';'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/InterfaceEventDiagnostics/Conflict 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "inherits conflicting event 'Changed' types"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/InterfaceEventDiagnostics/ExternalRead 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "event 'Changed' can only be used with += or -= outside its declaring type"; \
	echo True

test-delegate-completion: $(BIN)
	@./$(BIN) build Tests/DelegateCompletion >/dev/null
	@./Tests/DelegateCompletion/bin/DelegateCompletion
	@set -e; \
	output="$$(./$(BIN) check Tests/DelegateCompletionDiagnostics/MethodGroupMismatch 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "no method matches delegate signature 'RefAction'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/DelegateCompletionDiagnostics/InvokeMissingModifier 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "delegate 'RefAction' has no matching invocation for 1 argument(s)"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/DelegateCompletionDiagnostics/InvokeWrongModifier 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "delegate 'RefAction' has no matching invocation for 1 argument(s)"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/DelegateCompletionDiagnostics/EqualityMismatch 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "operator '==' is not defined for 'First' and 'Second'"; \
	echo True

test-conditional-expressions: $(BIN)
	@./$(BIN) build Tests/ConditionalExpressions >/dev/null
	@./Tests/ConditionalExpressions/bin/ConditionalExpressions
	@set -e; \
	output="$$(./$(BIN) check Tests/ConditionalExpressionDiagnostics/NonBool 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "conditional expression requires a bool condition, found 'int'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/ConditionalExpressionDiagnostics/NoCommonType 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "conditional branches of type 'int' and 'string' do not have a unique implicit result type"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/ConditionalExpressionDiagnostics/TargetMismatch 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "conditional branch of type 'int' cannot convert to target type 'Node'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/ConditionalExpressionDiagnostics/NoTarget 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "conditional expression has no type because both branches require a target type"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/ConditionalExpressionDiagnostics/DefiniteAssignment 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "out parameter 'value' must be assigned before returning"; \
	echo True

test-object-model-integration: $(BIN)
	@./$(BIN) check Tests/ObjectModelIntegration >/dev/null
	@./$(BIN) build Tests/ObjectModelIntegration >/dev/null
	@./Tests/ObjectModelIntegration/bin/ObjectModelIntegration
	@./$(BIN) publish Tests/ObjectModelIntegration >/dev/null


test-nested-value-structs: $(BIN)
	@./$(BIN) build Tests/NestedValueStructs >/dev/null
	@./Tests/NestedValueStructs/bin/NestedValueStructs
	@set -e; \
	output="$$(./$(BIN) check Tests/NestedValueStructDiagnostics/Direct 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "value-struct layout cycle involving 'Loop' and 'Loop'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/NestedValueStructDiagnostics/Indirect 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "value-struct layout cycle involving 'Right' and 'Left'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/NestedValueStructDiagnostics/Nullable 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "value-struct layout cycle involving 'MaybeLoop' and 'MaybeLoop'"; \
	echo True
	@set -e; \
	output="$$(./$(BIN) check Tests/NestedValueStructDiagnostics/Property 2>&1 || true)"; \
	printf '%s\n' "$$output" | grep -Fq "value-struct layout cycle involving 'PropertyLoop' and 'PropertyLoop'"; \
	echo True

test-readonly-structs: $(BIN)
	@./$(BIN) build Tests/ReadonlyStructs >/dev/null
	@./Tests/ReadonlyStructs/bin/ReadonlyStructs
	@set -e; output="$$(./$(BIN) check Tests/ReadonlyStructDiagnostics/MutableField 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "instance field 'Value' in readonly struct 'Bad' must be readonly"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ReadonlyStructDiagnostics/MethodMutation 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign through a readonly receiver"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ReadonlyStructDiagnostics/InNonReadonlyCall 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot call non-readonly method 'Add' through a readonly value receiver"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ReadonlyStructDiagnostics/ReadonlyFieldCall 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot call non-readonly method 'Add' through a readonly value receiver"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ReadonlyStructDiagnostics/RefEscape 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot pass readonly storage as ref"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ReadonlyStructDiagnostics/NonReadonlyProperty 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot read non-readonly property 'Current' through a readonly value receiver"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ReadonlyStructDiagnostics/InvalidReadonlyMethod 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "readonly method 'Read' requires an instance value struct"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ReadonlyStructDiagnostics/AutoSetter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "automatic property 'Value' in readonly struct 'Bad' cannot declare a setter"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ReadonlyStructDiagnostics/ReadonlySetter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "property setter 'Value' cannot be readonly"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ReadonlyStructDiagnostics/ReadonlyConstructor 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "constructor 'Bad' cannot be readonly"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ReadonlyStructDiagnostics/ReadonlyThisCall 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot call non-readonly method 'Add' through a readonly value receiver"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ReadonlyStructDiagnostics/NestedMutation 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign through a readonly receiver"; echo True

test-jagged-arrays: $(BIN)
	@./$(BIN) build Tests/JaggedArrays >/dev/null
	@./Tests/JaggedArrays/bin/JaggedArrays
	@set -e; output="$$(./$(BIN) check Tests/JaggedArrayDiagnostics/ElementMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign 'string[]' to array initializer element of type 'int[]'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/JaggedArrayDiagnostics/DepthMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign 'int[][]' to 'int[]'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/JaggedArrayDiagnostics/LengthType 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "array length must be 'int', got 'bool'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/JaggedArrayDiagnostics/SizedOrder 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "sized array dimension must appear before jagged array suffixes"; echo True

test-multidimensional-arrays: $(BIN)
	@./$(BIN) build Tests/MultidimensionalArrays >/dev/null
	@./Tests/MultidimensionalArrays/bin/MultidimensionalArrays
	@set -e; output="$$(./$(BIN) check Tests/MultidimensionalArrayDiagnostics/RankIndex 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "array type 'int[,]' requires 2 index(es), got 1"; echo True
	@set -e; output="$$(./$(BIN) check Tests/MultidimensionalArrayDiagnostics/IndexType 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "array index must be 'int', got 'bool'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/MultidimensionalArrayDiagnostics/LengthType 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "array length must be 'int', got 'bool'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/MultidimensionalArrayDiagnostics/AssignmentRank 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign 'int[,,]' to local 'values' of type 'int[,]'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/MultidimensionalArrayDiagnostics/GetLength 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "array GetLength expects exactly one int dimension"; echo True
	@set -e; output="$$(./$(BIN) check Tests/MultidimensionalArrayDiagnostics/MalformedType 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "only one rectangular array suffix is supported before jagged suffixes"; echo True
	@set -e; output="$$(./$(BIN) check Tests/MultidimensionalArrayDiagnostics/InitializerLiteral 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "rectangular array initializer requires nested braces for dimension 1"; echo True
	@set -e; output="$$(./$(BIN) run Tests/MultidimensionalArrayRuntime/IndexBounds 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "VOID runtime error: array index out of range"; echo True
	@set -e; output="$$(./$(BIN) run Tests/MultidimensionalArrayRuntime/DimensionBounds 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "VOID runtime error: array dimension out of range"; echo True
	@set -e; output="$$(./$(BIN) run Tests/MultidimensionalArrayRuntime/NegativeLength 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "VOID runtime error: array length cannot be negative"; echo True

test-closure-completion-i: $(BIN)
	@./$(BIN) build Tests/ClosureCompletionI >/dev/null
	@./Tests/ClosureCompletionI/bin/ClosureCompletionI

test-closure-completion-ii: $(BIN)
	@./$(BIN) build Tests/ClosureCompletionII >/dev/null
	@./Tests/ClosureCompletionII/bin/ClosureCompletionII
	@set -e; output="$$(./$(BIN) check Tests/ClosureCompletionIIDiagnostics/OutMissingAssignment 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "out parameter 'value' must be assigned before returning"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ClosureCompletionIIDiagnostics/InMutation 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign to in parameter 'value'"; echo True

test-iterator-completion: $(BIN)
	@./$(BIN) build Tests/IteratorCompletion >/dev/null
	@./Tests/IteratorCompletion/bin/IteratorCompletion

test-unsafe-member-completion: $(BIN)
	@./$(BIN) build Tests/UnsafeMemberCompletion >/dev/null
	@./Tests/UnsafeMemberCompletion/bin/UnsafeMemberCompletion
	@set -e; output="$$(./$(BIN) check Tests/UnsafeMemberCompletionDiagnostics/SafeProject 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unsafe struct 'Bad' requires compiler unsafe to be enabled"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafeMemberCompletionDiagnostics/SafeStructProperty 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "pointer property 'Pointer' requires an unsafe containing type"; echo True
	@./$(BIN) check Tests/UnsafeMemberCompletionDiagnostics/ClassProperty >/dev/null; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafeMemberCompletionDiagnostics/SafeConstructor 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "pointer constructor parameter 'pointer' requires an unsafe constructor or unsafe type"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafeMemberCompletionDiagnostics/UnsafeConstructorSafeProject 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unsafe constructor 'Bad' requires compiler unsafe to be enabled"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafeMemberCompletionDiagnostics/UnsupportedProperty 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "pointer property type is not supported"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafeMemberCompletionDiagnostics/UnsupportedConstructor 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "pointer constructor parameter type is not supported"; echo True
	@./$(BIN) check Tests/UnsafeMemberCompletionDiagnostics/InterfaceProperty >/dev/null; echo True

test-compound-targets: $(BIN)
	@./$(BIN) build Tests/CompoundTargets >/dev/null
	@./Tests/CompoundTargets/bin/CompoundTargets
	@set -e; output="$$(./$(BIN) check Tests/CompoundTargetDiagnostics/MissingSetter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type 'Box' does not define a matching index setter"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CompoundTargetDiagnostics/MissingIndexerOperator 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator '+' is not defined for 'Value' and 'Value'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CompoundTargetDiagnostics/DuplicateSwizzle 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "swizzle assignment 'xx' cannot write the same component more than once"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CompoundTargetDiagnostics/NonWritableSwizzle 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "swizzle assignment requires a writable struct variable or field"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CompoundTargetDiagnostics/MissingSwizzleOperator 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator '+' is not defined for 'V2' and 'V2'"; echo True

test-value-types-closures-control-flow-integration: $(BIN)
	@./$(BIN) check Tests/ValueTypesClosuresControlFlowIntegration >/dev/null
	@./$(BIN) build Tests/ValueTypesClosuresControlFlowIntegration >/dev/null
	@./Tests/ValueTypesClosuresControlFlowIntegration/bin/ValueTypesClosuresControlFlowIntegration
	@./$(BIN) publish Tests/ValueTypesClosuresControlFlowIntegration >/dev/null


test-static-interface-properties: $(BIN)
	@./$(BIN) build Tests/StaticInterfaceProperties >/dev/null
	@./Tests/StaticInterfaceProperties/bin/StaticInterfaceProperties
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfacePropertyDiagnostics/Missing 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface property 'IValue.Value'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfacePropertyDiagnostics/InstanceInstead 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface property 'IValue.Value'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfacePropertyDiagnostics/MissingSetter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface property 'IValue.Value'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfacePropertyDiagnostics/PrivateSetter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface property 'IValue.Value'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfacePropertyDiagnostics/PrivateGetter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface property 'IValue.Value'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfacePropertyDiagnostics/TypeMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface property 'IValue.Value'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfacePropertyDiagnostics/DirectAccess 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static interface property 'IValue.Value' is a contract and must be accessed through an implementing type"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfacePropertyDiagnostics/Body 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static interface property 'Value' must be a property signature ending in ';'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfacePropertyDiagnostics/KindConflict 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "inherits conflicting static and instance property 'Value' contracts"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfacePropertyDiagnostics/NonPublic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface property 'IValue.Value'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfacePropertyDiagnostics/InterfaceInitializer 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "property initializer for 'Value' requires an automatic property"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfacePropertyDiagnostics/InterfaceAccessorModifier 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static interface property 'Value' accessors cannot declare modifiers"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfacePropertyDiagnostics/InterfacePrivate 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static interface property 'Value' may only use public and static modifiers"; echo True

test-static-interface-events: $(BIN)
	@./$(BIN) build Tests/StaticInterfaceEvents >/dev/null
	@./Tests/StaticInterfaceEvents/bin/StaticInterfaceEvents
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceEventDiagnostics/Missing 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface event 'IHub.Changed'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceEventDiagnostics/InstanceInstead 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface event 'IHub.Changed'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceEventDiagnostics/NonPublic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface event 'IHub.Changed'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceEventDiagnostics/TypeMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface event 'IHub.Changed'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceEventDiagnostics/DirectAccess 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static interface event 'IHub.Changed' is a contract and must be accessed through an implementing type"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceEventDiagnostics/Initializer 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static interface event 'Changed' cannot have an initializer"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceEventDiagnostics/InterfacePrivate 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static interface event 'Changed' may only use public and static modifiers"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceEventDiagnostics/KindConflict 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "inherits conflicting static and instance event 'Changed' contracts"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceEventDiagnostics/TypeConflict 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "inherits conflicting event 'Changed' types"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceEventDiagnostics/NonDelegate 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "event 'Changed' must use a delegate type"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceEventDiagnostics/InstanceContractStaticImplementation 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface event 'IHub.Changed'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceEventDiagnostics/StructInstanceEvent 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "instance events are supported on classes only"; echo True

test-constructor-returns: $(BIN)
	@./$(BIN) build Tests/ConstructorReturns >/dev/null
	@./Tests/ConstructorReturns/bin/ConstructorReturns
	@set -e; output="$$(./$(BIN) check Tests/ConstructorReturnDiagnostics/ReturnValueClass 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "constructors cannot return a value"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstructorReturnDiagnostics/ReturnValueStruct 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "constructors cannot return a value"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstructorReturnDiagnostics/OutBeforeReturn 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "out parameter 'value' must be assigned before returning"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstructorReturnDiagnostics/OutMissingPath 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "out parameter 'value' must be assigned before returning"; echo True

test-rectangular-array-initializers: $(BIN)
	@./$(BIN) build Tests/RectangularArrayInitializers >/dev/null
	@./Tests/RectangularArrayInitializers/bin/RectangularArrayInitializers
	@set -e; output="$$(./$(BIN) check Tests/RectangularArrayInitializerDiagnostics/Ragged 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "rectangular array initializer has inconsistent length in dimension 1"; echo True
	@set -e; output="$$(./$(BIN) check Tests/RectangularArrayInitializerDiagnostics/Shallow 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "rectangular array initializer requires nested braces for dimension 1"; echo True
	@set -e; output="$$(./$(BIN) check Tests/RectangularArrayInitializerDiagnostics/Deep 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "rectangular array initializer has too many nested dimensions"; echo True
	@set -e; output="$$(./$(BIN) check Tests/RectangularArrayInitializerDiagnostics/TypeMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign 'string' to rectangular array initializer element of type 'int'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/RectangularArrayInitializerDiagnostics/Rank3OuterRagged 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "rectangular array initializer has inconsistent length in dimension 1"; echo True
	@set -e; output="$$(./$(BIN) check Tests/RectangularArrayInitializerDiagnostics/Rank3InnerRagged 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "rectangular array initializer has inconsistent length in dimension 2"; echo True
	@set -e; output="$$(./$(BIN) check Tests/RectangularArrayInitializerDiagnostics/ExplicitLengths 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "sized rectangular array initializer dimension 0 is 1 but initializer requires 2"; echo True


test-iterator-locals: $(BIN)
	@./$(BIN) build Tests/IteratorLocals >/dev/null
	@./Tests/IteratorLocals/bin/IteratorLocals
	@set -e; output="$$(./$(BIN) check Tests/IteratorLocalDiagnostics/ReadBeforeAssignment 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "iterator local 'value' cannot be read before it is assigned"; echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorLocalDiagnostics/PartialAssignment 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "iterator local 'value' cannot be read before it is assigned"; echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorLocalDiagnostics/CompoundBeforeAssignment 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "iterator local 'value' must be assigned before compound assignment"; echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorLocalDiagnostics/VarWithoutInitializer 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "'var' requires an initializer with an inferable value type"; echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorLocalDiagnostics/VarNull 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "'var' requires an initializer with an inferable value type"; echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorLocalDiagnostics/SameScopeDuplicate 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "a local or parameter named 'value' is already declared in this scope"; echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorLocalDiagnostics/Stackalloc 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "stackalloc local 'values' cannot be used in an iterator method"; echo True

test-receiver-expressions: $(BIN)
	@./$(BIN) build Tests/ReceiverExpressions >/dev/null
	@./Tests/ReceiverExpressions/bin/ReceiverExpressions
	@set -e; output="$$(./$(BIN) check Tests/ReceiverExpressionDiagnostics/ReadonlyParenthesized 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot call non-readonly method 'Add' through a readonly value receiver"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ReceiverExpressionDiagnostics/MissingMethod 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type 'Node' has no matching instance method 'Missing'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ReceiverExpressionDiagnostics/NullConditionalValue 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator '?.' requires a reference receiver, got 'Counter'"; echo True

test-generic-method-calls: $(BIN)
	@./$(BIN) build Tests/GenericMethodCalls >/dev/null
	@./Tests/GenericMethodCalls/bin/GenericMethodCalls
	@set -e; output="$$(./$(BIN) check Tests/GenericMethodCallDiagnostics/WrongArity 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "generic method 'Echo' with 2 type argument(s) was not found"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericMethodCallDiagnostics/MissingReceiver 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type 'Plain' has no matching instance method 'Echo'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericMethodCallDiagnostics/StaticAsInstance 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type 'Worker' has no matching instance method 'Echo'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericMethodCallDiagnostics/InstanceAsStatic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "instance method 'Worker.Echo' requires an instance receiver"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericMethodCallDiagnostics/ArgumentMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type 'Worker' has no matching instance method 'Echo'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericMethodCallDiagnostics/Ambiguous 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "call to instance method 'Echo' is ambiguous"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericMethodCallDiagnostics/Inaccessible 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "method 'Hidden' is inaccessible"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericMethodCallDiagnostics/UnknownNamed 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "instance method 'Worker.Echo' has no parameter named 'missing'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericMethodCallDiagnostics/ReadonlyReceiver 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot call non-readonly method 'Add' through a readonly value receiver"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericMethodCallDiagnostics/InterfaceMissing 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "class 'Worker' does not implement interface method 'IWorker.Echo'"; echo True

test-native-callback-by-ref: $(BIN) $(NATIVE_CALLBACK_BYREF_FIXTURE)
	@./$(BIN) build Tests/NativeCallbackByRef >/dev/null
	@./Tests/NativeCallbackByRef/bin/NativeCallbackByRef
	@set -e; output="$$(./$(BIN) check Tests/NativeCallbackByRefDiagnostics/UnsupportedManaged 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "native callback delegate has a signature that is not supported by the C ABI"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NativeCallbackByRefDiagnostics/UnsupportedReturn 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "native callback delegate has a signature that is not supported by the C ABI"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NativeCallbackByRefDiagnostics/OuterByRefDelegate 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "native callback delegate parameters cannot themselves use ref, out, or in"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NativeCallbackByRefDiagnostics/ModifierMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "no method matches delegate signature 'RefIntCallback'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NativeCallbackByRefDiagnostics/NestedDelegate 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "native callback delegate has a signature that is not supported by the C ABI"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NativeCallbackByRefDiagnostics/DelegateVariable 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "native callback arguments require a direct static method group"; echo True

test-library-output: $(BIN)
	@./$(BIN) check Tests/LibraryOutput/Library >/dev/null; echo True
	@./$(BIN) build Tests/LibraryOutput/Library >/dev/null
	@test -f Tests/LibraryOutput/Library/bin/libVoid099Lib.a; echo True
	@set -e; symbols="$$(nm -g --defined-only Tests/LibraryOutput/Library/bin/libVoid099Lib.a)"; printf '%s\n' "$$symbols" | grep -Fq ' void099_add'; printf '%s\n' "$$symbols" | grep -Fq ' void099_adjust'; printf '%s\n' "$$symbols" | grep -Fq ' void099_gc_value'; ! printf '%s\n' "$$symbols" | grep -Fq ' vc_m_'; echo True
	@mkdir -p Tests/LibraryOutput/Consumer/native
	@cp Tests/LibraryOutput/Library/bin/libVoid099Lib.a Tests/LibraryOutput/Consumer/native/libVoid099Lib.a
	@./$(BIN) build Tests/LibraryOutput/Consumer >/dev/null
	@./Tests/LibraryOutput/Consumer/bin/LibraryConsumer
	@$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror Tests/LibraryOutput/CConsumer/main.c Tests/LibraryOutput/Library/bin/libVoid099Lib.a -o Tests/LibraryOutput/CConsumer/library-consumer
	@./Tests/LibraryOutput/CConsumer/library-consumer
	@./$(BIN) publish Tests/LibraryOutput/Library >/dev/null
	@test -f Tests/LibraryOutput/Library/publish/libVoid099Lib.a; echo True
	@set -e; output="$$(./$(BIN) run Tests/LibraryOutput/Library 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'library projects cannot be run directly'; echo True
	@set -e; output="$$(./$(BIN) check Tests/LibraryOutputDiagnostics/ExecutableExport 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq '[Export] may be used only in a library project'; echo True
	@set -e; output="$$(./$(BIN) check Tests/LibraryOutputDiagnostics/Instance 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq '[Export] requires a public static implemented method'; echo True
	@set -e; output="$$(./$(BIN) check Tests/LibraryOutputDiagnostics/Private 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq '[Export] requires a public static implemented method'; echo True
	@set -e; output="$$(./$(BIN) check Tests/LibraryOutputDiagnostics/Extern 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq '[Export] requires a public static implemented method'; echo True
	@set -e; output="$$(./$(BIN) check Tests/LibraryOutputDiagnostics/Generic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "generic method 'Echo' cannot be exported directly"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LibraryOutputDiagnostics/BadSymbol 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "[Export] symbol 'bad-symbol' is not a valid C identifier"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LibraryOutputDiagnostics/Duplicate 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "export symbol 'same_symbol' is already declared"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LibraryOutputDiagnostics/ManagedReturn 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "return type that is not supported by the native library ABI"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LibraryOutputDiagnostics/ManagedParameter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "parameter type or modifier is not supported by the native library ABI"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LibraryOutputDiagnostics/Constructor 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'constructors cannot be exported as native library symbols'; echo True
	@set -e; output="$$(./$(BIN) check Tests/LibraryOutputDiagnostics/MissingArgument 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq '[Export] requires exactly one C symbol string'; echo True
	@set -e; output="$$(./$(BIN) check Tests/LibraryOutputDiagnostics/NonString 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq '[Export] symbol must be a string literal'; echo True

test-language-edge-project-completion-integration: $(BIN) $(LANGUAGE_EDGE_INTEGRATION_NATIVE_FIXTURE)
	@./$(BIN) check Tests/LanguageEdgeProjectCompletionIntegration/Library >/dev/null; echo True
	@./$(BIN) build Tests/LanguageEdgeProjectCompletionIntegration/Library >/dev/null
	@test -f Tests/LanguageEdgeProjectCompletionIntegration/Library/bin/libVoid100Lib.a; echo True
	@set -e; symbols="$$(nm -g --defined-only Tests/LanguageEdgeProjectCompletionIntegration/Library/bin/libVoid100Lib.a)"; printf '%s\n' "$$symbols" | grep -Fq ' void100_run_core'; printf '%s\n' "$$symbols" | grep -Fq ' void100_adjust'; ! printf '%s\n' "$$symbols" | grep -Fq ' vc_m_'; echo True
	@cp Tests/LanguageEdgeProjectCompletionIntegration/Library/bin/libVoid100Lib.a Tests/LanguageEdgeProjectCompletionIntegration/Consumer/native/libVoid100Lib.a
	@./$(BIN) check Tests/LanguageEdgeProjectCompletionIntegration/Consumer >/dev/null; echo True
	@./$(BIN) build Tests/LanguageEdgeProjectCompletionIntegration/Consumer >/dev/null
	@./Tests/LanguageEdgeProjectCompletionIntegration/Consumer/bin/LanguageEdgeProjectCompletionIntegration
	@./$(BIN) publish Tests/LanguageEdgeProjectCompletionIntegration/Library >/dev/null
	@test -f Tests/LanguageEdgeProjectCompletionIntegration/Library/publish/libVoid100Lib.a; echo True
	@cp Tests/LanguageEdgeProjectCompletionIntegration/Library/publish/libVoid100Lib.a Tests/LanguageEdgeProjectCompletionIntegration/Consumer/native/libVoid100Lib.a
	@./$(BIN) publish Tests/LanguageEdgeProjectCompletionIntegration/Consumer >/dev/null
	@set -e; test -f Tests/LanguageEdgeProjectCompletionIntegration/Consumer/publish/LanguageEdgeProjectCompletionIntegration$(EXE_SUFFIX); echo True
	@./Tests/LanguageEdgeProjectCompletionIntegration/Consumer/publish/LanguageEdgeProjectCompletionIntegration

test-unsafe-delegates: $(BIN) $(UNSAFE_DELEGATE_NATIVE_FIXTURE)
	@./$(BIN) build Tests/UnsafeDelegates >/dev/null
	@./Tests/UnsafeDelegates/bin/UnsafeDelegates
	@set -e; output="$$(./$(BIN) check Tests/UnsafeDelegateDiagnostics/SafeProject 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unsafe delegate 'Bad' requires compiler unsafe to be enabled"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafeDelegateDiagnostics/MissingUnsafe 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "pointer signatures require an unsafe method in an unsafe-enabled project"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafeDelegateDiagnostics/ManagedPointer 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "pointer parameter type is not supported"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafeDelegateDiagnostics/ManagedReturn 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "pointer return type is not supported"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafeDelegateDiagnostics/ModifierMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "no method matches delegate signature 'PointerRef'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafeDelegateDiagnostics/OuterByRef 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "native callback delegate parameters cannot themselves use ref, out, or in"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafeDelegateDiagnostics/NativeDelegateVariable 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "native callback arguments require a direct static method group"; echo True

# Milestone 102: unsafe pointer properties on classes and interfaces
test-unsafe-pointer-properties: $(BIN)
	@./$(BIN) build Tests/UnsafePointerProperties >/dev/null
	@./Tests/UnsafePointerProperties/bin/UnsafePointerProperties
	@set -e; output="$$(./$(BIN) check Tests/UnsafePointerPropertyDiagnostics/SafeClass 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "pointer property 'Pointer' requires an unsafe containing type"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafePointerPropertyDiagnostics/SafeInterface 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "pointer property 'Pointer' requires an unsafe containing type"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafePointerPropertyDiagnostics/UnsafeClassSafeProject 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unsafe class 'Bad' requires compiler unsafe to be enabled"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafePointerPropertyDiagnostics/UnsafeInterfaceSafeProject 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unsafe interface 'Bad' requires compiler unsafe to be enabled"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafePointerPropertyDiagnostics/ManagedClassPointer 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "pointer property type is not supported"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafePointerPropertyDiagnostics/ManagedInterfacePointer 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "pointer property type is not supported"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafePointerPropertyDiagnostics/MissingImplementation 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface property 'IRequired.Pointer'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafePointerPropertyDiagnostics/StaticMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface property 'IRequired.Pointer'"; echo True

# Milestone 103: do-while statements and iterator lowering
test-do-while: $(BIN)
	@./$(BIN) build Tests/DoWhileCompletion >/dev/null
	@./Tests/DoWhileCompletion/bin/DoWhileCompletion
	@set -e; output="$$(./$(BIN) check Tests/DoWhileDiagnostics/NonBool 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "do-while condition must be bool, got 'int'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/DoWhileDiagnostics/MissingWhile 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "expected 'while', found '('"; echo True
	@set -e; output="$$(./$(BIN) check Tests/DoWhileDiagnostics/MissingSemicolon 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "expected ';', found '}'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/DoWhileDiagnostics/MissingLeftParen 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "expected '(', found 'false'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/DoWhileDiagnostics/IteratorContinueUnassigned 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "iterator local 'value' cannot be read before it is assigned"; echo True
	@set -e; output="$$(./$(BIN) check Tests/DoWhileDiagnostics/IteratorBreakUnassigned 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "iterator local 'value' cannot be read before it is assigned"; echo True

# Milestone 104: expression-bodied member completion
test-expression-bodied-members: $(BIN)
	@./$(BIN) build Tests/ExpressionBodiedMembers >/dev/null
	@./Tests/ExpressionBodiedMembers/bin/ExpressionBodiedMembers
	@set -e; output="$$(./$(BIN) check Tests/ExpressionBodiedMemberDiagnostics/ReturnTypeMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "method returns 'int' but return statement provides 'string'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExpressionBodiedMemberDiagnostics/OutMissingAssignment 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "out parameter 'value' must be assigned before returning"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExpressionBodiedMemberDiagnostics/AbstractBody 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "abstract method 'Value' cannot declare a body"; echo True
	@./$(BIN) check Tests/ExpressionBodiedMemberDiagnostics/InterfaceBody >/dev/null; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExpressionBodiedMemberDiagnostics/MissingSemicolon 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "expected ';', found '}'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExpressionBodiedMemberDiagnostics/SetterMissingSemicolon 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "expected ';', found '}'"; echo True


# Milestone 105: custom event accessor completion
test-custom-event-accessors: $(BIN)
	@./$(BIN) build Tests/CustomEventAccessors >/dev/null
	@./Tests/CustomEventAccessors/bin/CustomEventAccessors
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/AbstractBody 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "abstract event 'Changed' accessors cannot have bodies"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/AbstractMissing 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "non-abstract class 'Bad' does not implement abstract event 'Changed'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/DuplicateAdd 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "event already has an add accessor"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/Initializer 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "custom event 'Changed' cannot have an initializer"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/InterfaceBody 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "interface event 'Changed' custom accessors must be add/remove declarations ending in ';'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/InternalAssign 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "custom event 'Changed' can only be used with += or -="; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/InternalInvoke 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "custom event 'Changed' can only be used with += or -="; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/InternalRead 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "custom event 'Changed' can only be used with += or -="; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/MissingBody 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "custom event 'Changed' add and remove accessors must have bodies"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/MissingRemove 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "custom event 'Changed' must declare both add and remove accessors"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/MissingSemicolon 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "expected ';', found 'identifier'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/NonDelegate 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "event 'Changed' must use a delegate type"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/OverrideNonVirtual 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "event 'Changed' cannot override non-virtual base event"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/PrivateExternal 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "field 'Changed' is inaccessible"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/SealedOverride 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "event 'Changed' cannot override a sealed base event"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/StaticVirtual 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "virtual/override/abstract/sealed events cannot be static"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/UnknownAccessor 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "expected 'add' or 'remove' event accessor"; echo True
	@set -e; output="$$(./$(BIN) check Tests/CustomEventAccessorDiagnostics/WrongDelegate 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "event subscription expects 'Action<int>', got 'TextHandler'"; echo True

# Milestone 106: null-conditional array completion
test-null-conditional-arrays: $(BIN)
	@./$(BIN) build Tests/NullConditionalArrays >/dev/null
	@./Tests/NullConditionalArrays/bin/NullConditionalArrays
	@set -e; output="$$(./$(BIN) check Tests/NullConditionalArrayDiagnostics/AssignmentTarget 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "null-conditional indexing cannot be used as an assignment target"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NullConditionalArrayDiagnostics/CompoundTarget 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "null-conditional indexing cannot be used as an assignment target"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NullConditionalArrayDiagnostics/IncrementTarget 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "null-conditional indexing cannot be used as an increment/decrement target"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NullConditionalArrayDiagnostics/MemberAssignmentTarget 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "null-conditional access cannot be used as an assignment target"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NullConditionalArrayDiagnostics/RankTooFew 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "array type 'int[,]' requires 2 index(es), got 1"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NullConditionalArrayDiagnostics/RankTooMany 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "array type 'int[]' requires 1 index(es), got 2"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NullConditionalArrayDiagnostics/IndexType 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "array index must be 'int', got 'bool'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NullConditionalArrayDiagnostics/RefArgument 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "ref argument requires a variable or writable field"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NullConditionalArrayDiagnostics/ValueReceiver 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator '?[]' requires a reference or nullable value receiver, got 'Value'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NullConditionalArrayDiagnostics/MissingBracket 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "expected ']', found ';'"; echo True

# Milestone 107: sized rectangular array initializer completion
test-sized-rectangular-array-initializers: $(BIN)
	@./$(BIN) build Tests/SizedRectangularArrayInitializers >/dev/null
	@./Tests/SizedRectangularArrayInitializers/bin/SizedRectangularArrayInitializers
	@set -e; output="$$(./$(BIN) check Tests/SizedRectangularArrayInitializerDiagnostics/NonConstant 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "sized rectangular array initializer dimension 0 must be a compile-time int constant"; echo True
	@set -e; output="$$(./$(BIN) check Tests/SizedRectangularArrayInitializerDiagnostics/MismatchFirst 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "sized rectangular array initializer dimension 0 is 1 but initializer requires 2"; echo True
	@set -e; output="$$(./$(BIN) check Tests/SizedRectangularArrayInitializerDiagnostics/MismatchSecond 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "sized rectangular array initializer dimension 1 is 3 but initializer requires 2"; echo True
	@set -e; output="$$(./$(BIN) check Tests/SizedRectangularArrayInitializerDiagnostics/Negative 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "sized rectangular array initializer dimension 0 cannot be negative"; echo True
	@set -e; output="$$(./$(BIN) check Tests/SizedRectangularArrayInitializerDiagnostics/WrongType 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "array length must be 'int', got 'bool'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/SizedRectangularArrayInitializerDiagnostics/Ragged 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "rectangular array initializer has inconsistent length in dimension 1"; echo True
	@set -e; output="$$(./$(BIN) check Tests/SizedRectangularArrayInitializerDiagnostics/ConstMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "sized rectangular array initializer dimension 1 is 3 but initializer requires 2"; echo True

# Milestone 108: implicit rectangular array initializer completion
test-implicit-rectangular-array-initializers: $(BIN)
	@./$(BIN) build Tests/ImplicitRectangularArrayInitializers >/dev/null
	@./Tests/ImplicitRectangularArrayInitializers/bin/ImplicitRectangularArrayInitializers
	@set -e; output="$$(./$(BIN) check Tests/ImplicitRectangularArrayInitializerDiagnostics/Ragged 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "rectangular array initializer has inconsistent length in dimension 1"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ImplicitRectangularArrayInitializerDiagnostics/MissingGroup 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "rectangular array initializer requires nested braces for dimension 1"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ImplicitRectangularArrayInitializerDiagnostics/ExtraGroup 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "rectangular array initializer has too many nested dimensions"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ImplicitRectangularArrayInitializerDiagnostics/RankMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit rectangular array rank 2 does not match target array rank 3"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ImplicitRectangularArrayInitializerDiagnostics/RankOneTarget 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit rectangular array rank 2 does not match target array rank 1"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ImplicitRectangularArrayInitializerDiagnostics/ElementMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign 'bool' to rectangular array initializer element of type 'int'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ImplicitRectangularArrayInitializerDiagnostics/TargetElementMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign 'int' to rectangular array initializer element of type 'string'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ImplicitRectangularArrayInitializerDiagnostics/EmptyInference 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicitly typed rectangular array initializer requires at least one value"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ImplicitRectangularArrayInitializerDiagnostics/NullInference 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "could not infer rectangular array element type from initializer"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ImplicitRectangularArrayInitializerDiagnostics/MissingInitializer 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "expected '{', found ';'"; echo True

# Milestone 109: generic constraint completion
test-generic-constraints: $(BIN)
	@./$(BIN) build Tests/GenericConstraints >/dev/null
	@./Tests/GenericConstraints/bin/GenericConstraints
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/ClassValue 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type argument 'int' must be a reference type for generic parameter 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/MethodClassValue 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type argument 'int' must be a reference type for generic parameter 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/StructReference 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type argument 'Node' must be a non-nullable value type for generic parameter 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/StructNullable 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type argument 'int?' must be a non-nullable value type for generic parameter 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/InterfaceMissing 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type argument 'Node' does not satisfy constraint 'IMark' for generic parameter 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/BaseMissing 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type argument 'Other' does not satisfy constraint 'Base' for generic parameter 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/NewMissing 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type argument 'Node' must have a public parameterless constructor for generic parameter 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/NewPrivate 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type argument 'Node' must have a public parameterless constructor for generic parameter 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/NewAbstract 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type argument 'Node' must have a public parameterless constructor for generic parameter 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/ParameterRelation 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type argument 'Other' does not satisfy constraint 'Base' for generic parameter 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/UnknownParameter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "generic constraint refers to unknown parameter 'U'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/DuplicateWhere 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "generic parameter 'T' has more than one where clause"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/DuplicateGenericParameter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "duplicate generic parameter 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/NewNotLast 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "'new()' constraint for 'T' must appear last"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/ClassNotFirst 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "'class' constraint for 'T' must appear first"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/StructNewConflict 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "'new()' cannot be combined with the 'struct' constraint for 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/DuplicateTypeConstraint 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "generic parameter 'T' contains a duplicate type constraint 'IMark'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/ValueTypeConstraint 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "constraint 'Point' for generic parameter 'T' must be a class or interface type"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/SealedConstraint 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "sealed class 'Base' cannot be used as a generic constraint"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/BaseAfterInterface 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "base class constraint 'Base' for 'T' must appear first and cannot be combined with 'class'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/ClassAndBase 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "base class constraint 'Base' for 'T' must appear first and cannot be combined with 'class'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConstraintDiagnostics/UnknownConstraintType 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "generic constraint type 'IMissing' for 'T' could not be resolved"; echo True

# Milestone 110: Language Surface Completion II integration
test-language-surface-completion-ii-integration: $(BIN)
	@./$(BIN) build Tests/LanguageSurfaceCompletionIIIntegration >/dev/null
	@./Tests/LanguageSurfaceCompletionIIIntegration/bin/LanguageSurfaceCompletionIIIntegration
	@set -e; output="$$(./$(BIN) check Tests/LanguageSurfaceCompletionIIIntegrationDiagnostics/RectangularStructConstraint 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type argument 'int[,]' must be a non-nullable value type for generic parameter 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LanguageSurfaceCompletionIIIntegrationDiagnostics/ConditionalRectangularWrite 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "null-conditional indexing cannot be used as an assignment target"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LanguageSurfaceCompletionIIIntegrationDiagnostics/GenericCustomEventRead 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "custom event 'Changed' can only be used with += or -="; echo True
	@set -e; output="$$(./$(BIN) check Tests/LanguageSurfaceCompletionIIIntegrationDiagnostics/UnsafePointerSafeProject 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unsafe delegate 'Step' requires compiler unsafe to be enabled"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LanguageSurfaceCompletionIIIntegrationDiagnostics/ImplicitRectangularRagged 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "rectangular array initializer has inconsistent length in dimension 1"; echo True

# Milestone 111: Exception & Throw Foundation
test-exception-throw-foundation: $(BIN)
	@output="$$(./$(BIN) parse Tests/ExceptionThrowUnhandled/Program.void)"; printf '%s\n' "$$output" | grep -Fq 'Throw'; echo True
	@./$(BIN) build Tests/ExceptionThrowFoundation >/dev/null
	@./Tests/ExceptionThrowFoundation/bin/ExceptionThrowFoundation
	@set -e; ./$(BIN) publish Tests/ExceptionThrowFoundation >/dev/null; test -x Tests/ExceptionThrowFoundation/publish/ExceptionThrowFoundation$(EXE_SUFFIX); echo True
	@./$(BIN) build Tests/ExceptionThrowUnhandled >/dev/null
	@set -e; set +e; output="$$(./Tests/ExceptionThrowUnhandled/bin/ExceptionThrowUnhandled 2>&1)"; status=$$?; set -e; test $$status -eq 1; echo True; printf '%s\n' "$$output" | grep -Fxq 'Unhandled FatalGameException: boom'; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExceptionThrowDiagnostics/ThrowInt 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "throw expression must be Exception or a derived class, got 'int'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExceptionThrowDiagnostics/ThrowObject 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "throw expression must be Exception or a derived class, got 'Node'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExceptionThrowDiagnostics/ThrowNull 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "throw expression must be Exception or a derived class, got 'null'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExceptionThrowDiagnostics/BareThrow 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "bare 'throw;' can only be used inside a catch block"; echo True
	@./$(BIN) check Tests/ExceptionThrowDiagnostics/IteratorThrow >/dev/null; echo True

# Milestone 112: Try/Catch Completion
test-try-catch-completion: $(BIN)
	@output="$$(./$(BIN) parse Tests/TryCatchCompletion/Program.void)"; printf '%s\n' "$$output" | grep -Fq 'Try'; echo True
	@output="$$(./$(BIN) parse Tests/TryCatchCompletion/Program.void)"; printf '%s\n' "$$output" | grep -Fq 'Catch FatalGameException error'; echo True
	@./$(BIN) build Tests/TryCatchCompletion >/dev/null
	@./Tests/TryCatchCompletion/bin/TryCatchCompletion
	@set -e; ./$(BIN) publish Tests/TryCatchCompletion >/dev/null; test -x Tests/TryCatchCompletion/publish/TryCatchCompletion$(EXE_SUFFIX); echo True
	@./$(BIN) build Tests/TryCatchUnhandled >/dev/null
	@set -e; set +e; output="$$(./Tests/TryCatchUnhandled/bin/TryCatchUnhandled 2>&1)"; status=$$?; set -e; test $$status -eq 1; echo True; printf '%s\n' "$$output" | grep -Fxq 'Unhandled FatalException: miss'; echo True
	@grep -Fq 'setjmp(vc_eh_' Tests/TryCatchCompletion/.void/TryCatchCompletion.c; echo True
	@grep -Fq 'vc_exception_handler_current = &vc_eh_' Tests/TryCatchCompletion/.void/TryCatchCompletion.c; echo True
	@set -e; output="$$(./$(BIN) check Tests/TryCatchDiagnostics/InvalidType 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'catch type must be Exception or a derived class'; echo True
	@set -e; output="$$(./$(BIN) check Tests/TryCatchDiagnostics/UnreachableBase 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'catch clause is unreachable because an earlier handler'; echo True
	@set -e; output="$$(./$(BIN) check Tests/TryCatchDiagnostics/AfterCatchAll 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'catch clause is unreachable after catch-all handler'; echo True
	@./$(BIN) check Tests/TryCatchDiagnostics/Nested >/dev/null; echo True
	@set -e; output="$$(./$(BIN) check Tests/TryCatchDiagnostics/Iterator 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'yield cannot be used in an iterator try block that has catch clauses'; echo True
	@./$(BIN) check Tests/TryCatchDiagnostics/FinallyOnly >/dev/null; echo True
	@./$(BIN) check Tests/TryCatchDiagnostics/FinallyAfterCatch >/dev/null; echo True
	@set -e; output="$$(./$(BIN) check Tests/TryCatchDiagnostics/MissingCatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "expected 'catch' or 'finally' after try block"; echo True

# Milestone 113: Exception Propagation & GC Unwinding
test-exception-propagation-gc-unwinding: $(BIN)
	@./$(BIN) build Tests/ExceptionPropagationGcUnwinding >/dev/null
	@./Tests/ExceptionPropagationGcUnwinding/bin/ExceptionPropagationGcUnwinding
	@set -e; ./$(BIN) publish Tests/ExceptionPropagationGcUnwinding >/dev/null; test -x Tests/ExceptionPropagationGcUnwinding/publish/ExceptionPropagationGcUnwinding$(EXE_SUFFIX); echo True
	@set -e; output="$$(VOID_GC_TRACE=1 ./Tests/ExceptionPropagationGcUnwinding/bin/ExceptionPropagationGcUnwinding 2>&1)"; printf '%s\n' "$$output" | grep -Fxq 'VOID GC: collection 1 freed 3, live 3'; echo True
	@grep -Fq '.previous = vc_exception_handler_current;' Tests/ExceptionPropagationGcUnwinding/.void/ExceptionPropagationGcUnwinding.c; echo True
	@grep -Fq '.gc_roots = vc_gc_roots;' Tests/ExceptionPropagationGcUnwinding/.void/ExceptionPropagationGcUnwinding.c; echo True
	@grep -Fq 'vc_gc_unwind_to(vc_handler->gc_roots);' Tests/ExceptionPropagationGcUnwinding/.void/ExceptionPropagationGcUnwinding.c; echo True
	@./$(BIN) check Tests/TryCatchDiagnostics/Nested >/dev/null; echo True
	@set -e; output="$$(./$(BIN) check Tests/TryCatchDiagnostics/Iterator 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'yield cannot be used in an iterator try block that has catch clauses'; echo True
	@./$(BIN) check Tests/TryCatchDiagnostics/FinallyOnly >/dev/null; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExceptionThrowDiagnostics/BareThrow 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "bare 'throw;' can only be used inside a catch block"; echo True

# Milestone 114: Finally Completion
test-finally-completion: $(BIN)
	@output="$$(./$(BIN) parse Tests/FinallyCompletion/Program.void)"; printf '%s\n' "$$output" | grep -Fq 'Finally'; echo True
	@./$(BIN) build Tests/FinallyCompletion >/dev/null
	@./Tests/FinallyCompletion/bin/FinallyCompletion
	@set -e; ./$(BIN) publish Tests/FinallyCompletion >/dev/null; test -x Tests/FinallyCompletion/publish/FinallyCompletion$(EXE_SUFFIX); echo True
	@grep -Fq 'VcExceptionHandler vc_fh_' Tests/FinallyCompletion/.void/FinallyCompletion.c; echo True
	@grep -Fq 'vc_gc_root_push(&vc_fr_' Tests/FinallyCompletion/.void/FinallyCompletion.c; echo True
	@./$(BIN) check Tests/FinallyDiagnostics/ReturnThrough >/dev/null; echo True
	@./$(BIN) check Tests/FinallyDiagnostics/BreakThrough >/dev/null; echo True
	@./$(BIN) check Tests/FinallyDiagnostics/ContinueThrough >/dev/null; echo True
	@set -e; output="$$(./$(BIN) check Tests/FinallyDiagnostics/ReturnInside 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'return cannot leave a finally block'; echo True
	@set -e; output="$$(./$(BIN) check Tests/FinallyDiagnostics/BreakInside 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'break cannot leave a finally block'; echo True
	@set -e; output="$$(./$(BIN) check Tests/FinallyDiagnostics/ContinueInside 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'continue cannot leave a finally block'; echo True
	@./$(BIN) check Tests/FinallyDiagnostics/Nested >/dev/null; echo True
	@./$(BIN) check Tests/FinallyDiagnostics/Iterator >/dev/null; echo True
	@set -e; output="$$(./$(BIN) check Tests/FinallyDiagnostics/MissingHandler 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "expected 'catch' or 'finally' after try block"; echo True

# Milestone 115: Structured Control Flow Through Finally
test-structured-control-finally: $(BIN)
	@output="$$(./$(BIN) parse Tests/StructuredControlFinally/Program.void)"; printf '%s\n' "$$output" | grep -Fq 'Finally'; echo True
	@./$(BIN) build Tests/StructuredControlFinally >/dev/null
	@./Tests/StructuredControlFinally/bin/StructuredControlFinally
	@set -e; ./$(BIN) publish Tests/StructuredControlFinally >/dev/null; test -x Tests/StructuredControlFinally/publish/StructuredControlFinally$(EXE_SUFFIX); echo True
	@grep -Fq 'vc_gc_unwind_to(vc_fh_' Tests/StructuredControlFinally/.void/StructuredControlFinally.c; echo True
	@grep -Fq '&(vc_ret_' Tests/StructuredControlFinally/.void/StructuredControlFinally.c; echo True
	@./$(BIN) check Tests/FinallyDiagnostics/ReturnThrough >/dev/null; echo True
	@./$(BIN) check Tests/FinallyDiagnostics/BreakThrough >/dev/null; echo True
	@./$(BIN) check Tests/FinallyDiagnostics/ContinueThrough >/dev/null; echo True
	@set -e; output="$$(./$(BIN) check Tests/FinallyDiagnostics/ReturnInside 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'return cannot leave a finally block'; echo True
	@set -e; output="$$(./$(BIN) check Tests/FinallyDiagnostics/BreakInside 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'break cannot leave a finally block'; echo True
	@set -e; output="$$(./$(BIN) check Tests/FinallyDiagnostics/ContinueInside 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'continue cannot leave a finally block'; echo True
	@set -e; output="$$(./$(BIN) check Tests/StructuredControlFinallyDiagnostics/OutMissing 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "out parameter 'value' must be assigned before returning"; echo True
	@./$(BIN) check Tests/FinallyDiagnostics/Nested >/dev/null; echo True
	@./$(BIN) check Tests/FinallyDiagnostics/Iterator >/dev/null; echo True

# Milestone 116: Nested Handlers & Rethrow Completion
test-nested-handlers-rethrow: $(BIN)
	@output="$$(./$(BIN) parse Tests/NestedHandlersRethrow/Program.void)"; printf '%s\n' "$$output" | grep -Fq 'Rethrow'; echo True
	@./$(BIN) check Tests/NestedHandlersRethrow >/dev/null
	@./$(BIN) build Tests/NestedHandlersRethrow >/dev/null
	@./Tests/NestedHandlersRethrow/bin/NestedHandlersRethrow
	@set -e; ./$(BIN) publish Tests/NestedHandlersRethrow >/dev/null; test -x Tests/NestedHandlersRethrow/publish/NestedHandlersRethrow$(EXE_SUFFIX); echo True
	@grep -Eq 'vc_throw_at\(vc_eh_([0-9]+)\.exception, vc_eh_\1\.fault_site\);' Tests/NestedHandlersRethrow/.void/NestedHandlersRethrow.c; echo True
	@grep -Fq 'vc_gc_trace_ref_slot' Tests/NestedHandlersRethrow/.void/NestedHandlersRethrow.c; echo True
	@grep -Fq 'vc_gc_unwind_to(vc_fh_' Tests/NestedHandlersRethrow/.void/NestedHandlersRethrow.c; echo True
	@./$(BIN) check Tests/TryCatchDiagnostics/Nested >/dev/null; echo True
	@./$(BIN) check Tests/FinallyDiagnostics/Nested >/dev/null; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExceptionThrowDiagnostics/BareThrow 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "bare 'throw;' can only be used inside a catch block"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NestedHandlersRethrowDiagnostics/RethrowFinallySibling 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "bare 'throw;' can only be used inside a catch block"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NestedHandlersRethrowDiagnostics/NestedReturnInsideFinally 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'return cannot leave a finally block'; echo True
	@set -e; output="$$(./$(BIN) check Tests/NestedHandlersRethrowDiagnostics/NestedBreakInsideFinally 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'break cannot leave a finally block'; echo True
	@set -e; output="$$(./$(BIN) check Tests/TryCatchDiagnostics/Iterator 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'yield cannot be used in an iterator try block that has catch clauses'; echo True


# Milestone 117: Iterator Exception Integration
test-iterator-exception-integration: $(BIN)
	@./$(BIN) check Tests/IteratorExceptionIntegration >/dev/null; echo True
	@./$(BIN) build Tests/IteratorExceptionIntegration >/dev/null
	@./Tests/IteratorExceptionIntegration/bin/IteratorExceptionIntegration
	@set -e; ./$(BIN) publish Tests/IteratorExceptionIntegration >/dev/null; test -x Tests/IteratorExceptionIntegration/publish/IteratorExceptionIntegration$(EXE_SUFFIX); echo True
	@grep -Fq 'setjmp(vc_eh_' Tests/IteratorExceptionIntegration/.void/IteratorExceptionIntegration.c; echo True
	@grep -Fq 'vc_throw_at(vc_eh_' Tests/IteratorExceptionIntegration/.void/IteratorExceptionIntegration.c; echo True
	@./$(BIN) check Tests/ExceptionThrowDiagnostics/IteratorThrow >/dev/null; echo True
	@./$(BIN) check Tests/FinallyDiagnostics/Iterator >/dev/null; echo True
	@set -e; output="$$(./$(BIN) check Tests/TryCatchDiagnostics/Iterator 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'yield cannot be used in an iterator try block that has catch clauses'; echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorExceptionDiagnostics/YieldTryCatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'yield cannot be used in an iterator try block that has catch clauses'; echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorExceptionDiagnostics/YieldCatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'yield cannot be used inside an iterator catch block'; echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorExceptionDiagnostics/YieldFinally 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'yield cannot be used inside an iterator finally block'; echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorExceptionDiagnostics/ThrowInt 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "throw expression must be Exception or a derived class, got 'int'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorExceptionDiagnostics/BareRethrow 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "bare 'throw;' can only be used inside a catch block"; echo True


# Milestone 118: Delegate/Event Exception Integration
test-delegate-event-exception-integration: $(BIN)
	@./$(BIN) check Tests/DelegateEventExceptionIntegration >/dev/null; echo True
	@./$(BIN) build Tests/DelegateEventExceptionIntegration >/dev/null
	@./Tests/DelegateEventExceptionIntegration/bin/DelegateEventExceptionIntegration
	@set -e; ./$(BIN) publish Tests/DelegateEventExceptionIntegration >/dev/null; test -x Tests/DelegateEventExceptionIntegration/publish/DelegateEventExceptionIntegration$(EXE_SUFFIX); echo True
	@grep -Fq 'VcGcRoot vc_delegate_root;' Tests/DelegateEventExceptionIntegration/.void/DelegateEventExceptionIntegration.c; echo True
	@grep -Fq 'vc_gc_root_push(&vc_delegate_root' Tests/DelegateEventExceptionIntegration/.void/DelegateEventExceptionIntegration.c; echo True
	@grep -Fq 'vc_gc_root_pop(&vc_delegate_root);' Tests/DelegateEventExceptionIntegration/.void/DelegateEventExceptionIntegration.c; echo True
	@./$(BIN) check Tests/DelegateCompletion >/dev/null; echo True
	@./$(BIN) check Tests/CustomEventAccessors >/dev/null; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticEventDiagnostics/ExternalInvoke 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "event 'Changed' can only be used with += or -= outside its declaring type"; echo True


# Milestone 119: Native & Library Boundary Exception Integration
test-native-library-boundary-exception-integration: $(BIN) $(NATIVE_BOUNDARY_EXCEPTION_FIXTURE)
	@./$(BIN) check Tests/NativeLibraryBoundaryExceptions/Runtime >/dev/null; echo True
	@./$(BIN) build Tests/NativeLibraryBoundaryExceptions/Runtime >/dev/null
	@./Tests/NativeLibraryBoundaryExceptions/Runtime/bin/NativeLibraryBoundaryExceptions
	@set -e; ./$(BIN) publish Tests/NativeLibraryBoundaryExceptions/Runtime >/dev/null; test -x Tests/NativeLibraryBoundaryExceptions/Runtime/publish/NativeLibraryBoundaryExceptions$(EXE_SUFFIX); echo True
	@grep -Fq 'vc_native_call_depth++;' Tests/NativeLibraryBoundaryExceptions/Runtime/.void/NativeLibraryBoundaryExceptions.c; echo True
	@grep -Fq 'vc_native_capture_exception(vc_exception, vc_boundary.fault_site, vc_boundary.captured_start, vc_boundary.captured_count);' Tests/NativeLibraryBoundaryExceptions/Runtime/.void/NativeLibraryBoundaryExceptions.c; echo True
	@grep -Fq 'vc_native_callback_' Tests/NativeLibraryBoundaryExceptions/Runtime/.void/NativeLibraryBoundaryExceptions.c; echo True
	@./$(BIN) check Tests/NativeLibraryBoundaryExceptions/Library >/dev/null; echo True
	@./$(BIN) build Tests/NativeLibraryBoundaryExceptions/Library >/dev/null
	@test -f Tests/NativeLibraryBoundaryExceptions/Library/bin/libVoid119BoundaryLib.a; echo True
	@$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror Tests/NativeLibraryBoundaryExceptions/CConsumer/main.c Tests/NativeLibraryBoundaryExceptions/Library/bin/libVoid119BoundaryLib.a -o Tests/NativeLibraryBoundaryExceptions/CConsumer/library-ok
	@./Tests/NativeLibraryBoundaryExceptions/CConsumer/library-ok
	@$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror Tests/NativeLibraryBoundaryExceptions/CConsumer/throw_main.c Tests/NativeLibraryBoundaryExceptions/Library/bin/libVoid119BoundaryLib.a -o Tests/NativeLibraryBoundaryExceptions/CConsumer/library-throw
	@set -e; output="$$(./Tests/NativeLibraryBoundaryExceptions/CConsumer/library-throw 2>&1 || status=$$?; test "$${status:-0}" -eq 1; printf '%s' "$$output")"; printf '%s\n' "$$output" | grep -Fq 'Unhandled exception at native library boundary: LibraryBoundaryException: library boundary boom'; echo True
	@grep -Fq 'vc_native_boundary_abort((void *)vc_boundary.exception, "native library boundary", vc_boundary.fault_site, vc_boundary.captured_start, vc_boundary.captured_count)' Tests/NativeLibraryBoundaryExceptions/Library/.void/Void119BoundaryLib.c; echo True
	@set -e; output="$$(./$(BIN) check Tests/NativeCallbackByRefDiagnostics/DelegateVariable 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'native callback arguments require a direct static method group'; echo True
	@set -e; output="$$(./$(BIN) check Tests/LibraryOutputDiagnostics/ManagedReturn 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'return type that is not supported by the native library ABI'; echo True
	@./$(BIN) check Tests/NativeCallbackByRef >/dev/null; echo True
	@./$(BIN) check Tests/LibraryOutput/Library >/dev/null; echo True


# Milestone 120: Exceptions & Runtime Control Flow Integration
test-exceptions-runtime-control-flow-integration: $(BIN)
	@./$(BIN) check Tests/ExceptionsRuntimeControlFlowIntegration >/dev/null; echo True
	@./$(BIN) build Tests/ExceptionsRuntimeControlFlowIntegration >/dev/null
	@./Tests/ExceptionsRuntimeControlFlowIntegration/bin/ExceptionsRuntimeControlFlowIntegration
	@set -e; ./$(BIN) publish Tests/ExceptionsRuntimeControlFlowIntegration >/dev/null; test -x Tests/ExceptionsRuntimeControlFlowIntegration/publish/ExceptionsRuntimeControlFlowIntegration$(EXE_SUFFIX); echo True
	@grep -Fq 'VcExceptionHandler vc_ti_handler;' Tests/ExceptionsRuntimeControlFlowIntegration/.void/ExceptionsRuntimeControlFlowIntegration.c; echo True
	@grep -Eq 'vc_ti_state_[0-9]+ = 0u;' Tests/ExceptionsRuntimeControlFlowIntegration/.void/ExceptionsRuntimeControlFlowIntegration.c; echo True
	@grep -Fq 'vc_rethrow_handler(&vc_ti_handler);' Tests/ExceptionsRuntimeControlFlowIntegration/.void/ExceptionsRuntimeControlFlowIntegration.c; echo True
	@./$(BIN) run Tests/StaticInitialization >/dev/null; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-exception-throw-foundation >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-try-catch-completion >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-exception-propagation-gc-unwinding >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-finally-completion >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-structured-control-finally >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-nested-handlers-rethrow >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-iterator-exception-integration >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-delegate-event-exception-integration >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-native-library-boundary-exception-integration >/dev/null; echo True; fi


# Milestone 121: IDisposable & Iterator Disposal Foundation
test-idisposable-iterator-disposal-foundation: $(BIN)
	@./$(BIN) parse StandardLibrary/Void/IDisposable.void >/dev/null; echo True
	@./$(BIN) check Tests/IteratorDisposalFoundation >/dev/null; echo True
	@./$(BIN) build Tests/IteratorDisposalFoundation >/dev/null
	@./Tests/IteratorDisposalFoundation/bin/IteratorDisposalFoundation
	@set -e; ./$(BIN) publish Tests/IteratorDisposalFoundation >/dev/null; test -x Tests/IteratorDisposalFoundation/publish/IteratorDisposalFoundation$(EXE_SUFFIX); echo True
	@grep -Fq '"IDisposable", "IDisposable"' Tests/IteratorDisposalFoundation/.void/IteratorDisposalFoundation.c; echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorDisposalDiagnostics/MissingDispose 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface method 'IDisposable.Dispose'"; echo True
	@./$(BIN) check Tests/StructInterfaces >/dev/null; echo True
	@./$(BIN) check Tests/IteratorCompletion >/dev/null; echo True
	@./$(BIN) check Tests/IteratorExceptionIntegration >/dev/null; echo True

# Milestone 122: Foreach Disposal Completion
test-foreach-disposal-completion: $(BIN)
	@./$(BIN) check Tests/ForeachDisposalCompletion >/dev/null; echo True
	@./$(BIN) build Tests/ForeachDisposalCompletion >/dev/null
	@./Tests/ForeachDisposalCompletion/bin/ForeachDisposalCompletion
	@set -e; ./$(BIN) publish Tests/ForeachDisposalCompletion >/dev/null; test -x Tests/ForeachDisposalCompletion/publish/ForeachDisposalCompletion$(EXE_SUFFIX); echo True
	@grep -Eq 'VcExceptionHandler vc_ffh_[0-9]+' Tests/ForeachDisposalCompletion/.void/ForeachDisposalCompletion.c; echo True
	@grep -Eq 'vc_gc_root_push\(&vc_r_[0-9]+, \(void \*\)&\(vc_ffh_[0-9]+\.exception\), vc_gc_trace_ref_slot\)' Tests/ForeachDisposalCompletion/.void/ForeachDisposalCompletion.c; echo True
	@grep -Eq 'static void vc_icall_[0-9]+\(void \*\);' Tests/ForeachDisposalCompletion/.void/ForeachDisposalCompletion.c; echo True
	@set -e; output="$$(./$(BIN) check Tests/ForeachDisposalDiagnostics/BadReturn 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'enumerator Dispose() must return void'; echo True
	@set -e; output="$$(./$(BIN) check Tests/ForeachDisposalDiagnostics/Inaccessible 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'enumerator Dispose() is inaccessible'; echo True
	@./$(BIN) check Tests/StructInterfaces >/dev/null; echo True
	@./$(BIN) check Tests/IteratorDisposalFoundation >/dev/null; echo True
	@./$(BIN) check Tests/IteratorExceptionIntegration >/dev/null; echo True

# Milestone 123: Using Statement Completion
test-using-statement-completion: $(BIN)
	@./$(BIN) check Tests/UsingStatementCompletion >/dev/null; echo True
	@./$(BIN) build Tests/UsingStatementCompletion >/dev/null
	@./Tests/UsingStatementCompletion/bin/UsingStatementCompletion
	@set -e; ./$(BIN) publish Tests/UsingStatementCompletion >/dev/null; test -x Tests/UsingStatementCompletion/publish/UsingStatementCompletion$(EXE_SUFFIX); echo True
	@./$(BIN) parse Tests/UsingStatementCompletion/Program.void | grep -Fq 'UsingStatement'; echo True
	@grep -Eq 'VcExceptionHandler vc_ufh_[0-9]+' Tests/UsingStatementCompletion/.void/UsingStatementCompletion.c; echo True
	@grep -Eq 'if \(vc_(using|l)_[0-9]+ != NULL\)' Tests/UsingStatementCompletion/.void/UsingStatementCompletion.c; echo True
	@grep -Eq 'static void vc_icall_[0-9]+\(void \*\);' Tests/UsingStatementCompletion/.void/UsingStatementCompletion.c; echo True
	@set -e; output="$$(./$(BIN) check Tests/UsingStatementDiagnostics/NonDisposable 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "must implement IDisposable"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UsingStatementDiagnostics/ReadonlyAssign 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign to using variable 'value'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UsingStatementDiagnostics/ReadonlyRef 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'cannot pass readonly storage as ref'; echo True
	@./$(BIN) check Tests/UsingStatementDiagnostics/Iterator >/dev/null; echo True
	@set -e; output="$$(./$(BIN) check Tests/UsingStatementDiagnostics/VarMultiple 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'implicitly typed using statement may declare only one resource'; echo True
	@./$(BIN) check Tests/IteratorDisposalFoundation >/dev/null; echo True
	@./$(BIN) check Tests/ForeachDisposalCompletion >/dev/null; echo True
	@./$(BIN) check Tests/StructuredControlFinally >/dev/null; echo True
	@./$(BIN) check Tests/ExceptionsRuntimeControlFlowIntegration >/dev/null; echo True

# Milestone 124: Using Declaration Completion
test-using-declaration-completion: $(BIN)
	@./$(BIN) check Tests/UsingDeclarationCompletion >/dev/null; echo True
	@./$(BIN) build Tests/UsingDeclarationCompletion >/dev/null
	@./Tests/UsingDeclarationCompletion/bin/UsingDeclarationCompletion
	@set -e; ./$(BIN) publish Tests/UsingDeclarationCompletion >/dev/null; test -x Tests/UsingDeclarationCompletion/publish/UsingDeclarationCompletion$(EXE_SUFFIX); echo True
	@./$(BIN) parse Tests/UsingDeclarationCompletion/Program.void | grep -Fq 'UsingDeclarationStatement'; echo True
	@grep -Eq 'VcExceptionHandler vc_ufh_[0-9]+' Tests/UsingDeclarationCompletion/.void/UsingDeclarationCompletion.c; echo True
	@set -e; count="$$(grep -Ec 'VcExceptionHandler vc_ufh_[0-9]+' Tests/UsingDeclarationCompletion/.void/UsingDeclarationCompletion.c)"; test "$$count" -ge 2; echo True
	@grep -Eq 'static void vc_icall_[0-9]+\(void \*\);' Tests/UsingDeclarationCompletion/.void/UsingDeclarationCompletion.c; echo True
	@set -e; output="$$(./$(BIN) check Tests/UsingDeclarationDiagnostics/NonDisposable 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "must implement IDisposable"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UsingDeclarationDiagnostics/ReadonlyAssign 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign to using variable 'value'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UsingDeclarationDiagnostics/ReadonlyRef 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'cannot pass readonly storage as ref'; echo True
	@./$(BIN) check Tests/UsingDeclarationDiagnostics/Iterator >/dev/null; echo True
	@set -e; output="$$(./$(BIN) check Tests/UsingDeclarationDiagnostics/DuplicateScope 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "a local or parameter named 'value' is already declared in this scope"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UsingDeclarationDiagnostics/Embedded 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'using declaration is only valid in a block or switch section'; echo True
	@set -e; output="$$(./$(BIN) check Tests/UsingDeclarationDiagnostics/MissingInitializer 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "expected '=', found ';'"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-using-statement-completion >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-foreach-disposal-completion >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-idisposable-iterator-disposal-foundation >/dev/null; echo True; fi


# Milestone 125: Generic Method Type Inference
test-generic-method-type-inference: $(BIN)
	@./$(BIN) check Tests/GenericMethodTypeInference >/dev/null; echo True
	@./$(BIN) build Tests/GenericMethodTypeInference >/dev/null
	@./Tests/GenericMethodTypeInference/bin/GenericMethodTypeInference
	@set -e; ./$(BIN) publish Tests/GenericMethodTypeInference >/dev/null; test -x Tests/GenericMethodTypeInference/publish/GenericMethodTypeInference$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericMethodTypeInferenceDiagnostics/NoEvidence 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "generic method type arguments for 'Make' could not be inferred from the call arguments"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericMethodTypeInferenceDiagnostics/NullOnly 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "generic method type arguments for 'Echo' could not be inferred from the call arguments"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericMethodTypeInferenceDiagnostics/DefaultOnly 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "generic method type arguments for 'Echo' could not be inferred from the call arguments"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericMethodTypeInferenceDiagnostics/Ambiguous 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "generic method type inference for 'Pick' is ambiguous"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericMethodTypeInferenceDiagnostics/ConstraintViolation 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type argument 'int' must be a reference type for generic parameter 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericMethodTypeInferenceDiagnostics/RefConflict 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "generic method type arguments for 'Pair' could not be inferred from the call arguments"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-generic-method-calls >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-generic-constraints >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-iterator-completion >/dev/null; echo True; fi

# Milestone 126: Extension Method Completion
test-extension-method-completion: $(BIN)
	@./$(BIN) check Tests/ExtensionMethodCompletion >/dev/null; echo True
	@./$(BIN) build Tests/ExtensionMethodCompletion >/dev/null
	@./Tests/ExtensionMethodCompletion/bin/ExtensionMethodCompletion
	@set -e; ./$(BIN) publish Tests/ExtensionMethodCompletion >/dev/null; test -x Tests/ExtensionMethodCompletion/publish/ExtensionMethodCompletion$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/ExtensionMethodCompletion 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'Parameter this value : int'; echo True
	@set -e; output="$$(./$(BIN) parse Tests/ExtensionMethodCompletion 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'Parameter this ref value : Counter'; echo True
	@set -e; output="$$(./$(BIN) parse Tests/ExtensionMethodCompletion 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'Parameter this in value : Counter'; echo True
	@grep -Eq 'vc_gc_root_push\([^;]*vc_receiver_' Tests/ExtensionMethodCompletion/.void/ExtensionMethodCompletion.c; echo True
	@grep -Eq 'vc_type_init_[0-9]+\(\), vc_m_[0-9]+\(vc_receiver_[0-9]+' Tests/ExtensionMethodCompletion/.void/ExtensionMethodCompletion.c; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExtensionMethodDiagnostics/NonStaticClass 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "extension method 'Twice' must be declared in a static class"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExtensionMethodDiagnostics/NonStaticMethod 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "extension method 'Twice' must be static"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExtensionMethodDiagnostics/OutReceiver 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "extension receiver cannot use 'out'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExtensionMethodDiagnostics/ParamsReceiver 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "extension receiver cannot use 'params'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExtensionMethodDiagnostics/DefaultReceiver 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'extension receiver cannot have a default value'; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExtensionMethodDiagnostics/MissingUsing 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type 'int' has no instance method 'Triple'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExtensionMethodDiagnostics/AliasUsing 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type 'int' has no instance method 'Triple'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExtensionMethodDiagnostics/ThisNotFirst 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "extension receiver modifier 'this' is valid only on the first parameter"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExtensionMethodDiagnostics/GenericContainingType 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'extension methods cannot be declared in a generic containing type'; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExtensionMethodDiagnostics/RefTemporary 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type 'Counter' has no matching instance or extension method 'Bump'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExtensionMethodDiagnostics/Ambiguous 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "call to extension method 'Mark' is ambiguous"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ExtensionMethodDiagnostics/GenericUninferable 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "generic extension method type arguments for 'Missing' could not be inferred"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-generic-method-type-inference >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-generic-method-calls >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-receiver-expressions >/dev/null; echo True; fi

test-static-interface-method-contracts: $(BIN)
	@./$(BIN) check Tests/StaticInterfaceMethods >/dev/null; echo True
	@./$(BIN) build Tests/StaticInterfaceMethods >/dev/null
	@./Tests/StaticInterfaceMethods/bin/StaticInterfaceMethods
	@set -e; ./$(BIN) publish Tests/StaticInterfaceMethods >/dev/null; test -x Tests/StaticInterfaceMethods/publish/StaticInterfaceMethods$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/StaticInterfaceMethods/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'Method Create : int [static]'; echo True
	@set -e; output="$$(./$(BIN) parse Tests/StaticInterfaceMethods/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'Method Echo<T> : T [static]'; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceMethodDiagnostics/Missing 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface method 'IValue.Get'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceMethodDiagnostics/InstanceInstead 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface method 'IValue.Get'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceMethodDiagnostics/NonPublic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface method 'IValue.Get'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceMethodDiagnostics/ReturnMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface method 'IValue.Get'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceMethodDiagnostics/ParameterMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface method 'IValue.Get'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceMethodDiagnostics/DirectAccess 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static interface method 'IValue.Get' is a contract and must be called through an implementing type"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceMethodDiagnostics/Body 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static interface method 'Get' must be a method signature ending in ';'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceMethodDiagnostics/InterfacePrivate 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static interface method 'Get' may only use public and static modifiers"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceMethodDiagnostics/KindConflict 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "inherits conflicting static and instance method 'Get' contracts"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceMethodDiagnostics/ReturnConflict 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "inherits conflicting method 'Get' return types"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceMethodDiagnostics/DelegateAccess 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static interface method 'IValue.Get' is a contract and must be accessed through an implementing type"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceMethodDiagnostics/GenericTypeMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface method"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-static-interface-properties >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-static-interface-events >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-interface-inheritance >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-generic-method-type-inference >/dev/null; echo True; fi


# Milestone 128: Constrained Static Interface Dispatch
test-constrained-static-interface-dispatch: $(BIN)
	@./$(BIN) check Tests/ConstrainedStaticInterfaceDispatch >/dev/null; echo True
	@./$(BIN) build Tests/ConstrainedStaticInterfaceDispatch >/dev/null
	@./Tests/ConstrainedStaticInterfaceDispatch/bin/ConstrainedStaticInterfaceDispatch
	@set -e; ./$(BIN) publish Tests/ConstrainedStaticInterfaceDispatch >/dev/null; test -x Tests/ConstrainedStaticInterfaceDispatch/publish/ConstrainedStaticInterfaceDispatch$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/ConstrainedStaticInterfaceDispatch/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'GenericConstraint T IFactory'; echo True
	@set -e; output="$$(./$(BIN) parse Tests/ConstrainedStaticInterfaceDispatch/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'MemberAccess .Create'; echo True
	@grep -Fq 'Runner__g1_ClassFactory' Tests/ConstrainedStaticInterfaceDispatch/.void/ConstrainedStaticInterfaceDispatch.c; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstrainedStaticInterfaceDispatchDiagnostics/Unconstrained 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static member 'T.Create' requires a matching static interface method constraint"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstrainedStaticInterfaceDispatchDiagnostics/ClassConstraint 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static member 'T.Create' requires a matching static interface method constraint"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstrainedStaticInterfaceDispatchDiagnostics/MissingMember 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static method 'T.Create' is not required by an applicable interface constraint"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstrainedStaticInterfaceDispatchDiagnostics/ExtraOverload 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static method 'T.Create' is not required by an applicable interface constraint"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstrainedStaticInterfaceDispatchDiagnostics/WrongImplementation 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface method 'IFactory.Create'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstrainedStaticInterfaceDispatchDiagnostics/DirectInterface 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static interface method 'IFactory.Create' is a contract and must be called through an implementing type"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-static-interface-method-contracts >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-generic-method-type-inference >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-generic-constraints >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-extension-method-completion >/dev/null; echo True; fi


# Milestone 129: Default Interface Method Completion
test-default-interface-methods: $(BIN)
	@./$(BIN) check Tests/DefaultInterfaceMethods >/dev/null; echo True
	@./$(BIN) build Tests/DefaultInterfaceMethods >/dev/null
	@./Tests/DefaultInterfaceMethods/bin/DefaultInterfaceMethods
	@set -e; ./$(BIN) publish Tests/DefaultInterfaceMethods >/dev/null; test -x Tests/DefaultInterfaceMethods/publish/DefaultInterfaceMethods$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/DefaultInterfaceMethods/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'Method Value'; echo True
	@set -e; output="$$(./$(BIN) check Tests/DefaultInterfaceMethodDiagnostics/StaticBody 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static interface method 'Value' must be a method signature ending in ';'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/DefaultInterfaceMethodDiagnostics/AmbiguousDiamond 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "inherits ambiguous default method 'Value' implementations"; echo True
	@set -e; output="$$(./$(BIN) check Tests/DefaultInterfaceMethodDiagnostics/Reabstracted 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface method 'IBase.Value'"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-constrained-static-interface-dispatch >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-static-interface-method-contracts >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-interface-inheritance >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-struct-interfaces >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-exception-propagation-gc-unwinding >/dev/null; echo True; fi

# Milestone 130: Deterministic Lifetime & Advanced Abstractions Integration
test-deterministic-lifetime-advanced-abstractions-integration: $(BIN)
	@./$(BIN) check Tests/DeterministicLifetimeAdvancedAbstractionsIntegration >/dev/null; echo True
	@./$(BIN) build Tests/DeterministicLifetimeAdvancedAbstractionsIntegration >/dev/null
	@./Tests/DeterministicLifetimeAdvancedAbstractionsIntegration/bin/DeterministicLifetimeAdvancedAbstractionsIntegration
	@set -e; ./$(BIN) publish Tests/DeterministicLifetimeAdvancedAbstractionsIntegration >/dev/null; test -x Tests/DeterministicLifetimeAdvancedAbstractionsIntegration/publish/DeterministicLifetimeAdvancedAbstractionsIntegration$(EXE_SUFFIX); echo True
	@./$(BIN) parse Tests/DeterministicLifetimeAdvancedAbstractionsIntegration/Program.void | grep -Fq 'UsingStatement'; echo True
	@set -e; output="$$(./$(BIN) check Tests/DeterministicLifetimeAdvancedAbstractionsIntegrationDiagnostics/NonDisposableIteratorUsing 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "must implement IDisposable"; echo True
	@./$(BIN) check Tests/UsingStatementDiagnostics/Iterator >/dev/null; echo True
	@./$(BIN) check Tests/UsingDeclarationDiagnostics/Iterator >/dev/null; echo True
	@./$(BIN) check Tests/IteratorDisposalFoundation >/dev/null; echo True
	@./$(BIN) check Tests/ForeachDisposalCompletion >/dev/null; echo True
	@./$(BIN) check Tests/UsingStatementCompletion >/dev/null; echo True
	@./$(BIN) check Tests/UsingDeclarationCompletion >/dev/null; echo True
	@./$(BIN) check Tests/GenericMethodTypeInference >/dev/null; echo True
	@./$(BIN) check Tests/ExtensionMethodCompletion >/dev/null; echo True
	@./$(BIN) check Tests/StaticInterfaceMethods >/dev/null; echo True
	@./$(BIN) check Tests/ConstrainedStaticInterfaceDispatch >/dev/null; echo True
	@./$(BIN) check Tests/DefaultInterfaceMethods >/dev/null; echo True


# Milestone 131: Declaration Pattern Foundation
test-declaration-patterns: $(BIN)
	@./$(BIN) check Tests/DeclarationPatterns >/dev/null; echo True
	@./$(BIN) build Tests/DeclarationPatterns >/dev/null
	@./Tests/DeclarationPatterns/bin/DeclarationPatterns
	@set -e; ./$(BIN) publish Tests/DeclarationPatterns >/dev/null; test -x Tests/DeclarationPatterns/publish/DeclarationPatterns$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/DeclarationPatterns/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'TypeRelation is DerivedValue derived'; echo True
	@set -e; output="$$(./$(BIN) check Tests/DeclarationPatternDiagnostics/OutsideCondition 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "declaration patterns are currently valid only in if, while, or pattern switch case labels"; echo True
	@set -e; output="$$(./$(BIN) check Tests/DeclarationPatternDiagnostics/ElseScope 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unknown identifier 'item'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/DeclarationPatternDiagnostics/AfterScope 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unknown identifier 'item'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/DeclarationPatternDiagnostics/Incompatible 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator 'is' cannot test or convert 'int' as 'string'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/DeclarationPatternDiagnostics/Duplicate 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "a local or parameter named 'item' is already declared in this scope"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-receiver-expressions >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-interface-inheritance >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-struct-interfaces >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-deterministic-lifetime-advanced-abstractions-integration >/dev/null; echo True; fi


# Milestone 132: Constant & Null Pattern Completion
test-constant-null-patterns: $(BIN)
	@./$(BIN) check Tests/ConstantNullPatterns >/dev/null; echo True
	@./$(BIN) build Tests/ConstantNullPatterns >/dev/null
	@./Tests/ConstantNullPatterns/bin/ConstantNullPatterns
	@set -e; ./$(BIN) publish Tests/ConstantNullPatterns >/dev/null; test -x Tests/ConstantNullPatterns/publish/ConstantNullPatterns$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/ConstantNullPatterns/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'ConstantPattern is'; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstantNullPatternDiagnostics/NullOnValue 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "null pattern cannot match value of type 'int'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstantNullPatternDiagnostics/IncompatibleConstant 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "constant pattern of type 'string' cannot match 'int'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstantNullPatternDiagnostics/ReferenceConstant 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "constant pattern cannot match value of type 'Box'"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-declaration-patterns >/dev/null; echo True; fi
	@./$(BIN) check Tests/NullableOperators >/dev/null; echo True

# Milestone 133: Relational Pattern Completion
test-relational-patterns: $(BIN)
	@./$(BIN) check Tests/RelationalPatterns >/dev/null; echo True
	@./$(BIN) build Tests/RelationalPatterns >/dev/null
	@./Tests/RelationalPatterns/bin/RelationalPatterns
	@set -e; ./$(BIN) publish Tests/RelationalPatterns >/dev/null; test -x Tests/RelationalPatterns/publish/RelationalPatterns$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/RelationalPatterns/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'RelationalPattern <'; echo True
	@set -e; output="$$(./$(BIN) check Tests/RelationalPatternDiagnostics/BoolOperand 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "relational pattern cannot match value of type 'bool'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/RelationalPatternDiagnostics/StringOperand 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "relational pattern cannot match value of type 'string'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/RelationalPatternDiagnostics/NullConstant 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "relational pattern cannot use null"; echo True
	@set -e; output="$$(./$(BIN) check Tests/RelationalPatternDiagnostics/MissingConstant 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "expected constant after relational pattern operator"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-constant-null-patterns >/dev/null; echo True; fi

# Milestone 134: Logical Pattern Completion
test-logical-patterns: $(BIN)
	@./$(BIN) check Tests/LogicalPatterns >/dev/null; echo True
	@./$(BIN) build Tests/LogicalPatterns >/dev/null
	@./Tests/LogicalPatterns/bin/LogicalPatterns
	@set -e; ./$(BIN) publish Tests/LogicalPatterns >/dev/null; test -x Tests/LogicalPatterns/publish/LogicalPatterns$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/LogicalPatterns/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'LogicalPattern and'; echo True
	@set -e; output="$$(./$(BIN) parse Tests/LogicalPatterns/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'LogicalPattern or'; echo True
	@set -e; output="$$(./$(BIN) parse Tests/LogicalPatterns/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'LogicalPattern not'; echo True
	@set -e; output="$$(./$(BIN) check Tests/LogicalPatternDiagnostics/DeclarationUnderOr 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "declaration patterns are not permitted beneath 'or'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LogicalPatternDiagnostics/DeclarationUnderNot 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "declaration patterns are not permitted beneath 'not'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LogicalPatternDiagnostics/OutsideCondition 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "declaration patterns are currently valid only in if, while, or pattern switch case labels"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LogicalPatternDiagnostics/DuplicateAnd 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "a local or parameter named 'd' is already declared in this scope"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LogicalPatternDiagnostics/MissingAfterAnd 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "expected type, found ')'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LogicalPatternDiagnostics/MissingAfterNot 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "expected type, found ')'"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-relational-patterns >/dev/null; echo True; fi

test-property-patterns: $(BIN)
	@./$(BIN) check Tests/PropertyPatterns >/dev/null; echo True
	@./$(BIN) build Tests/PropertyPatterns >/dev/null
	@./Tests/PropertyPatterns/bin/PropertyPatterns
	@set -e; ./$(BIN) publish Tests/PropertyPatterns >/dev/null; test -x Tests/PropertyPatterns/publish/PropertyPatterns$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/PropertyPatterns/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'PropertyPattern'; echo True
	@set -e; output="$$(./$(BIN) parse Tests/PropertyPatterns/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'PropertyPatternMember Value'; echo True
	@set -e; output="$$(./$(BIN) check Tests/PropertyPatternDiagnostics/MissingProperty 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "has no readable property 'Missing' for property pattern"; echo True
	@set -e; output="$$(./$(BIN) check Tests/PropertyPatternDiagnostics/InaccessibleGetter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "getter for property 'Hidden' is inaccessible"; echo True
	@set -e; output="$$(./$(BIN) check Tests/PropertyPatternDiagnostics/PrimitiveOperand 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "property pattern cannot match value of type 'int'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/PropertyPatternDiagnostics/DuplicateMember 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "property 'Value' appears more than once in property pattern"; echo True
	@./$(BIN) check Tests/PropertyPatternDiagnostics/NestedDeferred >/dev/null; echo True
	@./$(BIN) check Tests/PropertyPatternDiagnostics/DeclarationDeferred >/dev/null; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-logical-patterns >/dev/null; echo True; fi

# Milestone 136: Recursive Pattern & Flow Completion
test-recursive-patterns: $(BIN)
	@./$(BIN) check Tests/RecursivePatterns >/dev/null; echo True
	@./$(BIN) build Tests/RecursivePatterns >/dev/null
	@./Tests/RecursivePatterns/bin/RecursivePatterns
	@set -e; ./$(BIN) publish Tests/RecursivePatterns >/dev/null; test -x Tests/RecursivePatterns/publish/RecursivePatterns$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/RecursivePatterns/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'PropertyPatternMember Child'; echo True
	@set -e; output="$$(./$(BIN) parse Tests/RecursivePatterns/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'PropertyPatternMember Leaf'; echo True
	@set -e; output="$$(./$(BIN) check Tests/RecursivePatternDiagnostics/DeclarationUnderOr 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "declaration patterns are not permitted beneath 'or'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/RecursivePatternDiagnostics/DeclarationUnderNot 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "declaration patterns are not permitted beneath 'not'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/RecursivePatternDiagnostics/OutsideScope 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unknown identifier 'child'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/RecursivePatternDiagnostics/DuplicateBinding 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "a local or parameter named 'child' is already declared in this scope"; echo True
	@set -e; output="$$(./$(BIN) check Tests/RecursivePatternDiagnostics/NestedMissingProperty 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "has no readable property 'Missing' for property pattern"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-property-patterns >/dev/null; echo True; fi

# Milestone 137: Pattern Switch Statement Completion
test-pattern-switch-statements: $(BIN)
	@./$(BIN) check Tests/PatternSwitchStatement >/dev/null; echo True
	@./$(BIN) build Tests/PatternSwitchStatement >/dev/null
	@./Tests/PatternSwitchStatement/bin/PatternSwitchStatement
	@set -e; ./$(BIN) publish Tests/PatternSwitchStatement >/dev/null; test -x Tests/PatternSwitchStatement/publish/PatternSwitchStatement$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/PatternSwitchStatement/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'PatternCaseLabel'; echo True
	@set -e; output="$$(./$(BIN) parse Tests/PatternSwitchStatement/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'PropertyPatternMember Child'; echo True
	@set -e; output="$$(./$(BIN) check Tests/PatternSwitchDiagnostics/GroupedDeclaration 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'switch section with a declaration pattern cannot have multiple labels'; echo True
	@set -e; output="$$(./$(BIN) check Tests/PatternSwitchDiagnostics/IncompatiblePattern 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator 'is' cannot test or convert 'A' as 'S'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/PatternSwitchDiagnostics/DuplicateConstant 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "duplicate switch case value '1'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/PatternSwitchDiagnostics/MissingTerminator 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'switch section must end with break, return, or continue'; echo True
	@set -e; output="$$(./$(BIN) check Tests/PatternSwitchDiagnostics/DeclarationOutside 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unknown identifier 'x'"; echo True
	@set -e; output="$$(./$(BIN) run Tests/Switch 2>/dev/null)"; test "$$(printf '%s\n' "$$output" | grep -c '^True$$')" -eq 17; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-recursive-patterns >/dev/null; echo True; fi

# Milestone 138: Switch Guards & Pattern Ordering
test-switch-guards-pattern-ordering: $(BIN)
	@./$(BIN) check Tests/SwitchGuardsPatternOrdering >/dev/null; echo True
	@./$(BIN) build Tests/SwitchGuardsPatternOrdering >/dev/null
	@./Tests/SwitchGuardsPatternOrdering/bin/SwitchGuardsPatternOrdering
	@set -e; ./$(BIN) publish Tests/SwitchGuardsPatternOrdering >/dev/null; test -x Tests/SwitchGuardsPatternOrdering/publish/SwitchGuardsPatternOrdering$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/SwitchGuardsPatternOrdering/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'WhenGuard'; echo True
	@set -e; output="$$(./$(BIN) parse Tests/SwitchGuardsPatternOrdering/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'PatternCaseLabel'; echo True
	@set -e; output="$$(./$(BIN) check Tests/SwitchGuardDiagnostics/GuardNotBool 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "switch case when guard must be bool, got 'int'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/SwitchGuardDiagnostics/DefaultGuard 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'default switch label cannot have a when guard'; echo True
	@set -e; output="$$(./$(BIN) check Tests/SwitchGuardDiagnostics/UnguardedConstantSubsumesGuarded 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "duplicate switch case value '1'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/SwitchGuardDiagnostics/NullSubsumesLater 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'unreachable switch case: an earlier unguarded null case already matches'; echo True
	@set -e; output="$$(./$(BIN) check Tests/SwitchGuardDiagnostics/UnknownGuardLocal 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unknown identifier 'box'"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-pattern-switch-statements >/dev/null; echo True; fi

# Milestone 139: Switch Expression Completion
test-switch-expressions: $(BIN)
	@./$(BIN) check Tests/SwitchExpressions >/dev/null; echo True
	@./$(BIN) build Tests/SwitchExpressions >/dev/null
	@./Tests/SwitchExpressions/bin/SwitchExpressions
	@set -e; ./$(BIN) publish Tests/SwitchExpressions >/dev/null; test -x Tests/SwitchExpressions/publish/SwitchExpressions$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/SwitchExpressions/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'SwitchExpression'; echo True
	@set -e; output="$$(./$(BIN) parse Tests/SwitchExpressions/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'SwitchExpressionArm _'; echo True
	@set -e; output="$$(./$(BIN) check Tests/SwitchExpressionDiagnostics/MissingDiscard 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "switch expression requires a final unguarded discard '_' arm"; echo True
	@set -e; output="$$(./$(BIN) check Tests/SwitchExpressionDiagnostics/GuardNotBool 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "switch expression when guard must be bool, got 'int'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/SwitchExpressionDiagnostics/ArmTypeMismatch 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "switch expression arm of type 'string' cannot convert to target type 'int'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/SwitchExpressionDiagnostics/DiscardNotLast 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'unreachable switch expression arm after unguarded discard arm'; echo True
	@set -e; output="$$(./$(BIN) check Tests/SwitchExpressionDiagnostics/DeclarationOutside 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unknown identifier 'hit'"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-switch-guards-pattern-ordering >/dev/null; echo True; fi

# Milestone 140: Pattern Matching & Control-Flow Integration
test-pattern-control-flow-integration: $(BIN)
	@./$(BIN) check Tests/PatternControlFlowIntegration >/dev/null; echo True
	@./$(BIN) build Tests/PatternControlFlowIntegration >/dev/null
	@./Tests/PatternControlFlowIntegration/bin/PatternControlFlowIntegration
	@set -e; ./$(BIN) publish Tests/PatternControlFlowIntegration >/dev/null; test -x Tests/PatternControlFlowIntegration/publish/PatternControlFlowIntegration$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/PatternControlFlowIntegration/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'SwitchExpression'; echo True
	@set -e; output="$$(./$(BIN) parse Tests/PatternControlFlowIntegration/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'WhenGuard'; echo True
	@set -e; output="$$(./$(BIN) parse Tests/PatternControlFlowIntegration/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'PropertyPatternMember Payload'; echo True
	@set -e; output="$$(./$(BIN) check Tests/PatternControlFlowIntegrationDiagnostics/ArmScope 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unknown identifier 'hit'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/PatternControlFlowIntegrationDiagnostics/GuardType 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "switch expression when guard must be bool, got 'int'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/PatternControlFlowIntegrationDiagnostics/Exhaustive 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "switch expression requires a final unguarded discard '_' arm"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-switch-expressions >/dev/null; echo True; fi


test-iterator-pattern-local-capture: $(BIN)
	@./$(BIN) check Tests/IteratorPatternLocalCapture >/dev/null; echo True
	@./$(BIN) build Tests/IteratorPatternLocalCapture >/dev/null
	@./Tests/IteratorPatternLocalCapture/bin/IteratorPatternLocalCapture
	@grep -Fq '__local_' Tests/IteratorPatternLocalCapture/.void/IteratorPatternLocalCapture.c; echo True
	@set -e; ./$(BIN) publish Tests/IteratorPatternLocalCapture >/dev/null; test -x Tests/IteratorPatternLocalCapture/publish/IteratorPatternLocalCapture$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorPatternLocalCaptureDiagnostics/OutsideScope 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unknown identifier 'box'"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-pattern-control-flow-integration >/dev/null; echo True; fi

# Milestone 142: Iterator Switch/Guard Local Capture Completion
test-iterator-switch-guard-local-capture: $(BIN)
	@./$(BIN) check Tests/IteratorSwitchGuardLocalCapture >/dev/null; echo True
	@./$(BIN) build Tests/IteratorSwitchGuardLocalCapture >/dev/null
	@./Tests/IteratorSwitchGuardLocalCapture/bin/IteratorSwitchGuardLocalCapture
	@grep -Fq '__local_' Tests/IteratorSwitchGuardLocalCapture/.void/IteratorSwitchGuardLocalCapture.c; echo True
	@set -e; ./$(BIN) publish Tests/IteratorSwitchGuardLocalCapture >/dev/null; test -x Tests/IteratorSwitchGuardLocalCapture/publish/IteratorSwitchGuardLocalCapture$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorSwitchGuardLocalCaptureDiagnostics/OutsideScope 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unknown identifier 'box'"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-iterator-pattern-local-capture >/dev/null; echo True; fi

# Milestone 143: Iterator Captured-Lifetime & GC Integration
test-iterator-captured-lifetime-gc-integration: $(BIN)
	@./$(BIN) check Tests/IteratorCapturedLifetimeGcIntegration >/dev/null; echo True
	@./$(BIN) build Tests/IteratorCapturedLifetimeGcIntegration >/dev/null
	@./Tests/IteratorCapturedLifetimeGcIntegration/bin/IteratorCapturedLifetimeGcIntegration
	@grep -Fq '__local_' Tests/IteratorCapturedLifetimeGcIntegration/.void/IteratorCapturedLifetimeGcIntegration.c; echo True
	@set -e; ./$(BIN) publish Tests/IteratorCapturedLifetimeGcIntegration >/dev/null; test -x Tests/IteratorCapturedLifetimeGcIntegration/publish/IteratorCapturedLifetimeGcIntegration$(EXE_SUFFIX); echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-iterator-switch-guard-local-capture >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-iterator-exception-integration >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-idisposable-iterator-disposal-foundation >/dev/null; echo True; fi

# Milestone 144: Iterator State-Machine Integration Audit
test-iterator-state-machine-integration-audit: $(BIN)
	@./$(BIN) check Tests/IteratorStateMachineIntegrationAudit >/dev/null; echo True
	@./$(BIN) build Tests/IteratorStateMachineIntegrationAudit >/dev/null
	@./Tests/IteratorStateMachineIntegrationAudit/bin/IteratorStateMachineIntegrationAudit
	@set -e; output="$$(./$(BIN) parse Tests/IteratorStateMachineIntegrationAudit/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'SwitchExpression'; echo True
	@set -e; ./$(BIN) publish Tests/IteratorStateMachineIntegrationAudit >/dev/null; test -x Tests/IteratorStateMachineIntegrationAudit/publish/IteratorStateMachineIntegrationAudit$(EXE_SUFFIX); echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-iterator-captured-lifetime-gc-integration >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-foreach-disposal-completion >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-using-statement-completion >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-iterator-completion >/dev/null; echo True; fi

# Milestone 145: Diagnostic Contract Cleanup
test-diagnostic-contract-cleanup: $(BIN)
	@./$(BIN) check Tests/DiagnosticContractCleanup >/dev/null; echo True
	@./$(BIN) build Tests/DiagnosticContractCleanup >/dev/null
	@./Tests/DiagnosticContractCleanup/bin/DiagnosticContractCleanup
	@set -e; ./$(BIN) publish Tests/DiagnosticContractCleanup >/dev/null; test -x Tests/DiagnosticContractCleanup/publish/DiagnosticContractCleanup$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/DiagnosticContractCleanupDiagnostics/ConsoleWriteLineArity 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type 'Console' has no matching static method 'WriteLine'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnsafeDelegateDiagnostics/ManagedPointer 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "pointer parameter type is not supported"; echo True
	@set -e; output="$$(./$(BIN) check Tests/NativeCallbackByRefDiagnostics/UnsupportedManaged 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "native callback delegate has a signature that is not supported by the C ABI"; echo True
	@set -e; ! grep -REq 'milestone[[:space:]]+[0-9]+|the milestone[[:space:]]+[0-9]+' Compiler/src; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-iterator-state-machine-integration-audit >/dev/null; echo True; fi

# Milestone 146: Unary Operator Completion
test-unary-operator-completion: $(BIN)
	@./$(BIN) check Tests/UnaryOperatorCompletion >/dev/null; echo True
	@./$(BIN) build Tests/UnaryOperatorCompletion >/dev/null
	@./Tests/UnaryOperatorCompletion/bin/UnaryOperatorCompletion
	@set -e; ./$(BIN) publish Tests/UnaryOperatorCompletion >/dev/null; test -x Tests/UnaryOperatorCompletion/publish/UnaryOperatorCompletion$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/UnaryOperatorCompletion/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'Unary -'; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnaryOperatorCompletionDiagnostics/WrongArity 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unary operator overload '!' requires exactly one parameter"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnaryOperatorCompletionDiagnostics/WrongOperand 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "unary operator overload '-' must take 'Value' as its operand"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnaryOperatorCompletionDiagnostics/IncrementReturn 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator overload '++' on 'Value' must return 'Value'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnaryOperatorCompletionDiagnostics/NonStatic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator overload '-' must be static"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnaryOperatorCompletionDiagnostics/NullableLiftReturn 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator '-' returning 'RefValue' cannot be lifted to nullable"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UnaryOperatorCompletionDiagnostics/PropertyUpdate 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "user-defined operator '++' requires addressable storage"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-diagnostic-contract-cleanup >/dev/null; echo True; fi

# Milestone 147: Binary Operator Completion
test-binary-operator-completion: $(BIN)
	@./$(BIN) check Tests/BinaryOperatorCompletion >/dev/null; echo True
	@./$(BIN) build Tests/BinaryOperatorCompletion >/dev/null
	@./Tests/BinaryOperatorCompletion/bin/BinaryOperatorCompletion
	@set -e; ./$(BIN) publish Tests/BinaryOperatorCompletion >/dev/null; test -x Tests/BinaryOperatorCompletion/publish/BinaryOperatorCompletion$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/BinaryOperatorCompletion/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'Binary +'; echo True
	@set -e; output="$$(./$(BIN) check Tests/BinaryOperatorCompletionDiagnostics/WrongArity 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "binary operator overload '*' requires exactly two parameters"; echo True
	@set -e; output="$$(./$(BIN) check Tests/BinaryOperatorCompletionDiagnostics/WrongOperand 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator overload '+' must include 'Value' as an operand"; echo True
	@set -e; output="$$(./$(BIN) check Tests/BinaryOperatorCompletionDiagnostics/ComparisonReturn 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "comparison operator overload '<' must return bool"; echo True
	@set -e; output="$$(./$(BIN) check Tests/BinaryOperatorCompletionDiagnostics/MissingPair 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator overload '==' requires matching '!=' overload with the same parameter types"; echo True
	@set -e; output="$$(./$(BIN) check Tests/BinaryOperatorCompletionDiagnostics/NullableLiftReturn 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator '+' returning 'RefValue' cannot be lifted to nullable"; echo True
	@set -e; output="$$(./$(BIN) check Tests/BinaryOperatorCompletionDiagnostics/NonStatic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator overload '+' must be static"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-compound-targets >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-unary-operator-completion >/dev/null; echo True; fi

# Milestone 148: Static Interface Operator Contracts
test-static-interface-operator-contracts: $(BIN)
	@./$(BIN) check Tests/StaticInterfaceOperatorContracts >/dev/null; echo True
	@./$(BIN) build Tests/StaticInterfaceOperatorContracts >/dev/null
	@./Tests/StaticInterfaceOperatorContracts/bin/StaticInterfaceOperatorContracts
	@set -e; ./$(BIN) publish Tests/StaticInterfaceOperatorContracts >/dev/null; test -x Tests/StaticInterfaceOperatorContracts/publish/StaticInterfaceOperatorContracts$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/StaticInterfaceOperatorContracts/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'Operator + : T [static]'; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceOperatorContractDiagnostics/Missing 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface operator 'IAdd<Number>.+'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceOperatorContractDiagnostics/WrongReturn 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface operator 'IAdd<Number>.+'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceOperatorContractDiagnostics/NonPublic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface operator 'IAdd<Number>.+'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceOperatorContractDiagnostics/InterfaceBody 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static interface operator '+' must be a signature ending in ';'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceOperatorContractDiagnostics/NonStatic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "interface operator '+' must be static"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceOperatorContractDiagnostics/ConflictReturn 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "inherits conflicting operator '+' return types"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceOperatorContractDiagnostics/MissingPair 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator overload '==' requires matching '!=' overload with the same parameter types"; echo True
	@./$(BIN) check Tests/StaticInterfaceOperatorContractDiagnostics/ConversionContract >/dev/null; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceOperatorContractDiagnostics/DirectUse 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator '+' is not defined for 'IAdd<Number>' and 'IAdd<Number>'"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-binary-operator-completion >/dev/null; echo True; fi

# Milestone 149: constrained generic operator dispatch.
test-constrained-generic-operator-dispatch: $(BIN)
	@./$(BIN) check Tests/ConstrainedGenericOperatorDispatch >/dev/null; echo True
	@./$(BIN) build Tests/ConstrainedGenericOperatorDispatch >/dev/null
	@./Tests/ConstrainedGenericOperatorDispatch/bin/ConstrainedGenericOperatorDispatch
	@set -e; ./$(BIN) publish Tests/ConstrainedGenericOperatorDispatch >/dev/null; test -x Tests/ConstrainedGenericOperatorDispatch/publish/ConstrainedGenericOperatorDispatch$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstrainedGenericOperatorDispatchDiagnostics/MissingConstraint 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator '+' is not required by an applicable interface constraint for 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstrainedGenericOperatorDispatchDiagnostics/MissingOperatorConstraint 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator '-' is not required by an applicable interface constraint for 'T'"; echo True

# Milestone 150: Iterator Lifetime & Generic Operator Integration
test-iterator-lifetime-generic-operator-integration: $(BIN)
	@./$(BIN) check Tests/IteratorLifetimeGenericOperatorIntegration >/dev/null; echo True
	@./$(BIN) build Tests/IteratorLifetimeGenericOperatorIntegration >/dev/null
	@./Tests/IteratorLifetimeGenericOperatorIntegration/bin/IteratorLifetimeGenericOperatorIntegration
	@set -e; ./$(BIN) publish Tests/IteratorLifetimeGenericOperatorIntegration >/dev/null; test -x Tests/IteratorLifetimeGenericOperatorIntegration/publish/IteratorLifetimeGenericOperatorIntegration$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorLifetimeGenericOperatorIntegrationDiagnostics/MissingUnaryContract 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "operator '-' is not required by an applicable interface constraint for 'T'"; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-constrained-generic-operator-dispatch >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-iterator-state-machine-integration-audit >/dev/null; echo True; fi

# Milestone 151: Standard Conversion Classification Completion
test-standard-conversion-classification-completion: $(BIN)
	@./$(BIN) check Tests/StandardConversionClassificationCompletion >/dev/null; echo True
	@./$(BIN) build Tests/StandardConversionClassificationCompletion >/dev/null
	@./Tests/StandardConversionClassificationCompletion/bin/StandardConversionClassificationCompletion
	@set -e; ./$(BIN) publish Tests/StandardConversionClassificationCompletion >/dev/null; test -x Tests/StandardConversionClassificationCompletion/publish/StandardConversionClassificationCompletion$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/StandardConversionClassificationDiagnostics/ImplicitNarrowing 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign 'int' to local 'narrowed' of type 'byte'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StandardConversionClassificationDiagnostics/ImplicitEnum 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign 'int' to local 'mode' of type 'Mode'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StandardConversionClassificationDiagnostics/ImplicitReferenceDowncast 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign 'BaseNode' to local 'derived' of type 'DerivedNode'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StandardConversionClassificationDiagnostics/ImplicitNullableUnwrap 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign 'int?' to local 'value' of type 'int'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StandardConversionClassificationDiagnostics/PointerNumericCast 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot explicitly cast 'int' to 'int*'"; echo True
	@grep -Fq 'VcSemanticStandardConversionKind vc_semantic_classify_standard_conversion' Compiler/include/semantic.h; echo True
	@! grep -Fq 'codegen_standard_assignable' Compiler/src/compiler.c; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-iterator-lifetime-generic-operator-integration >/dev/null; echo True; fi

# Milestone 152: User-Defined Conversion Resolution Completion
test-user-defined-conversion-resolution-completion: $(BIN)
	@./$(BIN) check Tests/UserDefinedConversionResolutionCompletion >/dev/null; echo True
	@./$(BIN) build Tests/UserDefinedConversionResolutionCompletion >/dev/null
	@./Tests/UserDefinedConversionResolutionCompletion/bin/UserDefinedConversionResolutionCompletion
	@set -e; ./$(BIN) publish Tests/UserDefinedConversionResolutionCompletion >/dev/null; test -x Tests/UserDefinedConversionResolutionCompletion/publish/UserDefinedConversionResolutionCompletion$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/UserDefinedConversionResolutionDiagnostics/AmbiguousImplicit 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'byte' to 'Choice' is ambiguous"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UserDefinedConversionResolutionDiagnostics/AmbiguousExplicit 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "explicit conversion from 'int' to 'Choice' is ambiguous"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UserDefinedConversionResolutionDiagnostics/ExplicitOnlyImplicit 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign 'int' to local 'value' of type 'ExplicitOnly'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/UserDefinedConversionResolutionDiagnostics/Inaccessible 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "conversion operator 'implicit' must be public"; echo True
	@grep -Fq 'binding->has_conversion' Compiler/src/compiler.c; echo True
	@grep -Fq 'conversion_method_index' Compiler/include/semantic.h; echo True
	@./$(BIN) run Tests/Conversions >/dev/null; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-standard-conversion-classification-completion >/dev/null; echo True; fi

# Milestone 153: Lifted Nullable Conversion Completion
test-lifted-nullable-conversion-completion: $(BIN)
	@./$(BIN) check Tests/LiftedNullableConversionCompletion >/dev/null; echo True
	@./$(BIN) build Tests/LiftedNullableConversionCompletion >/dev/null
	@./Tests/LiftedNullableConversionCompletion/bin/LiftedNullableConversionCompletion
	@set -e; ./$(BIN) publish Tests/LiftedNullableConversionCompletion >/dev/null; test -x Tests/LiftedNullableConversionCompletion/publish/LiftedNullableConversionCompletion$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/LiftedNullableConversionDiagnostics/ExplicitOnlyImplicit 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign 'Source?' to local 'target' of type 'Target?'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LiftedNullableConversionDiagnostics/AmbiguousLifted 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'byte?' to 'Choice?' is ambiguous"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LiftedNullableConversionDiagnostics/ImplicitExtraction 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign 'Source?' to local 'target' of type 'Target'"; echo True
	@set -e; output="$$(./$(BIN) run Tests/LiftedNullableConversionRuntime/NullExtraction 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "VOID runtime error: nullable value has no value"; echo True
	@grep -Fq 'lifted_nullable_conversion' Compiler/include/semantic.h; echo True
	@grep -Fq 'resolve_user_conversion_model_direct' Compiler/src/semantic.c; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-user-defined-conversion-resolution-completion >/dev/null; echo True; fi
	@./$(BIN) run Tests/NullableValues >/dev/null; echo True

# Milestone 154: Conversion-Site Integration
test-conversion-site-integration: $(BIN)
	@./$(BIN) check Tests/ConversionSiteIntegration >/dev/null; echo True
	@./$(BIN) build Tests/ConversionSiteIntegration >/dev/null
	@./Tests/ConversionSiteIntegration/bin/ConversionSiteIntegration
	@set -e; ./$(BIN) publish Tests/ConversionSiteIntegration >/dev/null; test -x Tests/ConversionSiteIntegration/publish/ConversionSiteIntegration$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/ConversionSiteIntegrationDiagnostics/ExplicitOnlyAssignment 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign 'int' to 'Target'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConversionSiteIntegrationDiagnostics/InferredSwitchNoCommonType 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "switch expression arms of type 'Left' and 'Right' do not have a unique implicit result type"; echo True
	@grep -Fq 'Conversion binding is deferred until the' Compiler/src/semantic.c; echo True
	@grep -Fq 'emit_expression_as(&context, node->as.property_declaration.getter_body, property->type)' Compiler/src/compiler.c; echo True
	@./$(BIN) run Tests/Conversions >/dev/null; echo True
	@./$(BIN) run Tests/ObjectInitializers >/dev/null; echo True
	@./$(BIN) run Tests/CollectionInitializers >/dev/null; echo True
	@./$(BIN) run Tests/ConditionalExpressions >/dev/null; echo True
	@./$(BIN) run Tests/SwitchExpressions >/dev/null; echo True
	@./$(BIN) run Tests/InstanceInitializers >/dev/null; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-lifted-nullable-conversion-completion >/dev/null; echo True; fi

# Milestone 155: Static Interface Conversion Contracts
test-static-interface-conversion-contracts: $(BIN)
	@./$(BIN) check Tests/StaticInterfaceConversionContracts >/dev/null; echo True
	@./$(BIN) build Tests/StaticInterfaceConversionContracts >/dev/null
	@./Tests/StaticInterfaceConversionContracts/bin/StaticInterfaceConversionContracts
	@set -e; ./$(BIN) publish Tests/StaticInterfaceConversionContracts >/dev/null; test -x Tests/StaticInterfaceConversionContracts/publish/StaticInterfaceConversionContracts$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) parse Tests/StaticInterfaceConversionContracts/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'Operator implicit : int [static]'; echo True
	@set -e; output="$$(./$(BIN) parse Tests/StaticInterfaceConversionContracts/Program.void 2>/dev/null)"; printf '%s\n' "$$output" | grep -Fq 'Operator explicit : T [static]'; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceConversionContractDiagnostics/MissingImplicit 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface operator 'IConvert<Number>.implicit'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceConversionContractDiagnostics/WrongKind 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface operator 'IConvert<Number>.implicit'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceConversionContractDiagnostics/WrongTarget 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "does not implement interface operator 'IConvert<Number>.implicit'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceConversionContractDiagnostics/NonPublicImplementation 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "conversion operator 'implicit' must be public"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceConversionContractDiagnostics/InterfaceBody 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "static interface operator 'implicit' must be a signature ending in ';'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceConversionContractDiagnostics/NonStatic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "interface operator 'implicit' must be static"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceConversionContractDiagnostics/ConflictingKind 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "has conflicting implicit/explicit conversion contracts from 'Number' to 'int'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceConversionContractDiagnostics/StandardConversion 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "conversion from 'int' to 'long' conflicts with an existing standard conversion"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceConversionContractDiagnostics/WrongArity 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "conversion operator 'implicit' requires exactly one parameter"; echo True
	@set -e; output="$$(./$(BIN) check Tests/StaticInterfaceConversionContractDiagnostics/DirectUse 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "cannot assign 'IConvert<Number>' to local 'result' of type 'int'"; echo True
	@grep -Fq 'candidate->is_interface_method' Compiler/src/semantic.c; echo True
	@./$(BIN) check Tests/StaticInterfaceOperatorContractDiagnostics/ConversionContract >/dev/null; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-static-interface-operator-contracts >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-conversion-site-integration >/dev/null; echo True; fi

# Milestone 156: Constrained Generic Implicit Conversion Dispatch
test-constrained-generic-implicit-conversion-dispatch: $(BIN)
	@./$(BIN) check Tests/ConstrainedGenericImplicitConversionDispatch >/dev/null; echo True
	@./$(BIN) build Tests/ConstrainedGenericImplicitConversionDispatch >/dev/null
	@./Tests/ConstrainedGenericImplicitConversionDispatch/bin/ConstrainedGenericImplicitConversionDispatch
	@set -e; ./$(BIN) publish Tests/ConstrainedGenericImplicitConversionDispatch >/dev/null; test -x Tests/ConstrainedGenericImplicitConversionDispatch/publish/ConstrainedGenericImplicitConversionDispatch$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstrainedGenericImplicitConversionDispatchDiagnostics/MissingConstraint 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'Sneaky' to 'int' is not required by an applicable interface constraint for 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstrainedGenericImplicitConversionDispatchDiagnostics/MissingConversionConstraint 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'Sneaky' to 'int' is not required by an applicable interface constraint for 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstrainedGenericImplicitConversionDispatchDiagnostics/WrongGenericParameter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'Number' to 'int' is not required by an applicable interface constraint for 'U'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstrainedGenericImplicitConversionDispatchDiagnostics/WrongTargetGenericParameter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'int' to 'Number' is not required by an applicable interface constraint for 'U'"; echo True
	@grep -Fq 'validate_constrained_conversion_dispatch' Compiler/src/semantic.c; echo True
	@grep -Fq 'target_generic_parameter_origin' Compiler/src/semantic.c; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-static-interface-conversion-contracts >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-constrained-generic-operator-dispatch >/dev/null; echo True; fi

# Milestone 157: Constrained Generic Explicit Conversion Dispatch
test-constrained-generic-explicit-conversion-dispatch: $(BIN)
	@./$(BIN) check Tests/ConstrainedGenericExplicitConversionDispatch >/dev/null; echo True
	@./$(BIN) build Tests/ConstrainedGenericExplicitConversionDispatch >/dev/null
	@./Tests/ConstrainedGenericExplicitConversionDispatch/bin/ConstrainedGenericExplicitConversionDispatch
	@set -e; ./$(BIN) publish Tests/ConstrainedGenericExplicitConversionDispatch >/dev/null; test -x Tests/ConstrainedGenericExplicitConversionDispatch/publish/ConstrainedGenericExplicitConversionDispatch$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstrainedGenericExplicitConversionDispatchDiagnostics/MissingConstraint 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "explicit conversion from 'Sneaky' to 'int' is not required by an applicable interface constraint for 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstrainedGenericExplicitConversionDispatchDiagnostics/WrongGenericParameter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "explicit conversion from 'Number' to 'int' is not required by an applicable interface constraint for 'U'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConstrainedGenericExplicitConversionDispatchDiagnostics/WrongTargetGenericParameter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "explicit conversion from 'int' to 'Number' is not required by an applicable interface constraint for 'U'"; echo True
	@grep -Fq 'const VcTokenKind conversion_kind = implementation->operator_kind;' Compiler/src/semantic.c; echo True
	@grep -Fq 'type_ref_generic_parameter_origin(expression->as.cast_expression.type)' Compiler/src/semantic.c; echo True
	@grep -Fq 'conversion_kind == VC_TOKEN_KW_EXPLICIT ? "explicit" : "implicit"' Compiler/src/semantic.c; echo True
	@./$(BIN) check Tests/ConstrainedGenericImplicitConversionDispatch >/dev/null; echo True
	@./$(BIN) check Tests/StaticInterfaceConversionContracts >/dev/null; echo True


# Milestone 158: Generic Conversion Provenance & Inference Integration
test-generic-conversion-provenance-inference-integration: $(BIN)
	@./$(BIN) check Tests/GenericConversionProvenanceInferenceIntegration >/dev/null; echo True
	@./$(BIN) build Tests/GenericConversionProvenanceInferenceIntegration >/dev/null
	@./Tests/GenericConversionProvenanceInferenceIntegration/bin/GenericConversionProvenanceInferenceIntegration
	@set -e; ./$(BIN) publish Tests/GenericConversionProvenanceInferenceIntegration >/dev/null; test -x Tests/GenericConversionProvenanceInferenceIntegration/publish/GenericConversionProvenanceInferenceIntegration$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConversionProvenanceInferenceIntegrationDiagnostics/VarAliasMissingContract 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'Number' to 'int' is not required by an applicable interface constraint for 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConversionProvenanceInferenceIntegrationDiagnostics/ConditionalMissingContract 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'Number' to 'int' is not required by an applicable interface constraint for 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConversionProvenanceInferenceIntegrationDiagnostics/SwitchMissingContract 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'Number' to 'int' is not required by an applicable interface constraint for 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConversionProvenanceInferenceIntegrationDiagnostics/InferredGenericReturnMissingContract 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'Number' to 'int' is not required by an applicable interface constraint for 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConversionProvenanceInferenceIntegrationDiagnostics/TargetConditionalMissingContract 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'int' to 'Number' is not required by an applicable interface constraint for 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConversionProvenanceInferenceIntegrationDiagnostics/GenericTypeFieldMissingContract 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'Number' to 'int' is not required by an applicable interface constraint for 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConversionProvenanceInferenceIntegrationDiagnostics/GenericTypeMethodArgumentMissingContract 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'int' to 'Number' is not required by an applicable interface constraint for 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/GenericConversionProvenanceInferenceIntegrationDiagnostics/GenericTypeConstructorMissingContract 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'int' to 'Number' is not required by an applicable interface constraint for 'T'"; echo True
	@grep -Fq 'specialized_generic_arguments' Compiler/include/ast.h; echo True
	@grep -Fq 'inferred_generic_argument_origin' Compiler/src/semantic.c; echo True
	@grep -Fq 'member_generic_parameter_origin_from_receiver' Compiler/src/semantic.c; echo True
	@./$(BIN) check Tests/ConstrainedGenericExplicitConversionDispatch >/dev/null; echo True
	@./$(BIN) check Tests/ConstrainedGenericImplicitConversionDispatch >/dev/null; echo True
	@./$(BIN) check Tests/StaticInterfaceConversionContracts >/dev/null; echo True


# Milestone 159: Iterator, Closure & GC Conversion Integration
test-iterator-closure-gc-conversion-integration: $(BIN)
	@./$(BIN) check Tests/IteratorClosureGcConversionIntegration >/dev/null; echo True
	@./$(BIN) build Tests/IteratorClosureGcConversionIntegration >/dev/null
	@./Tests/IteratorClosureGcConversionIntegration/bin/IteratorClosureGcConversionIntegration
	@set -e; ./$(BIN) publish Tests/IteratorClosureGcConversionIntegration >/dev/null; test -x Tests/IteratorClosureGcConversionIntegration/publish/IteratorClosureGcConversionIntegration$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorClosureGcConversionIntegrationDiagnostics/IteratorLambdaMissingContract 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'Number' to 'int' is not required by an applicable interface constraint for 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/IteratorClosureGcConversionIntegrationDiagnostics/ClosureMissingExplicitContract 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "explicit conversion from 'Number' to 'int' is not required by an applicable interface constraint for 'T'"; echo True
	@grep -Fq 'case VC_AST_LAMBDA_EXPRESSION:' Compiler/src/iterator.c; echo True
	@grep -Fq 'type->generic_parameter_origin = binding->generic_parameter_origin;' Compiler/src/iterator.c; echo True
	@./$(BIN) check Tests/GenericConversionProvenanceInferenceIntegration >/dev/null; echo True
	@./$(BIN) check Tests/IteratorLifetimeGenericOperatorIntegration >/dev/null; echo True
	@./$(BIN) check Tests/ClosureCompletionII >/dev/null; echo True
	@./$(BIN) check Tests/IteratorCapturedLifetimeGcIntegration >/dev/null; echo True

# Milestone 160: Conversion Semantics & Generic Conversion Integration
test-conversion-semantics-generic-conversion-integration: $(BIN)
	@./$(BIN) check Tests/ConversionSemanticsGenericConversionIntegration >/dev/null; echo True
	@./$(BIN) build Tests/ConversionSemanticsGenericConversionIntegration >/dev/null
	@./Tests/ConversionSemanticsGenericConversionIntegration/bin/ConversionSemanticsGenericConversionIntegration
	@set -e; ./$(BIN) publish Tests/ConversionSemanticsGenericConversionIntegration >/dev/null; test -x Tests/ConversionSemanticsGenericConversionIntegration/publish/ConversionSemanticsGenericConversionIntegration$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/ConversionSemanticsGenericConversionIntegrationDiagnostics/DelegateInvokeMissingContract 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'Number' to 'int' is not required by an applicable interface constraint for 'T'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConversionSemanticsGenericConversionIntegrationDiagnostics/DelegateWrongGenericParameter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'Number' to 'int' is not required by an applicable interface constraint for 'U'"; echo True
	@set -e; output="$$(./$(BIN) check Tests/ConversionSemanticsGenericConversionIntegrationDiagnostics/CoalesceMissingContract 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "implicit conversion from 'Number' to 'int?' is not required by an applicable interface constraint for 'T'"; echo True
	@grep -Fq 'When ?? selects the underlying result type' Compiler/src/semantic.c; echo True
	@grep -Fq 'Delegate specializations are shared concrete types' Compiler/src/semantic.c; echo True
	@./$(BIN) check Tests/StandardConversionClassificationCompletion >/dev/null; echo True
	@./$(BIN) check Tests/UserDefinedConversionResolutionCompletion >/dev/null; echo True
	@./$(BIN) check Tests/LiftedNullableConversionCompletion >/dev/null; echo True
	@./$(BIN) check Tests/ConversionSiteIntegration >/dev/null; echo True
	@./$(BIN) check Tests/StaticInterfaceConversionContracts >/dev/null; echo True
	@./$(BIN) check Tests/ConstrainedGenericImplicitConversionDispatch >/dev/null; echo True
	@./$(BIN) check Tests/ConstrainedGenericExplicitConversionDispatch >/dev/null; echo True
	@./$(BIN) check Tests/GenericConversionProvenanceInferenceIntegration >/dev/null; echo True
	@./$(BIN) check Tests/IteratorClosureGcConversionIntegration >/dev/null; echo True
	@./$(BIN) check Tests/LibraryOutput/Library >/dev/null; echo True

# Milestone 161: Native Thread Runtime Foundation
test-native-thread-runtime-foundation: $(BIN)
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/NativeThreadRuntimeFoundation/native_thread_test.c Runtime/src/vc_thread.c -o $(NATIVE_THREAD_RUNTIME_TEST)
	@./$(NATIVE_THREAD_RUNTIME_TEST)
	@./$(BIN) check Tests/NativeThreadRuntimeFoundation >/dev/null; echo True
	@./$(BIN) build Tests/NativeThreadRuntimeFoundation >/dev/null
	@./Tests/NativeThreadRuntimeFoundation/bin/NativeThreadRuntimeFoundation
	@set -e; ./$(BIN) publish Tests/NativeThreadRuntimeFoundation >/dev/null; test -x Tests/NativeThreadRuntimeFoundation/publish/NativeThreadRuntimeFoundation$(EXE_SUFFIX); echo True
	@grep -Fq '#include "vc_thread.h"' Tests/NativeThreadRuntimeFoundation/.void/NativeThreadRuntimeFoundation.c; echo True
	@grep -Fq 'vc_native_thread_runtime_init()' Tests/NativeThreadRuntimeFoundation/.void/NativeThreadRuntimeFoundation.c; echo True
	@grep -Fq 'vc_native_thread_runtime_shutdown()' Tests/NativeThreadRuntimeFoundation/.void/NativeThreadRuntimeFoundation.c; echo True
	@! grep -Eq 'pthread_|CreateThread|WaitForSingleObject' Tests/NativeThreadRuntimeFoundation/.void/NativeThreadRuntimeFoundation.c; echo True
	@grep -Fq 'pthread_create' Runtime/src/vc_thread.c; echo True
	@grep -Fq 'CreateThread' Runtime/src/vc_thread.c; echo True
	@grep -Fq 'runtime_include_directory' Compiler/src/compiler.c; echo True
	@grep -Fq 'arguments[index++] = "-pthread";' Compiler/src/compiler.c; echo True
	@! grep -Eq 'pthread|windows.h|vc_gc_|vc_exception_|vc_runtime_alloc' Runtime/include/vc_thread.h; echo True
	@./$(BIN) build Tests/LibraryOutput/Library >/dev/null; echo True

# Milestone 162: Per-Thread Runtime Execution Context
test-per-thread-runtime-execution-context: $(BIN)
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/PerThreadRuntimeExecutionContext/runtime_context_test.c Runtime/src/vc_thread.c -o $(RUNTIME_CONTEXT_TEST)
	@./$(RUNTIME_CONTEXT_TEST)
	@./$(BIN) check Tests/PerThreadRuntimeExecutionContext >/dev/null; echo True
	@./$(BIN) build Tests/PerThreadRuntimeExecutionContext >/dev/null
	@./Tests/PerThreadRuntimeExecutionContext/bin/PerThreadRuntimeExecutionContext
	@set -e; ./$(BIN) publish Tests/PerThreadRuntimeExecutionContext >/dev/null; test -x Tests/PerThreadRuntimeExecutionContext/publish/PerThreadRuntimeExecutionContext$(EXE_SUFFIX); echo True
	@grep -Fq 'typedef struct VcRuntimeThreadContext' Tests/PerThreadRuntimeExecutionContext/.void/PerThreadRuntimeExecutionContext.c; echo True
	@grep -Fq 'VcGcRoot *gc_roots;' Tests/PerThreadRuntimeExecutionContext/.void/PerThreadRuntimeExecutionContext.c && grep -Fq 'VcExceptionHandler *exception_handler_current;' Tests/PerThreadRuntimeExecutionContext/.void/PerThreadRuntimeExecutionContext.c && grep -Fq 'void *native_pending_exception;' Tests/PerThreadRuntimeExecutionContext/.void/PerThreadRuntimeExecutionContext.c && grep -Fq 'size_t native_call_depth;' Tests/PerThreadRuntimeExecutionContext/.void/PerThreadRuntimeExecutionContext.c && grep -Fq 'size_t active_type_initializer;' Tests/PerThreadRuntimeExecutionContext/.void/PerThreadRuntimeExecutionContext.c; echo True
	@! grep -Fq 'static VcGcRoot *vc_gc_roots = NULL;' Tests/PerThreadRuntimeExecutionContext/.void/PerThreadRuntimeExecutionContext.c && ! grep -Fq 'static VcExceptionHandler *vc_exception_handler_current = NULL;' Tests/PerThreadRuntimeExecutionContext/.void/PerThreadRuntimeExecutionContext.c && ! grep -Fq 'static size_t vc_native_call_depth = 0u;' Tests/PerThreadRuntimeExecutionContext/.void/PerThreadRuntimeExecutionContext.c; echo True
	@grep -Fq 'vc_runtime_thread_attach(&vc_main_thread_context)' Tests/PerThreadRuntimeExecutionContext/.void/PerThreadRuntimeExecutionContext.c && grep -Fq 'vc_runtime_thread_detach(&vc_main_thread_context)' Tests/PerThreadRuntimeExecutionContext/.void/PerThreadRuntimeExecutionContext.c; echo True
	@grep -Fq 'static VcGcRoot *vc_gc_static_roots = NULL;' Tests/PerThreadRuntimeExecutionContext/.void/PerThreadRuntimeExecutionContext.c && grep -Fq 'vc_gc_static_root_push' Tests/PerThreadRuntimeExecutionContext/.void/PerThreadRuntimeExecutionContext.c; echo True
	@grep -Fq 'vc_native_thread_context_current' Runtime/include/vc_thread.h && grep -Fq 'vc_native_thread_context_attach' Runtime/include/vc_thread.h && grep -Fq 'vc_native_thread_context_detach' Runtime/include/vc_thread.h && grep -Fq 'VC_NATIVE_THREAD_LOCAL' Runtime/src/vc_thread.c; echo True
	@! grep -Eq 'pthread_|CreateThread|WaitForSingleObject|windows.h' Tests/PerThreadRuntimeExecutionContext/.void/PerThreadRuntimeExecutionContext.c; echo True
	@./$(BIN) build Tests/LibraryOutput/Library >/dev/null; grep -Fq 'vc_runtime_thread_attach(&vc_boundary_thread_context)' Tests/LibraryOutput/Library/.void/Void099Lib.c && grep -Fq 'vc_gc_static_root_push' Tests/LibraryOutput/Library/.void/Void099Lib.c; echo True
	@./$(BIN) run Tests/ExceptionPropagationGcUnwinding >/dev/null; echo True
	@./$(BIN) build Tests/NativeCallbacks >/dev/null; output="$$(./Tests/NativeCallbacks/bin/NativeCallbacks)"; test "$$output" = "$$(printf 'True\n12\n37\n99')"; echo True
	@grep -Fq 'vc_runtime_thread_attach(&vc_callback_thread_context)' Tests/NativeCallbacks/.void/NativeCallbacks.c && grep -Fq 'vc_runtime_thread_detach(&vc_callback_thread_context)' Tests/NativeCallbacks/.void/NativeCallbacks.c; echo True
	@./$(BIN) check Tests/NativeThreadRuntimeFoundation >/dev/null; echo True
	@./$(BIN) check Tests/GC >/dev/null; echo True

# Milestone 163: Thread-Safe Allocation & Runtime Thread Registry
test-thread-safe-allocation-runtime-registry: $(BIN)
	@./$(BIN) check Tests/ThreadSafeAllocationRuntimeRegistry/Library >/dev/null; echo True
	@./$(BIN) build Tests/ThreadSafeAllocationRuntimeRegistry/Library >/dev/null; test -f Tests/ThreadSafeAllocationRuntimeRegistry/Library/bin/libVoid163ThreadLib.a; echo True
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/ThreadSafeAllocationRuntimeRegistry/CConsumer/main.c Tests/ThreadSafeAllocationRuntimeRegistry/Library/bin/libVoid163ThreadLib.a -o $(THREAD_SAFE_ALLOCATION_REGISTRY_TEST)
	@./$(THREAD_SAFE_ALLOCATION_REGISTRY_TEST)
	@./$(BIN) publish Tests/ThreadSafeAllocationRuntimeRegistry/Library >/dev/null; test -f Tests/ThreadSafeAllocationRuntimeRegistry/Library/publish/libVoid163ThreadLib.a; echo True
	@grep -Fq 'struct VcRuntimeThreadContext *registry_previous;' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c && grep -Fq 'struct VcRuntimeThreadContext *registry_next;' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c && grep -Fq 'bool registered;' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c; echo True
	@grep -Fq 'static VcRuntimeThreadContext *vc_runtime_threads = NULL;' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c && grep -Fq 'static size_t vc_runtime_thread_count = 0u;' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c; echo True
	@grep -Fq 'vc_native_thread_runtime_registry_lock();' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c && grep -Fq 'context->registered = true;' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c && grep -Fq 'context->registered = false;' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c; echo True
	@grep -Fq 'vc_native_thread_runtime_heap_lock();' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c && grep -Fq 'vc_gc_allocations = item;' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c; echo True
	@grep -Fq 'vc_gc_stop_world_begin' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c && ! grep -Fq 'if (vc_runtime_thread_count != 1u || vc_runtime_thread_current() == NULL)' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c; echo True
	@grep -Fq 'vc_native_thread_runtime_registry_lock();' Runtime/include/vc_thread.h && grep -Fq 'vc_native_thread_runtime_heap_lock();' Runtime/include/vc_thread.h && grep -Fq 'PTHREAD_MUTEX_INITIALIZER' Runtime/src/vc_thread.c && grep -Fq 'SRWLOCK_INIT' Runtime/src/vc_thread.c; echo True
	@grep -Fq 'vc_native_thread_runtime_registry_lock();' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c && grep -Fq 'vc_library_initialized' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c; echo True
	@! grep -Eq 'pthread_|windows.h|AcquireSRWLock|pthread_mutex' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c; echo True
	@./$(BIN) check Tests/PerThreadRuntimeExecutionContext >/dev/null; echo True
	@./$(BIN) run Tests/GC >/dev/null; echo True
	@./$(BIN) build Tests/LibraryOutput/Library >/dev/null; echo True


# Milestone 164: Cooperative Stop-the-World GC & Safepoints
test-cooperative-stop-the-world-gc-safepoints: $(BIN)
	@./$(BIN) check Tests/CooperativeStopTheWorldGc/Library >/dev/null; echo True
	@./$(BIN) build Tests/CooperativeStopTheWorldGc/Library >/dev/null; test -f Tests/CooperativeStopTheWorldGc/Library/bin/libVoid164GcLib.a; echo True
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/CooperativeStopTheWorldGc/CConsumer/main.c Tests/CooperativeStopTheWorldGc/Library/bin/libVoid164GcLib.a -o $(COOPERATIVE_STW_GC_TEST)
	@trace="$$(mktemp)"; VOID_GC_TRACE=1 ./$(COOPERATIVE_STW_GC_TEST) 2>$$trace; grep -Fq 'VOID GC: collection' $$trace; rm -f $$trace; echo True
	@./$(BIN) publish Tests/CooperativeStopTheWorldGc/Library >/dev/null; test -f Tests/CooperativeStopTheWorldGc/Library/publish/libVoid164GcLib.a; echo True
	@grep -Fq 'bool gc_parked;' Tests/CooperativeStopTheWorldGc/Library/.void/Void164GcLib.c && grep -Fq 'bool gc_native_safe;' Tests/CooperativeStopTheWorldGc/Library/.void/Void164GcLib.c; echo True
	@grep -Fq 'static bool vc_gc_stop_requested = false;' Tests/CooperativeStopTheWorldGc/Library/.void/Void164GcLib.c && grep -Fq 'static VcRuntimeThreadContext *vc_gc_collector_context = NULL;' Tests/CooperativeStopTheWorldGc/Library/.void/Void164GcLib.c && grep -Fq 'collector_registered_here' Tests/CooperativeStopTheWorldGc/Library/.void/Void164GcLib.c; echo True
	@grep -Fq 'for (VcRuntimeThreadContext *context = vc_runtime_threads; context != NULL; context = context->registry_next)' Tests/CooperativeStopTheWorldGc/Library/.void/Void164GcLib.c && grep -Fq 'for (VcGcRoot *root = context->gc_roots;' Tests/CooperativeStopTheWorldGc/Library/.void/Void164GcLib.c; echo True
	@grep -Fq 'vc_gc_park_current_locked(context);' Tests/CooperativeStopTheWorldGc/Library/.void/Void164GcLib.c && grep -Fq 'while (!vc_gc_all_other_threads_safe_locked(collector))' Tests/CooperativeStopTheWorldGc/Library/.void/Void164GcLib.c; echo True
	@grep -Fq 'vc_gc_native_call_begin();' Tests/CooperativeStopTheWorldGc/Library/.void/Void164GcLib.c && grep -Fq 'vc_gc_native_call_end(vc_native_was_safe);' Tests/CooperativeStopTheWorldGc/Library/.void/Void164GcLib.c; echo True
	@grep -Fq 'vc_native_thread_runtime_gc_wait(void);' Runtime/include/vc_thread.h && grep -Fq 'pthread_cond_wait' Runtime/src/vc_thread.c && grep -Fq 'SleepConditionVariableSRW' Runtime/src/vc_thread.c; echo True
	@! grep -Fq 'if (vc_runtime_thread_count != 1u || vc_runtime_thread_current() == NULL)' Tests/CooperativeStopTheWorldGc/Library/.void/Void164GcLib.c; echo True
	@! grep -Eq 'pthread_|windows.h|AcquireSRWLock|pthread_mutex|pthread_cond' Tests/CooperativeStopTheWorldGc/Library/.void/Void164GcLib.c; echo True
	@./$(BIN) build Tests/ThreadSafeAllocationRuntimeRegistry/Library >/dev/null; $(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/ThreadSafeAllocationRuntimeRegistry/CConsumer/main.c Tests/ThreadSafeAllocationRuntimeRegistry/Library/bin/libVoid163ThreadLib.a -o $(THREAD_SAFE_ALLOCATION_REGISTRY_TEST); timeout 20s ./$(THREAD_SAFE_ALLOCATION_REGISTRY_TEST) >/dev/null; init_line="$$(grep -n 'vc_library_init();' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c | head -n 1 | cut -d: -f1)"; attach_line="$$(grep -n 'vc_runtime_thread_attach(&vc_boundary_thread_context)' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c | head -n 1 | cut -d: -f1)"; test -n "$$init_line" -a -n "$$attach_line" -a "$$init_line" -lt "$$attach_line"; echo True
	@./$(BIN) run Tests/GC >/dev/null; echo True
	@./$(BIN) build Tests/NativeCallbacks >/dev/null; output="$$(./Tests/NativeCallbacks/bin/NativeCallbacks)"; test "$$output" = "$$(printf 'True\n12\n37\n99')"; echo True
	@./$(BIN) build Tests/LibraryOutput/Library >/dev/null; echo True


# Milestone 165: Managed Thread Surface
test-managed-thread-surface: $(BIN)
	@./$(BIN) check Tests/ManagedThreadSurface >/dev/null; echo True
	@./$(BIN) build Tests/ManagedThreadSurface >/dev/null
	@timeout 30s ./Tests/ManagedThreadSurface/bin/ManagedThreadSurface
	@set -e; ./$(BIN) publish Tests/ManagedThreadSurface >/dev/null; test -x Tests/ManagedThreadSurface/publish/ManagedThreadSurface$(EXE_SUFFIX); echo True
	@grep -Fq 'namespace Void.Threading;' StandardLibrary/Void/Threading/Thread.void && grep -Fq 'public Thread(Action start)' StandardLibrary/Void/Threading/Thread.void && grep -Fq 'public void Start()' StandardLibrary/Void/Threading/Thread.void && grep -Fq 'public void Join()' StandardLibrary/Void/Threading/Thread.void; echo True
	@grep -Fq 'typedef struct VcManagedThreadState' Tests/ManagedThreadSurface/.void/ManagedThreadSurface.c && grep -Fq 'static void vc_managed_thread_worker' Tests/ManagedThreadSurface/.void/ManagedThreadSurface.c; echo True
	@grep -Fq 'vc_runtime_thread_attach(&vc_context)' Tests/ManagedThreadSurface/.void/ManagedThreadSurface.c && grep -Fq 'vc_runtime_thread_detach(&vc_context)' Tests/ManagedThreadSurface/.void/ManagedThreadSurface.c; echo True
	@grep -Fq 'vc_native_thread_create(&vc_state->native_thread, vc_managed_thread_worker, vc_state)' Tests/ManagedThreadSurface/.void/ManagedThreadSurface.c && grep -Fq 'vc_native_thread_join(&vc_state->native_thread)' Tests/ManagedThreadSurface/.void/ManagedThreadSurface.c; echo True
	@grep -Fq 'vc_gc_native_call_begin();' Tests/ManagedThreadSurface/.void/ManagedThreadSurface.c && grep -Fq 'vc_gc_native_call_end(vc_was_native_safe);' Tests/ManagedThreadSurface/.void/ManagedThreadSurface.c; echo True
	@grep -Fq 'vc_gc_static_root_push(&vc_state->owner_root' Tests/ManagedThreadSurface/.void/ManagedThreadSurface.c && grep -Fq 'vc_gc_static_root_remove(&vc_state->owner_root)' Tests/ManagedThreadSurface/.void/ManagedThreadSurface.c && grep -Fq 'vc_gc_root_push(&vc_start_root' Tests/ManagedThreadSurface/.void/ManagedThreadSurface.c; echo True
	@! grep -Eq 'pthread_|CreateThread|WaitForSingleObject|windows.h|AcquireSRWLock' Tests/ManagedThreadSurface/.void/ManagedThreadSurface.c; echo True
	@set -e; output="$$(./$(BIN) check Tests/ManagedThreadSurfaceDiagnostics/InternalIntrinsic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'Runtime.ThreadStart is reserved for Void.Threading.Thread'; echo True
	@./$(BIN) build Tests/ManagedThreadSurfaceRuntime/NullStart >/dev/null; set -e; output="$$(timeout 10s ./Tests/ManagedThreadSurfaceRuntime/NullStart/bin/NullStart 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: thread start delegate cannot be null'; echo True
	@./$(BIN) build Tests/ManagedThreadSurfaceRuntime/StartTwice >/dev/null; set -e; output="$$(timeout 10s ./Tests/ManagedThreadSurfaceRuntime/StartTwice/bin/StartTwice 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: thread has already been started'; echo True
	@./$(BIN) build Tests/ManagedThreadSurfaceRuntime/JoinBeforeStart >/dev/null; set -e; output="$$(timeout 10s ./Tests/ManagedThreadSurfaceRuntime/JoinBeforeStart/bin/JoinBeforeStart 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: thread has not been started'; echo True
	@./$(BIN) build Tests/ManagedThreadSurfaceRuntime/JoinTwice >/dev/null; set -e; output="$$(timeout 10s ./Tests/ManagedThreadSurfaceRuntime/JoinTwice/bin/JoinTwice 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: thread has already been joined'; echo True
	@./$(BIN) check Tests/CooperativeStopTheWorldGc/Library >/dev/null; echo True
	@./$(BIN) check Tests/ClosureCompletionII >/dev/null; echo True
	@./$(BIN) check Tests/GenericMethodCalls >/dev/null; echo True
	@./$(BIN) build Tests/LibraryOutput/Library >/dev/null; echo True


# Milestone 166: Monitor / Mutual-Exclusion Runtime Foundation
test-monitor-mutual-exclusion-runtime-foundation: $(BIN)
	@./$(BIN) check Tests/MonitorMutualExclusionFoundation >/dev/null; echo True
	@./$(BIN) build Tests/MonitorMutualExclusionFoundation >/dev/null
	@timeout 30s ./Tests/MonitorMutualExclusionFoundation/bin/MonitorMutualExclusionFoundation
	@set -e; ./$(BIN) publish Tests/MonitorMutualExclusionFoundation >/dev/null; test -x Tests/MonitorMutualExclusionFoundation/publish/MonitorMutualExclusionFoundation$(EXE_SUFFIX); echo True
	@grep -Fq 'public sealed class Monitor' StandardLibrary/Void/Threading/Monitor.void && grep -Fq 'public static void Enter(object value)' StandardLibrary/Void/Threading/Monitor.void && grep -Fq 'public static void Exit(object value)' StandardLibrary/Void/Threading/Monitor.void; echo True
	@grep -Fq 'VcNativeMonitor *monitor;' Tests/MonitorMutualExclusionFoundation/.void/MonitorMutualExclusionFoundation.c && grep -Fq 'static VcNativeMonitor *vc_monitor_resolve' Tests/MonitorMutualExclusionFoundation/.void/MonitorMutualExclusionFoundation.c && grep -Fq 'vc_native_monitor_create()' Tests/MonitorMutualExclusionFoundation/.void/MonitorMutualExclusionFoundation.c; echo True
	@grep -Fq 'const bool was_native_safe = vc_gc_native_call_begin();' Tests/MonitorMutualExclusionFoundation/.void/MonitorMutualExclusionFoundation.c && grep -Fq 'vc_native_monitor_enter(monitor, (void *)context)' Tests/MonitorMutualExclusionFoundation/.void/MonitorMutualExclusionFoundation.c && grep -Fq 'vc_gc_native_call_end(was_native_safe);' Tests/MonitorMutualExclusionFoundation/.void/MonitorMutualExclusionFoundation.c; echo True
	@grep -Fq 'vc_native_monitor_destroy(item->monitor)' Tests/MonitorMutualExclusionFoundation/.void/MonitorMutualExclusionFoundation.c; echo True
	@grep -Fq 'typedef struct VcNativeMonitor VcNativeMonitor;' Runtime/include/vc_thread.h && grep -Fq 'pthread_cond_wait(&monitor->entry_condition' Runtime/src/vc_thread.c && grep -Fq 'SleepConditionVariableSRW(&monitor->entry_condition' Runtime/src/vc_thread.c; echo True
	@! grep -Eq 'pthread_|windows.h|AcquireSRWLock|SleepConditionVariableSRW' Tests/MonitorMutualExclusionFoundation/.void/MonitorMutualExclusionFoundation.c; echo True
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/MonitorMutualExclusionFoundation/native_monitor_test.c Runtime/src/vc_thread.c -o $(NATIVE_MONITOR_TEST)
	@./$(NATIVE_MONITOR_TEST)
	@set -e; output="$$(./$(BIN) check Tests/MonitorMutualExclusionFoundationDiagnostics/InternalIntrinsic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'Runtime.MonitorEnter is reserved for Void.Threading.Monitor'; echo True
	@./$(BIN) build Tests/MonitorMutualExclusionFoundationRuntime/NullEnter >/dev/null; set -e; output="$$(timeout 10s ./Tests/MonitorMutualExclusionFoundationRuntime/NullEnter/bin/NullEnter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor object cannot be null'; echo True
	@./$(BIN) build Tests/MonitorMutualExclusionFoundationRuntime/ExitWithoutEnter >/dev/null; set -e; output="$$(timeout 10s ./Tests/MonitorMutualExclusionFoundationRuntime/ExitWithoutEnter/bin/ExitWithoutEnter 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor is not entered'; echo True
	@./$(BIN) build Tests/MonitorMutualExclusionFoundationRuntime/WrongOwner >/dev/null; set -e; output="$$(timeout 10s ./Tests/MonitorMutualExclusionFoundationRuntime/WrongOwner/bin/WrongOwner 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor can only be exited by its owning thread'; echo True
	@./$(BIN) check Tests/ManagedThreadSurface >/dev/null; echo True
	@./$(BIN) check Tests/CooperativeStopTheWorldGc/Library >/dev/null; echo True


# Milestone 167: lock Statement Completion
test-lock-statement-completion: $(BIN)
	@./$(BIN) check Tests/LockStatementCompletion >/dev/null; echo True
	@./$(BIN) build Tests/LockStatementCompletion >/dev/null
	@timeout 45s ./Tests/LockStatementCompletion/bin/LockStatementCompletion
	@set -e; ./$(BIN) publish Tests/LockStatementCompletion >/dev/null; test -x Tests/LockStatementCompletion/publish/LockStatementCompletion$(EXE_SUFFIX); echo True
	@./$(BIN) parse Tests/LockStatementCompletion/Program.void | grep -Fq 'LockStatement'; echo True
	@grep -Fq 'void *vc_lock_' Tests/LockStatementCompletion/.void/LockStatementCompletion.c && grep -Fq 'vc_monitor_enter(vc_lock_' Tests/LockStatementCompletion/.void/LockStatementCompletion.c && grep -Fq 'vc_monitor_exit(vc_lock_' Tests/LockStatementCompletion/.void/LockStatementCompletion.c; echo True
	@grep -Fq 'VcExceptionHandler vc_lfh_' Tests/LockStatementCompletion/.void/LockStatementCompletion.c && grep -Fq 'setjmp(vc_lfh_' Tests/LockStatementCompletion/.void/LockStatementCompletion.c; echo True
	@! grep -Eq 'pthread_|windows.h|AcquireSRWLock|SleepConditionVariableSRW' Tests/LockStatementCompletion/.void/LockStatementCompletion.c; echo True
	@./$(BIN) build Tests/LockStatementCompletionRuntime/NullTarget >/dev/null; set -e; output="$$(timeout 10s ./Tests/LockStatementCompletionRuntime/NullTarget/bin/LockNullTarget 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor object cannot be null'; echo True
	@set -e; output="$$(./$(BIN) check Tests/LockStatementCompletionDiagnostics/ValueType 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "lock expression type 'int' must be a managed object reference"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LockStatementCompletionDiagnostics/StringTarget 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "lock expression type 'string' must be a managed object reference"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LockStatementCompletionDiagnostics/TypeTarget 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "lock expression type 'Type' must be a managed object reference"; echo True
	@set -e; output="$$(./$(BIN) check Tests/LockStatementCompletionDiagnostics/IteratorYield 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'yield cannot be used inside a lock statement'; echo True
	@./$(BIN) build Tests/ThreadSafeAllocationRuntimeRegistry/Library >/dev/null; $(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/ThreadSafeAllocationRuntimeRegistry/CConsumer/main.c Tests/ThreadSafeAllocationRuntimeRegistry/Library/bin/libVoid163ThreadLib.a -o $(THREAD_SAFE_ALLOCATION_REGISTRY_TEST); timeout 30s ./$(THREAD_SAFE_ALLOCATION_REGISTRY_TEST) >/dev/null; echo True
	@grep -Fq 'vc_type_initializer_monitor_enter' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c && grep -Fq 'vc_type_initializer_monitor_exit' Tests/ThreadSafeAllocationRuntimeRegistry/Library/.void/Void163ThreadLib.c; echo True
	@./$(BIN) build Tests/NativeCallbacks >/dev/null; output="$$(./Tests/NativeCallbacks/bin/NativeCallbacks)"; test "$$output" = "$$(printf 'True\n12\n37\n99')"; echo True
	@grep -Fq '(void)atexit(vc_type_initializer_runtime_shutdown);' Tests/NativeCallbacks/.void/NativeCallbacks.c && grep -Fq 'vc_type_initializer_monitor_enter' Tests/NativeCallbacks/.void/NativeCallbacks.c; echo True
	@./$(BIN) check Tests/MonitorMutualExclusionFoundation >/dev/null; echo True
	@./$(BIN) check Tests/ManagedThreadSurface >/dev/null; echo True
	@./$(BIN) check Tests/StructuredControlFinally >/dev/null; echo True


# Milestone 168: Thread Exception, Native Boundary & Cleanup Integration
test-thread-exception-native-boundary-cleanup-integration: $(BIN) $(THREAD_EXCEPTION_NATIVE_FIXTURE)
	@./$(BIN) check Tests/ThreadExceptionNativeBoundaryCleanup >/dev/null; echo True
	@./$(BIN) build Tests/ThreadExceptionNativeBoundaryCleanup >/dev/null
	@timeout 45s ./Tests/ThreadExceptionNativeBoundaryCleanup/bin/ThreadExceptionNativeBoundaryCleanup
	@set -e; ./$(BIN) publish Tests/ThreadExceptionNativeBoundaryCleanup >/dev/null; test -x Tests/ThreadExceptionNativeBoundaryCleanup/publish/ThreadExceptionNativeBoundaryCleanup$(EXE_SUFFIX); echo True
	@grep -Fq 'void *exception;' Tests/ThreadExceptionNativeBoundaryCleanup/.void/ThreadExceptionNativeBoundaryCleanup.c && grep -Fq 'VcGcRoot exception_root;' Tests/ThreadExceptionNativeBoundaryCleanup/.void/ThreadExceptionNativeBoundaryCleanup.c && grep -Fq 'bool exception_root_active;' Tests/ThreadExceptionNativeBoundaryCleanup/.void/ThreadExceptionNativeBoundaryCleanup.c; echo True
	@grep -Fq 'VcExceptionHandler vc_boundary;' Tests/ThreadExceptionNativeBoundaryCleanup/.void/ThreadExceptionNativeBoundaryCleanup.c && grep -Fq 'vc_state->exception = (void *)vc_boundary.exception;' Tests/ThreadExceptionNativeBoundaryCleanup/.void/ThreadExceptionNativeBoundaryCleanup.c && grep -Fq 'vc_exception_in_flight_clear((void *)vc_boundary.exception);' Tests/ThreadExceptionNativeBoundaryCleanup/.void/ThreadExceptionNativeBoundaryCleanup.c; echo True
	@grep -Fq 'void *exception_in_flight;' Tests/ThreadExceptionNativeBoundaryCleanup/.void/ThreadExceptionNativeBoundaryCleanup.c && grep -Fq 'if (context->exception_in_flight != NULL) vc_gc_mark(context->exception_in_flight);' Tests/ThreadExceptionNativeBoundaryCleanup/.void/ThreadExceptionNativeBoundaryCleanup.c; echo True
	@grep -Fq 'vc_exception_in_flight = vc_exception;' Tests/ThreadExceptionNativeBoundaryCleanup/.void/ThreadExceptionNativeBoundaryCleanup.c && grep -Fq 'vc_exception_in_flight_clear(vc_exception);' Tests/ThreadExceptionNativeBoundaryCleanup/.void/ThreadExceptionNativeBoundaryCleanup.c; echo True
	@grep -Fq 'if (vc_exception_root_active) vc_throw(vc_exception);' Tests/ThreadExceptionNativeBoundaryCleanup/.void/ThreadExceptionNativeBoundaryCleanup.c; echo True
	@! grep -Eq 'pthread_|windows.h|CreateThread|WaitForSingleObject|AcquireSRWLock' Tests/ThreadExceptionNativeBoundaryCleanup/.void/ThreadExceptionNativeBoundaryCleanup.c; echo True
	@./$(BIN) build Tests/ThreadExceptionNativeBoundaryCleanupRuntime/JoinAfterException >/dev/null; set -e; output="$$(timeout 10s ./Tests/ThreadExceptionNativeBoundaryCleanupRuntime/JoinAfterException/bin/JoinAfterException 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: thread has already been joined'; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-native-library-boundary-exception-integration >/dev/null; echo True; fi
	@./$(BIN) run Tests/ExceptionPropagationGcUnwinding >/dev/null; echo True
	@./$(BIN) check Tests/LockStatementCompletion >/dev/null; echo True
	@./$(BIN) build Tests/LibraryOutput/Library >/dev/null; grep -Fq 'void *exception_in_flight;' Tests/LibraryOutput/Library/.void/Void099Lib.c; echo True


# Milestone 169: Threaded Closure, Delegate, Generic & GC Integration
test-threaded-closure-delegate-generic-gc-integration: $(BIN)
	@./$(BIN) check Tests/ThreadedClosureDelegateGenericGcIntegration >/dev/null; echo True
	@./$(BIN) build Tests/ThreadedClosureDelegateGenericGcIntegration >/dev/null
	@timeout 45s ./Tests/ThreadedClosureDelegateGenericGcIntegration/bin/ThreadedClosureDelegateGenericGcIntegration
	@set -e; ./$(BIN) publish Tests/ThreadedClosureDelegateGenericGcIntegration >/dev/null; test -x Tests/ThreadedClosureDelegateGenericGcIntegration/publish/ThreadedClosureDelegateGenericGcIntegration$(EXE_SUFFIX); echo True
	@grep -Fq 'vc_lambda_0' Tests/ThreadedClosureDelegateGenericGcIntegration/.void/ThreadedClosureDelegateGenericGcIntegration.c && grep -Fq 'vc_lambda_1' Tests/ThreadedClosureDelegateGenericGcIntegration/.void/ThreadedClosureDelegateGenericGcIntegration.c && grep -Fq 'vc_lambda_5' Tests/ThreadedClosureDelegateGenericGcIntegration/.void/ThreadedClosureDelegateGenericGcIntegration.c; echo True
	@grep -Fq 'vc_gc_root_push(&vc_r_0, (void *)&(vc_frame), vc_gc_trace_ref_slot);' Tests/ThreadedClosureDelegateGenericGcIntegration/.void/ThreadedClosureDelegateGenericGcIntegration.c; echo True
	@grep -Fq 'vc_delegate_combine_' Tests/ThreadedClosureDelegateGenericGcIntegration/.void/ThreadedClosureDelegateGenericGcIntegration.c; echo True
	@grep -Fq 'vc_managed_thread_worker' Tests/ThreadedClosureDelegateGenericGcIntegration/.void/ThreadedClosureDelegateGenericGcIntegration.c && grep -Fq 'vc_gc_root_push(&vc_start_root' Tests/ThreadedClosureDelegateGenericGcIntegration/.void/ThreadedClosureDelegateGenericGcIntegration.c; echo True
	@! grep -Eq 'pthread_|windows.h|CreateThread|WaitForSingleObject|AcquireSRWLock' Tests/ThreadedClosureDelegateGenericGcIntegration/.void/ThreadedClosureDelegateGenericGcIntegration.c; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-closure-completion-ii >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-delegate-completion >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-generic-method-type-inference >/dev/null; echo True; fi
	@./$(BIN) check Tests/ManagedThreadSurface >/dev/null; echo True
	@./$(BIN) check Tests/ThreadExceptionNativeBoundaryCleanup >/dev/null; echo True


# Milestone 170: Native Concurrency & Synchronization Integration Audit
test-native-concurrency-synchronization-integration-audit: $(BIN)
	@./$(BIN) check Tests/NativeConcurrencySynchronizationIntegrationAudit >/dev/null; echo True
	@./$(BIN) build Tests/NativeConcurrencySynchronizationIntegrationAudit >/dev/null
	@timeout 45s ./Tests/NativeConcurrencySynchronizationIntegrationAudit/bin/NativeConcurrencySynchronizationIntegrationAudit
	@set -e; ./$(BIN) publish Tests/NativeConcurrencySynchronizationIntegrationAudit >/dev/null; test -x Tests/NativeConcurrencySynchronizationIntegrationAudit/publish/NativeConcurrencySynchronizationIntegrationAudit$(EXE_SUFFIX); echo True
	@grep -Fq 'vc_managed_thread_worker' Tests/NativeConcurrencySynchronizationIntegrationAudit/.void/NativeConcurrencySynchronizationIntegrationAudit.c && grep -Fq 'vc_native_thread_create' Tests/NativeConcurrencySynchronizationIntegrationAudit/.void/NativeConcurrencySynchronizationIntegrationAudit.c && grep -Fq 'vc_native_thread_join' Tests/NativeConcurrencySynchronizationIntegrationAudit/.void/NativeConcurrencySynchronizationIntegrationAudit.c; echo True
	@grep -Fq 'vc_monitor_enter' Tests/NativeConcurrencySynchronizationIntegrationAudit/.void/NativeConcurrencySynchronizationIntegrationAudit.c && grep -Fq 'vc_monitor_exit' Tests/NativeConcurrencySynchronizationIntegrationAudit/.void/NativeConcurrencySynchronizationIntegrationAudit.c; echo True
	@grep -Fq 'vc_exception_in_flight' Tests/NativeConcurrencySynchronizationIntegrationAudit/.void/NativeConcurrencySynchronizationIntegrationAudit.c && grep -Fq 'VcExceptionHandler vc_boundary' Tests/NativeConcurrencySynchronizationIntegrationAudit/.void/NativeConcurrencySynchronizationIntegrationAudit.c; echo True
	@grep -Fq 'vc_gc_safepoint' Tests/NativeConcurrencySynchronizationIntegrationAudit/.void/NativeConcurrencySynchronizationIntegrationAudit.c && grep -Fq 'vc_gc_root_push' Tests/NativeConcurrencySynchronizationIntegrationAudit/.void/NativeConcurrencySynchronizationIntegrationAudit.c; echo True
	@grep -Fq 'vc_delegate_combine_' Tests/NativeConcurrencySynchronizationIntegrationAudit/.void/NativeConcurrencySynchronizationIntegrationAudit.c && grep -Fq 'vc_lambda_' Tests/NativeConcurrencySynchronizationIntegrationAudit/.void/NativeConcurrencySynchronizationIntegrationAudit.c; echo True
	@grep -Fq 'vc_type_initializer_monitors' Tests/NativeConcurrencySynchronizationIntegrationAudit/.void/NativeConcurrencySynchronizationIntegrationAudit.c; echo True
	@! grep -Eq 'pthread_|windows.h|CreateThread|WaitForSingleObject|AcquireSRWLock' Tests/NativeConcurrencySynchronizationIntegrationAudit/.void/NativeConcurrencySynchronizationIntegrationAudit.c; echo True
	@! grep -Eq '#include <pthread.h>|#include <windows.h>' Runtime/include/vc_thread.h; echo True
	@grep -Fq '#if defined(_WIN32)' Runtime/src/vc_thread.c && grep -Fq '#elif defined(__unix__) || defined(__APPLE__)' Runtime/src/vc_thread.c; echo True
	@set -e; for i in 1 2 3 4 5; do output="$$(timeout 45s ./Tests/NativeConcurrencySynchronizationIntegrationAudit/bin/NativeConcurrencySynchronizationIntegrationAudit)"; test "$$(printf '%s\n' "$$output" | grep -c '^True$$')" -eq 10; done; echo True
	@./$(BIN) check Tests/ManagedThreadSurface >/dev/null; ./$(BIN) check Tests/MonitorMutualExclusionFoundation >/dev/null; ./$(BIN) check Tests/LockStatementCompletion >/dev/null; ./$(BIN) check Tests/ThreadExceptionNativeBoundaryCleanup >/dev/null; ./$(BIN) check Tests/ThreadedClosureDelegateGenericGcIntegration >/dev/null; echo True


# Milestone 171: Monitor Condition-Wait Runtime Foundation
test-monitor-condition-wait-runtime-foundation: $(BIN)
	@./$(BIN) check Tests/MonitorConditionWaitRuntimeFoundation >/dev/null; echo True
	@./$(BIN) build Tests/MonitorConditionWaitRuntimeFoundation >/dev/null
	@timeout 20s ./Tests/MonitorConditionWaitRuntimeFoundation/bin/MonitorConditionWaitRuntimeFoundation
	@set -e; ./$(BIN) publish Tests/MonitorConditionWaitRuntimeFoundation >/dev/null; test -x Tests/MonitorConditionWaitRuntimeFoundation/publish/MonitorConditionWaitRuntimeFoundation$(EXE_SUFFIX); echo True
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/MonitorConditionWaitRuntimeFoundation/native_monitor_condition_test.c Runtime/src/vc_thread.c -o $(MONITOR_CONDITION_WAIT_RUNTIME_TEST)
	@timeout 30s ./$(MONITOR_CONDITION_WAIT_RUNTIME_TEST)
	@grep -Fq 'bool vc_native_monitor_wait(VcNativeMonitor *monitor, void *owner);' Runtime/include/vc_thread.h && grep -Fq 'bool vc_native_monitor_signal(VcNativeMonitor *monitor, void *owner);' Runtime/include/vc_thread.h && grep -Fq 'bool vc_native_monitor_broadcast(VcNativeMonitor *monitor, void *owner);' Runtime/include/vc_thread.h; echo True
	@grep -Fq 'entry_condition' Runtime/src/vc_thread.c && grep -Fq 'condition_waiters' Runtime/src/vc_thread.c && grep -Fq 'VcNativeMonitorWaiter' Runtime/src/vc_thread.c; echo True
	@grep -Fq 'static VC_MAYBE_UNUSED bool vc_monitor_wait(void *object)' Tests/MonitorConditionWaitRuntimeFoundation/.void/MonitorConditionWaitRuntimeFoundation.c && grep -Fq 'const bool was_native_safe = vc_gc_native_call_begin();' Tests/MonitorConditionWaitRuntimeFoundation/.void/MonitorConditionWaitRuntimeFoundation.c && grep -Fq 'vc_native_monitor_wait(monitor, (void *)context)' Tests/MonitorConditionWaitRuntimeFoundation/.void/MonitorConditionWaitRuntimeFoundation.c && grep -Fq 'vc_gc_native_call_end(was_native_safe);' Tests/MonitorConditionWaitRuntimeFoundation/.void/MonitorConditionWaitRuntimeFoundation.c; echo True
	@grep -Fq 'static VC_MAYBE_UNUSED bool vc_monitor_signal(void *object)' Tests/MonitorConditionWaitRuntimeFoundation/.void/MonitorConditionWaitRuntimeFoundation.c && grep -Fq 'static VC_MAYBE_UNUSED bool vc_monitor_broadcast(void *object)' Tests/MonitorConditionWaitRuntimeFoundation/.void/MonitorConditionWaitRuntimeFoundation.c; echo True
	@! grep -Eq 'pthread_|windows.h|AcquireSRWLock|SleepConditionVariableSRW' Tests/MonitorConditionWaitRuntimeFoundation/.void/MonitorConditionWaitRuntimeFoundation.c; echo True
	@grep -Fq 'while (!waiter.signaled)' Runtime/src/vc_thread.c && grep -Fq 'condition_waiters' Runtime/src/vc_thread.c; echo True
	@./$(BIN) check Tests/CooperativeStopTheWorldGc/Library >/dev/null; ./$(BIN) check Tests/MonitorMutualExclusionFoundation >/dev/null; ./$(BIN) check Tests/LockStatementCompletion >/dev/null; echo True
	@set -e; out=$$(mktemp); VOID_TEST_SLOW_START=1 timeout 15s ./$(MONITOR_CONDITION_WAIT_RUNTIME_TEST) >$$out; test "$$(grep -c '^True$$' $$out)" -eq 35; ! grep -Fxq False $$out; rm -f $$out


# Milestone 172: Managed Monitor.Wait Completion
test-managed-monitor-wait-completion: $(BIN)
	@./$(BIN) check Tests/ManagedMonitorWaitCompletion >/dev/null; echo True
	@./$(BIN) build Tests/ManagedMonitorWaitCompletion >/dev/null
	@timeout 20s ./Tests/ManagedMonitorWaitCompletion/bin/ManagedMonitorWaitCompletion
	@set -e; ./$(BIN) publish Tests/ManagedMonitorWaitCompletion >/dev/null; test -x Tests/ManagedMonitorWaitCompletion/publish/ManagedMonitorWaitCompletion$(EXE_SUFFIX); echo True
	@grep -Fq 'public static bool Wait(object value)' StandardLibrary/Void/Threading/Monitor.void && grep -Fq 'return Runtime.MonitorWait(value);' StandardLibrary/Void/Threading/Monitor.void; echo True
	@grep -Fq 'vc_monitor_wait(' Tests/ManagedMonitorWaitCompletion/.void/ManagedMonitorWaitCompletion.c && grep -Fq 'static VC_MAYBE_UNUSED bool vc_monitor_wait(void *object)' Tests/ManagedMonitorWaitCompletion/.void/ManagedMonitorWaitCompletion.c; echo True
	@grep -Fq 'const bool was_native_safe = vc_gc_native_call_begin();' Tests/ManagedMonitorWaitCompletion/.void/ManagedMonitorWaitCompletion.c && grep -Fq 'vc_native_monitor_wait(monitor, (void *)context)' Tests/ManagedMonitorWaitCompletion/.void/ManagedMonitorWaitCompletion.c && grep -Fq 'vc_gc_native_call_end(was_native_safe);' Tests/ManagedMonitorWaitCompletion/.void/ManagedMonitorWaitCompletion.c; echo True
	@grep -Fq 'monitor wait requires ownership' Tests/ManagedMonitorWaitCompletion/.void/ManagedMonitorWaitCompletion.c && grep -Fq 'return true;' Tests/ManagedMonitorWaitCompletion/.void/ManagedMonitorWaitCompletion.c; echo True
	@grep -Fq 'public static bool Wait(object value)' StandardLibrary/Void/Threading/Monitor.void && grep -Fq 'return Runtime.MonitorWait(value);' StandardLibrary/Void/Threading/Monitor.void; echo True
	@set -e; output="$$(./$(BIN) check Tests/ManagedMonitorWaitCompletionDiagnostics/InternalIntrinsic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'Runtime.MonitorWait is reserved for Void.Threading.Monitor'; echo True
	@./$(BIN) build Tests/ManagedMonitorWaitCompletionRuntime/WaitWithoutOwnership >/dev/null; set -e; output="$$(timeout 10s ./Tests/ManagedMonitorWaitCompletionRuntime/WaitWithoutOwnership/bin/WaitWithoutOwnership 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor wait requires ownership'; echo True
	@./$(BIN) build Tests/ManagedMonitorWaitCompletionRuntime/NullTarget >/dev/null; set -e; output="$$(timeout 10s ./Tests/ManagedMonitorWaitCompletionRuntime/NullTarget/bin/NullTarget 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor object cannot be null'; echo True
	@./$(BIN) build Tests/ManagedMonitorWaitCompletionRuntime/WrongOwner >/dev/null; set -e; output="$$(timeout 10s ./Tests/ManagedMonitorWaitCompletionRuntime/WrongOwner/bin/WaitWrongOwner 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor wait requires ownership'; echo True
	@grep -Fq 'strcmp(member, "MonitorWait") == 0' Compiler/src/semantic.c && grep -Fq '? VC_SEM_TYPE_BOOL : VC_SEM_TYPE_VOID' Compiler/src/semantic.c; echo True
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/MonitorConditionWaitRuntimeFoundation/native_monitor_condition_test.c Runtime/src/vc_thread.c -o $(MONITOR_CONDITION_WAIT_RUNTIME_TEST)
	@timeout 30s ./$(MONITOR_CONDITION_WAIT_RUNTIME_TEST)
	@./$(BIN) check Tests/MonitorMutualExclusionFoundation >/dev/null; ./$(BIN) check Tests/LockStatementCompletion >/dev/null; ./$(BIN) check Tests/CooperativeStopTheWorldGc/Library >/dev/null; echo True


# Milestone 173: Monitor.Pulse / PulseAll Completion
test-monitor-pulse-completion: $(BIN)
	@./$(BIN) check Tests/MonitorPulseCompletion >/dev/null; echo True
	@./$(BIN) build Tests/MonitorPulseCompletion >/dev/null
	@timeout 30s ./Tests/MonitorPulseCompletion/bin/MonitorPulseCompletion
	@set -e; for i in 1 2 3 4 5; do output="$$(timeout 30s ./Tests/MonitorPulseCompletion/bin/MonitorPulseCompletion)"; test "$$(printf '%s\n' "$$output" | grep -c '^True$$')" -eq 9; done; echo True
	@set -e; ./$(BIN) publish Tests/MonitorPulseCompletion >/dev/null; test -x Tests/MonitorPulseCompletion/publish/MonitorPulseCompletion$(EXE_SUFFIX); echo True
	@grep -Fq 'public static void Pulse(object value)' StandardLibrary/Void/Threading/Monitor.void && grep -Fq 'Runtime.MonitorPulse(value);' StandardLibrary/Void/Threading/Monitor.void; echo True
	@grep -Fq 'public static void PulseAll(object value)' StandardLibrary/Void/Threading/Monitor.void && grep -Fq 'Runtime.MonitorPulseAll(value);' StandardLibrary/Void/Threading/Monitor.void; echo True
	@grep -Fq 'vc_monitor_signal(' Tests/MonitorPulseCompletion/.void/MonitorPulseCompletion.c && grep -Fq 'vc_monitor_broadcast(' Tests/MonitorPulseCompletion/.void/MonitorPulseCompletion.c; echo True
	@grep -Fq 'monitor pulse requires ownership' Tests/MonitorPulseCompletion/.void/MonitorPulseCompletion.c && grep -Fq 'monitor pulse-all requires ownership' Tests/MonitorPulseCompletion/.void/MonitorPulseCompletion.c; echo True
	@grep -Fq 'strcmp(member, "MonitorPulse") == 0' Compiler/src/compiler.c && grep -Fq 'strcmp(member, "MonitorPulseAll") == 0' Compiler/src/compiler.c; echo True
	@grep -Fq 'strcmp(callee->as.member_access_expression.member, "MonitorPulse") == 0' Compiler/src/semantic.c && grep -Fq 'strcmp(callee->as.member_access_expression.member, "MonitorPulseAll") == 0' Compiler/src/semantic.c; echo True
	@set -e; output="$$(./$(BIN) check Tests/MonitorPulseCompletionDiagnostics/InternalPulse 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'Runtime.MonitorPulse is reserved for Void.Threading.Monitor'; echo True
	@set -e; output="$$(./$(BIN) check Tests/MonitorPulseCompletionDiagnostics/InternalPulseAll 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'Runtime.MonitorPulseAll is reserved for Void.Threading.Monitor'; echo True
	@./$(BIN) build Tests/MonitorPulseCompletionRuntime/PulseWithoutOwnership >/dev/null; set -e; output="$$(timeout 10s ./Tests/MonitorPulseCompletionRuntime/PulseWithoutOwnership/bin/PulseWithoutOwnership 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor pulse requires ownership'; echo True
	@./$(BIN) build Tests/MonitorPulseCompletionRuntime/PulseAllWithoutOwnership >/dev/null; set -e; output="$$(timeout 10s ./Tests/MonitorPulseCompletionRuntime/PulseAllWithoutOwnership/bin/PulseAllWithoutOwnership 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor pulse-all requires ownership'; echo True
	@./$(BIN) build Tests/MonitorPulseCompletionRuntime/NullPulse >/dev/null; set -e; output="$$(timeout 10s ./Tests/MonitorPulseCompletionRuntime/NullPulse/bin/NullPulse 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor object cannot be null'; echo True
	@./$(BIN) build Tests/MonitorPulseCompletionRuntime/NullPulseAll >/dev/null; set -e; output="$$(timeout 10s ./Tests/MonitorPulseCompletionRuntime/NullPulseAll/bin/NullPulseAll 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor object cannot be null'; echo True
	@./$(BIN) build Tests/MonitorPulseCompletionRuntime/WrongOwnerPulse >/dev/null; set -e; output="$$(timeout 10s ./Tests/MonitorPulseCompletionRuntime/WrongOwnerPulse/bin/WrongOwnerPulse 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor pulse requires ownership'; echo True
	@./$(BIN) build Tests/MonitorPulseCompletionRuntime/WrongOwnerPulseAll >/dev/null; set -e; output="$$(timeout 10s ./Tests/MonitorPulseCompletionRuntime/WrongOwnerPulseAll/bin/WrongOwnerPulseAll 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor pulse-all requires ownership'; echo True
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/MonitorConditionWaitRuntimeFoundation/native_monitor_condition_test.c Runtime/src/vc_thread.c -o $(MONITOR_CONDITION_WAIT_RUNTIME_TEST)
	@timeout 30s ./$(MONITOR_CONDITION_WAIT_RUNTIME_TEST)
	@./$(BIN) check Tests/ManagedMonitorWaitCompletion >/dev/null; ./$(BIN) check Tests/LockStatementCompletion >/dev/null; ./$(BIN) check Tests/CooperativeStopTheWorldGc/Library >/dev/null; echo True


# Milestone 174: Portable Atomic Runtime Foundation
test-portable-atomic-runtime-foundation: $(BIN)
	@./$(BIN) check Tests/PortableAtomicRuntimeFoundation >/dev/null; echo True
	@./$(BIN) build Tests/PortableAtomicRuntimeFoundation >/dev/null
	@timeout 10s ./Tests/PortableAtomicRuntimeFoundation/bin/PortableAtomicRuntimeFoundation
	@set -e; ./$(BIN) publish Tests/PortableAtomicRuntimeFoundation >/dev/null; test -x Tests/PortableAtomicRuntimeFoundation/publish/PortableAtomicRuntimeFoundation$(EXE_SUFFIX); echo True
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/PortableAtomicRuntimeFoundation/native_atomic_test.c Runtime/src/vc_atomic.c -o $(PORTABLE_ATOMIC_RUNTIME_TEST)
	@timeout 30s ./$(PORTABLE_ATOMIC_RUNTIME_TEST)
	@grep -Fq 'VC_NATIVE_MEMORY_ORDER_RELAXED' Runtime/include/vc_atomic.h && grep -Fq 'VC_NATIVE_MEMORY_ORDER_ACQUIRE' Runtime/include/vc_atomic.h && grep -Fq 'VC_NATIVE_MEMORY_ORDER_RELEASE' Runtime/include/vc_atomic.h && grep -Fq 'VC_NATIVE_MEMORY_ORDER_ACQ_REL' Runtime/include/vc_atomic.h && grep -Fq 'VC_NATIVE_MEMORY_ORDER_SEQ_CST' Runtime/include/vc_atomic.h; echo True
	@grep -Fq 'vc_native_atomic_i32_compare_exchange' Runtime/include/vc_atomic.h && grep -Fq 'vc_native_atomic_i32_fetch_add' Runtime/include/vc_atomic.h; echo True
	@grep -Fq 'vc_native_atomic_i64_compare_exchange' Runtime/include/vc_atomic.h && grep -Fq 'vc_native_atomic_i64_fetch_add' Runtime/include/vc_atomic.h; echo True
	@grep -Fq 'vc_native_atomic_iptr_compare_exchange' Runtime/include/vc_atomic.h && grep -Fq 'vc_native_atomic_iptr_fetch_add' Runtime/include/vc_atomic.h; echo True
	@grep -Fq 'vc_native_atomic_ptr_compare_exchange' Runtime/include/vc_atomic.h && grep -Fq 'vc_native_atomic_ptr_exchange' Runtime/include/vc_atomic.h; echo True
	@! grep -Eq 'windows.h|Interlocked|__atomic|pthread' Runtime/include/vc_atomic.h; echo True
	@grep -Fq '#if defined(_WIN32)' Runtime/src/vc_atomic.c && grep -Fq 'InterlockedCompareExchange' Runtime/src/vc_atomic.c && grep -Fq '#elif defined(__GNUC__) || defined(__clang__)' Runtime/src/vc_atomic.c && grep -Fq '__atomic_compare_exchange_n' Runtime/src/vc_atomic.c; echo True
	@grep -Fq '__ATOMIC_RELAXED' Runtime/src/vc_atomic.c && grep -Fq '__ATOMIC_ACQUIRE' Runtime/src/vc_atomic.c && grep -Fq '__ATOMIC_RELEASE' Runtime/src/vc_atomic.c && grep -Fq '__ATOMIC_ACQ_REL' Runtime/src/vc_atomic.c && grep -Fq '__ATOMIC_SEQ_CST' Runtime/src/vc_atomic.c; echo True
	@grep -Fq '#include "vc_atomic.h"' Tests/PortableAtomicRuntimeFoundation/.void/PortableAtomicRuntimeFoundation.c; echo True
	@grep -Fq 'vc_atomic.c' Compiler/src/compiler.c && grep -Fq 'vc_atomic_runtime.o' Compiler/src/compiler.c; echo True
	@./$(BIN) build Tests/ThreadSafeAllocationRuntimeRegistry/Library >/dev/null; ar t Tests/ThreadSafeAllocationRuntimeRegistry/Library/bin/libVoid163ThreadLib.a | grep -Fq 'vc_atomic_runtime.o'; echo True
	@./$(BIN) check Tests/MonitorPulseCompletion >/dev/null; ./$(BIN) check Tests/NativeConcurrencySynchronizationIntegrationAudit >/dev/null; echo True


# Milestone 175: Interlocked Integer Operations
test-interlocked-integer-operations: $(BIN)
	@./$(BIN) check Tests/InterlockedIntegerOperations >/dev/null; echo True
	@./$(BIN) build Tests/InterlockedIntegerOperations >/dev/null
	@timeout 20s ./Tests/InterlockedIntegerOperations/bin/InterlockedIntegerOperations
	@set -e; ./$(BIN) publish Tests/InterlockedIntegerOperations >/dev/null; test -x Tests/InterlockedIntegerOperations/publish/InterlockedIntegerOperations$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/InterlockedIntegerOperationsDiagnostics/InternalIntrinsic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'Runtime.InterlockedIncrementInt is reserved for Void.Threading.Interlocked'; echo True
	@set -e; output="$$(./$(BIN) check Tests/InterlockedIntegerOperationsDiagnostics/MissingRef 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type 'Interlocked' has no matching static method 'Increment'"; echo True
	@grep -Fq 'public static int Increment(ref int location)' StandardLibrary/Void/Threading/Interlocked.void && grep -Fq 'public static long Increment(ref long location)' StandardLibrary/Void/Threading/Interlocked.void && grep -Fq 'CompareExchange(ref int location' StandardLibrary/Void/Threading/Interlocked.void && grep -Fq 'CompareExchange(ref long location' StandardLibrary/Void/Threading/Interlocked.void; echo True
	@grep -Fq 'vc_native_atomic_i32_add' Tests/InterlockedIntegerOperations/.void/InterlockedIntegerOperations.c && grep -Fq 'vc_native_atomic_i64_add' Tests/InterlockedIntegerOperations/.void/InterlockedIntegerOperations.c && grep -Fq 'vc_native_atomic_i32_exchange' Tests/InterlockedIntegerOperations/.void/InterlockedIntegerOperations.c && grep -Fq 'vc_native_atomic_i64_compare_exchange' Tests/InterlockedIntegerOperations/.void/InterlockedIntegerOperations.c && grep -Fq 'VC_NATIVE_MEMORY_ORDER_SEQ_CST' Tests/InterlockedIntegerOperations/.void/InterlockedIntegerOperations.c; echo True
	@! grep -Eq 'InterlockedCompareExchange|__atomic_|windows.h' Tests/InterlockedIntegerOperations/.void/InterlockedIntegerOperations.c; echo True
	@grep -Fq 'vc_native_atomic_i32_add' Runtime/include/vc_atomic.h && grep -Fq 'vc_native_atomic_i64_add' Runtime/include/vc_atomic.h && grep -Fq '(uint32_t)previous + (uint32_t)value' Runtime/src/vc_atomic.c && grep -Fq '(uint64_t)previous + (uint64_t)value' Runtime/src/vc_atomic.c; echo True
	@set -e; for i in 1 2 3 4 5 6 7 8 9 10; do count="$$(timeout 20s ./Tests/InterlockedIntegerOperations/bin/InterlockedIntegerOperations | grep -c '^True$$')"; test "$$count" -eq 30; done; echo True
	@./$(BIN) check Tests/PortableAtomicRuntimeFoundation >/dev/null; echo True


# Milestone 176: Interlocked Managed-Reference Operations & GC Safety
test-interlocked-managed-reference-operations-gc-safety: $(BIN)
	@./$(BIN) check Tests/InterlockedManagedReferenceOperations >/dev/null; echo True
	@./$(BIN) build Tests/InterlockedManagedReferenceOperations >/dev/null
	@timeout 30s ./Tests/InterlockedManagedReferenceOperations/bin/InterlockedManagedReferenceOperations
	@set -e; ./$(BIN) publish Tests/InterlockedManagedReferenceOperations >/dev/null; test -x Tests/InterlockedManagedReferenceOperations/publish/InterlockedManagedReferenceOperations$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/InterlockedManagedReferenceOperationsDiagnostics/InternalIntrinsic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'Runtime.InterlockedExchangeRef is reserved for Void.Threading.Interlocked'; echo True
	@set -e; output="$$(./$(BIN) check Tests/InterlockedManagedReferenceOperationsDiagnostics/ValueType 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type argument 'int' must be a reference type for generic parameter 'T'"; echo True
	@grep -Fq 'public static T Exchange<T>(ref T location, T value) where T : class' StandardLibrary/Void/Threading/Interlocked.void && grep -Fq 'public static T CompareExchange<T>(ref T location, T value, T comparand) where T : class' StandardLibrary/Void/Threading/Interlocked.void; echo True
	@grep -Fq 'vc_native_atomic_ptr_exchange((void **)(void *)&(' Tests/InterlockedManagedReferenceOperations/.void/InterlockedManagedReferenceOperations.c && grep -Fq 'vc_native_atomic_ptr_compare_exchange((void **)(void *)&(' Tests/InterlockedManagedReferenceOperations/.void/InterlockedManagedReferenceOperations.c && grep -Fq 'VC_NATIVE_MEMORY_ORDER_SEQ_CST' Tests/InterlockedManagedReferenceOperations/.void/InterlockedManagedReferenceOperations.c; echo True
	@grep -Fq 'vc_gc_root_push(&vc_r_0, (void *)&(vc_p_1), vc_gc_trace_ref_slot);' Tests/InterlockedManagedReferenceOperations/.void/InterlockedManagedReferenceOperations.c && grep -Fq 'vc_gc_root_push(&vc_r_1, (void *)&(vc_p_2), vc_gc_trace_ref_slot);' Tests/InterlockedManagedReferenceOperations/.void/InterlockedManagedReferenceOperations.c; echo True
	@grep -Fq 'vc_c_2 * vc_ret_0 = ((vc_c_2 *)vc_native_atomic_ptr_exchange' Tests/InterlockedManagedReferenceOperations/.void/InterlockedManagedReferenceOperations.c && grep -Fq 'void * vc_ret_0 = ((void *)vc_native_atomic_ptr_exchange' Tests/InterlockedManagedReferenceOperations/.void/InterlockedManagedReferenceOperations.c && grep -Fq 'VcArray * vc_ret_0 = ((VcArray *)vc_native_atomic_ptr_exchange' Tests/InterlockedManagedReferenceOperations/.void/InterlockedManagedReferenceOperations.c; echo True
	@! grep -Eq 'InterlockedCompareExchangePointer|__atomic_|windows.h' Tests/InterlockedManagedReferenceOperations/.void/InterlockedManagedReferenceOperations.c; echo True
	@grep -Fq 'vc_gc_root_push' Tests/InterlockedManagedReferenceOperations/.void/InterlockedManagedReferenceOperations.c && grep -Fq 'vc_gc_safepoint' Tests/InterlockedManagedReferenceOperations/.void/InterlockedManagedReferenceOperations.c; echo True
	@set -e; for i in 1 2 3 4 5; do count="$$(timeout 30s ./Tests/InterlockedManagedReferenceOperations/bin/InterlockedManagedReferenceOperations | grep -c '^True$$')"; test "$$count" -eq 27; done; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-interlocked-integer-operations >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-portable-atomic-runtime-foundation >/dev/null; echo True; fi
	@./$(BIN) check Tests/ThreadedClosureDelegateGenericGcIntegration >/dev/null; ./$(BIN) check Tests/CooperativeStopTheWorldGc/Library >/dev/null; echo True


# Milestone 177: Volatile.Read / Volatile.Write & Memory Ordering
test-volatile-memory-ordering: $(BIN)
	@./$(BIN) check Tests/VolatileMemoryOrdering >/dev/null; echo True
	@./$(BIN) build Tests/VolatileMemoryOrdering >/dev/null
	@timeout 30s ./Tests/VolatileMemoryOrdering/bin/VolatileMemoryOrdering
	@set -e; ./$(BIN) publish Tests/VolatileMemoryOrdering >/dev/null; test -x Tests/VolatileMemoryOrdering/publish/VolatileMemoryOrdering$(EXE_SUFFIX); echo True
	@set -e; output="$$(./$(BIN) check Tests/VolatileMemoryOrderingDiagnostics/InternalIntrinsic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'Runtime.VolatileReadInt is reserved for Void.Threading.Volatile'; echo True
	@set -e; output="$$(./$(BIN) check Tests/VolatileMemoryOrderingDiagnostics/MissingRef 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "generic method type arguments for 'Volatile.Read' could not be inferred"; echo True
	@set -e; output="$$(./$(BIN) check Tests/VolatileMemoryOrderingDiagnostics/ValueType 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq "type argument 'int' must be a reference type for generic parameter 'T'"; echo True
	@grep -Fq 'public static int Read(ref int location)' StandardLibrary/Void/Threading/Volatile.void && grep -Fq 'public static long Read(ref long location)' StandardLibrary/Void/Threading/Volatile.void && grep -Fq 'public static T Read<T>(ref T location) where T : class' StandardLibrary/Void/Threading/Volatile.void && grep -Fq 'public static void Write<T>(ref T location, T value) where T : class' StandardLibrary/Void/Threading/Volatile.void; echo True
	@grep -Fq 'vc_native_atomic_i32_load' Tests/VolatileMemoryOrdering/.void/VolatileMemoryOrdering.c && grep -Fq 'vc_native_atomic_i64_load' Tests/VolatileMemoryOrdering/.void/VolatileMemoryOrdering.c && grep -Fq 'VC_NATIVE_MEMORY_ORDER_ACQUIRE' Tests/VolatileMemoryOrdering/.void/VolatileMemoryOrdering.c && grep -Fq 'VC_NATIVE_MEMORY_ORDER_RELEASE' Tests/VolatileMemoryOrdering/.void/VolatileMemoryOrdering.c; echo True
	@grep -Fq 'vc_native_atomic_ptr_load' Tests/VolatileMemoryOrdering/.void/VolatileMemoryOrdering.c && grep -Fq 'vc_native_atomic_ptr_store' Tests/VolatileMemoryOrdering/.void/VolatileMemoryOrdering.c; echo True
	@grep -Fq 'vc_gc_root_push(&vc_r_0, (void *)&(vc_p_1), vc_gc_trace_ref_slot);' Tests/VolatileMemoryOrdering/.void/VolatileMemoryOrdering.c && grep -Fq 'VcGcRoot vc_r_' Tests/VolatileMemoryOrdering/.void/VolatileMemoryOrdering.c; echo True
	@! grep -Eq 'InterlockedCompareExchange|__atomic_|windows.h' Tests/VolatileMemoryOrdering/.void/VolatileMemoryOrdering.c; echo True
	@grep -Fq 'VC_NATIVE_MEMORY_ORDER_ACQUIRE' Runtime/include/vc_atomic.h && grep -Fq 'VC_NATIVE_MEMORY_ORDER_RELEASE' Runtime/include/vc_atomic.h; echo True
	@set -e; for i in 1 2 3 4 5 6 7 8 9 10; do count="$$(timeout 30s ./Tests/VolatileMemoryOrdering/bin/VolatileMemoryOrdering | grep -c '^True$$')"; test "$$count" -eq 29; done; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-interlocked-managed-reference-operations-gc-safety >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-portable-atomic-runtime-foundation >/dev/null; echo True; fi
	@./$(BIN) check Tests/ThreadedClosureDelegateGenericGcIntegration >/dev/null; ./$(BIN) check Tests/CooperativeStopTheWorldGc/Library >/dev/null; echo True


# Milestone 178: Synchronization Exception, Native Boundary & Cleanup Integration
test-synchronization-exception-native-boundary-cleanup-integration: $(BIN) $(SYNC_EXCEPTION_NATIVE_FIXTURE)
	@./$(BIN) check Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration >/dev/null; echo True
	@./$(BIN) build Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration >/dev/null
	@timeout 45s ./Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/bin/SynchronizationExceptionNativeBoundaryCleanupIntegration
	@set -e; ./$(BIN) publish Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration >/dev/null; test -x Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/publish/SynchronizationExceptionNativeBoundaryCleanupIntegration$(EXE_SUFFIX); echo True
	@grep -Fq 'vc_monitor_wait' Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/.void/SynchronizationExceptionNativeBoundaryCleanupIntegration.c && grep -Fq 'vc_monitor_broadcast' Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/.void/SynchronizationExceptionNativeBoundaryCleanupIntegration.c && grep -Fq 'vc_monitor_exit' Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/.void/SynchronizationExceptionNativeBoundaryCleanupIntegration.c; echo True
	@grep -Fq 'vc_native_atomic_ptr_exchange' Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/.void/SynchronizationExceptionNativeBoundaryCleanupIntegration.c && grep -Fq 'VC_NATIVE_MEMORY_ORDER_SEQ_CST' Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/.void/SynchronizationExceptionNativeBoundaryCleanupIntegration.c && grep -Fq 'VC_NATIVE_MEMORY_ORDER_ACQUIRE' Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/.void/SynchronizationExceptionNativeBoundaryCleanupIntegration.c && grep -Fq 'VC_NATIVE_MEMORY_ORDER_RELEASE' Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/.void/SynchronizationExceptionNativeBoundaryCleanupIntegration.c; echo True
	@grep -Fq 'vc_native_pending_exception' Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/.void/SynchronizationExceptionNativeBoundaryCleanupIntegration.c && grep -Fq 'vc_native_take_pending_exception' Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/.void/SynchronizationExceptionNativeBoundaryCleanupIntegration.c && grep -Fq 'VcExceptionHandler vc_boundary' Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/.void/SynchronizationExceptionNativeBoundaryCleanupIntegration.c; echo True
	@grep -Fq 'vc_gc_root_push' Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/.void/SynchronizationExceptionNativeBoundaryCleanupIntegration.c && grep -Fq 'vc_gc_safepoint' Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/.void/SynchronizationExceptionNativeBoundaryCleanupIntegration.c; echo True
	@! grep -Eq 'pthread_|windows.h|InterlockedCompareExchange|__atomic_' Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/.void/SynchronizationExceptionNativeBoundaryCleanupIntegration.c; echo True
	@set -e; for i in 1 2 3 4 5 6 7 8 9 10; do count="$$(timeout 30s ./Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration/bin/SynchronizationExceptionNativeBoundaryCleanupIntegration | grep -c '^True$$')"; test "$$count" -eq 19; done; echo True
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-monitor-pulse-completion >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-volatile-memory-ordering >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-interlocked-managed-reference-operations-gc-safety >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-thread-exception-native-boundary-cleanup-integration >/dev/null; echo True; fi
	@if [ "$$VOID_FULL_SUITE" = "1" ]; then echo True; else $(MAKE) -s test-native-library-boundary-exception-integration >/dev/null; echo True; fi


# Milestone 179: Threaded Generic, Closure & Delegate Synchronization Integration
test-threaded-generic-closure-delegate-synchronization-integration: $(BIN)
	@./$(BIN) check Tests/ThreadedGenericClosureDelegateSynchronizationIntegration >/dev/null; echo True
	@./$(BIN) build Tests/ThreadedGenericClosureDelegateSynchronizationIntegration >/dev/null
	@timeout 45s ./Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/bin/ThreadedGenericClosureDelegateSynchronizationIntegration
	@set -e; ./$(BIN) publish Tests/ThreadedGenericClosureDelegateSynchronizationIntegration >/dev/null; test -x Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/publish/ThreadedGenericClosureDelegateSynchronizationIntegration$(EXE_SUFFIX); echo True
	@grep -Fq 'vc_monitor_wait' Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void/ThreadedGenericClosureDelegateSynchronizationIntegration.c && grep -Fq 'vc_monitor_broadcast' Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void/ThreadedGenericClosureDelegateSynchronizationIntegration.c && grep -Fq 'vc_monitor_exit' Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void/ThreadedGenericClosureDelegateSynchronizationIntegration.c; echo True
	@grep -Fq 'vc_native_atomic_ptr_exchange' Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void/ThreadedGenericClosureDelegateSynchronizationIntegration.c && grep -Fq 'vc_native_atomic_i32_add' Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void/ThreadedGenericClosureDelegateSynchronizationIntegration.c && grep -Fq 'VC_NATIVE_MEMORY_ORDER_SEQ_CST' Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void/ThreadedGenericClosureDelegateSynchronizationIntegration.c; echo True
	@grep -Fq 'vc_native_atomic_ptr_load' Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void/ThreadedGenericClosureDelegateSynchronizationIntegration.c && grep -Fq 'vc_native_atomic_ptr_store' Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void/ThreadedGenericClosureDelegateSynchronizationIntegration.c && grep -Fq 'VC_NATIVE_MEMORY_ORDER_ACQUIRE' Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void/ThreadedGenericClosureDelegateSynchronizationIntegration.c && grep -Fq 'VC_NATIVE_MEMORY_ORDER_RELEASE' Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void/ThreadedGenericClosureDelegateSynchronizationIntegration.c; echo True
	@grep -Fq 'GenericRunner__g1_ScoreNode' Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void/ThreadedGenericClosureDelegateSynchronizationIntegration.c && grep -Fq 'vc_delegate_combine_' Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void/ThreadedGenericClosureDelegateSynchronizationIntegration.c && grep -Fq 'vc_managed_thread_worker' Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void/ThreadedGenericClosureDelegateSynchronizationIntegration.c && grep -Fq 'vc_gc_root_push' Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void/ThreadedGenericClosureDelegateSynchronizationIntegration.c && grep -Fq 'vc_gc_safepoint' Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void/ThreadedGenericClosureDelegateSynchronizationIntegration.c; echo True
	@! grep -Eq 'pthread_|windows.h|CreateThread|WaitForSingleObject|InterlockedCompareExchange|__atomic_' Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/.void/ThreadedGenericClosureDelegateSynchronizationIntegration.c; echo True
	@set -e; for i in 1 2 3 4 5 6 7 8 9 10; do count="$$(timeout 30s ./Tests/ThreadedGenericClosureDelegateSynchronizationIntegration/bin/ThreadedGenericClosureDelegateSynchronizationIntegration | grep -c '^True$$')"; test "$$count" -eq 17; done; echo True
	@./$(BIN) check Tests/ThreadedClosureDelegateGenericGcIntegration >/dev/null; echo True
	@./$(BIN) check Tests/MonitorPulseCompletion >/dev/null; echo True
	@./$(BIN) check Tests/InterlockedManagedReferenceOperations >/dev/null; echo True
	@./$(BIN) check Tests/VolatileMemoryOrdering >/dev/null; echo True
	@./$(BIN) check Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration >/dev/null; echo True


# Milestone 180: Advanced Synchronization & Atomic Integration Audit
test-advanced-synchronization-atomic-integration-audit: $(BIN)
	@./$(BIN) check Tests/AdvancedSynchronizationAtomicIntegrationAudit >/dev/null; echo True
	@./$(BIN) build Tests/AdvancedSynchronizationAtomicIntegrationAudit >/dev/null
	@timeout 45s ./Tests/AdvancedSynchronizationAtomicIntegrationAudit/bin/AdvancedSynchronizationAtomicIntegrationAudit
	@set -e; ./$(BIN) publish Tests/AdvancedSynchronizationAtomicIntegrationAudit >/dev/null; test -x Tests/AdvancedSynchronizationAtomicIntegrationAudit/publish/AdvancedSynchronizationAtomicIntegrationAudit$(EXE_SUFFIX); echo True
	@grep -Fq 'vc_monitor_wait' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c && grep -Fq 'vc_monitor_signal' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c && grep -Fq 'vc_monitor_broadcast' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c && grep -Fq 'vc_gc_native_call_begin' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c; echo True
	@grep -Fq 'vc_native_atomic_i32_compare_exchange' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c && grep -Fq 'vc_native_atomic_i64_compare_exchange' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c && grep -Fq 'VC_NATIVE_MEMORY_ORDER_SEQ_CST' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c && grep -Fq 'VC_NATIVE_MEMORY_ORDER_ACQUIRE' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c && grep -Fq 'VC_NATIVE_MEMORY_ORDER_RELEASE' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c; echo True
	@grep -Fq 'vc_native_atomic_ptr_exchange' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c && grep -Fq 'vc_native_atomic_ptr_compare_exchange' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c && grep -Fq 'vc_native_atomic_ptr_load' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c && grep -Fq 'vc_native_atomic_ptr_store' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c; echo True
	@grep -Fq 'AuditRunner__g1_AlternateAuditNode' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c && grep -Fq 'vc_delegate_combine_' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c && grep -Fq 'vc_managed_thread_worker' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c && grep -Fq 'vc_gc_root_push' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c && grep -Fq 'vc_gc_safepoint' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c && grep -Fq 'VcExceptionHandler vc_boundary' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c; echo True
	@! grep -Eq 'pthread_|#include <windows.h>|CreateThread|WaitForSingleObject|AcquireSRWLock|SleepConditionVariable|__atomic_' Tests/AdvancedSynchronizationAtomicIntegrationAudit/.void/AdvancedSynchronizationAtomicIntegrationAudit.c; echo True
	@! grep -Eq 'pthread_|windows.h|__atomic_|InterlockedCompareExchange' Runtime/include/vc_thread.h Runtime/include/vc_atomic.h && grep -Fq '#if defined(_WIN32)' Runtime/src/vc_thread.c && grep -Fq '#elif defined(__unix__) || defined(__APPLE__)' Runtime/src/vc_thread.c && grep -Fq '#if defined(_WIN32)' Runtime/src/vc_atomic.c && grep -Fq '#elif defined(__GNUC__) || defined(__clang__)' Runtime/src/vc_atomic.c; echo True
	@set -e; for i in 1 2 3 4 5 6 7 8 9 10; do count="$$(timeout 30s ./Tests/AdvancedSynchronizationAtomicIntegrationAudit/bin/AdvancedSynchronizationAtomicIntegrationAudit | grep -c '^True$$')"; test "$$count" -eq 30; done; echo True
	@./$(BIN) check Tests/MonitorConditionWaitRuntimeFoundation >/dev/null; ./$(BIN) check Tests/ManagedMonitorWaitCompletion >/dev/null; ./$(BIN) check Tests/MonitorPulseCompletion >/dev/null; ./$(BIN) check Tests/PortableAtomicRuntimeFoundation >/dev/null; ./$(BIN) check Tests/InterlockedIntegerOperations >/dev/null; ./$(BIN) check Tests/InterlockedManagedReferenceOperations >/dev/null; ./$(BIN) check Tests/VolatileMemoryOrdering >/dev/null; ./$(BIN) check Tests/SynchronizationExceptionNativeBoundaryCleanupIntegration >/dev/null; ./$(BIN) check Tests/ThreadedGenericClosureDelegateSynchronizationIntegration >/dev/null; echo True


# Milestone 181: Monotonic Time & Timed-Wait Runtime Foundation
test-monotonic-time-timed-wait-runtime-foundation: $(BIN)
	@./$(BIN) check Tests/MonotonicTimeTimedWaitRuntimeFoundation >/dev/null; echo True
	@./$(BIN) build Tests/MonotonicTimeTimedWaitRuntimeFoundation >/dev/null
	@timeout 10s ./Tests/MonotonicTimeTimedWaitRuntimeFoundation/bin/MonotonicTimeTimedWaitRuntimeFoundation
	@set -e; ./$(BIN) publish Tests/MonotonicTimeTimedWaitRuntimeFoundation >/dev/null; test -x Tests/MonotonicTimeTimedWaitRuntimeFoundation/publish/MonotonicTimeTimedWaitRuntimeFoundation$(EXE_SUFFIX); echo True
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/MonotonicTimeTimedWaitRuntimeFoundation/native_timed_wait_test.c Runtime/src/vc_thread.c -o $(MONOTONIC_TIMED_WAIT_RUNTIME_TEST)
	@timeout 15s ./$(MONOTONIC_TIMED_WAIT_RUNTIME_TEST)
	@grep -Fq 'bool vc_native_monotonic_time_ns(uint64_t *nanoseconds);' Runtime/include/vc_thread.h && grep -Fq 'bool vc_native_sleep_ms(uint32_t milliseconds);' Runtime/include/vc_thread.h && grep -Fq 'VcNativeWaitResult vc_native_monitor_wait_timed(VcNativeMonitor *monitor, void *owner, uint32_t milliseconds);' Runtime/include/vc_thread.h; echo True
	@grep -Fq 'VC_NATIVE_WAIT_SIGNALED' Runtime/include/vc_thread.h && grep -Fq 'VC_NATIVE_WAIT_TIMED_OUT' Runtime/include/vc_thread.h && grep -Fq 'VC_NATIVE_TIMEOUT_INFINITE' Runtime/include/vc_thread.h; echo True
	@grep -Fq 'QueryPerformanceCounter' Runtime/src/vc_thread.c && grep -Fq 'SleepConditionVariableSRW' Runtime/src/vc_thread.c && grep -Fq 'GetLastError' Runtime/src/vc_thread.c; echo True
	@grep -Fq 'CLOCK_MONOTONIC' Runtime/src/vc_thread.c && grep -Fq 'pthread_condattr_setclock' Runtime/src/vc_thread.c && grep -Fq 'pthread_cond_timedwait' Runtime/src/vc_thread.c && grep -Fq 'nanosleep' Runtime/src/vc_thread.c; echo True
	@grep -Fq 'pthread_cond_timedwait_relative_np' Runtime/src/vc_thread.c; echo True
	@! grep -Eq '#include <pthread.h>|#include <windows.h>|QueryPerformanceCounter|nanosleep' Runtime/include/vc_thread.h; echo True
	@grep -Fq 'VC_NATIVE_WAIT_TIMED_OUT' Runtime/include/vc_thread.h; echo True
	@./$(BIN) check Tests/MonitorConditionWaitRuntimeFoundation >/dev/null; ./$(BIN) check Tests/AdvancedSynchronizationAtomicIntegrationAudit >/dev/null; echo True
	@set -e; out=$$(mktemp); VOID_TEST_SLOW_START=1 timeout 15s ./$(MONOTONIC_TIMED_WAIT_RUNTIME_TEST) >$$out; test "$$(grep -c '^True$$' $$out)" -eq 39; ! grep -Fxq False $$out; rm -f $$out


# Milestone 182: Managed Thread.Sleep & Timeout Contract
test-managed-thread-sleep-timeout-contract: $(BIN)
	@./$(BIN) check Tests/ManagedThreadSleepTimeoutContract >/dev/null; echo True
	@./$(BIN) build Tests/ManagedThreadSleepTimeoutContract >/dev/null
	@timeout 20s ./Tests/ManagedThreadSleepTimeoutContract/bin/ManagedThreadSleepTimeoutContract
	@set -e; for i in 1 2 3 4 5 6 7 8 9 10; do count="$$(timeout 20s ./Tests/ManagedThreadSleepTimeoutContract/bin/ManagedThreadSleepTimeoutContract | grep -c '^True$$')"; test "$$count" -eq 9; done; echo True
	@set -e; ./$(BIN) publish Tests/ManagedThreadSleepTimeoutContract >/dev/null; test -x Tests/ManagedThreadSleepTimeoutContract/publish/ManagedThreadSleepTimeoutContract$(EXE_SUFFIX); echo True
	@grep -Fq 'public static void Sleep(int millisecondsTimeout)' StandardLibrary/Void/Threading/Thread.void && grep -Fq 'Runtime.ThreadSleep(millisecondsTimeout);' StandardLibrary/Void/Threading/Thread.void; echo True
	@grep -Fq 'public const int Infinite = -1;' StandardLibrary/Void/Threading/Timeout.void; echo True
	@grep -Fq 'static VC_MAYBE_UNUSED void vc_managed_thread_sleep(int32_t milliseconds_timeout)' Tests/ManagedThreadSleepTimeoutContract/.void/ManagedThreadSleepTimeoutContract.c && grep -Fq 'VC_NATIVE_TIMEOUT_INFINITE' Tests/ManagedThreadSleepTimeoutContract/.void/ManagedThreadSleepTimeoutContract.c && grep -Fq 'vc_native_sleep_ms(native_timeout)' Tests/ManagedThreadSleepTimeoutContract/.void/ManagedThreadSleepTimeoutContract.c; echo True
	@sleep_line="$$(grep -n 'static VC_MAYBE_UNUSED void vc_managed_thread_sleep' Tests/ManagedThreadSleepTimeoutContract/.void/ManagedThreadSleepTimeoutContract.c | head -n1 | cut -d: -f1)"; begin_line="$$(tail -n +"$$sleep_line" Tests/ManagedThreadSleepTimeoutContract/.void/ManagedThreadSleepTimeoutContract.c | grep -n 'vc_gc_native_call_begin' | head -n1 | cut -d: -f1)"; native_line="$$(tail -n +"$$sleep_line" Tests/ManagedThreadSleepTimeoutContract/.void/ManagedThreadSleepTimeoutContract.c | grep -n 'vc_native_sleep_ms' | head -n1 | cut -d: -f1)"; end_line="$$(tail -n +"$$sleep_line" Tests/ManagedThreadSleepTimeoutContract/.void/ManagedThreadSleepTimeoutContract.c | grep -n 'vc_gc_native_call_end' | head -n1 | cut -d: -f1)"; test -n "$$begin_line" -a -n "$$native_line" -a -n "$$end_line" -a "$$begin_line" -lt "$$native_line" -a "$$native_line" -lt "$$end_line"; echo True
	@grep -Fq 'public static void Sleep(int millisecondsTimeout)' StandardLibrary/Void/Threading/Thread.void; echo True
	@./$(BIN) check Tests/ManagedThreadSleepTimeoutContractDiagnostics/InternalIntrinsic >/dev/null 2>&1 && exit 1 || true; output="$$(./$(BIN) check Tests/ManagedThreadSleepTimeoutContractDiagnostics/InternalIntrinsic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'Runtime.ThreadSleep is reserved for Void.Threading.Thread'; echo True
	@./$(BIN) build Tests/ManagedThreadSleepTimeoutContractRuntime/InvalidTimeout >/dev/null; output="$$(timeout 10s ./Tests/ManagedThreadSleepTimeoutContractRuntime/InvalidTimeout/bin/InvalidTimeout 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: thread sleep timeout must be -1 or non-negative'; echo True
	@./$(BIN) build Tests/ManagedThreadSleepTimeoutContractRuntime/Infinite >/dev/null; output="$$(timeout 5s ./Tests/ManagedThreadSleepTimeoutContractRuntime/Infinite/bin/Infinite)"; test "$$(printf '%s\n' "$$output" | grep -c '^True$$')" -eq 1; echo True
	@./$(BIN) check Tests/MonotonicTimeTimedWaitRuntimeFoundation >/dev/null; ./$(BIN) check Tests/ManagedThreadSurface >/dev/null; echo True


# Milestone 183: Timed Monitor.Wait Completion
test-timed-monitor-wait-completion: $(BIN)
	@./$(BIN) check Tests/TimedMonitorWaitCompletion >/dev/null; echo True
	@./$(BIN) build Tests/TimedMonitorWaitCompletion >/dev/null
	@timeout 20s ./Tests/TimedMonitorWaitCompletion/bin/TimedMonitorWaitCompletion
	@set -e; for i in 1 2 3 4 5 6 7 8 9 10; do count="$$(timeout 20s ./Tests/TimedMonitorWaitCompletion/bin/TimedMonitorWaitCompletion | grep -c '^True$$')"; test "$$count" -eq 14; done; echo True
	@set -e; ./$(BIN) publish Tests/TimedMonitorWaitCompletion >/dev/null; test -x Tests/TimedMonitorWaitCompletion/publish/TimedMonitorWaitCompletion$(EXE_SUFFIX); echo True
	@grep -Fq 'public static bool Wait(object value, int millisecondsTimeout)' StandardLibrary/Void/Threading/Monitor.void && grep -Fq 'return Runtime.MonitorWait(value, millisecondsTimeout);' StandardLibrary/Void/Threading/Monitor.void; echo True
	@grep -Fq 'public const int Infinite = -1;' StandardLibrary/Void/Threading/Timeout.void; echo True
	@grep -Fq 'static VC_MAYBE_UNUSED bool vc_monitor_wait_timed(void *object, int32_t milliseconds_timeout)' Tests/TimedMonitorWaitCompletion/.void/TimedMonitorWaitCompletion.c && grep -Fq 'vc_native_monitor_wait_timed(monitor, (void *)context, native_timeout)' Tests/TimedMonitorWaitCompletion/.void/TimedMonitorWaitCompletion.c; echo True
	@grep -Fq 'milliseconds_timeout == -1 ? VC_NATIVE_TIMEOUT_INFINITE' Tests/TimedMonitorWaitCompletion/.void/TimedMonitorWaitCompletion.c && grep -Fq 'return result == VC_NATIVE_WAIT_SIGNALED;' Tests/TimedMonitorWaitCompletion/.void/TimedMonitorWaitCompletion.c; echo True
	@wait_line="$$(grep -n 'static VC_MAYBE_UNUSED bool vc_monitor_wait_timed' Tests/TimedMonitorWaitCompletion/.void/TimedMonitorWaitCompletion.c | head -n1 | cut -d: -f1)"; begin_line="$$(tail -n +"$$wait_line" Tests/TimedMonitorWaitCompletion/.void/TimedMonitorWaitCompletion.c | grep -n 'vc_gc_native_call_begin' | head -n1 | cut -d: -f1)"; native_line="$$(tail -n +"$$wait_line" Tests/TimedMonitorWaitCompletion/.void/TimedMonitorWaitCompletion.c | grep -n 'vc_native_monitor_wait_timed' | head -n1 | cut -d: -f1)"; end_line="$$(tail -n +"$$wait_line" Tests/TimedMonitorWaitCompletion/.void/TimedMonitorWaitCompletion.c | grep -n 'vc_gc_native_call_end' | head -n1 | cut -d: -f1)"; test -n "$$begin_line" -a -n "$$native_line" -a -n "$$end_line" -a "$$begin_line" -lt "$$native_line" -a "$$native_line" -lt "$$end_line"; echo True
	@grep -Fq 'static VC_MAYBE_UNUSED bool vc_monitor_wait(void *object)' Tests/TimedMonitorWaitCompletion/.void/TimedMonitorWaitCompletion.c && grep -Fq 'vc_native_monitor_wait(monitor, (void *)context)' Tests/TimedMonitorWaitCompletion/.void/TimedMonitorWaitCompletion.c; echo True
	@grep -Fq 'argument_count != 1 && argument_count != 2' Compiler/src/compiler.c && grep -Fq 'argument_types[1] == VC_SEM_TYPE_INT' Compiler/src/semantic.c; echo True
	@set -e; output="$$(./$(BIN) check Tests/TimedMonitorWaitCompletionDiagnostics/InternalIntrinsic 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'Runtime.MonitorWait is reserved for Void.Threading.Monitor'; echo True
	@./$(BIN) build Tests/TimedMonitorWaitCompletionRuntime/InvalidTimeout >/dev/null; set -e; output="$$(timeout 10s ./Tests/TimedMonitorWaitCompletionRuntime/InvalidTimeout/bin/TimedMonitorWaitInvalidTimeout 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor wait timeout must be -1 or non-negative'; echo True
	@./$(BIN) build Tests/TimedMonitorWaitCompletionRuntime/WaitWithoutOwnership >/dev/null; set -e; output="$$(timeout 10s ./Tests/TimedMonitorWaitCompletionRuntime/WaitWithoutOwnership/bin/TimedMonitorWaitWaitWithoutOwnership 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor wait requires ownership'; echo True
	@./$(BIN) build Tests/TimedMonitorWaitCompletionRuntime/NullTarget >/dev/null; set -e; output="$$(timeout 10s ./Tests/TimedMonitorWaitCompletionRuntime/NullTarget/bin/TimedMonitorWaitNullTarget 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor object cannot be null'; echo True
	@./$(BIN) build Tests/TimedMonitorWaitCompletionRuntime/WrongOwner >/dev/null; set -e; output="$$(timeout 10s ./Tests/TimedMonitorWaitCompletionRuntime/WrongOwner/bin/TimedMonitorWaitWrongOwner 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: monitor wait requires ownership'; echo True
	@./$(BIN) check Tests/MonotonicTimeTimedWaitRuntimeFoundation >/dev/null; ./$(BIN) check Tests/ManagedThreadSleepTimeoutContract >/dev/null; ./$(BIN) check Tests/MonitorPulseCompletion >/dev/null; echo True


# Milestone 184: ManualResetEvent Completion
test-manual-reset-event-completion: $(BIN)
	@./$(BIN) check Tests/ManualResetEventCompletion >/dev/null; echo True
	@./$(BIN) build Tests/ManualResetEventCompletion >/dev/null
	@timeout 20s ./Tests/ManualResetEventCompletion/bin/ManualResetEventCompletion
	@set -e; for i in 1 2 3 4 5 6 7 8 9 10; do count="$$(timeout 20s ./Tests/ManualResetEventCompletion/bin/ManualResetEventCompletion | grep -c '^True$$')"; test "$$count" -eq 22; done; echo True
	@set -e; ./$(BIN) publish Tests/ManualResetEventCompletion >/dev/null; test -x Tests/ManualResetEventCompletion/publish/ManualResetEventCompletion$(EXE_SUFFIX); echo True
	@grep -Fq 'public sealed class ManualResetEvent' StandardLibrary/Void/Threading/ManualResetEvent.void && grep -Fq 'public bool Set()' StandardLibrary/Void/Threading/ManualResetEvent.void && grep -Fq 'public bool Reset()' StandardLibrary/Void/Threading/ManualResetEvent.void && grep -Fq 'public bool WaitOne()' StandardLibrary/Void/Threading/ManualResetEvent.void && grep -Fq 'public bool WaitOne(int millisecondsTimeout)' StandardLibrary/Void/Threading/ManualResetEvent.void; echo True
	@grep -Fq 'private ManualResetEventGate _gate;' StandardLibrary/Void/Threading/ManualResetEvent.void && grep -Fq 'Interlocked.Increment(ref _generation);' StandardLibrary/Void/Threading/ManualResetEvent.void && grep -Fq 'Monitor.PulseAll(_gate);' StandardLibrary/Void/Threading/ManualResetEvent.void; echo True
	@grep -Fq 'Monitor.Wait(_gate, millisecondsTimeout)' StandardLibrary/Void/Threading/ManualResetEvent.void && grep -Fq 'bool released = notified && (_signaled || _generation != generation);' StandardLibrary/Void/Threading/ManualResetEvent.void; echo True
	@grep -Fq 'vc_monitor_wait_timed(' Tests/ManualResetEventCompletion/.void/ManualResetEventCompletion.c && grep -Fq 'vc_monitor_broadcast(' Tests/ManualResetEventCompletion/.void/ManualResetEventCompletion.c; echo True
	@./$(BIN) build Tests/ManualResetEventCompletionRuntime/InvalidTimeout >/dev/null; set -e; output="$$(timeout 10s ./Tests/ManualResetEventCompletionRuntime/InvalidTimeout/bin/ManualResetEventInvalidTimeout 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: manual reset event wait timeout must be -1 or non-negative'; echo True
	@./$(BIN) check Tests/TimedMonitorWaitCompletion >/dev/null


# Milestone 185: AutoResetEvent Completion
test-auto-reset-event-completion: $(BIN)
	@./$(BIN) check Tests/AutoResetEventCompletion >/dev/null; echo True
	@./$(BIN) build Tests/AutoResetEventCompletion >/dev/null
	@timeout 20s ./Tests/AutoResetEventCompletion/bin/AutoResetEventCompletion
	@set -e; for i in 1 2 3 4 5 6 7 8 9 10; do count="$$(timeout 20s ./Tests/AutoResetEventCompletion/bin/AutoResetEventCompletion | grep -c '^True$$')"; test "$$count" -eq 30; done; echo True
	@set -e; ./$(BIN) publish Tests/AutoResetEventCompletion >/dev/null; test -x Tests/AutoResetEventCompletion/publish/AutoResetEventCompletion$(EXE_SUFFIX); echo True
	@grep -Fq 'public sealed class AutoResetEvent' StandardLibrary/Void/Threading/AutoResetEvent.void && grep -Fq 'public bool Set()' StandardLibrary/Void/Threading/AutoResetEvent.void && grep -Fq 'public bool Reset()' StandardLibrary/Void/Threading/AutoResetEvent.void && grep -Fq 'public bool WaitOne()' StandardLibrary/Void/Threading/AutoResetEvent.void && grep -Fq 'public bool WaitOne(int millisecondsTimeout)' StandardLibrary/Void/Threading/AutoResetEvent.void; echo True
	@grep -Fq 'private sealed class AutoResetEventWaiter' StandardLibrary/Void/Threading/AutoResetEvent.void && grep -Fq 'AutoResetEventWaiter waiter = this.Dequeue();' StandardLibrary/Void/Threading/AutoResetEvent.void && grep -Fq 'Monitor.Pulse(waiter);' StandardLibrary/Void/Threading/AutoResetEvent.void && grep -Fq 'bool removed = this.Remove(waiter);' StandardLibrary/Void/Threading/AutoResetEvent.void; echo True
	@grep -Fq 'Monitor.Wait(waiter, millisecondsTimeout)' StandardLibrary/Void/Threading/AutoResetEvent.void && grep -Fq 'return !removed;' StandardLibrary/Void/Threading/AutoResetEvent.void; echo True
	@grep -Fq 'vc_monitor_wait_timed(' Tests/AutoResetEventCompletion/.void/AutoResetEventCompletion.c && grep -Fq 'vc_monitor_signal(' Tests/AutoResetEventCompletion/.void/AutoResetEventCompletion.c; echo True
	@! grep -R -Fq 'AutoResetEvent' Compiler/src Runtime/include Runtime/src; echo True
	@./$(BIN) build Tests/AutoResetEventCompletionRuntime/InvalidTimeout >/dev/null; set -e; output="$$(timeout 10s ./Tests/AutoResetEventCompletionRuntime/InvalidTimeout/bin/AutoResetEventInvalidTimeout 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: auto reset event wait timeout must be -1 or non-negative'; echo True
	@./$(BIN) check Tests/ManualResetEventCompletion >/dev/null; ./$(BIN) check Tests/TimedMonitorWaitCompletion >/dev/null; echo True

# Milestone 186: Semaphore Completion
test-semaphore-completion: $(BIN)
	@./$(BIN) check Tests/SemaphoreCompletion >/dev/null; echo True
	@./$(BIN) build Tests/SemaphoreCompletion >/dev/null
	@timeout 20s ./Tests/SemaphoreCompletion/bin/SemaphoreCompletion
	@set -e; ./$(BIN) publish Tests/SemaphoreCompletion >/dev/null; test -x Tests/SemaphoreCompletion/publish/SemaphoreCompletion$(EXE_SUFFIX); echo True
	@grep -Fq 'public sealed class Semaphore' StandardLibrary/Void/Threading/Semaphore.void && grep -Fq 'public Semaphore(int initialCount, int maximumCount)' StandardLibrary/Void/Threading/Semaphore.void && grep -Fq 'public bool WaitOne()' StandardLibrary/Void/Threading/Semaphore.void && grep -Fq 'public bool WaitOne(int millisecondsTimeout)' StandardLibrary/Void/Threading/Semaphore.void && grep -Fq 'public int Release()' StandardLibrary/Void/Threading/Semaphore.void && grep -Fq 'public int Release(int releaseCount)' StandardLibrary/Void/Threading/Semaphore.void; echo True
	@grep -Fq 'private sealed class SemaphoreWaiter' StandardLibrary/Void/Threading/Semaphore.void && grep -Fq 'SemaphoreWaiter waiter = this.Dequeue();' StandardLibrary/Void/Threading/Semaphore.void && grep -Fq 'bool removed = this.Remove(waiter);' StandardLibrary/Void/Threading/Semaphore.void && grep -Fq 'this.ReleaseWaiter(waiter);' StandardLibrary/Void/Threading/Semaphore.void; echo True
	@grep -Fq 'if (releaseCount > _maximumCount - _count)' StandardLibrary/Void/Threading/Semaphore.void && grep -Fq 'int previousCount = _count;' StandardLibrary/Void/Threading/Semaphore.void && grep -Fq 'return previousCount;' StandardLibrary/Void/Threading/Semaphore.void; echo True
	@grep -Fq 'vc_monitor_wait_timed(' Tests/SemaphoreCompletion/.void/SemaphoreCompletion.c && grep -Fq 'vc_monitor_signal(' Tests/SemaphoreCompletion/.void/SemaphoreCompletion.c; echo True
	@! grep -R -Fq 'Semaphore' Compiler/src Runtime/include Runtime/src; echo True
	@./$(BIN) build Tests/SemaphoreCompletionRuntime/InvalidMaximum >/dev/null; set -e; output="$$(timeout 10s ./Tests/SemaphoreCompletionRuntime/InvalidMaximum/bin/SemaphoreInvalidMaximum 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: semaphore maximum count must be positive'; echo True
	@./$(BIN) build Tests/SemaphoreCompletionRuntime/InvalidInitial >/dev/null; set -e; output="$$(timeout 10s ./Tests/SemaphoreCompletionRuntime/InvalidInitial/bin/SemaphoreInvalidInitial 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: semaphore initial count must be non-negative'; echo True
	@./$(BIN) build Tests/SemaphoreCompletionRuntime/InitialExceedsMaximum >/dev/null; set -e; output="$$(timeout 10s ./Tests/SemaphoreCompletionRuntime/InitialExceedsMaximum/bin/SemaphoreInitialExceedsMaximum 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: semaphore initial count cannot exceed maximum count'; echo True
	@./$(BIN) build Tests/SemaphoreCompletionRuntime/InvalidTimeout >/dev/null; set -e; output="$$(timeout 10s ./Tests/SemaphoreCompletionRuntime/InvalidTimeout/bin/SemaphoreInvalidTimeout 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: semaphore wait timeout must be -1 or non-negative'; echo True
	@./$(BIN) build Tests/SemaphoreCompletionRuntime/InvalidReleaseCount >/dev/null; set -e; output="$$(timeout 10s ./Tests/SemaphoreCompletionRuntime/InvalidReleaseCount/bin/SemaphoreInvalidReleaseCount 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: semaphore release count must be positive'; echo True
	@./$(BIN) build Tests/SemaphoreCompletionRuntime/OverRelease >/dev/null; set -e; output="$$(timeout 10s ./Tests/SemaphoreCompletionRuntime/OverRelease/bin/SemaphoreOverRelease 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: semaphore release would exceed maximum count'; echo True
	@./$(BIN) check Tests/AutoResetEventCompletion >/dev/null; ./$(BIN) check Tests/ManualResetEventCompletion >/dev/null; ./$(BIN) check Tests/TimedMonitorWaitCompletion >/dev/null; echo True


# Milestone 187: Cancellation Token & Source Foundation
test-cancellation-token-source-foundation: $(BIN)
	@./$(BIN) check Tests/CancellationTokenSourceFoundation >/dev/null; echo True
	@./$(BIN) build Tests/CancellationTokenSourceFoundation >/dev/null
	@./Tests/CancellationTokenSourceFoundation/bin/CancellationTokenSourceFoundation
	@set -e; ./$(BIN) publish Tests/CancellationTokenSourceFoundation >/dev/null; test -x Tests/CancellationTokenSourceFoundation/publish/CancellationTokenSourceFoundation$(EXE_SUFFIX); echo True
	@grep -Fq 'public readonly struct CancellationToken' StandardLibrary/Void/Threading/Cancellation.void && grep -Fq 'public sealed class CancellationTokenSource' StandardLibrary/Void/Threading/Cancellation.void && grep -Fq 'public class OperationCanceledException : Exception' StandardLibrary/Void/OperationCanceledException.void; echo True
	@grep -Fq 'Volatile.Read(ref _cancellationRequested)' StandardLibrary/Void/Threading/Cancellation.void && grep -Fq 'Interlocked.Exchange(ref _cancellationRequested, 1)' StandardLibrary/Void/Threading/Cancellation.void; echo True
	@grep -Fq 'throw new OperationCanceledException(this);' StandardLibrary/Void/Threading/Cancellation.void && grep -Fq 'public CancellationToken CancellationToken => _cancellationToken;' StandardLibrary/Void/OperationCanceledException.void; echo True
	@! grep -R -Fq 'CancellationTokenSource' Compiler/src Runtime/include Runtime/src; echo True
	@grep -Fq 'vc_native_atomic_i32_load' Tests/CancellationTokenSourceFoundation/.void/CancellationTokenSourceFoundation.c && grep -Fq 'VC_NATIVE_MEMORY_ORDER_ACQUIRE' Tests/CancellationTokenSourceFoundation/.void/CancellationTokenSourceFoundation.c && grep -Fq 'vc_native_atomic_i32_exchange' Tests/CancellationTokenSourceFoundation/.void/CancellationTokenSourceFoundation.c && grep -Fq 'VC_NATIVE_MEMORY_ORDER_SEQ_CST' Tests/CancellationTokenSourceFoundation/.void/CancellationTokenSourceFoundation.c; echo True
	@./$(BIN) build Tests/CancellationTokenSourceRuntime/UnhandledCancellation >/dev/null
	@set -e; set +e; output="$$(./Tests/CancellationTokenSourceRuntime/UnhandledCancellation/bin/CancellationUnhandled 2>&1)"; status=$$?; set -e; test $$status -eq 1; echo True; printf '%s\n' "$$output" | grep -Fxq 'Unhandled OperationCanceledException: The operation was canceled.'; echo True
	@./$(BIN) check Tests/SemaphoreCompletion >/dev/null; ./$(BIN) check Tests/AutoResetEventCompletion >/dev/null; ./$(BIN) check Tests/VolatileMemoryOrdering >/dev/null; ./$(BIN) check Tests/InterlockedManagedReferenceOperations >/dev/null; echo True

# Milestone 188: Cancellation Registration & Wakeup Completion
test-cancellation-registration-wakeup-completion: $(BIN)
	@./$(BIN) check Tests/CancellationRegistrationWakeupCompletion >/dev/null; echo True
	@./$(BIN) build Tests/CancellationRegistrationWakeupCompletion >/dev/null
	@timeout 20s ./Tests/CancellationRegistrationWakeupCompletion/bin/CancellationRegistrationWakeupCompletion
	@set -e; ./$(BIN) publish Tests/CancellationRegistrationWakeupCompletion >/dev/null; test -x Tests/CancellationRegistrationWakeupCompletion/publish/CancellationRegistrationWakeupCompletion$(EXE_SUFFIX); echo True
	@grep -Fq 'public readonly struct CancellationTokenRegistration : IDisposable' StandardLibrary/Void/Threading/Cancellation.void && grep -Fq 'public CancellationTokenRegistration Register(Action callback)' StandardLibrary/Void/Threading/Cancellation.void; echo True
	@grep -Fq 'Interlocked.CompareExchange(ref node.State, 1, 0)' StandardLibrary/Void/Threading/Cancellation.void && grep -Fq 'Interlocked.CompareExchange(ref current.State, 2, 0)' StandardLibrary/Void/Threading/Cancellation.void; echo True
	@grep -Fq 'CancellationCallbackNode current = _callbacks;' StandardLibrary/Void/Threading/Cancellation.void && grep -Fq '_callbacks = null;' StandardLibrary/Void/Threading/Cancellation.void && grep -Fq 'if (firstError != null)' StandardLibrary/Void/Threading/Cancellation.void; echo True
	@! grep -R -Fq 'CancellationTokenRegistration' Compiler/src Runtime/include Runtime/src; echo True
	@./$(BIN) build Tests/CancellationRegistrationWakeupRuntime/NullCallback >/dev/null; set -e; output="$$(timeout 10s ./Tests/CancellationRegistrationWakeupRuntime/NullCallback/bin/CancellationNullCallback 2>&1 || true)"; printf '%s\n' "$$output" | grep -Fq 'VOID runtime error: cancellation callback cannot be null'; echo True
	@./$(BIN) check Tests/CancellationTokenSourceFoundation >/dev/null; ./$(BIN) check Tests/SemaphoreCompletion >/dev/null; ./$(BIN) check Tests/ManualResetEventCompletion >/dev/null; echo True


# Milestone 189: Timed & Cancellable Synchronization Integration
test-timed-cancellable-synchronization-integration: $(BIN) $(TIMED_CANCELLABLE_NATIVE_FIXTURE)
	@./$(BIN) check Tests/TimedCancellableSynchronizationIntegration >/dev/null; echo True
	@./$(BIN) build Tests/TimedCancellableSynchronizationIntegration >/dev/null
	@timeout 30s ./Tests/TimedCancellableSynchronizationIntegration/bin/TimedCancellableSynchronizationIntegration
	@set -e; ./$(BIN) publish Tests/TimedCancellableSynchronizationIntegration >/dev/null; test -x Tests/TimedCancellableSynchronizationIntegration/publish/TimedCancellableSynchronizationIntegration$(EXE_SUFFIX); echo True
	@grep -Fq 'public bool WaitOne(CancellationToken cancellationToken)' StandardLibrary/Void/Threading/ManualResetEvent.void && grep -Fq 'public bool WaitOne(int millisecondsTimeout, CancellationToken cancellationToken)' StandardLibrary/Void/Threading/ManualResetEvent.void; echo True
	@grep -Fq 'public bool WaitOne(CancellationToken cancellationToken)' StandardLibrary/Void/Threading/AutoResetEvent.void && grep -Fq 'public bool WaitOne(int millisecondsTimeout, CancellationToken cancellationToken)' StandardLibrary/Void/Threading/AutoResetEvent.void; echo True
	@grep -Fq 'public bool WaitOne(CancellationToken cancellationToken)' StandardLibrary/Void/Threading/Semaphore.void && grep -Fq 'public bool WaitOne(int millisecondsTimeout, CancellationToken cancellationToken)' StandardLibrary/Void/Threading/Semaphore.void; echo True
	@grep -Fq 'CancellationTokenRegistration registration = cancellationToken.Register' StandardLibrary/Void/Threading/ManualResetEvent.void && grep -Fq 'CancellationTokenRegistration registration = cancellationToken.Register' StandardLibrary/Void/Threading/AutoResetEvent.void && grep -Fq 'CancellationTokenRegistration registration = cancellationToken.Register' StandardLibrary/Void/Threading/Semaphore.void; echo True
	@grep -Fq 'throw new OperationCanceledException(cancellationToken);' StandardLibrary/Void/Threading/ManualResetEvent.void && grep -Fq 'throw new OperationCanceledException(cancellationToken);' StandardLibrary/Void/Threading/AutoResetEvent.void && grep -Fq 'throw new OperationCanceledException(cancellationToken);' StandardLibrary/Void/Threading/Semaphore.void; echo True
	@! grep -Fq 'CancellationToken' StandardLibrary/Void/Threading/Monitor.void; echo True
	@! grep -R -Fq 'CancellationToken' Compiler/src Runtime/include Runtime/src; echo True
	@grep -Fq 'vc_monitor_wait_timed(' Tests/TimedCancellableSynchronizationIntegration/.void/TimedCancellableSynchronizationIntegration.c && grep -Fq 'vc_native_atomic_i32_load' Tests/TimedCancellableSynchronizationIntegration/.void/TimedCancellableSynchronizationIntegration.c; echo True
	@set -e; if ./$(BIN) check Tests/TimedCancellableSynchronizationIntegrationDiagnostics/InvalidToken >/dev/null 2>&1; then exit 1; fi; echo True
	@./$(BIN) check Tests/CancellationRegistrationWakeupCompletion >/dev/null; ./$(BIN) check Tests/SemaphoreCompletion >/dev/null; ./$(BIN) check Tests/AutoResetEventCompletion >/dev/null; ./$(BIN) check Tests/ManualResetEventCompletion >/dev/null; ./$(BIN) check Tests/TimedMonitorWaitCompletion >/dev/null; echo True

# Milestone 190: Timed Synchronization & Cancellation Integration Audit
test-timed-synchronization-cancellation-integration-audit: $(BIN) $(TIMED_SYNC_AUDIT_NATIVE_FIXTURE)
	@./$(BIN) check Tests/TimedSynchronizationCancellationIntegrationAudit >/dev/null; echo True
	@./$(BIN) build Tests/TimedSynchronizationCancellationIntegrationAudit >/dev/null
	@timeout 45s ./Tests/TimedSynchronizationCancellationIntegrationAudit/bin/TimedSynchronizationCancellationIntegrationAudit
	@set -e; ./$(BIN) publish Tests/TimedSynchronizationCancellationIntegrationAudit >/dev/null; test -x Tests/TimedSynchronizationCancellationIntegrationAudit/publish/TimedSynchronizationCancellationIntegrationAudit$(EXE_SUFFIX); echo True
	@grep -Fq 'vc_monitor_wait_timed(' Tests/TimedSynchronizationCancellationIntegrationAudit/.void/TimedSynchronizationCancellationIntegrationAudit.c && grep -Fq 'vc_gc_native_call_begin' Tests/TimedSynchronizationCancellationIntegrationAudit/.void/TimedSynchronizationCancellationIntegrationAudit.c && grep -Fq 'vc_native_atomic_i32_load' Tests/TimedSynchronizationCancellationIntegrationAudit/.void/TimedSynchronizationCancellationIntegrationAudit.c; echo True
	@grep -Fq 'VcExceptionHandler vc_boundary' Tests/TimedSynchronizationCancellationIntegrationAudit/.void/TimedSynchronizationCancellationIntegrationAudit.c && grep -Fq 'vc_gc_root_push' Tests/TimedSynchronizationCancellationIntegrationAudit/.void/TimedSynchronizationCancellationIntegrationAudit.c && grep -Fq 'vc_managed_thread_worker' Tests/TimedSynchronizationCancellationIntegrationAudit/.void/TimedSynchronizationCancellationIntegrationAudit.c; echo True
	@grep -Fq 'QueryPerformanceCounter' Runtime/src/vc_thread.c && grep -Fq 'SleepConditionVariableSRW' Runtime/src/vc_thread.c && grep -Fq 'GetLastError' Runtime/src/vc_thread.c; echo True
	@grep -Fq 'clock_gettime(CLOCK_MONOTONIC' Runtime/src/vc_thread.c && grep -Fq 'pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC)' Runtime/src/vc_thread.c && grep -Fq 'pthread_cond_timedwait(' Runtime/src/vc_thread.c && grep -Fq 'pthread_cond_timedwait_relative_np(' Runtime/src/vc_thread.c; echo True
	@grep -Fq 'VC_NATIVE_TIMEOUT_INFINITE' Runtime/include/vc_thread.h && grep -Fq 'VC_NATIVE_WAIT_TIMED_OUT' Runtime/include/vc_thread.h && grep -Fq 'bool vc_native_sleep_ms(uint32_t milliseconds);' Runtime/include/vc_thread.h; echo True
	@! grep -Fq 'CancellationToken' StandardLibrary/Void/Threading/Monitor.void; echo True
	@! grep -R -Fq 'CancellationToken' Compiler/src Runtime/include Runtime/src; echo True
	@set -e; if ./$(BIN) check Tests/TimedSynchronizationCancellationIntegrationAuditDiagnostics/MonitorCancellationBoundary >/dev/null 2>&1; then exit 1; fi; echo True
	@./$(BIN) check Tests/MonotonicTimeTimedWaitRuntimeFoundation >/dev/null; ./$(BIN) check Tests/ManagedThreadSleepTimeoutContract >/dev/null; ./$(BIN) check Tests/TimedMonitorWaitCompletion >/dev/null; ./$(BIN) check Tests/ManualResetEventCompletion >/dev/null; ./$(BIN) check Tests/AutoResetEventCompletion >/dev/null; ./$(BIN) check Tests/SemaphoreCompletion >/dev/null; ./$(BIN) check Tests/CancellationTokenSourceFoundation >/dev/null; ./$(BIN) check Tests/CancellationRegistrationWakeupCompletion >/dev/null; ./$(BIN) check Tests/TimedCancellableSynchronizationIntegration >/dev/null; echo True

# One-time #190 race stress. Deliberately NOT a dependency of `make test`.
test-timed-synchronization-cancellation-race-stress: $(BIN)
	@./$(BIN) check Tests/TimedSynchronizationCancellationRaceStress >/dev/null; echo True
	@./$(BIN) build Tests/TimedSynchronizationCancellationRaceStress >/dev/null
	@timeout 120s ./Tests/TimedSynchronizationCancellationRaceStress/bin/TimedSynchronizationCancellationRaceStress


# Milestone 191: Managed ThreadPool & Work Queue Foundation
test-managed-threadpool-work-queue-foundation: $(BIN)
	@./$(BIN) check Tests/ManagedThreadPoolWorkQueueFoundation >/dev/null; echo True
	@./$(BIN) build Tests/ManagedThreadPoolWorkQueueFoundation >/dev/null
	@timeout 20s ./Tests/ManagedThreadPoolWorkQueueFoundation/bin/ManagedThreadPoolWorkQueueFoundation
	@set -e; ./$(BIN) publish Tests/ManagedThreadPoolWorkQueueFoundation >/dev/null; test -x Tests/ManagedThreadPoolWorkQueueFoundation/publish/ManagedThreadPoolWorkQueueFoundation$(EXE_SUFFIX); echo True
	@grep -Fq 'vc_managed_thread_worker' Tests/ManagedThreadPoolWorkQueueFoundation/.void/ManagedThreadPoolWorkQueueFoundation.c && grep -Fq 'vc_monitor_wait(' Tests/ManagedThreadPoolWorkQueueFoundation/.void/ManagedThreadPoolWorkQueueFoundation.c; echo True
	@! grep -R -Fq 'ThreadPool' Compiler/src Runtime/include Runtime/src; echo True
	@./$(BIN) build Tests/ManagedThreadPoolWorkQueueRuntime/NullWorkItem >/dev/null
	@set -e; out=$$(mktemp); if timeout 5s ./Tests/ManagedThreadPoolWorkQueueRuntime/NullWorkItem/bin/NullWorkItem >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: thread pool work item cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/ManagedThreadSurface >/dev/null; ./$(BIN) check Tests/MonitorPulseCompletion >/dev/null; ./$(BIN) check Tests/AdvancedSynchronizationAtomicIntegrationAudit >/dev/null; echo True


# Milestone 192: ThreadPool Worker Lifecycle & GC Coordination
test-threadpool-worker-lifecycle-gc-coordination: $(BIN)
	@./$(BIN) check Tests/ThreadPoolWorkerLifecycleGcCoordination >/dev/null; echo True
	@./$(BIN) build Tests/ThreadPoolWorkerLifecycleGcCoordination >/dev/null
	@timeout 30s ./Tests/ThreadPoolWorkerLifecycleGcCoordination/bin/ThreadPoolWorkerLifecycleGcCoordination
	@set -e; ./$(BIN) publish Tests/ThreadPoolWorkerLifecycleGcCoordination >/dev/null; test -x Tests/ThreadPoolWorkerLifecycleGcCoordination/publish/ThreadPoolWorkerLifecycleGcCoordination$(EXE_SUFFIX); echo True
	@grep -Fq 'private static int _readyWorkers;' StandardLibrary/Void/Threading/ThreadPool.void && grep -Fq 'while (_readyWorkers < 2)' StandardLibrary/Void/Threading/ThreadPool.void && grep -Fq 'Monitor.PulseAll(_gate);' StandardLibrary/Void/Threading/ThreadPool.void; echo True
	@grep -Fq 'vc_managed_thread_worker' Tests/ThreadPoolWorkerLifecycleGcCoordination/.void/ThreadPoolWorkerLifecycleGcCoordination.c && grep -Fq 'vc_gc_static_root_push(&vc_state->owner_root' Tests/ThreadPoolWorkerLifecycleGcCoordination/.void/ThreadPoolWorkerLifecycleGcCoordination.c; echo True
	@grep -Fq 'vc_monitor_wait(' Tests/ThreadPoolWorkerLifecycleGcCoordination/.void/ThreadPoolWorkerLifecycleGcCoordination.c && grep -Fq 'vc_gc_native_call_begin' Tests/ThreadPoolWorkerLifecycleGcCoordination/.void/ThreadPoolWorkerLifecycleGcCoordination.c && grep -Fq 'vc_gc_root_push' Tests/ThreadPoolWorkerLifecycleGcCoordination/.void/ThreadPoolWorkerLifecycleGcCoordination.c; echo True
	@grep -Fq 'if (vc_runtime_thread_count != 0u)' Tests/ThreadPoolWorkerLifecycleGcCoordination/.void/ThreadPoolWorkerLifecycleGcCoordination.c && grep -Fq 'vc_native_thread_runtime_registry_lock();' Tests/ThreadPoolWorkerLifecycleGcCoordination/.void/ThreadPoolWorkerLifecycleGcCoordination.c; echo True
	@! grep -R -Fq 'ThreadPool' Compiler/src Runtime/include Runtime/src; echo True
	@./$(BIN) check Tests/ManagedThreadPoolWorkQueueFoundation >/dev/null; ./$(BIN) check Tests/CooperativeStopTheWorldGc/Library >/dev/null; ./$(BIN) check Tests/ThreadSafeAllocationRuntimeRegistry/Library >/dev/null; echo True


# Milestone 193: ThreadPool Exception, Shutdown & Stress Completion
test-threadpool-exception-shutdown-stress: $(BIN)
	@./$(BIN) check Tests/ThreadPoolExceptionShutdownStress >/dev/null; echo True
	@./$(BIN) build Tests/ThreadPoolExceptionShutdownStress >/dev/null
	@timeout 30s ./Tests/ThreadPoolExceptionShutdownStress/bin/ThreadPoolExceptionShutdownStress

test-threadpool-exception-shutdown-completion: $(BIN)
	@./$(BIN) check Tests/ThreadPoolExceptionShutdownCompletion >/dev/null; echo True
	@./$(BIN) build Tests/ThreadPoolExceptionShutdownCompletion >/dev/null
	@timeout 30s ./Tests/ThreadPoolExceptionShutdownCompletion/bin/ThreadPoolExceptionShutdownCompletion
	@set -e; ./$(BIN) publish Tests/ThreadPoolExceptionShutdownCompletion >/dev/null; test -x Tests/ThreadPoolExceptionShutdownCompletion/publish/ThreadPoolExceptionShutdownCompletion$(EXE_SUFFIX); echo True
	@./$(BIN) check Tests/ThreadPoolExceptionShutdownRuntime/QueueAfterShutdown >/dev/null; echo True
	@./$(BIN) build Tests/ThreadPoolExceptionShutdownRuntime/QueueAfterShutdown >/dev/null
	@out=$$(mktemp); if timeout 5s ./Tests/ThreadPoolExceptionShutdownRuntime/QueueAfterShutdown/bin/QueueAfterShutdown >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: thread pool has been shut down' $$out; rm -f $$out; echo True
	@grep -Fq 'public static void Shutdown()' StandardLibrary/Void/Threading/ThreadPool.void && grep -Fq 'while (_queue.Count == 0 && !_shutdownRequested)' StandardLibrary/Void/Threading/ThreadPool.void && grep -Fq 'catch (Exception)' StandardLibrary/Void/Threading/ThreadPool.void; echo True
	@grep -Fq 'vc_managed_thread_join' Tests/ThreadPoolExceptionShutdownCompletion/.void/ThreadPoolExceptionShutdownCompletion.c && grep -Fq 'vc_monitor_pulse_all' Tests/ThreadPoolExceptionShutdownCompletion/.void/ThreadPoolExceptionShutdownCompletion.c; echo True
	@./$(BIN) check Tests/ThreadPoolWorkerLifecycleGcCoordination >/dev/null; ./$(BIN) check Tests/ManagedThreadPoolWorkQueueFoundation >/dev/null; ./$(BIN) check Tests/ThreadExceptionNativeBoundaryCleanup >/dev/null; echo True


# Milestone 194: Task State & Completion Foundation
test-task-state-completion-foundation: $(BIN)
	@./$(BIN) check Tests/TaskStateCompletionFoundation >/dev/null; echo True
	@./$(BIN) build Tests/TaskStateCompletionFoundation >/dev/null
	@timeout 20s ./Tests/TaskStateCompletionFoundation/bin/TaskStateCompletionFoundation
	@set -e; ./$(BIN) publish Tests/TaskStateCompletionFoundation >/dev/null; test -x Tests/TaskStateCompletionFoundation/publish/TaskStateCompletionFoundation$(EXE_SUFFIX); echo True
	@grep -Fq 'public enum TaskStatus' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'public class Task' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'internal bool TrySetRunning()' StandardLibrary/Void/Threading/Tasks/Task.void; echo True
	@grep -Fq 'internal bool TrySetCompleted()' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'internal bool TrySetException(Exception exception)' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'internal bool TrySetCanceled(CancellationToken cancellationToken)' StandardLibrary/Void/Threading/Tasks/Task.void; echo True
	@grep -Fq 'private Exception _exception;' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'private CancellationToken _cancellationToken;' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'Monitor.Enter(_gate);' StandardLibrary/Void/Threading/Tasks/Task.void; echo True
	@! grep -R -Fq 'Void.Threading.Tasks.Task' Compiler/src Runtime/include Runtime/src; echo True
	@./$(BIN) check Tests/TaskStateCompletionRuntime/NullException >/dev/null; ./$(BIN) build Tests/TaskStateCompletionRuntime/NullException >/dev/null
	@out=$$(mktemp); if timeout 5s ./Tests/TaskStateCompletionRuntime/NullException/bin/NullException >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task exception cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/ThreadPoolExceptionShutdownCompletion >/dev/null; ./$(BIN) check Tests/CancellationTokenSourceFoundation >/dev/null; ./$(BIN) check Tests/ExceptionPropagationGcUnwinding >/dev/null; echo True


# Milestone 195: Generic Task<T> Result & GC Completion
test-generic-task-result-gc-completion: $(BIN)
	@./$(BIN) check Tests/GenericTaskResultGcCompletion >/dev/null; echo True
	@./$(BIN) build Tests/GenericTaskResultGcCompletion >/dev/null
	@timeout 20s ./Tests/GenericTaskResultGcCompletion/bin/GenericTaskResultGcCompletion
	@set -e; ./$(BIN) publish Tests/GenericTaskResultGcCompletion >/dev/null; test -x Tests/GenericTaskResultGcCompletion/publish/GenericTaskResultGcCompletion$(EXE_SUFFIX); echo True
	@grep -Fq 'public class Task<T> : Task' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'public T Result' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'internal bool TrySetResult(T result)' StandardLibrary/Void/Threading/Tasks/Task.void; echo True
	@grep -Fq 'Task__g1_int' Tests/GenericTaskResultGcCompletion/.void/GenericTaskResultGcCompletion.c && grep -Fq 'Task__g1_Score' Tests/GenericTaskResultGcCompletion/.void/GenericTaskResultGcCompletion.c && grep -Fq 'Task__g1_Payload' Tests/GenericTaskResultGcCompletion/.void/GenericTaskResultGcCompletion.c; echo True
	@./$(BIN) check Tests/GenericTaskResultRuntime/IncompleteResult >/dev/null; ./$(BIN) build Tests/GenericTaskResultRuntime/IncompleteResult >/dev/null
	@timeout 5s ./Tests/GenericTaskResultRuntime/IncompleteResult/bin/IncompleteResult
	@./$(BIN) check Tests/TaskStateCompletionFoundation >/dev/null; ./$(BIN) check Tests/GenericMethodTypeInference >/dev/null; ./$(BIN) check Tests/GC >/dev/null; echo True


# Milestone 196: Task.Run & ThreadPool Scheduling Integration
test-task-run-threadpool-scheduling-integration: $(BIN) $(TASK_RUN_NATIVE_FIXTURE)
	@./$(BIN) check Tests/TaskRunThreadPoolSchedulingIntegration >/dev/null; echo True
	@./$(BIN) build Tests/TaskRunThreadPoolSchedulingIntegration >/dev/null
	@timeout 20s ./Tests/TaskRunThreadPoolSchedulingIntegration/bin/TaskRunThreadPoolSchedulingIntegration
	@set -e; ./$(BIN) publish Tests/TaskRunThreadPoolSchedulingIntegration >/dev/null; test -x Tests/TaskRunThreadPoolSchedulingIntegration/publish/TaskRunThreadPoolSchedulingIntegration$(EXE_SUFFIX); echo True
	@grep -Fq 'public static Task Run(Action action)' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'public static Task<T> Run<T>(Func<T> function)' StandardLibrary/Void/Threading/Tasks/Task.void; echo True
	@grep -Fq 'task action cannot be null' Tests/TaskRunThreadPoolSchedulingIntegration/.void/TaskRunThreadPoolSchedulingIntegration.c && grep -Fq 'task function cannot be null' Tests/TaskRunThreadPoolSchedulingIntegration/.void/TaskRunThreadPoolSchedulingIntegration.c; echo True
	@grep -Fq 'voidc196_add' Tests/TaskRunThreadPoolSchedulingIntegration/.void/TaskRunThreadPoolSchedulingIntegration.c && grep -Fq 'Task__g1_int' Tests/TaskRunThreadPoolSchedulingIntegration/.void/TaskRunThreadPoolSchedulingIntegration.c && grep -Fq 'Task__g1_RunPayload' Tests/TaskRunThreadPoolSchedulingIntegration/.void/TaskRunThreadPoolSchedulingIntegration.c; echo True
	@./$(BIN) check Tests/TaskRunThreadPoolSchedulingRuntime/NullAction >/dev/null; ./$(BIN) build Tests/TaskRunThreadPoolSchedulingRuntime/NullAction >/dev/null
	@out=$$(mktemp); if timeout 5s ./Tests/TaskRunThreadPoolSchedulingRuntime/NullAction/bin/NullAction >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task action cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/TaskRunThreadPoolSchedulingRuntime/NullFunction >/dev/null; ./$(BIN) build Tests/TaskRunThreadPoolSchedulingRuntime/NullFunction >/dev/null
	@out=$$(mktemp); if timeout 5s ./Tests/TaskRunThreadPoolSchedulingRuntime/NullFunction/bin/NullFunction >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task function cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/TaskRunThreadPoolSchedulingRuntime/RunAfterShutdown >/dev/null; ./$(BIN) build Tests/TaskRunThreadPoolSchedulingRuntime/RunAfterShutdown >/dev/null
	@out=$$(mktemp); if timeout 5s ./Tests/TaskRunThreadPoolSchedulingRuntime/RunAfterShutdown/bin/RunAfterShutdown >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: thread pool has been shut down' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/ThreadPoolExceptionShutdownCompletion >/dev/null; ./$(BIN) check Tests/GenericTaskResultGcCompletion >/dev/null; ./$(BIN) check Tests/ThreadedClosureDelegateGenericGcIntegration >/dev/null; echo True


# Milestone 197: Task Waiting, Timeout & Fault Propagation Completion
test-task-waiting-timeout-fault-propagation: $(BIN)
	@./$(BIN) check Tests/TaskWaitingTimeoutFaultPropagation >/dev/null; echo True
	@./$(BIN) build Tests/TaskWaitingTimeoutFaultPropagation >/dev/null
	@timeout 30s ./Tests/TaskWaitingTimeoutFaultPropagation/bin/TaskWaitingTimeoutFaultPropagation
	@set -e; ./$(BIN) publish Tests/TaskWaitingTimeoutFaultPropagation >/dev/null; test -x Tests/TaskWaitingTimeoutFaultPropagation/publish/TaskWaitingTimeoutFaultPropagation$(EXE_SUFFIX); echo True
	@grep -Fq 'public void Wait()' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'public bool Wait(int millisecondsTimeout)' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'Monitor.PulseAll(_gate);' StandardLibrary/Void/Threading/Tasks/Task.void; echo True
	@grep -Fq 'vc_monitor_wait(' Tests/TaskWaitingTimeoutFaultPropagation/.void/TaskWaitingTimeoutFaultPropagation.c && grep -Fq 'vc_monitor_wait_timed(' Tests/TaskWaitingTimeoutFaultPropagation/.void/TaskWaitingTimeoutFaultPropagation.c && grep -Fq 'vc_throw(' Tests/TaskWaitingTimeoutFaultPropagation/.void/TaskWaitingTimeoutFaultPropagation.c; echo True
	@! grep -R -Fq 'Void.Threading.Tasks.Task' Compiler/src Runtime/include Runtime/src; echo True
	@./$(BIN) check Tests/TaskWaitingTimeoutFaultPropagationRuntime/InvalidTimeout >/dev/null; ./$(BIN) build Tests/TaskWaitingTimeoutFaultPropagationRuntime/InvalidTimeout >/dev/null
	@out=$$(mktemp); if timeout 5s ./Tests/TaskWaitingTimeoutFaultPropagationRuntime/InvalidTimeout/bin/InvalidTimeout >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task wait timeout must be -1 or non-negative' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/TaskStateCompletionFoundation >/dev/null; ./$(BIN) check Tests/GenericTaskResultGcCompletion >/dev/null; ./$(BIN) check Tests/TaskRunThreadPoolSchedulingIntegration >/dev/null; echo True


# Milestone 198: Task Cancellation Integration
test-task-cancellation-integration: $(BIN)
	@./$(BIN) check Tests/TaskCancellationIntegration >/dev/null; echo True
	@./$(BIN) build Tests/TaskCancellationIntegration >/dev/null
	@timeout 30s ./Tests/TaskCancellationIntegration/bin/TaskCancellationIntegration
	@set -e; ./$(BIN) publish Tests/TaskCancellationIntegration >/dev/null; test -x Tests/TaskCancellationIntegration/publish/TaskCancellationIntegration$(EXE_SUFFIX); echo True
	@grep -Fq 'public static Task Run(Action action, CancellationToken cancellationToken)' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'public static Task<T> Run<T>(Func<T> function, CancellationToken cancellationToken)' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'TrySetCanceledIfPending' StandardLibrary/Void/Threading/Tasks/Task.void; echo True
	@grep -Fq 'CancellationTokenRegistration' Tests/TaskCancellationIntegration/.void/TaskCancellationIntegration.c && grep -Fq 'OperationCanceledException' Tests/TaskCancellationIntegration/.void/TaskCancellationIntegration.c && grep -Fq 'vc_monitor_pulse_all' Tests/TaskCancellationIntegration/.void/TaskCancellationIntegration.c; echo True
	@! grep -R -Fq 'Void.Threading.Tasks.Task' Compiler/src Runtime/include Runtime/src; echo True
	@./$(BIN) check Tests/TaskCancellationRuntime/NullAction >/dev/null; ./$(BIN) build Tests/TaskCancellationRuntime/NullAction >/dev/null
	@out=$$(mktemp); if timeout 5s ./Tests/TaskCancellationRuntime/NullAction/bin/NullAction >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task action cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/TaskCancellationRuntime/NullFunction >/dev/null; ./$(BIN) build Tests/TaskCancellationRuntime/NullFunction >/dev/null
	@out=$$(mktemp); if timeout 5s ./Tests/TaskCancellationRuntime/NullFunction/bin/NullFunction >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task function cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/CancellationTokenSourceFoundation >/dev/null; ./$(BIN) check Tests/TaskWaitingTimeoutFaultPropagation >/dev/null; ./$(BIN) check Tests/TaskRunThreadPoolSchedulingIntegration >/dev/null; echo True


# Milestone 199: Task Continuation & Completion-Source Foundation
test-task-continuation-completion-source-foundation: $(BIN)
	@./$(BIN) check Tests/TaskContinuationCompletionSourceFoundation >/dev/null; echo True
	@./$(BIN) build Tests/TaskContinuationCompletionSourceFoundation >/dev/null
	@timeout 30s ./Tests/TaskContinuationCompletionSourceFoundation/bin/TaskContinuationCompletionSourceFoundation
	@set -e; ./$(BIN) publish Tests/TaskContinuationCompletionSourceFoundation >/dev/null; test -x Tests/TaskContinuationCompletionSourceFoundation/publish/TaskContinuationCompletionSourceFoundation$(EXE_SUFFIX); echo True
	@grep -Fq 'public Task ContinueWith(Action<Task> continuationAction)' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'public Task<TResult> ContinueWith<TResult>(Func<Task, TResult> continuationFunction)' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'private void RegisterContinuation(Action workItem)' StandardLibrary/Void/Threading/Tasks/Task.void; echo True
	@grep -Fq 'public sealed class TaskCompletionSource' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'public sealed class TaskCompletionSource<T>' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'public bool TrySetResult()' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'public bool TrySetResult(T result)' StandardLibrary/Void/Threading/Tasks/Task.void; echo True
	@! grep -R -Fq 'TaskCompletionSource' Compiler/src Runtime/include Runtime/src; echo True
	@./$(BIN) check Tests/TaskContinuationCompletionSourceRuntime/NullAction >/dev/null; ./$(BIN) build Tests/TaskContinuationCompletionSourceRuntime/NullAction >/dev/null
	@out=$$(mktemp); if timeout 5s ./Tests/TaskContinuationCompletionSourceRuntime/NullAction/bin/NullAction >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task continuation action cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/TaskContinuationCompletionSourceRuntime/NullFunction >/dev/null; ./$(BIN) build Tests/TaskContinuationCompletionSourceRuntime/NullFunction >/dev/null
	@out=$$(mktemp); if timeout 5s ./Tests/TaskContinuationCompletionSourceRuntime/NullFunction/bin/NullFunction >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task continuation function cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/TaskContinuationCompletionSourceRuntime/DuplicateCompletion >/dev/null; ./$(BIN) build Tests/TaskContinuationCompletionSourceRuntime/DuplicateCompletion >/dev/null
	@out=$$(mktemp); if timeout 5s ./Tests/TaskContinuationCompletionSourceRuntime/DuplicateCompletion/bin/DuplicateCompletion >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task completion source has already completed' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/TaskContinuationCompletionSourceRuntime/NullException >/dev/null; ./$(BIN) build Tests/TaskContinuationCompletionSourceRuntime/NullException >/dev/null
	@out=$$(mktemp); if timeout 5s ./Tests/TaskContinuationCompletionSourceRuntime/NullException/bin/NullException >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task exception cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/TaskCancellationIntegration >/dev/null; ./$(BIN) check Tests/TaskWaitingTimeoutFaultPropagation >/dev/null; ./$(BIN) check Tests/TaskRunThreadPoolSchedulingIntegration >/dev/null; echo True


# Milestone 200: ThreadPool & Task Integration Audit
# One-time block-closing stress. Deliberately NOT a dependency of `make test`.
test-threadpool-task-integration-audit-stress: $(BIN)
	@./$(BIN) check Tests/ThreadPoolTaskIntegrationAudit >/dev/null
	@./$(BIN) build Tests/ThreadPoolTaskIntegrationAudit >/dev/null
	@set -e; i=0; while [ $$i -lt 50 ]; do out=$$(mktemp); timeout 10s ./Tests/ThreadPoolTaskIntegrationAudit/bin/ThreadPoolTaskIntegrationAudit >$$out; test "$$(grep -c '^True$$' $$out)" -eq 20; test -z "$$(grep -v '^True$$' $$out)"; rm -f $$out; i=$$((i + 1)); done; echo True

test-threadpool-task-integration-audit: $(BIN)
	@./$(BIN) check Tests/ThreadPoolTaskIntegrationAudit >/dev/null; echo True
	@./$(BIN) build Tests/ThreadPoolTaskIntegrationAudit >/dev/null
	@timeout 20s ./Tests/ThreadPoolTaskIntegrationAudit/bin/ThreadPoolTaskIntegrationAudit
	@set -e; ./$(BIN) publish Tests/ThreadPoolTaskIntegrationAudit >/dev/null; test -x Tests/ThreadPoolTaskIntegrationAudit/publish/ThreadPoolTaskIntegrationAudit$(EXE_SUFFIX); echo True
	@grep -Fq 'private static int _activeWorkItems;' StandardLibrary/Void/Threading/ThreadPool.void && grep -Fq 'private static bool _shutdownDrained;' StandardLibrary/Void/Threading/ThreadPool.void && grep -Fq 'internal static bool QueueTaskContinuation(Action workItem)' StandardLibrary/Void/Threading/ThreadPool.void; echo True
	@grep -Fq '_activeWorkItems++;' StandardLibrary/Void/Threading/ThreadPool.void && grep -Fq '_activeWorkItems--;' StandardLibrary/Void/Threading/ThreadPool.void && grep -Fq '_shutdownDrained = true;' StandardLibrary/Void/Threading/ThreadPool.void; echo True
	@grep -Fq 'ThreadPool.QueueTaskContinuation(continuations[i]);' StandardLibrary/Void/Threading/Tasks/Task.void; echo True
	@grep -Fq 'vc_managed_thread_worker' Tests/ThreadPoolTaskIntegrationAudit/.void/ThreadPoolTaskIntegrationAudit.c && grep -Fq 'vc_monitor_wait(' Tests/ThreadPoolTaskIntegrationAudit/.void/ThreadPoolTaskIntegrationAudit.c && grep -Fq 'vc_gc_root_push' Tests/ThreadPoolTaskIntegrationAudit/.void/ThreadPoolTaskIntegrationAudit.c; echo True
	@grep -Fq 'Task__g1_AuditPayload200' Tests/ThreadPoolTaskIntegrationAudit/.void/ThreadPoolTaskIntegrationAudit.c && grep -Fq 'Task__g1_int' Tests/ThreadPoolTaskIntegrationAudit/.void/ThreadPoolTaskIntegrationAudit.c && grep -Fq 'AuditFault200' Tests/ThreadPoolTaskIntegrationAudit/.void/ThreadPoolTaskIntegrationAudit.c; echo True
	@! grep -R -Fq 'QueueTaskContinuation' Compiler/src Runtime/include Runtime/src; echo True
	@./$(BIN) check Tests/ThreadPoolTaskIntegrationAuditRuntime/ContinuationAfterShutdown >/dev/null; ./$(BIN) build Tests/ThreadPoolTaskIntegrationAuditRuntime/ContinuationAfterShutdown >/dev/null
	@out=$$(mktemp); if timeout 5s ./Tests/ThreadPoolTaskIntegrationAuditRuntime/ContinuationAfterShutdown/bin/ContinuationAfterShutdown >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: thread pool has been shut down' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/ThreadPoolTaskIntegrationAuditRuntime/PendingContinuationAfterShutdown >/dev/null; ./$(BIN) build Tests/ThreadPoolTaskIntegrationAuditRuntime/PendingContinuationAfterShutdown >/dev/null
	@out=$$(mktemp); if timeout 5s ./Tests/ThreadPoolTaskIntegrationAuditRuntime/PendingContinuationAfterShutdown/bin/PendingContinuationAfterShutdown >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: thread pool has been shut down' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/ManagedThreadPoolWorkQueueFoundation >/dev/null; ./$(BIN) check Tests/ThreadPoolWorkerLifecycleGcCoordination >/dev/null; ./$(BIN) check Tests/ThreadPoolExceptionShutdownCompletion >/dev/null; echo True
	@./$(BIN) check Tests/TaskStateCompletionFoundation >/dev/null; ./$(BIN) check Tests/GenericTaskResultGcCompletion >/dev/null; ./$(BIN) check Tests/TaskRunThreadPoolSchedulingIntegration >/dev/null; ./$(BIN) check Tests/TaskWaitingTimeoutFaultPropagation >/dev/null; ./$(BIN) check Tests/TaskCancellationIntegration >/dev/null; ./$(BIN) check Tests/TaskContinuationCompletionSourceFoundation >/dev/null; echo True


# Milestone 201: Async/Await Syntax & Semantic Foundation
test-async-await-syntax-semantic-foundation: $(BIN)
	@./$(BIN) check Tests/AsyncAwaitSyntaxSemanticFoundation >/dev/null; echo True
	@out=$$(mktemp); ./$(BIN) parse Tests/AsyncAwaitSyntaxSemanticFoundation/Program.void >$$out; grep -Fq 'Method Observe : Task [public static async]' $$out; grep -Fq 'Method Transform : Task<int> [public static async]' $$out; grep -Fq 'Method Immediate : Task<int> [public static async]' $$out; grep -Fq 'Await' $$out; rm -f $$out; echo True
	@out=$$(mktemp); ./$(BIN) parse Tests/AsyncAwaitSyntaxSemanticFoundation/Program.void >$$out; grep -Fq 'Type async' $$out; grep -Fq 'Constructor async [public]' $$out; grep -Fq 'Method MakeContextualType : async [public static]' $$out; grep -Fq 'Method await : int [public static]' $$out; grep -Fq 'Local await : int' $$out; rm -f $$out; echo True
	@grep -Fq 'VC_AST_MOD_ASYNC' Compiler/include/ast.h && grep -Fq 'VC_AST_AWAIT_EXPRESSION' Compiler/include/ast.h && ! grep -Fq 'VC_TOKEN_KW_ASYNC' Compiler/include/lexer.h && ! grep -Fq 'VC_TOKEN_KW_AWAIT' Compiler/include/lexer.h; echo True
	@grep -Fq 'bool is_async;' Compiler/include/semantic.h && grep -Fq 'VcSemanticType async_result_type;' Compiler/include/semantic.h && grep -Fq 'case VC_AST_AWAIT_EXPRESSION:' Compiler/src/semantic.c; echo True
	@grep -Fq 'case VC_AST_AWAIT_EXPRESSION:' Compiler/src/monomorph.c && grep -Fq 'async_method_depth' Compiler/src/parser.c; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/InvalidReturnType >$$out 2>&1; grep -Fq "async method 'Bad' must return Task or Task<T>, got 'int'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/InvalidVoidReturn >$$out 2>&1; grep -Fq "async method 'Bad' must return Task or Task<T>, got 'void'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/AwaitNonTask >$$out 2>&1; grep -Fq "await requires Task or Task<T>, got 'int'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/AwaitOutsideAsync >$$out 2>&1; grep -Fq 'await expression may only be used inside an async method' $$out; rm -f $$out; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/AwaitNonAsyncLambda >$$out 2>&1; grep -Fq 'await expression cannot be used inside a non-async lambda' $$out; rm -f $$out; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/RefParameter >$$out 2>&1; grep -Fq "async method 'Bad' cannot declare ref parameter 'value'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/OutParameter >$$out 2>&1; grep -Fq "async method 'Bad' cannot declare out parameter 'value'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/InParameter >$$out 2>&1; grep -Fq "async method 'Bad' cannot declare in parameter 'value'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/AsyncConstructor >$$out 2>&1; grep -Fq "constructor 'Program' cannot be async" $$out; rm -f $$out; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/AsyncProperty >$$out 2>&1; grep -Fq "'async' modifier is valid only on methods" $$out; rm -f $$out; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/AsyncField >$$out 2>&1; grep -Fq "'async' modifier is valid only on methods" $$out; rm -f $$out; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/AsyncOperator >$$out 2>&1; grep -Fq 'operator overloads cannot be async' $$out; rm -f $$out; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/AsyncAbstract >$$out 2>&1; grep -Fq "async method 'Bad' cannot be abstract" $$out; rm -f $$out; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/AsyncExtern >$$out 2>&1; grep -Fq "async method 'Bad' cannot be extern" $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/AsyncInterface >/dev/null && echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/UserTaskReturn >$$out 2>&1; grep -Fq "async method 'Bad' must return Task or Task<T>, got 'Task'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/AwaitDerivedTask >$$out 2>&1; grep -Fq "await requires Task or Task<T>, got 'DerivedTask'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/AsyncTaskReturnValue >$$out 2>&1; grep -Fq 'async Task method cannot return a value' $$out; rm -f $$out; echo True
	@out=$$(mktemp); ! ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/AsyncTaskResultMissing >$$out 2>&1; grep -Fq "async Task<T> method must return a value of type 'int'" $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/ThreadPoolTaskIntegrationAudit >/dev/null; ./$(BIN) check Tests/TaskContinuationCompletionSourceFoundation >/dev/null; echo True


# Milestone 202: Async Task State-Machine Lowering Foundation
test-async-task-state-machine-lowering-foundation: $(BIN)
	@./$(BIN) check Tests/AsyncTaskStateMachineLoweringFoundation >/dev/null; echo True
	@./$(BIN) build Tests/AsyncTaskStateMachineLoweringFoundation >/dev/null
	@./Tests/AsyncTaskStateMachineLoweringFoundation/bin/AsyncTaskStateMachineLoweringFoundation
	@grep -Fq 'vc_has_async_methods' Compiler/include/async_lower.h && grep -Fq 'vc_lower_async_methods' Compiler/include/async_lower.h; echo True
	@grep -Fq 'vc_has_async_methods(trees' Compiler/src/compiler.c && grep -Fq 'vc_lower_async_methods(trees' Compiler/src/compiler.c; echo True
	@grep -Fq 'TaskCompletionSource' Compiler/src/async_lower.c && grep -Fq '"_state"' Compiler/src/async_lower.c && grep -Fq '"MoveNext"' Compiler/src/async_lower.c; echo True
	@grep -Fq '__voidc$$async$$' Tests/AsyncTaskStateMachineLoweringFoundation/.void/AsyncTaskStateMachineLoweringFoundation.c; echo True
	@! grep -Fq 'VC_AST_AWAIT_EXPRESSION' Compiler/src/compiler.c && ! grep -R -Fq '__voidc$$async$$' Runtime StandardLibrary; echo True
	@grep -Fq 'lowered_state_owner_struct_index' Compiler/src/semantic.c && grep -Fq '"__voidc$$async$$"' Compiler/src/semantic.c; echo True
	@./$(BIN) check Tests/AsyncAwaitSyntaxSemanticFoundation >/dev/null; echo True
	@./$(BIN) check Tests/TaskContinuationCompletionSourceFoundation >/dev/null; ./$(BIN) check Tests/ThreadPoolTaskIntegrationAudit >/dev/null; echo True


# Milestone 203: Await Suspension & Resumption Completion
test-await-suspension-resumption-completion: $(BIN)
	@./$(BIN) check Tests/AwaitSuspensionResumptionCompletion >/dev/null; echo True
	@./$(BIN) build Tests/AwaitSuspensionResumptionCompletion >/dev/null
	@./Tests/AwaitSuspensionResumptionCompletion/bin/AwaitSuspensionResumptionCompletion
	@grep -Fq '"_awaiter"' Compiler/src/async_lower.c && grep -Fq '"Resume"' Compiler/src/async_lower.c; echo True
	@grep -Fq 'single_generic_type(context->tree, location, "Action", "Task")' Compiler/src/async_lower.c && grep -Fq '"ContinueWith"' Compiler/src/async_lower.c; echo True
	@grep -Fq '"IsCompleted"' Compiler/src/async_lower.c && grep -Fq '"Wait"' Compiler/src/async_lower.c && grep -Fq 'ExecuteAfterAwait' Compiler/src/async_lower.c; echo True
	@./$(BIN) check Tests/AsyncTaskStateMachineLoweringFoundation >/dev/null; echo True
	@./$(BIN) check Tests/AsyncAwaitSyntaxSemanticFoundation >/dev/null; echo True
	@./$(BIN) check Tests/TaskContinuationCompletionSourceFoundation >/dev/null; echo True


# Milestone 204: Async Task<T> & Typed Await Results
test-async-task-typed-await-results: $(BIN)
	@./$(BIN) check Tests/AsyncTaskTypedAwaitResults >/dev/null && echo True
	@./$(BIN) build Tests/AsyncTaskTypedAwaitResults >/dev/null
	@./Tests/AsyncTaskTypedAwaitResults/bin/AsyncTaskTypedAwaitResults
	@grep -Fq 'TaskCompletionSource__g1_int' Tests/AsyncTaskTypedAwaitResults/.void/AsyncTaskTypedAwaitResults.c && grep -Fq '__voidc$$async$$Program$$AwaitInt' Tests/AsyncTaskTypedAwaitResults/.void/AsyncTaskTypedAwaitResults.c && echo True
	@grep -Fq 'Lowering may introduce ordinary generic dependencies' Compiler/src/compiler.c && grep -Fq 'monomorphize_compilation(trees, compilation, diagnostic' Compiler/src/compiler.c && grep -Fq 'vc_monomorphize_diagnostics(trees, sources, compilation->source_count' Compiler/src/compiler.c && echo True
	@grep -Fq 'VC_ASYNC_AWAIT_RESUME' Compiler/src/async_lower.c && grep -Fq 'return_result' Compiler/src/async_lower.c && grep -Fq '"GetResult"' Compiler/src/async_lower.c && grep -Fq 'has_await_protocol' Compiler/src/async_lower.c && echo True
	@./$(BIN) check Tests/AwaitSuspensionResumptionCompletion >/dev/null && echo True
	@./$(BIN) check Tests/AsyncTaskStateMachineLoweringFoundation >/dev/null && echo True
	@./$(BIN) check Tests/GenericTaskResultGcCompletion >/dev/null && ./$(BIN) check Tests/TaskContinuationCompletionSourceFoundation >/dev/null && echo True


# Milestone 205: Multiple Await & Structured Control-Flow Completion
test-async-multiple-await-structured-control-flow: $(BIN)
	@./$(BIN) check Tests/AsyncMultipleAwaitStructuredControlFlow >/dev/null && echo True
	@./$(BIN) build Tests/AsyncMultipleAwaitStructuredControlFlow >/dev/null
	@./Tests/AsyncMultipleAwaitStructuredControlFlow/bin/AsyncMultipleAwaitStructuredControlFlow
	@./$(BIN) check Tests/AsyncMultipleAwaitStructuredControlFlowDiagnostics/TryAwait >/dev/null && echo True
	@./$(BIN) check Tests/AsyncMultipleAwaitStructuredControlFlowDiagnostics/LambdaCapture >/dev/null && echo True
	@grep -Fq 'VC_ASYNC_AWAIT_START' Compiler/src/async_lower.c && grep -Fq 'VC_ASYNC_AWAIT_RESUME' Compiler/src/async_lower.c && grep -Fq 'compile_async_statement' Compiler/src/async_lower.c && echo True
	@grep -Fq 'analyze_async_local_promotion' Compiler/src/async_lower.c && grep -Fq 'live_in' Compiler/src/async_lower.c && grep -Fq 'promoted' Compiler/src/async_lower.c && echo True
	@./$(BIN) check Tests/AsyncTaskTypedAwaitResults >/dev/null && echo True
	@./$(BIN) check Tests/AwaitSuspensionResumptionCompletion >/dev/null && echo True
	@./$(BIN) check Tests/AsyncTaskStateMachineLoweringFoundation >/dev/null && echo True


# Milestone 206: Async Exception, Catch & Finally Integration
test-async-exception-catch-finally-integration: $(BIN)
	@./$(BIN) check Tests/AsyncExceptionCatchFinallyIntegration >/dev/null && echo True
	@./$(BIN) build Tests/AsyncExceptionCatchFinallyIntegration >/dev/null
	@./Tests/AsyncExceptionCatchFinallyIntegration/bin/AsyncExceptionCatchFinallyIntegration
	@./$(BIN) publish Tests/AsyncExceptionCatchFinallyIntegration >/dev/null && echo True
	@output="$$(./$(BIN) check Tests/AsyncExceptionCatchFinallyDiagnostics/AwaitInLock 2>&1)"; status=$$?; test $$status -ne 0 && printf '%s' "$$output" | grep -Fq 'await cannot be used inside a lock statement' && echo True
	@grep -Fq 'VC_ASYNC_CATCH_DISPATCH' Compiler/src/async_lower.c && grep -Fq '_pendingException' Compiler/src/async_lower.c && echo True
	@grep -Fq 'compile_async_try' Compiler/src/async_lower.c && grep -Fq 'exception_target' Compiler/src/async_lower.c && echo True
	@grep -Fq 'compile_async_using' Compiler/src/async_lower.c && grep -Fq 'make_async_dispose_statement' Compiler/src/async_lower.c && echo True
	@./$(BIN) check Tests/AsyncMultipleAwaitStructuredControlFlow >/dev/null && echo True
	@./$(BIN) check Tests/AsyncTaskTypedAwaitResults >/dev/null && echo True
	@./$(BIN) check Tests/ExceptionsRuntimeControlFlowIntegration >/dev/null && echo True


# Milestone 207: Async Cancellation Integration
test-async-cancellation-integration: $(BIN)
	@./$(BIN) check Tests/AsyncCancellationIntegration >/dev/null && echo True
	@./$(BIN) build Tests/AsyncCancellationIntegration >/dev/null
	@./Tests/AsyncCancellationIntegration/bin/AsyncCancellationIntegration
	@./$(BIN) publish Tests/AsyncCancellationIntegration >/dev/null && echo True
	@grep -Fq 'OperationCanceledException' Compiler/src/async_lower.c && grep -Fq '"SetCanceled"' Compiler/src/async_lower.c && grep -Fq '"CancellationToken"' Compiler/src/async_lower.c && echo True
	@! grep -Fq 'CancellationTokenSource' Compiler/src/async_lower.c && ! grep -Fq '"Register"' Compiler/src/async_lower.c && echo True
	@grep -Fq '__voidc$$async$$Program$$AwaitCanceledTyped' Tests/AsyncCancellationIntegration/.void/AsyncCancellationIntegration.c && grep -Fq 'OperationCanceledException' Tests/AsyncCancellationIntegration/.void/AsyncCancellationIntegration.c && echo True
	@./$(BIN) check Tests/AsyncExceptionCatchFinallyIntegration >/dev/null && echo True
	@./$(BIN) check Tests/AsyncMultipleAwaitStructuredControlFlow >/dev/null && echo True
	@./$(BIN) check Tests/TaskCancellationIntegration >/dev/null && echo True
	@./$(BIN) check Tests/TaskContinuationCompletionSourceFoundation >/dev/null && echo True


# Milestone 208: Async Generics, Closures & GC Lifetime Integration
test-async-generics-closures-gc-lifetime-integration: $(BIN)
	@./$(BIN) check Tests/AsyncGenericsClosuresGcLifetimeIntegration >/dev/null && echo True
	@./$(BIN) build Tests/AsyncGenericsClosuresGcLifetimeIntegration >/dev/null
	@./Tests/AsyncGenericsClosuresGcLifetimeIntegration/bin/AsyncGenericsClosuresGcLifetimeIntegration
	@./$(BIN) publish Tests/AsyncGenericsClosuresGcLifetimeIntegration >/dev/null && echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/AsyncGenericsClosuresGcLifetimeIntegration/bin/AsyncGenericsClosuresGcLifetimeIntegration 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+' && echo True
	@grep -Fq '__voidc$$async$$Program$$EchoAfter__gm1_Node208' Tests/AsyncGenericsClosuresGcLifetimeIntegration/.void/AsyncGenericsClosuresGcLifetimeIntegration.c && grep -Fq '__voidc$$async$$Program$$GenericClosureAfter__gm1_Node208' Tests/AsyncGenericsClosuresGcLifetimeIntegration/.void/AsyncGenericsClosuresGcLifetimeIntegration.c && echo True
	@grep -Fq '__voidc$$async$$AsyncBox208__g1_Node208$$GetAfter' Tests/AsyncGenericsClosuresGcLifetimeIntegration/.void/AsyncGenericsClosuresGcLifetimeIntegration.c && grep -Fq '__voidc$$async$$AsyncBox208__g1_int$$EchoAfter__gm1_Node208' Tests/AsyncGenericsClosuresGcLifetimeIntegration/.void/AsyncGenericsClosuresGcLifetimeIntegration.c && echo True
	@grep -Fq 'vc_closure_find' Tests/AsyncGenericsClosuresGcLifetimeIntegration/.void/AsyncGenericsClosuresGcLifetimeIntegration.c && grep -Fq 'vc_gc_mark((void *)vc_frame->vc_cap_' Tests/AsyncGenericsClosuresGcLifetimeIntegration/.void/AsyncGenericsClosuresGcLifetimeIntegration.c && echo True
	@grep -Fq 'semantic_lambda_for_node' Compiler/src/async_lower.c && grep -Fq 'rewrite_lambda_capture' Compiler/src/async_lower.c && echo True
	@! grep -Fq 'async methods with captured lambda/delegate bodies require async closure integration' Compiler/src/async_lower.c && echo True
	@./$(BIN) check Tests/AsyncCancellationIntegration >/dev/null && echo True
	@./$(BIN) check Tests/AsyncExceptionCatchFinallyIntegration >/dev/null && echo True
	@./$(BIN) check Tests/AsyncMultipleAwaitStructuredControlFlowDiagnostics/LambdaCapture >/dev/null && echo True
	@./$(BIN) check Tests/GenericMethodCalls >/dev/null && echo True
	@./$(BIN) check Tests/ClosureCompletionII >/dev/null && echo True


# Milestone 209: Async Object Model & Library Boundary Integration
test-async-object-model-library-boundary-integration: $(BIN) $(ASYNC_OBJECT_MODEL_NATIVE_FIXTURE)
	@./$(BIN) check Tests/AsyncObjectModelLibraryBoundaryIntegration/Runtime >/dev/null && echo True
	@./$(BIN) build Tests/AsyncObjectModelLibraryBoundaryIntegration/Runtime >/dev/null
	@./Tests/AsyncObjectModelLibraryBoundaryIntegration/Runtime/bin/AsyncObjectModelLibraryBoundaryIntegration
	@grep -Fq '__voidc$$async$$IDefaultAsync209$$Read' Tests/AsyncObjectModelLibraryBoundaryIntegration/Runtime/.void/AsyncObjectModelLibraryBoundaryIntegration.c && grep -Fq '__voidc$$async$$AsyncOverride209$$Calculate' Tests/AsyncObjectModelLibraryBoundaryIntegration/Runtime/.void/AsyncObjectModelLibraryBoundaryIntegration.c && grep -Fq 'strncmp(current->name, "__voidc$$async$$", 14)' Compiler/src/compiler.c && echo True
	@./$(BIN) check Tests/AsyncObjectModelLibraryBoundaryIntegration/Library >/dev/null && echo True
	@./$(BIN) build Tests/AsyncObjectModelLibraryBoundaryIntegration/Library >/dev/null
	@test -f Tests/AsyncObjectModelLibraryBoundaryIntegration/Library/bin/libVoid209AsyncLib.a && echo True
	@set -e; symbols="$$(nm -g --defined-only Tests/AsyncObjectModelLibraryBoundaryIntegration/Library/bin/libVoid209AsyncLib.a)"; printf '%s\n' "$$symbols" | grep -Fq ' void209_async_library_value'; ! printf '%s\n' "$$symbols" | grep -Fq ' Core'; echo True
	@mkdir -p Tests/AsyncObjectModelLibraryBoundaryIntegration/Consumer/native
	@cp Tests/AsyncObjectModelLibraryBoundaryIntegration/Library/bin/libVoid209AsyncLib.a Tests/AsyncObjectModelLibraryBoundaryIntegration/Consumer/native/libVoid209AsyncLib.a
	@./$(BIN) build Tests/AsyncObjectModelLibraryBoundaryIntegration/Consumer >/dev/null
	@./Tests/AsyncObjectModelLibraryBoundaryIntegration/Consumer/bin/AsyncObjectModelLibraryConsumer209
	@$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 Tests/AsyncObjectModelLibraryBoundaryIntegration/CConsumer/main.c Tests/AsyncObjectModelLibraryBoundaryIntegration/Library/bin/libVoid209AsyncLib.a -pthread -o Tests/AsyncObjectModelLibraryBoundaryIntegration/CConsumer/async-library-consumer
	@./Tests/AsyncObjectModelLibraryBoundaryIntegration/CConsumer/async-library-consumer
	@./$(BIN) publish Tests/AsyncObjectModelLibraryBoundaryIntegration/Library >/dev/null; test -f Tests/AsyncObjectModelLibraryBoundaryIntegration/Library/publish/libVoid209AsyncLib.a; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncObjectModelLibraryBoundaryDiagnostics/AsyncExport >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "async method 'Run' cannot be exported as a native library symbol" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncObjectModelLibraryBoundaryDiagnostics/AsyncInterfaceSignature >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "async method 'Run' requires a body" $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/AsyncInterface >/dev/null && echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/AsyncAbstract >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "async method 'Bad' cannot be abstract" $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/ObjectModelIntegration >/dev/null; ./$(BIN) check Tests/DefaultInterfaceMethods >/dev/null; ./$(BIN) check Tests/DelegateCompletion >/dev/null; ./$(BIN) check Tests/StaticEvents >/dev/null; echo True
	@./$(BIN) check Tests/AsyncGenericsClosuresGcLifetimeIntegration >/dev/null; ./$(BIN) check Tests/AsyncCancellationIntegration >/dev/null; ./$(BIN) check Tests/LibraryOutput/Library >/dev/null; echo True


# Milestone 210: Async/Await & Task Runtime Integration Audit
# Repeated race coverage is deliberately separate from `make test`.
test-async-await-task-runtime-integration-audit-stress: $(BIN)
	@./$(BIN) check Tests/AsyncAwaitTaskRuntimeIntegrationStress >/dev/null
	@./$(BIN) build Tests/AsyncAwaitTaskRuntimeIntegrationStress >/dev/null
	@set -e; i=0; while [ $$i -lt 50 ]; do out=$$(mktemp); timeout 20s ./Tests/AsyncAwaitTaskRuntimeIntegrationStress/bin/AsyncAwaitTaskRuntimeIntegrationStress >$$out; test "$$(grep -c '^True$$' $$out)" -eq 4; test -z "$$(grep -v '^True$$' $$out)"; rm -f $$out; i=$$((i + 1)); done; echo True

test-async-await-task-runtime-integration-audit: $(BIN)
	@./$(BIN) check Tests/AsyncAwaitTaskRuntimeIntegrationAudit >/dev/null && echo True
	@./$(BIN) build Tests/AsyncAwaitTaskRuntimeIntegrationAudit >/dev/null
	@timeout 30s ./Tests/AsyncAwaitTaskRuntimeIntegrationAudit/bin/AsyncAwaitTaskRuntimeIntegrationAudit
	@set -e; ./$(BIN) publish Tests/AsyncAwaitTaskRuntimeIntegrationAudit >/dev/null; test -x Tests/AsyncAwaitTaskRuntimeIntegrationAudit/publish/AsyncAwaitTaskRuntimeIntegrationAudit$(EXE_SUFFIX); echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/AsyncAwaitTaskRuntimeIntegrationAudit/bin/AsyncAwaitTaskRuntimeIntegrationAudit 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@grep -Fq '__voidc$$async$$Program$$MultiAwait' Tests/AsyncAwaitTaskRuntimeIntegrationAudit/.void/AsyncAwaitTaskRuntimeIntegrationAudit.c && grep -Fq '__voidc$$async$$Program$$GenericClosure__gm1_AuditNode210' Tests/AsyncAwaitTaskRuntimeIntegrationAudit/.void/AsyncAwaitTaskRuntimeIntegrationAudit.c && grep -Fq '__voidc$$async$$AuditService210$$Compute' Tests/AsyncAwaitTaskRuntimeIntegrationAudit/.void/AsyncAwaitTaskRuntimeIntegrationAudit.c; echo True
	@grep -Fq 'vc_gc_mark((void *)vc_frame->vc_cap_' Tests/AsyncAwaitTaskRuntimeIntegrationAudit/.void/AsyncAwaitTaskRuntimeIntegrationAudit.c && grep -Fq 'TaskCompletionSource__g1_AuditNode210' Tests/AsyncAwaitTaskRuntimeIntegrationAudit/.void/AsyncAwaitTaskRuntimeIntegrationAudit.c && grep -Fq 'TaskCompletionSource__g1_int' Tests/AsyncAwaitTaskRuntimeIntegrationAudit/.void/AsyncAwaitTaskRuntimeIntegrationAudit.c; echo True
	@grep -Fq '"ContinueWith"' Compiler/src/async_lower.c && grep -Fq 'ThreadPool.QueueTaskContinuation(continuations[i]);' StandardLibrary/Void/Threading/Tasks/Task.void && grep -Fq 'internal static bool QueueTaskContinuation(Action workItem)' StandardLibrary/Void/Threading/ThreadPool.void; echo True
	@! grep -R -Fq 'QueueTaskContinuation' Compiler/src Runtime/include Runtime/src; echo True
	@./$(BIN) check Tests/AsyncAwaitTaskRuntimeIntegrationAuditRuntime/LatePendingAwaitAfterShutdown >/dev/null; ./$(BIN) build Tests/AsyncAwaitTaskRuntimeIntegrationAuditRuntime/LatePendingAwaitAfterShutdown >/dev/null
	@out=$$(mktemp); if timeout 5s ./Tests/AsyncAwaitTaskRuntimeIntegrationAuditRuntime/LatePendingAwaitAfterShutdown/bin/LatePendingAwaitAfterShutdown >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: thread pool has been shut down' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/AsyncMultipleAwaitStructuredControlFlow >/dev/null; ./$(BIN) check Tests/AsyncExceptionCatchFinallyIntegration >/dev/null; ./$(BIN) check Tests/AsyncCancellationIntegration >/dev/null; ./$(BIN) check Tests/AsyncGenericsClosuresGcLifetimeIntegration >/dev/null; ./$(BIN) check Tests/AsyncObjectModelLibraryBoundaryIntegration/Runtime >/dev/null; echo True
	@./$(BIN) check Tests/ThreadPoolTaskIntegrationAudit >/dev/null; ./$(BIN) check Tests/TaskContinuationCompletionSourceFoundation >/dev/null; ./$(BIN) check Tests/TaskCancellationIntegration >/dev/null; echo True
	@./$(BIN) build Tests/AsyncObjectModelLibraryBoundaryIntegration/Library >/dev/null
	@mkdir -p Tests/AsyncObjectModelLibraryBoundaryIntegration/Consumer/native; cp Tests/AsyncObjectModelLibraryBoundaryIntegration/Library/bin/libVoid209AsyncLib.a Tests/AsyncObjectModelLibraryBoundaryIntegration/Consumer/native/libVoid209AsyncLib.a; ./$(BIN) build Tests/AsyncObjectModelLibraryBoundaryIntegration/Consumer >/dev/null; ./Tests/AsyncObjectModelLibraryBoundaryIntegration/Consumer/bin/AsyncObjectModelLibraryConsumer209
	@$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 Tests/AsyncObjectModelLibraryBoundaryIntegration/CConsumer/main.c Tests/AsyncObjectModelLibraryBoundaryIntegration/Library/bin/libVoid209AsyncLib.a -pthread -o Tests/AsyncObjectModelLibraryBoundaryIntegration/CConsumer/async-library-consumer; ./Tests/AsyncObjectModelLibraryBoundaryIntegration/CConsumer/async-library-consumer


# Milestone 211: General Await Expression Lowering Foundation
test-general-await-expression-lowering-foundation: $(BIN)
	@./$(BIN) check Tests/GeneralAwaitExpressionLoweringFoundation >/dev/null && echo True
	@./$(BIN) build Tests/GeneralAwaitExpressionLoweringFoundation >/dev/null
	@./Tests/GeneralAwaitExpressionLoweringFoundation/bin/GeneralAwaitExpressionLoweringFoundation
	@set -e; ./$(BIN) publish Tests/GeneralAwaitExpressionLoweringFoundation >/dev/null; test -x Tests/GeneralAwaitExpressionLoweringFoundation/publish/GeneralAwaitExpressionLoweringFoundation$(EXE_SUFFIX); echo True
	@./$(BIN) check Tests/GeneralAwaitExpressionLoweringDiagnostics/CallArgument >/dev/null && echo True
	@./$(BIN) check Tests/GeneralAwaitExpressionLoweringDiagnostics/ShortCircuit >/dev/null && echo True
	@grep -Fq 'normalize_async_foundation_expression' Compiler/src/async_lower.c && grep -Fq 'async_spill_expression' Compiler/src/async_lower.c && grep -Fq '__voidc_async_spill_' Compiler/src/async_lower.c && echo True
	@grep -Fq '__voidc$$async$$Program$$ManagedLeftSpill' Tests/GeneralAwaitExpressionLoweringFoundation/.void/GeneralAwaitExpressionLoweringFoundation.c && grep -Fq 'TaskCompletionSource__g1_int' Tests/GeneralAwaitExpressionLoweringFoundation/.void/GeneralAwaitExpressionLoweringFoundation.c && echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/GeneralAwaitExpressionLoweringFoundation/bin/GeneralAwaitExpressionLoweringFoundation 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@./$(BIN) check Tests/AsyncAwaitTaskRuntimeIntegrationAudit >/dev/null && echo True
	@./$(BIN) check Tests/AsyncGenericsClosuresGcLifetimeIntegration >/dev/null && echo True
	@./$(BIN) check Tests/AsyncMultipleAwaitStructuredControlFlow >/dev/null && echo True


# Milestone 212: Awaited Conditions & Expression Evaluation Completion
test-awaited-conditions-expression-evaluation-completion: $(BIN)
	@./$(BIN) check Tests/AwaitedConditionsExpressionEvaluationCompletion >/dev/null && echo True
	@./$(BIN) build Tests/AwaitedConditionsExpressionEvaluationCompletion >/dev/null
	@./Tests/AwaitedConditionsExpressionEvaluationCompletion/bin/AwaitedConditionsExpressionEvaluationCompletion
	@set -e; ./$(BIN) publish Tests/AwaitedConditionsExpressionEvaluationCompletion >/dev/null; test -x Tests/AwaitedConditionsExpressionEvaluationCompletion/publish/AwaitedConditionsExpressionEvaluationCompletion$(EXE_SUFFIX); echo True
	@./$(BIN) check Tests/GeneralAwaitExpressionLoweringDiagnostics/CallArgument >/dev/null && echo True
	@./$(BIN) check Tests/GeneralAwaitExpressionLoweringDiagnostics/ShortCircuit >/dev/null && echo True
	@./$(BIN) check Tests/AwaitedConditionsExpressionEvaluationDiagnostics/SwitchExpressionArm >/dev/null && echo True
	@./$(BIN) check Tests/AwaitedConditionsExpressionEvaluationDiagnostics/ForeachAwait >/dev/null && echo True
	@grep -Fq 'async_make_lazy_binary' Compiler/src/async_lower.c && grep -Fq 'async_make_conditional_expression' Compiler/src/async_lower.c && grep -Fq 'async_normalize_call' Compiler/src/async_lower.c && echo True
	@grep -Fq '__voidc$$async$$Program$$CallArguments' Tests/AwaitedConditionsExpressionEvaluationCompletion/.void/AwaitedConditionsExpressionEvaluationCompletion.c && grep -Fq '__voidc$$async$$Program$$ForConditionIncrement' Tests/AwaitedConditionsExpressionEvaluationCompletion/.void/AwaitedConditionsExpressionEvaluationCompletion.c && echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/AwaitedConditionsExpressionEvaluationCompletion/bin/AwaitedConditionsExpressionEvaluationCompletion 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@./$(BIN) check Tests/GeneralAwaitExpressionLoweringFoundation >/dev/null && echo True
	@./$(BIN) check Tests/AsyncAwaitTaskRuntimeIntegrationAudit >/dev/null && echo True
	@./$(BIN) check Tests/AsyncMultipleAwaitStructuredControlFlow >/dev/null && echo True


# Milestone 213: Synchronous Foreach Suspension & Cleanup Completion
test-synchronous-foreach-suspension-cleanup-completion: $(BIN)
	@./$(BIN) check Tests/SynchronousForeachSuspensionCleanupCompletion >/dev/null && echo True
	@./$(BIN) build Tests/SynchronousForeachSuspensionCleanupCompletion >/dev/null
	@./Tests/SynchronousForeachSuspensionCleanupCompletion/bin/SynchronousForeachSuspensionCleanupCompletion
	@set -e; ./$(BIN) publish Tests/SynchronousForeachSuspensionCleanupCompletion >/dev/null; test -x Tests/SynchronousForeachSuspensionCleanupCompletion/publish/SynchronousForeachSuspensionCleanupCompletion$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SynchronousForeachSuspensionCleanupDiagnostics/LockAwait >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'await cannot be used inside a lock statement' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/AwaitedConditionsExpressionEvaluationDiagnostics/ForeachAwait >/dev/null && echo True
	@grep -Fq 'add_async_foreach' Compiler/src/async_lower.c && grep -Fq 'foreach_array_item_expression' Compiler/src/async_lower.c && grep -Fq '__voidc_async_foreach_enumerator_' Compiler/src/async_lower.c; echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/SynchronousForeachSuspensionCleanupCompletion/bin/SynchronousForeachSuspensionCleanupCompletion 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@./$(BIN) check Tests/AwaitedConditionsExpressionEvaluationCompletion >/dev/null; ./$(BIN) check Tests/AsyncExceptionCatchFinallyIntegration >/dev/null; ./$(BIN) check Tests/ForeachDisposalCompletion >/dev/null; echo True


# Milestone 214: Awaited Switch Expression & Pattern Guard Completion
test-awaited-switch-expression-pattern-guard-completion: $(BIN)
	@./$(BIN) check Tests/AwaitedSwitchExpressionPatternGuardCompletion >/dev/null && echo True
	@./$(BIN) build Tests/AwaitedSwitchExpressionPatternGuardCompletion >/dev/null
	@./Tests/AwaitedSwitchExpressionPatternGuardCompletion/bin/AwaitedSwitchExpressionPatternGuardCompletion
	@set -e; ./$(BIN) publish Tests/AwaitedSwitchExpressionPatternGuardCompletion >/dev/null; test -x Tests/AwaitedSwitchExpressionPatternGuardCompletion/publish/AwaitedSwitchExpressionPatternGuardCompletion$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AwaitedSwitchExpressionPatternGuardDiagnostics/NonBoolGuard >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "switch expression when guard must be bool, got 'int'" $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/AwaitedConditionsExpressionEvaluationDiagnostics/SwitchExpressionArm >/dev/null && echo True
	@grep -Fq 'async_make_switch_expression' Compiler/src/async_lower.c && grep -Fq 'collect_async_pattern_locals' Compiler/src/async_lower.c && grep -Fq 'append_async_pattern_capture_assignments' Compiler/src/async_lower.c && echo True
	@grep -Fq '__voidc$$async$$Program$$PatternResult' Tests/AwaitedSwitchExpressionPatternGuardCompletion/.void/AwaitedSwitchExpressionPatternGuardCompletion.c && grep -Fq '__voidc$$async$$Program$$RecursivePattern' Tests/AwaitedSwitchExpressionPatternGuardCompletion/.void/AwaitedSwitchExpressionPatternGuardCompletion.c && echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/AwaitedSwitchExpressionPatternGuardCompletion/bin/AwaitedSwitchExpressionPatternGuardCompletion 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@./$(BIN) check Tests/SwitchExpressions >/dev/null; ./$(BIN) check Tests/AwaitedConditionsExpressionEvaluationCompletion >/dev/null; ./$(BIN) check Tests/SynchronousForeachSuspensionCleanupCompletion >/dev/null; echo True


# Milestone 215: Async Enumeration Protocol Foundation
test-async-enumeration-protocol-foundation: $(BIN)
	@./$(BIN) check Tests/AsyncEnumerationProtocolFoundation >/dev/null && echo True
	@./$(BIN) build Tests/AsyncEnumerationProtocolFoundation >/dev/null
	@./Tests/AsyncEnumerationProtocolFoundation/bin/AsyncEnumerationProtocolFoundation
	@set -e; ./$(BIN) publish Tests/AsyncEnumerationProtocolFoundation >/dev/null; test -x Tests/AsyncEnumerationProtocolFoundation/publish/AsyncEnumerationProtocolFoundation$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncEnumerationProtocolDiagnostics/MissingMoveNext >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "does not implement interface method 'IAsyncEnumerator<int>.MoveNextAsync'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncEnumerationProtocolDiagnostics/WrongMoveNextResult >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "does not implement interface method 'IAsyncEnumerator<int>.MoveNextAsync'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncEnumerationProtocolDiagnostics/MissingDisposeAsync >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "does not implement interface method 'IAsyncDisposable.DisposeAsync'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncEnumerationProtocolDiagnostics/BadEnumerableReturn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "does not implement interface method 'IAsyncEnumerable<int>.GetAsyncEnumerator'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncEnumerationProtocolDiagnostics/WrongCurrent >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "does not implement interface property 'IAsyncEnumerator<int>.Current'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncEnumerationProtocolDiagnostics/MissingCancellationToken >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "does not implement interface method 'IAsyncEnumerable<int>.GetAsyncEnumerator'" $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/AsyncEnumerationProtocolDiagnostics/AwaitForeachBoundary >/dev/null && echo True
	@grep -Fq 'public interface IAsyncDisposable' StandardLibrary/Void/IAsyncDisposable.void && grep -Fq 'Task DisposeAsync();' StandardLibrary/Void/IAsyncDisposable.void && grep -Fq 'public interface IAsyncEnumerator<T> : IAsyncDisposable' StandardLibrary/Void/Collections/AsyncEnumerable.void && grep -Fq 'Task<bool> MoveNextAsync();' StandardLibrary/Void/Collections/AsyncEnumerable.void && grep -Fq 'GetAsyncEnumerator(CancellationToken cancellationToken = default)' StandardLibrary/Void/Collections/AsyncEnumerable.void; echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/AsyncEnumerationProtocolFoundation/bin/AsyncEnumerationProtocolFoundation 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@./$(BIN) check Tests/TaskContinuationCompletionSourceFoundation >/dev/null && echo True
	@./$(BIN) check Tests/AsyncCancellationIntegration >/dev/null && echo True
	@./$(BIN) check Tests/AsyncExceptionCatchFinallyIntegration >/dev/null && echo True


# Milestone 216: Await Foreach Syntax & Lowering Completion
test-await-foreach-syntax-lowering-completion: $(BIN)
	@./$(BIN) check Tests/AwaitForeachSyntaxLoweringCompletion >/dev/null && echo True
	@./$(BIN) build Tests/AwaitForeachSyntaxLoweringCompletion >/dev/null
	@./Tests/AwaitForeachSyntaxLoweringCompletion/bin/AwaitForeachSyntaxLoweringCompletion
	@set -e; ./$(BIN) publish Tests/AwaitForeachSyntaxLoweringCompletion >/dev/null; test -x Tests/AwaitForeachSyntaxLoweringCompletion/publish/AwaitForeachSyntaxLoweringCompletion$(EXE_SUFFIX); echo True
	@./$(BIN) check Tests/AsyncEnumerationProtocolDiagnostics/AwaitForeachBoundary >/dev/null && echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AwaitForeachSyntaxLoweringDiagnostics/NonAsync >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "await foreach may only be used inside an async method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AwaitForeachSyntaxLoweringDiagnostics/ArraySource >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "is not asynchronously enumerable" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AwaitForeachSyntaxLoweringDiagnostics/MissingGetAsyncEnumerator >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "does not provide an unambiguous GetAsyncEnumerator() method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AwaitForeachSyntaxLoweringDiagnostics/WrongMoveNextResult >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "awaitable type 'bool' must provide an unambiguous GetAwaiter() method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AwaitForeachSyntaxLoweringDiagnostics/MissingCurrent >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "must provide a Current property" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AwaitForeachSyntaxLoweringDiagnostics/MissingDisposeAsync >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "must provide DisposeAsync() returning an awaitable whose GetResult() returns void" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AwaitForeachSyntaxLoweringDiagnostics/TypeMismatch >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "await foreach item of type 'string' cannot be assigned to 'int'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AwaitForeachSyntaxLoweringDiagnostics/LockAwaitForeach >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "await foreach cannot be used inside a lock statement" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AwaitForeachSyntaxLoweringDiagnostics/IterationAssignment >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign to foreach iteration variable 'value'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AwaitForeachSyntaxLoweringDiagnostics/RequiredCancellationToken >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "does not provide an unambiguous GetAsyncEnumerator() method" $$out; rm -f $$out; echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/AwaitForeachSyntaxLoweringCompletion/bin/AwaitForeachSyntaxLoweringCompletion 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@./$(BIN) check Tests/AsyncEnumerationProtocolFoundation >/dev/null && echo True
	@./$(BIN) check Tests/SynchronousForeachSuspensionCleanupCompletion >/dev/null && echo True


# Milestone 217: Async Iterator Method Foundation
test-async-iterator-method-foundation: $(BIN)
	@./$(BIN) check Tests/AsyncIteratorMethodFoundation >/dev/null && echo True
	@./$(BIN) build Tests/AsyncIteratorMethodFoundation >/dev/null
	@./Tests/AsyncIteratorMethodFoundation/bin/AsyncIteratorMethodFoundation
	@set -e; ./$(BIN) publish Tests/AsyncIteratorMethodFoundation >/dev/null; test -x Tests/AsyncIteratorMethodFoundation/publish/AsyncIteratorMethodFoundation$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorMethodDiagnostics/WrongTaskReturn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "async iterator method 'Bad' must return IAsyncEnumerable<T>" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorMethodDiagnostics/WrongEnumeratorReturn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "async iterator method 'Bad' must return IAsyncEnumerable<T>" $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/AsyncIteratorMethodDiagnostics/AwaitForeachInside >/dev/null && echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorMethodDiagnostics/YieldTypeMismatch >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "yield value of type 'string' cannot be assigned to 'int'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorMethodDiagnostics/RefParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "async method 'Bad' cannot declare ref parameter 'value'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorMethodDiagnostics/YieldInCatch >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "yield cannot be used inside an iterator catch block" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorMethodDiagnostics/YieldInFinally >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "yield cannot be used inside an iterator finally block" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorMethodDiagnostics/NonAsyncEnumerableYield >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "yield method 'Bad' must return IEnumerator<T>" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorMethodDiagnostics/LockYield >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "yield cannot be used inside a lock statement" $$out; rm -f $$out; echo True
	@grep -Fq '__voidc$$iterator$$Program$$Pending' Tests/AsyncIteratorMethodFoundation/.void/AsyncIteratorMethodFoundation.c && grep -Fq '__voidc$$async$$__voidc$$iterator$$Program$$Pending' Tests/AsyncIteratorMethodFoundation/.void/AsyncIteratorMethodFoundation.c && grep -Fq 'vc_has_async_iterators' Compiler/src/iterator.c; echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/AsyncIteratorMethodFoundation/bin/AsyncIteratorMethodFoundation 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@./$(BIN) check Tests/AwaitForeachSyntaxLoweringCompletion >/dev/null && echo True
	@./$(BIN) check Tests/AsyncEnumerationProtocolFoundation >/dev/null && echo True


# Milestone 218: Async Iterator Composition Completion
test-async-iterator-composition-completion: $(BIN)
	@./$(BIN) check Tests/AsyncIteratorCompositionCompletion >/dev/null && echo True
	@./$(BIN) build Tests/AsyncIteratorCompositionCompletion >/dev/null
	@./Tests/AsyncIteratorCompositionCompletion/bin/AsyncIteratorCompositionCompletion
	@set -e; ./$(BIN) publish Tests/AsyncIteratorCompositionCompletion >/dev/null; test -x Tests/AsyncIteratorCompositionCompletion/publish/AsyncIteratorCompositionCompletion$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorCompositionDiagnostics/ArraySource >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "await foreach source type 'int[]' is not asynchronously enumerable" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorCompositionDiagnostics/LockAwaitForeach >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "yield cannot be used inside a lock statement" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorCompositionDiagnostics/TypeMismatch >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "await foreach item of type 'string' cannot be assigned to 'int'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorCompositionDiagnostics/WrongMoveNextResult >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "MoveNextAsync() awaitable GetResult() must return bool, got 'int'" $$out; rm -f $$out; echo True
	@grep -Fq '__voidc$$iterator$$Program$$Pass' Tests/AsyncIteratorCompositionCompletion/.void/AsyncIteratorCompositionCompletion.c && grep -Fq '__voidc$$async$$__voidc$$iterator$$Program$$Pass' Tests/AsyncIteratorCompositionCompletion/.void/AsyncIteratorCompositionCompletion.c && grep -Fq 'VC_ITERATOR_ASYNC_FOREACH_CLEANUP' Compiler/src/iterator.c; echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/AsyncIteratorCompositionCompletion/bin/AsyncIteratorCompositionCompletion 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@./$(BIN) check Tests/AsyncIteratorMethodDiagnostics/AwaitForeachInside >/dev/null && echo True
	@./$(BIN) check Tests/AsyncIteratorMethodFoundation >/dev/null && echo True
	@./$(BIN) check Tests/AwaitForeachSyntaxLoweringCompletion >/dev/null && echo True
	@./$(BIN) check Tests/AsyncEnumerationProtocolFoundation >/dev/null && echo True


# Milestone 219: Async Iterator Cancellation Integration
test-async-iterator-cancellation-integration: $(BIN)
	@./$(BIN) check Tests/AsyncIteratorCancellationIntegration >/dev/null && echo True
	@./$(BIN) build Tests/AsyncIteratorCancellationIntegration >/dev/null
	@./Tests/AsyncIteratorCancellationIntegration/bin/AsyncIteratorCancellationIntegration
	@set -e; ./$(BIN) publish Tests/AsyncIteratorCancellationIntegration >/dev/null; test -x Tests/AsyncIteratorCancellationIntegration/publish/AsyncIteratorCancellationIntegration$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorCancellationDiagnostics/NonAsync >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "EnumeratorCancellation may only be used on an async iterator parameter" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorCancellationDiagnostics/WrongType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "EnumeratorCancellation requires a CancellationToken parameter" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorCancellationDiagnostics/Duplicate >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "an async iterator may have only one EnumeratorCancellation parameter" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorCancellationDiagnostics/Arguments >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "EnumeratorCancellation does not accept arguments" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorCancellationDiagnostics/SyncIterator >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "EnumeratorCancellation may only be used on an async iterator parameter" $$out; rm -f $$out; echo True
	@grep -Fq 'EnumeratorCancellation' Compiler/src/semantic.c && grep -Fq 'CanBeCanceled' Compiler/src/iterator.c && grep -Fq 'parse_attribute_lists(parser, &attributes)' Compiler/src/parser.c; echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/AsyncIteratorCancellationIntegration/bin/AsyncIteratorCancellationIntegration 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@./$(BIN) parse Tests/AsyncIteratorCancellationIntegration/Program.void | grep -Fq 'Attribute EnumeratorCancellation'; echo True
	@./$(BIN) check Tests/AsyncIteratorCompositionCompletion >/dev/null && echo True
	@./$(BIN) check Tests/AsyncIteratorMethodFoundation >/dev/null && echo True
	@./$(BIN) check Tests/AwaitForeachSyntaxLoweringCompletion >/dev/null && echo True
	@./$(BIN) check Tests/AsyncEnumerationProtocolFoundation >/dev/null && echo True


# Milestone 220: Async Language & Asynchronous Iteration Integration Audit
test-async-language-asynchronous-iteration-integration-audit: $(BIN)
	@./$(BIN) check Tests/AsyncLanguageAsynchronousIterationIntegrationAudit >/dev/null && echo True
	@./$(BIN) build Tests/AsyncLanguageAsynchronousIterationIntegrationAudit >/dev/null
	@./Tests/AsyncLanguageAsynchronousIterationIntegrationAudit/bin/AsyncLanguageAsynchronousIterationIntegrationAudit
	@set -e; ./$(BIN) publish Tests/AsyncLanguageAsynchronousIterationIntegrationAudit >/dev/null; test -x Tests/AsyncLanguageAsynchronousIterationIntegrationAudit/publish/AsyncLanguageAsynchronousIterationIntegrationAudit$(EXE_SUFFIX); echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/AsyncLanguageAsynchronousIterationIntegrationAudit/bin/AsyncLanguageAsynchronousIterationIntegrationAudit 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@grep -Fq '__voidc$$iterator$$Stream220$$Values' Tests/AsyncLanguageAsynchronousIterationIntegrationAudit/.void/AsyncLanguageAsynchronousIterationIntegrationAudit.c && grep -Fq '__voidc$$async$$__voidc$$iterator$$Stream220$$Values' Tests/AsyncLanguageAsynchronousIterationIntegrationAudit/.void/AsyncLanguageAsynchronousIterationIntegrationAudit.c && grep -Fq '__voidc$$iterator$$Program$$CancellableOuter' Tests/AsyncLanguageAsynchronousIterationIntegrationAudit/.void/AsyncLanguageAsynchronousIterationIntegrationAudit.c; echo True
	@grep -Fq 'lowered_state_owner_chain_allows_access' Compiler/src/semantic.c && grep -Fq 'emit_lowered_state_base_codegen' Compiler/src/compiler.c && grep -Fq 'captures_this || async_iterator' Compiler/src/iterator.c; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AsyncLanguageAsynchronousIterationIntegrationDiagnostics/NativeExport >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "async method 'Values' cannot be exported as a native library symbol" $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Library >/dev/null && echo True
	@./$(BIN) build Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Library >/dev/null
	@grep -Fq '__voidc$$iterator$$AsyncStreamLibrary220$$Values' Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Library/.void/Void220AsyncStreamLib.c && grep -Fq '__voidc$$async$$__voidc$$iterator$$AsyncStreamLibrary220$$Values' Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Library/.void/Void220AsyncStreamLib.c; echo True
	@mkdir -p Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Consumer/native; cp Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Library/bin/libVoid220AsyncStreamLib.a Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Consumer/native/libVoid220AsyncStreamLib.a; ./$(BIN) build Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Consumer >/dev/null; ./Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Consumer/bin/AsyncLanguageAsynchronousIterationLibraryConsumer220
	@$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/CConsumer/main.c Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/Library/bin/libVoid220AsyncStreamLib.a -pthread -o Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/CConsumer/async-stream-consumer; ./Tests/AsyncLanguageAsynchronousIterationIntegrationLibrary/CConsumer/async-stream-consumer
	@./$(BIN) check Tests/GeneralAwaitExpressionLoweringFoundation >/dev/null; ./$(BIN) check Tests/AwaitedConditionsExpressionEvaluationCompletion >/dev/null; ./$(BIN) check Tests/SynchronousForeachSuspensionCleanupCompletion >/dev/null; ./$(BIN) check Tests/AwaitedSwitchExpressionPatternGuardCompletion >/dev/null; echo True
	@./$(BIN) check Tests/AsyncEnumerationProtocolFoundation >/dev/null; ./$(BIN) check Tests/AwaitForeachSyntaxLoweringCompletion >/dev/null; ./$(BIN) check Tests/AsyncIteratorMethodFoundation >/dev/null; ./$(BIN) check Tests/AsyncIteratorCompositionCompletion >/dev/null; ./$(BIN) check Tests/AsyncIteratorCancellationIntegration >/dev/null; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/AwaitForeachSyntaxLoweringDiagnostics/LockAwaitForeach >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "await foreach cannot be used inside a lock statement" $$out; rm -f $$out; out=$$(mktemp); if ./$(BIN) check Tests/AsyncIteratorCancellationDiagnostics/Duplicate >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "an async iterator may have only one EnumeratorCancellation parameter" $$out; rm -f $$out; echo True
	@grep -Fq 'public interface IAsyncEnumerable<T>' StandardLibrary/Void/Collections/AsyncEnumerable.void && grep -Fq 'Task<bool> MoveNextAsync();' StandardLibrary/Void/Collections/AsyncEnumerable.void && ! grep -R -Fq 'IAsyncEnumerable' Runtime/include Runtime/src; echo True


# Milestone 221: By-Reference Value & Ref Local Foundation
test-by-reference-value-ref-local-foundation: $(BIN)
	@./$(BIN) check Tests/ByReferenceValueRefLocalFoundation >/dev/null && echo True
	@./$(BIN) build Tests/ByReferenceValueRefLocalFoundation >/dev/null
	@./Tests/ByReferenceValueRefLocalFoundation/bin/ByReferenceValueRefLocalFoundation
	@set -e; ./$(BIN) publish Tests/ByReferenceValueRefLocalFoundation >/dev/null; test -x Tests/ByReferenceValueRefLocalFoundation/publish/ByReferenceValueRefLocalFoundation$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceValueRefLocalDiagnostics/NoInitializer >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "expected '=' after ref local declaration" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceValueRefLocalDiagnostics/MissingRefInitializer >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "expected 'ref' in ref local initializer" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceValueRefLocalDiagnostics/TypeMismatch >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "requires storage of exactly type 'int', got 'long'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceValueRefLocalDiagnostics/InParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot create writable ref local from in parameter 'value'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceValueRefLocalDiagnostics/ReadonlyField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot create writable ref local from readonly storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceValueRefLocalDiagnostics/ConstField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot create ref local from const storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceValueRefLocalDiagnostics/PropertySource >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref local initializer requires a writable local, parameter, field, or array element" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceValueRefLocalDiagnostics/ForeachSource >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot create ref local from foreach iteration variable 'value'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceValueRefLocalDiagnostics/UsingSource >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot create ref local from using variable 'resource'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceValueRefLocalDiagnostics/LambdaCapture >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref local 'alias' cannot be captured by a lambda" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceValueRefLocalDiagnostics/AsyncMethod >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref locals are not supported in async or iterator methods yet" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceValueRefLocalDiagnostics/IteratorMethod >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref locals are not supported in async or iterator methods yet" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceValueRefLocalDiagnostics/TemporaryStruct >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref local initializer requires a writable local, parameter, field, or array element" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceValueRefLocalDiagnostics/NullConditionalArray >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref local initializer requires a writable local, parameter, field, or array element" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceValueRefLocalDiagnostics/UnassignedOutRead >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref local 'alias' refers to storage that is not definitely assigned" $$out; rm -f $$out; echo True
	@./$(BIN) parse Tests/ByReferenceValueRefLocalFoundation/Program.void | grep -Fq 'Local ref arrayElement : var'; echo True
	@grep -Eq 'int32_t \*vc_l_[0-9]+ = &\(vc_l_[0-9]+\);' Tests/ByReferenceValueRefLocalFoundation/.void/ByReferenceValueRefLocalFoundation.c && grep -Fq 'vc_ref_owner_' Tests/ByReferenceValueRefLocalFoundation/.void/ByReferenceValueRefLocalFoundation.c && grep -Eq 'vc_c_[0-9]+ \* \*vc_l_[0-9]+ = &\(' Tests/ByReferenceValueRefLocalFoundation/.void/ByReferenceValueRefLocalFoundation.c; echo True
	@grep -Fq 'emit_ref_local_declaration' Compiler/src/compiler.c && grep -Fq 'ref_has_local_target' Compiler/src/semantic.c && grep -Fq 'Local ref' Compiler/src/ast.c; echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/ByReferenceValueRefLocalFoundation/bin/ByReferenceValueRefLocalFoundation 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@./$(BIN) parse Tests/RefOutIn.void >/dev/null && echo True
	@./$(BIN) check Tests/AsyncLanguageAsynchronousIterationIntegrationAudit >/dev/null && echo True


# Milestone 222: Ref Assignment & Aliasing Completion
test-ref-assignment-aliasing-completion: $(BIN)
	@./$(BIN) check Tests/RefAssignmentAliasingCompletion >/dev/null && echo True
	@./$(BIN) build Tests/RefAssignmentAliasingCompletion >/dev/null
	@./Tests/RefAssignmentAliasingCompletion/bin/RefAssignmentAliasingCompletion
	@set -e; ./$(BIN) publish Tests/RefAssignmentAliasingCompletion >/dev/null; test -x Tests/RefAssignmentAliasingCompletion/publish/RefAssignmentAliasingCompletion$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefAssignmentAliasingDiagnostics/NonRefTarget >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref assignment target 'a' must be a ref local" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefAssignmentAliasingDiagnostics/RefParameterTarget >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref assignment target 'a' must be a ref local" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefAssignmentAliasingDiagnostics/TypeMismatch >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref assignment to 'alias' of type 'int' requires storage of exactly type 'int', got 'long'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefAssignmentAliasingDiagnostics/InSource >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot create writable ref local from in parameter 'value'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefAssignmentAliasingDiagnostics/ReadonlySource >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot create writable ref local from readonly storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefAssignmentAliasingDiagnostics/PropertySource >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref local initializer requires a writable local, parameter, field, or array element" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefAssignmentAliasingDiagnostics/TemporaryStruct >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref local initializer requires a writable local, parameter, field, or array element" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefAssignmentAliasingDiagnostics/NullConditionalArray >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref local initializer requires a writable local, parameter, field, or array element" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefAssignmentAliasingDiagnostics/ConditionalOutRead >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref local 'alias' refers to storage that is not definitely assigned" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefAssignmentAliasingDiagnostics/LoopOutRead >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref local 'alias' refers to storage that is not definitely assigned" $$out; rm -f $$out; echo True
	@./$(BIN) parse Tests/RefAssignmentAliasingCompletion/Program.void | grep -Fq 'Assignment = ref'; echo True
	@grep -Eq 'void \*vc_l_[0-9]+_owner = NULL;' Tests/RefAssignmentAliasingCompletion/.void/RefAssignmentAliasingCompletion.c && grep -Eq 'vc_l_[0-9]+_owner = vc_l_[0-9]+_owner;' Tests/RefAssignmentAliasingCompletion/.void/RefAssignmentAliasingCompletion.c && grep -Eq 'vc_l_[0-9]+ = vc_l_[0-9]+, \*vc_l_[0-9]+' Tests/RefAssignmentAliasingCompletion/.void/RefAssignmentAliasingCompletion.c; echo True
	@grep -Fq 'snapshot_ref_flow' Compiler/src/semantic.c && grep -Fq 'find_codegen_ref_name' Compiler/src/compiler.c && grep -Fq 'is_ref_assignment' Compiler/src/parser.c; echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/RefAssignmentAliasingCompletion/bin/RefAssignmentAliasingCompletion 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@./$(BIN) check Tests/ByReferenceValueRefLocalFoundation >/dev/null && echo True
	@./$(BIN) parse Tests/RefOutIn.void >/dev/null && echo True


test-ref-return-method-completion: $(BIN)
	@./$(BIN) check Tests/RefReturnMethodCompletion >/dev/null && echo True
	@./$(BIN) build Tests/RefReturnMethodCompletion >/dev/null
	@./Tests/RefReturnMethodCompletion/bin/RefReturnMethodCompletion
	@set -e; ./$(BIN) publish Tests/RefReturnMethodCompletion >/dev/null; test -x Tests/RefReturnMethodCompletion/publish/RefReturnMethodCompletion$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturnMethodDiagnostics/LocalReturn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return a ref to storage whose lifetime ends with the method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturnMethodDiagnostics/ByValueParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return a ref to storage whose lifetime ends with the method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturnMethodDiagnostics/InParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return readonly storage by writable ref" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturnMethodDiagnostics/PropertySource >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "return ref requires writable local, parameter, field, array element, or ref-returning call storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturnMethodDiagnostics/ReadonlyField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return readonly storage by writable ref" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturnMethodDiagnostics/MissingReturnRef >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref-returning method requires 'return ref'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturnMethodDiagnostics/UnexpectedReturnRef >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "'return ref' requires a ref-returning method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturnMethodDiagnostics/AsyncMethod >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref-returning method 'Bad' cannot be async" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturnMethodDiagnostics/TemporaryStructReceiver >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref-returning call receiver does not have a stable lifetime" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturnMethodDiagnostics/OverrideShape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "override 'Get' must return by ref" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturnMethodDiagnostics/InterfaceShape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "class 'C' does not implement interface method 'I.Get'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturnMethodDiagnostics/NullConditionalCall >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "null-conditional call cannot return writable storage by ref" $$out; rm -f $$out; echo True
	@./$(BIN) parse Tests/RefReturnMethodCompletion/Program.void | grep -Fq 'Method StaticRef : ref int'; echo True
	@grep -Fq 'typedef struct VcRefReturn' Tests/RefReturnMethodCompletion/.void/RefReturnMethodCompletion.c && grep -Fq 'static VcRefReturn vc_m_' Tests/RefReturnMethodCompletion/.void/RefReturnMethodCompletion.c && grep -Fq '.owner' Tests/RefReturnMethodCompletion/.void/RefReturnMethodCompletion.c; echo True
	@grep -Fq 'returns_ref' Compiler/include/semantic.h && grep -Fq 'emit_ref_return_capture' Compiler/src/compiler.c && grep -Fq 'ref_return_storage_escapes_safely' Compiler/src/semantic.c; echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/RefReturnMethodCompletion/bin/RefReturnMethodCompletion 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@./$(BIN) check Tests/RefAssignmentAliasingCompletion >/dev/null && echo True
	@./$(BIN) check Tests/ByReferenceValueRefLocalFoundation >/dev/null && echo True
	@./$(BIN) parse Tests/RefOutIn.void >/dev/null && echo True


test: $(BIN)
	@$(PYTHON) Tests/run_full_suite.py $(if $(strip $(TEST_JOBS)),--jobs $(TEST_JOBS),)


test-ref-returning-properties-indexers-completion: $(BIN)
	@./$(BIN) check Tests/RefReturningPropertiesIndexersCompletion >/dev/null && echo True
	@./$(BIN) build Tests/RefReturningPropertiesIndexersCompletion >/dev/null
	@./Tests/RefReturningPropertiesIndexersCompletion/bin/RefReturningPropertiesIndexersCompletion
	@set -e; ./$(BIN) publish Tests/RefReturningPropertiesIndexersCompletion >/dev/null; test -x Tests/RefReturningPropertiesIndexersCompletion/publish/RefReturningPropertiesIndexersCompletion$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturningPropertiesIndexersDiagnostics/Setter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref-returning property 'P' cannot declare a setter" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturningPropertiesIndexersDiagnostics/Auto >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref-returning property 'P' requires an explicit getter body" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturningPropertiesIndexersDiagnostics/MissingExpressionRef >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref-returning property expression body requires ref" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturningPropertiesIndexersDiagnostics/MissingReturnRef >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref-returning property getter requires 'return ref'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturningPropertiesIndexersDiagnostics/TemporaryStructReceiver >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref-returning member receiver does not have a stable lifetime" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturningPropertiesIndexersDiagnostics/TemporaryIndexerReceiver >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref-returning member receiver does not have a stable lifetime" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturningPropertiesIndexersDiagnostics/NullConditionalProperty >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "null-conditional access cannot produce a writable ref" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturningPropertiesIndexersDiagnostics/OverrideShape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "override property 'P' must preserve ref return shape" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturningPropertiesIndexersDiagnostics/InterfaceShape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "class 'C' does not implement interface property 'I.P'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReturningPropertiesIndexersDiagnostics/NormalPropertyRef >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref local initializer requires a writable local, parameter, field, or array element" $$out; rm -f $$out; echo True
	@./$(BIN) parse Tests/RefReturningPropertiesIndexersCompletion/Program.void | grep -Fq 'Property RefValue : ref int'; echo True
	@grep -Fq 'static VcRefReturn vc_pg_' Tests/RefReturningPropertiesIndexersCompletion/.void/RefReturningPropertiesIndexersCompletion.c && grep -Fq 'vc_ipget_' Tests/RefReturningPropertiesIndexersCompletion/.void/RefReturningPropertiesIndexersCompletion.c && grep -Fq '.owner' Tests/RefReturningPropertiesIndexersCompletion/.void/RefReturningPropertiesIndexersCompletion.c; echo True

test-ref-readonly-scoped-escape-completion: $(BIN)
	@./$(BIN) check Tests/RefReadonlyScopedEscapeCompletion >/dev/null && echo True
	@./$(BIN) build Tests/RefReadonlyScopedEscapeCompletion >/dev/null
	@./Tests/RefReadonlyScopedEscapeCompletion/bin/RefReadonlyScopedEscapeCompletion
	@set -e; ./$(BIN) publish Tests/RefReadonlyScopedEscapeCompletion >/dev/null; test -x Tests/RefReadonlyScopedEscapeCompletion/publish/RefReadonlyScopedEscapeCompletion$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/AsyncRefReadonlyLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref locals are not supported in async or iterator methods yet" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/IteratorRefReadonlyLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref locals are not supported in async or iterator methods yet" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/LambdaCaptureRefReadonlyLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref local 'alias' cannot be captured by a lambda" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/WriteReadonlyLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign through a readonly receiver" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/MutateReadonlyStructMember >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign through a readonly receiver" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/WriteReadonlyProperty >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign through a readonly receiver" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/WriteReadonlyCall >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign through a readonly receiver" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/WriteReadonlyIndexer >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign through a readonly receiver" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/WritableFromReadonlyReturn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot create writable ref local from ref readonly return" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/WritableFromReadonlyProperty >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot create writable ref local from ref readonly member" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/WritableReturnFromReadonly >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return a ref readonly value by writable ref" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/RefArgumentReadonlyLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot pass readonly storage as ref" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/OutArgumentReadonlyLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot pass readonly storage as out" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/ScopedValueParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "scoped parameter requires ref, in, or out" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/ScopedValueLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "expected 'ref', found 'int'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/ScopedRefParameterEscape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return a ref to storage whose lifetime ends with the method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/ScopedInParameterEscape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return a ref to storage whose lifetime ends with the method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/ScopedOutParameterEscape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return a ref to storage whose lifetime ends with the method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/ScopedLocalEscape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return a ref to storage whose lifetime ends with the method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/ScopedReadonlyLocalEscape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return a ref to storage whose lifetime ends with the method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/ForwardLocalEscape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return a ref from a call whose receiver lifetime ends with the method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/ForwardScopedParameterEscape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return a ref from a call whose receiver lifetime ends with the method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/OverrideShape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "override 'Get' must preserve ref return shape" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/InterfaceShape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "class 'C225Diag' does not implement interface method 'I225Diag.Get'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/OverridePropertyShape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "override property 'View' must preserve ref return shape" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/InterfacePropertyShape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "class 'C225Diag' does not implement interface property 'I225Diag.View'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/OverrideScopedContract >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "override 'Use' must preserve scoped parameter contract" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefReadonlyScopedEscapeDiagnostics/InterfaceScopedContract >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "class 'C225Diag' does not implement interface method 'I225Diag.Use'" $$out; rm -f $$out; echo True
	@./$(BIN) parse Tests/RefReadonlyScopedEscapeCompletion/Program.void | grep -Fq 'Method StaticView : ref readonly int'; echo True
	@./$(BIN) parse Tests/RefReadonlyScopedEscapeCompletion/Program.void | grep -Fq 'Property View : ref readonly int'; echo True
	@./$(BIN) parse Tests/RefReadonlyScopedEscapeCompletion/Program.void | grep -Fq 'Parameter scoped ref value : int'; echo True
	@./$(BIN) parse Tests/RefReadonlyScopedEscapeCompletion/Program.void | grep -Fq 'Parameter scoped in value : int'; echo True
	@./$(BIN) parse Tests/RefReadonlyScopedEscapeCompletion/Program.void | grep -Fq 'Parameter scoped out value : int'; echo True
	@./$(BIN) parse Tests/RefReadonlyScopedEscapeCompletion/Program.void | grep -Fq 'Local scoped ref readonly scopedReadonly : int'; echo True
	@grep -Fq 'typedef struct VcRefReturn' Tests/RefReadonlyScopedEscapeCompletion/.void/RefReadonlyScopedEscapeCompletion.c && grep -Fq 'static VcRefReturn vc_pg_' Tests/RefReadonlyScopedEscapeCompletion/.void/RefReadonlyScopedEscapeCompletion.c && grep -Fq 'vc_ipget_' Tests/RefReadonlyScopedEscapeCompletion/.void/RefReadonlyScopedEscapeCompletion.c && grep -Fq '.owner' Tests/RefReadonlyScopedEscapeCompletion/.void/RefReadonlyScopedEscapeCompletion.c; echo True
	@grep -Fq 'int32_t const *vc_l_' Tests/RefReadonlyScopedEscapeCompletion/.void/RefReadonlyScopedEscapeCompletion.c; echo True
	@grep -Fq 'returns_ref_readonly' Compiler/include/semantic.h && grep -Fq 'match_contextual_scoped' Compiler/src/parser.c && grep -Fq 'ref_return_storage_escapes_safely' Compiler/src/semantic.c && grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/RefReadonlyScopedEscapeCompletion/bin/RefReadonlyScopedEscapeCompletion 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@./$(BIN) check Tests/RefReturningPropertiesIndexersCompletion >/dev/null && echo True
	@./$(BIN) check Tests/RefReturnMethodCompletion >/dev/null && echo True
	@./$(BIN) check Tests/RefAssignmentAliasingCompletion >/dev/null && echo True
	@./$(BIN) check Tests/ByReferenceValueRefLocalFoundation >/dev/null && echo True
	@./$(BIN) check Tests/DeterministicLifetimeAdvancedAbstractionsIntegration >/dev/null && echo True

# Milestone 226: Ref Struct & Ref Field Foundation
test-ref-struct-foundation: $(BIN)
	@./$(BIN) check Tests/RefStructFoundation >/dev/null && echo True
	@./$(BIN) build Tests/RefStructFoundation >/dev/null
	@./Tests/RefStructFoundation/bin/RefStructFoundation
	@set -e; ./$(BIN) publish Tests/RefStructFoundation >/dev/null; test -x Tests/RefStructFoundation/publish/RefStructFoundation$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/RefFieldOrdinaryStruct >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref field 'X' is supported only in a ref struct" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/RefFieldClass >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref field 'X' is supported only in a ref struct" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/RefFieldStatic >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref field 'X' must be an instance non-const field" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/RefFieldInitializer >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref field 'X' cannot have a field initializer" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/RefFieldReadonlyModifier >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref field 'X' uses 'ref readonly' rather than the readonly field modifier" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/ReadonlyRefStructWritable >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref field 'X' in readonly ref struct 'S' must be ref readonly" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/RefStructClassField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct field 'Value' is allowed only as instance storage inside another ref struct" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/RefStructStructField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct field 'Value' is allowed only as instance storage inside another ref struct" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/RefStructStaticField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct field 'Value' is allowed only as instance storage inside another ref struct" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/RefStructArray >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type 'R' is not supported as a local type" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/RefStructNullable >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type 'R' is not supported as a local type" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/RefStructGenericArgument >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct type argument 'R' is not supported by generic type 'Marker'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/RefStructInterface >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct 'R' cannot implement interfaces" $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/RefStructFoundationDiagnostics/RefStructProperty >/dev/null && echo True
	@./$(BIN) check Tests/RefStructFoundationDiagnostics/RefStructReturn >/dev/null && echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/AsyncLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct local 'value' cannot be used in async or iterator methods" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/IteratorLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct local 'value' cannot be used in async or iterator methods" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/LambdaCaptureLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct local 'value' cannot be captured by a lambda" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/LambdaCaptureThis >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct 'this' cannot be captured by a lambda" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/DelegateMethodGroup >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct instance methods cannot be converted to delegates" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/RefReadonlyFieldWrite >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign through a readonly receiver" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/RefReadonlyFieldRebind >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref readonly field can be rebound only by its declaring constructor" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/ShortLivedRefField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot store a ref to shorter-lived storage in a ref field" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/AsyncParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct parameters cannot be used in async or iterator methods" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/IteratorParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct parameters cannot be used in async or iterator methods" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/StaticRefStruct >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct 'R' cannot be static" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructFoundationDiagnostics/Boxing >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign 'R' to local 'boxed' of type 'object'" $$out; rm -f $$out; echo True
	@./$(BIN) parse Tests/RefStructFoundation/Program.void | grep -Fq 'Struct IntWindow [public ref]'; echo True
	@./$(BIN) parse Tests/RefStructFoundation/Program.void | grep -Fq 'Field Value : ref int'; echo True
	@./$(BIN) parse Tests/RefStructFoundation/Program.void | grep -Fq 'Field Stable : ref readonly int'; echo True
	@./$(BIN) parse Tests/RefStructFoundation/Program.void | grep -Fq 'Struct ReadWindow [public readonly ref]'; echo True
	@grep -Fq 'VcRefReturn vc_f_0;' Tests/RefStructFoundation/.void/RefStructFoundation.c; echo True
	@grep -Fq 'vc_gc_mark(vc_typed->vc_f_0.owner);' Tests/RefStructFoundation/.void/RefStructFoundation.c; echo True
	@! grep -Fq 'vc_box_s_0' Tests/RefStructFoundation/.void/RefStructFoundation.c; echo True
	@grep -Fq 'is_ref_struct' Compiler/include/semantic.h && grep -Fq 'field->is_ref' Compiler/src/compiler.c && grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/RefStructFoundation/bin/RefStructFoundation 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [0-9]+, live [1-9][0-9]*'; echo True
	@./$(BIN) check Tests/RefReadonlyScopedEscapeCompletion >/dev/null && echo True
	@./$(BIN) check Tests/RefReturningPropertiesIndexersCompletion >/dev/null && echo True
	@./$(BIN) check Tests/RefReturnMethodCompletion >/dev/null && echo True
	@./$(BIN) check Tests/RefAssignmentAliasingCompletion >/dev/null && echo True
	@./$(BIN) check Tests/ByReferenceValueRefLocalFoundation >/dev/null && echo True

# Milestone 227: Ref Struct Escape & Scoped Value Completion
test-ref-struct-escape-completion: $(BIN)
	@./$(BIN) check Tests/RefStructEscapeCompletion >/dev/null && echo True
	@./$(BIN) build Tests/RefStructEscapeCompletion >/dev/null
	@./Tests/RefStructEscapeCompletion/bin/RefStructEscapeCompletion
	@set -e; ./$(BIN) publish Tests/RefStructEscapeCompletion >/dev/null; test -x Tests/RefStructEscapeCompletion/publish/RefStructEscapeCompletion$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructEscapeDiagnostics/ReturnLocalCapture >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return ref struct value that refers to scoped or local storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructEscapeDiagnostics/ReturnScopedParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return ref struct value that refers to scoped or local storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructEscapeDiagnostics/ReturnScopedLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return ref struct value that refers to scoped or local storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructEscapeDiagnostics/OuterAssignmentInnerCapture >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign ref struct value with shorter lifetime to 'outer'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructEscapeDiagnostics/OuterAssignmentScopedInner >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign ref struct value with shorter lifetime to 'outer'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructEscapeDiagnostics/ReturnCallLocalRef >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return ref struct value that refers to scoped or local storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructEscapeDiagnostics/ReturnIdentityLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return ref struct value that refers to scoped or local storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructEscapeDiagnostics/ReturnPropertyUnsafeReceiver >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return ref struct value that refers to scoped or local storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructEscapeDiagnostics/GetterLocalCapture >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return ref struct value that refers to scoped or local storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructEscapeDiagnostics/AutoPropertyClass >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "automatic ref struct property 'Value' requires instance storage in a ref struct" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructEscapeDiagnostics/AutoPropertyStatic >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "automatic ref struct property 'Value' requires instance storage in a ref struct" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructEscapeDiagnostics/ScopedClassParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "scoped parameter requires ref, in, or out" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RefStructEscapeDiagnostics/ScopedClassLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "scoped value local 'value' requires a ref struct type" $$out; rm -f $$out; echo True
	@./$(BIN) parse Tests/RefStructEscapeCompletion/Program.void | grep -Fq 'Method Identity : RefView227'; echo True
	@./$(BIN) parse Tests/RefStructEscapeCompletion/Program.void | grep -Fq 'Property View : RefView227'; echo True
	@./$(BIN) parse Tests/RefStructEscapeCompletion/Program.void | grep -Fq 'Parameter scoped value : RefView227'; echo True
	@./$(BIN) parse Tests/RefStructEscapeCompletion/Program.void | grep -Fq 'Local scoped scopedValue : RefView227'; echo True
	@grep -Fq 'static vc_s_0 vc_m_3(int32_t * vc_p_0, void *vc_ref_owner_0, int32_t vc_p_1)' Tests/RefStructEscapeCompletion/.void/RefStructEscapeCompletion.c; echo True
	@grep -Fq 'vc_this.vc_f_0.owner = vc_ref_owner_0' Tests/RefStructEscapeCompletion/.void/RefStructEscapeCompletion.c; echo True
	@grep -Fq 'vc_m_3(&(((vc_c_' Tests/RefStructEscapeCompletion/.void/RefStructEscapeCompletion.c && grep -Fq ', vc_p_0, 25)' Tests/RefStructEscapeCompletion/.void/RefStructEscapeCompletion.c; echo True
	@grep -Fq 'vc_gc_mark(vc_typed->vc_f_0.owner);' Tests/RefStructEscapeCompletion/.void/RefStructEscapeCompletion.c; echo True
	@grep -Fq 'ref_struct_escape_depth' Compiler/src/semantic.c && grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/RefStructEscapeCompletion/bin/RefStructEscapeCompletion 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [0-9]+, live [1-9][0-9]*'; echo True
	@./$(BIN) check Tests/RefStructFoundationDiagnostics/RefStructProperty >/dev/null && echo True
	@./$(BIN) check Tests/RefStructFoundationDiagnostics/RefStructReturn >/dev/null && echo True
	@./$(BIN) check Tests/RefStructFoundation >/dev/null && echo True
	@./$(BIN) check Tests/RefReadonlyScopedEscapeCompletion >/dev/null && echo True
	@./$(BIN) check Tests/RefReturningPropertiesIndexersCompletion >/dev/null && echo True
	@./$(BIN) check Tests/RefReturnMethodCompletion >/dev/null && echo True
	@./$(BIN) check Tests/RefAssignmentAliasingCompletion >/dev/null && echo True
	@./$(BIN) check Tests/ByReferenceValueRefLocalFoundation >/dev/null && echo True


test-span-foundation: $(BIN)
	@./$(BIN) check Tests/SpanFoundation >/dev/null && echo True
	@./$(BIN) build Tests/SpanFoundation >/dev/null
	@./Tests/SpanFoundation/bin/SpanFoundation
	@set -e; ./$(BIN) publish Tests/SpanFoundation >/dev/null; test -x Tests/SpanFoundation/publish/SpanFoundation$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanFoundationDiagnostics/ReadOnlyWrite >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign through a readonly receiver" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanFoundationDiagnostics/WritableRefFromReadOnly >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot create writable ref local from ref readonly member" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanFoundationDiagnostics/ClassField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct field 'Value' is allowed only as instance storage inside another ref struct" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanFoundationDiagnostics/StructField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct field 'Value' is allowed only as instance storage inside another ref struct" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanFoundationDiagnostics/StaticField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct field 'Value' is allowed only as instance storage inside another ref struct" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanFoundationDiagnostics/ArrayOfSpan >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type 'Span' is not supported as a local type" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanFoundationDiagnostics/GenericArgument >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct type argument 'Span<int>' is not supported by generic type 'Box'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanFoundationDiagnostics/Boxing >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign 'Span<int>' to local 'value' of type 'object'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanFoundationDiagnostics/LambdaCapture >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct local 'span' cannot be captured by a lambda" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanFoundationDiagnostics/AsyncLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct local 'span' cannot be used in async or iterator methods" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanFoundationDiagnostics/IteratorLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct local 'span' cannot be used in async or iterator methods" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanFoundationDiagnostics/WrongArrayType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "no matching constructor for 'Span<int>' was found" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/SpanFoundationRuntime/NegativeIndex >/dev/null; out=$$(mktemp); if ./Tests/SpanFoundationRuntime/NegativeIndex/bin/NegativeIndex >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled IndexOutOfRangeException: Span index out of range" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/SpanFoundationRuntime/UpperBound >/dev/null; out=$$(mktemp); if ./Tests/SpanFoundationRuntime/UpperBound/bin/UpperBound >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled IndexOutOfRangeException: Span index out of range" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/SpanFoundationRuntime/ReadOnlyUpperBound >/dev/null; out=$$(mktemp); if ./Tests/SpanFoundationRuntime/ReadOnlyUpperBound/bin/ReadOnlyUpperBound >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled IndexOutOfRangeException: ReadOnlySpan index out of range" $$out; rm -f $$out; echo True
	@./$(BIN) parse StandardLibrary/Void/Span.void | grep -Fq 'Struct Span<T> [public ref]'; echo True
	@./$(BIN) parse StandardLibrary/Void/Span.void | grep -Fq 'Method get_Item : ref T [public]'; echo True
	@./$(BIN) parse StandardLibrary/Void/Span.void | grep -Fq 'Struct ReadOnlySpan<T> [public readonly ref]'; echo True
	@./$(BIN) parse StandardLibrary/Void/Span.void | grep -Fq 'Method get_Item : ref readonly T [public]'; echo True
	@grep -Fq 'vc_runtime_fail("Span index out of range")' Tests/SpanFoundation/.void/SpanFoundation.c; echo True
	@grep -Fq 'vc_runtime_fail("ReadOnlySpan index out of range")' Tests/SpanFoundation/.void/SpanFoundation.c; echo True
	@grep -Eq 'vc_gc_mark\(\(void \*\)vc_typed->vc_f_0\);' Tests/SpanFoundation/.void/SpanFoundation.c; echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py && grep -Fq 'namespace Void;' StandardLibrary/Void/Span.void; echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/SpanFoundation/bin/SpanFoundation 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [0-9]+, live [1-9][0-9]*'; echo True
	@./$(BIN) check Tests/RefStructEscapeCompletion >/dev/null && echo True
	@./$(BIN) check Tests/RefStructFoundation >/dev/null && echo True
	@./$(BIN) check Tests/RefReadonlyScopedEscapeCompletion >/dev/null && echo True
	@./$(BIN) check Tests/RefReturningPropertiesIndexersCompletion >/dev/null && echo True
	@./$(BIN) check Tests/RefReturnMethodCompletion >/dev/null && echo True



test-span-conversion-slicing-stackalloc-completion: $(BIN)
	@./$(BIN) check Tests/SpanConversionSlicingStackallocCompletion >/dev/null && echo True
	@./$(BIN) build Tests/SpanConversionSlicingStackallocCompletion >/dev/null
	@./Tests/SpanConversionSlicingStackallocCompletion/bin/SpanConversionSlicingStackallocCompletion
	@set -e; ./$(BIN) publish Tests/SpanConversionSlicingStackallocCompletion >/dev/null; test -x Tests/SpanConversionSlicingStackallocCompletion/publish/SpanConversionSlicingStackallocCompletion$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanConversionSlicingStackallocDiagnostics/ReturnStackalloc >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return ref struct value that refers to scoped or local storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanConversionSlicingStackallocDiagnostics/ReturnStackallocReadOnly >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return ref struct value that refers to scoped or local storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanConversionSlicingStackallocDiagnostics/OuterStackAssignment >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign ref struct value with shorter lifetime to 'outer'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanConversionSlicingStackallocDiagnostics/OuterReadonlyConversion >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign ref struct value with shorter lifetime to 'outer'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanConversionSlicingStackallocDiagnostics/WrongStackallocElement >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "stackalloc element type 'short' does not match span element type 'int'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanConversionSlicingStackallocDiagnostics/ManagedStackallocElement >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "stackalloc does not support element type 'string'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanConversionSlicingStackallocDiagnostics/PointerStackallocSafe >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "pointer locals require an unsafe method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanConversionSlicingStackallocDiagnostics/ReturnDirectLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return ref struct value that refers to scoped or local storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanConversionSlicingStackallocDiagnostics/ReturnScopedRef >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return ref struct value that refers to scoped or local storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanConversionSlicingStackallocDiagnostics/ReturnScopedIn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return ref struct value that refers to scoped or local storage" $$out; rm -f $$out; echo True
	@./$(BIN) build Tests/SpanConversionSlicingStackallocRuntime/NegativeStackallocLength >/dev/null; out=$$(mktemp); if ./Tests/SpanConversionSlicingStackallocRuntime/NegativeStackallocLength/bin/NegativeStackallocLength >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "VOID runtime error: stackalloc count cannot be negative" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/SpanConversionSlicingStackallocRuntime/SpanSegmentBounds >/dev/null; out=$$(mktemp); if ./Tests/SpanConversionSlicingStackallocRuntime/SpanSegmentBounds/bin/SpanSegmentBounds >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled ArgumentOutOfRangeException: Span range out of range" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/SpanConversionSlicingStackallocRuntime/ReadOnlySegmentBounds >/dev/null; out=$$(mktemp); if ./Tests/SpanConversionSlicingStackallocRuntime/ReadOnlySegmentBounds/bin/ReadOnlySegmentBounds >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled ArgumentOutOfRangeException: ReadOnlySpan range out of range" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/SpanConversionSlicingStackallocRuntime/SliceNegativeStart >/dev/null; out=$$(mktemp); if ./Tests/SpanConversionSlicingStackallocRuntime/SliceNegativeStart/bin/SliceNegativeStart >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled ArgumentOutOfRangeException: Span range out of range" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/SpanConversionSlicingStackallocRuntime/SliceNegativeLength >/dev/null; out=$$(mktemp); if ./Tests/SpanConversionSlicingStackallocRuntime/SliceNegativeLength/bin/SliceNegativeLength >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled ArgumentOutOfRangeException: Span range out of range" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/SpanConversionSlicingStackallocRuntime/SliceUpperBound >/dev/null; out=$$(mktemp); if ./Tests/SpanConversionSlicingStackallocRuntime/SliceUpperBound/bin/SliceUpperBound >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled ArgumentOutOfRangeException: Span range out of range" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/SpanConversionSlicingStackallocRuntime/ReadOnlySliceUpperBound >/dev/null; out=$$(mktemp); if ./Tests/SpanConversionSlicingStackallocRuntime/ReadOnlySliceUpperBound/bin/ReadOnlySliceUpperBound >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled ArgumentOutOfRangeException: ReadOnlySpan range out of range" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/SpanConversionSlicingStackallocRuntime/DirectNegativeLength >/dev/null; out=$$(mktemp); if ./Tests/SpanConversionSlicingStackallocRuntime/DirectNegativeLength/bin/DirectNegativeLength >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled ArgumentOutOfRangeException: Span length cannot be negative" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/SpanConversionSlicingStackallocRuntime/ReadOnlyDirectNegativeLength >/dev/null; out=$$(mktemp); if ./Tests/SpanConversionSlicingStackallocRuntime/ReadOnlyDirectNegativeLength/bin/ReadOnlyDirectNegativeLength >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled ArgumentOutOfRangeException: ReadOnlySpan length cannot be negative" $$out; rm -f $$out; echo True
	@./$(BIN) parse StandardLibrary/Void/Span.void | grep -Fq 'Field _reference : ref T [private]'; echo True
	@./$(BIN) parse StandardLibrary/Void/Span.void | grep -Fq 'Field _reference : ref readonly T [private]'; echo True
	@test "$$(./$(BIN) parse StandardLibrary/Void/Span.void | grep -Fc 'Method Slice : Span<T> [public]')" -eq 2; echo True
	@test "$$(./$(BIN) parse StandardLibrary/Void/Span.void | grep -Fc 'Method Slice : ReadOnlySpan<T> [public]')" -eq 2; echo True
	@./$(BIN) parse StandardLibrary/Void/Span.void | grep -Fq 'Operator implicit : Span<T> [public static]'; echo True
	@test "$$(./$(BIN) parse StandardLibrary/Void/Span.void | grep -Fc 'Operator implicit : ReadOnlySpan<T> [public static]')" -eq 2; echo True
	@grep -Fq 'stackalloc count cannot be negative' Tests/SpanConversionSlicingStackallocCompletion/.void/SpanConversionSlicingStackallocCompletion.c; echo True
	@grep -Eq '\.owner = \(void \*\)\(\(vc_self->vc_f_0\)\.owner\)' Tests/SpanConversionSlicingStackallocCompletion/.void/SpanConversionSlicingStackallocCompletion.c; echo True
	@grep -Fq 'vc_gc_mark(vc_typed->vc_f_0.owner);' Tests/SpanConversionSlicingStackallocCompletion/.void/SpanConversionSlicingStackallocCompletion.c; echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True
	@./$(BIN) run Tests/StackAlloc >/dev/null && echo True
	@./$(BIN) check Tests/SpanFoundation >/dev/null && echo True

# Milestone 230: By-Reference Semantics & Span Foundations Integration Audit
test-by-reference-span-integration-audit: $(BIN)
	@./$(BIN) check Tests/ByReferenceSpanIntegrationAudit >/dev/null && echo True
	@./$(BIN) build Tests/ByReferenceSpanIntegrationAudit >/dev/null
	@./Tests/ByReferenceSpanIntegrationAudit/bin/ByReferenceSpanIntegrationAudit
	@set -e; ./$(BIN) publish Tests/ByReferenceSpanIntegrationAudit >/dev/null; test -x Tests/ByReferenceSpanIntegrationAudit/publish/ByReferenceSpanIntegrationAudit$(EXE_SUFFIX); echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/ByReferenceSpanIntegrationAudit/bin/ByReferenceSpanIntegrationAudit 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceSpanIntegrationAuditDiagnostics/ReturnStackSlice >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return ref struct value that refers to scoped or local storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceSpanIntegrationAuditDiagnostics/ScopedSpanReturn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return ref struct value that refers to scoped or local storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceSpanIntegrationAuditDiagnostics/ReadonlyRebind >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot create writable ref local from ref readonly member" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceSpanIntegrationAuditDiagnostics/ReadonlyWrite >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign through a readonly receiver" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceSpanIntegrationAuditDiagnostics/HeapNestedView >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct field 'View' is allowed only as instance storage inside another ref struct" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceSpanIntegrationAuditDiagnostics/AsyncSpanLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct local 'values' cannot be used in async or iterator methods" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceSpanIntegrationAuditDiagnostics/LambdaCaptureSpan >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct local 'values' cannot be captured by a lambda" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceSpanIntegrationAuditDiagnostics/ReturnLocalWrappedView >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot return ref struct value that refers to scoped or local storage" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ByReferenceSpanIntegrationAuditDiagnostics/NativeExportSpan >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "exported method 'Bad' has a return type that is not supported by the native library ABI" $$out; rm -f $$out; echo True
	@parse="$$(./$(BIN) parse Tests/ByReferenceSpanIntegrationAudit/Program.void)"; printf '%s' "$$parse" | grep -Fq 'Property Head : ref int [public]' && printf '%s' "$$parse" | grep -Fq 'Property Tail : ref readonly int [public]' && printf '%s' "$$parse" | grep -Fq 'Method get_Item : ref int [public]' && printf '%s' "$$parse" | grep -Fq 'Field _span : Span<int> [private]' && printf '%s' "$$parse" | grep -Fq 'Parameter scoped values : Span<int>'; echo True
	@grep -Fq 'typedef struct VcRefReturn' Tests/ByReferenceSpanIntegrationAudit/.void/ByReferenceSpanIntegrationAudit.c && grep -Fq 'vc_gc_mark(vc_typed->vc_f_0.owner);' Tests/ByReferenceSpanIntegrationAudit/.void/ByReferenceSpanIntegrationAudit.c && grep -Fq 'vc_ref_owner_0' Tests/ByReferenceSpanIntegrationAudit/.void/ByReferenceSpanIntegrationAudit.c && grep -Fq 'stackalloc count cannot be negative' Tests/ByReferenceSpanIntegrationAudit/.void/ByReferenceSpanIntegrationAudit.c; echo True
	@grep -Fq 'private ref T _reference;' StandardLibrary/Void/Span.void && grep -Fq 'private ref readonly T _reference;' StandardLibrary/Void/Span.void && grep -Fq 'return new Span<T>(ref first, length);' StandardLibrary/Void/Span.void && grep -Fq 'return new ReadOnlySpan<T>(in first, length);' StandardLibrary/Void/Span.void; echo True
	@test -f StandardLibrary/Void/Span.void && test -f StandardLibrary/Void/Threading/Tasks/Task.void && test -f StandardLibrary/Void/Collections/List.void && test ! -d StandardLibrary/Void.Collections; echo True
	@./$(BIN) check Tests/ByReferenceValueRefLocalFoundation >/dev/null && ./$(BIN) check Tests/RefAssignmentAliasingCompletion >/dev/null && ./$(BIN) check Tests/RefReturnMethodCompletion >/dev/null && ./$(BIN) check Tests/RefReturningPropertiesIndexersCompletion >/dev/null && ./$(BIN) check Tests/RefReadonlyScopedEscapeCompletion >/dev/null; echo True
	@./$(BIN) check Tests/RefStructFoundation >/dev/null && ./$(BIN) check Tests/RefStructEscapeCompletion >/dev/null && ./$(BIN) check Tests/SpanFoundation >/dev/null && ./$(BIN) check Tests/SpanConversionSlicingStackallocCompletion >/dev/null; echo True
	@./$(BIN) run Tests/StackAlloc >/dev/null && ./$(BIN) parse Tests/RefOutIn.void >/dev/null; echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True


# Milestone 231: Index & From-End Foundation
test-index-from-end-foundation: $(BIN)
	@./$(BIN) check Tests/IndexFromEndFoundation >/dev/null && echo True
	@./$(BIN) build Tests/IndexFromEndFoundation >/dev/null
	@./Tests/IndexFromEndFoundation/bin/IndexFromEndFoundation
	@set -e; ./$(BIN) publish Tests/IndexFromEndFoundation >/dev/null; test -x Tests/IndexFromEndFoundation/publish/IndexFromEndFoundation$(EXE_SUFFIX); echo True
	@./$(BIN) check Tests/IndexFromEndNoUsing >/dev/null && echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/IndexFromEndDiagnostics/BoolOperand >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "operator '^' requires an 'int' operand, got 'bool'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/IndexFromEndDiagnostics/StringOperand >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "operator '^' requires an 'int' operand, got 'string'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/IndexFromEndDiagnostics/FloatOperand >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "operator '^' requires an 'int' operand, got 'float'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/IndexFromEndDiagnostics/IndexOperand >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "operator '^' requires an 'int' operand, got 'Index'" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/IndexFromEndRuntime/NegativeValue >/dev/null; out=$$(mktemp); if ./Tests/IndexFromEndRuntime/NegativeValue/bin/NegativeValue >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled ArgumentOutOfRangeException: Index value cannot be negative" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) run Tests/IndexFromEndRuntime/NegativeLength | grep -Fxq -- "-2"; echo True
	@./$(BIN) parse Tests/IndexFromEndSyntax/PrefixCaret.void | grep -Fq 'Unary ^'; echo True
	@./$(BIN) parse Tests/IndexFromEndSyntax/BinaryCaret.void | grep -Fq 'Binary ^'; echo True
	@./$(BIN) parse StandardLibrary/Void/Index.void | grep -Fq 'Struct Index [public readonly]'; echo True
	@./$(BIN) parse StandardLibrary/Void/Index.void | grep -Fq 'Field _value : int [private readonly]'; echo True
	@./$(BIN) parse StandardLibrary/Void/Index.void | grep -Fq 'Field _fromEnd : bool [private readonly]'; echo True
	@parse="$$(./$(BIN) parse StandardLibrary/Void/Index.void)"; printf '%s' "$$parse" | grep -Fq 'Constructor Index : <none> [public]' && printf '%s' "$$parse" | grep -Fq 'Parameter value : int' && printf '%s' "$$parse" | grep -Fq 'Parameter fromEnd : bool'; echo True
	@./$(BIN) parse StandardLibrary/Void/Index.void | grep -Fq 'Property Value : int [public]'; echo True
	@./$(BIN) parse StandardLibrary/Void/Index.void | grep -Fq 'Property IsFromEnd : bool [public]'; echo True
	@./$(BIN) parse StandardLibrary/Void/Index.void | grep -Fq 'Method GetOffset : int [public]'; echo True
	@grep -Fq 'Index value cannot be negative' Tests/IndexFromEndFoundation/.void/IndexFromEndFoundation.c && grep -Eq 'vc_ctor_[0-9]+\([^;]*true\)' Tests/IndexFromEndFoundation/.void/IndexFromEndFoundation.c; echo True
	@grep -Fq 'case VC_TOKEN_CARET:' Compiler/src/parser.c && grep -Fq 'resolve_from_end_index_contract' Compiler/src/semantic.c && grep -Fq 'unary_kind == VC_TOKEN_CARET' Compiler/src/compiler.c; echo True
	@./$(BIN) check Tests/ByReferenceSpanIntegrationAudit >/dev/null && echo True
	@./$(BIN) check Tests/UnaryOperatorCompletion >/dev/null && echo True
	@test -f StandardLibrary/Void/Index.void && test -f StandardLibrary/Void/Span.void && test -f StandardLibrary/Void/Collections/List.void; echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True

# Milestone 232: Range Syntax & Value Foundation
test-range-syntax-value-foundation: $(BIN)
	@./$(BIN) check Tests/RangeSyntaxValueFoundation >/dev/null && echo True
	@./$(BIN) build Tests/RangeSyntaxValueFoundation >/dev/null
	@./Tests/RangeSyntaxValueFoundation/bin/RangeSyntaxValueFoundation
	@set -e; ./$(BIN) publish Tests/RangeSyntaxValueFoundation >/dev/null; test -x Tests/RangeSyntaxValueFoundation/publish/RangeSyntaxValueFoundation$(EXE_SUFFIX); echo True
	@./$(BIN) check Tests/RangeSyntaxNoUsing >/dev/null && echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RangeSyntaxDiagnostics/BoolStart >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "range start must be convertible to 'Index', got 'bool'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RangeSyntaxDiagnostics/StringEnd >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "range end must be convertible to 'Index', got 'string'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RangeSyntaxDiagnostics/Chained >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "range operator '..' cannot be chained" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/RangeSyntaxDiagnostics/IndexOfIndex >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "operator '^' requires an 'int' operand, got 'Index'" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/RangeSyntaxRuntime/NegativeLength >/dev/null; out=$$(mktemp); if ./Tests/RangeSyntaxRuntime/NegativeLength/bin/NegativeLength >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled ArgumentOutOfRangeException: Range length cannot be negative" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/RangeSyntaxRuntime/Reverse >/dev/null; out=$$(mktemp); if ./Tests/RangeSyntaxRuntime/Reverse/bin/Reverse >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled ArgumentOutOfRangeException: Range out of range" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/RangeSyntaxRuntime/StartOutOfRange >/dev/null; out=$$(mktemp); if ./Tests/RangeSyntaxRuntime/StartOutOfRange/bin/StartOutOfRange >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled ArgumentOutOfRangeException: Range out of range" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/RangeSyntaxRuntime/EndOutOfRange >/dev/null; out=$$(mktemp); if ./Tests/RangeSyntaxRuntime/EndOutOfRange/bin/EndOutOfRange >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled ArgumentOutOfRangeException: Range out of range" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/RangeSyntaxRuntime/FromEndOutOfRange >/dev/null; out=$$(mktemp); if ./Tests/RangeSyntaxRuntime/FromEndOutOfRange/bin/FromEndOutOfRange >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled ArgumentOutOfRangeException: Range out of range" $$out; rm -f $$out; echo True
	@lex="$$(./$(BIN) lex Tests/RangeSyntaxValueSyntax/Forms.void)"; printf '%s' "$$lex" | grep -Fq '..' && printf '%s' "$$lex" | grep -Fq '1.25f'; echo True
	@parse="$$(./$(BIN) parse Tests/RangeSyntaxValueSyntax/Forms.void)"; printf '%s' "$$parse" | grep -Fq 'Range' && printf '%s' "$$parse" | grep -Fq 'Unary ^'; echo True
	@./$(BIN) parse StandardLibrary/Void/Range.void | grep -Fq 'Struct Range [public readonly]'; echo True
	@parse="$$(./$(BIN) parse StandardLibrary/Void/Range.void)"; printf '%s' "$$parse" | grep -Fq 'Field _start : Index [private readonly]' && printf '%s' "$$parse" | grep -Fq 'Field _end : Index [private readonly]'; echo True
	@parse="$$(./$(BIN) parse StandardLibrary/Void/Range.void)"; printf '%s' "$$parse" | grep -Fq 'Constructor Range : <none> [public]' && printf '%s' "$$parse" | grep -Fq 'Parameter start : Index' && printf '%s' "$$parse" | grep -Fq 'Parameter end : Index'; echo True
	@parse="$$(./$(BIN) parse StandardLibrary/Void/Range.void)"; printf '%s' "$$parse" | grep -Fq 'Property Start : Index [public]' && printf '%s' "$$parse" | grep -Fq 'Property End : Index [public]'; echo True
	@parse="$$(./$(BIN) parse StandardLibrary/Void/Range.void)"; printf '%s' "$$parse" | grep -Fq 'Method GetOffsetAndLength : void [public]' && printf '%s' "$$parse" | grep -Fq 'Parameter out offset : int' && printf '%s' "$$parse" | grep -Fq 'Parameter out rangeLength : int'; echo True
	@./$(BIN) parse StandardLibrary/Void/Index.void | grep -Fq 'Operator implicit : Index [public static]'; echo True
	@grep -Fq 'Range length cannot be negative' Tests/RangeSyntaxValueFoundation/.void/RangeSyntaxValueFoundation.c && grep -Fq 'Range out of range' Tests/RangeSyntaxValueFoundation/.void/RangeSyntaxValueFoundation.c; echo True
	@grep -Fq 'VC_TOKEN_DOT_DOT' Compiler/include/lexer.h && grep -Fq 'VC_AST_RANGE_EXPRESSION' Compiler/include/ast.h && grep -Fq 'parse_range' Compiler/src/parser.c && grep -Fq 'resolve_range_contract' Compiler/src/semantic.c && grep -Fq 'case VC_AST_RANGE_EXPRESSION:' Compiler/src/compiler.c; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory test-index-from-end-foundation >$$out; test $$(grep -c '^True$$' $$out) -eq 63; rm -f $$out; echo True
	@./$(BIN) check Tests/ByReferenceSpanIntegrationAudit >/dev/null && echo True
	@test -f StandardLibrary/Void/Index.void && test -f StandardLibrary/Void/Range.void && test -f StandardLibrary/Void/Span.void && test -f StandardLibrary/Void/Collections/List.void; echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True



.PHONY: test-general-index-range-consumer-semantics

# Milestone 233: General Index/Range Consumer Semantics
test-general-index-range-consumer-semantics: $(BIN)
	@./$(BIN) check Tests/GeneralIndexRangeConsumerSemantics >/dev/null && echo True
	@./$(BIN) build Tests/GeneralIndexRangeConsumerSemantics >/dev/null
	@./Tests/GeneralIndexRangeConsumerSemantics/bin/GeneralIndexRangeConsumerSemantics
	@set -e; ./$(BIN) publish Tests/GeneralIndexRangeConsumerSemantics >/dev/null; test -x Tests/GeneralIndexRangeConsumerSemantics/publish/GeneralIndexRangeConsumerSemantics$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GeneralIndexRangeConsumerSemanticsDiagnostics/MissingLength >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "must provide readable int Length to consume 'Index'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GeneralIndexRangeConsumerSemanticsDiagnostics/WrongLength >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "must provide readable int Length to consume 'Index'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GeneralIndexRangeConsumerSemanticsDiagnostics/MissingIntegerIndexer >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "must provide an unambiguous integer index getter to consume 'Index'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GeneralIndexRangeConsumerSemanticsDiagnostics/MissingIntegerSetter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "does not define a matching index setter" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GeneralIndexRangeConsumerSemanticsDiagnostics/MissingSlice >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "must provide an unambiguous Slice(int, int) method to consume 'Range'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GeneralIndexRangeConsumerSemanticsDiagnostics/InaccessibleLength >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "must provide readable int Length to consume 'Index'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GeneralIndexRangeConsumerSemanticsDiagnostics/InvalidSlice >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Range consumer member for type 'C' is inaccessible or invalid" $$out; rm -f $$out; echo True
	@grep -Fq 'vc_consumer_offset_' Tests/GeneralIndexRangeConsumerSemantics/.void/GeneralIndexRangeConsumerSemantics.c && grep -Fq 'vc_receiver_' Tests/GeneralIndexRangeConsumerSemantics/.void/GeneralIndexRangeConsumerSemantics.c; echo True
	@grep -Fq 'resolve_index_range_consumer' Compiler/src/semantic.c && grep -Fq 'emit_index_range_consumer_prefix' Compiler/src/compiler.c && grep -Fq 'consumer_length_property_index' Compiler/include/semantic.h; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory test-range-syntax-value-foundation >$$out; test $$(grep -c '^True$$' $$out) -eq 73; rm -f $$out; echo True
	@./$(BIN) check Tests/ByReferenceSpanIntegrationAudit >/dev/null && echo True
	@./$(BIN) check Tests/RefReturningPropertiesIndexersCompletion >/dev/null && echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True

.PHONY: test-arrays-span-index-range-completion

# Milestone 234: Arrays & Span Index/Range Completion
test-arrays-span-index-range-completion: $(BIN)
	@./$(BIN) check Tests/ArraysSpanIndexRangeCompletion >/dev/null && echo True
	@./$(BIN) build Tests/ArraysSpanIndexRangeCompletion >/dev/null
	@./Tests/ArraysSpanIndexRangeCompletion/bin/ArraysSpanIndexRangeCompletion
	@set -e; ./$(BIN) publish Tests/ArraysSpanIndexRangeCompletion >/dev/null; test -x Tests/ArraysSpanIndexRangeCompletion/publish/ArraysSpanIndexRangeCompletion$(EXE_SUFFIX); echo True
	@./$(BIN) check Tests/ArraysSpanIndexRangeNoUsing >/dev/null && echo True
	@./$(BIN) build Tests/ArraysSpanIndexRangeNoUsing >/dev/null
	@./Tests/ArraysSpanIndexRangeNoUsing/bin/ArraysSpanIndexRangeNoUsing
	@out=$$(mktemp); if ./$(BIN) check Tests/ArraysSpanIndexRangeDiagnostics/MultiDimensional >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "array Index/Range consumption requires a one-dimensional array" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ArraysSpanIndexRangeDiagnostics/ArrayRangeAssignment >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "range consumption result is not an assignment target" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ArraysSpanIndexRangeDiagnostics/ReadOnlySpanWrite >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign through a readonly receiver" $$out; rm -f $$out; echo True
	@./$(BIN) build Tests/ArraysSpanIndexRangeRuntime/ArrayIndexEnd >/dev/null; out=$$(mktemp); if ./Tests/ArraysSpanIndexRangeRuntime/ArrayIndexEnd/bin/ArrayIndexEnd >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "VOID runtime error: array index out of range" $$out; rm -f $$out; echo True
	@./$(BIN) build Tests/ArraysSpanIndexRangeRuntime/ArrayIndexPastStart >/dev/null; out=$$(mktemp); if ./Tests/ArraysSpanIndexRangeRuntime/ArrayIndexPastStart/bin/ArrayIndexPastStart >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "VOID runtime error: array index out of range" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/ArraysSpanIndexRangeRuntime/ArrayRangeReverse >/dev/null; out=$$(mktemp); if ./Tests/ArraysSpanIndexRangeRuntime/ArrayRangeReverse/bin/ArrayRangeReverse >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled ArgumentOutOfRangeException: Range out of range" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/ArraysSpanIndexRangeRuntime/SpanIndexEnd >/dev/null; out=$$(mktemp); if ./Tests/ArraysSpanIndexRangeRuntime/SpanIndexEnd/bin/SpanIndexEnd >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled IndexOutOfRangeException: Span index out of range" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/ArraysSpanIndexRangeRuntime/SpanRangeReverse >/dev/null; out=$$(mktemp); if ./Tests/ArraysSpanIndexRangeRuntime/SpanRangeReverse/bin/SpanRangeReverse >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled ArgumentOutOfRangeException: Range out of range" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/ArraysSpanIndexRangeRuntime/ReadOnlySpanIndexEnd >/dev/null; out=$$(mktemp); if ./Tests/ArraysSpanIndexRangeRuntime/ReadOnlySpanIndexEnd/bin/ReadOnlySpanIndexEnd >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "Unhandled IndexOutOfRangeException: ReadOnlySpan index out of range" $$out; rm -f $$out; echo True
	@grep -Fq 'vc_array_slice(' Tests/ArraysSpanIndexRangeCompletion/.void/ArraysSpanIndexRangeCompletion.c && grep -Fq 'vc_consumer_offset_' Tests/ArraysSpanIndexRangeCompletion/.void/ArraysSpanIndexRangeCompletion.c; echo True
	@grep -Fq 'consumer_array' Compiler/include/semantic.h && grep -Fq 'binding->consumer_array' Compiler/src/compiler.c && grep -Fq 'ref_struct_value_escape_depth' Compiler/src/semantic.c; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory test-general-index-range-consumer-semantics >$$out; test $$(grep -c '^True$$' $$out) -eq 55; rm -f $$out; echo True
	@./$(BIN) check Tests/ByReferenceSpanIntegrationAudit >/dev/null && echo True
	@./$(BIN) check Tests/SpanConversionSlicingStackallocCompletion >/dev/null && echo True
	@./$(BIN) check Tests/NullConditionalArrays >/dev/null && echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True


.PHONY: test-managed-array-backed-memory-foundation

# Milestone 235: Managed Array-Backed Memory<T> Foundation
test-managed-array-backed-memory-foundation: $(BIN)
	@./$(BIN) check Tests/ManagedArrayBackedMemoryFoundation >/dev/null && echo True
	@./$(BIN) build Tests/ManagedArrayBackedMemoryFoundation >/dev/null
	@./Tests/ManagedArrayBackedMemoryFoundation/bin/ManagedArrayBackedMemoryFoundation
	@set -e; ./$(BIN) publish Tests/ManagedArrayBackedMemoryFoundation >/dev/null; test -x Tests/ManagedArrayBackedMemoryFoundation/publish/ManagedArrayBackedMemoryFoundation$(EXE_SUFFIX); echo True
	@parse="$$(./$(BIN) parse StandardLibrary/Void/Memory.void)"; printf '%s' "$$parse" | grep -Fq 'Struct Memory<T> [public readonly]' && printf '%s' "$$parse" | grep -Fq 'Struct ReadOnlyMemory<T> [public readonly]'; echo True
	@parse="$$(./$(BIN) parse StandardLibrary/Void/Memory.void)"; test "$$(printf '%s' "$$parse" | grep -Fc 'Field _array : T[] [private readonly]')" -eq 2 && test "$$(printf '%s' "$$parse" | grep -Fc 'Field _start : int [private readonly]')" -eq 2 && test "$$(printf '%s' "$$parse" | grep -Fc 'Field _length : int [private readonly]')" -eq 2; echo True
	@parse="$$(./$(BIN) parse StandardLibrary/Void/Memory.void)"; test "$$(printf '%s' "$$parse" | grep -Fc 'Parameter array : T[]')" -eq 4; echo True
	@parse="$$(./$(BIN) parse StandardLibrary/Void/Memory.void)"; test "$$(printf '%s' "$$parse" | grep -Fc 'Parameter start : int')" -eq 2 && test "$$(printf '%s' "$$parse" | grep -Fc 'Parameter length : int')" -eq 2; echo True
	@set -e; ./$(BIN) build Tests/ManagedArrayBackedMemoryFoundationRuntime/NegativeStart >/dev/null; out=$$(mktemp); if ./Tests/ManagedArrayBackedMemoryFoundationRuntime/NegativeStart/bin/NegativeStart >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: Memory range out of range' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/ManagedArrayBackedMemoryFoundationRuntime/NegativeLength >/dev/null; out=$$(mktemp); if ./Tests/ManagedArrayBackedMemoryFoundationRuntime/NegativeLength/bin/NegativeLength >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: Memory range out of range' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/ManagedArrayBackedMemoryFoundationRuntime/PastEnd >/dev/null; out=$$(mktemp); if ./Tests/ManagedArrayBackedMemoryFoundationRuntime/PastEnd/bin/PastEnd >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: Memory range out of range' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/ManagedArrayBackedMemoryFoundationRuntime/NullNonEmpty >/dev/null; out=$$(mktemp); if ./Tests/ManagedArrayBackedMemoryFoundationRuntime/NullNonEmpty/bin/NullNonEmpty >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: Memory range out of range' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/ManagedArrayBackedMemoryFoundationRuntime/ReadOnlyPastEnd >/dev/null; out=$$(mktemp); if ./Tests/ManagedArrayBackedMemoryFoundationRuntime/ReadOnlyPastEnd/bin/ReadOnlyPastEnd >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: ReadOnlyMemory range out of range' $$out; rm -f $$out; echo True
	@grep -Fq 'private readonly T[] _array;' StandardLibrary/Void/Memory.void && grep -Fq 'private readonly int _start;' StandardLibrary/Void/Memory.void && grep -Fq 'private readonly int _length;' StandardLibrary/Void/Memory.void; echo True
	@! grep -R -Fq 'Memory<' Compiler Runtime; echo True
	@grep -Fq 'Void.Memory__g1_Node' Tests/ManagedArrayBackedMemoryFoundation/.void/ManagedArrayBackedMemoryFoundation.c && grep -Fq 'Void.ReadOnlyMemory__g1_int' Tests/ManagedArrayBackedMemoryFoundation/.void/ManagedArrayBackedMemoryFoundation.c; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory test-arrays-span-index-range-completion >$$out; test $$(grep -c '^True$$' $$out) -eq 56; rm -f $$out; echo True
	@./$(BIN) check Tests/ReadonlyStructs >/dev/null && echo True
	@./$(BIN) check Tests/GC >/dev/null && echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True


.PHONY: test-memory-slicing-conversion-span-bridge

# Milestone 236: Memory Slicing, Conversion & Span Bridge
test-memory-slicing-conversion-span-bridge: $(BIN)
	@./$(BIN) check Tests/MemorySlicingConversionSpanBridge >/dev/null && echo True
	@./$(BIN) build Tests/MemorySlicingConversionSpanBridge >/dev/null
	@./Tests/MemorySlicingConversionSpanBridge/bin/MemorySlicingConversionSpanBridge
	@set -e; ./$(BIN) publish Tests/MemorySlicingConversionSpanBridge >/dev/null; test -x Tests/MemorySlicingConversionSpanBridge/publish/MemorySlicingConversionSpanBridge$(EXE_SUFFIX); echo True
	@parse="$$(./$(BIN) parse StandardLibrary/Void/Memory.void)"; test "$$(printf '%s' "$$parse" | grep -Fc 'Property Length : int [public]')" -eq 2 && test "$$(printf '%s' "$$parse" | grep -Fc 'Property IsEmpty : bool [public]')" -eq 2 && printf '%s' "$$parse" | grep -Fq 'Property Span : Span<T> [public]' && printf '%s' "$$parse" | grep -Fq 'Property Span : ReadOnlySpan<T> [public]'; echo True
	@parse="$$(./$(BIN) parse StandardLibrary/Void/Memory.void)"; test "$$(printf '%s' "$$parse" | grep -Fc 'Method Slice(start:int) -> Memory<T> [public]')" -eq 1 && test "$$(printf '%s' "$$parse" | grep -Fc 'Method Slice(start:int, length:int) -> Memory<T> [public]')" -eq 1 && test "$$(printf '%s' "$$parse" | grep -Fc 'Method Slice(start:int) -> ReadOnlyMemory<T> [public]')" -eq 1 && test "$$(printf '%s' "$$parse" | grep -Fc 'Method Slice(start:int, length:int) -> ReadOnlyMemory<T> [public]')" -eq 1; echo True
	@grep -Fq 'vc_consumer_offset_' Tests/MemorySlicingConversionSpanBridge/.void/MemorySlicingConversionSpanBridge.c && grep -Fq 'vc_receiver_' Tests/MemorySlicingConversionSpanBridge/.void/MemorySlicingConversionSpanBridge.c; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/MemorySlicingConversionSpanBridgeDiagnostics/ImplicitMemoryToSpan >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign 'Memory<int>' to local 's' of type 'Span<int>'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/MemorySlicingConversionSpanBridgeDiagnostics/ImplicitReadOnlyMemoryToSpan >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign 'ReadOnlyMemory<int>' to local 's' of type 'ReadOnlySpan<int>'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/MemorySlicingConversionSpanBridgeDiagnostics/ReadOnlySpanWrite >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'cannot assign through a readonly receiver' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/MemorySlicingConversionSpanBridgeDiagnostics/ReadOnlyToWritableMemory >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign 'ReadOnlyMemory<int>' to local 'm' of type 'Memory<int>'" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/MemorySlicingConversionSpanBridgeRuntime/MemorySliceNegativeStart >/dev/null; out=$$(mktemp); if ./Tests/MemorySlicingConversionSpanBridgeRuntime/MemorySliceNegativeStart/bin/MemorySliceNegativeStart >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: Memory range out of range' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/MemorySlicingConversionSpanBridgeRuntime/MemorySliceNegativeLength >/dev/null; out=$$(mktemp); if ./Tests/MemorySlicingConversionSpanBridgeRuntime/MemorySliceNegativeLength/bin/MemorySliceNegativeLength >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: Memory range out of range' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/MemorySlicingConversionSpanBridgeRuntime/MemorySlicePastEnd >/dev/null; out=$$(mktemp); if ./Tests/MemorySlicingConversionSpanBridgeRuntime/MemorySlicePastEnd/bin/MemorySlicePastEnd >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: Memory range out of range' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/MemorySlicingConversionSpanBridgeRuntime/ReadOnlyMemorySlicePastEnd >/dev/null; out=$$(mktemp); if ./Tests/MemorySlicingConversionSpanBridgeRuntime/ReadOnlyMemorySlicePastEnd/bin/ReadOnlyMemorySlicePastEnd >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: ReadOnlyMemory range out of range' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/MemorySlicingConversionSpanBridgeRuntime/MemoryRangeReverse >/dev/null; out=$$(mktemp); if ./Tests/MemorySlicingConversionSpanBridgeRuntime/MemoryRangeReverse/bin/MemoryRangeReverse >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: Range out of range' $$out; rm -f $$out; echo True
	@grep -Fq 'public static implicit operator Memory<T>(T[] array)' StandardLibrary/Void/Memory.void && grep -Fq 'public static implicit operator ReadOnlyMemory<T>(Memory<T> memory)' StandardLibrary/Void/Memory.void && grep -Fq 'public static implicit operator ReadOnlyMemory<T>(T[] array)' StandardLibrary/Void/Memory.void; echo True
	@! grep -Eq 'implicit operator (Span|ReadOnlySpan)<T>\(Memory|ReadOnlyMemory' StandardLibrary/Void/Memory.void; echo True
	@grep -Fq 'emit_value_receiver_pointer' Compiler/src/compiler.c && grep -Fq '!receiver_is_plain_storage(context, target_node)' Compiler/src/compiler.c; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory test-managed-array-backed-memory-foundation >$$out; test $$(grep -c '^True$$' $$out) -eq 30; rm -f $$out; echo True
	@./$(BIN) check Tests/ArraysSpanIndexRangeCompletion >/dev/null && echo True
	@./$(BIN) check Tests/SpanConversionSlicingStackallocCompletion >/dev/null && echo True
	@./$(BIN) check Tests/GC >/dev/null && echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True

.PHONY: test-memory-suspension-gc-integration

# Milestone 237: Memory Suspension & GC Integration
test-memory-suspension-gc-integration: $(BIN)
	@./$(BIN) check Tests/MemorySuspensionGcIntegration >/dev/null && echo True
	@./$(BIN) build Tests/MemorySuspensionGcIntegration >/dev/null
	@./Tests/MemorySuspensionGcIntegration/bin/MemorySuspensionGcIntegration
	@set -e; ./$(BIN) publish Tests/MemorySuspensionGcIntegration >/dev/null; test -x Tests/MemorySuspensionGcIntegration/publish/MemorySuspensionGcIntegration$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/MemorySuspensionGcIntegrationDiagnostics/AsyncSpanLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct local 'span' cannot be used in async or iterator methods" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/MemorySuspensionGcIntegrationDiagnostics/IteratorSpanLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct local 'span' cannot be used in async or iterator methods" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/MemorySuspensionGcIntegrationDiagnostics/AsyncIteratorSpanLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct local 'span' cannot be used in async or iterator methods" $$out; rm -f $$out; echo True
	@grep -Fq '__voidc$$async$$Program$$ReadAfter' Tests/MemorySuspensionGcIntegration/.void/MemorySuspensionGcIntegration.c && grep -Fq '__voidc$$iterator$$Program$$IterateMemory' Tests/MemorySuspensionGcIntegration/.void/MemorySuspensionGcIntegration.c && grep -Fq '__voidc$$async$$__voidc$$iterator$$Program$$IterateMemoryAsync' Tests/MemorySuspensionGcIntegration/.void/MemorySuspensionGcIntegration.c && grep -Fq 'Memory__g1_Node237' Tests/MemorySuspensionGcIntegration/.void/MemorySuspensionGcIntegration.c && grep -Fq 'Task__g1_Memory__g1_Node237' Tests/MemorySuspensionGcIntegration/.void/MemorySuspensionGcIntegration.c; echo True
	@! grep -R -Fq 'Memory<' Compiler Runtime; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory test-memory-slicing-conversion-span-bridge >$$out; test $$(grep -c '^True$$' $$out) -eq 56; rm -f $$out; echo True
	@./$(BIN) check Tests/AsyncGenericsClosuresGcLifetimeIntegration >/dev/null && echo True
	@./$(BIN) check Tests/IteratorCapturedLifetimeGcIntegration >/dev/null && echo True
	@./$(BIN) check Tests/AsyncLanguageAsynchronousIterationIntegrationAudit >/dev/null && echo True
	@./$(BIN) check Tests/ThreadPoolWorkerLifecycleGcCoordination >/dev/null && echo True
	@./$(BIN) check Tests/ThreadPoolTaskIntegrationAudit >/dev/null && echo True
	@./$(BIN) check Tests/TaskContinuationCompletionSourceFoundation >/dev/null && echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True

.PHONY: test-span-enumeration-ref-foreach

# Milestone 238: Span Enumeration & Ref Foreach
test-span-enumeration-ref-foreach: $(BIN)
	@./$(BIN) check Tests/SpanEnumerationRefForeach >/dev/null && echo True
	@./$(BIN) build Tests/SpanEnumerationRefForeach >/dev/null
	@./Tests/SpanEnumerationRefForeach/bin/SpanEnumerationRefForeach
	@set -e; ./$(BIN) publish Tests/SpanEnumerationRefForeach >/dev/null; test -x Tests/SpanEnumerationRefForeach/publish/SpanEnumerationRefForeach$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanEnumerationRefForeachDiagnostics/ValueCurrent >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'ref foreach requires enumerator Current to return by ref' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanEnumerationRefForeachDiagnostics/ReadOnlyWritable >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'writable ref foreach cannot bind to ref readonly Current' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanEnumerationRefForeachDiagnostics/ReadOnlyMutation >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'cannot assign through a readonly receiver' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanEnumerationRefForeachDiagnostics/Capture >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref foreach iteration variable 'x' cannot be captured by a lambda" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanEnumerationRefForeachDiagnostics/Escape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'cannot return a ref to storage whose lifetime ends with the method' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanEnumerationRefForeachDiagnostics/Async >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'ref foreach is not supported in async or iterator methods' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanEnumerationRefForeachDiagnostics/Iterator >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'ref foreach is not supported in async or iterator methods' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanEnumerationRefForeachDiagnostics/ArrayRef >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'ref foreach requires an enumerator Current property that returns by ref' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/SpanEnumerationRefForeachDiagnostics/TypeMismatch >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref foreach item of type 'int' requires exactly type 'long'" $$out; rm -f $$out; echo True
	@grep -Fq 'public SpanEnumerator<T> GetEnumerator()' StandardLibrary/Void/Span.void && grep -Fq 'public ReadOnlySpanEnumerator<T> GetEnumerator()' StandardLibrary/Void/Span.void && grep -Fq 'public ref T Current' StandardLibrary/Void/Span.void && grep -Fq 'public ref readonly T Current' StandardLibrary/Void/Span.void; echo True
	@! grep -R -Eq 'SpanEnumerator|ReadOnlySpanEnumerator' Compiler; echo True
	@grep -Eq 'VcRefReturn vc_fe_r_[0-9]+ = ' Tests/SpanEnumerationRefForeach/.void/SpanEnumerationRefForeach.c && grep -Eq 'const int32_t \*vc_fe_p_[0-9]+' Tests/SpanEnumerationRefForeach/.void/SpanEnumerationRefForeach.c; echo True
	@./$(BIN) check Tests/ForeachDisposalCompletion >/dev/null && echo True
	@./$(BIN) check Tests/RefReturningPropertiesIndexersCompletion >/dev/null && echo True
	@./$(BIN) check Tests/RefReadonlyScopedEscapeCompletion >/dev/null && echo True
	@./$(BIN) check Tests/SpanConversionSlicingStackallocCompletion >/dev/null && echo True
	@./$(BIN) check Tests/ByReferenceSpanIntegrationAudit >/dev/null && echo True
	@./$(BIN) check Tests/MemorySuspensionGcIntegration >/dev/null && echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True
.PHONY: test-contiguous-memory-operations-completion

# Milestone 239: Contiguous Memory Operations Completion
test-contiguous-memory-operations-completion: $(BIN)
	@./$(BIN) check Tests/ContiguousMemoryOperationsCompletion >/dev/null && echo True
	@./$(BIN) build Tests/ContiguousMemoryOperationsCompletion >/dev/null
	@./Tests/ContiguousMemoryOperationsCompletion/bin/ContiguousMemoryOperationsCompletion
	@set -e; ./$(BIN) publish Tests/ContiguousMemoryOperationsCompletion >/dev/null; test -x Tests/ContiguousMemoryOperationsCompletion/publish/ContiguousMemoryOperationsCompletion$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ContiguousMemoryOperationsDiagnostics/ReadOnlyFill >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type 'ReadOnlySpan<int>' has no matching instance method 'Fill'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ContiguousMemoryOperationsDiagnostics/ReadOnlyClear >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type 'ReadOnlySpan<int>' has no matching instance method 'Clear'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ContiguousMemoryOperationsDiagnostics/ReadOnlyDestination >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type 'Span<int>' has no matching instance method 'CopyTo'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ContiguousMemoryOperationsDiagnostics/TypeMismatch >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type 'Span<int>' has no matching instance method 'CopyTo'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ContiguousMemoryOperationsDiagnostics/MemoryReadOnlyDestination >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type 'Memory<int>' has no matching instance method 'CopyTo'" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/ContiguousMemoryOperationsRuntime/SpanShort >/dev/null; out=$$(mktemp); if ./Tests/ContiguousMemoryOperationsRuntime/SpanShort/bin/ContiguousMemoryOperationsRuntimeSpanShort >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentException: Destination is too short' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/ContiguousMemoryOperationsRuntime/ReadOnlySpanShort >/dev/null; out=$$(mktemp); if ./Tests/ContiguousMemoryOperationsRuntime/ReadOnlySpanShort/bin/ContiguousMemoryOperationsRuntimeReadOnlySpanShort >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentException: Destination is too short' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/ContiguousMemoryOperationsRuntime/MemoryShort >/dev/null; out=$$(mktemp); if ./Tests/ContiguousMemoryOperationsRuntime/MemoryShort/bin/ContiguousMemoryOperationsRuntimeMemoryShort >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentException: Destination is too short' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/ContiguousMemoryOperationsRuntime/ReadOnlyMemoryShort >/dev/null; out=$$(mktemp); if ./Tests/ContiguousMemoryOperationsRuntime/ReadOnlyMemoryShort/bin/ContiguousMemoryOperationsRuntimeReadOnlyMemoryShort >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentException: Destination is too short' $$out; rm -f $$out; echo True
	@grep -Fq 'public readonly void CopyTo(Span<T> destination)' StandardLibrary/Void/Span.void && grep -Fq 'public readonly bool TryCopyTo(Span<T> destination)' StandardLibrary/Void/Span.void && grep -Fq 'public void Fill(T value)' StandardLibrary/Void/Span.void && grep -Fq 'public void Clear()' StandardLibrary/Void/Span.void && grep -Fq 'public void CopyTo(Memory<T> destination)' StandardLibrary/Void/Memory.void; echo True
	@grep -Fq 'vc_span_copy' Tests/ContiguousMemoryOperationsCompletion/.void/ContiguousMemoryOperationsCompletion.c && grep -Fq 'memmove(destination, source, (size_t)element_count * element_size)' Tests/ContiguousMemoryOperationsCompletion/.void/ContiguousMemoryOperationsCompletion.c; echo True
	@! grep -Eq 'public .*unsafe .*CopyTo|public .*unsafe .*Fill|public .*unsafe .*Clear' StandardLibrary/Void/Span.void StandardLibrary/Void/Memory.void; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory test-span-enumeration-ref-foreach >$$out; test $$(grep -c '^True$$' $$out) -eq 47; rm -f $$out; echo True
	@./$(BIN) check Tests/MemorySlicingConversionSpanBridge >/dev/null && echo True
	@./$(BIN) check Tests/ByReferenceSpanIntegrationAudit >/dev/null && echo True
	@./$(BIN) check Tests/GC >/dev/null && echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True

.PHONY: test-index-range-contiguous-memory-integration-audit

# Milestone 240: Index, Range & Contiguous Memory Integration Audit
test-index-range-contiguous-memory-integration-audit: $(BIN)
	@./$(BIN) check Tests/IndexRangeContiguousMemoryIntegrationAudit >/dev/null && echo True
	@./$(BIN) build Tests/IndexRangeContiguousMemoryIntegrationAudit >/dev/null
	@./Tests/IndexRangeContiguousMemoryIntegrationAudit/bin/IndexRangeContiguousMemoryIntegrationAudit
	@set -e; ./$(BIN) publish Tests/IndexRangeContiguousMemoryIntegrationAudit >/dev/null; test -x Tests/IndexRangeContiguousMemoryIntegrationAudit/publish/IndexRangeContiguousMemoryIntegrationAudit$(EXE_SUFFIX); echo True
	@output="$$(VOID_GC_TRACE=1 ./Tests/IndexRangeContiguousMemoryIntegrationAudit/bin/IndexRangeContiguousMemoryIntegrationAudit 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/IndexRangeContiguousMemoryIntegrationAuditDiagnostics/ReadOnlyWrite >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'cannot assign through a readonly receiver' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/IndexRangeContiguousMemoryIntegrationAuditDiagnostics/ReadOnlyRefForeach >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'writable ref foreach cannot bind to ref readonly Current' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/IndexRangeContiguousMemoryIntegrationAuditDiagnostics/ImplicitMemoryToSpan >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign 'Memory<int>' to local 'span' of type 'Span<int>'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/IndexRangeContiguousMemoryIntegrationAuditDiagnostics/ReturnStackRange >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'cannot return ref struct value that refers to scoped or local storage' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/IndexRangeContiguousMemoryIntegrationAuditDiagnostics/AsyncSpanRangeLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct local 'span' cannot be used in async or iterator methods" $$out; rm -f $$out; echo True
	@parse="$$(./$(BIN) parse Tests/IndexRangeContiguousMemoryIntegrationAudit/Program.void)"; printf '%s' "$$parse" | grep -Fq 'Method get_Item : ref int [public]' && printf '%s' "$$parse" | grep -Fq 'Method Slice : Window240 [public]' && printf '%s' "$$parse" | grep -Fq 'Parameter memory : Memory<Node240>'; echo True
	@grep -Fq 'typedef struct VcRefReturn' Tests/IndexRangeContiguousMemoryIntegrationAudit/.void/IndexRangeContiguousMemoryIntegrationAudit.c && grep -Fq 'vc_span_copy' Tests/IndexRangeContiguousMemoryIntegrationAudit/.void/IndexRangeContiguousMemoryIntegrationAudit.c && grep -Fq 'memmove(destination, source, (size_t)element_count * element_size)' Tests/IndexRangeContiguousMemoryIntegrationAudit/.void/IndexRangeContiguousMemoryIntegrationAudit.c && grep -Fq 'vc_gc_mark' Tests/IndexRangeContiguousMemoryIntegrationAudit/.void/IndexRangeContiguousMemoryIntegrationAudit.c; echo True
	@! grep -R -Eq 'Memory__g|ReadOnlyMemory__g|SpanEnumerator|ReadOnlySpanEnumerator' Compiler; echo True
	@test $$(find StandardLibrary/Void -name Memory.void -type f | wc -l) -eq 1 && test $$(find StandardLibrary/Void -name Span.void -type f | wc -l) -eq 1 && test ! -d StandardLibrary/Void.Collections && test -f StandardLibrary/Void/Collections/List.void; echo True
	@./$(BIN) check Tests/IndexFromEndFoundation >/dev/null && echo True
	@./$(BIN) check Tests/RangeSyntaxValueFoundation >/dev/null && echo True
	@./$(BIN) check Tests/GeneralIndexRangeConsumerSemantics >/dev/null && echo True
	@./$(BIN) check Tests/ArraysSpanIndexRangeCompletion >/dev/null && echo True
	@./$(BIN) check Tests/ManagedArrayBackedMemoryFoundation >/dev/null && echo True
	@./$(BIN) check Tests/MemorySlicingConversionSpanBridge >/dev/null && echo True
	@./$(BIN) check Tests/MemorySuspensionGcIntegration >/dev/null && echo True
	@./$(BIN) check Tests/SpanEnumerationRefForeach >/dev/null && echo True
	@./$(BIN) check Tests/ContiguousMemoryOperationsCompletion >/dev/null && echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True



.PHONY: test-unmanaged-generic-constraint-foundation

# Milestone 241: Unmanaged Generic Constraint Foundation
test-unmanaged-generic-constraint-foundation: $(BIN)
	@./$(BIN) check Tests/UnmanagedGenericConstraintFoundation >/dev/null && echo True
	@./$(BIN) build Tests/UnmanagedGenericConstraintFoundation >/dev/null
	@./Tests/UnmanagedGenericConstraintFoundation/bin/UnmanagedGenericConstraintFoundation
	@set -e; ./$(BIN) publish Tests/UnmanagedGenericConstraintFoundation >/dev/null; test -x Tests/UnmanagedGenericConstraintFoundation/publish/UnmanagedGenericConstraintFoundation$(EXE_SUFFIX); echo True
	@parse="$$(./$(BIN) parse Tests/UnmanagedGenericConstraintFoundation/Program.void)"; printf '%s' "$$parse" | grep -Fq 'GenericConstraint T unmanaged' && printf '%s' "$$parse" | grep -Fq 'GenericConstraint T unmanaged IScore241'; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedGenericConstraintDiagnostics/ManagedReference >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type argument 'string' must be an unmanaged type for generic parameter 'T'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedGenericConstraintDiagnostics/ManagedField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type argument 'Bad241' must be an unmanaged type for generic parameter 'T'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedGenericConstraintDiagnostics/AutoPropertyManaged >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type argument 'Bad241' must be an unmanaged type for generic parameter 'T'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedGenericConstraintDiagnostics/ArrayField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type argument 'Bad241' must be an unmanaged type for generic parameter 'T'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedGenericConstraintDiagnostics/Nullable >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type argument 'int?' must be an unmanaged type for generic parameter 'T'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedGenericConstraintDiagnostics/MemoryValue >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "must be an unmanaged type for generic parameter 'T'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedGenericConstraintDiagnostics/GenericTypeArgument >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type argument 'Node241' must be an unmanaged type for generic parameter 'T'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedGenericConstraintDiagnostics/Ordering >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "'unmanaged' constraint for 'T' must appear first" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedGenericConstraintDiagnostics/CombineStruct >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot have both struct and unmanaged constraints" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedGenericConstraintDiagnostics/CombineNew >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "'new()' cannot be combined with the 'unmanaged' constraint" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedGenericConstraintDiagnostics/Duplicate >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "duplicate 'unmanaged' constraint for 'T'" $$out; rm -f $$out; echo True
	@grep -Fq 'bool vc_semantic_type_is_unmanaged(const VcSemanticModel *model, VcSemanticType type);' Compiler/include/semantic.h && grep -Fq 'semantic_type_is_unmanaged_recursive' Compiler/src/semantic.c; echo True
	@! grep -Rq 'VC_TOKEN_KW_UNMANAGED' Compiler; echo True
	@./$(BIN) check Tests/GenericConstraints >/dev/null && echo True
	@./$(BIN) check Tests/GenericMethodTypeInference >/dev/null && echo True
	@./$(BIN) check Tests/ConstrainedStaticInterfaceDispatch >/dev/null && echo True
	@./$(BIN) check Tests/NativeStructs >/dev/null && echo True
	@./$(BIN) check Tests/Pointers >/dev/null && echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True

.PHONY: test-generic-pointer-unmanaged-stack-storage-completion

# Milestone 242: Generic Pointer & Unmanaged Stack Storage Completion
test-generic-pointer-unmanaged-stack-storage-completion: $(BIN)
	@./$(BIN) check Tests/GenericPointerUnmanagedStackStorage >/dev/null && echo True
	@./$(BIN) build Tests/GenericPointerUnmanagedStackStorage >/dev/null
	@./Tests/GenericPointerUnmanagedStackStorage/bin/GenericPointerUnmanagedStackStorage
	@set -e; ./$(BIN) publish Tests/GenericPointerUnmanagedStackStorage >/dev/null; test -x Tests/GenericPointerUnmanagedStackStorage/publish/GenericPointerUnmanagedStackStorage$(EXE_SUFFIX); echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GenericPointerUnmanagedStackStorageDiagnostics/PointerLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "generic pointer local using 'T' requires 'where T : unmanaged'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GenericPointerUnmanagedStackStorageDiagnostics/PointerSignature >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "generic pointer return type using 'T' requires 'where T : unmanaged'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GenericPointerUnmanagedStackStorageDiagnostics/Sizeof >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "sizeof generic type using 'T' requires 'where T : unmanaged'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GenericPointerUnmanagedStackStorageDiagnostics/StackallocPointer >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "stackalloc generic element type using 'T' requires 'where T : unmanaged'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GenericPointerUnmanagedStackStorageDiagnostics/StackallocSpan >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "stackalloc generic element type using 'T' requires 'where T : unmanaged'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GenericPointerUnmanagedStackStorageDiagnostics/AddressOf >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "taking the address of generic storage using 'T' requires 'where T : unmanaged'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GenericPointerUnmanagedStackStorageDiagnostics/PointerField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "generic pointer field 'Pointer' using 'T' requires 'where T : unmanaged'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GenericPointerUnmanagedStackStorageDiagnostics/PointerCast >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "generic pointer type using 'T' requires 'where T : unmanaged'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GenericPointerUnmanagedStackStorageDiagnostics/DefaultPointer >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "generic pointer default using 'T' requires 'where T : unmanaged'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GenericPointerUnmanagedStackStorageDiagnostics/TypeofPointer >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "generic pointer typeof using 'T' requires 'where T : unmanaged'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GenericPointerUnmanagedStackStorageDiagnostics/SafePointer >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "pointer locals require an unsafe method" $$out; rm -f $$out; echo True
	@./$(BIN) build Tests/GenericPointerUnmanagedStackStorageRuntime/NegativeCount >/dev/null; out=$$(mktemp); if ./Tests/GenericPointerUnmanagedStackStorageRuntime/NegativeCount/bin/GenericPointerUnmanagedStackStorageNegativeCount >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "VOID runtime error: stackalloc count cannot be negative" $$out; rm -f $$out; echo True
	@grep -Fq 'stackalloc count cannot be negative' Tests/GenericPointerUnmanagedStackStorage/.void/GenericPointerUnmanagedStackStorage.c && grep -Fq 'sizeof(' Tests/GenericPointerUnmanagedStackStorage/.void/GenericPointerUnmanagedStackStorage.c; echo True
	@grep -Fq 'type_ref_has_generic_parameter_origin' Compiler/src/semantic.c && grep -Fq 'vc_semantic_type_is_unmanaged(context->model, element_type)' Compiler/src/semantic.c && ! grep -Rq 'Nested242\|Auto242\|PointerBox242' Compiler; echo True
	@./$(BIN) check Tests/UnmanagedGenericConstraintFoundation >/dev/null && echo True
	@./$(BIN) check Tests/GenericConstraints >/dev/null && echo True
	@./$(BIN) check Tests/GenericMethodTypeInference >/dev/null && echo True
	@./$(BIN) check Tests/Pointers >/dev/null && echo True
	@./$(BIN) check Tests/StackAlloc >/dev/null && echo True
	@./$(BIN) check Tests/SpanConversionSlicingStackallocCompletion >/dev/null && echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True

.PHONY: test-managed-pinning-runtime-foundation

# Milestone 243: Managed Pinning Runtime Foundation
test-managed-pinning-runtime-foundation: $(BIN)
	@./$(BIN) check Tests/ManagedPinningRuntimeFoundation/Library >/dev/null && echo True
	@./$(BIN) build Tests/ManagedPinningRuntimeFoundation/Library >/dev/null; test -f Tests/ManagedPinningRuntimeFoundation/Library/bin/libVoid243PinLib.a; echo True
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/ManagedPinningRuntimeFoundation/CConsumer/main.c Runtime/src/vc_thread.c Runtime/src/vc_atomic.c -o $(MANAGED_PINNING_RUNTIME_TEST)
	@./$(MANAGED_PINNING_RUNTIME_TEST)
	@./$(BIN) publish Tests/ManagedPinningRuntimeFoundation/Library >/dev/null; test -f Tests/ManagedPinningRuntimeFoundation/Library/publish/libVoid243PinLib.a; echo True
	@grep -Fq 'size_t pin_count;' Tests/ManagedPinningRuntimeFoundation/Library/.void/Void243PinLib.c && grep -Fq 'typedef struct VcGcPin' Tests/ManagedPinningRuntimeFoundation/Library/.void/Void243PinLib.c && grep -Fq 'bool active;' Tests/ManagedPinningRuntimeFoundation/Library/.void/Void243PinLib.c; echo True
	@grep -Fq 'vc_gc_pin_acquire(VcGcPin *pin, void *owner)' Tests/ManagedPinningRuntimeFoundation/Library/.void/Void243PinLib.c && grep -Fq 'vc_gc_pin_release(VcGcPin *pin)' Tests/ManagedPinningRuntimeFoundation/Library/.void/Void243PinLib.c && grep -Fq 'if (item->pin_count == SIZE_MAX) break;' Tests/ManagedPinningRuntimeFoundation/Library/.void/Void243PinLib.c; echo True
	@grep -Fq 'if (item->pin_count != 0u) vc_gc_mark(item->memory);' Tests/ManagedPinningRuntimeFoundation/Library/.void/Void243PinLib.c && grep -Fq 'if (item->trace != NULL) item->trace(item->memory);' Tests/ManagedPinningRuntimeFoundation/Library/.void/Void243PinLib.c; echo True
	@grep -Fq 'item->pin_count = 0u;' Tests/ManagedPinningRuntimeFoundation/Library/.void/Void243PinLib.c; echo True
	@test "$$(grep -Fc 'static VcGcAllocation *vc_gc_allocations = NULL;' Tests/ManagedPinningRuntimeFoundation/Library/.void/Void243PinLib.c)" -eq 1 && ! grep -Fq 'vc_gc_pinned_allocations' Tests/ManagedPinningRuntimeFoundation/Library/.void/Void243PinLib.c; echo True
	@! grep -RiqE '\b(fixed|pinning|gcpin)\b' StandardLibrary/Void; echo True
	@./$(BIN) check Tests/GC >/dev/null && echo True
	@$(MAKE) --no-print-directory -s test-cooperative-stop-the-world-gc-safepoints >/dev/null && echo True
	@$(MAKE) --no-print-directory -s test-thread-safe-allocation-runtime-registry >/dev/null && echo True
	@./$(BIN) check Tests/GenericPointerUnmanagedStackStorage >/dev/null && echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True

.PHONY: test-fixed-statement-scoped-pointer-semantics

# Milestone 244: fixed Statement & Scoped Pointer Semantics
test-fixed-statement-scoped-pointer-semantics: $(BIN)
	@./$(BIN) check Tests/FixedStatementScopedPointerSemantics >/dev/null && echo True
	@./$(BIN) build Tests/FixedStatementScopedPointerSemantics >/dev/null
	@./Tests/FixedStatementScopedPointerSemantics/bin/FixedStatementScopedPointerSemantics
	@set -e; ./$(BIN) publish Tests/FixedStatementScopedPointerSemantics >/dev/null; test -x Tests/FixedStatementScopedPointerSemantics/publish/FixedStatementScopedPointerSemantics$(EXE_SUFFIX); echo True
	@./$(BIN) check Tests/FixedStatementScopedPointerCleanup/Library >/dev/null && echo True
	@./$(BIN) build Tests/FixedStatementScopedPointerCleanup/Library >/dev/null; test -f Tests/FixedStatementScopedPointerCleanup/Library/bin/libVoid244FixedCleanup.a; echo True
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/FixedStatementScopedPointerCleanup/CConsumer/main.c Runtime/src/vc_thread.c Runtime/src/vc_atomic.c -o $(FIXED_STATEMENT_CLEANUP_TEST)
	@./$(FIXED_STATEMENT_CLEANUP_TEST)
	@./$(BIN) publish Tests/FixedStatementScopedPointerCleanup/Library >/dev/null; test -f Tests/FixedStatementScopedPointerCleanup/Library/publish/libVoid244FixedCleanup.a; echo True
	@grep -Fq 'VcGcPin vc_fixed_pin_' Tests/FixedStatementScopedPointerSemantics/.void/FixedStatementScopedPointerSemantics.c && grep -Fq 'vc_gc_pin_acquire(&vc_fixed_pin_' Tests/FixedStatementScopedPointerSemantics/.void/FixedStatementScopedPointerSemantics.c && grep -Fq 'vc_gc_pin_release(&vc_fixed_pin_' Tests/FixedStatementScopedPointerSemantics/.void/FixedStatementScopedPointerSemantics.c; echo True
	@grep -Fq 'VC_FINALLY_TRANSFER_PIN' Compiler/src/compiler.c && grep -Fq 'pointer_pin_escape_depth' Compiler/src/semantic.c && ! grep -RqE 'Box244|GenericBox244|FixedStatementScopedPointer' Compiler; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/FixedStatementScopedPointerDiagnostics/Async >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'fixed statements are not supported in async or iterator methods' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/FixedStatementScopedPointerDiagnostics/Capture >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "pointer local 'p' depends on a fixed pin and cannot be captured by a lambda" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/FixedStatementScopedPointerDiagnostics/FieldStoreEscape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'cannot store a pointer that depends on a fixed pin into longer-lived storage' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/FixedStatementScopedPointerDiagnostics/Iterator >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'fixed statements are not supported in async or iterator methods' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/FixedStatementScopedPointerDiagnostics/LocalAddress >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'fixed initializer must take the address of writable managed instance field storage' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/FixedStatementScopedPointerDiagnostics/NonPointer >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "fixed variable 'p' must have a pointer type" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/FixedStatementScopedPointerDiagnostics/OuterAssignmentEscape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign pointer whose fixed pin ends sooner to local 'escaped'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/FixedStatementScopedPointerDiagnostics/PointerMutation >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'cannot modify readonly receiver' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/FixedStatementScopedPointerDiagnostics/ReadonlyField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'cannot take the address of readonly storage' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/FixedStatementScopedPointerDiagnostics/ReturnEscape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'cannot return a pointer that depends on a fixed pin whose lifetime ends here' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/FixedStatementScopedPointerDiagnostics/SafeContext >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'fixed statement requires an unsafe method' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/FixedStatementScopedPointerDiagnostics/StaticField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'fixed initializer must take the address of writable managed instance field storage' $$out; rm -f $$out; echo True
	@./$(BIN) build Tests/FixedStatementScopedPointerRuntime/NullOwner >/dev/null; out=$$(mktemp); if ./Tests/FixedStatementScopedPointerRuntime/NullOwner/bin/FixedStatementScopedPointerRuntime >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: object reference is null' $$out; rm -f $$out; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-managed-pinning-runtime-foundation >$$out; test "$$(grep -c '^True$$' $$out)" -eq 43; rm -f $$out; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-generic-pointer-unmanaged-stack-storage-completion >$$out; test "$$(grep -c '^True$$' $$out)" -eq 55; rm -f $$out; echo True
	@./$(BIN) check Tests/RefReadonlyScopedEscapeCompletion >/dev/null && echo True
	@./$(BIN) check Tests/UnsafePointerProperties >/dev/null && echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True


.PHONY: test-structural-pinnable-reference-protocol

# Milestone 245: Structural Pinnable Reference Protocol
test-structural-pinnable-reference-protocol: $(BIN)
	@./$(BIN) check Tests/StructuralPinnableReferenceProtocol >/dev/null && echo True
	@./$(BIN) build Tests/StructuralPinnableReferenceProtocol >/dev/null
	@./Tests/StructuralPinnableReferenceProtocol/bin/StructuralPinnableReferenceProtocol
	@set -e; ./$(BIN) publish Tests/StructuralPinnableReferenceProtocol >/dev/null; test -x Tests/StructuralPinnableReferenceProtocol/publish/StructuralPinnableReferenceProtocol$(EXE_SUFFIX); echo True
	@grep -Fq 'vc_fixed_pinnable_ref_' Tests/StructuralPinnableReferenceProtocol/.void/StructuralPinnableReferenceProtocol.c && grep -Fq 'vc_gc_pin_acquire(&vc_fixed_pin_' Tests/StructuralPinnableReferenceProtocol/.void/StructuralPinnableReferenceProtocol.c && grep -Fq '.owner)) vc_runtime_fail("fixed pin acquisition failed")' Tests/StructuralPinnableReferenceProtocol/.void/StructuralPinnableReferenceProtocol.c; echo True
	@grep -Fq 'vc_vcall_' Tests/StructuralPinnableReferenceProtocol/.void/StructuralPinnableReferenceProtocol.c && grep -Fq 'vc_icall_' Tests/StructuralPinnableReferenceProtocol/.void/StructuralPinnableReferenceProtocol.c; echo True
	@grep -Fq 'fixed_pinnable_protocol' Compiler/include/semantic.h && grep -Fq 'GetPinnableReference' Compiler/src/semantic.c && ! grep -RqE 'PinBox245|RefPin245|ValuePin245|RelayPin245|StructuralPinnableReferenceProtocol' Compiler; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/StructuralPinnableReferenceProtocolDiagnostics/MissingMethod >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "must provide an unambiguous GetPinnableReference() method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/StructuralPinnableReferenceProtocolDiagnostics/ByValue >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'GetPinnableReference() must return by ref' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/StructuralPinnableReferenceProtocolDiagnostics/WrongElement >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "returns ref 'long' but fixed pointer element type is 'int'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/StructuralPinnableReferenceProtocolDiagnostics/ReadonlyReturn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'returns ref readonly storage and cannot initialize a writable fixed pointer' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/StructuralPinnableReferenceProtocolDiagnostics/Inaccessible >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'GetPinnableReference() is inaccessible' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/StructuralPinnableReferenceProtocolDiagnostics/ReadonlyReceiver >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'cannot call non-readonly GetPinnableReference() through a readonly value receiver' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/StructuralPinnableReferenceProtocolDiagnostics/OptionalParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'GetPinnableReference() must be parameterless' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/StructuralPinnableReferenceProtocolDiagnostics/ReturnEscape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'cannot return a pointer that depends on a fixed pin whose lifetime ends here' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/StructuralPinnableReferenceProtocolDiagnostics/Capture >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "pointer local 'p' depends on a fixed pin and cannot be captured by a lambda" $$out; rm -f $$out; echo True
	@./$(BIN) build Tests/StructuralPinnableReferenceProtocolRuntime/NullReceiver >/dev/null; out=$$(mktemp); if ./Tests/StructuralPinnableReferenceProtocolRuntime/NullReceiver/bin/StructuralPinnableReferenceProtocolRuntime >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: object reference is null' $$out; rm -f $$out; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-fixed-statement-scoped-pointer-semantics >$$out; test "$$(grep -c '^True$$' $$out)" -eq 51; rm -f $$out; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-managed-pinning-runtime-foundation >$$out; test "$$(grep -c '^True$$' $$out)" -eq 43; rm -f $$out; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-generic-pointer-unmanaged-stack-storage-completion >$$out; test "$$(grep -c '^True$$' $$out)" -eq 55; rm -f $$out; echo True
	@./$(BIN) check Tests/RefReadonlyScopedEscapeCompletion >/dev/null && echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True


.PHONY: test-array-span-memory-pinning-completion

# Milestone 246: Array, Span & Memory Pinning Completion
test-array-span-memory-pinning-completion: $(BIN)
	@./$(BIN) check Tests/ArraySpanMemoryPinningCompletion >/dev/null && echo True
	@./$(BIN) build Tests/ArraySpanMemoryPinningCompletion >/dev/null
	@./Tests/ArraySpanMemoryPinningCompletion/bin/ArraySpanMemoryPinningCompletion
	@set -e; ./$(BIN) publish Tests/ArraySpanMemoryPinningCompletion >/dev/null; test -x Tests/ArraySpanMemoryPinningCompletion/publish/ArraySpanMemoryPinningCompletion$(EXE_SUFFIX); echo True
	@grep -Fq 'vc_fixed_owner_' Tests/ArraySpanMemoryPinningCompletion/.void/ArraySpanMemoryPinningCompletion.c && grep -Fq -- '->data : NULL' Tests/ArraySpanMemoryPinningCompletion/.void/ArraySpanMemoryPinningCompletion.c && grep -Fq 'vc_gc_pin_acquire(&vc_fixed_pin_' Tests/ArraySpanMemoryPinningCompletion/.void/ArraySpanMemoryPinningCompletion.c; echo True
	@grep -Fq 'vc_array_pinnable_reference(' Tests/ArraySpanMemoryPinningCompletion/.void/ArraySpanMemoryPinningCompletion.c && grep -Fq 'vc_fixed_pinnable_ref_' Tests/ArraySpanMemoryPinningCompletion/.void/ArraySpanMemoryPinningCompletion.c; echo True
	@grep -Fq 'fixed_array' Compiler/include/semantic.h && grep -Fq 'fixed array initializer requires a one-dimensional array' Compiler/src/semantic.c && grep -Fq 'ArrayPinnableReference' StandardLibrary/Void/Span.void && grep -Fq 'GetPinnableReference' StandardLibrary/Void/Memory.void; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ArraySpanMemoryPinningCompletionDiagnostics/ReadOnlySpanWritable >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'returns ref readonly storage and cannot initialize a writable fixed pointer' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ArraySpanMemoryPinningCompletionDiagnostics/ReadOnlyMemoryWritable >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'returns ref readonly storage and cannot initialize a writable fixed pointer' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ArraySpanMemoryPinningCompletionDiagnostics/MultiDimensionalArray >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'fixed array initializer requires a one-dimensional array' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ArraySpanMemoryPinningCompletionDiagnostics/ArrayElementMismatch >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "fixed array element type 'int' does not match pointer element type 'long'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ArraySpanMemoryPinningCompletionDiagnostics/SpanElementMismatch >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "returns ref 'int' but fixed pointer element type is 'long'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ArraySpanMemoryPinningCompletionDiagnostics/MemoryElementMismatch >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "returns ref 'int' but fixed pointer element type is 'long'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ArraySpanMemoryPinningCompletionDiagnostics/ArrayReturnEscape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'cannot return a pointer that depends on a fixed pin whose lifetime ends here' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/ArraySpanMemoryPinningCompletionDiagnostics/SpanReturnEscape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'cannot return a pointer that depends on a fixed pin whose lifetime ends here' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/StructuralPinnableReferenceProtocol >/dev/null && echo True
	@./$(BIN) check Tests/MemorySlicingConversionSpanBridge >/dev/null && echo True
	@./$(BIN) check Tests/ArraysSpanIndexRangeCompletion >/dev/null && echo True
	@./$(BIN) check Tests/ContiguousMemoryOperationsCompletion >/dev/null && echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True


.PHONY: test-unmanaged-span-construction-completion

# Milestone 247: Unmanaged Span / ReadOnlySpan Construction
test-unmanaged-span-construction-completion: $(BIN)
	@./$(BIN) check Tests/UnmanagedSpanConstructionCompletion >/dev/null && echo True
	@./$(BIN) build Tests/UnmanagedSpanConstructionCompletion >/dev/null
	@./Tests/UnmanagedSpanConstructionCompletion/bin/UnmanagedSpanConstructionCompletion
	@set -e; ./$(BIN) publish Tests/UnmanagedSpanConstructionCompletion >/dev/null; test -x Tests/UnmanagedSpanConstructionCompletion/publish/UnmanagedSpanConstructionCompletion$(EXE_SUFFIX); echo True
	@parse="$$(./$(BIN) parse StandardLibrary/Void/Span.void)"; printf '%s' "$$parse" | grep -Fq 'Constructor Span' && printf '%s' "$$parse" | grep -Fq 'Parameter pointer : void*' && printf '%s' "$$parse" | grep -Fq 'Constructor ReadOnlySpan'; echo True
	@grep -Fq 'public unsafe Span(void* pointer, int length)' StandardLibrary/Void/Span.void && grep -Fq 'public unsafe ReadOnlySpan(void* pointer, int length)' StandardLibrary/Void/Span.void && grep -Fq 'where T : unmanaged' StandardLibrary/Void/Span.void; echo True
	@grep -Fq 'target_element == VC_SEM_TYPE_VOID' Compiler/src/semantic.c && grep -Fq '(explicit_context && source_element == VC_SEM_TYPE_VOID)' Compiler/src/semantic.c; echo True
	@grep -Fq 'pointer_pin_escape_depth(context, operand)' Compiler/src/semantic.c && grep -Fq 'VC_TOKEN_STAR' Compiler/src/semantic.c; echo True
	@! grep -RqE 'NativeSpan|NativeReadOnlySpan|PointerSpan' Compiler StandardLibrary; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedSpanConstructionDiagnostics/ManagedElement >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "must be an unmanaged type for generic parameter 'T'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedSpanConstructionDiagnostics/VoidPointerImplicitBack >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot assign 'void*' to local 'typed' of type 'int*'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedSpanConstructionDiagnostics/FixedEscape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'cannot return ref struct value that refers to scoped or local storage' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedSpanConstructionDiagnostics/ReadOnlyWrite >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'cannot assign through a readonly receiver' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedSpanConstructionDiagnostics/SafeConstructor >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "unsafe constructor 'Span<int>' requires an unsafe method" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/UnmanagedSpanConstructionRuntime/NegativeLength >/dev/null; out=$$(mktemp); if ./Tests/UnmanagedSpanConstructionRuntime/NegativeLength/bin/UnmanagedSpanConstructionRuntimeNegativeLength >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: Span length cannot be negative' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/UnmanagedSpanConstructionRuntime/NullNonEmpty >/dev/null; out=$$(mktemp); if ./Tests/UnmanagedSpanConstructionRuntime/NullNonEmpty/bin/UnmanagedSpanConstructionRuntimeNullNonEmpty >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentNullException: Span pointer cannot be null for a non-empty span' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/UnmanagedSpanConstructionRuntime/ReadOnlyNegativeLength >/dev/null; out=$$(mktemp); if ./Tests/UnmanagedSpanConstructionRuntime/ReadOnlyNegativeLength/bin/UnmanagedSpanConstructionRuntimeReadOnlyNegativeLength >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: ReadOnlySpan length cannot be negative' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/UnmanagedSpanConstructionRuntime/ReadOnlyNullNonEmpty >/dev/null; out=$$(mktemp); if ./Tests/UnmanagedSpanConstructionRuntime/ReadOnlyNullNonEmpty/bin/UnmanagedSpanConstructionRuntimeReadOnlyNullNonEmpty >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentNullException: ReadOnlySpan pointer cannot be null for a non-empty span' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/ArraySpanMemoryPinningCompletion >/dev/null && echo True
	@./$(BIN) check Tests/ContiguousMemoryOperationsCompletion >/dev/null && echo True
	@./$(BIN) check Tests/SpanEnumerationRefForeach >/dev/null && echo True
	@./$(BIN) check Tests/GenericPointerUnmanagedStackStorage >/dev/null && echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True

.PHONY: test-native-buffer-call-integration

# Milestone 248: Native Buffer Call Integration
test-native-buffer-call-integration: $(BIN) $(NATIVE_BUFFER_CALL_FIXTURE)
	@./$(BIN) check Tests/NativeBufferCallIntegration >/dev/null && echo True
	@./$(BIN) build Tests/NativeBufferCallIntegration >/dev/null
	@./Tests/NativeBufferCallIntegration/bin/NativeBufferCallIntegration
	@set -e; ./$(BIN) publish Tests/NativeBufferCallIntegration >/dev/null; test -x Tests/NativeBufferCallIntegration/publish/NativeBufferCallIntegration$(EXE_SUFFIX); echo True
	@c=Tests/NativeBufferCallIntegration/.void/NativeBufferCallIntegration.c; grep -Fq 'extern int32_t voidc248_sum_i32(const int32_t *, int32_t);' $$c && grep -Fq 'extern void voidc248_add_i32(int32_t *, int32_t, int32_t);' $$c; echo True
	@c=Tests/NativeBufferCallIntegration/.void/NativeBufferCallIntegration.c; grep -Fq 'extern int32_t voidc248_dot_i32(const int32_t *, int32_t, const int32_t *, int32_t);' $$c; echo True
	@c=Tests/NativeBufferCallIntegration/.void/NativeBufferCallIntegration.c; grep -Fq 'vc_gc_pin_acquire(&vc_native_span_pin_' $$c && grep -Fq 'vc_gc_pin_release(&vc_native_span_pin_' $$c; echo True
	@grep -Fq 'bool vc_semantic_span_type_info(' Compiler/include/semantic.h && grep -Fq 'native Span parameters must be passed by value' Compiler/src/semantic.c && grep -Fq 'native Span element type' Compiler/src/semantic.c; echo True
	@! grep -RqE 'NativeSpan|NativeReadOnlySpan|PointerSpan' Compiler StandardLibrary; echo True
	@grep -Fq 'const int32_t *values, int32_t length' Tests/NativeBufferCallIntegration/native/fixture.c && grep -Fq 'int32_t *values, int32_t length' Tests/NativeBufferCallIntegration/native/fixture.c; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/NativeBufferCallIntegrationDiagnostics/ManagedElement >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "native Span element type 'string' must be unmanaged and supported by the C ABI" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/NativeBufferCallIntegrationDiagnostics/ReadOnlyManagedElement >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "native Span element type 'string' must be unmanaged and supported by the C ABI" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/NativeBufferCallIntegrationDiagnostics/ManagedStructElement >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "native Span element type 'Bad248' must be unmanaged and supported by the C ABI" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/NativeBufferCallIntegrationDiagnostics/ByRefParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native Span parameters must be passed by value' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/NativeBufferCallIntegrationDiagnostics/SpanReturn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "extern method 'Bad' has a return type that is not supported by the C ABI" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/NativeBufferCallIntegrationDiagnostics/MemoryParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'extern method parameter type is not supported by the C ABI' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/NativeBufferCallIntegrationDiagnostics/ReadonlyToWritable >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type 'Native248Bad' has no matching static method 'Bad'" $$out; rm -f $$out; echo True
	@./$(BIN) build Tests/NativeIntegration >/dev/null && echo True
	@./$(BIN) build Tests/NativeByRef >/dev/null && echo True
	@./$(BIN) check Tests/UnmanagedSpanConstructionCompletion >/dev/null && echo True
	@./$(BIN) check Tests/ArraySpanMemoryPinningCompletion >/dev/null && echo True
	@./$(BIN) check Tests/MemorySlicingConversionSpanBridge >/dev/null && echo True
	@./$(BIN) check Tests/ByReferenceSpanIntegrationAudit >/dev/null && echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-unmanaged-span-construction-completion >$$out; test "$$(grep -c '^True$$' $$out)" -eq 50; rm -f $$out; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-array-span-memory-pinning-completion >$$out; test "$$(grep -c '^True$$' $$out)" -eq 51; rm -f $$out; echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True
.PHONY: test-pinning-gc-threads-exceptions-native-boundary-integration

# Milestone 249: Pinning, GC, Threads, Exceptions & Native Boundary Integration
test-pinning-gc-threads-exceptions-native-boundary-integration: $(BIN)
	@./$(BIN) check Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/Library >/dev/null && echo True
	@./$(BIN) build Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/Library >/dev/null; test -f Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/Library/bin/libVoid249PinInteropIntegration.a; echo True
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/CConsumer/main.c Runtime/src/vc_thread.c Runtime/src/vc_atomic.c -o $(PINNING_GC_THREADS_NATIVE_TEST)
	@./$(PINNING_GC_THREADS_NATIVE_TEST)
	@./$(BIN) publish Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/Library >/dev/null; test -f Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/Library/publish/libVoid249PinInteropIntegration.a; echo True
	@c=Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/Library/.void/Void249PinInteropIntegration.c; grep -Fq 'extern int32_t voidc249_block_until_release(int32_t *, int32_t);' $$c && grep -Fq 'extern int32_t voidc249_two_buffers(int32_t *, int32_t, const int32_t *, int32_t, int32_t (*)(int32_t), int32_t);' $$c; echo True
	@c=Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/Library/.void/Void249PinInteropIntegration.c; grep -Fq 'vc_gc_pin_acquire(&vc_native_span_pin_' $$c && grep -Fq 'vc_gc_pin_release(&vc_native_span_pin_' $$c && grep -Fq 'vc_native_take_pending_exception(&vc_pending_site, &vc_pending_start, &vc_pending_count)' $$c; echo True
	@c=Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/Library/.void/Void249PinInteropIntegration.c; grep -Fq 'volatile bool vc_callback_thread_attached = false;' $$c && grep -Fq 'vc_native_capture_exception(vc_exception, vc_boundary.fault_site, vc_boundary.captured_start, vc_boundary.captured_count);' $$c; echo True
	@c=Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/Library/.void/Void249PinInteropIntegration.c; grep -Fq 'bool vc_native_was_safe = vc_gc_native_call_begin();' $$c && grep -Fq 'bool vc_callback_restore_native_safe = vc_gc_managed_reentry_begin();' $$c; echo True
	@grep -Fq 'check(observed_nested_pin_count == 2);' Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/CConsumer/main.c && grep -Fq 'check(two_buffer_pin_before == 2);' Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/CConsumer/main.c && grep -Fq 'check(all_pin_counts_zero());' Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/CConsumer/main.c; echo True
	@grep -Fq 'volatile bool vc_callback_thread_attached = false;' Compiler/src/compiler.c && ! grep -Fq 'Wno-clobbered' Makefile; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-native-buffer-call-integration >$$out; test "$$(grep -c '^True$$' $$out)" -eq 48; rm -f $$out; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-array-span-memory-pinning-completion >$$out; test "$$(grep -c '^True$$' $$out)" -eq 51; rm -f $$out; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-fixed-statement-scoped-pointer-semantics >$$out; test "$$(grep -c '^True$$' $$out)" -eq 51; rm -f $$out; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-thread-exception-native-boundary-cleanup-integration >$$out; test "$$(grep -c '^True$$' $$out)" -eq 27; rm -f $$out; echo True
	@$(MAKE) --no-print-directory -s test-cooperative-stop-the-world-gc-safepoints >/dev/null && echo True
	@./$(BIN) check Tests/MemorySuspensionGcIntegration >/dev/null && echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-managed-pinning-runtime-foundation >$$out; test "$$(grep -c '^True$$' $$out)" -eq 43; rm -f $$out; echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True
.PHONY: test-unmanaged-memory-native-buffer-integration-audit

# Milestone 250: Unmanaged Memory & Native Buffer Integration Audit
test-unmanaged-memory-native-buffer-integration-audit: $(BIN)
	@./$(BIN) check Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Surface >/dev/null && echo True
	@./$(BIN) build Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Surface >/dev/null
	@./Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Surface/bin/UnmanagedMemoryNativeBufferIntegrationAudit
	@set -e; ./$(BIN) publish Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Surface >/dev/null; test -x Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Surface/publish/UnmanagedMemoryNativeBufferIntegrationAudit$(EXE_SUFFIX); echo True
	@./$(BIN) check Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Library >/dev/null && echo True
	@./$(BIN) build Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Library >/dev/null; test -f Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Library/bin/libVoid250UnmanagedMemoryNativeBufferAudit.a; echo True
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/UnmanagedMemoryNativeBufferIntegrationAudit/CConsumer/main.c Runtime/src/vc_thread.c Runtime/src/vc_atomic.c -o $(UNMANAGED_MEMORY_NATIVE_BUFFER_AUDIT_TEST)
	@./$(UNMANAGED_MEMORY_NATIVE_BUFFER_AUDIT_TEST)
	@./$(BIN) publish Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Library >/dev/null; test -f Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Library/publish/libVoid250UnmanagedMemoryNativeBufferAudit.a; echo True
	@c=Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Library/.void/Void250UnmanagedMemoryNativeBufferAudit.c; grep -Fq 'extern int32_t voidc250_audit_buffers(int32_t *, int32_t, const int32_t *, int32_t, int32_t (*)(int32_t), int32_t);' $$c && grep -Fq 'extern int32_t voidc250_observe_pin(int32_t *, int32_t);' $$c; echo True
	@c=Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Library/.void/Void250UnmanagedMemoryNativeBufferAudit.c; grep -Fq 'vc_gc_pin_acquire(&vc_native_span_pin_' $$c && grep -Fq 'vc_gc_pin_release(&vc_native_span_pin_' $$c && grep -Fq 'vc_native_take_pending_exception(&vc_pending_site, &vc_pending_start, &vc_pending_count)' $$c; echo True
	@c=Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Library/.void/Void250UnmanagedMemoryNativeBufferAudit.c; grep -Fq 'volatile bool vc_callback_thread_attached = false;' $$c && grep -Fq 'vc_native_capture_exception(vc_exception, vc_boundary.fault_site, vc_boundary.captured_start, vc_boundary.captured_count);' $$c; echo True
	@c=Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Library/.void/Void250UnmanagedMemoryNativeBufferAudit.c; grep -Fq 'bool vc_native_was_safe = vc_gc_native_call_begin();' $$c && grep -Fq 'bool vc_callback_restore_native_safe = vc_gc_managed_reentry_begin();' $$c; echo True
	@grep -Fq 'check(audit_pin_before_writable == 2);' Tests/UnmanagedMemoryNativeBufferIntegrationAudit/CConsumer/main.c && grep -Fq 'check(audit_pin_before_writable == (int32_t)SIZE_MAX);' Tests/UnmanagedMemoryNativeBufferIntegrationAudit/CConsumer/main.c && grep -Fq 'check(all_pin_counts_zero());' Tests/UnmanagedMemoryNativeBufferIntegrationAudit/CConsumer/main.c; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedMemoryNativeBufferIntegrationAuditDiagnostics/ManagedGeneric >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type argument 'string' must be an unmanaged type for generic parameter 'T'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedMemoryNativeBufferIntegrationAuditDiagnostics/FixedSpanEscape >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'cannot return ref struct value that refers to scoped or local storage' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedMemoryNativeBufferIntegrationAuditDiagnostics/ReadonlyToWritableNative >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type 'Native250Bad' has no matching static method 'Bad'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/UnmanagedMemoryNativeBufferIntegrationAuditDiagnostics/MemoryNativeParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'extern method parameter type is not supported by the C ABI' $$out; rm -f $$out; echo True
	@grep -Fq 'bool vc_semantic_type_is_unmanaged(const VcSemanticModel *model, VcSemanticType type);' Compiler/include/semantic.h && grep -Fq 'pointer_pin_escape_depth(context, operand)' Compiler/src/semantic.c && grep -Fq 'bool vc_semantic_span_type_info(' Compiler/include/semantic.h; echo True
	@! grep -RqE 'NativeSpan|NativeReadOnlySpan|PointerSpan' Compiler StandardLibrary && ! grep -Fq 'Memory(void* pointer' StandardLibrary/Void/Memory.void && ! grep -Fq 'ReadOnlyMemory(void* pointer' StandardLibrary/Void/Memory.void; echo True
	@test $$(find StandardLibrary/Void -name Span.void -type f | wc -l) -eq 1 && test $$(find StandardLibrary/Void -name Memory.void -type f | wc -l) -eq 1 && test -d StandardLibrary/Void/Threading && test -d StandardLibrary/Void/Collections; echo True
	@./$(BIN) check Tests/UnmanagedGenericConstraintFoundation >/dev/null && echo True
	@./$(BIN) check Tests/GenericPointerUnmanagedStackStorage >/dev/null && echo True
	@./$(BIN) check Tests/ManagedPinningRuntimeFoundation/Library >/dev/null && echo True
	@./$(BIN) check Tests/FixedStatementScopedPointerSemantics >/dev/null && echo True
	@./$(BIN) check Tests/StructuralPinnableReferenceProtocol >/dev/null && echo True
	@./$(BIN) check Tests/ArraySpanMemoryPinningCompletion >/dev/null && echo True
	@./$(BIN) check Tests/UnmanagedSpanConstructionCompletion >/dev/null && echo True
	@./$(BIN) check Tests/NativeBufferCallIntegration >/dev/null && echo True
	@./$(BIN) check Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/Library >/dev/null && echo True
	@./$(BIN) check Tests/UnmanagedMemoryNativeBufferIntegrationAudit/AsyncUnsafe >/dev/null && echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-async-object-model-library-boundary-integration >$$out; test "$$(grep -c '^True$$' $$out)" -eq 31; rm -f $$out; echo True
	@grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c && grep -Fq '"version": "0.0.250"' Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Library/UnmanagedMemoryNativeBufferIntegrationAudit.voidproj && grep -Fq '"version": "0.0.250"' Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Surface/UnmanagedMemoryNativeBufferIntegrationAudit.voidproj; echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py && grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-native-heap-runtime-foundation

# Milestone 251: Native Heap Runtime Foundation
test-native-heap-runtime-foundation: $(BIN)
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 Tests/NativeHeapRuntimeFoundation/native_heap_test.c Runtime/src/vc_memory.c -o $(NATIVE_HEAP_RUNTIME_TEST)
	@./$(NATIVE_HEAP_RUNTIME_TEST)
	@./$(BIN) check Tests/NativeHeapRuntimeFoundation >/dev/null && echo True
	@./$(BIN) build Tests/NativeHeapRuntimeFoundation >/dev/null
	@./Tests/NativeHeapRuntimeFoundation/bin/NativeHeapRuntimeFoundation
	@set -e; ./$(BIN) publish Tests/NativeHeapRuntimeFoundation >/dev/null; test -x Tests/NativeHeapRuntimeFoundation/publish/NativeHeapRuntimeFoundation$(EXE_SUFFIX); echo True
	@grep -Fq 'bool vc_native_memory_allocate(size_t size, void **memory);' Runtime/include/vc_memory.h && grep -Fq 'void vc_native_memory_free(void *memory);' Runtime/include/vc_memory.h; echo True
	@grep -Fq 'if (size == 0u)' Runtime/src/vc_memory.c && grep -Fq '*memory = NULL;' Runtime/src/vc_memory.c && grep -Fq 'void *result = malloc(size);' Runtime/src/vc_memory.c && grep -Fq 'free(memory);' Runtime/src/vc_memory.c; echo True
	@! grep -Eq 'realloc|aligned_alloc|posix_memalign|_aligned_malloc|calloc' Runtime/src/vc_memory.c; echo True
	@! grep -Eq 'windows.h|pthread|_WIN32|__APPLE__|__linux__' Runtime/include/vc_memory.h; echo True
	@grep -Fq 'vc_memory.c' Compiler/src/compiler.c && grep -Fq 'vc_memory_runtime.o' Compiler/src/compiler.c && grep -Fq 'runtime_memory_source' Compiler/src/compiler.c; echo True
	@./$(BIN) build Tests/LibraryOutput/Library >/dev/null; ar t Tests/LibraryOutput/Library/bin/libVoid099Lib.a | grep -Fq 'vc_memory_runtime.o'; echo True
	@./$(BIN) publish Tests/LibraryOutput/Library >/dev/null; ar t Tests/LibraryOutput/Library/publish/libVoid099Lib.a | grep -Fq 'vc_memory_runtime.o'; echo True
	@grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c && grep -Fq '"version": "0.0.251"' Tests/NativeHeapRuntimeFoundation/NativeHeapRuntimeFoundation.voidproj; echo True
	@./$(BIN) check Tests/NativeThreadRuntimeFoundation >/dev/null && ./$(BIN) check Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Surface >/dev/null && echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py && grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True
.PHONY: test-native-memory-allocation-surface

# Milestone 252: NativeMemory Allocation Surface
test-native-memory-allocation-surface: $(BIN)
	@./$(BIN) check Tests/NativeMemoryAllocationSurface >/dev/null && echo True
	@./$(BIN) build Tests/NativeMemoryAllocationSurface >/dev/null
	@./Tests/NativeMemoryAllocationSurface/bin/NativeMemoryAllocationSurface
	@set -e; ./$(BIN) publish Tests/NativeMemoryAllocationSurface >/dev/null; test -x Tests/NativeMemoryAllocationSurface/publish/NativeMemoryAllocationSurface$(EXE_SUFFIX); echo True
	@c=Tests/NativeMemoryAllocationSurface/.void/NativeMemoryAllocationSurface.c; grep -Fq '#include "vc_memory.h"' $$c && grep -Fq 'static VC_MAYBE_UNUSED bool vc_native_memory_try_allocate_u64(uint64_t byte_count, void **memory)' $$c; echo True
	@c=Tests/NativeMemoryAllocationSurface/.void/NativeMemoryAllocationSurface.c; grep -Fq 'if (byte_count > (uint64_t)SIZE_MAX) return false;' $$c && grep -Fq 'return vc_native_memory_allocate((size_t)byte_count, memory);' $$c; echo True
	@c=Tests/NativeMemoryAllocationSurface/.void/NativeMemoryAllocationSurface.c; grep -Fq 'vc_native_memory_try_allocate_u64((uint64_t)(' $$c && grep -Fq 'vc_native_memory_free((void *)(' $$c; echo True
	@grep -Fq 'public static unsafe void* Allocate(ulong byteCount)' StandardLibrary/Void/NativeMemory.void && grep -Fq 'public static unsafe bool TryAllocate(ulong byteCount, out void* memory)' StandardLibrary/Void/NativeMemory.void && grep -Fq 'public static unsafe void Free(void* memory)' StandardLibrary/Void/NativeMemory.void; echo True
	@grep -Fq 'Runtime.NativeMemoryTryAllocate(byteCount, out memory)' StandardLibrary/Void/NativeMemory.void && grep -Fq 'Runtime.NativeMemoryFree(memory)' StandardLibrary/Void/NativeMemory.void && ! grep -Eq 'extern|malloc|free[(]' StandardLibrary/Void/NativeMemory.void; echo True
	@grep -Fq 'if (size == 0u)' Runtime/src/vc_memory.c && grep -Fq '*memory = NULL;' Runtime/src/vc_memory.c && grep -Fq 'void vc_native_memory_free(void *memory);' Runtime/include/vc_memory.h; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/NativeMemoryAllocationSurfaceDiagnostics/SafeAllocate >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "unsafe method 'Allocate' requires an unsafe method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/NativeMemoryAllocationSurfaceDiagnostics/SafeFree >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "unsafe method 'Free' requires an unsafe method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/NativeMemoryAllocationSurfaceDiagnostics/RuntimeBypass >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Runtime.NativeMemoryTryAllocate is reserved for unsafe Void.NativeMemory methods' $$out; rm -f $$out; echo True
	@test $$(find StandardLibrary/Void -name NativeMemory.void -type f | wc -l) -eq 1 && test -f StandardLibrary/Void/NativeMemory.void; echo True
	@! grep -Fq 'Allocate<' StandardLibrary/Void/NativeMemory.void && ! grep -Eq 'Realloc|Aligned|aligned_alloc|posix_memalign|_aligned_malloc' StandardLibrary/Void/NativeMemory.void Runtime/src/vc_memory.c; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-native-heap-runtime-foundation >$$out; test "$$(grep -c '^True$$' $$out)" -eq 26; rm -f $$out; echo True
	@./$(BIN) check Tests/UnmanagedMemoryNativeBufferIntegrationAudit/Surface >/dev/null && echo True
	@./$(BIN) build Tests/LibraryOutput/Library >/dev/null; ar t Tests/LibraryOutput/Library/bin/libVoid099Lib.a | grep -Fq 'vc_memory_runtime.o'; echo True
	@grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c && grep -Fq '"version": "0.0.252"' Tests/NativeMemoryAllocationSurface/NativeMemoryAllocationSurface.voidproj && grep -Fq '"version": "0.0.251"' Tests/NativeHeapRuntimeFoundation/NativeHeapRuntimeFoundation.voidproj; echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py && grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-generic-typed-native-allocation-completion

# Milestone 253: Generic Typed Native Allocation Completion
test-generic-typed-native-allocation-completion: $(BIN)
	@./$(BIN) check Tests/GenericTypedNativeAllocationCompletion >/dev/null && echo True
	@./$(BIN) build Tests/GenericTypedNativeAllocationCompletion >/dev/null
	@./Tests/GenericTypedNativeAllocationCompletion/bin/GenericTypedNativeAllocationCompletion
	@set -e; ./$(BIN) publish Tests/GenericTypedNativeAllocationCompletion >/dev/null; test -x Tests/GenericTypedNativeAllocationCompletion/publish/GenericTypedNativeAllocationCompletion$(EXE_SUFFIX); echo True
	@c=Tests/GenericTypedNativeAllocationCompletion/.void/GenericTypedNativeAllocationCompletion.c; grep -Fq 'static VC_MAYBE_UNUSED bool vc_native_memory_try_allocate_elements_u64(uint64_t count, size_t element_size, void **memory)' $$c; echo True
	@c=Tests/GenericTypedNativeAllocationCompletion/.void/GenericTypedNativeAllocationCompletion.c; grep -Fq 'if (count > (uint64_t)SIZE_MAX) return false;' $$c && grep -Fq 'if (element_size != 0u && native_count > SIZE_MAX / element_size) return false;' $$c && grep -Fq 'return vc_native_memory_allocate(native_count * element_size, memory);' $$c; echo True
	@c=Tests/GenericTypedNativeAllocationCompletion/.void/GenericTypedNativeAllocationCompletion.c; grep -Fq 'sizeof(int32_t)' $$c && grep -Fq 'sizeof(vc_s_' $$c && grep -Fq 'vc_native_memory_try_allocate_elements_u64' $$c; echo True
	@grep -Fq 'public static unsafe T* Allocate<T>(ulong count) where T : unmanaged' StandardLibrary/Void/NativeMemory.void && grep -Fq 'public static unsafe bool TryAllocate<T>(ulong count, out T* memory) where T : unmanaged' StandardLibrary/Void/NativeMemory.void; echo True
	@grep -Fq 'Runtime.NativeMemoryTryAllocateElements(count, sizeof(T), out memory)' StandardLibrary/Void/NativeMemory.void && grep -Fq 'Runtime.NativeMemoryTryAllocateElements(count, sizeof(T), out raw)' StandardLibrary/Void/NativeMemory.void; echo True
	@grep -Fq 'return vc_semantic_type_is_unmanaged(model, type);' Compiler/src/semantic.c && grep -Fq 'semantic_type_is_unmanaged_recursive' Compiler/src/semantic.c; echo True
	@! grep -Rq 'Nested253\|Pair253\|Mode253' Compiler Runtime StandardLibrary; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GenericTypedNativeAllocationDiagnostics/ManagedType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type argument 'string' must be an unmanaged type for generic parameter 'T'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GenericTypedNativeAllocationDiagnostics/ManagedStruct >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type argument 'Managed253' must be an unmanaged type for generic parameter 'T'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GenericTypedNativeAllocationDiagnostics/MissingConstraint >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "generic pointer return type using 'T' requires 'where T : unmanaged'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GenericTypedNativeAllocationDiagnostics/SafeAllocate >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "unsafe method 'Allocate' requires an unsafe method" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/GenericTypedNativeAllocationDiagnostics/RuntimeBypass >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Runtime.NativeMemoryTryAllocateElements is reserved for unsafe Void.NativeMemory methods' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/GenericTypedNativeAllocationRuntime/Overflow >/dev/null; out=$$(mktemp); if ./Tests/GenericTypedNativeAllocationRuntime/Overflow/bin/GenericTypedNativeAllocationRuntimeOverflow >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled OutOfMemoryException: native typed memory allocation failed' $$out; rm -f $$out; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-native-memory-allocation-surface >$$out; test "$$(grep -c '^True$$' $$out)" -eq 28; rm -f $$out; echo True
	@./$(BIN) check Tests/UnmanagedGenericConstraintFoundation >/dev/null && ./$(BIN) check Tests/GenericPointerUnmanagedStackStorage >/dev/null && echo True
	@./$(BIN) check Tests/NativeStructs >/dev/null && ./$(BIN) check Tests/UnmanagedSpanConstructionCompletion >/dev/null && echo True
	@grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c && grep -Fq '"version": "0.0.253"' Tests/GenericTypedNativeAllocationCompletion/GenericTypedNativeAllocationCompletion.voidproj && grep -Fq '"version": "0.0.252"' Tests/NativeMemoryAllocationSurface/NativeMemoryAllocationSurface.voidproj; echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py && grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-native-reallocation-alignment-completion

# Milestone 254: Native Reallocation & Alignment Completion
test-native-reallocation-alignment-completion: $(BIN)
	@./$(BIN) check Tests/NativeReallocationAlignmentCompletion >/dev/null && echo True
	@./$(BIN) build Tests/NativeReallocationAlignmentCompletion >/dev/null
	@./Tests/NativeReallocationAlignmentCompletion/bin/NativeReallocationAlignmentCompletion
	@set -e; ./$(BIN) publish Tests/NativeReallocationAlignmentCompletion >/dev/null; test -x Tests/NativeReallocationAlignmentCompletion/publish/NativeReallocationAlignmentCompletion$(EXE_SUFFIX); echo True
	@cc -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 Tests/NativeReallocationAlignmentCompletion/native_memory_contract_test.c Runtime/src/vc_memory.c -o Tests/NativeReallocationAlignmentCompletion/native_memory_contract_test
	@./Tests/NativeReallocationAlignmentCompletion/native_memory_contract_test
	@grep -Fq 'bool vc_native_memory_reallocate(void **memory, size_t size);' Runtime/include/vc_memory.h && grep -Fq 'bool vc_native_memory_allocate_aligned(size_t size, size_t alignment, void **memory);' Runtime/include/vc_memory.h && grep -Fq 'bool vc_native_memory_reallocate_aligned(void **memory, size_t size, size_t alignment);' Runtime/include/vc_memory.h; echo True
	@grep -Fq 'if (size == 0u)' Runtime/src/vc_memory.c && grep -Fq 'vc_native_memory_free(original);' Runtime/src/vc_memory.c && grep -Fq '*memory = NULL;' Runtime/src/vc_memory.c; echo True
	@grep -Fq 'if (!vc_native_memory_allocate_impl(size, alignment, &replacement))' Runtime/src/vc_memory.c && grep -Fq 'memcpy(replacement, original, copy_size);' Runtime/src/vc_memory.c && grep -Fq '*memory = replacement;' Runtime/src/vc_memory.c; echo True
	@grep -Fq 'alignment != 0u && (alignment & (alignment - 1u)) == 0u' Runtime/src/vc_memory.c && grep -Fq 'const size_t effective_alignment = vc_native_memory_effective_alignment(alignment);' Runtime/src/vc_memory.c; echo True
	@! grep -Eq '(^|[^A-Za-z_])realloc[[:space:]]*\(|aligned_alloc[[:space:]]*\(|posix_memalign[[:space:]]*\(|_aligned_malloc[[:space:]]*\(' Runtime/src/vc_memory.c; echo True
	@grep -Fq 'public static unsafe void* Reallocate(void* memory, ulong byteCount)' StandardLibrary/Void/NativeMemory.void && grep -Fq 'public static unsafe bool TryReallocate(ref void* memory, ulong byteCount)' StandardLibrary/Void/NativeMemory.void; echo True
	@grep -Fq 'public static unsafe T* Reallocate<T>(T* memory, ulong count) where T : unmanaged' StandardLibrary/Void/NativeMemory.void && grep -Fq 'public static unsafe bool TryReallocate<T>(ref T* memory, ulong count) where T : unmanaged' StandardLibrary/Void/NativeMemory.void; echo True
	@grep -Fq 'public static unsafe void* AllocateAligned(ulong byteCount, ulong alignment)' StandardLibrary/Void/NativeMemory.void && grep -Fq 'public static unsafe bool TryAllocateAligned(ulong byteCount, ulong alignment, out void* memory)' StandardLibrary/Void/NativeMemory.void; echo True
	@grep -Fq 'public static unsafe T* AllocateAligned<T>(ulong count, ulong alignment) where T : unmanaged' StandardLibrary/Void/NativeMemory.void && grep -Fq 'public static unsafe T* ReallocateAligned<T>(T* memory, ulong count, ulong alignment) where T : unmanaged' StandardLibrary/Void/NativeMemory.void; echo True
	@c=Tests/NativeReallocationAlignmentCompletion/.void/NativeReallocationAlignmentCompletion.c; grep -Fq 'static VC_MAYBE_UNUSED bool vc_native_memory_try_reallocate_u64(void *original, uint64_t byte_count, void **memory)' $$c && grep -Fq '*memory = original;' $$c; echo True
	@c=Tests/NativeReallocationAlignmentCompletion/.void/NativeReallocationAlignmentCompletion.c; grep -Fq 'vc_native_memory_try_reallocate_aligned_u64' $$c && grep -Fq 'vc_native_memory_allocate_aligned' $$c && grep -Fq 'vc_native_memory_reallocate_aligned' $$c; echo True
	@c=Tests/NativeReallocationAlignmentCompletion/.void/NativeReallocationAlignmentCompletion.c; grep -Fq 'vc_native_memory_try_reallocate_elements_u64' $$c && grep -Fq 'vc_native_memory_try_reallocate_aligned_elements_u64' $$c; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/NativeReallocationAlignmentDiagnostics/SafeReallocate >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'pointer locals require an unsafe method' $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/NativeReallocationAlignmentDiagnostics/ManagedAlignedType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type argument 'string' must be an unmanaged type for generic parameter 'T'" $$out; rm -f $$out; echo True
	@out=$$(mktemp); if ./$(BIN) check Tests/NativeReallocationAlignmentDiagnostics/RuntimeBypass >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Runtime.NativeMemoryTryAllocateAligned is reserved for unsafe Void.NativeMemory methods' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/NativeReallocationAlignmentRuntime/InvalidAlignment >/dev/null; out=$$(mktemp); if ./Tests/NativeReallocationAlignmentRuntime/InvalidAlignment/bin/NativeReallocationAlignmentRuntimeInvalidAlignment >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentException: Alignment must be a power of two' $$out; rm -f $$out; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-generic-typed-native-allocation-completion >$$out; test "$$(grep -c '^True$$' $$out)" -eq 39; rm -f $$out; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-native-memory-allocation-surface >$$out; test "$$(grep -c '^True$$' $$out)" -eq 28; rm -f $$out; echo True
	@out=$$(mktemp); $(MAKE) --no-print-directory -s test-native-heap-runtime-foundation >$$out; test "$$(grep -c '^True$$' $$out)" -eq 26; rm -f $$out; echo True
	@grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c && grep -Fq '"version": "0.0.254"' Tests/NativeReallocationAlignmentCompletion/NativeReallocationAlignmentCompletion.voidproj && grep -Fq '"version": "0.0.253"' Tests/GenericTypedNativeAllocationCompletion/GenericTypedNativeAllocationCompletion.voidproj; echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py && grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-native-function-pointer-type-foundation

# Milestone 255: Native Function Pointer Type Foundation
test-native-function-pointer-type-foundation: $(BIN)
	@set -e; ./$(BIN) check Tests/NativeFunctionPointerTypeFoundation >/dev/null; echo True
	@./$(BIN) build Tests/NativeFunctionPointerTypeFoundation >/dev/null
	@./Tests/NativeFunctionPointerTypeFoundation/bin/NativeFunctionPointerTypeFoundation
	@set -e; ./$(BIN) publish Tests/NativeFunctionPointerTypeFoundation >/dev/null; test -x Tests/NativeFunctionPointerTypeFoundation/publish/NativeFunctionPointerTypeFoundation$(EXE_SUFFIX); echo True
	@set -e; c=Tests/NativeFunctionPointerTypeFoundation/.void/NativeFunctionPointerTypeFoundation.c; grep -Fq 'typedef int32_t (*vc_fnptr_0)(int32_t);' $$c; grep -Fq 'typedef void (*vc_fnptr_2)(void);' $$c; grep -Fq 'typedef vc_s_0 (*vc_fnptr_4)(vc_s_0);' $$c; echo True
	@set -e; grep -Fq '#define VC_SEM_TYPE_FUNCTION_POINTER_BASE UINT32_C(0xC0000000)' Compiler/include/semantic.h; grep -Fq 'return type >= VC_SEM_TYPE_POINTER_BASE && type < VC_SEM_TYPE_FUNCTION_POINTER_BASE;' Compiler/src/semantic.c; grep -Fq 'return type >= VC_SEM_TYPE_FUNCTION_POINTER_BASE;' Compiler/src/semantic.c; echo True
	@set -e; grep -Fq 'vc_semantic_type_is_pointer(type) || vc_semantic_type_is_function_pointer(type)' Compiler/src/semantic.c; grep -Fq 'VC_SEM_CONVERSION_FUNCTION_POINTER' Compiler/include/semantic.h; echo True
	@set -e; grep -Fq '#define VC_TYPE_FUNCTION_POINTER UINT32_C(1024)' Tests/NativeFunctionPointerTypeFoundation/.void/NativeFunctionPointerTypeFoundation.c; grep -Fq 'strcmp(member, "IsFunctionPointer") == 0 ? "VC_TYPE_FUNCTION_POINTER"' Compiler/src/compiler.c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionPointerTypeFoundationDiagnostics/SafeLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function pointer locals require an unsafe method' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionPointerTypeFoundationDiagnostics/SafeSignature >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function pointer signatures require an unsafe method in an unsafe-enabled project' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionPointerTypeFoundationDiagnostics/ManagedSignature >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "native function pointer field 'Callback' has a signature that is not supported by the C ABI" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionPointerTypeFoundationDiagnostics/FunctionToDataPointer >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot explicitly cast 'delegate*<int, int>' to 'void*'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionPointerTypeFoundationDiagnostics/DataToFunctionPointer >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot explicitly cast 'void*' to 'delegate*<int, int>'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionPointerTypeFoundationDiagnostics/NullableSuffix >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "native function pointers are already nullable and cannot use '?'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionPointerTypeFoundationDiagnostics/DataPointerSuffix >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "native function pointers are distinct from data pointers and cannot use a data-pointer '*' suffix" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionPointerTypeFoundationDiagnostics/VoidParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function pointer parameters cannot be void' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionPointerTypeFoundationDiagnostics/SafeField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "native function pointer field 'Callback' requires an unsafe value struct" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) check Tests/NativeFunctionPointerTypeFoundationDiagnostics/ExternParameter >/dev/null; echo True
	@set -e; out=$$(mktemp); $(MAKE) --no-print-directory -s test-native-reallocation-alignment-completion >$$out; test "$$(grep -c '^True$$' $$out)" -eq 51; rm -f $$out; echo True
	@set -e; out=$$(mktemp); $(MAKE) --no-print-directory -s test-generic-pointer-unmanaged-stack-storage-completion >$$out; test "$$(grep -c '^True$$' $$out)" -eq 55; rm -f $$out; echo True
	@set -e; out=$$(mktemp); $(MAKE) --no-print-directory -s test-rectangular-array-initializers >$$out; test "$$(grep -c '^True$$' $$out)" -gt 0; rm -f $$out; echo True
	@set -e; out=$$(mktemp); $(MAKE) --no-print-directory -s test-unsafe-member-completion >$$out; test "$$(grep -c '^True$$' $$out)" -gt 0; rm -f $$out; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.255"' Tests/NativeFunctionPointerTypeFoundation/NativeFunctionPointerTypeFoundation.voidproj; grep -Fq '"version": "0.0.254"' Tests/NativeReallocationAlignmentCompletion/NativeReallocationAlignmentCompletion.voidproj; echo True
	@set -e; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-native-function-address-indirect-call-completion

# Milestone 256: Native Function Address & Indirect Call Completion
test-native-function-address-indirect-call-completion: $(BIN) $(NATIVE_FUNCTION_ADDRESS_FIXTURE)
	@set -e; ./$(BIN) check Tests/NativeFunctionAddressIndirectCallCompletion >/dev/null; echo True
	@./$(BIN) build Tests/NativeFunctionAddressIndirectCallCompletion >/dev/null
	@./Tests/NativeFunctionAddressIndirectCallCompletion/bin/NativeFunctionAddressIndirectCallCompletion
	@set -e; ./$(BIN) publish Tests/NativeFunctionAddressIndirectCallCompletion >/dev/null; test -x Tests/NativeFunctionAddressIndirectCallCompletion/publish/NativeFunctionAddressIndirectCallCompletion$(EXE_SUFFIX); echo True
	@set -e; c=Tests/NativeFunctionAddressIndirectCallCompletion/.void/NativeFunctionAddressIndirectCallCompletion.c; grep -Fq 'typedef int32_t (*vc_fnptr_0)(int32_t, int32_t);' $$c; grep -Fq 'vc_l_0 = &voidc256_add;' $$c; echo True
	@set -e; c=Tests/NativeFunctionAddressIndirectCallCompletion/.void/NativeFunctionAddressIndirectCallCompletion.c; grep -Fq 'vc_native_fn_call_0' $$c; grep -Fq 'vc_gc_native_call_begin()' $$c; grep -Fq 'vc_native_call_depth++' $$c; grep -Fq 'vc_gc_native_call_end(vc_native_was_safe)' $$c; grep -Fq 'vc_native_pending_exception != NULL' $$c; echo True
	@set -e; c=Tests/NativeFunctionAddressIndirectCallCompletion/.void/NativeFunctionAddressIndirectCallCompletion.c; grep -Fq 'vc_l_0 = &voidc256_add;' $$c; ! grep -Eq 'vc_l_0 = .*void ?\\*' $$c; echo True
	@set -e; ./$(BIN) build Tests/NativeFunctionAddressIndirectCallRuntime/NullInvoke >/dev/null; out=$$(mktemp); if ./Tests/NativeFunctionAddressIndirectCallRuntime/NullInvoke/bin/NativeFunctionAddressIndirectCallRuntimeNullInvoke >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: native function pointer is null' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionAddressIndirectCallDiagnostics/SafeAddress >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function address acquisition requires an unsafe method' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionAddressIndirectCallDiagnostics/ManagedMethod >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "no static extern method matches native function pointer signature 'delegate*<int, int>'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionAddressIndirectCallDiagnostics/SignatureMismatch >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "no static extern method matches native function pointer signature 'delegate*<long, long>'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionAddressIndirectCallDiagnostics/InstanceTarget >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function address requires a direct static extern method group' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionAddressIndirectCallDiagnostics/WrongArgumentCount >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "native function pointer 'delegate*<int, int>' expects 1 argument(s), got 2" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionAddressIndirectCallDiagnostics/WrongArgumentType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "native function pointer argument 1 expects 'int', got 'string'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionAddressIndirectCallDiagnostics/RefArgument >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function pointer arguments are passed by value' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionAddressIndirectCallDiagnostics/NamedArgument >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function pointer invocation does not support named arguments' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionAddressIndirectCallDiagnostics/SafeInvoke >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function pointer invocation requires an unsafe method' $$out; rm -f $$out; echo True
	@set -e; grep -Fq 'has_function_pointer_address' Compiler/include/semantic.h; grep -Fq 'has_function_pointer_invoke' Compiler/include/semantic.h; grep -Fq 'vc_native_fn_call_' Compiler/src/compiler.c; grep -Fq 'vc_gc_native_call_begin()' Compiler/src/compiler.c; echo True
	@set -e; out=$$(mktemp); $(MAKE) --no-print-directory -s test-native-function-pointer-type-foundation >$$out; test "$$(grep -c '^True$$' $$out)" -eq 44; rm -f $$out; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.256"' Tests/NativeFunctionAddressIndirectCallCompletion/NativeFunctionAddressIndirectCallCompletion.voidproj; grep -Fq '"version": "0.0.255"' Tests/NativeFunctionPointerTypeFoundation/NativeFunctionPointerTypeFoundation.voidproj; echo True
	@set -e; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-native-function-pointer-abi-integration

# Milestone 257: Native Function Pointer ABI Integration
test-native-function-pointer-abi-integration: $(BIN) $(NATIVE_FUNCTION_ABI_FIXTURE)
	@set -e; ./$(BIN) check Tests/NativeFunctionPointerAbiIntegration >/dev/null; echo True
	@./$(BIN) build Tests/NativeFunctionPointerAbiIntegration >/dev/null
	@./Tests/NativeFunctionPointerAbiIntegration/bin/NativeFunctionPointerAbiIntegration
	@set -e; ./$(BIN) publish Tests/NativeFunctionPointerAbiIntegration >/dev/null; test -x Tests/NativeFunctionPointerAbiIntegration/publish/NativeFunctionPointerAbiIntegration$(EXE_SUFFIX); echo True
	@set -e; c=Tests/NativeFunctionPointerAbiIntegration/.void/NativeFunctionPointerAbiIntegration.c; grep -Fq 'extern int32_t voidc257_apply(vc_fnptr_0, int32_t, int32_t);' $$c; grep -Fq 'extern vc_fnptr_0 voidc257_select(int32_t);' $$c; grep -Fq 'extern vc_fnptr_0 voidc257_passthrough(vc_fnptr_0);' $$c; echo True
	@set -e; c=Tests/NativeFunctionPointerAbiIntegration/.void/NativeFunctionPointerAbiIntegration.c; grep -Fq 'static VC_MAYBE_UNUSED int32_t vc_native_call_2(vc_fnptr_0 vc_p_0, int32_t vc_p_1, int32_t vc_p_2)' $$c; grep -Fq 'static VC_MAYBE_UNUSED vc_fnptr_0 vc_native_call_3(int32_t vc_p_0)' $$c; grep -Fq 'static VC_MAYBE_UNUSED vc_fnptr_0 vc_native_call_4(vc_fnptr_0 vc_p_0)' $$c; echo True
	@set -e; c=Tests/NativeFunctionPointerAbiIntegration/.void/NativeFunctionPointerAbiIntegration.c; ! grep -Eq 'voidc257_(apply|select|passthrough).*void ?\*' $$c; grep -Fq 'vc_fnptr_0 vc_result = voidc257_select(vc_p_0);' $$c; echo True
	@set -e; c=Tests/NativeFunctionPointerAbiIntegration/.void/NativeFunctionPointerAbiIntegration.c; grep -Fq 'vc_gc_native_call_begin()' $$c; grep -Fq 'vc_gc_native_call_end(vc_native_was_safe)' $$c; grep -Fq 'vc_native_pending_exception != NULL' $$c; echo True
	@set -e; grep -Fq 'typedef int32_t (*Voidc257BinaryFn)(int32_t, int32_t);' Tests/NativeFunctionPointerAbiIntegration/native/fixture.c; grep -Fq 'Voidc257BinaryFn voidc257_select' Tests/NativeFunctionPointerAbiIntegration/native/fixture.c; echo True
	@set -e; grep -Fq 'vc_semantic_type_is_function_pointer(method.return_type)' Compiler/src/semantic.c; grep -Fq 'vc_semantic_type_is_function_pointer(method.parameter_types[parameter_index])' Compiler/src/semantic.c; grep -Fq 'native function pointer extern parameters must be passed by value' Compiler/src/semantic.c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionPointerAbiIntegrationDiagnostics/ByRefParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function pointer extern parameters must be passed by value' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionPointerAbiIntegrationDiagnostics/ManagedParameterSignature >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function pointer parameter signature is not supported by the C ABI' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionPointerAbiIntegrationDiagnostics/ManagedReturnSignature >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function pointer return signature is not supported by the C ABI' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionPointerAbiIntegrationDiagnostics/DataPointerConversion >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot explicitly cast 'delegate*<int, int>' to 'void*'" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) check Tests/NativeFunctionPointerTypeFoundationDiagnostics/ExternParameter >/dev/null; echo True
	@set -e; ./$(BIN) check Tests/NativeFunctionAddressIndirectCallCompletion >/dev/null; ./$(BIN) build Tests/NativeFunctionAddressIndirectCallCompletion >/dev/null; out=$$(mktemp); ./Tests/NativeFunctionAddressIndirectCallCompletion/bin/NativeFunctionAddressIndirectCallCompletion >$$out; test "$$(grep -c '^True$$' $$out)" -eq 15; rm -f $$out; echo True
	@set -e; ./$(BIN) check Tests/NativeFunctionPointerTypeFoundation >/dev/null; ./$(BIN) build Tests/NativeFunctionPointerTypeFoundation >/dev/null; out=$$(mktemp); ./Tests/NativeFunctionPointerTypeFoundation/bin/NativeFunctionPointerTypeFoundation >$$out; test "$$(grep -c '^True$$' $$out)" -eq 22; rm -f $$out; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.257"' Tests/NativeFunctionPointerAbiIntegration/NativeFunctionPointerAbiIntegration.voidproj; grep -Fq '"version": "0.0.256"' Tests/NativeFunctionAddressIndirectCallCompletion/NativeFunctionAddressIndirectCallCompletion.voidproj; echo True
	@set -e; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-generic-function-pointer-unmanaged-storage-integration

# Milestone 258: Generic Function Pointer & Unmanaged Storage Integration
test-generic-function-pointer-unmanaged-storage-integration: $(BIN) $(NATIVE_FUNCTION_GENERIC_FIXTURE)
	@set -e; ./$(BIN) check Tests/GenericFunctionPointerUnmanagedStorageIntegration >/dev/null; echo True
	@./$(BIN) build Tests/GenericFunctionPointerUnmanagedStorageIntegration >/dev/null
	@./Tests/GenericFunctionPointerUnmanagedStorageIntegration/bin/GenericFunctionPointerUnmanagedStorageIntegration
	@set -e; ./$(BIN) publish Tests/GenericFunctionPointerUnmanagedStorageIntegration >/dev/null; test -x Tests/GenericFunctionPointerUnmanagedStorageIntegration/publish/GenericFunctionPointerUnmanagedStorageIntegration$(EXE_SUFFIX); echo True
	@set -e; c=Tests/GenericFunctionPointerUnmanagedStorageIntegration/.void/GenericFunctionPointerUnmanagedStorageIntegration.c; grep -Fq 'typedef int32_t (*vc_fnptr_0)(int32_t, int32_t);' $$c; grep -Fq 'vc_fnptr_0 * vc_l_' $$c; grep -Fq 'sizeof(vc_fnptr_0)' $$c; grep -Fq 'vc_fnptr_0 vc_sa_' $$c; ! grep -Eq 'vc_fnptr_0[^;]*void ?\*|void ?\*[^;]*vc_fnptr_0' $$c; echo True
	@set -e; c=Tests/GenericFunctionPointerUnmanagedStorageIntegration/.void/GenericFunctionPointerUnmanagedStorageIntegration.c; grep -Fq 'Span__g1_delegate__x2a__g3_int_int_int' $$c; grep -Fq 'ReadOnlySpan__g1_delegate__x2a__g3_int_int_int' $$c; grep -Fq 'Span__g1_NativeEntry258' $$c; echo True
	@set -e; grep -Fq 'typedef struct VcTypeScan' Compiler/src/parser.c; grep -Fq 'generic_call_scan_greater' Compiler/src/parser.c; grep -Fq 'if (function_pointer)' Compiler/src/parser.c; echo True
	@set -e; grep -Fq 'vc_semantic_type_is_function_pointer(type)' Compiler/src/semantic.c; grep -Fq 'generic_function_pointer' Compiler/src/semantic.c; grep -Fq 'native function pointer type arguments require an unsafe method' Compiler/src/semantic.c; echo True
	@set -e; grep -Fq 'had_explicit_function_pointer_type_argument' Compiler/include/ast.h; grep -Fq 'monomorph_type_ref_has_explicit_function_pointer' Compiler/src/monomorph.c; grep -Fq 'call->as.call_expression.had_explicit_function_pointer_type_argument = true' Compiler/src/monomorph.c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/GenericFunctionPointerUnmanagedStorageIntegrationDiagnostics/SafeGenericArgument >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function pointer type arguments require an unsafe method' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/GenericFunctionPointerUnmanagedStorageIntegrationDiagnostics/SafeGenericStorage >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function pointer type arguments require an unsafe method' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/GenericFunctionPointerUnmanagedStorageIntegrationDiagnostics/SafeGenericTypeof >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function pointer type metadata requires an unsafe method' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/GenericFunctionPointerUnmanagedStorageIntegrationDiagnostics/MissingUnmanagedConstraint >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "requires 'where T : unmanaged'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionPointerTypeFoundationDiagnostics/DataPointerSuffix >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "native function pointers are distinct from data pointers and cannot use a data-pointer '*' suffix" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeFunctionPointerAbiIntegrationDiagnostics/DataPointerConversion >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot explicitly cast 'delegate*<int, int>' to 'void*'" $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) check Tests/NativeFunctionPointerAbiIntegration >/dev/null; ./$(BIN) build Tests/NativeFunctionPointerAbiIntegration >/dev/null; out=$$(mktemp); ./Tests/NativeFunctionPointerAbiIntegration/bin/NativeFunctionPointerAbiIntegration >$$out; test "$$(grep -c '^True$$' $$out)" -eq 12; rm -f $$out; echo True
	@set -e; ./$(BIN) check Tests/GenericPointerUnmanagedStackStorage >/dev/null; ./$(BIN) build Tests/GenericPointerUnmanagedStackStorage >/dev/null; out=$$(mktemp); ./Tests/GenericPointerUnmanagedStackStorage/bin/GenericPointerUnmanagedStackStorage >$$out; test "$$(grep -c '^True$$' $$out)" -gt 0; rm -f $$out; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.258"' Tests/GenericFunctionPointerUnmanagedStorageIntegration/GenericFunctionPointerUnmanagedStorageIntegration.voidproj; grep -Fq '"version": "0.0.257"' Tests/NativeFunctionPointerAbiIntegration/NativeFunctionPointerAbiIntegration.voidproj; echo True
	@set -e; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-native-heap-function-pointers-gc-threads-exception-boundary-integration

# Milestone 259: Native Heap, Function Pointers, GC, Threads & Exception Boundary Integration
test-native-heap-function-pointers-gc-threads-exception-boundary-integration: $(BIN)
	@./$(BIN) check Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/Library >/dev/null && echo True
	@./$(BIN) build Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/Library >/dev/null; test -f Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/Library/bin/libVoid259NativeHeapFunctionPointerIntegration.a; echo True
	@ar t Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/Library/bin/libVoid259NativeHeapFunctionPointerIntegration.a | grep -Fq 'vc_memory_runtime.o'; echo True
	@ar t Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/Library/bin/libVoid259NativeHeapFunctionPointerIntegration.a | grep -Fq 'vc_thread_runtime.o'; echo True
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/CConsumer/main.c Runtime/src/vc_thread.c Runtime/src/vc_atomic.c Runtime/src/vc_memory.c -o $(NATIVE_HEAP_FUNCTION_POINTER_INTEGRATION_TEST)
	@./$(NATIVE_HEAP_FUNCTION_POINTER_INTEGRATION_TEST)
	@./$(BIN) publish Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/Library >/dev/null; test -f Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/Library/publish/libVoid259NativeHeapFunctionPointerIntegration.a; echo True
	@c=Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/Library/.void/Void259NativeHeapFunctionPointerIntegration.c; grep -Fq 'extern int32_t voidc259_drive(int32_t (*)(int32_t, int32_t), int32_t (*)(int32_t), int32_t);' $$c && grep -Fq 'extern int32_t voidc259_block_sum(int32_t *, int32_t);' $$c; echo True
	@c=Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/Library/.void/Void259NativeHeapFunctionPointerIntegration.c; grep -Fq 'vc_native_fn_call_' $$c && grep -Fq 'vc_gc_native_call_begin();' $$c && grep -Fq 'vc_gc_native_call_end(vc_native_was_safe);' $$c; echo True
	@c=Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/Library/.void/Void259NativeHeapFunctionPointerIntegration.c; grep -Fq 'vc_native_memory_try_allocate_elements_u64' $$c && grep -Fq 'vc_native_memory_free' $$c && grep -Fq 'vc_native_memory_try_reallocate_aligned_elements_u64' $$c; echo True
	@c=Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/Library/.void/Void259NativeHeapFunctionPointerIntegration.c; grep -Fq 'vc_native_capture_exception(vc_exception, vc_boundary.fault_site, vc_boundary.captured_start, vc_boundary.captured_count);' $$c && grep -Fq 'vc_native_take_pending_exception(&vc_pending_site, &vc_pending_start, &vc_pending_count)' $$c && grep -Fq 'volatile bool vc_callback_thread_attached = false;' $$c; echo True
	@! grep -Eq 'vc_gc_|VcGc' Runtime/src/vc_memory.c && echo True
	@grep -Fq 'A leaf return with no active cleanup state does not need a named' Compiler/src/compiler.c && ! grep -Fq 'Wno-clobbered' Makefile; echo True
	@./$(BIN) check Tests/GenericFunctionPointerUnmanagedStorageIntegration >/dev/null && echo True
	@./$(BIN) check Tests/PinningGcThreadsExceptionsNativeBoundaryIntegration/Library >/dev/null && echo True
	@./$(BIN) check Tests/ThreadExceptionNativeBoundaryCleanup >/dev/null && echo True
	@grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c && grep -Fq '"version": "0.0.259"' Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/Library/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration.voidproj && grep -Fq '"version": "0.0.258"' Tests/GenericFunctionPointerUnmanagedStorageIntegration/GenericFunctionPointerUnmanagedStorageIntegration.voidproj; echo True
	@grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py && grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-native-heap-function-pointer-integration-audit

# Milestone 260: Native Heap & Function Pointer Integration Audit
test-native-heap-function-pointer-integration-audit: $(BIN) $(NATIVE_HEAP_FUNCTION_POINTER_AUDIT_FIXTURE)
	@set -e; ./$(BIN) check Tests/NativeHeapFunctionPointerIntegrationAudit/Surface >/dev/null; echo True
	@./$(BIN) build Tests/NativeHeapFunctionPointerIntegrationAudit/Surface >/dev/null
	@./Tests/NativeHeapFunctionPointerIntegrationAudit/Surface/bin/NativeHeapFunctionPointerIntegrationAudit
	@set -e; ./$(BIN) publish Tests/NativeHeapFunctionPointerIntegrationAudit/Surface >/dev/null; test -x Tests/NativeHeapFunctionPointerIntegrationAudit/Surface/publish/NativeHeapFunctionPointerIntegrationAudit$(EXE_SUFFIX); echo True
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 Tests/NativeHeapFunctionPointerIntegrationAudit/native_memory_contract_test.c Runtime/src/vc_memory.c -o $(NATIVE_HEAP_FUNCTION_POINTER_AUDIT_MEMORY_TEST)
	@./$(NATIVE_HEAP_FUNCTION_POINTER_AUDIT_MEMORY_TEST)
	@set -e; ./$(BIN) check Tests/NativeHeapFunctionPointerIntegrationAudit/Library >/dev/null; echo True
	@set -e; ./$(BIN) build Tests/NativeHeapFunctionPointerIntegrationAudit/Library >/dev/null; test -f Tests/NativeHeapFunctionPointerIntegrationAudit/Library/bin/libVoid260NativeHeapFunctionPointerAudit.a; echo True
	@ar t Tests/NativeHeapFunctionPointerIntegrationAudit/Library/bin/libVoid260NativeHeapFunctionPointerAudit.a | grep -Fq 'vc_memory_runtime.o'; echo True
	@ar t Tests/NativeHeapFunctionPointerIntegrationAudit/Library/bin/libVoid260NativeHeapFunctionPointerAudit.a | grep -Fq 'vc_thread_runtime.o'; echo True
	@$(CC) -IRuntime/include -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -pthread Tests/NativeHeapFunctionPointerIntegrationAudit/CConsumer/main.c Runtime/src/vc_thread.c Runtime/src/vc_atomic.c Runtime/src/vc_memory.c -o $(NATIVE_HEAP_FUNCTION_POINTER_AUDIT_TEST)
	@./$(NATIVE_HEAP_FUNCTION_POINTER_AUDIT_TEST)
	@set -e; ./$(BIN) publish Tests/NativeHeapFunctionPointerIntegrationAudit/Library >/dev/null; test -f Tests/NativeHeapFunctionPointerIntegrationAudit/Library/publish/libVoid260NativeHeapFunctionPointerAudit.a; echo True
	@set -e; c=Tests/NativeHeapFunctionPointerIntegrationAudit/Surface/.void/NativeHeapFunctionPointerIntegrationAudit.c; grep -Fq 'typedef int32_t (*vc_fnptr_0)(int32_t, int32_t);' $$c; grep -Fq 'vc_fnptr_0 * vc_l_' $$c; grep -Fq 'sizeof(vc_fnptr_0)' $$c; ! grep -Eq 'vc_fnptr_0[^;]*void ?\*|void ?\*[^;]*vc_fnptr_0' $$c; echo True
	@set -e; c=Tests/NativeHeapFunctionPointerIntegrationAudit/Surface/.void/NativeHeapFunctionPointerIntegrationAudit.c; grep -Fq 'Span__g1_delegate__x2a__g3_int_int_int' $$c; grep -Fq 'ReadOnlySpan__g1_delegate__x2a__g3_int_int_int' $$c; grep -Fq 'Span__g1_AuditEntry260' $$c; echo True
	@set -e; c=Tests/NativeHeapFunctionPointerIntegrationAudit/Library/.void/Void260NativeHeapFunctionPointerAudit.c; grep -Fq 'extern int32_t voidc260_audit_drive(vc_fnptr_0, int32_t (*)(int32_t), int32_t);' $$c; grep -Fq 'extern int32_t voidc260_audit_block_sum(int32_t *, int32_t);' $$c; echo True
	@set -e; c=Tests/NativeHeapFunctionPointerIntegrationAudit/Library/.void/Void260NativeHeapFunctionPointerAudit.c; grep -Fq 'vc_native_fn_call_' $$c; grep -Fq 'vc_gc_native_call_begin();' $$c; grep -Fq 'vc_gc_native_call_end(vc_native_was_safe);' $$c; echo True
	@set -e; c=Tests/NativeHeapFunctionPointerIntegrationAudit/Library/.void/Void260NativeHeapFunctionPointerAudit.c; grep -Fq 'vc_native_memory_try_allocate_elements_u64' $$c; grep -Fq 'vc_native_memory_try_allocate_aligned_elements_u64' $$c; grep -Fq 'vc_native_memory_free' $$c; echo True
	@set -e; c=Tests/NativeHeapFunctionPointerIntegrationAudit/Library/.void/Void260NativeHeapFunctionPointerAudit.c; grep -Fq 'vc_native_capture_exception(vc_exception, vc_boundary.fault_site, vc_boundary.captured_start, vc_boundary.captured_count);' $$c; grep -Fq 'vc_native_take_pending_exception(&vc_pending_site, &vc_pending_start, &vc_pending_count)' $$c; grep -Fq 'volatile bool vc_callback_thread_attached = false;' $$c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeHeapFunctionPointerIntegrationAuditDiagnostics/ManagedNativeAllocation >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type argument 'string' must be an unmanaged type for generic parameter 'T'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeHeapFunctionPointerIntegrationAuditDiagnostics/SafeFunctionPointerGeneric >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function pointer type arguments require an unsafe method' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeHeapFunctionPointerIntegrationAuditDiagnostics/FunctionToDataPointer >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "cannot explicitly cast 'delegate*<int, int>' to 'void*'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeHeapFunctionPointerIntegrationAuditDiagnostics/ByRefExternFunctionPointer >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function pointer extern parameters must be passed by value' $$out; rm -f $$out; echo True
	@set -e; grep -Fq 'if (size == 0u)' Runtime/src/vc_memory.c; grep -Fq 'void *replacement = NULL;' Runtime/src/vc_memory.c; grep -Fq 'vc_native_memory_free(original);' Runtime/src/vc_memory.c; grep -Fq '*memory = replacement;' Runtime/src/vc_memory.c; echo True
	@set -e; grep -Fq 'alignment != 0u && (alignment & (alignment - 1u)) == 0u' Runtime/src/vc_memory.c; grep -Fq 'VcNativeMemoryHeader' Runtime/src/vc_memory.c; ! grep -Eq '(^|[^A-Za-z0-9_])realloc\(' Runtime/src/vc_memory.c; ! grep -Eq 'aligned_alloc|posix_memalign|_aligned_malloc' Runtime/src/vc_memory.c; echo True
	@set -e; ! grep -Eq 'vc_gc_|VcGc' Runtime/src/vc_memory.c; grep -Fq 'VC_SEM_TYPE_FUNCTION_POINTER_BASE' Compiler/include/semantic.h; grep -Fq 'type >= VC_SEM_TYPE_POINTER_BASE && type < VC_SEM_TYPE_FUNCTION_POINTER_BASE' Compiler/src/semantic.c; grep -Fq 'type >= VC_SEM_TYPE_FUNCTION_POINTER_BASE' Compiler/src/semantic.c; echo True
	@set -e; grep -Fq 'native function pointer type arguments require an unsafe method' Compiler/src/semantic.c; grep -Fq 'native function pointer extern parameters must be passed by value' Compiler/src/semantic.c; grep -Fq 'had_explicit_function_pointer_type_argument' Compiler/include/ast.h; echo True
	@set -e; grep -Fq 'A leaf return with no active cleanup state does not need a named' Compiler/src/compiler.c; grep -Fq 'static VC_MAYBE_UNUSED %s *%s(' Compiler/src/compiler.c; ! grep -Fq 'Wno-clobbered' Makefile; echo True
	@./$(BIN) check Tests/NativeHeapRuntimeFoundation >/dev/null && echo True
	@./$(BIN) check Tests/NativeMemoryAllocationSurface >/dev/null && echo True
	@./$(BIN) check Tests/GenericTypedNativeAllocationCompletion >/dev/null && echo True
	@./$(BIN) check Tests/NativeReallocationAlignmentCompletion >/dev/null && echo True
	@./$(BIN) check Tests/NativeFunctionPointerTypeFoundation >/dev/null && echo True
	@./$(BIN) check Tests/NativeFunctionAddressIndirectCallCompletion >/dev/null && echo True
	@./$(BIN) check Tests/NativeFunctionPointerAbiIntegration >/dev/null && echo True
	@./$(BIN) check Tests/GenericFunctionPointerUnmanagedStorageIntegration >/dev/null && echo True
	@./$(BIN) check Tests/NativeHeapFunctionPointersGcThreadsExceptionBoundaryIntegration/Library >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.260"' Tests/NativeHeapFunctionPointerIntegrationAudit/Surface/NativeHeapFunctionPointerIntegrationAudit.voidproj; grep -Fq '"version": "0.0.260"' Tests/NativeHeapFunctionPointerIntegrationAudit/Library/NativeHeapFunctionPointerIntegrationAudit.voidproj; echo True
	@set -e; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-awaiter-protocol-semantic-foundation

# Milestone 261: Awaiter Protocol Semantic Foundation
test-awaiter-protocol-semantic-foundation: $(BIN)
	@$(CC) $(CPPFLAGS) $(CFLAGS) Tests/AwaiterProtocolSemanticFoundation/semantic_test.c Compiler/src/ast.c Compiler/src/diagnostic.c Compiler/src/lexer.c Compiler/src/parser.c Compiler/src/semantic.c Compiler/src/monomorph.c -o $(AWAITER_PROTOCOL_SEMANTIC_TEST)
	@./$(AWAITER_PROTOCOL_SEMANTIC_TEST)
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterProtocolSemanticFoundationDiagnostics/MissingGetAwaiter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "awaitable type 'BadAwaitable261' must provide an unambiguous GetAwaiter() method" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterProtocolSemanticFoundationDiagnostics/GetAwaiterPrimitive >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "GetAwaiter() must return an awaiter type, got 'int'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterProtocolSemanticFoundationDiagnostics/MissingIsCompleted >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "awaiter type 'BadAwaiter261' must provide a bool IsCompleted property" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterProtocolSemanticFoundationDiagnostics/IsCompletedField >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "awaiter type 'BadAwaiter261' must provide a bool IsCompleted property" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterProtocolSemanticFoundationDiagnostics/InaccessibleIsCompleted >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'awaiter IsCompleted getter is inaccessible' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterProtocolSemanticFoundationDiagnostics/WrongIsCompletedType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'awaiter IsCompleted property must return bool by value' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterProtocolSemanticFoundationDiagnostics/MissingOnCompleted >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "awaiter type 'BadAwaiter261' must provide an unambiguous void OnCompleted(Action) method" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterProtocolSemanticFoundationDiagnostics/WrongOnCompletedParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "awaiter type 'BadAwaiter261' must provide an unambiguous void OnCompleted(Action) method" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterProtocolSemanticFoundationDiagnostics/WrongOnCompletedReturn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'awaiter OnCompleted(Action) must return void' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterProtocolSemanticFoundationDiagnostics/MissingGetResult >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "awaiter type 'BadAwaiter261' must provide an unambiguous GetResult() method" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterProtocolSemanticFoundationDiagnostics/InaccessibleGetAwaiter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'GetAwaiter() is inaccessible' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterProtocolSemanticFoundationDiagnostics/InaccessibleGetResult >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'GetResult() is inaccessible' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/AsyncAwaitSyntaxSemanticFoundation >/dev/null && echo True
	@./$(BIN) run Tests/AwaitSuspensionResumptionCompletion >/dev/null && echo True
	@set -e; grep -Fq 'bool has_await_protocol;' Compiler/include/semantic.h; grep -Fq 'await_get_awaiter_method_index' Compiler/include/semantic.h; grep -Fq 'await_get_result_method_index' Compiler/include/semantic.h; echo True
	@set -e; grep -Fq 'analyze_await_protocol' Compiler/src/semantic.c; grep -Fq 'GetAwaiter' Compiler/src/semantic.c; grep -Fq 'IsCompleted' Compiler/src/semantic.c; grep -Fq 'OnCompleted' Compiler/src/semantic.c; grep -Fq 'GetResult' Compiler/src/semantic.c; echo True
	@set -e; ! grep -Fq 'semantic_task_type_info(context->model, awaited' Compiler/src/semantic.c; grep -Fq 'analyze_await_protocol(context, expression, awaited)' Compiler/src/semantic.c; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; echo True
	@set -e; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-general-awaiter-state-machine-lowering

# Milestone 262: General Awaiter State-Machine Lowering
test-general-awaiter-state-machine-lowering: $(BIN)
	@./$(BIN) check Tests/GeneralAwaiterStateMachineLowering >/dev/null && echo True
	@./$(BIN) build Tests/GeneralAwaiterStateMachineLowering >/dev/null
	@./Tests/GeneralAwaiterStateMachineLowering/bin/GeneralAwaiterStateMachineLowering
	@set -e; ./$(BIN) publish Tests/GeneralAwaiterStateMachineLowering >/dev/null; test -x Tests/GeneralAwaiterStateMachineLowering/publish/GeneralAwaiterStateMachineLowering$(EXE_SUFFIX); echo True
	@set -e; grep -Fq 'const VcSemanticBinding *protocol_binding;' Compiler/src/async_lower.c; grep -Fq 'ResumeAwaiter' Compiler/src/async_lower.c; ! grep -Fq 'protocol_await ? "ResumeAwaiter" : "Resume"' Compiler/src/async_lower.c; echo True
	@set -e; grep -Fq 'await_get_awaiter_method_index' Compiler/src/async_lower.c; grep -Fq 'await_is_completed_property_index' Compiler/src/async_lower.c; grep -Fq 'await_on_completed_method_index' Compiler/src/async_lower.c; grep -Fq 'await_get_result_method_index' Compiler/src/async_lower.c; echo True
	@set -e; grep -Fq 'make_state_assignment_statement(context, source->location, source->target)' Compiler/src/async_lower.c; grep -Fq 'named_type(context->tree, source->location, "Action")' Compiler/src/async_lower.c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterProtocolSemanticFoundationDiagnostics/MissingOnCompleted >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "must provide an unambiguous void OnCompleted(Action) method" $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/AwaitSuspensionResumptionCompletion >/dev/null && echo True
	@./$(BIN) check Tests/AsyncAwaitTaskRuntimeIntegrationAudit >/dev/null && echo True
	@set -e; ! grep -R -Fq 'AwaitProbe262\|SyncIntAwaiter262\|AsyncIntAwaiter262' Compiler Runtime StandardLibrary; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.262"' Tests/GeneralAwaiterStateMachineLowering/GeneralAwaiterStateMachineLowering.voidproj; echo True
	@set -e; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-task-awaiter-task-de-specialization

# Milestone 263: TaskAwaiter & Task Await De-Specialization
test-task-awaiter-task-de-specialization: $(BIN)
	@./$(BIN) check Tests/TaskAwaiterTaskDeSpecialization >/dev/null && echo True
	@./$(BIN) build Tests/TaskAwaiterTaskDeSpecialization >/dev/null
	@./Tests/TaskAwaiterTaskDeSpecialization/bin/TaskAwaiterTaskDeSpecialization
	@set -e; ./$(BIN) publish Tests/TaskAwaiterTaskDeSpecialization >/dev/null; test -x Tests/TaskAwaiterTaskDeSpecialization/publish/TaskAwaiterTaskDeSpecialization$(EXE_SUFFIX); echo True
	@set -e; grep -Fq 'public readonly struct TaskAwaiter' StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public readonly struct TaskAwaiter<T>' StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'RegisterAwaitContinuation(Action continuation)' StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq '_task.RegisterAwaitContinuation(continuation);' StandardLibrary/Void/Threading/Tasks/Task.void; echo True
	@set -e; ! grep -Fq 'semantic_task_type_info(context->model, awaited' Compiler/src/semantic.c; grep -Fq 'type = analyze_await_protocol(context, expression, awaited);' Compiler/src/semantic.c; echo True
	@set -e; ! grep -Fq '"ContinueWith"' Compiler/src/async_lower.c; ! grep -Fq '"Wait"' Compiler/src/async_lower.c; ! grep -Fq '"Result"' Compiler/src/async_lower.c; ! grep -Fq 'task_type_from_semantic' Compiler/src/async_lower.c; ! grep -Fq 'make_resume_method' Compiler/src/async_lower.c; echo True
	@set -e; grep -Fq 'add_bound_async_await' Compiler/src/async_lower.c; grep -Fq 'await_get_awaiter_method_index' Compiler/src/async_lower.c; grep -Fq 'await_on_completed_method_index' Compiler/src/async_lower.c; grep -Fq 'await_get_result_method_index' Compiler/src/async_lower.c; ! grep -Fq 'compile_async_known_await' Compiler/src/async_lower.c; echo True
	@./$(BIN) run Tests/GeneralAwaiterStateMachineLowering >/dev/null && echo True
	@./$(BIN) run Tests/AsyncTaskTypedAwaitResults >/dev/null && echo True
	@./$(BIN) run Tests/AwaitForeachSyntaxLoweringCompletion >/dev/null && echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterProtocolSemanticFoundationDiagnostics/MissingOnCompleted >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'must provide an unambiguous void OnCompleted(Action) method' $$out; rm -f $$out; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.263"' Tests/TaskAwaiterTaskDeSpecialization/TaskAwaiterTaskDeSpecialization.voidproj; grep -Fq '"version": "0.0.262"' Tests/GeneralAwaiterStateMachineLowering/GeneralAwaiterStateMachineLowering.voidproj; echo True
	@set -e; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-generic-struct-extension-awaiter-completion

# Milestone 264: Generic, Struct & Extension Awaiter Completion
test-generic-struct-extension-awaiter-completion: $(BIN)
	@./$(BIN) check Tests/GenericStructExtensionAwaiterCompletion >/dev/null && echo True
	@./$(BIN) build Tests/GenericStructExtensionAwaiterCompletion >/dev/null
	@./Tests/GenericStructExtensionAwaiterCompletion/bin/GenericStructExtensionAwaiterCompletion
	@set -e; ./$(BIN) publish Tests/GenericStructExtensionAwaiterCompletion >/dev/null; test -x Tests/GenericStructExtensionAwaiterCompletion/publish/GenericStructExtensionAwaiterCompletion$(EXE_SUFFIX); echo True
	@set -e; grep -Fq 'VcAstNode *awaiter_inference_call;' Compiler/include/ast.h; grep -Fq 'node->as.await_expression.awaiter_inference_call' Compiler/src/monomorph.c; grep -Fq 'request_extension_generic_method_inference(context, inference_call' Compiler/src/semantic.c; echo True
	@set -e; grep -Fq 'await_get_awaiter_extension_method' Compiler/include/semantic.h; grep -Fq 'resolve_extension_method_call(context, get_awaiter_lookup' Compiler/src/semantic.c; grep -Fq "ref struct awaiter type '%s' cannot be stored across async suspension" Compiler/src/semantic.c; echo True
	@set -e; ! grep -R -Fq 'GenericAwaiter264\|StatefulStructAwaiter264\|AwaitExtensions264' Compiler Runtime StandardLibrary; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/GenericStructExtensionAwaiterCompletionDiagnostics/RefStructAwaiter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct awaiter type 'RefAwaiter264' cannot be stored across async suspension" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/GenericStructExtensionAwaiterCompletionDiagnostics/AmbiguousExtension >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "has ambiguous extension GetAwaiter() methods" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/GenericStructExtensionAwaiterCompletionDiagnostics/MissingExtensionNamespace >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "must provide an unambiguous GetAwaiter() method" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/GenericStructExtensionAwaiterCompletionDiagnostics/UninferableGenericExtension >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "generic extension GetAwaiter() type arguments could not be inferred" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/GenericStructExtensionAwaiterCompletionDiagnostics/UnsafeGetAwaiter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "unsafe GetAwaiter() requires an unsafe method" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/GenericStructExtensionAwaiterCompletionDiagnostics/UnsafeOnCompleted >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "unsafe OnCompleted(Action) requires an unsafe method" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/GenericStructExtensionAwaiterCompletionDiagnostics/UnsafeGetResult >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "unsafe GetResult() requires an unsafe method" $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/TaskAwaiterTaskDeSpecialization >/dev/null && echo True
	@./$(BIN) run Tests/GeneralAwaiterStateMachineLowering >/dev/null && echo True
	@./$(BIN) run Tests/ExtensionMethodCompletion >/dev/null && echo True
	@./$(BIN) run Tests/GenericMethodTypeInference >/dev/null && echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterProtocolSemanticFoundationDiagnostics/MissingOnCompleted >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'must provide an unambiguous void OnCompleted(Action) method' $$out; rm -f $$out; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.264"' Tests/GenericStructExtensionAwaiterCompletion/GenericStructExtensionAwaiterCompletion.voidproj; grep -Fq '"version": "0.0.263"' Tests/TaskAwaiterTaskDeSpecialization/TaskAwaiterTaskDeSpecialization.voidproj; echo True
	@set -e; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-task-yield-allocation-free-yield-awaitable

# Milestone 265: Task.Yield / Allocation-Free Yield Awaitable
test-task-yield-allocation-free-yield-awaitable: $(BIN)
	@./$(BIN) check Tests/TaskYieldAllocationFreeYieldAwaitable >/dev/null && echo True
	@./$(BIN) build Tests/TaskYieldAllocationFreeYieldAwaitable >/dev/null
	@./Tests/TaskYieldAllocationFreeYieldAwaitable/bin/TaskYieldAllocationFreeYieldAwaitable
	@set -e; ./$(BIN) publish Tests/TaskYieldAllocationFreeYieldAwaitable >/dev/null; test -x Tests/TaskYieldAllocationFreeYieldAwaitable/publish/TaskYieldAllocationFreeYieldAwaitable$(EXE_SUFFIX); echo True
	@set -e; grep -Fq 'public static YieldAwaitable Yield()' StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public readonly struct YieldAwaitable' StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public readonly struct YieldAwaiter' StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public bool IsCompleted => false;' StandardLibrary/Void/Threading/Tasks/Task.void; echo True
	@set -e; block=$$(sed -n '/public static YieldAwaitable Yield()/,/^    }/p' StandardLibrary/Void/Threading/Tasks/Task.void); printf '%s' "$$block" | grep -Fq 'return default;'; ! printf '%s' "$$block" | grep -Fq 'new Task'; grep -Fq 'ThreadPool.QueueTaskContinuation(continuation);' StandardLibrary/Void/Threading/Tasks/Task.void; ! grep -R -Eq 'TaskYieldAllocationFreeYieldAwaitable|YieldState265|YieldPayload265|YieldAwaitable|Task\.Yield' Compiler Runtime; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/TaskYieldAllocationFreeYieldAwaitableRuntime/NullContinuation >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: yield continuation cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/GenericStructExtensionAwaiterCompletion >/dev/null && echo True
	@./$(BIN) run Tests/TaskAwaiterTaskDeSpecialization >/dev/null && echo True
	@./$(BIN) run Tests/GeneralAwaiterStateMachineLowering >/dev/null && echo True
	@./$(BIN) run Tests/AsyncAwaitTaskRuntimeIntegrationAudit >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.265"' Tests/TaskYieldAllocationFreeYieldAwaitable/TaskYieldAllocationFreeYieldAwaitable.voidproj; grep -Fq '"version": "0.0.264"' Tests/GenericStructExtensionAwaiterCompletion/GenericStructExtensionAwaiterCompletion.voidproj; echo True
	@set -e; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-value-task-foundation

# Milestone 266: ValueTask Foundation
test-value-task-foundation: $(BIN)
	@./$(BIN) check Tests/ValueTaskFoundation >/dev/null && echo True
	@./$(BIN) build Tests/ValueTaskFoundation >/dev/null
	@./Tests/ValueTaskFoundation/bin/ValueTaskFoundation
	@set -e; ./$(BIN) publish Tests/ValueTaskFoundation >/dev/null; test -x Tests/ValueTaskFoundation/publish/ValueTaskFoundation$(EXE_SUFFIX); echo True
	@set -e; grep -Fq 'public readonly struct ValueTask' StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'private readonly Task _task;' StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public bool IsCompleted => _task == null || _task.IsCompleted;' StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public readonly struct ValueTaskAwaiter' StandardLibrary/Void/Threading/Tasks/Task.void; echo True
	@set -e; ! grep -R -Eq 'ValueTaskFoundation|ValueTaskAwaiter|ValueTask' Compiler Runtime; grep -Fq 'ThreadPool.QueueTaskContinuation(continuation);' StandardLibrary/Void/Threading/Tasks/Task.void; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/ValueTaskFoundationRuntime/NullBackingTask >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: value task backing task cannot be null' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/ValueTaskFoundationRuntime/NullContinuation >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: value task await continuation cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/TaskYieldAllocationFreeYieldAwaitable >/dev/null && echo True
	@./$(BIN) run Tests/GenericStructExtensionAwaiterCompletion >/dev/null && echo True
	@./$(BIN) run Tests/TaskAwaiterTaskDeSpecialization >/dev/null && echo True
	@./$(BIN) run Tests/AsyncAwaitTaskRuntimeIntegrationAudit >/dev/null && echo True
	@./$(BIN) run Tests/AsyncCancellationIntegration >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.266"' Tests/ValueTaskFoundation/ValueTaskFoundation.voidproj; grep -Fq '"version": "0.0.265"' Tests/TaskYieldAllocationFreeYieldAwaitable/TaskYieldAllocationFreeYieldAwaitable.voidproj; echo True
	@set -e; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-generic-value-task-result-gc-completion

# Milestone 267: ValueTask<T> Result & GC Completion
test-generic-value-task-result-gc-completion: $(BIN)
	@./$(BIN) check Tests/GenericValueTaskResultGcCompletion >/dev/null && echo True
	@./$(BIN) build Tests/GenericValueTaskResultGcCompletion >/dev/null
	@./Tests/GenericValueTaskResultGcCompletion/bin/GenericValueTaskResultGcCompletion
	@set -e; ./$(BIN) publish Tests/GenericValueTaskResultGcCompletion >/dev/null; test -x Tests/GenericValueTaskResultGcCompletion/publish/GenericValueTaskResultGcCompletion$(EXE_SUFFIX); echo True
	@set -e; grep -Fq 'public readonly struct ValueTask<T>' StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'private readonly T _result;' StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'private readonly Task<T> _task;' StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public readonly struct ValueTaskAwaiter<T>' StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public T GetResult()' StandardLibrary/Void/Threading/Tasks/Task.void; echo True
	@set -e; ! grep -R -Eq 'GenericValueTaskResultGcCompletion|ValueTaskAwaiter<|ValueTask<' Compiler Runtime; grep -Fq 'ValueTask<Payload267>' Tests/GenericValueTaskResultGcCompletion/Program.void; grep -Fq 'GC.Collect();' Tests/GenericValueTaskResultGcCompletion/Program.void; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/GenericValueTaskResultGcCompletionRuntime/NullBackingTask >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: value task backing task cannot be null' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/GenericValueTaskResultGcCompletionRuntime/NullContinuation >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: value task await continuation cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/ValueTaskFoundation >/dev/null && echo True
	@./$(BIN) run Tests/TaskAwaiterTaskDeSpecialization >/dev/null && echo True
	@./$(BIN) run Tests/GenericStructExtensionAwaiterCompletion >/dev/null && echo True
	@./$(BIN) run Tests/AsyncAwaitTaskRuntimeIntegrationAudit >/dev/null && echo True
	@./$(BIN) run Tests/AsyncCancellationIntegration >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.267"' Tests/GenericValueTaskResultGcCompletion/GenericValueTaskResultGcCompletion.voidproj; grep -Fq '"version": "0.0.266"' Tests/ValueTaskFoundation/ValueTaskFoundation.voidproj; echo True
	@set -e; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-async-value-task-return-integration

# Milestone 268: Async ValueTask Return Integration
test-async-value-task-return-integration: $(BIN)
	@./$(BIN) check Tests/AsyncValueTaskReturnIntegration >/dev/null && echo True
	@./$(BIN) build Tests/AsyncValueTaskReturnIntegration >/dev/null
	@./Tests/AsyncValueTaskReturnIntegration/bin/AsyncValueTaskReturnIntegration
	@set -e; ./$(BIN) publish Tests/AsyncValueTaskReturnIntegration >/dev/null; test -x Tests/AsyncValueTaskReturnIntegration/publish/AsyncValueTaskReturnIntegration$(EXE_SUFFIX); echo True
	@set -e; grep -Fq 'bool async_returns_value_task;' Compiler/include/semantic.h; grep -Fq 'semantic_value_task_type_info' Compiler/src/semantic.c; grep -Fq 'method.async_returns_value_task = !returns_task;' Compiler/src/semantic.c; echo True
	@set -e; grep -Fq 'context->semantic_method->async_returns_value_task' Compiler/src/async_lower.c; grep -Fq 'new_expression_with_type' Compiler/src/async_lower.c; grep -Fq 'TaskCompletionSource' Compiler/src/async_lower.c; ! grep -Rq 'ValueTaskCompletionSource' Compiler Runtime StandardLibrary; echo True
	@set -e; c=Tests/AsyncValueTaskReturnIntegration/.void/AsyncValueTaskReturnIntegration.c; grep -Fq 'TaskCompletionSource' $$c; grep -Fq 'ValueTask' $$c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AsyncValueTaskReturnIntegrationDiagnostics/ValueReturn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'async ValueTask method cannot return a value' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AsyncValueTaskReturnIntegrationDiagnostics/MissingResult >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "async ValueTask<T> method must return a value of type 'int'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AsyncAwaitSyntaxSemanticDiagnostics/AsyncTaskReturnValue >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'async Task method cannot return a value' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/GenericValueTaskResultGcCompletion >/dev/null && echo True
	@./$(BIN) run Tests/ValueTaskFoundation >/dev/null && echo True
	@./$(BIN) run Tests/TaskAwaiterTaskDeSpecialization >/dev/null && echo True
	@./$(BIN) run Tests/GeneralAwaiterStateMachineLowering >/dev/null && echo True
	@./$(BIN) run Tests/AsyncAwaitTaskRuntimeIntegrationAudit >/dev/null && echo True
	@./$(BIN) run Tests/AsyncCancellationIntegration >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.268"' Tests/AsyncValueTaskReturnIntegration/AsyncValueTaskReturnIntegration.voidproj; grep -Fq '"version": "0.0.267"' Tests/GenericValueTaskResultGcCompletion/GenericValueTaskResultGcCompletion.voidproj; echo True
	@set -e; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-awaiter-valuetask-gc-threads-exceptions-async-iteration-integration

# Milestone 269: Awaiter/ValueTask GC, Threads, Exceptions & Async-Iteration Integration
test-awaiter-valuetask-gc-threads-exceptions-async-iteration-integration: $(BIN)
	@./$(BIN) check Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration >/dev/null && echo True
	@./$(BIN) build Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration >/dev/null
	@./Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/bin/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration
	@set -e; ./$(BIN) publish Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration >/dev/null; test -x Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/publish/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration$(EXE_SUFFIX); echo True
	@set -e; c=Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/.void/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration.c; grep -Fq 'ValueTaskAwaiter' $$c; grep -Fq 'TaskAwaiter' $$c; echo True
	@set -e; grep -Fq 'analyze_await_protocol(' Compiler/src/semantic.c; grep -Fq 'move_next_await_binding' Compiler/src/async_lower.c; grep -Fq 'dispose_await_binding' Compiler/src/async_lower.c; ! grep -Fq 'semantic_async_iteration_completion_type_info' Compiler/src/semantic.c; ! grep -Fq 'move_next_value_task' Compiler/src/async_lower.c; ! grep -Fq 'dispose_value_task' Compiler/src/async_lower.c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegrationDiagnostics/WrongValueTaskMoveNextResult >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "MoveNextAsync() awaitable GetResult() must return bool, got 'int'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegrationDiagnostics/WrongValueTaskDisposeResult >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "DisposeAsync() awaitable GetResult() must return void, got 'int'" $$out; rm -f $$out; echo True
	@set -e; output="$$(VOID_GC_TRACE=1 ./Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/bin/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@./$(BIN) check Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/Library >/dev/null && echo True
	@./$(BIN) build Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/Library >/dev/null; test -f Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/Library/bin/libVoid269AwaiterValueTaskIntegration.a; echo True
	@$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/CConsumer/main.c Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/Library/bin/libVoid269AwaiterValueTaskIntegration.a -pthread -o Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/CConsumer/value-task-library-consumer
	@./Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/CConsumer/value-task-library-consumer
	@./$(BIN) publish Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/Library >/dev/null; test -f Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/Library/publish/libVoid269AwaiterValueTaskIntegration.a; echo True
	@./$(BIN) run Tests/AwaitForeachSyntaxLoweringCompletion >/dev/null && echo True
	@./$(BIN) run Tests/AsyncIteratorCompositionCompletion >/dev/null && echo True
	@./$(BIN) run Tests/AsyncIteratorCancellationIntegration >/dev/null && echo True
	@./$(BIN) run Tests/AsyncValueTaskReturnIntegration >/dev/null && echo True
	@./$(BIN) run Tests/GenericValueTaskResultGcCompletion >/dev/null && echo True
	@./$(BIN) run Tests/AsyncCancellationIntegration >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.269"' Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration.voidproj; grep -Fq '"version": "0.0.269"' Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/Library/AwaiterValueTaskGcThreadsExceptionsAsyncIterationLibrary.voidproj; echo True
	@set -e; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-generalized-awaitables-valuetask-integration-audit

# Milestone 270: Generalized Awaitables & ValueTask Integration Audit
test-generalized-awaitables-valuetask-integration-audit: $(BIN)
	@./$(BIN) check Tests/GeneralizedAwaitablesValueTaskIntegrationAudit >/dev/null && echo True
	@./$(BIN) build Tests/GeneralizedAwaitablesValueTaskIntegrationAudit >/dev/null
	@./Tests/GeneralizedAwaitablesValueTaskIntegrationAudit/bin/GeneralizedAwaitablesValueTaskIntegrationAudit
	@set -e; ./$(BIN) publish Tests/GeneralizedAwaitablesValueTaskIntegrationAudit >/dev/null; test -x Tests/GeneralizedAwaitablesValueTaskIntegrationAudit/publish/GeneralizedAwaitablesValueTaskIntegrationAudit$(EXE_SUFFIX); echo True
	@set -e; c=Tests/GeneralizedAwaitablesValueTaskIntegrationAudit/.void/GeneralizedAwaitablesValueTaskIntegrationAudit.c; grep -Fq 'MoveAwaiter270' $$c; grep -Fq 'DisposeAwaiter270' $$c; grep -Fq 'ValueTaskAwaiter' $$c; grep -Fq 'YieldAwaiter' $$c; echo True
	@set -e; grep -Fq 'await_foreach_protocol_expression' Compiler/src/semantic.c; grep -Fq 'analyze_await_protocol(' Compiler/src/semantic.c; grep -Fq 'move_next_await_binding' Compiler/src/async_lower.c; grep -Fq 'dispose_await_binding' Compiler/src/async_lower.c; grep -Fq 'compile_async_bound_await' Compiler/src/async_lower.c; ! grep -Fq 'semantic_async_iteration_completion_type_info' Compiler/src/semantic.c; ! grep -Fq 'move_next_value_task' Compiler/src/async_lower.c; ! grep -Fq 'dispose_value_task' Compiler/src/async_lower.c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/GeneralizedAwaitablesValueTaskIntegrationAuditDiagnostics/WrongMoveNextAwaiterResult >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "MoveNextAsync() awaitable GetResult() must return bool, got 'int'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/GeneralizedAwaitablesValueTaskIntegrationAuditDiagnostics/WrongDisposeAwaiterResult >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "DisposeAsync() awaitable GetResult() must return void, got 'int'" $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration >/dev/null && echo True
	@./$(BIN) run Tests/AsyncValueTaskReturnIntegration >/dev/null && echo True
	@./$(BIN) run Tests/GenericValueTaskResultGcCompletion >/dev/null && echo True
	@./$(BIN) run Tests/ValueTaskFoundation >/dev/null && echo True
	@./$(BIN) run Tests/TaskYieldAllocationFreeYieldAwaitable >/dev/null && echo True
	@./$(BIN) run Tests/GenericStructExtensionAwaiterCompletion >/dev/null && echo True
	@./$(BIN) run Tests/TaskAwaiterTaskDeSpecialization >/dev/null && echo True
	@./$(BIN) run Tests/GeneralAwaiterStateMachineLowering >/dev/null && echo True
	@./$(BIN) run Tests/AwaitForeachSyntaxLoweringCompletion >/dev/null && echo True
	@./$(BIN) run Tests/AsyncIteratorCompositionCompletion >/dev/null && echo True
	@./$(BIN) run Tests/AsyncIteratorCancellationIntegration >/dev/null && echo True
	@./$(BIN) run Tests/AsyncAwaitTaskRuntimeIntegrationAudit >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.270"' Tests/GeneralizedAwaitablesValueTaskIntegrationAudit/GeneralizedAwaitablesValueTaskIntegrationAudit.voidproj; grep -Fq '"version": "0.0.269"' Tests/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration/AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration.voidproj; echo True
	@set -e; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-async-lambda-syntax-semantic-foundation

# Milestone 271: Async Lambda Syntax & Semantic Foundation
test-async-lambda-syntax-semantic-foundation: $(BIN)
	@$(CC) $(CPPFLAGS) $(CFLAGS) Tests/AsyncLambdaSyntaxSemanticFoundationModel/semantic_test.c Compiler/src/ast.c Compiler/src/diagnostic.c Compiler/src/lexer.c Compiler/src/parser.c Compiler/src/semantic.c Compiler/src/monomorph.c -o $(ASYNC_LAMBDA_SEMANTIC_TEST)
	@./$(ASYNC_LAMBDA_SEMANTIC_TEST)
	@./$(BIN) check Tests/AsyncLambdaSyntaxSemanticFoundation >/dev/null && echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AsyncLambdaSyntaxSemanticFoundationDiagnostics/NonAwaitableTargetReturn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "async lambda target delegate must return Task, Task<T>, ValueTask, or ValueTask<T>, got 'int'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AsyncLambdaSyntaxSemanticFoundationDiagnostics/AsyncVoidTarget >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "async lambda target delegate must return Task, Task<T>, ValueTask, or ValueTask<T>, got 'void'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AsyncLambdaSyntaxSemanticFoundationDiagnostics/TaskReturnValue >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'async Task lambda cannot return a value' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AsyncLambdaSyntaxSemanticFoundationDiagnostics/TaskResultMissing >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "async Task<T> lambda must return a value of type 'int'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AsyncLambdaSyntaxSemanticFoundationDiagnostics/ValueTaskReturnValue >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'async ValueTask lambda cannot return a value' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AsyncLambdaSyntaxSemanticFoundationDiagnostics/ValueTaskResultMissing >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "async ValueTask<T> lambda must return a value of type 'int'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AsyncLambdaSyntaxSemanticFoundationDiagnostics/AwaitInNonAsyncLambda >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'await expression cannot be used inside a non-async lambda' $$out; rm -f $$out; echo True
	@set -e; grep -Fq 'bool is_async;' Compiler/include/ast.h; grep -Fq 'looks_like_lambda_at' Compiler/src/parser.c; grep -Fq 'parser->async_method_depth++' Compiler/src/parser.c; echo True
	@set -e; grep -Fq 'VcSemanticType async_result_type;' Compiler/include/semantic.h; grep -Fq 'async_returns_value_task' Compiler/include/semantic.h; grep -Fq 'async lambda target delegate must return Task, Task<T>, ValueTask, or ValueTask<T>' Compiler/src/semantic.c; echo True
	@set -e; ! grep -Fq 'lambda->is_async' Compiler/src/compiler.c; echo True
	@./$(BIN) run Tests/Lambdas >/dev/null && echo True
	@./$(BIN) check Tests/AsyncAwaitSyntaxSemanticFoundation >/dev/null && echo True
	@./$(BIN) check Tests/GeneralizedAwaitablesValueTaskIntegrationAudit >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.271"' Tests/AsyncLambdaSyntaxSemanticFoundation/AsyncLambdaSyntaxSemanticFoundation.voidproj; echo True
	@set -e; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-async-lambda-state-machine-lowering

# Milestone 272: Async Lambda State-Machine Lowering
test-async-lambda-state-machine-lowering: $(BIN)
	@./$(BIN) check Tests/AsyncLambdaStateMachineLowering >/dev/null && echo True
	@./$(BIN) build Tests/AsyncLambdaStateMachineLowering >/dev/null
	@./Tests/AsyncLambdaStateMachineLowering/bin/AsyncLambdaStateMachineLowering
	@set -e; ./$(BIN) publish Tests/AsyncLambdaStateMachineLowering >/dev/null; test -x Tests/AsyncLambdaStateMachineLowering/publish/AsyncLambdaStateMachineLowering$(EXE_SUFFIX); echo True
	@set -e; c=Tests/AsyncLambdaStateMachineLowering/.void/AsyncLambdaStateMachineLowering.c; grep -Fq '__voidc$$async$$Program$$__voidc$$async_lambda$$' $$c; grep -Fq 'WorkerIntAwaiter272' $$c; grep -Fq 'ValueTaskAwaiter' $$c; echo True
	@set -e; grep -Fq 'vc_lower_async_lambdas' Compiler/src/compiler.c; grep -Fq '__voidc$$async_lambda$$' Compiler/src/async_lower.c; grep -Fq 'VC_AST_MOD_PRIVATE | VC_AST_MOD_STATIC | VC_AST_MOD_ASYNC' Compiler/src/async_lower.c; grep -Fq 'vc_lower_async_methods' Compiler/src/compiler.c; ! grep -Fq 'lambda->is_async' Compiler/src/compiler.c; echo True
	@./$(BIN) check Tests/AsyncLambdaSyntaxSemanticFoundation >/dev/null && echo True
	@./$(BIN) run Tests/GeneralAwaiterStateMachineLowering >/dev/null && echo True
	@./$(BIN) run Tests/AsyncValueTaskReturnIntegration >/dev/null && echo True
	@./$(BIN) run Tests/Lambdas >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.272"' Tests/AsyncLambdaStateMachineLowering/AsyncLambdaStateMachineLowering.voidproj; grep -Fq '"version": "0.0.271"' Tests/AsyncLambdaSyntaxSemanticFoundation/AsyncLambdaSyntaxSemanticFoundation.voidproj; echo True
	@set -e; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-async-lambda-capture-generic-gc-lifetime-completion

# Milestone 273: Async Lambda Capture, Generic & GC Lifetime Completion
test-async-lambda-capture-generic-gc-lifetime-completion: $(BIN)
	@./$(BIN) check Tests/AsyncLambdaCaptureGenericGcLifetimeCompletion >/dev/null && echo True
	@./$(BIN) build Tests/AsyncLambdaCaptureGenericGcLifetimeCompletion >/dev/null
	@./Tests/AsyncLambdaCaptureGenericGcLifetimeCompletion/bin/AsyncLambdaCaptureGenericGcLifetimeCompletion
	@set -e; ./$(BIN) publish Tests/AsyncLambdaCaptureGenericGcLifetimeCompletion >/dev/null; test -x Tests/AsyncLambdaCaptureGenericGcLifetimeCompletion/publish/AsyncLambdaCaptureGenericGcLifetimeCompletion$(EXE_SUFFIX); echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AsyncLambdaCaptureGenericGcLifetimeCompletionDiagnostics/RefLocal >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref local 'alias' cannot be captured by a lambda" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AsyncLambdaCaptureGenericGcLifetimeCompletionDiagnostics/RefStruct >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "ref struct local 'value' cannot be captured by a lambda" $$out; rm -f $$out; echo True
	@set -e; grep -Fq 'VC_AST_COMPILER_CLOSURE_FRAME_EXPRESSION' Compiler/include/ast.h; grep -Fq 'VC_AST_COMPILER_CAPTURE_EXPRESSION' Compiler/include/ast.h; grep -Fq 'collect_async_lambda_bridge_captures' Compiler/src/async_lower.c; ! grep -Fq 'capturing async lambdas are not supported' Compiler/src/async_lower.c; echo True
	@set -e; grep -Fq '__voidc_async_closure' Compiler/src/async_lower.c; grep -Fq 'emit_compiler_capture_source' Compiler/src/compiler.c; grep -Fq 'vc_closure_find' Compiler/src/compiler.c; echo True
	@set -e; c=Tests/AsyncLambdaCaptureGenericGcLifetimeCompletion/.void/AsyncLambdaCaptureGenericGcLifetimeCompletion.c; grep -Fq '__voidc$$async$$Program$$__voidc$$async_lambda$$' $$c; grep -Fq 'vc_closure_find' $$c; grep -Fq 'GenericBox273__g1_Item273' $$c; echo True
	@set -e; output="$$(VOID_GC_TRACE=1 ./Tests/AsyncLambdaCaptureGenericGcLifetimeCompletion/bin/AsyncLambdaCaptureGenericGcLifetimeCompletion 2>&1 >/dev/null)"; printf '%s' "$$output" | grep -Eq 'VOID GC: collection [0-9]+ freed [1-9][0-9]*, live [0-9]+'; echo True
	@./$(BIN) run Tests/AsyncLambdaStateMachineLowering >/dev/null && echo True
	@./$(BIN) run Tests/AsyncGenericsClosuresGcLifetimeIntegration >/dev/null && echo True
	@./$(BIN) run Tests/Lambdas >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.273"' Tests/AsyncLambdaCaptureGenericGcLifetimeCompletion/AsyncLambdaCaptureGenericGcLifetimeCompletion.voidproj; grep -Fq '"version": "0.0.272"' Tests/AsyncLambdaStateMachineLowering/AsyncLambdaStateMachineLowering.voidproj; echo True
	@set -e; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-async-lambda-delegate-event-valuetask-integration

# Milestone 274: Async Lambda Delegate, Event & ValueTask Integration
test-async-lambda-delegate-event-valuetask-integration: $(BIN)
	@./$(BIN) check Tests/AsyncLambdaDelegateEventValueTaskIntegration >/dev/null && echo True
	@./$(BIN) build Tests/AsyncLambdaDelegateEventValueTaskIntegration >/dev/null
	@./Tests/AsyncLambdaDelegateEventValueTaskIntegration/bin/AsyncLambdaDelegateEventValueTaskIntegration
	@set -e; ./$(BIN) publish Tests/AsyncLambdaDelegateEventValueTaskIntegration >/dev/null; test -x Tests/AsyncLambdaDelegateEventValueTaskIntegration/publish/AsyncLambdaDelegateEventValueTaskIntegration$(EXE_SUFFIX); echo True
	@set -e; c=Tests/AsyncLambdaDelegateEventValueTaskIntegration/.void/AsyncLambdaDelegateEventValueTaskIntegration.c; grep -Fq 'vc_delegate_call_' $$c; grep -Fq 'ValueTaskAwaiter' $$c; grep -Fq '__voidc$$async$$Program$$__voidc$$async_lambda$$' $$c; echo True
	@set -e; grep -Fq 'callee->kind == VC_AST_CALL_EXPRESSION || callee->kind == VC_AST_INDEX_EXPRESSION' Compiler/src/semantic.c; grep -Fq 'binding.has_delegate_invoke = true' Compiler/src/semantic.c; grep -Fq 'vc_delegate_call_%zu(' Compiler/src/compiler.c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AsyncLambdaDelegateEventValueTaskIntegrationDiagnostics/VoidEventTarget >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "async lambda target delegate must return Task, Task<T>, ValueTask, or ValueTask<T>, got 'void'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AsyncLambdaDelegateEventValueTaskIntegrationDiagnostics/NonAwaitableEventTarget >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "async lambda target delegate must return Task, Task<T>, ValueTask, or ValueTask<T>, got 'int'" $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/AsyncLambdaDelegateEventValueTaskIntegration/Library >/dev/null && echo True
	@./$(BIN) build Tests/AsyncLambdaDelegateEventValueTaskIntegration/Library >/dev/null; test -f Tests/AsyncLambdaDelegateEventValueTaskIntegration/Library/bin/libVoid274AsyncLambdaDelegateEventIntegration.a; echo True
	@$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 Tests/AsyncLambdaDelegateEventValueTaskIntegration/CConsumer/main.c Tests/AsyncLambdaDelegateEventValueTaskIntegration/Library/bin/libVoid274AsyncLambdaDelegateEventIntegration.a -pthread -o Tests/AsyncLambdaDelegateEventValueTaskIntegration/CConsumer/async-lambda-delegate-event-consumer
	@./Tests/AsyncLambdaDelegateEventValueTaskIntegration/CConsumer/async-lambda-delegate-event-consumer
	@./$(BIN) publish Tests/AsyncLambdaDelegateEventValueTaskIntegration/Library >/dev/null; test -f Tests/AsyncLambdaDelegateEventValueTaskIntegration/Library/publish/libVoid274AsyncLambdaDelegateEventIntegration.a; echo True
	@./$(BIN) run Tests/AsyncLambdaCaptureGenericGcLifetimeCompletion >/dev/null && ./$(BIN) run Tests/AwaitedConditionsExpressionEvaluationCompletion >/dev/null && echo True
	@./$(BIN) run Tests/AsyncLambdaStateMachineLowering >/dev/null && echo True
	@./$(BIN) run Tests/DelegateCompletion >/dev/null && echo True
	@./$(BIN) run Tests/InterfaceEvents >/dev/null && echo True
	@./$(BIN) run Tests/CustomEventAccessors >/dev/null && echo True
	@$(MAKE) --no-print-directory test-async-object-model-library-boundary-integration >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.274"' Tests/AsyncLambdaDelegateEventValueTaskIntegration/AsyncLambdaDelegateEventValueTaskIntegration.voidproj; grep -Fq '"version": "0.0.273"' Tests/AsyncLambdaCaptureGenericGcLifetimeCompletion/AsyncLambdaCaptureGenericGcLifetimeCompletion.voidproj; echo True
	@set -e; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-await-using-syntax-structural-async-dispose-semantics

# Milestone 275: await using Syntax & Structural Async-Dispose Semantics
test-await-using-syntax-structural-async-dispose-semantics: $(BIN)
	@$(CC) $(CPPFLAGS) $(CFLAGS) Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemanticsModel/semantic_test.c Compiler/src/ast.c Compiler/src/diagnostic.c Compiler/src/lexer.c Compiler/src/parser.c Compiler/src/semantic.c Compiler/src/monomorph.c -o $(AWAIT_USING_SEMANTIC_TEST)
	@./$(AWAIT_USING_SEMANTIC_TEST)
	@set -e; out=$$(mktemp); ./$(BIN) parse Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemantics/Program.void >$$out; test "$$(grep -c 'AwaitUsingStatement' $$out)" -ge 4; test "$$(grep -c 'AwaitUsingDeclarationStatement' $$out)" -ge 4; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemanticsDiagnostics/NonAsyncMethod >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'await using may only be used inside an async method' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemanticsDiagnostics/NonAsyncLambda >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'await using cannot be used inside a non-async lambda' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemanticsDiagnostics/MissingDisposeAsync >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "must provide DisposeAsync() returning an awaitable whose GetResult() returns void" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemanticsDiagnostics/NonAwaitableDisposeResult >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "awaitable type 'int' must provide an unambiguous GetAwaiter() method" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemanticsDiagnostics/WrongGetResult >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "DisposeAsync() awaitable GetResult() must return void, got 'int'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemanticsDiagnostics/InsideLock >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'await using cannot be used inside a lock statement' $$out; rm -f $$out; echo True
	@set -e; grep -Fq 'bool is_await;' Compiler/include/ast.h; grep -Fq 'VcAstNode *dispose_await_protocol;' Compiler/include/ast.h; grep -Fq 'binding.using_async = statement->as.using_statement.is_await' Compiler/src/semantic.c; grep -Fq 'analyze_await_protocol(' Compiler/src/semantic.c; grep -Fq 'compile_async_using_dispose_await' Compiler/src/async_lower.c; echo True
	@$(MAKE) --no-print-directory test-using-statement-completion >/dev/null && echo True
	@$(MAKE) --no-print-directory test-using-declaration-completion >/dev/null && echo True
	@./$(BIN) check Tests/GeneralizedAwaitablesValueTaskIntegrationAudit >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.275"' Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemantics/AwaitUsingSyntaxStructuralAsyncDisposeSemantics.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-await-using-state-machine-structured-cleanup-completion

# Milestone 276: await using State-Machine & Structured Cleanup Completion
test-await-using-state-machine-structured-cleanup-completion: $(BIN)
	@./$(BIN) check Tests/AwaitUsingStateMachineStructuredCleanupCompletion >/dev/null && echo True
	@./$(BIN) build Tests/AwaitUsingStateMachineStructuredCleanupCompletion >/dev/null
	@./Tests/AwaitUsingStateMachineStructuredCleanupCompletion/bin/AwaitUsingStateMachineStructuredCleanupCompletion
	@set -e; ./$(BIN) publish Tests/AwaitUsingStateMachineStructuredCleanupCompletion >/dev/null; test -x Tests/AwaitUsingStateMachineStructuredCleanupCompletion/publish/AwaitUsingStateMachineStructuredCleanupCompletion$(EXE_SUFFIX); echo True
	@set -e; grep -Fq 'compile_async_using_dispose_await' Compiler/src/async_lower.c; grep -Fq 'compile_async_bound_await(context, dispose_call, await_binding' Compiler/src/async_lower.c; grep -Fq 'compile_async_using_cleanup' Compiler/src/async_lower.c; ! grep -Fq 'await using state-machine lowering is not available before milestone 276' Compiler/src/async_lower.c; echo True
	@./$(BIN) check Tests/AwaitUsingSyntaxStructuralAsyncDisposeSemantics >/dev/null && echo True
	@./$(BIN) check Tests/GeneralizedAwaitablesValueTaskIntegrationAudit >/dev/null && echo True
	@./$(BIN) run Tests/AsyncExceptionCatchFinallyIntegration >/dev/null && echo True
	@$(MAKE) --no-print-directory test-using-statement-completion >/dev/null && echo True
	@./$(BIN) run Tests/AsyncValueTaskReturnIntegration >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.276"' Tests/AwaitUsingStateMachineStructuredCleanupCompletion/AwaitUsingStateMachineStructuredCleanupCompletion.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-task-completion-factories-async-task-run-unwrapping

# Milestone 277: Task Completion Factories & Async Task.Run Unwrapping
test-task-completion-factories-async-task-run-unwrapping: $(BIN) $(TASK_RUN_NATIVE_FIXTURE)
	@./$(BIN) check Tests/TaskCompletionFactoriesAsyncTaskRunUnwrapping >/dev/null && echo True
	@./$(BIN) build Tests/TaskCompletionFactoriesAsyncTaskRunUnwrapping >/dev/null
	@timeout 30s ./Tests/TaskCompletionFactoriesAsyncTaskRunUnwrapping/bin/TaskCompletionFactoriesAsyncTaskRunUnwrapping
	@set -e; ./$(BIN) publish Tests/TaskCompletionFactoriesAsyncTaskRunUnwrapping >/dev/null; test -x Tests/TaskCompletionFactoriesAsyncTaskRunUnwrapping/publish/TaskCompletionFactoriesAsyncTaskRunUnwrapping$(EXE_SUFFIX); echo True
	@set -e; f=StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public static Task CompletedTask' $$f; grep -Fq 'public static Task<T> FromResult<T>(T result)' $$f; grep -Fq 'public static Task FromException(Exception exception)' $$f; grep -Fq 'public static Task<T> FromException<T>(Exception exception)' $$f; grep -Fq 'public static Task FromCanceled(CancellationToken cancellationToken)' $$f; grep -Fq 'public static Task<T> FromCanceled<T>(CancellationToken cancellationToken)' $$f; echo True
	@set -e; f=StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public static Task Run(Func<Task> function)' $$f; grep -Fq 'public static Task<T> Run<T>(Func<Task<T>> function)' $$f; grep -Fq 'public static Task Run(Func<Task> function, CancellationToken cancellationToken)' $$f; grep -Fq 'public static Task<T> Run<T>(Func<Task<T>> function, CancellationToken cancellationToken)' $$f; grep -Fq 'RegisterContinuation(completion)' $$f; grep -Fq 'RegisterAwaitContinuation(completion)' $$f; echo True
	@set -e; block="$$(sed -n '/private static Task RunAsync(Func<Task>/,/public static YieldAwaitable Yield()/p' StandardLibrary/Void/Threading/Tasks/Task.void)"; ! printf '%s' "$$block" | grep -Eq '\.Wait\(|\.Result([^A-Za-z0-9_]|$$)'; printf '%s' "$$block" | grep -Fq 'ThreadPool.QueueUserWorkItem(workItem)'; grep -Fq 'lowered_async_forwarder' Compiler/include/ast.h; grep -Fq 'target_typed_lambda_matches_parameter' Compiler/src/semantic.c; grep -Fq 'target_typed_lambda_preference_rank' Compiler/src/semantic.c; ! grep -R -Fq 'Task.Run' Compiler/src Compiler/include; echo True
	@./$(BIN) check Tests/TaskCompletionFactoriesAsyncTaskRunUnwrappingRuntime/NullAsyncFunction >/dev/null; ./$(BIN) build Tests/TaskCompletionFactoriesAsyncTaskRunUnwrappingRuntime/NullAsyncFunction >/dev/null
	@set -e; out=$$(mktemp); if timeout 5s ./Tests/TaskCompletionFactoriesAsyncTaskRunUnwrappingRuntime/NullAsyncFunction/bin/NullAsyncFunction >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task async function cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/TaskCompletionFactoriesAsyncTaskRunUnwrappingRuntime/UncanceledFromCanceled >/dev/null; ./$(BIN) build Tests/TaskCompletionFactoriesAsyncTaskRunUnwrappingRuntime/UncanceledFromCanceled >/dev/null
	@set -e; out=$$(mktemp); if timeout 5s ./Tests/TaskCompletionFactoriesAsyncTaskRunUnwrappingRuntime/UncanceledFromCanceled/bin/UncanceledFromCanceled >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task canceled token must already be canceled' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/TaskCompletionFactoriesAsyncTaskRunUnwrappingRuntime/NullFactoryException >/dev/null; ./$(BIN) build Tests/TaskCompletionFactoriesAsyncTaskRunUnwrappingRuntime/NullFactoryException >/dev/null
	@set -e; out=$$(mktemp); if timeout 5s ./Tests/TaskCompletionFactoriesAsyncTaskRunUnwrappingRuntime/NullFactoryException/bin/NullFactoryException >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task exception cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/TaskRunThreadPoolSchedulingIntegration >/dev/null && echo True
	@./$(BIN) run Tests/TaskCancellationIntegration >/dev/null && echo True
	@./$(BIN) run Tests/AsyncLambdaStateMachineLowering >/dev/null && echo True
	@./$(BIN) run Tests/AsyncLambdaDelegateEventValueTaskIntegration >/dev/null && echo True
	@./$(BIN) run Tests/DelegateCompletion >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.277"' Tests/TaskCompletionFactoriesAsyncTaskRunUnwrapping/TaskCompletionFactoriesAsyncTaskRunUnwrapping.voidproj; echo True
	@set -e; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-task-when-all-completion

# Milestone 278: Task.WhenAll Completion
test-task-when-all-completion: $(BIN)
	@./$(BIN) check Tests/TaskWhenAllCompletion >/dev/null && echo True
	@./$(BIN) build Tests/TaskWhenAllCompletion >/dev/null
	@timeout 30s ./Tests/TaskWhenAllCompletion/bin/TaskWhenAllCompletion
	@set -e; ./$(BIN) publish Tests/TaskWhenAllCompletion >/dev/null; test -x Tests/TaskWhenAllCompletion/publish/TaskWhenAllCompletion$(EXE_SUFFIX); echo True
	@set -e; f=StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public static Task WhenAll(params Task[] tasks)' $$f; grep -Fq 'public static Task<T[]> WhenAll<T>(params Task<T>[] tasks)' $$f; grep -Fq 'private static void CompleteWhenAll(Task[] tasks, Task completion)' $$f; grep -Fq 'private static void CompleteWhenAll<T>(Task<T>[] tasks, Task<T[]> completion)' $$f; echo True
	@set -e; block="$$(sed -n '/private static void CompleteWhenAll(Task\[\] tasks/,/private static void TransferCompletion(Task source/p' StandardLibrary/Void/Threading/Tasks/Task.void)"; ! printf '%s' "$$block" | grep -Eq '\.Wait\(|\.Result([^A-Za-z0-9_]|$$)|Thread\.Sleep'; printf '%s' "$$block" | grep -Fq 'RegisterAwaitContinuation(completed)'; printf '%s' "$$block" | grep -Fq 'Status == TaskStatus.Faulted'; printf '%s' "$$block" | grep -Fq 'Status == TaskStatus.Canceled'; echo True
	@./$(BIN) build Tests/TaskWhenAllCompletionRuntime/NullArray >/dev/null
	@set -e; out=$$(mktemp); if timeout 5s ./Tests/TaskWhenAllCompletionRuntime/NullArray/bin/NullArray >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task when-all tasks cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) build Tests/TaskWhenAllCompletionRuntime/NullElement >/dev/null
	@set -e; out=$$(mktemp); if timeout 5s ./Tests/TaskWhenAllCompletionRuntime/NullElement/bin/NullElement >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task when-all task cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) build Tests/TaskWhenAllCompletionRuntime/NullGenericArray >/dev/null
	@set -e; out=$$(mktemp); if timeout 5s ./Tests/TaskWhenAllCompletionRuntime/NullGenericArray/bin/NullGenericArray >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task when-all tasks cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/TaskCompletionFactoriesAsyncTaskRunUnwrapping >/dev/null && echo True
	@./$(BIN) run Tests/GenericMethodCalls >/dev/null && echo True
	@./$(BIN) run Tests/GenericMethodTypeInference >/dev/null && echo True
	@./$(BIN) run Tests/AsyncLambdaStateMachineLowering >/dev/null && echo True
	@./$(BIN) run Tests/TaskCancellationIntegration >/dev/null && echo True
	@set -e; grep -Fq 'require_better_than_rank' Compiler/src/semantic.c; grep -Fq 'static_match_rank' Compiler/src/semantic.c; ! grep -R -Fq 'Task.WhenAll' Compiler/src Compiler/include; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.278"' Tests/TaskWhenAllCompletion/TaskWhenAllCompletion.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True



.PHONY: test-task-when-any-completion

# Milestone 279: Task.WhenAny Completion
test-task-when-any-completion: $(BIN)
	@./$(BIN) check Tests/TaskWhenAnyCompletion >/dev/null && echo True
	@./$(BIN) build Tests/TaskWhenAnyCompletion >/dev/null
	@timeout 30s ./Tests/TaskWhenAnyCompletion/bin/TaskWhenAnyCompletion
	@set -e; ./$(BIN) publish Tests/TaskWhenAnyCompletion >/dev/null; test -x Tests/TaskWhenAnyCompletion/publish/TaskWhenAnyCompletion$(EXE_SUFFIX); echo True
	@set -e; f=StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public static Task<Task> WhenAny(params Task[] tasks)' $$f; grep -Fq 'public static Task<Task<T>> WhenAny<T>(params Task<T>[] tasks)' $$f; grep -Fq 'private static void RegisterWhenAny(Task candidate, Task<Task> completion)' $$f; grep -Fq 'private static void RegisterWhenAny<T>(Task<T> candidate, Task<Task<T>> completion)' $$f; block="$$(sed -n '/private static void RegisterWhenAny(Task candidate/,/private static void TransferCompletion(Task source/p' $$f)"; ! printf '%s' "$$block" | grep -Eq '\.Wait\(|\.Result([^A-Za-z0-9_]|$$)|Thread\.Sleep|TransferCompletion'; printf '%s' "$$block" | grep -Fq 'RegisterAwaitContinuation(completed)'; printf '%s' "$$block" | grep -Fq 'TrySetResult(candidate)'; ! grep -R -Fq 'Task.WhenAny' Compiler/src Compiler/include; echo True
	@./$(BIN) build Tests/TaskWhenAnyCompletionRuntime/NullArray >/dev/null
	@set -e; out=$$(mktemp); if timeout 5s ./Tests/TaskWhenAnyCompletionRuntime/NullArray/bin/NullArray >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task when-any tasks cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) build Tests/TaskWhenAnyCompletionRuntime/NullElement >/dev/null
	@set -e; out=$$(mktemp); if timeout 5s ./Tests/TaskWhenAnyCompletionRuntime/NullElement/bin/NullElement >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task when-any task cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) build Tests/TaskWhenAnyCompletionRuntime/EmptyArray >/dev/null
	@set -e; out=$$(mktemp); if timeout 5s ./Tests/TaskWhenAnyCompletionRuntime/EmptyArray/bin/EmptyArray >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task when-any requires at least one task' $$out; rm -f $$out; echo True
	@./$(BIN) build Tests/TaskWhenAnyCompletionRuntime/NullGenericArray >/dev/null
	@set -e; out=$$(mktemp); if timeout 5s ./Tests/TaskWhenAnyCompletionRuntime/NullGenericArray/bin/NullGenericArray >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task when-any tasks cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) build Tests/TaskWhenAnyCompletionRuntime/NullGenericElement >/dev/null
	@set -e; out=$$(mktemp); if timeout 5s ./Tests/TaskWhenAnyCompletionRuntime/NullGenericElement/bin/NullGenericElement >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task when-any task cannot be null' $$out; rm -f $$out; echo True
	@./$(BIN) build Tests/TaskWhenAnyCompletionRuntime/EmptyGenericArray >/dev/null
	@set -e; out=$$(mktemp); if timeout 5s ./Tests/TaskWhenAnyCompletionRuntime/EmptyGenericArray/bin/EmptyGenericArray >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task when-any requires at least one task' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/TaskWhenAllCompletion >/dev/null && echo True
	@./$(BIN) run Tests/TaskCompletionFactoriesAsyncTaskRunUnwrapping >/dev/null && echo True
	@./$(BIN) run Tests/AwaitUsingStateMachineStructuredCleanupCompletion >/dev/null && echo True
	@./$(BIN) run Tests/AsyncLambdaStateMachineLowering >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.279"' Tests/TaskWhenAnyCompletion/TaskWhenAnyCompletion.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-async-lambdas-async-disposal-task-composition-integration-audit

# Milestone 280: Async Lambdas, Async Disposal & Task Composition Integration Audit
test-async-lambdas-async-disposal-task-composition-integration-audit: $(BIN)
	@./$(BIN) check Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit >/dev/null && echo True
	@./$(BIN) build Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit >/dev/null
	@timeout 30s ./Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/bin/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit
	@set -e; ./$(BIN) publish Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit >/dev/null; test -x Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/publish/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit$(EXE_SUFFIX); echo True
	@set -e; c=Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/.void/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit.c; grep -Fq '__voidc$$async$$Program$$__voidc$$async_lambda$$' $$c; grep -Fq 'WorkerIntAwaiter280' $$c; grep -Fq 'ValueTaskAwaiter' $$c; grep -Fq '__voidc$$async$$Program$$ResourceReturn' $$c; echo True
	@set -e; grep -Fq 'target_typed_lambda_matches_parameter' Compiler/src/semantic.c; grep -Fq 'lowered_async_forwarder' Compiler/include/ast.h; grep -Fq 'compile_async_bound_await' Compiler/src/async_lower.c; grep -Fq 'compile_async_using_dispose_await' Compiler/src/async_lower.c; grep -Fq 'compile_async_using_cleanup' Compiler/src/async_lower.c; ! grep -R -Fq 'Task.Run' Compiler/src Compiler/include; ! grep -R -Fq 'Task.WhenAll' Compiler/src Compiler/include; ! grep -R -Fq 'Task.WhenAny' Compiler/src Compiler/include; echo True
	@set -e; f=StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public static Task Run(Func<Task> function)' $$f; grep -Fq 'public static Task<T[]> WhenAll<T>(params Task<T>[] tasks)' $$f; grep -Fq 'public static Task<Task<T>> WhenAny<T>(params Task<T>[] tasks)' $$f; grep -Fq 'RegisterAwaitContinuation(completed)' $$f; echo True
	@set -e; grep -Fq 'public interface IAsyncDisposable' StandardLibrary/Void/IAsyncDisposable.void; grep -Fq 'Task DisposeAsync();' StandardLibrary/Void/IAsyncDisposable.void; grep -Fq 'public interface IAsyncEnumerator<T> : IAsyncDisposable' StandardLibrary/Void/Collections/AsyncEnumerable.void; grep -Fq 'Task<bool> MoveNextAsync();' StandardLibrary/Void/Collections/AsyncEnumerable.void; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAuditDiagnostics/AsyncVoidTarget >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "async lambda target delegate must return Task, Task<T>, ValueTask, or ValueTask<T>, got 'void'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAuditDiagnostics/WrongDisposeAwaiterResult >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "DisposeAsync() awaitable GetResult() must return void, got 'int'" $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/Library >/dev/null && echo True
	@./$(BIN) build Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/Library >/dev/null; test -f Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/Library/bin/libVoid280AsyncCompositionAudit.a; echo True
	@$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/CConsumer/main.c Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/Library/bin/libVoid280AsyncCompositionAudit.a -pthread -o Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/CConsumer/async-composition-audit-consumer
	@./Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/CConsumer/async-composition-audit-consumer
	@./$(BIN) publish Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/Library >/dev/null; test -f Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/Library/publish/libVoid280AsyncCompositionAudit.a; echo True
	@./$(BIN) run Tests/AsyncLambdaDelegateEventValueTaskIntegration >/dev/null && echo True
	@./$(BIN) run Tests/AwaitUsingStateMachineStructuredCleanupCompletion >/dev/null && echo True
	@./$(BIN) run Tests/TaskWhenAnyCompletion >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.280"' Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit.voidproj; grep -Fq '"version": "0.0.280"' Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit/Library/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAuditLibrary.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-shared-monotonic-timer-queue-foundation

# Milestone 281: Shared Monotonic Timer Queue Foundation
test-shared-monotonic-timer-queue-foundation: $(BIN)
	@./$(BIN) check Tests/SharedMonotonicTimerQueueFoundation >/dev/null && echo True
	@timeout 20s ./$(BIN) run Tests/SharedMonotonicTimerQueueFoundation 2>/dev/null
	@set -e; ./$(BIN) publish Tests/SharedMonotonicTimerQueueFoundation >/dev/null; test -x Tests/SharedMonotonicTimerQueueFoundation/publish/SharedMonotonicTimerQueueFoundation$(EXE_SUFFIX); echo True
	@set -e; f=StandardLibrary/Void/Threading/TimerQueue.void; grep -Fq 'internal static class TimerQueue' $$f; grep -Fq 'Runtime.MonotonicTimeMilliseconds()' $$f; grep -Fq 'Monitor.Wait(_gate, waitMilliseconds)' $$f; grep -Fq 'ThreadPool.QueueTaskContinuation(invoke)' $$f; test "$$(grep -c 'new Thread' $$f)" -eq 1; ! grep -Fq 'Thread.Sleep' $$f; echo True
	@set -e; grep -Fq 'public static Task Delay(int millisecondsDelay)' StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public void CancelAfter(int millisecondsDelay)' StandardLibrary/Void/Threading/Cancellation.void; echo True
	@set -e; grep -Fq 'vc_managed_monotonic_time_milliseconds()' Compiler/src/compiler.c; grep -Fq 'vc_native_monotonic_time_ns(&vc_nanoseconds)' Compiler/src/compiler.c; grep -Fq 'Runtime.MonotonicTimeMilliseconds is reserved for Void.Threading.TimerQueue' Compiler/src/semantic.c; grep -Fq 'bool vc_native_monotonic_time_ns(uint64_t *nanoseconds);' Runtime/include/vc_thread.h; echo True
	@set -e; c=Tests/SharedMonotonicTimerQueueFoundation/.void/SharedMonotonicTimerQueueFoundation.c; grep -Fq 'vc_managed_monotonic_time_milliseconds' $$c; grep -Fq 'vc_native_monotonic_time_ns(&vc_nanoseconds)' $$c; grep -Fq 'vc_monitor_wait_timed' $$c; grep -Fq 'vc_managed_thread_start' $$c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/SharedMonotonicTimerQueueFoundationDiagnostics/NegativeDelay >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: timer delay must be non-negative' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/SharedMonotonicTimerQueueFoundationDiagnostics/NullCallback >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: timer callback cannot be null' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/SharedMonotonicTimerQueueFoundationDiagnostics/RegisterAfterShutdown >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: timer queue has been shut down' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/SharedMonotonicTimerQueueFoundationDiagnostics/PrivateMonotonicRuntime >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Runtime.MonotonicTimeMilliseconds is reserved for Void.Threading.TimerQueue' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/ManagedThreadPoolWorkQueueFoundation >/dev/null && echo True
	@./$(BIN) run Tests/ManagedThreadSleepTimeoutContract >/dev/null && echo True
	@./$(BIN) check Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.281"' Tests/SharedMonotonicTimerQueueFoundation/SharedMonotonicTimerQueueFoundation.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-task-delay-foundation

# Milestone 282: Task.Delay Foundation
test-task-delay-foundation: $(BIN)
	@./$(BIN) check Tests/TaskDelayFoundation >/dev/null && echo True
	@./$(BIN) build Tests/TaskDelayFoundation >/dev/null
	@timeout 20s ./Tests/TaskDelayFoundation/bin/TaskDelayFoundation
	@set -e; ./$(BIN) publish Tests/TaskDelayFoundation >/dev/null; test -x Tests/TaskDelayFoundation/publish/TaskDelayFoundation$(EXE_SUFFIX); echo True
	@set -e; f=StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public static Task Delay(int millisecondsDelay)' $$f; grep -Fq 'millisecondsDelay < Timeout.Infinite' $$f; grep -Fq 'millisecondsDelay == 0' $$f; grep -Fq 'millisecondsDelay == Timeout.Infinite' $$f; grep -Fq 'TimerQueue.Register(millisecondsDelay, completion)' $$f; ! grep -A25 -F 'public static Task Delay(int millisecondsDelay)' $$f | grep -Fq 'Thread.Sleep'; echo True
	@set -e; f=StandardLibrary/Void/Threading/TimerQueue.void; grep -Fq 'Runtime.MonotonicTimeMilliseconds()' $$f; grep -Fq 'ThreadPool.QueueTaskContinuation(invoke)' $$f; test "$$(grep -c 'new Thread' $$f)" -eq 1; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/TaskDelayFoundationDiagnostics/InvalidDelay >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task delay must be -1 or non-negative' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/SharedMonotonicTimerQueueFoundation >/dev/null && echo True
	@./$(BIN) run Tests/TaskWhenAllCompletion >/dev/null && echo True
	@./$(BIN) check Tests/AsyncLambdasAsyncDisposalTaskCompositionIntegrationAudit >/dev/null && echo True
	@set -e; grep -Fq 'public void CancelAfter(int millisecondsDelay)' StandardLibrary/Void/Threading/Cancellation.void; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.282"' Tests/TaskDelayFoundation/TaskDelayFoundation.voidproj; grep -Fq '"version": "0.0.281"' Tests/SharedMonotonicTimerQueueFoundation/SharedMonotonicTimerQueueFoundation.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True



.PHONY: test-task-delay-cancellation-race-completion

# Milestone 283: Task.Delay Cancellation & Race Completion
test-task-delay-cancellation-race-completion: $(BIN)
	@./$(BIN) check Tests/TaskDelayCancellationRaceCompletion >/dev/null && echo True
	@./$(BIN) build Tests/TaskDelayCancellationRaceCompletion >/dev/null
	@timeout 25s ./Tests/TaskDelayCancellationRaceCompletion/bin/TaskDelayCancellationRaceCompletion
	@set -e; ./$(BIN) publish Tests/TaskDelayCancellationRaceCompletion >/dev/null; test -x Tests/TaskDelayCancellationRaceCompletion/publish/TaskDelayCancellationRaceCompletion$(EXE_SUFFIX); echo True
	@set -e; f=StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public static Task Delay(int millisecondsDelay, CancellationToken cancellationToken)' $$f; grep -Fq 'cancellationToken.IsCancellationRequested' $$f; grep -Fq 'Task.FromCanceled(cancellationToken)' $$f; grep -Fq 'TimerQueueRegistration timerRegistration = default' $$f; grep -Fq 'CancellationTokenRegistration cancellationRegistration = default' $$f; grep -Fq 'task.TrySetCanceled(cancellationToken)' $$f; grep -Fq 'timerRegistration.Dispose()' $$f; grep -Fq 'cancellationRegistration.Dispose()' $$f; ! grep -A60 -F 'public static Task Delay(int millisecondsDelay, CancellationToken cancellationToken)' $$f | grep -Fq 'Thread.Sleep'; echo True
	@set -e; f=StandardLibrary/Void/Threading/TimerQueue.void; grep -Fq 'Runtime.MonotonicTimeMilliseconds()' $$f; grep -Fq 'ThreadPool.QueueTaskContinuation(invoke)' $$f; test "$$(grep -c 'new Thread' $$f)" -eq 1; grep -Fq 'public void CancelAfter(int millisecondsDelay)' StandardLibrary/Void/Threading/Cancellation.void; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/TaskDelayCancellationRaceCompletionDiagnostics/InvalidDelay >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task delay must be -1 or non-negative' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/TaskDelayFoundation >/dev/null && echo True
	@./$(BIN) run Tests/TaskCancellationIntegration >/dev/null && echo True
	@./$(BIN) check Tests/SharedMonotonicTimerQueueFoundation >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.283"' Tests/TaskDelayCancellationRaceCompletion/TaskDelayCancellationRaceCompletion.voidproj; grep -Fq '"version": "0.0.282"' Tests/TaskDelayFoundation/TaskDelayFoundation.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-cancellation-token-source-disposal-registration-lifetime-completion

# Milestone 284: CancellationTokenSource Disposal & Registration Lifetime Completion
test-cancellation-token-source-disposal-registration-lifetime-completion: $(BIN)
	@./$(BIN) check Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletion >/dev/null && echo True
	@./$(BIN) build Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletion >/dev/null
	@timeout 25s ./Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletion/bin/CancellationTokenSourceDisposalRegistrationLifetimeCompletion
	@set -e; ./$(BIN) publish Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletion >/dev/null; test -x Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletion/publish/CancellationTokenSourceDisposalRegistrationLifetimeCompletion$(EXE_SUFFIX); echo True
	@set -e; f=StandardLibrary/Void/Threading/Cancellation.void; grep -Fq 'public sealed class CancellationTokenSource : IDisposable' $$f; grep -Fq 'public void Dispose()' $$f; grep -Fq 'cancellation token source has been disposed' $$f; grep -Fq 'CancellationCallbackNode callbacks = null' $$f; grep -Fq 'current.Callback = null' $$f; echo True
	@set -e; f=StandardLibrary/Void/Threading/Cancellation.void; grep -Fq 'private CancellationOwnedResourceNode _ownedResources' $$f; grep -Fq 'internal CancellationSourceOwnership Own(CancellationTokenRegistration registration)' $$f; grep -Fq 'internal CancellationSourceOwnership Own(TimerQueueRegistration registration)' $$f; grep -Fq 'CancellationTokenSource.DisposeOwnedResources(ownedResources)' $$f; grep -Fq 'CancellationRegistration = default' $$f; grep -Fq 'TimerRegistration = default' $$f; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletionDiagnostics/CancelAfterDispose >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: cancellation token source has been disposed' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletionDiagnostics/RegisterAfterDispose >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: cancellation token source has been disposed' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletionDiagnostics/TokenAfterDispose >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: cancellation token source has been disposed' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/CancellationRegistrationWakeupCompletion >/dev/null && echo True
	@./$(BIN) run Tests/TaskDelayCancellationRaceCompletion >/dev/null && echo True
	@./$(BIN) run Tests/CancellationTokenSourceFoundation >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.284"' Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletion/CancellationTokenSourceDisposalRegistrationLifetimeCompletion.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'public void CancelAfter(int millisecondsDelay)' StandardLibrary/Void/Threading/Cancellation.void; echo True


.PHONY: test-cancellation-token-source-cancel-after-completion

# Milestone 285: CancellationTokenSource.CancelAfter Completion
test-cancellation-token-source-cancel-after-completion: $(BIN)
	@./$(BIN) check Tests/CancellationTokenSourceCancelAfterCompletion >/dev/null && echo True
	@./$(BIN) build Tests/CancellationTokenSourceCancelAfterCompletion >/dev/null
	@timeout 25s ./Tests/CancellationTokenSourceCancelAfterCompletion/bin/CancellationTokenSourceCancelAfterCompletion
	@set -e; ./$(BIN) publish Tests/CancellationTokenSourceCancelAfterCompletion >/dev/null; test -x Tests/CancellationTokenSourceCancelAfterCompletion/publish/CancellationTokenSourceCancelAfterCompletion$(EXE_SUFFIX); echo True
	@set -e; f=StandardLibrary/Void/Threading/Cancellation.void; grep -Fq 'public void CancelAfter(int millisecondsDelay)' $$f; grep -Fq 'TimerQueue.Register(millisecondsDelay, cancellation)' $$f; grep -Fq '_cancelAfterGeneration' $$f; grep -Fq '_cancelAfterResource' $$f; grep -Fq 'CancellationTokenSource.FinishCancellation(callbacks, ownedResources)' $$f; ! grep -A80 -F 'public void CancelAfter(int millisecondsDelay)' $$f | grep -Fq 'Task.Delay'; ! grep -A80 -F 'public void CancelAfter(int millisecondsDelay)' $$f | grep -Fq 'Thread.Sleep'; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/CancellationTokenSourceCancelAfterCompletionDiagnostics/InvalidDelay >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: cancellation delay must be -1 or non-negative' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/CancellationTokenSourceCancelAfterCompletionDiagnostics/AfterDispose >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: cancellation token source has been disposed' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletion >/dev/null && echo True
	@./$(BIN) run Tests/TaskDelayCancellationRaceCompletion >/dev/null && echo True
	@./$(BIN) check Tests/SharedMonotonicTimerQueueFoundation >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.285"' Tests/CancellationTokenSourceCancelAfterCompletion/CancellationTokenSourceCancelAfterCompletion.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-linked-cancellation-token-source-completion

# Milestone 286: Linked CancellationTokenSource Completion
test-linked-cancellation-token-source-completion: $(BIN)
	@./$(BIN) check Tests/LinkedCancellationTokenSourceCompletion >/dev/null && echo True
	@./$(BIN) build Tests/LinkedCancellationTokenSourceCompletion >/dev/null
	@timeout 25s ./Tests/LinkedCancellationTokenSourceCompletion/bin/LinkedCancellationTokenSourceCompletion
	@set -e; ./$(BIN) publish Tests/LinkedCancellationTokenSourceCompletion >/dev/null; test -x Tests/LinkedCancellationTokenSourceCompletion/publish/LinkedCancellationTokenSourceCompletion$(EXE_SUFFIX); echo True
	@set -e; f=StandardLibrary/Void/Threading/Cancellation.void; grep -Fq 'public static CancellationTokenSource CreateLinkedTokenSource(params CancellationToken[] tokens)' $$f; grep -Fq 'token.Register(() => linkedSource.CancelFromLinkedParent())' $$f; grep -Fq 'linkedSource.Own(registration)' $$f; grep -Fq 'private void CancelCore(bool failIfDisposed)' $$f; ! grep -A60 -F 'CreateLinkedTokenSource(params CancellationToken[] tokens)' $$f | grep -Fq 'Thread.Sleep'; ! grep -A60 -F 'CreateLinkedTokenSource(params CancellationToken[] tokens)' $$f | grep -Fq 'TimerQueue'; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/LinkedCancellationTokenSourceCompletionDiagnostics/NullArray >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: linked cancellation token array cannot be null' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/LinkedCancellationTokenSourceCompletionDiagnostics/EmptyArray >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: linked cancellation requires at least one token' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/LinkedCancellationTokenSourceCompletionDiagnostics/DisposedParent >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: cancellation token source has been disposed' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/CancellationTokenSourceDisposalRegistrationLifetimeCompletion >/dev/null && echo True
	@./$(BIN) run Tests/CancellationTokenSourceCancelAfterCompletion >/dev/null && echo True
	@./$(BIN) run Tests/TaskDelayCancellationRaceCompletion >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.286"' Tests/LinkedCancellationTokenSourceCompletion/LinkedCancellationTokenSourceCompletion.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-task-wait-async-cancellation-completion

# Milestone 287: Task.WaitAsync Cancellation Completion
test-task-wait-async-cancellation-completion: $(BIN)
	@./$(BIN) check Tests/TaskWaitAsyncCancellationCompletion >/dev/null && echo True
	@./$(BIN) build Tests/TaskWaitAsyncCancellationCompletion >/dev/null
	@timeout 30s ./Tests/TaskWaitAsyncCancellationCompletion/bin/TaskWaitAsyncCancellationCompletion
	@set -e; ./$(BIN) publish Tests/TaskWaitAsyncCancellationCompletion >/dev/null; test -x Tests/TaskWaitAsyncCancellationCompletion/publish/TaskWaitAsyncCancellationCompletion$(EXE_SUFFIX); echo True
	@set -e; f=StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public Task WaitAsync(CancellationToken cancellationToken)' $$f; grep -Fq 'public Task<T> WaitAsync(CancellationToken cancellationToken)' $$f; grep -Fq 'TaskWaitCancellationState' $$f; grep -Fq 'cancellationToken.Register(() => state.OnCancellation())' $$f; grep -Fq 'RegisterAwaitContinuation(() => state.OnSourceCompleted())' $$f; ! grep -A28 -F 'public Task WaitAsync(CancellationToken cancellationToken)' $$f | grep -Fq 'TimerQueue'; ! grep -A28 -F 'public Task WaitAsync(CancellationToken cancellationToken)' $$f | grep -Fq 'Thread.Sleep'; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/TaskWaitAsyncCancellationCompletionDiagnostics/DisposedToken >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: cancellation token source has been disposed' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/LinkedCancellationTokenSourceCompletion >/dev/null && echo True
	@./$(BIN) run Tests/CancellationTokenSourceCancelAfterCompletion >/dev/null && echo True
	@./$(BIN) run Tests/TaskDelayCancellationRaceCompletion >/dev/null && echo True
	@./$(BIN) check Tests/TaskCompletionFactoriesAsyncTaskRunUnwrapping >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.287"' Tests/TaskWaitAsyncCancellationCompletion/TaskWaitAsyncCancellationCompletion.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True


.PHONY: test-task-wait-async-timeout-combined-cancellation-completion

# Milestone 288: Task.WaitAsync Timeout & Combined Cancellation Completion
test-task-wait-async-timeout-combined-cancellation-completion: $(BIN)
	@./$(BIN) check Tests/TaskWaitAsyncTimeoutCombinedCancellationCompletion >/dev/null && echo True
	@./$(BIN) build Tests/TaskWaitAsyncTimeoutCombinedCancellationCompletion >/dev/null
	@timeout 45s ./Tests/TaskWaitAsyncTimeoutCombinedCancellationCompletion/bin/TaskWaitAsyncTimeoutCombinedCancellationCompletion
	@set -e; ./$(BIN) publish Tests/TaskWaitAsyncTimeoutCombinedCancellationCompletion >/dev/null; test -x Tests/TaskWaitAsyncTimeoutCombinedCancellationCompletion/publish/TaskWaitAsyncTimeoutCombinedCancellationCompletion$(EXE_SUFFIX); echo True
	@set -e; f=StandardLibrary/Void/Threading/Tasks/Task.void; grep -Fq 'public Task WaitAsync(int millisecondsTimeout)' $$f; grep -Fq 'public Task WaitAsync(int millisecondsTimeout, CancellationToken cancellationToken)' $$f; grep -Fq 'public Task<T> WaitAsync(int millisecondsTimeout)' $$f; grep -Fq 'public Task<T> WaitAsync(int millisecondsTimeout, CancellationToken cancellationToken)' $$f; grep -Fq 'TimerQueue.Register(millisecondsTimeout, () => state.OnTimeout())' $$f; grep -Fq 'completion.TrySetException(new TimeoutException())' $$f; grep -Fq 'private void Finish(Task completion)' $$f; grep -Fq 'private void Finish(Task<T> completion)' $$f; test -f StandardLibrary/Void/TimeoutException.void; ! grep -A48 -F 'public Task WaitAsync(int millisecondsTimeout, CancellationToken cancellationToken)' $$f | grep -Fq 'Thread.Sleep'; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/TaskWaitAsyncTimeoutCombinedCancellationCompletionDiagnostics/InvalidTimeout >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task wait timeout must be -1 or non-negative' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/TaskWaitAsyncTimeoutCombinedCancellationCompletionDiagnostics/DisposedToken >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: cancellation token source has been disposed' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/TaskWaitAsyncCancellationCompletion >/dev/null && echo True
	@./$(BIN) run Tests/CancellationTokenSourceCancelAfterCompletion >/dev/null && echo True
	@./$(BIN) check Tests/SharedMonotonicTimerQueueFoundation >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.288"' Tests/TaskWaitAsyncTimeoutCombinedCancellationCompletion/TaskWaitAsyncTimeoutCombinedCancellationCompletion.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-async-timing-cancellation-gc-thread-integration

# Milestone 289: Async Timing, Cancellation, GC & Thread Integration
test-async-timing-cancellation-gc-thread-integration: $(BIN)
	@./$(BIN) check Tests/AsyncTimingCancellationGcThreadIntegration >/dev/null && echo True
	@./$(BIN) build Tests/AsyncTimingCancellationGcThreadIntegration >/dev/null
	@timeout 60s ./Tests/AsyncTimingCancellationGcThreadIntegration/bin/AsyncTimingCancellationGcThreadIntegration
	@set -e; ./$(BIN) publish Tests/AsyncTimingCancellationGcThreadIntegration >/dev/null; test -x Tests/AsyncTimingCancellationGcThreadIntegration/publish/AsyncTimingCancellationGcThreadIntegration$(EXE_SUFFIX); echo True
	@set -e; f=StandardLibrary/Void/Threading/TimerQueue.void; grep -Fq 'Runtime.MonotonicTimeMilliseconds()' $$f; grep -Fq 'ThreadPool.QueueTaskContinuation(invoke)' $$f; test "$$(grep -c 'new Thread' $$f)" -eq 1; ! grep -Fq 'Thread.Sleep' $$f; echo True
	@set -e; t=StandardLibrary/Void/Threading/Tasks/Task.void; c=StandardLibrary/Void/Threading/Cancellation.void; grep -Fq 'TimerQueue.Register(millisecondsDelay, completion)' $$t; grep -Fq 'TimerQueue.Register(millisecondsTimeout, () => state.OnTimeout())' $$t; grep -Fq 'TimerQueue.Register(millisecondsDelay, cancellation)' $$c; grep -Fq 'CreateLinkedTokenSource(params CancellationToken[] tokens)' $$c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/AsyncTimingCancellationGcThreadIntegrationDiagnostics/InvalidDelayWithLinkedToken >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task delay must be -1 or non-negative' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/AsyncTimingCancellationGcThreadIntegrationDiagnostics/InvalidWaitTimeoutWithLinkedToken >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task wait timeout must be -1 or non-negative' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/AsyncTimingCancellationGcThreadIntegration/Library >/dev/null && echo True
	@./$(BIN) build Tests/AsyncTimingCancellationGcThreadIntegration/Library >/dev/null; test -f Tests/AsyncTimingCancellationGcThreadIntegration/Library/bin/libVoid289AsyncTimingCancellationIntegration.a; echo True
	@$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 Tests/AsyncTimingCancellationGcThreadIntegration/CConsumer/main.c Tests/AsyncTimingCancellationGcThreadIntegration/Library/bin/libVoid289AsyncTimingCancellationIntegration.a -pthread -o Tests/AsyncTimingCancellationGcThreadIntegration/CConsumer/async-timing-cancellation-integration-consumer
	@./Tests/AsyncTimingCancellationGcThreadIntegration/CConsumer/async-timing-cancellation-integration-consumer
	@./$(BIN) publish Tests/AsyncTimingCancellationGcThreadIntegration/Library >/dev/null; test -f Tests/AsyncTimingCancellationGcThreadIntegration/Library/publish/libVoid289AsyncTimingCancellationIntegration.a; echo True
	@./$(BIN) check Tests/TaskWaitAsyncTimeoutCombinedCancellationCompletion >/dev/null && echo True
	@./$(BIN) check Tests/AsyncIteratorCancellationIntegration >/dev/null && echo True
	@./$(BIN) check Tests/AwaitUsingStateMachineStructuredCleanupCompletion >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.289"' Tests/AsyncTimingCancellationGcThreadIntegration/AsyncTimingCancellationGcThreadIntegration.voidproj; grep -Fq '"version": "0.0.289"' Tests/AsyncTimingCancellationGcThreadIntegration/Library/AsyncTimingCancellationGcThreadIntegrationLibrary.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-async-timing-cancellation-integration-audit

# Milestone 290: Async Timing & Cancellation Integration Audit
test-async-timing-cancellation-integration-audit: $(BIN)
	@./$(BIN) check Tests/AsyncTimingCancellationIntegrationAudit >/dev/null && echo True
	@./$(BIN) build Tests/AsyncTimingCancellationIntegrationAudit >/dev/null
	@timeout 60s ./Tests/AsyncTimingCancellationIntegrationAudit/bin/AsyncTimingCancellationIntegrationAudit
	@set -e; f=StandardLibrary/Void/Threading/TimerQueue.void; grep -Fq 'Runtime.MonotonicTimeMilliseconds()' $$f; grep -Fq 'ThreadPool.QueueTaskContinuation(invoke)' $$f; test "$$(grep -c 'new Thread' $$f)" -eq 1; ! grep -Fq 'Thread.Sleep' $$f; grep -Fq 'TimerQueue.Shutdown();' StandardLibrary/Void/Threading/ThreadPool.void; echo True
	@set -e; t=StandardLibrary/Void/Threading/Tasks/Task.void; c=StandardLibrary/Void/Threading/Cancellation.void; grep -Fq 'TimerQueue.Register(millisecondsDelay, completion)' $$t; grep -Fq 'TimerQueue.Register(millisecondsTimeout, () => state.OnTimeout())' $$t; grep -Fq 'TimerQueue.Register(millisecondsDelay, cancellation)' $$c; grep -Fq 'CreateLinkedTokenSource(params CancellationToken[] tokens)' $$c; ! grep -Fq 'Task.Delay' $$c; echo True
	@set -e; r=Runtime/src/vc_thread.c; grep -Fq 'QueryPerformanceCounter(&counter)' $$r; grep -Fq 'QueryPerformanceFrequency(&frequency)' $$r; grep -Fq 'clock_gettime(CLOCK_MONOTONIC, &now)' $$r; grep -Fq 'pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC)' $$r; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/AsyncTimingCancellationIntegrationAuditDiagnostics/InvalidDelayPrecedence >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task delay must be -1 or non-negative' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/AsyncTimingCancellationIntegrationAuditDiagnostics/InvalidWaitTimeoutPrecedence >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: task wait timeout must be -1 or non-negative' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/AsyncTimingCancellationIntegrationAuditDiagnostics/AfterShutdown >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: timer queue has been shut down' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/AsyncTimingCancellationIntegrationAudit/Library >/dev/null && echo True
	@./$(BIN) build Tests/AsyncTimingCancellationIntegrationAudit/Library >/dev/null; test -f Tests/AsyncTimingCancellationIntegrationAudit/Library/bin/libVoid290AsyncTimingCancellationAudit.a; echo True
	@$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 Tests/AsyncTimingCancellationIntegrationAudit/CConsumer/main.c Tests/AsyncTimingCancellationIntegrationAudit/Library/bin/libVoid290AsyncTimingCancellationAudit.a -pthread -o Tests/AsyncTimingCancellationIntegrationAudit/CConsumer/async-timing-cancellation-audit-consumer
	@./Tests/AsyncTimingCancellationIntegrationAudit/CConsumer/async-timing-cancellation-audit-consumer
	@./$(BIN) check Tests/AsyncTimingCancellationGcThreadIntegration >/dev/null && echo True
	@./$(BIN) check Tests/TaskWaitAsyncTimeoutCombinedCancellationCompletion >/dev/null && echo True
	@./$(BIN) check Tests/LinkedCancellationTokenSourceCompletion >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.290"' Tests/AsyncTimingCancellationIntegrationAudit/AsyncTimingCancellationIntegrationAudit.voidproj; grep -Fq '"version": "0.0.290"' Tests/AsyncTimingCancellationIntegrationAudit/Library/AsyncTimingCancellationIntegrationAuditLibrary.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; echo True



.PHONY: test-managed-string-representation-foundation

# Milestone 291: Managed String Representation Foundation
test-managed-string-representation-foundation: $(BIN)
	@./$(BIN) check Tests/ManagedStringRepresentationFoundation >/dev/null && echo True
	@timeout 30s ./$(BIN) run Tests/ManagedStringRepresentationFoundation 2>/dev/null
	@set -e; ./$(BIN) publish Tests/ManagedStringRepresentationFoundation >/dev/null; test -x Tests/ManagedStringRepresentationFoundation/publish/ManagedStringRepresentationFoundation$(EXE_SUFFIX); echo True
	@set -e; c=Tests/ManagedStringRepresentationFoundation/.void/ManagedStringRepresentationFoundation.c; grep -Fq 'typedef struct VcString' $$c; grep -Fq 'size_t length;' $$c; grep -Fq 'char data[];' $$c; grep -Fq 'value->length = length;' $$c; grep -Fq "value->data[length] = '\\0';" $$c; grep -Fq 'vc_string_literal("\101\000\102", 3u)' $$c; grep -Fq 'typedef struct VcStringLiteralEntry' $$c; grep -Fq 'vc_gc_mark(literal->value);' $$c; echo True
	@set -e; grep -Fq 'case VC_SEM_TYPE_STRING: return "VcString *";' Compiler/src/compiler.c; grep -Fq 'static const char *native_abi_c_type' Compiler/src/compiler.c; grep -Fq 'type == VC_SEM_TYPE_STRING || type == VC_SEM_TYPE_OBJECT || vc_semantic_type_is_array(type)' Compiler/src/compiler.c; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True
	@set -e; c=Tests/ManagedStringRepresentationFoundation/.void/ManagedStringRepresentationFoundation.c; grep -Fq 'extern int32_t strcmp(const char *, const char *);' $$c; grep -Fq 'strcmp(vc_string_data(vc_p_0), vc_string_data(vc_p_1))' $$c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/ManagedStringRepresentationFoundationDiagnostics/CStringType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type 'cstring' is not supported as a local type" $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/NativeInterop >/dev/null && echo True
	@./$(BIN) run Tests/StaticCalls >/dev/null && echo True
	@./$(BIN) run Tests/Attributes >/dev/null && echo True
	@./$(BIN) run Tests/ErgonomicsIntegration >/dev/null && echo True
	@./$(BIN) run Tests/AsyncLambdaCaptureGenericGcLifetimeCompletion >/dev/null && echo True
	@./$(BIN) run Tests/ConstReadonly >/dev/null && ./$(BIN) run Tests/ConstantNullPatterns >/dev/null && echo True
	@./$(BIN) run Tests/OptionalParams >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.291"' Tests/ManagedStringRepresentationFoundation/ManagedStringRepresentationFoundation.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True

.PHONY: test-length-aware-string-semantics

# Milestone 292: Length-Aware String Semantics & Embedded-NUL Completion
test-length-aware-string-semantics: $(BIN)
	@./$(BIN) check Tests/LengthAwareStringSemantics >/dev/null && echo True
	@timeout 30s ./$(BIN) run Tests/LengthAwareStringSemantics 2>/dev/null
	@set -e; ./$(BIN) publish Tests/LengthAwareStringSemantics >/dev/null; test -x Tests/LengthAwareStringSemantics/publish/LengthAwareStringSemantics$(EXE_SUFFIX); echo True
	@set -e; c=Tests/LengthAwareStringSemantics/.void/LengthAwareStringSemantics.c; grep -Fq 'left->length != right->length' $$c; grep -Fq 'memcmp(left->data, right->data, left->length)' $$c; grep -Fq 'i < value->length' $$c; grep -Fq 'static VC_MAYBE_UNUSED void vc_string_write_line(FILE *stream, const VcString *value)' $$c; grep -Fq 'vc_runtime_find_type(const VcString *full_name)' $$c; grep -Fq 'vc_has_attribute_type(const char *type_name, const VcString *attribute_name)' $$c; grep -Fq 'vc_attribute_value_type(const char *type_name, const VcString *attribute_name, const VcString *argument_name)' $$c; ! grep -Fq 'strcmp(left->data, right->data)' $$c; echo True
	@set -e; out=$$(mktemp); ./$(BIN) run Tests/LengthAwareStringSemanticsOutput >$$out 2>/dev/null; $(PYTHON) -c 'from pathlib import Path; import sys; data=Path(sys.argv[1]).read_bytes(); raise SystemExit(0 if data.endswith(b"OUT\x00PUT" + (b"\r\n" if sys.platform == "win32" else b"\n")) else 1)' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/LengthAwareStringSemanticsDiagnostics/MetadataArgument >$$out 2>&1; then rm -f $$out; exit 1; fi; $(PYTHON) -c 'from pathlib import Path; import sys; data=Path(sys.argv[1]).read_bytes(); raise SystemExit(0 if b"VOID runtime error: attribute argument was not found\n" in data else 1)' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/LengthAwareStringSemanticsDiagnostics/RuntimeFail >$$out 2>&1; then rm -f $$out; exit 1; fi; $(PYTHON) -c 'from pathlib import Path; import sys; data=Path(sys.argv[1]).read_bytes(); raise SystemExit(0 if b"VOID runtime error: FAIL\x00TAIL\n" in data else 1)' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/LengthAwareStringSemanticsDiagnostics/UnhandledException >$$out 2>&1; then rm -f $$out; exit 1; fi; $(PYTHON) -c 'from pathlib import Path; import sys; data=Path(sys.argv[1]).read_bytes(); raise SystemExit(0 if b"Unhandled Exception: BOOM\x00TAIL\n" in data else 1)' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/ManagedStringRepresentationFoundation >/dev/null && echo True
	@./$(BIN) run Tests/DictionaryHashSet >/dev/null && echo True
	@./$(BIN) run Tests/ConstantNullPatterns >/dev/null && echo True
	@./$(BIN) run Tests/Attributes >/dev/null && echo True
	@./$(BIN) run Tests/TypeMetadata >/dev/null && echo True
	@./$(BIN) run Tests/ExceptionsRuntimeControlFlowIntegration >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.292"' Tests/LengthAwareStringSemantics/LengthAwareStringSemantics.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True



.PHONY: test-native-string-abi-contract-metadata-foundation

# Milestone 293: Native String ABI Contract Metadata Foundation
test-native-string-abi-contract-metadata-foundation: $(BIN)
	@./$(BIN) check Tests/NativeStringAbiContractMetadataFoundation >/dev/null && echo True
	@timeout 30s ./$(BIN) run Tests/NativeStringAbiContractMetadataFoundation 2>/dev/null
	@set -e; ./$(BIN) publish Tests/NativeStringAbiContractMetadataFoundation >/dev/null; test -x Tests/NativeStringAbiContractMetadataFoundation/publish/NativeStringAbiContractMetadataFoundation$(EXE_SUFFIX); echo True
	@set -e; h=Compiler/include/semantic.h; c=Compiler/src/semantic.c; e=StandardLibrary/Void/NativeStringKind.void; grep -Fq 'typedef enum VcNativeStringContract' $$h; grep -Fq 'VC_NATIVE_STRING_CONTRACT_BORROWED_NUL' $$h; grep -Fq 'VC_NATIVE_STRING_CONTRACT_BORROWED_POINTER_LENGTH' $$h; grep -Fq 'VC_NATIVE_STRING_CONTRACT_RETAINED_NUL' $$h; grep -Fq 'VC_NATIVE_STRING_CONTRACT_RETAINED_POINTER_LENGTH' $$h; grep -Fq 'VC_NATIVE_STRING_CONTRACT_STATIC_NUL' $$h; grep -Fq 'VC_NATIVE_STRING_CONTRACT_STATIC_POINTER_LENGTH' $$h; grep -Fq 'VC_NATIVE_STRING_CONTRACT_OWNED_NUL' $$h; grep -Fq 'VC_NATIVE_STRING_CONTRACT_OWNED_POINTER_LENGTH' $$h; grep -Fq 'native_string_parameter_contracts' $$h; grep -Fq 'native_string_return_contract' $$h; grep -Fq 'resolve_native_string_contracts' $$c; grep -Fq 'public enum NativeStringKind' $$e; grep -Fq 'BorrowedNul' $$e; echo True
	@set -e; c=Tests/NativeStringAbiContractMetadataFoundation/.void/NativeStringAbiContractMetadataFoundation.c; grep -Fq 'extern int32_t strcmp(const char *, const char *);' $$c; grep -Fq 'strcmp(vc_string_borrow_native_z(vc_p_0), vc_string_borrow_native_z(vc_p_1))' $$c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringAbiContractMetadataFoundationDiagnostics/UnknownContract >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] NativeStringKind member is not recognized' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringAbiContractMetadataFoundationDiagnostics/NonStringParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] parameter contract requires a string parameter' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringAbiContractMetadataFoundationDiagnostics/NonExternParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] parameter contracts may be used only on extern methods' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringAbiContractMetadataFoundationDiagnostics/NonStringReturn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] method contract requires a string return type' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringAbiContractMetadataFoundationDiagnostics/NonExternReturn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] return contracts may be used only on extern methods' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringAbiContractMetadataFoundationDiagnostics/OwnedParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] static and owned contracts are valid only on returns' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringAbiContractMetadataFoundationDiagnostics/RetainedReturn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] retained contracts are valid only on parameters' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringAbiContractMetadataFoundationDiagnostics/ByRefParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] parameter contracts require a by-value string parameter' $$out; rm -f $$out; echo True
	@set -e; p=Tests/NativeStringAbiContractMetadataFoundation/Program.void; grep -Fq '[NativeString] string left' $$p; grep -Fq '[NativeString]' $$p; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringAbiContractMetadataFoundationDiagnostics/NonLiteral >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] contract must be a NativeStringKind value' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringAbiContractMetadataFoundationDiagnostics/DuplicateParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] may appear only once on a parameter' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringAbiContractMetadataFoundationDiagnostics/DuplicateReturn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] may appear only once on a method return' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringAbiContractMetadataFoundationDiagnostics/Constructor >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] contracts may be used only on non-generic extern methods' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringAbiContractMetadataFoundationDiagnostics/GenericMethod >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] contracts may be used only on non-generic extern methods' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/NativeInterop >/dev/null && echo True
	@./$(BIN) run Tests/ManagedStringRepresentationFoundation >/dev/null && echo True
	@./$(BIN) run Tests/LengthAwareStringSemantics >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.293"' Tests/NativeStringAbiContractMetadataFoundation/NativeStringAbiContractMetadataFoundation.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True

.PHONY: test-borrowed-nul-native-string-parameters

# Milestone 294: Borrowed NUL-Terminated UTF-8 Parameter Completion
test-borrowed-nul-native-string-parameters: $(BIN) $(BORROWED_NUL_NATIVE_FIXTURE)
	@./$(BIN) check Tests/BorrowedNulNativeStringParameters >/dev/null && echo True
	@./$(BIN) build Tests/BorrowedNulNativeStringParameters >/dev/null
	@timeout 30s ./Tests/BorrowedNulNativeStringParameters/bin/BorrowedNulNativeStringParameters
	@set -e; ./$(BIN) publish Tests/BorrowedNulNativeStringParameters >/dev/null; test -x Tests/BorrowedNulNativeStringParameters/publish/BorrowedNulNativeStringParameters$(EXE_SUFFIX); echo True
	@set -e; c=Tests/BorrowedNulNativeStringParameters/.void/BorrowedNulNativeStringParameters.c; grep -Fq 'vc_string_borrow_native_z' $$c; grep -Fq "memchr(value->data, '\\0', value->length)" $$c; grep -Fq 'native borrowed NUL string contains embedded NUL' $$c; grep -Fq 'void294_is_null(vc_string_borrow_native_z(vc_p_0))' $$c; grep -Fq 'void294_length(vc_string_borrow_native_z(vc_p_0))' $$c; grep -Fq 'void294_length_legacy(vc_string_data(vc_p_0))' $$c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/BorrowedNulNativeStringParametersDiagnostics/EmbeddedNul >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: native borrowed NUL string contains embedded NUL' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/BorrowedNulNativeStringParametersDiagnostics/RawStringContract >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] contract must be a NativeStringKind value' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/BorrowedNulNativeStringParametersDiagnostics/TooManyArguments >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] accepts a NativeStringKind and optional native release symbol' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/NativeStringAbiContractMetadataFoundation >/dev/null && echo True
	@./$(BIN) run Tests/ManagedStringRepresentationFoundation >/dev/null && echo True
	@./$(BIN) run Tests/LengthAwareStringSemantics >/dev/null && echo True
	@./$(BIN) run Tests/NativeInterop >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.294"' Tests/BorrowedNulNativeStringParameters/BorrowedNulNativeStringParameters.voidproj; grep -Fq 'public enum NativeStringKind' StandardLibrary/Void/NativeStringKind.void; grep -Fq 'BorrowedNul' StandardLibrary/Void/NativeStringKind.void; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True

.PHONY: test-pointer-length-native-string-parameters

# Milestone 295: Pointer + Length Native UTF-8 Parameters
test-pointer-length-native-string-parameters: $(BIN) $(POINTER_LENGTH_NATIVE_FIXTURE)
	@./$(BIN) check Tests/PointerLengthNativeStringParameters >/dev/null && echo True
	@./$(BIN) build Tests/PointerLengthNativeStringParameters >/dev/null
	@timeout 30s ./Tests/PointerLengthNativeStringParameters/bin/PointerLengthNativeStringParameters
	@set -e; ./$(BIN) publish Tests/PointerLengthNativeStringParameters >/dev/null; test -x Tests/PointerLengthNativeStringParameters/publish/PointerLengthNativeStringParameters$(EXE_SUFFIX); echo True
	@set -e; c=Tests/PointerLengthNativeStringParameters/.void/PointerLengthNativeStringParameters.c; grep -Fq 'extern int32_t void295_pair_kind(const char *, size_t);' $$c; grep -Fq 'extern int32_t void295_mix(int32_t, const char *, size_t, int32_t, const char *, size_t, int32_t);' $$c; grep -Fq 'vc_string_data(vc_p_0), vc_string_length(vc_p_0)' $$c; grep -Fq 'void295_mix(vc_p_0, vc_string_data(vc_p_1), vc_string_length(vc_p_1), vc_p_2, vc_string_data(vc_p_3), vc_string_length(vc_p_3), vc_p_4)' $$c; grep -Fq 'void295_borrowed_nul_length(vc_string_borrow_native_z(vc_p_0))' $$c; grep -Fq 'void295_legacy_length(vc_string_data(vc_p_0))' $$c; echo True
	@./$(BIN) run Tests/BorrowedNulNativeStringParameters >/dev/null && echo True
	@./$(BIN) run Tests/NativeInterop >/dev/null && echo True
	@./$(BIN) check Tests/NativeStringAbiContractMetadataFoundation >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.295"' Tests/PointerLengthNativeStringParameters/PointerLengthNativeStringParameters.voidproj; grep -Fq 'BorrowedPointerLength' StandardLibrary/Void/NativeStringKind.void; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True

.PHONY: test-borrowed-static-native-string-returns

# Milestone 296: Borrowed & Static Native String Returns
test-borrowed-static-native-string-returns: $(BIN) $(STRING_RETURN_NATIVE_FIXTURE) $(STRING_RETURN_INVALID_NATIVE_FIXTURE)
	@./$(BIN) check Tests/BorrowedStaticNativeStringReturns >/dev/null && echo True
	@./$(BIN) build Tests/BorrowedStaticNativeStringReturns >/dev/null
	@timeout 30s ./Tests/BorrowedStaticNativeStringReturns/bin/BorrowedStaticNativeStringReturns
	@set -e; ./$(BIN) publish Tests/BorrowedStaticNativeStringReturns >/dev/null; test -x Tests/BorrowedStaticNativeStringReturns/publish/BorrowedStaticNativeStringReturns$(EXE_SUFFIX); echo True
	@set -e; c=Tests/BorrowedStaticNativeStringReturns/.void/BorrowedStaticNativeStringReturns.c; grep -Fq 'typedef struct VcNativeStringView' $$c; grep -Fq 'extern const char * void296_borrowed_nul(void);' $$c; grep -Fq 'extern const char * void296_static_nul(void);' $$c; grep -Fq 'extern VcNativeStringView void296_borrowed_view(void);' $$c; grep -Fq 'extern VcNativeStringView void296_static_view(void);' $$c; grep -Fq 'VcNativeStringView vc_native_result = void296_borrowed_view();' $$c; grep -Fq 'vc_string_from_native_view(vc_native_result)' $$c; grep -Fq 'vc_string_from_native_z(vc_native_result)' $$c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) run Tests/BorrowedStaticNativeStringReturnsDiagnostics/InvalidPointerLength >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: native string view has null data with nonzero length' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/PointerLengthNativeStringParameters >/dev/null && echo True
	@./$(BIN) run Tests/BorrowedNulNativeStringParameters >/dev/null && echo True
	@./$(BIN) check Tests/NativeStringAbiContractMetadataFoundation >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.296"' Tests/BorrowedStaticNativeStringReturns/BorrowedStaticNativeStringReturns.voidproj; grep -Fq 'BorrowedPointerLength' StandardLibrary/Void/NativeStringKind.void; grep -Fq 'StaticPointerLength' StandardLibrary/Void/NativeStringKind.void; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True

.PHONY: test-owned-native-string-returns

# Milestone 297: Owned Native String Returns & Release Contracts
test-owned-native-string-returns: $(BIN) $(OWNED_STRING_RETURN_NATIVE_FIXTURE)
	@./$(BIN) check Tests/OwnedNativeStringReturns >/dev/null && echo True
	@./$(BIN) build Tests/OwnedNativeStringReturns >/dev/null
	@timeout 30s ./Tests/OwnedNativeStringReturns/bin/OwnedNativeStringReturns
	@set -e; ./$(BIN) publish Tests/OwnedNativeStringReturns >/dev/null; test -x Tests/OwnedNativeStringReturns/publish/OwnedNativeStringReturns$(EXE_SUFFIX); echo True
	@set -e; c=Tests/OwnedNativeStringReturns/.void/OwnedNativeStringReturns.c; grep -Fq 'extern void void297_release_nul(void *);' $$c; grep -Fq 'extern void void297_release_view(void *);' $$c; grep -Fq 'extern VcNativeStringView void297_owned_view(void);' $$c; grep -Fq 'vc_string_release_native_owned((void *)vc_native_result, void297_release_nul);' $$c; grep -Fq 'vc_string_release_native_owned((void *)vc_native_result.data, void297_release_view);' $$c; grep -Fq 'if (vc_native_pending_exception != NULL)' $$c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/OwnedNativeStringReturnsDiagnostics/MissingRelease >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] owned return contracts require a native release symbol' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/OwnedNativeStringReturnsDiagnostics/ReleaseOnBorrowed >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] native release symbol is valid only for owned return contracts' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/OwnedNativeStringReturnsDiagnostics/NonLiteralRelease >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] native release symbol must be a string literal' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/OwnedNativeStringReturnsDiagnostics/InvalidReleaseSymbol >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "[NativeString] native release symbol 'bad-release' is not a valid C identifier" $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/BorrowedStaticNativeStringReturns >/dev/null && echo True
	@./$(BIN) run Tests/PointerLengthNativeStringParameters >/dev/null && echo True
	@./$(BIN) run Tests/BorrowedNulNativeStringParameters >/dev/null && echo True
	@./$(BIN) check Tests/NativeStringAbiContractMetadataFoundation >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.297"' Tests/OwnedNativeStringReturns/OwnedNativeStringReturns.voidproj; grep -Fq 'OwnedNul' StandardLibrary/Void/NativeStringKind.void; grep -Fq 'OwnedPointerLength' StandardLibrary/Void/NativeStringKind.void; grep -Fq 'native_string_release_c_name' Compiler/include/semantic.h; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True


.PHONY: test-void-string-export-abi

# Milestone 298: VOID String Export ABI
test-void-string-export-abi: $(BIN)
	@./$(BIN) check Tests/VoidStringExportAbi/Library >/dev/null && echo True
	@./$(BIN) build Tests/VoidStringExportAbi/Library >/dev/null; test -f Tests/VoidStringExportAbi/Library/bin/libVoidStringExportAbi.a; echo True
	@set -e; symbols="$$(nm -g --defined-only Tests/VoidStringExportAbi/Library/bin/libVoidStringExportAbi.a)"; printf '%s\n' "$$symbols" | grep -Fq ' void298_echo'; printf '%s\n' "$$symbols" | grep -Fq ' void298_make_embedded'; ! printf '%s\n' "$$symbols" | grep -Fq ' vc_m_'; echo True
	@$(CC) $(CFLAGS) Tests/VoidStringExportAbi/CConsumer/main.c Tests/VoidStringExportAbi/Library/bin/libVoidStringExportAbi.a -o Tests/VoidStringExportAbi/CConsumer/consumer
	@./Tests/VoidStringExportAbi/CConsumer/consumer
	@$(CC) $(CFLAGS) Tests/VoidStringExportAbi/CConsumer/invalid_view.c Tests/VoidStringExportAbi/Library/bin/libVoidStringExportAbi.a -o Tests/VoidStringExportAbi/CConsumer/invalid-view
	@set -e; out=$$(mktemp); if (ulimit -c 0; ./Tests/VoidStringExportAbi/CConsumer/invalid-view >$$out 2>&1); then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: export string view has null data with nonzero length' $$out; rm -f $$out; echo True
	@set -e; c=Tests/VoidStringExportAbi/Library/.void/VoidStringExportAbi.c; grep -Fq 'typedef struct VcExportStringView' $$c; grep -Fq 'typedef struct VcExportStringResult' $$c; grep -Fq 'VcExportStringResult void298_echo(VcExportStringView vc_p_0)' $$c; grep -Fq 'int32_t void298_check_pair(VcExportStringView vc_p_0, int32_t vc_p_1, VcExportStringView vc_p_2)' $$c; grep -Fq 'VcString *vc_export_p_0 = vc_string_from_export_view(vc_p_0);' $$c; grep -Fq 'VcString *vc_export_p_2 = vc_string_from_export_view(vc_p_2);' $$c; grep -Fq 'vc_gc_root_push(&vc_export_root_0' $$c; grep -Fq 'vc_gc_root_push(&vc_export_root_2' $$c; grep -Fq 'VcExportStringResult vc_result = vc_string_to_export_result(vc_managed_result);' $$c; grep -Fq 'result.release = vc_export_string_release;' $$c; ! grep -Eq '^VcString \* void298_' $$c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/VoidStringExportAbiDiagnostics/ByRefParameter >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'exported string parameters must be passed by value' $$out; rm -f $$out; echo True
	@./$(BIN) publish Tests/VoidStringExportAbi/Library >/dev/null; test -f Tests/VoidStringExportAbi/Library/publish/libVoidStringExportAbi.a; echo True
	@./$(BIN) check Tests/LibraryOutput/Library >/dev/null && ./$(BIN) build Tests/LibraryOutput/Library >/dev/null && echo True
	@./$(BIN) check Tests/OwnedNativeStringReturns >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.298"' Tests/VoidStringExportAbi/Library/VoidStringExportAbi.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True


.PHONY: test-native-string-lifetime-callback-boundary-integration

# Milestone 299: Native String Lifetime, Callback & Boundary Integration Completion
test-native-string-lifetime-callback-boundary-integration: $(BIN) $(NATIVE_STRING_BOUNDARY_NATIVE_FIXTURE)
	@./$(BIN) check Tests/NativeStringLifetimeCallbackBoundaryIntegration >/dev/null && echo True
	@./$(BIN) build Tests/NativeStringLifetimeCallbackBoundaryIntegration >/dev/null
	@timeout 30s ./Tests/NativeStringLifetimeCallbackBoundaryIntegration/bin/NativeStringLifetimeCallbackBoundaryIntegration
	@set -e; ./$(BIN) publish Tests/NativeStringLifetimeCallbackBoundaryIntegration >/dev/null; test -x Tests/NativeStringLifetimeCallbackBoundaryIntegration/publish/NativeStringLifetimeCallbackBoundaryIntegration$(EXE_SUFFIX); echo True
	@set -e; c=Tests/NativeStringLifetimeCallbackBoundaryIntegration/.void/NativeStringLifetimeCallbackBoundaryIntegration.c; grep -Fq 'extern int32_t void299_call_nul(int32_t (*)(const char *));' $$c; grep -Fq 'extern int32_t void299_call_view(int32_t (*)(const char *, size_t));' $$c; grep -Fq 'extern int32_t void299_call_pair(int32_t (*)(const char *, size_t, int32_t, const char *, size_t));' $$c; grep -Fq 'vc_string_from_native_z(vc_p_0)' $$c; grep -Fq 'vc_string_from_native_view((VcNativeStringView){vc_p_0_data, vc_p_0_length})' $$c; grep -Fq 'vc_gc_root_push(&vc_callback_root_0' $$c; grep -Fq 'vc_gc_root_pop(&vc_callback_root_0)' $$c; ! grep -Eq 'void299_call_(nul|view|pair)\([^;]*VcString' $$c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringLifetimeCallbackBoundaryIntegrationDiagnostics/RetainedCall >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'retained native string parameters cannot borrow managed string storage' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringLifetimeCallbackBoundaryIntegrationDiagnostics/RawFunctionPointerString >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native function pointer parameter signature is not supported by the C ABI' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringLifetimeCallbackBoundaryIntegrationDiagnostics/MissingCallbackContract >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native callback delegate has a signature that is not supported by the C ABI' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringLifetimeCallbackBoundaryIntegrationDiagnostics/CallbackStringReturn >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'native callback delegate has a signature that is not supported by the C ABI' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringLifetimeCallbackBoundaryIntegrationDiagnostics/RetainedCallbackContract >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] native callback string parameters support only BorrowedNul and BorrowedPointerLength' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringLifetimeCallbackBoundaryIntegrationDiagnostics/WritableStringBuffer >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'extern string parameters must be passed by value; use explicit pointer or native buffer storage for writable text' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/NativeCallbacks >/dev/null && echo True
	@./$(BIN) run Tests/NativeCallbackByRef >/dev/null && echo True
	@./$(BIN) run Tests/NativeFunctionPointerAbiIntegration >/dev/null && echo True
	@./$(BIN) check Tests/VoidStringExportAbi/Library >/dev/null && echo True
	@./$(BIN) check Tests/OwnedNativeStringReturns >/dev/null && echo True
	@./$(BIN) check Tests/NativeStringAbiContractMetadataFoundation >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.299"' Tests/NativeStringLifetimeCallbackBoundaryIntegration/NativeStringLifetimeCallbackBoundaryIntegration.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True


.PHONY: test-managed-strings-native-string-abi-integration-audit

# Milestone 300: Managed Strings & Native String ABI Integration Audit
test-managed-strings-native-string-abi-integration-audit: $(BIN) $(MANAGED_STRING_AUDIT_NATIVE_FIXTURE)
	@./$(BIN) check Tests/ManagedStringsNativeStringAbiIntegrationAudit >/dev/null && echo True
	@set -e; ./$(BIN) build Tests/ManagedStringsNativeStringAbiIntegrationAudit >/dev/null; test -x Tests/ManagedStringsNativeStringAbiIntegrationAudit/bin/ManagedStringsNativeStringAbiIntegrationAudit$(EXE_SUFFIX); echo True
	@timeout 30s ./Tests/ManagedStringsNativeStringAbiIntegrationAudit/bin/ManagedStringsNativeStringAbiIntegrationAudit
	@set -e; ./$(BIN) publish Tests/ManagedStringsNativeStringAbiIntegrationAudit >/dev/null; test -x Tests/ManagedStringsNativeStringAbiIntegrationAudit/publish/ManagedStringsNativeStringAbiIntegrationAudit$(EXE_SUFFIX); echo True
	@set -e; c=Tests/ManagedStringsNativeStringAbiIntegrationAudit/.void/ManagedStringsNativeStringAbiIntegrationAudit.c; grep -Fq 'typedef struct VcString' $$c; grep -Fq 'typedef struct VcNativeStringView' $$c; grep -Fq 'extern int32_t void300_borrowed_nul_length(const char *);' $$c; grep -Fq 'extern int32_t void300_view_matches(const char *, size_t);' $$c; grep -Fq 'extern const char * void300_borrowed_nul(void);' $$c; grep -Fq 'extern VcNativeStringView void300_static_view(void);' $$c; grep -Fq 'extern int32_t void300_call_view(int32_t (*)(const char *, size_t));' $$c; grep -Fq 'vc_string_borrow_native_z' $$c; grep -Fq 'vc_string_from_native_view' $$c; grep -Fq 'vc_string_release_native_owned' $$c; grep -Fq 'vc_gc_root_push(&vc_callback_root_0' $$c; ! grep -Eq 'extern .*void300_.*VcString' $$c; echo True
	@./$(BIN) check Tests/ManagedStringsNativeStringAbiIntegrationAudit/Library >/dev/null && ./$(BIN) build Tests/ManagedStringsNativeStringAbiIntegrationAudit/Library >/dev/null; test -f Tests/ManagedStringsNativeStringAbiIntegrationAudit/Library/bin/libManagedStringsNativeStringAbiIntegrationAuditLibrary.a; echo True
	@$(CC) $(CFLAGS) Tests/ManagedStringsNativeStringAbiIntegrationAudit/CConsumer/main.c Tests/ManagedStringsNativeStringAbiIntegrationAudit/Library/bin/libManagedStringsNativeStringAbiIntegrationAuditLibrary.a -pthread -o $(MANAGED_STRING_AUDIT_C_CONSUMER)
	@$(MANAGED_STRING_AUDIT_C_CONSUMER)
	@set -e; c=Tests/ManagedStringsNativeStringAbiIntegrationAudit/Library/.void/ManagedStringsNativeStringAbiIntegrationAuditLibrary.c; grep -Fq 'VcExportStringResult void300_export_echo(VcExportStringView vc_p_0)' $$c; grep -Fq 'int32_t void300_export_check(VcExportStringView vc_p_0)' $$c; grep -Fq 'VcString *vc_export_p_0 = vc_string_from_export_view(vc_p_0);' $$c; grep -Fq 'VcExportStringResult vc_result = vc_string_to_export_result(vc_managed_result);' $$c; grep -Fq 'result.release = vc_export_string_release;' $$c; ! grep -Eq '^(VcString \*|.*\(VcString \*)).*void300_export_' $$c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/ManagedStringRepresentationFoundationDiagnostics/CStringType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type 'cstring' is not supported as a local type" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/BorrowedNulNativeStringParametersDiagnostics/RawStringContract >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq '[NativeString] contract must be a NativeStringKind value' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/NativeStringLifetimeCallbackBoundaryIntegrationDiagnostics/RetainedCall >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'retained native string parameters cannot borrow managed string storage' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/ManagedStringRepresentationFoundation >/dev/null && echo True
	@./$(BIN) run Tests/LengthAwareStringSemantics >/dev/null && echo True
	@./$(BIN) check Tests/NativeStringAbiContractMetadataFoundation >/dev/null && echo True
	@./$(BIN) run Tests/NativeStringLifetimeCallbackBoundaryIntegration >/dev/null && echo True
	@./$(BIN) check Tests/VoidStringExportAbi/Library >/dev/null && echo True
	@./$(BIN) check Tests/OwnedNativeStringReturns >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.300"' Tests/ManagedStringsNativeStringAbiIntegrationAudit/ManagedStringsNativeStringAbiIntegrationAudit.voidproj; grep -Fq '"version": "0.0.300"' Tests/ManagedStringsNativeStringAbiIntegrationAudit/Library/ManagedStringsNativeStringAbiIntegrationAuditLibrary.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True


.PHONY: test-utf8-validity-unicode-scalar-foundation

# Milestone 301: UTF-8 Validity & Unicode Scalar Foundation
test-utf8-validity-unicode-scalar-foundation: $(BIN) $(UTF8_VALIDITY_NATIVE_FIXTURE) $(UTF8_VALIDITY_DIAGNOSTIC_FIXTURES)
	@./$(BIN) check Tests/Utf8ValidityUnicodeScalarFoundation >/dev/null; echo True
	@set -e; ./$(BIN) build Tests/Utf8ValidityUnicodeScalarFoundation >/dev/null; test -x Tests/Utf8ValidityUnicodeScalarFoundation/bin/Utf8ValidityUnicodeScalarFoundation$(EXE_SUFFIX); echo True
	@timeout 20s ./Tests/Utf8ValidityUnicodeScalarFoundation/bin/Utf8ValidityUnicodeScalarFoundation
	@set -e; ./$(BIN) publish Tests/Utf8ValidityUnicodeScalarFoundation >/dev/null; test -x Tests/Utf8ValidityUnicodeScalarFoundation/publish/Utf8ValidityUnicodeScalarFoundation$(EXE_SUFFIX); echo True
	@set -e; c=Tests/Utf8ValidityUnicodeScalarFoundation/.void/Utf8ValidityUnicodeScalarFoundation.c; grep -Fq '#include "vc_utf8.h"' $$c; grep -Fq 'vc_utf8_validate_and_count(data, length, &scalar_length)' $$c; grep -Fq 'vc_char_require_scalar' $$c; grep -Fq 'vc_utf8_decode_one' Runtime/include/vc_utf8.h; echo True
	@set -e; grep -Fq 'first >= UINT8_C(0xc2)' Runtime/include/vc_utf8.h; grep -Fq 'first == UINT8_C(0xe0) && second < UINT8_C(0xa0)' Runtime/include/vc_utf8.h; grep -Fq 'first == UINT8_C(0xed) && second > UINT8_C(0x9f)' Runtime/include/vc_utf8.h; grep -Fq 'first == UINT8_C(0xf4) && second > UINT8_C(0x8f)' Runtime/include/vc_utf8.h; grep -Fq 'scalar >= UINT32_C(0xd800)' Runtime/include/vc_utf8.h; echo True
	@set -e; for d in InvalidNativeNul InvalidOverlongView InvalidSurrogateView InvalidCallbackView InvalidOwnedReturn InvalidLiteral InvalidCharSurrogate InvalidCharAboveMaximum; do ./$(BIN) build Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/$$d >/dev/null; done; echo True
	@set -e; out=$$(mktemp); if Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidNativeNul/bin/Utf8ValidityUnicodeScalarFoundationDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string data is not valid UTF-8' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidOverlongView/bin/Utf8ValidityUnicodeScalarFoundationDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string data is not valid UTF-8' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidSurrogateView/bin/Utf8ValidityUnicodeScalarFoundationDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string data is not valid UTF-8' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidCallbackView/bin/Utf8ValidityUnicodeScalarFoundationDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string data is not valid UTF-8' $$out; rm -f $$out; echo True
	@set -e; rm -f Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidOwnedReturn/release.marker; out=$$(mktemp); if Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidOwnedReturn/bin/Utf8ValidityUnicodeScalarFoundationDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: native owned string data is not valid UTF-8' $$out; test -f Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidOwnedReturn/release.marker; rm -f $$out Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidOwnedReturn/release.marker; echo True
	@set -e; out=$$(mktemp); if Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidLiteral/bin/Utf8ValidityUnicodeScalarFoundationDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string data is not valid UTF-8' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidCharSurrogate/bin/Utf8ValidityUnicodeScalarFoundationDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: char value is not a Unicode scalar value' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if Tests/Utf8ValidityUnicodeScalarFoundationDiagnostics/InvalidCharAboveMaximum/bin/Utf8ValidityUnicodeScalarFoundationDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: char value is not a Unicode scalar value' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/Utf8ValidityUnicodeScalarFoundation/Library >/dev/null; ./$(BIN) build Tests/Utf8ValidityUnicodeScalarFoundation/Library >/dev/null; test -f Tests/Utf8ValidityUnicodeScalarFoundation/Library/bin/libUtf8ValidityUnicodeScalarFoundationLibrary.a; echo True
	@$(CC) $(CFLAGS) Tests/Utf8ValidityUnicodeScalarFoundation/CConsumer/invalid_utf8.c Tests/Utf8ValidityUnicodeScalarFoundation/Library/bin/libUtf8ValidityUnicodeScalarFoundationLibrary.a -pthread -o $(UTF8_VALIDITY_EXPORT_CONSUMER)
	@set -e; out=$$(mktemp); if $(UTF8_VALIDITY_EXPORT_CONSUMER) >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string data is not valid UTF-8' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/ManagedStringsNativeStringAbiIntegrationAudit >/dev/null; echo True
	@./$(BIN) check Tests/NativeStringLifetimeCallbackBoundaryIntegration >/dev/null; ./$(BIN) check Tests/OwnedNativeStringReturns >/dev/null; ./$(BIN) check Tests/VoidStringExportAbi/Library >/dev/null; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.301"' Tests/Utf8ValidityUnicodeScalarFoundation/Utf8ValidityUnicodeScalarFoundation.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True


.PHONY: test-string-core-properties-empty-semantics

# Milestone 302: String Core Properties & Empty Semantics
test-string-core-properties-empty-semantics: $(BIN)
	@./$(BIN) check Tests/StringCoreProperties >/dev/null; echo True
	@set -e; ./$(BIN) build Tests/StringCoreProperties >/dev/null; test -x Tests/StringCoreProperties/bin/StringCoreProperties$(EXE_SUFFIX); echo True
	@timeout 20s ./Tests/StringCoreProperties/bin/StringCoreProperties
	@set -e; ./$(BIN) publish Tests/StringCoreProperties >/dev/null; test -x Tests/StringCoreProperties/publish/StringCoreProperties$(EXE_SUFFIX); echo True
	@set -e; c=Tests/StringCoreProperties/.void/StringCoreProperties.c; grep -Fq 'size_t scalar_length;' $$c; grep -Fq 'vc_string_scalar_length_i32' $$c; grep -Fq 'vc_string_byte_length_i32' $$c; grep -Fq 'vc_string_is_empty' $$c; grep -Fq 'vc_string_literal("", 0u)' $$c; echo True
	@set -e; grep -Fq 'vc_utf8_validate_and_count' Runtime/include/vc_utf8.h; grep -Fq 'value->scalar_length = scalar_length;' Compiler/src/compiler.c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/StringCorePropertiesDiagnostics/ReadonlyMember >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'string members are readonly' $$out; rm -f $$out; echo True
	@set -e; ./$(BIN) build Tests/StringCorePropertiesDiagnostics/NullReceiver >/dev/null; out=$$(mktemp); if Tests/StringCorePropertiesDiagnostics/NullReceiver/bin/StringCorePropertiesDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string reference is null' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/StringCorePropertiesDiagnostics/UnknownStaticMember >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "string has no static member 'NotEmpty'" $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/Utf8ValidityUnicodeScalarFoundation >/dev/null; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.302"' Tests/StringCoreProperties/StringCoreProperties.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True

.PHONY: test-string-character-indexing-enumeration

# Milestone 303: String Character Indexing, Index & Enumeration
test-string-character-indexing-enumeration: $(BIN)
	@./$(BIN) check Tests/StringCharacterIndexingEnumeration >/dev/null; echo True
	@set -e; ./$(BIN) build Tests/StringCharacterIndexingEnumeration >/dev/null; test -x Tests/StringCharacterIndexingEnumeration/bin/StringCharacterIndexingEnumeration$(EXE_SUFFIX); echo True
	@timeout 20s ./Tests/StringCharacterIndexingEnumeration/bin/StringCharacterIndexingEnumeration
	@set -e; ./$(BIN) publish Tests/StringCharacterIndexingEnumeration >/dev/null; test -x Tests/StringCharacterIndexingEnumeration/publish/StringCharacterIndexingEnumeration$(EXE_SUFFIX); echo True
	@set -e; c=Tests/StringCharacterIndexingEnumeration/.void/StringCharacterIndexingEnumeration.c; grep -Fq 'vc_string_char_at' $$c; grep -Fq 'vc_string_scalar_length_i32' $$c; grep -Fq 'vc_utf8_decode_one' $$c; grep -Fq 'while (vc_fe_o_' $$c; ! grep -Fq 'StringEnumerator' $$c; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/StringCharacterIndexingEnumerationDiagnostics/OutOfRangeInt >/dev/null; if Tests/StringCharacterIndexingEnumerationDiagnostics/OutOfRangeInt/bin/StringCharacterIndexingEnumerationDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string index out of range' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/StringCharacterIndexingEnumerationDiagnostics/OutOfRangeFromEnd >/dev/null; if Tests/StringCharacterIndexingEnumerationDiagnostics/OutOfRangeFromEnd/bin/StringCharacterIndexingEnumerationDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string index out of range' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/StringCharacterIndexingEnumerationDiagnostics/NullReceiver >/dev/null; if Tests/StringCharacterIndexingEnumerationDiagnostics/NullReceiver/bin/StringCharacterIndexingEnumerationDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string reference is null' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/StringCharacterIndexingEnumerationDiagnostics/ReadonlyIndex >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'string characters are readonly' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/StringCharacterIndexingEnumerationDiagnostics/RefForeach >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'ref foreach is not supported for string characters' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/StringCharacterIndexingEnumerationDiagnostics/WrongIndexType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "string index must be 'int', 'Index', or 'Range', got 'bool'" $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/StringCoreProperties >/dev/null; echo True
	@./$(BIN) check Tests/Utf8ValidityUnicodeScalarFoundation >/dev/null; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.303"' Tests/StringCharacterIndexingEnumeration/StringCharacterIndexingEnumeration.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True

.PHONY: test-string-range-slicing-substring

# Milestone 305: String Range Slicing & Substring Completion
test-string-range-slicing-substring: $(BIN)
	@./$(BIN) check Tests/StringRangeSlicingSubstring >/dev/null; echo True
	@set -e; ./$(BIN) build Tests/StringRangeSlicingSubstring >/dev/null; test -x Tests/StringRangeSlicingSubstring/bin/StringRangeSlicingSubstring$(EXE_SUFFIX); echo True
	@timeout 20s ./Tests/StringRangeSlicingSubstring/bin/StringRangeSlicingSubstring
	@set -e; ./$(BIN) publish Tests/StringRangeSlicingSubstring >/dev/null; test -x Tests/StringRangeSlicingSubstring/publish/StringRangeSlicingSubstring$(EXE_SUFFIX); echo True
	@set -e; c=Tests/StringRangeSlicingSubstring/.void/StringRangeSlicingSubstring.c; grep -Fq 'vc_string_slice' $$c; grep -Fq 'vc_utf8_byte_offset' $$c; grep -Fq 'vc_string_scalar_length_i32' $$c; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/StringRangeSlicingSubstringDiagnostics/NullRangeReceiver >/dev/null; if Tests/StringRangeSlicingSubstringDiagnostics/NullRangeReceiver/bin/StringRangeSlicingSubstringDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string reference is null' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/StringRangeSlicingSubstringDiagnostics/ReverseRange >/dev/null; if Tests/StringRangeSlicingSubstringDiagnostics/ReverseRange/bin/StringRangeSlicingSubstringDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: Range out of range' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/StringRangeSlicingSubstringDiagnostics/OutOfRangeRange >/dev/null; if Tests/StringRangeSlicingSubstringDiagnostics/OutOfRangeRange/bin/StringRangeSlicingSubstringDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: Range out of range' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/StringRangeSlicingSubstringDiagnostics/SubstringNegativeStart >/dev/null; if Tests/StringRangeSlicingSubstringDiagnostics/SubstringNegativeStart/bin/StringRangeSlicingSubstringDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string slice out of range' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/StringRangeSlicingSubstringDiagnostics/SubstringNegativeLength >/dev/null; if Tests/StringRangeSlicingSubstringDiagnostics/SubstringNegativeLength/bin/StringRangeSlicingSubstringDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string slice out of range' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/StringRangeSlicingSubstringDiagnostics/SubstringOutOfRange >/dev/null; if Tests/StringRangeSlicingSubstringDiagnostics/SubstringOutOfRange/bin/StringRangeSlicingSubstringDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string slice out of range' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/StringRangeSlicingSubstringDiagnostics/SubstringWrongType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'string.Substring expects (int start) or (int start, int length)' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/StringRangeSlicingSubstringDiagnostics/SubstringWrongArity >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'string.Substring expects (int start) or (int start, int length)' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/StringRangeSlicingSubstringDiagnostics/ReadonlyRange >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'string slices are readonly' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/StringRangeSlicingSubstringDiagnostics/NullSubstring >/dev/null; if Tests/StringRangeSlicingSubstringDiagnostics/NullSubstring/bin/StringRangeSlicingSubstringDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string reference is null' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/StringCharacterIndexingEnumeration >/dev/null; echo True
	@./$(BIN) run Tests/StringCoreProperties >/dev/null; echo True
	@./$(BIN) check Tests/Utf8ValidityUnicodeScalarFoundation >/dev/null; echo True
	@./$(BIN) run Tests/ArraysSpanIndexRangeCompletion >/dev/null; echo True
	@./$(BIN) run Tests/RangeSyntaxValueFoundation >/dev/null; echo True
	@./$(BIN) check Tests/GeneralIndexRangeConsumerSemantics >/dev/null; echo True
	@./$(BIN) check Tests/IndexRangeContiguousMemoryIntegrationAudit >/dev/null; echo True
	@./$(BIN) run Tests/ManagedStringsNativeStringAbiIntegrationAudit >/dev/null; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/StringCharacterIndexingEnumerationDiagnostics/WrongIndexType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "string index must be 'int', 'Index', or 'Range', got 'bool'" $$out; rm -f $$out; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.304"' Tests/StringRangeSlicingSubstring/StringRangeSlicingSubstring.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True


.PHONY: test-string-concatenation-composition

# Milestone 305: String Concatenation & Composition
test-string-concatenation-composition: $(BIN)
	@./$(BIN) check Tests/StringConcatenationComposition >/dev/null; echo True
	@set -e; ./$(BIN) build Tests/StringConcatenationComposition >/dev/null; test -x Tests/StringConcatenationComposition/bin/StringConcatenationComposition$(EXE_SUFFIX); echo True
	@timeout 20s ./Tests/StringConcatenationComposition/bin/StringConcatenationComposition
	@set -e; ./$(BIN) publish Tests/StringConcatenationComposition >/dev/null; test -x Tests/StringConcatenationComposition/publish/StringConcatenationComposition$(EXE_SUFFIX); echo True
	@set -e; c=Tests/StringConcatenationComposition/.void/StringConcatenationComposition.c; grep -Fq 'vc_string_concat_ss' $$c; grep -Fq 'vc_string_concat_sc' $$c; grep -Fq 'vc_string_concat_cs' $$c; grep -Fq 'vc_string_alloc_known_valid' $$c; grep -Fq 'vc_utf8_encode_scalar' $$c; grep -Fq 'string concatenation is too large' $$c; ! grep -Fq 'strcat(' $$c; echo True
	@./$(BIN) check Tests/StringConcatenationCompositionDiagnostics/InvalidRightOperand >/dev/null; echo True
	@./$(BIN) check Tests/StringConcatenationCompositionDiagnostics/InvalidLeftOperand >/dev/null; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/StringConcatenationCompositionDiagnostics/ConcatWrongArity >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'string.Concat expects a string operand paired with string, char, or primitive value' $$out; rm -f $$out; echo True
	@./$(BIN) check Tests/StringConcatenationCompositionDiagnostics/ConcatWrongType >/dev/null; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/StringConcatenationCompositionDiagnostics/InvalidCompound >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "operator '-' is not defined for 'string' and 'string'" $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/StringRangeSlicingSubstring >/dev/null; echo True
	@./$(BIN) run Tests/StringCharacterIndexingEnumeration >/dev/null; echo True
	@./$(BIN) run Tests/StringCoreProperties >/dev/null; echo True
	@./$(BIN) check Tests/Utf8ValidityUnicodeScalarFoundation >/dev/null; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.305"' Tests/StringConcatenationComposition/StringConcatenationComposition.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True

.PHONY: test-ordinal-string-search-comparison

# Milestone 306: Ordinal String Search & Comparison
test-ordinal-string-search-comparison: $(BIN)
	@./$(BIN) check Tests/OrdinalStringSearchComparison >/dev/null; echo True
	@set -e; ./$(BIN) build Tests/OrdinalStringSearchComparison >/dev/null; test -x Tests/OrdinalStringSearchComparison/bin/OrdinalStringSearchComparison$(EXE_SUFFIX); echo True
	@timeout 20s ./Tests/OrdinalStringSearchComparison/bin/OrdinalStringSearchComparison
	@set -e; ./$(BIN) publish Tests/OrdinalStringSearchComparison >/dev/null; test -x Tests/OrdinalStringSearchComparison/publish/OrdinalStringSearchComparison$(EXE_SUFFIX); echo True
	@set -e; c=Tests/OrdinalStringSearchComparison/.void/OrdinalStringSearchComparison.c; grep -Fq 'vc_string_compare_ordinal' $$c; grep -Fq 'vc_string_index_of_string' $$c; grep -Fq 'vc_string_last_index_of_string' $$c; grep -Fq 'vc_string_index_of_char' $$c; grep -Fq 'vc_string_last_index_of_char' $$c; grep -Fq 'vc_utf8_decode_one' $$c; grep -Fq 'string search value is null' $$c; ! grep -Fq 'strstr(' $$c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/OrdinalStringSearchComparisonDiagnostics/ContainsWrongArity >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'string.Contains expects (string value)' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/OrdinalStringSearchComparisonDiagnostics/ContainsWrongType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'string.Contains expects (string value)' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/OrdinalStringSearchComparisonDiagnostics/IndexOfWrongType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'string.IndexOf expects (string value) or (char value)' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/OrdinalStringSearchComparisonDiagnostics/StaticEqualsWrongArity >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'string.Equals expects (string left, string right)' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/OrdinalStringSearchComparisonDiagnostics/CompareOrdinalWrongType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'string.CompareOrdinal expects (string left, string right)' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/OrdinalStringSearchComparisonDiagnostics/NullReceiver >/dev/null; if Tests/OrdinalStringSearchComparisonDiagnostics/NullReceiver/bin/OrdinalStringSearchComparisonDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string reference is null' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/OrdinalStringSearchComparisonDiagnostics/NullSearchValue >/dev/null; if Tests/OrdinalStringSearchComparisonDiagnostics/NullSearchValue/bin/OrdinalStringSearchComparisonDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string search value is null' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/StringConcatenationComposition >/dev/null; echo True
	@./$(BIN) run Tests/StringRangeSlicingSubstring >/dev/null; echo True
	@./$(BIN) run Tests/StringCharacterIndexingEnumeration >/dev/null; echo True
	@./$(BIN) run Tests/StringCoreProperties >/dev/null; echo True
	@./$(BIN) check Tests/Utf8ValidityUnicodeScalarFoundation >/dev/null; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.306"' Tests/OrdinalStringSearchComparison/OrdinalStringSearchComparison.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True



.PHONY: test-immutable-string-editing

# Milestone 307: Immutable String Editing Operations
test-immutable-string-editing: $(BIN)
	@./$(BIN) check Tests/ImmutableStringEditing >/dev/null; echo True
	@set -e; ./$(BIN) build Tests/ImmutableStringEditing >/dev/null; test -x Tests/ImmutableStringEditing/bin/ImmutableStringEditing$(EXE_SUFFIX); echo True
	@timeout 20s ./Tests/ImmutableStringEditing/bin/ImmutableStringEditing
	@set -e; ./$(BIN) publish Tests/ImmutableStringEditing >/dev/null; test -x Tests/ImmutableStringEditing/publish/ImmutableStringEditing$(EXE_SUFFIX); echo True
	@set -e; c=Tests/ImmutableStringEditing/.void/ImmutableStringEditing.c; grep -Fq 'vc_string_insert' $$c; grep -Fq 'vc_string_remove_start' $$c; grep -Fq 'vc_string_remove_range' $$c; grep -Fq 'vc_string_replace_string' $$c; grep -Fq 'vc_string_replace_char' $$c; grep -Fq 'vc_utf8_byte_offset' $$c; grep -Fq 'vc_utf8_decode_one' $$c; ! grep -Fq 'strstr(' $$c; echo True
	@set -e; grep -Fq 'VC_SEM_STRING_OP_INSERT' Compiler/include/semantic.h; grep -Fq 'VC_SEM_STRING_OP_REMOVE_RANGE' Compiler/include/semantic.h; grep -Fq 'VC_SEM_STRING_OP_REPLACE_STRING' Compiler/include/semantic.h; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/ImmutableStringEditingDiagnostics/InsertWrongArity >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'string.Insert expects (int start, string value)' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/ImmutableStringEditingDiagnostics/InsertWrongType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'string.Insert expects (int start, string value)' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/ImmutableStringEditingDiagnostics/RemoveWrongType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'string.Remove expects (int start) or (int start, int count)' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/ImmutableStringEditingDiagnostics/RemoveWrongArity >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'string.Remove expects (int start) or (int start, int count)' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/ImmutableStringEditingDiagnostics/ReplaceWrongType >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'string.Replace expects (string oldValue, string newValue) or (char oldChar, char newChar)' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/ImmutableStringEditingDiagnostics/NullReceiver >/dev/null; if Tests/ImmutableStringEditingDiagnostics/NullReceiver/bin/ImmutableStringEditingDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string reference is null' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/ImmutableStringEditingDiagnostics/InsertNullValue >/dev/null; if Tests/ImmutableStringEditingDiagnostics/InsertNullValue/bin/ImmutableStringEditingDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string insert value is null' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/ImmutableStringEditingDiagnostics/InsertOutOfRange >/dev/null; if Tests/ImmutableStringEditingDiagnostics/InsertOutOfRange/bin/ImmutableStringEditingDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string insert index out of range' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/ImmutableStringEditingDiagnostics/RemoveOutOfRange >/dev/null; if Tests/ImmutableStringEditingDiagnostics/RemoveOutOfRange/bin/ImmutableStringEditingDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string remove range out of range' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/ImmutableStringEditingDiagnostics/ReplaceOldNull >/dev/null; if Tests/ImmutableStringEditingDiagnostics/ReplaceOldNull/bin/ImmutableStringEditingDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string replace old value is null' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/ImmutableStringEditingDiagnostics/ReplaceOldEmpty >/dev/null; if Tests/ImmutableStringEditingDiagnostics/ReplaceOldEmpty/bin/ImmutableStringEditingDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: string replace old value is empty' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/OrdinalStringSearchComparison >/dev/null; echo True
	@./$(BIN) run Tests/StringConcatenationComposition >/dev/null; echo True
	@./$(BIN) run Tests/StringRangeSlicingSubstring >/dev/null; echo True
	@./$(BIN) run Tests/StringCharacterIndexingEnumeration >/dev/null; echo True
	@./$(BIN) run Tests/StringCoreProperties >/dev/null; echo True
	@./$(BIN) check Tests/Utf8ValidityUnicodeScalarFoundation >/dev/null; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.307"' Tests/ImmutableStringEditing/ImmutableStringEditing.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True


.PHONY: test-string-builder-foundation

# Milestone 308: StringBuilder / Efficient Text Construction Foundation
test-string-builder-foundation: $(BIN)
	@./$(BIN) check Tests/StringBuilderFoundation >/dev/null; echo True
	@set -e; ./$(BIN) build Tests/StringBuilderFoundation >/dev/null; test -x Tests/StringBuilderFoundation/bin/StringBuilderFoundation$(EXE_SUFFIX); echo True
	@timeout 20s ./Tests/StringBuilderFoundation/bin/StringBuilderFoundation
	@set -e; ./$(BIN) publish Tests/StringBuilderFoundation >/dev/null; test -x Tests/StringBuilderFoundation/publish/StringBuilderFoundation$(EXE_SUFFIX); echo True
	@set -e; c=Tests/StringBuilderFoundation/.void/StringBuilderFoundation.c; grep -Fq 'vc_utf8_string_copy_to_array' $$c; grep -Fq 'vc_utf8_encode_char_to_array' $$c; grep -Fq 'vc_string_from_utf8_array' $$c; grep -Fq 'vc_array_new(sizeof(uint8_t)' $$c; ! grep -Fq 'vc_string_builder_' $$c; echo True
	@set -e; f=StandardLibrary/Void/Text/StringBuilder.void; grep -Fq 'namespace Void.Text;' $$f; grep -Fq 'public sealed class StringBuilder' $$f; grep -Fq 'public StringBuilder Append(string value)' $$f; grep -Fq 'public StringBuilder Append(char value)' $$f; grep -Fq 'public StringBuilder AppendLine(string value)' $$f; grep -Fq 'public StringBuilder Clear()' $$f; grep -Fq 'public string ToString()' $$f; grep -Fq 'Runtime.Utf8StringCopy' $$f; grep -Fq 'Runtime.StringFromUtf8' $$f; ! grep -Fq 'Runtime.StringBuilder' $$f; ! grep -Eq 'malloc|realloc|free[(]' $$f; echo True
	@set -e; ./$(BIN) check Tests/StringBuilderFoundationDiagnostics/AppendPrimitive >/dev/null; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/StringBuilderFoundationDiagnostics/RuntimeIntrinsicAccess >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Runtime.Utf8StringCopy is a low-level text primitive reserved for Void.Text implementations' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/StringBuilderFoundationDiagnostics/NegativeCapacity >/dev/null; if Tests/StringBuilderFoundationDiagnostics/NegativeCapacity/bin/StringBuilderFoundationDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: StringBuilder capacity cannot be negative' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/StringBuilderFoundationDiagnostics/InvalidChar >/dev/null; if Tests/StringBuilderFoundationDiagnostics/InvalidChar/bin/StringBuilderFoundationDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'VOID runtime error: char value is not a Unicode scalar value' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/ImmutableStringEditing >/dev/null; echo True
	@./$(BIN) run Tests/OrdinalStringSearchComparison >/dev/null; echo True
	@./$(BIN) run Tests/StringConcatenationComposition >/dev/null; echo True
	@./$(BIN) run Tests/StringRangeSlicingSubstring >/dev/null; echo True
	@./$(BIN) run Tests/StringCharacterIndexingEnumeration >/dev/null; echo True
	@./$(BIN) run Tests/StringCoreProperties >/dev/null; echo True
	@./$(BIN) check Tests/Utf8ValidityUnicodeScalarFoundation >/dev/null; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.308"' Tests/StringBuilderFoundation/StringBuilderFoundation.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; test $$(find StandardLibrary/Void -path '*/StringBuilder.void' -type f | wc -l) -eq 1; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True



.PHONY: test-interpolated-strings-primitive-formatting

# Milestone 309: Interpolated Strings & Primitive Text Formatting
test-interpolated-strings-primitive-formatting: $(BIN)
	@./$(BIN) check Tests/InterpolatedStringsPrimitiveFormatting >/dev/null; echo True
	@set -e; ./$(BIN) build Tests/InterpolatedStringsPrimitiveFormatting >/dev/null; test -x Tests/InterpolatedStringsPrimitiveFormatting/bin/InterpolatedStringsPrimitiveFormatting$(EXE_SUFFIX); echo True
	@timeout 20s ./Tests/InterpolatedStringsPrimitiveFormatting/bin/InterpolatedStringsPrimitiveFormatting
	@set -e; ./$(BIN) publish Tests/InterpolatedStringsPrimitiveFormatting >/dev/null; test -x Tests/InterpolatedStringsPrimitiveFormatting/publish/InterpolatedStringsPrimitiveFormatting$(EXE_SUFFIX); echo True
	@set -e; c=Tests/InterpolatedStringsPrimitiveFormatting/.void/InterpolatedStringsPrimitiveFormatting.c; grep -Fq 'vc_utf8_try_format_i64' $$c; grep -Fq 'vc_utf8_try_format_f64' $$c; grep -Fq 'vc_format_i64' $$c; grep -Fq 'vc_string_concat_s_i64' $$c; grep -Fq 'vc_string_concat_decimal_s' $$c; ! grep -Fq 'vc_string_builder_' $$c; echo True
	@set -e; f=StandardLibrary/Void/Text/StringBuilder.void; grep -Fq 'public StringBuilder Append(bool value)' $$f; grep -Fq 'public StringBuilder Append(int value)' $$f; grep -Fq 'public StringBuilder Append(ulong value)' $$f; grep -Fq 'public StringBuilder Append(float value)' $$f; grep -Fq 'public StringBuilder Append(double value)' $$f; grep -Fq 'public StringBuilder Append(decimal value)' $$f; grep -Fq 'Utf8Formatter.TryFormat' $$f; ! grep -Fq 'Runtime.StringBuilder' $$f; ! grep -Eq 'malloc|realloc|free[(]' $$f; echo True
	@set -e; f=StandardLibrary/Void/Buffers/Text/Utf8Formatter.void; grep -Fq 'public static class Utf8Formatter' $$f; grep -Fq 'public static bool TryFormat(int value, Span<byte> destination, out int bytesWritten, StandardFormat format = default)' $$f; grep -Fq 'Runtime.Utf8TryFormatInt64' $$f; echo True
	@set -e; f=StandardLibrary/Void/Buffers/StandardFormat.void; grep -Fq 'public readonly struct StandardFormat' $$f; grep -Fq 'public char Symbol' $$f; grep -Fq 'public byte Precision' $$f; grep -Fq 'public bool IsDefault' $$f; echo True
	@set -e; grep -Fq 'VC_TOKEN_INTERPOLATED_START' Compiler/include/lexer.h; grep -Fq 'lex_interpolated' Compiler/src/lexer.c; grep -Fq 'parse_interpolated_string' Compiler/src/parser.c; grep -Fq 'Void.Text.StringBuilder' Compiler/src/parser.c; grep -Fq 'is_primitive_text_formattable' Compiler/src/semantic.c; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/InterpolatedStringsPrimitiveFormattingDiagnostics/EmptyExpression >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'interpolation expression cannot be empty' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/InterpolatedStringsPrimitiveFormattingDiagnostics/UnescapedClosingBrace >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "unescaped '}' in interpolated string literal" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/InterpolatedStringsPrimitiveFormattingDiagnostics/UnterminatedExpression >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'unterminated interpolated string literal' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/InterpolatedStringsPrimitiveFormattingDiagnostics/UnsupportedValue >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "type 'StringBuilder' has no matching instance method 'Append'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/InterpolatedStringsPrimitiveFormattingDiagnostics/RuntimeIntrinsicAccess >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Runtime.Utf8TryFormatInt64 is a low-level UTF-8 formatting primitive reserved for Void.Buffers.Text.Utf8Formatter' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/InterpolatedStringsPrimitiveFormattingDiagnostics/UnsupportedFormatSpecifier >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq "expected '}' after interpolation expression, found ':'" $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/InterpolatedStringsPrimitiveFormattingDiagnostics/UnsupportedStandardFormat >/dev/null; if Tests/InterpolatedStringsPrimitiveFormattingDiagnostics/UnsupportedStandardFormat/bin/InterpolatedStringsPrimitiveFormattingDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled FormatException: non-default StandardFormat is not implemented yet' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/StringBuilderFoundation >/dev/null; echo True
	@./$(BIN) run Tests/ImmutableStringEditing >/dev/null; echo True
	@./$(BIN) run Tests/OrdinalStringSearchComparison >/dev/null; echo True
	@./$(BIN) run Tests/StringConcatenationComposition >/dev/null; echo True
	@./$(BIN) run Tests/StringRangeSlicingSubstring >/dev/null; echo True
	@./$(BIN) run Tests/StringCharacterIndexingEnumeration >/dev/null; echo True
	@./$(BIN) run Tests/StringCoreProperties >/dev/null; echo True
	@./$(BIN) check Tests/Utf8ValidityUnicodeScalarFoundation >/dev/null; echo True
	@./$(BIN) run Tests/GenericMethodCalls >/dev/null; echo True
	@./$(BIN) run Tests/NamedArguments >/dev/null; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.309"' Tests/InterpolatedStringsPrimitiveFormatting/InterpolatedStringsPrimitiveFormatting.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; test $$(find StandardLibrary/Void -path '*/StringBuilder.void' -type f | wc -l) -eq 1; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True


.PHONY: test-core-strings-utf8-text-construction-integration-audit

# Milestone 310: Core Strings, UTF-8 & Text Construction Integration Audit
test-core-strings-utf8-text-construction-integration-audit: $(BIN)
	@./$(BIN) check Tests/CoreStringsUtf8TextConstructionIntegrationAudit >/dev/null; echo True
	@set -e; ./$(BIN) build Tests/CoreStringsUtf8TextConstructionIntegrationAudit >/dev/null; test -x Tests/CoreStringsUtf8TextConstructionIntegrationAudit/bin/CoreStringsUtf8TextConstructionIntegrationAudit$(EXE_SUFFIX); echo True
	@timeout 20s ./Tests/CoreStringsUtf8TextConstructionIntegrationAudit/bin/CoreStringsUtf8TextConstructionIntegrationAudit
	@set -e; ./$(BIN) publish Tests/CoreStringsUtf8TextConstructionIntegrationAudit >/dev/null; test -x Tests/CoreStringsUtf8TextConstructionIntegrationAudit/publish/CoreStringsUtf8TextConstructionIntegrationAudit$(EXE_SUFFIX); echo True
	@set -e; f=StandardLibrary/Void/Text/StringBuilder.void; grep -Fq 'public int Capacity' $$f; grep -Fq 'public int MaxCapacity' $$f; grep -Fq 'public StringBuilder(int capacity, int maxCapacity)' $$f; grep -Fq 'public StringBuilder(string value)' $$f; grep -Fq 'public StringBuilder(string value, int capacity)' $$f; grep -Fq 'this.Append(Environment.NewLine)' $$f; grep -Fq 'internal int ByteLength' $$f; grep -Fq 'internal int ByteCapacity' $$f; ! grep -Fq 'Runtime.StringBuilder' $$f; echo True
	@set -e; f=StandardLibrary/Void/Text/Encoding.void; grep -Fq 'public abstract class Encoding' $$f; grep -Fq 'public static Encoding UTF8' $$f; grep -Fq 'public sealed class UTF8Encoding' $$f; grep -Fq 'public override int GetByteCount(string value)' $$f; grep -Fq 'public override byte[] GetBytes(string value)' $$f; grep -Fq 'public override string GetString(byte[] bytes)' $$f; echo True
	@set -e; f=StandardLibrary/Void/Environment.void; grep -Fq 'public static class Environment' $$f; grep -Fq 'public static string NewLine' $$f; grep -Fq 'Runtime.PlatformNewLine()' $$f; echo True
	@set -e; grep -Fq 'vc_platform_new_line' Compiler/src/compiler.c; grep -Fq 'Runtime.PlatformNewLine is reserved for Void.Environment' Compiler/src/semantic.c; grep -Fq 'reserved for Void.Text implementations' Compiler/src/semantic.c; ! grep -R -Fq 'Runtime.StringBuilder' Compiler StandardLibrary; ! grep -R -Fq 'vc_string_builder_' Compiler StandardLibrary; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/CoreStringsUtf8TextConstructionIntegrationAuditDiagnostics/CapacityBelowLength >/dev/null; if Tests/CoreStringsUtf8TextConstructionIntegrationAuditDiagnostics/CapacityBelowLength/bin/CoreStringsUtf8TextConstructionIntegrationAuditDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: StringBuilder capacity cannot be less than Length' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/CoreStringsUtf8TextConstructionIntegrationAuditDiagnostics/MaxCapacityExceeded >/dev/null; if Tests/CoreStringsUtf8TextConstructionIntegrationAuditDiagnostics/MaxCapacityExceeded/bin/CoreStringsUtf8TextConstructionIntegrationAuditDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentOutOfRangeException: StringBuilder length exceeds MaxCapacity' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); ./$(BIN) build Tests/CoreStringsUtf8TextConstructionIntegrationAuditDiagnostics/EncodingNull >/dev/null; if Tests/CoreStringsUtf8TextConstructionIntegrationAuditDiagnostics/EncodingNull/bin/CoreStringsUtf8TextConstructionIntegrationAuditDiagnostics >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Unhandled ArgumentNullException: Encoding.GetBytes value cannot be null' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/CoreStringsUtf8TextConstructionIntegrationAuditDiagnostics/RuntimePlatformNewLineAccess >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Runtime.PlatformNewLine is reserved for Void.Environment' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/CoreStringsUtf8TextConstructionIntegrationAuditDiagnostics/RuntimeUtf8PrimitiveAccess >$$out 2>&1; then rm -f $$out; exit 1; fi; grep -Fq 'Runtime.Utf8StringByteCount is a low-level text primitive reserved for Void.Text implementations' $$out; rm -f $$out; echo True
	@./$(BIN) run Tests/InterpolatedStringsPrimitiveFormatting >/dev/null; echo True
	@./$(BIN) run Tests/StringBuilderFoundation >/dev/null; echo True
	@./$(BIN) run Tests/ImmutableStringEditing >/dev/null; echo True
	@./$(BIN) run Tests/OrdinalStringSearchComparison >/dev/null; echo True
	@./$(BIN) run Tests/StringConcatenationComposition >/dev/null; echo True
	@./$(BIN) run Tests/StringRangeSlicingSubstring >/dev/null; echo True
	@./$(BIN) run Tests/StringCharacterIndexingEnumeration >/dev/null; echo True
	@./$(BIN) run Tests/StringCoreProperties >/dev/null; echo True
	@./$(BIN) check Tests/Utf8ValidityUnicodeScalarFoundation >/dev/null; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.310"' Tests/CoreStringsUtf8TextConstructionIntegrationAudit/CoreStringsUtf8TextConstructionIntegrationAudit.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; ! grep -R -Fq 'cstring' Compiler StandardLibrary; echo True

.PHONY: test-standard-library-compatibility-cleanup
test-standard-library-compatibility-cleanup: $(BIN)
	@$(PYTHON) Tests/test_standard_library_cleanup.py

.PHONY: test-console-read-line-foundation
test-console-read-line-foundation: $(BIN)
	@$(PYTHON) Tests/test_console_read_line_foundation.py
	@$(PYTHON) Tests/test_standard_library_namespace_layout.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.311"' Tests/ConsoleReadLineFoundation/ConsoleReadLineFoundation.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True

.PHONY: test-console-read-shared-input-buffering
test-console-read-shared-input-buffering: $(BIN)
	@$(PYTHON) Tests/test_console_read_shared_input_buffering.py
	@$(PYTHON) Tests/test_console_read_line_foundation.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_namespace_layout.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_cleanup.py >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.312"' Tests/ConsoleReadSharedInputBuffering/ConsoleReadSharedInputBuffering.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True

.PHONY: test-console-text-reader-writer
test-console-text-reader-writer: $(BIN)
	@$(PYTHON) Tests/test_console_text_reader_writer.py
	@$(PYTHON) Tests/test_console_read_shared_input_buffering.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_read_line_foundation.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_namespace_layout.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_cleanup.py >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.313"' Tests/ConsoleTextReaderWriter/ConsoleTextReaderWriter.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True

.PHONY: test-console-redirection
test-console-redirection: $(BIN)
	@$(PYTHON) Tests/test_console_redirection.py
	@$(PYTHON) Tests/test_console_text_reader_writer.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_read_shared_input_buffering.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_read_line_foundation.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_namespace_layout.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_cleanup.py >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.314"' Tests/ConsoleRedirection/ConsoleRedirection.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True


.PHONY: test-console-standard-streams
test-console-standard-streams: $(BIN)
	@$(PYTHON) Tests/test_console_standard_streams.py
	@$(PYTHON) Tests/test_console_redirection.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_text_reader_writer.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_read_shared_input_buffering.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_read_line_foundation.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_namespace_layout.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_cleanup.py >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.315"' Tests/ConsoleStandardStreams/ConsoleStandardStreams.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True


.PHONY: test-console-encoding-integration
# Milestone 316: Console Encoding Integration
test-console-encoding-integration: $(BIN)
	@$(PYTHON) Tests/test_console_encoding_integration.py
	@$(PYTHON) Tests/test_console_standard_streams.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_redirection.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_text_reader_writer.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_read_shared_input_buffering.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_read_line_foundation.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_namespace_layout.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_cleanup.py >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.316"' Tests/ConsoleEncodingIntegration/ConsoleEncodingIntegration.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True


.PHONY: test-console-key-read-key
# Milestone 317: ConsoleKey, ConsoleModifiers, ConsoleKeyInfo & ReadKey
test-console-key-read-key: $(BIN)
	@$(PYTHON) Tests/test_console_key_read_key.py
	@$(PYTHON) Tests/test_console_encoding_integration.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_standard_streams.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_redirection.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_text_reader_writer.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_read_shared_input_buffering.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_read_line_foundation.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_namespace_layout.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_cleanup.py >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.317"' Tests/ConsoleKeyReadKey/ConsoleKeyReadKey.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True

.PHONY: test-console-input-control
# Milestone 318: KeyAvailable, Ctrl+C & Console Input Control
test-console-input-control: $(BIN)
	@$(PYTHON) Tests/test_console_input_control.py
	@$(PYTHON) Tests/test_console_key_read_key.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_encoding_integration.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_standard_streams.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_redirection.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_text_reader_writer.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_read_shared_input_buffering.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_read_line_foundation.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_namespace_layout.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_cleanup.py >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.318"' Tests/ConsoleInputControl/ConsoleInputControl.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True

.PHONY: test-console-presentation-basics
# Milestone 319: Interactive Console Presentation Basics
test-console-presentation-basics: $(BIN)
	@$(PYTHON) Tests/test_console_presentation_basics.py
	@$(PYTHON) Tests/test_console_input_control.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_key_read_key.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_encoding_integration.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_standard_streams.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_redirection.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_text_reader_writer.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_read_shared_input_buffering.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_read_line_foundation.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_namespace_layout.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_cleanup.py >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.319"' Tests/ConsolePresentationBasics/ConsolePresentationBasics.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True

.PHONY: test-console-standard-io-terminal-integration-audit
# Milestone 320: Console, Standard IO & Terminal Integration Audit
test-console-standard-io-terminal-integration-audit: $(BIN)
	@$(PYTHON) Tests/test_console_standard_io_terminal_integration_audit.py
	@$(PYTHON) Tests/test_console_presentation_basics.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_input_control.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_key_read_key.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_encoding_integration.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_standard_streams.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_redirection.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_text_reader_writer.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_read_shared_input_buffering.py >/dev/null && echo True
	@$(PYTHON) Tests/test_console_read_line_foundation.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_namespace_layout.py >/dev/null && echo True
	@$(PYTHON) Tests/test_standard_library_cleanup.py >/dev/null && echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.320"' Tests/ConsoleStandardIoTerminalIntegrationAudit/ConsoleStandardIoTerminalIntegrationAudit.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True

.PHONY: test-engine-list-operations test-engine-set-operations test-engine-memory-stream test-engine-readiness
test-engine-list-operations: $(BIN)
	@$(PYTHON) Tests/test_engine_readiness.py list
test-engine-set-operations: $(BIN)
	@$(PYTHON) Tests/test_engine_readiness.py set
test-engine-memory-stream: $(BIN)
	@$(PYTHON) Tests/test_engine_readiness.py stream
test-engine-readiness: test-engine-list-operations test-engine-set-operations test-engine-memory-stream

.PHONY: test-interface-indexer-dispatch test-engineering-pass
test-interface-indexer-dispatch: $(BIN)
	@$(PYTHON) Tests/test_interface_indexer_dispatch.py
test-engineering-pass: test-interface-indexer-dispatch test-engine-filesystem test-engine-readiness

.PHONY: test-engine-filesystem
test-engine-filesystem: $(BIN)
	@$(PYTHON) Tests/test_engine_filesystem.py

.PHONY: test-structured-diagnostic-foundation
test-structured-diagnostic-foundation: $(BIN)
	@$(PYTHON) Tests/test_rich_diagnostics.py 321

.PHONY: test-source-span-rendering
test-source-span-rendering: $(BIN)
	@$(PYTHON) Tests/test_rich_diagnostics.py 322

.PHONY: test-diagnostic-notes
test-diagnostic-notes: $(BIN)
	@$(PYTHON) Tests/test_rich_diagnostics.py 323

.PHONY: test-actionable-help-diagnostics
test-actionable-help-diagnostics: $(BIN)
	@$(PYTHON) Tests/test_rich_diagnostics.py 324

.PHONY: test-parser-recovery-diagnostics
test-parser-recovery-diagnostics: $(BIN)
	@$(PYTHON) Tests/test_rich_diagnostics.py 325

.PHONY: test-semantic-explanation-diagnostics
test-semantic-explanation-diagnostics: $(BIN)
	@$(PYTHON) Tests/test_rich_diagnostics.py 326

.PHONY: test-related-source-locations
test-related-source-locations: $(BIN)
	@$(PYTHON) Tests/test_rich_diagnostics.py 327

.PHONY: test-stable-diagnostic-codes
test-stable-diagnostic-codes: $(BIN)
	@$(PYTHON) Tests/test_rich_diagnostics.py 328

.PHONY: test-machine-readable-diagnostics
test-machine-readable-diagnostics: $(BIN)
	@$(PYTHON) Tests/test_rich_diagnostics.py 329

.PHONY: test-rich-diagnostics-integration-audit test-rich-diagnostics
test-rich-diagnostics-integration-audit: $(BIN)
	@$(PYTHON) Tests/test_rich_diagnostics.py 330

test-rich-diagnostics: test-structured-diagnostic-foundation test-source-span-rendering test-diagnostic-notes test-actionable-help-diagnostics test-parser-recovery-diagnostics test-semantic-explanation-diagnostics test-related-source-locations test-stable-diagnostic-codes test-machine-readable-diagnostics test-rich-diagnostics-integration-audit

.PHONY: test-host-process-portability
test-host-process-portability: $(BIN)
	@$(PYTHON) Tests/HostProcessPortability/test.py

.PHONY: test-runtime-crash-diagnostics
test-runtime-crash-diagnostics: $(BIN)
	@$(PYTHON) Tests/RuntimeCrashDiagnostics/test.py

.PHONY: test-type-qualified-member-access-foundation
TYPE_RECEIVER_MODEL_TEST := Tests/TypeQualifiedMemberAccessFoundation/bin/model-test$(EXE_SUFFIX)
$(TYPE_RECEIVER_MODEL_TEST): Tests/TypeQualifiedMemberAccessFoundation/model_test.c $(SOURCES) $(HEADERS)
	@mkdir -p Tests/TypeQualifiedMemberAccessFoundation/bin
	$(CC) $(CPPFLAGS) $(CFLAGS) Tests/TypeQualifiedMemberAccessFoundation/model_test.c Compiler/src/ast.c Compiler/src/lexer.c Compiler/src/parser.c Compiler/src/monomorph.c Compiler/src/semantic.c Compiler/src/diagnostic.c -o $@

test-type-qualified-member-access-foundation: $(BIN) $(TYPE_RECEIVER_MODEL_TEST)
	@$(TYPE_RECEIVER_MODEL_TEST)
	@$(PYTHON) Tests/test_type_qualified_member_access_foundation.py

.PHONY: test-static-value-type-receivers
STATIC_VALUE_MODEL_TEST := Tests/StaticValueTypeReceivers/bin/model-test$(EXE_SUFFIX)
$(STATIC_VALUE_MODEL_TEST): Tests/StaticValueTypeReceivers/model_test.c $(SOURCES) $(HEADERS)
	@mkdir -p Tests/StaticValueTypeReceivers/bin
	$(CC) $(CPPFLAGS) $(CFLAGS) Tests/StaticValueTypeReceivers/model_test.c Compiler/src/ast.c Compiler/src/lexer.c Compiler/src/parser.c Compiler/src/monomorph.c Compiler/src/semantic.c Compiler/src/diagnostic.c -o $@

test-static-value-type-receivers: $(BIN) $(STATIC_VALUE_MODEL_TEST)
	@$(STATIC_VALUE_MODEL_TEST)
	@$(PYTHON) Tests/test_static_value_type_receivers.py
.PHONY: test-static-api-integration
# Milestone 339: Static API Integration & Generic Default Surfaces
test-static-api-integration: $(BIN)
	@$(PYTHON) Tests/test_static_api_integration.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.339"' Tests/StaticApiIntegration/StaticApiIntegration.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True

.PHONY: test-builtin-floating-decimal-parsing
# Milestone 338: Floating-Point & Decimal Parsing Completion
test-builtin-floating-decimal-parsing: $(BIN)
	@$(PYTHON) Tests/test_builtin_floating_decimal_parsing.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.338"' Tests/BuiltInFloatingDecimalParsing/BuiltInFloatingDecimalParsing.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True

.PHONY: test-builtin-integral-bool-char-parsing
# Milestone 337: Integral, Boolean & Character Parsing Foundation
test-builtin-integral-bool-char-parsing: $(BIN)
	@$(PYTHON) Tests/test_builtin_integral_bool_char_parsing.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.337"' Tests/BuiltInIntegralBoolCharParsing/BuiltInIntegralBoolCharParsing.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-builtin-static-values
BUILTIN_STATIC_VALUES_MODEL_TEST := Tests/BuiltInStaticValues/bin/model-test$(EXE_SUFFIX)
$(BUILTIN_STATIC_VALUES_MODEL_TEST): Tests/BuiltInStaticValues/model_test.c $(SOURCES) $(HEADERS)
	@mkdir -p Tests/BuiltInStaticValues/bin
	$(CC) $(CPPFLAGS) $(CFLAGS) Tests/BuiltInStaticValues/model_test.c Compiler/src/ast.c Compiler/src/lexer.c Compiler/src/parser.c Compiler/src/monomorph.c Compiler/src/semantic.c Compiler/src/diagnostic.c -o $@


test-builtin-static-values: $(BIN) $(BUILTIN_STATIC_VALUES_MODEL_TEST)
	@$(BUILTIN_STATIC_VALUES_MODEL_TEST)
	@$(PYTHON) Tests/test_builtin_static_values.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.336"' Tests/BuiltInStaticValues/BuiltInStaticValues.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-builtin-associated-member-foundation
BUILTIN_ASSOCIATED_MEMBER_MODEL_TEST := Tests/BuiltInAssociatedMemberFoundation/bin/model-test$(EXE_SUFFIX)
$(BUILTIN_ASSOCIATED_MEMBER_MODEL_TEST): Tests/BuiltInAssociatedMemberFoundation/model_test.c $(SOURCES) $(HEADERS)
	@mkdir -p Tests/BuiltInAssociatedMemberFoundation/bin
	$(CC) $(CPPFLAGS) $(CFLAGS) Tests/BuiltInAssociatedMemberFoundation/model_test.c Compiler/src/ast.c Compiler/src/lexer.c Compiler/src/parser.c Compiler/src/monomorph.c Compiler/src/semantic.c Compiler/src/diagnostic.c -o $@

test-builtin-associated-member-foundation: $(BIN) $(BUILTIN_ASSOCIATED_MEMBER_MODEL_TEST)
	@$(BUILTIN_ASSOCIATED_MEMBER_MODEL_TEST)
	@$(PYTHON) Tests/test_builtin_associated_member_foundation.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.335"' Tests/BuiltInAssociatedMemberFoundation/BuiltInAssociatedMemberFoundation.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-constructed-generic-static-members
CONSTRUCTED_GENERIC_STATIC_MODEL_TEST := Tests/ConstructedGenericStaticMembers/bin/model-test$(EXE_SUFFIX)
$(CONSTRUCTED_GENERIC_STATIC_MODEL_TEST): Tests/ConstructedGenericStaticMembers/model_test.c $(SOURCES) $(HEADERS)
	@mkdir -p Tests/ConstructedGenericStaticMembers/bin
	$(CC) $(CPPFLAGS) $(CFLAGS) Tests/ConstructedGenericStaticMembers/model_test.c Compiler/src/ast.c Compiler/src/lexer.c Compiler/src/parser.c Compiler/src/monomorph.c Compiler/src/semantic.c Compiler/src/diagnostic.c -o $@

test-constructed-generic-static-members: $(BIN) $(CONSTRUCTED_GENERIC_STATIC_MODEL_TEST)
	@$(CONSTRUCTED_GENERIC_STATIC_MODEL_TEST)
	@$(PYTHON) Tests/test_constructed_generic_static_members.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.334"' Tests/ConstructedGenericStaticMembers/ConstructedGenericStaticMembers.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-static-callable-type-receivers
STATIC_CALLABLE_MODEL_TEST := Tests/StaticCallableTypeReceivers/bin/model-test$(EXE_SUFFIX)
$(STATIC_CALLABLE_MODEL_TEST): Tests/StaticCallableTypeReceivers/model_test.c $(SOURCES) $(HEADERS)
	@mkdir -p Tests/StaticCallableTypeReceivers/bin
	$(CC) $(CPPFLAGS) $(CFLAGS) Tests/StaticCallableTypeReceivers/model_test.c Compiler/src/ast.c Compiler/src/lexer.c Compiler/src/parser.c Compiler/src/monomorph.c Compiler/src/semantic.c Compiler/src/diagnostic.c -o $@

test-static-callable-type-receivers: $(BIN) $(STATIC_CALLABLE_MODEL_TEST)
	@$(STATIC_CALLABLE_MODEL_TEST)
	@$(PYTHON) Tests/test_static_callable_type_receivers.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.333"' Tests/StaticCallableTypeReceivers/StaticCallableTypeReceivers.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; echo True

.PHONY: test-type-qualified-static-members-builtins-integration-audit
# Milestone 340: whole-block composition audit; historical suites remain separate.
test-type-qualified-static-members-builtins-integration-audit: $(BIN)
	@$(PYTHON) Tests/test_type_qualified_static_members_builtins_integration_audit.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True

.PHONY: test-inline-typed-out-variable-declarations
# Milestone 341: inline typed out-variable declarations with ordinary local/out integration.
test-inline-typed-out-variable-declarations: $(BIN)
	@$(PYTHON) Tests/test_inline_typed_out_variable_declarations.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.341"' Tests/InlineTypedOutVariableDeclarations/InlineTypedOutVariableDeclarations.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True
.PHONY: test-out-var-type-inference-overload-integration
# Milestone 342: out var type inference and overload integration.
test-out-var-type-inference-overload-integration: $(BIN)
	@$(PYTHON) Tests/test_out_var_type_inference_overload_integration.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.342"' Tests/OutVarTypeInferenceOverloadIntegration/OutVarTypeInferenceOverloadIntegration.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True
.PHONY: test-out-variable-scope-flow-discard-completion
# Milestone 343: inline out-variable scope/flow completion and context-specific out discard.
test-out-variable-scope-flow-discard-completion: $(BIN)
	@$(PYTHON) Tests/test_out_variable_scope_flow_discard_completion.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.343"' Tests/OutVariableScopeFlowDiscardCompletion/OutVariableScopeFlowDiscardCompletion.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True


.PHONY: test-runtime-source-excerpt-diagnostic-foundation
# Milestone 344: runtime source-excerpt diagnostic foundation.
test-runtime-source-excerpt-diagnostic-foundation: $(BIN)
	@$(PYTHON) Tests/test_runtime_source_excerpt_diagnostic_foundation.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.344"' Tests/RuntimeSourceExcerptDiagnosticFoundation/RuntimeSourceExcerptDiagnosticFoundation.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True
.PHONY: test-synchronous-void-call-stack-foundation
# Milestone 345: truthful per-thread synchronous VOID call-stack foundation.
test-synchronous-void-call-stack-foundation: $(BIN)
	@$(PYTHON) Tests/test_synchronous_void_call_stack_foundation.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.345"' Tests/SynchronousVoidCallStackFoundation/SynchronousVoidCallStackFoundation.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True
.PHONY: test-exception-rethrow-runtime-failure-stack-preservation
# Milestone 346: exception/rethrow/runtime-failure captured stack preservation.
test-exception-rethrow-runtime-failure-stack-preservation: $(BIN)
	@$(PYTHON) Tests/test_exception_rethrow_runtime_failure_stack_preservation.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.346"' Tests/ExceptionRethrowRuntimeFailureStackPreservation/ExceptionRethrowRuntimeFailureStackPreservation.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True

.PHONY: test-unified-runtime-diagnostic-presentation
# Milestone 347: unified runtime diagnostic presentation over source excerpts and truthful stacks.
test-unified-runtime-diagnostic-presentation: $(BIN)
	@$(PYTHON) Tests/test_unified_runtime_diagnostic_presentation.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.347"' Tests/UnifiedRuntimeDiagnosticPresentation/UnifiedRuntimeDiagnosticPresentation.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True

.PHONY: test-source-availability-publish-native-boundary-fallbacks
# Milestone 348: truthful source availability/publish/native-boundary fallbacks.
test-source-availability-publish-native-boundary-fallbacks: $(BIN)
	@$(PYTHON) Tests/test_source_availability_publish_native_boundary_fallbacks.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.348"' Tests/SourceAvailabilityPublishNativeBoundaryFallbacks/SourceAvailabilityPublishNativeBoundaryFallbacks.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True

.PHONY: test-runtime-diagnostics-cross-feature-integration
# Milestone 349: cross-feature integration of runtime diagnostics across language/runtime systems.
test-runtime-diagnostics-cross-feature-integration: $(BIN)
	@$(PYTHON) Tests/test_runtime_diagnostics_cross_feature_integration.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version":"0.0.349"' Tests/RuntimeDiagnosticsCrossFeatureIntegration/RuntimeDiagnosticsCrossFeatureIntegration.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'parser.add_argument("--jobs"' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; grep -Fq 'CPPFLAGS ?= -ICompiler/include' Makefile; echo True

.PHONY: test-out-variables-runtime-diagnostics-integration-audit
# Milestone 350: permanent whole-block application/flow/tooling/runtime audit.
test-out-variables-runtime-diagnostics-integration-audit: $(BIN)
	@$(PYTHON) Tests/test_out_variables_runtime_diagnostics_integration_audit.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.350"' Tests/OutVariableIntegrationAudit/OutVariableIntegrationAudit.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2' Makefile; echo True

.PHONY: test-implicit-instance-receiver-binding
# Milestone 351: unqualified instance calls share ordinary overload resolution and receiver binding.
IMPLICIT_RECEIVER_MODEL_TEST := Tests/ImplicitInstanceReceiverBinding/bin/model-test$(EXE_SUFFIX)
$(IMPLICIT_RECEIVER_MODEL_TEST): Tests/ImplicitInstanceReceiverBinding/model_test.c $(SOURCES) $(HEADERS)
	@mkdir -p Tests/ImplicitInstanceReceiverBinding/bin
	$(CC) $(CPPFLAGS) $(CFLAGS) Tests/ImplicitInstanceReceiverBinding/model_test.c Compiler/src/ast.c Compiler/src/lexer.c Compiler/src/parser.c Compiler/src/monomorph.c Compiler/src/semantic.c Compiler/src/diagnostic.c -o $@

test-implicit-instance-receiver-binding: $(BIN) $(IMPLICIT_RECEIVER_MODEL_TEST)
	@$(IMPLICIT_RECEIVER_MODEL_TEST)
	@./$(BIN) build Tests/ImplicitInstanceReceiverBinding >/dev/null
	@./Tests/ImplicitInstanceReceiverBinding/bin/ImplicitInstanceReceiverBinding$(EXE_SUFFIX)
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/ImplicitInstanceReceiverBindingDiagnostics/StaticContext >$$out 2>&1; then cat $$out; rm -f $$out; exit 1; fi; grep -Fq "instance method 'Get' requires an object receiver in a static context" $$out; grep -Fq 'code: VOID3003' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/ImplicitInstanceReceiverBindingDiagnostics/NoMatch >$$out 2>&1; then cat $$out; rm -f $$out; exit 1; fi; grep -Fq "no matching method 'Get' was found" $$out; grep -Fq 'code: VOID3003' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/ImplicitInstanceReceiverBindingDiagnostics/Inaccessible >$$out 2>&1; then cat $$out; rm -f $$out; exit 1; fi; grep -Fq "method 'Hidden' is inaccessible" $$out; grep -Fq 'code: VOID3005' $$out; rm -f $$out; echo True
	@set -e; out=$$(mktemp); if ./$(BIN) check Tests/ImplicitInstanceReceiverBindingDiagnostics/Ambiguous >$$out 2>&1; then cat $$out; rm -f $$out; exit 1; fi; grep -Fq "call to 'Choose' is ambiguous" $$out; grep -Fq 'code: VOID3003' $$out; rm -f $$out; echo True
	@./$(BIN) build Tests/ImplicitInstanceReceiverBindingStrict >/dev/null
	@set -e; thread_flags='-pthread'; if [ "$(OS)" = "Windows_NT" ]; then thread_flags=''; fi; $(CC) $(CFLAGS) -IRuntime/include $$thread_flags Tests/ImplicitInstanceReceiverBindingStrict/.void/ImplicitInstanceReceiverBindingStrict.c Runtime/src/vc_thread.c Runtime/src/vc_atomic.c Runtime/src/vc_memory.c Runtime/src/vc_filesystem.c -o Tests/ImplicitInstanceReceiverBindingStrict/bin/strict-check$(EXE_SUFFIX); ./Tests/ImplicitInstanceReceiverBindingStrict/bin/strict-check$(EXE_SUFFIX) >/dev/null; echo True
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.351"' Tests/ImplicitInstanceReceiverBinding/ImplicitInstanceReceiverBinding.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; echo True

.PHONY: test-ordinary-local-definite-assignment
# Milestone 352: ordinary locals participate in the shared definite-assignment flow state.
test-ordinary-local-definite-assignment: $(BIN)
	@$(PYTHON) Tests/test_ordinary_local_definite_assignment.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.352"' Tests/OrdinaryLocalDefiniteAssignment/OrdinaryLocalDefiniteAssignment.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'test-ordinary-local-definite-assignment' Tests/run_full_suite.py; echo True

.PHONY: test-using-alias-semantic-binding
# Milestone 353: parsed using aliases bind through canonical namespace/type semantics.
test-using-alias-semantic-binding: $(BIN)
	@$(PYTHON) Tests/test_using_alias_semantic_binding.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.353"' Tests/UsingAliasSemanticBinding/UsingAliasSemanticBinding.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'test-using-alias-semantic-binding' Tests/run_full_suite.py; echo True

.PHONY: test-defined-signed-integer-arithmetic-foundation

test-defined-signed-integer-arithmetic-foundation: $(BIN)
	@$(PYTHON) Tests/test_defined_signed_integer_arithmetic_foundation.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.354"' Tests/DefinedSignedIntegerArithmeticFoundation/DefinedSignedIntegerArithmeticFoundation.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'test-defined-signed-integer-arithmetic-foundation' Tests/run_full_suite.py; echo True

.PHONY: test-signed-arithmetic-conversion-boundary-completion
# Milestone 355: defined signed operations, shifts and integral conversions.
test-signed-arithmetic-conversion-boundary-completion: $(BIN)
	@$(PYTHON) Tests/test_signed_arithmetic_conversion_boundary_completion.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.355"' Tests/SignedArithmeticConversionBoundaryCompletion/SignedArithmeticConversionBoundaryCompletion.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'test-signed-arithmetic-conversion-boundary-completion' Tests/run_full_suite.py; echo True

.PHONY: test-strict-generated-c-cleanliness-completion
# Milestone 356: independently strict-compile and run actual generated C.
test-strict-generated-c-cleanliness-completion: $(BIN)
	@$(PYTHON) Tests/test_strict_generated_c_cleanliness_completion.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.356"' Tests/StrictGeneratedCCleanlinessCompletion/StrictGeneratedCCleanlinessCompletion.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'test-strict-generated-c-cleanliness-completion' Tests/run_full_suite.py; echo True

.PHONY: test-literal-host-c-boundary-consistency
# Milestone 357: VOID-owned literal decoding, numeric validation and strict host-C representation.
test-literal-host-c-boundary-consistency: $(BIN)
	@$(PYTHON) Tests/test_literal_host_c_boundary_consistency.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'test-literal-host-c-boundary-consistency' Tests/run_full_suite.py; echo True

.PHONY: test-correctness-cross-feature-integration
# Milestone 358: composed binding, flow, numeric, native, async/iterator, GC and UTF-8 behavior.
test-correctness-cross-feature-integration: $(BIN)
	@$(PYTHON) Tests/test_correctness_cross_feature_integration.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.358"' Tests/CorrectnessCrossFeatureIntegration/CorrectnessCrossFeatureIntegration.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'test-correctness-cross-feature-integration' Tests/run_full_suite.py; echo True

.PHONY: test-correctness-block-stabilization
# Milestone 359: app-style cross-feature stabilization and protected volatile numeric updates.
test-correctness-block-stabilization: $(BIN)
	@$(PYTHON) Tests/test_correctness_block_stabilization.py
	@set -e; grep -Fq '#define VOIDC_VERSION "0.0.360"' Compiler/src/main.c; grep -Fq '"version": "0.0.359"' Tests/CorrectnessBlockStabilization/CorrectnessBlockStabilization.voidproj; grep -Fq 'EXPECTED_TRUE_COUNT = 18994' Tests/run_full_suite.py; grep -Fq 'EXPECTED_FOCUSED_SUITES = 308' Tests/run_full_suite.py; grep -Fq 'DEFAULT_WORKER_COUNT = 3' Tests/run_full_suite.py; grep -Fq 'test-correctness-block-stabilization' Tests/run_full_suite.py; echo True

.PHONY: test-correctness-native-backend-integration-audit
# Milestone 360: unused-body validation and strict native cross-feature audit.
test-correctness-native-backend-integration-audit: $(BIN)
	@$(PYTHON) Tests/test_correctness_native_backend_integration_audit.py
