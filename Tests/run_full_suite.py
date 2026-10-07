#!/usr/bin/env python3
"""Isolated parallel runner for VOID's complete cumulative regression suite.

This runner intentionally delegates focused regression semantics to the existing
Make targets. It only orchestrates them, captures successful output, and runs
independent suites in clean worker copies so generated files cannot race.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import queue
import shutil
import subprocess
import sys
import tempfile
import threading
import time

EXPECTED_TRUE_COUNT = 18687
EXPECTED_FOCUSED_SUITES = 298

FOCUSED_TARGETS = ['test-out-variables-runtime-diagnostics-integration-audit', 'test-runtime-diagnostics-cross-feature-integration', 'test-source-availability-publish-native-boundary-fallbacks', 'test-unified-runtime-diagnostic-presentation', 'test-exception-rethrow-runtime-failure-stack-preservation', 'test-synchronous-void-call-stack-foundation', 'test-runtime-source-excerpt-diagnostic-foundation', 'test-out-variable-scope-flow-discard-completion', 'test-out-var-type-inference-overload-integration', 'test-inline-typed-out-variable-declarations', 'test-type-qualified-static-members-builtins-integration-audit', 'test-static-api-integration', 'test-builtin-floating-decimal-parsing', 'test-builtin-integral-bool-char-parsing', 'test-builtin-static-values', 'test-builtin-associated-member-foundation', 'test-constructed-generic-static-members', 'test-static-callable-type-receivers', 'test-static-value-type-receivers', 'test-type-qualified-member-access-foundation', 'test-runtime-crash-diagnostics', 'test-host-process-portability', 'test-rich-diagnostics-integration-audit', 'test-machine-readable-diagnostics', 'test-stable-diagnostic-codes', 'test-related-source-locations', 'test-semantic-explanation-diagnostics', 'test-parser-recovery-diagnostics', 'test-actionable-help-diagnostics', 'test-diagnostic-notes', 'test-source-span-rendering', 'test-structured-diagnostic-foundation', 'test-engine-filesystem', 'test-interface-indexer-dispatch', 'test-engine-list-operations', 'test-engine-set-operations', 'test-engine-memory-stream', 'test-console-standard-io-terminal-integration-audit',
 'test-console-presentation-basics',
 'test-console-input-control',
 'test-console-key-read-key',
 'test-console-encoding-integration',
 'test-console-standard-streams',
 'test-console-redirection',
 'test-console-text-reader-writer',
 'test-console-read-shared-input-buffering',
 'test-console-read-line-foundation',
 'test-standard-library-compatibility-cleanup',
 'test-diagnostics',
 'test-check',
 'test-semantic-queries',
 'test-debug-info',
 'test-lsp',
 'test-lsp-diagnostics',
 'test-lsp-hover-signature',
 'test-lsp-completion',
 'test-lsp-navigation',
 'test-tooling-integration',
 'test-base-constructors',
 'test-constructor-delegation',
 'test-interface-inheritance',
 'test-struct-interfaces',
 'test-virtual-properties',
 'test-static-events',
 'test-interface-events',
 'test-delegate-completion',
 'test-conditional-expressions',
 'test-object-model-integration',
 'test-nested-value-structs',
 'test-readonly-structs',
 'test-jagged-arrays',
 'test-multidimensional-arrays',
 'test-closure-completion-i',
 'test-closure-completion-ii',
 'test-iterator-completion',
 'test-unsafe-member-completion',
 'test-compound-targets',
 'test-value-types-closures-control-flow-integration',
 'test-static-interface-properties',
 'test-static-interface-events',
 'test-constructor-returns',
 'test-rectangular-array-initializers',
 'test-iterator-locals',
 'test-receiver-expressions',
 'test-generic-method-calls',
 'test-native-callback-by-ref',
 'test-library-output',
 'test-language-edge-project-completion-integration',
 'test-unsafe-delegates',
 'test-unsafe-pointer-properties',
 'test-do-while',
 'test-expression-bodied-members',
 'test-custom-event-accessors',
 'test-null-conditional-arrays',
 'test-sized-rectangular-array-initializers',
 'test-implicit-rectangular-array-initializers',
 'test-generic-constraints',
 'test-language-surface-completion-ii-integration',
 'test-exception-throw-foundation',
 'test-try-catch-completion',
 'test-exception-propagation-gc-unwinding',
 'test-finally-completion',
 'test-structured-control-finally',
 'test-nested-handlers-rethrow',
 'test-iterator-exception-integration',
 'test-delegate-event-exception-integration',
 'test-native-library-boundary-exception-integration',
 'test-exceptions-runtime-control-flow-integration',
 'test-idisposable-iterator-disposal-foundation',
 'test-foreach-disposal-completion',
 'test-using-statement-completion',
 'test-using-declaration-completion',
 'test-generic-method-type-inference',
 'test-extension-method-completion',
 'test-static-interface-method-contracts',
 'test-constrained-static-interface-dispatch',
 'test-default-interface-methods',
 'test-deterministic-lifetime-advanced-abstractions-integration',
 'test-declaration-patterns',
 'test-constant-null-patterns',
 'test-relational-patterns',
 'test-logical-patterns',
 'test-property-patterns',
 'test-recursive-patterns',
 'test-pattern-switch-statements',
 'test-switch-guards-pattern-ordering',
 'test-switch-expressions',
 'test-pattern-control-flow-integration',
 'test-iterator-pattern-local-capture',
 'test-iterator-switch-guard-local-capture',
 'test-iterator-captured-lifetime-gc-integration',
 'test-iterator-state-machine-integration-audit',
 'test-diagnostic-contract-cleanup',
 'test-unary-operator-completion',
 'test-binary-operator-completion',
 'test-static-interface-operator-contracts',
 'test-constrained-generic-operator-dispatch',
 'test-iterator-lifetime-generic-operator-integration',
 'test-standard-conversion-classification-completion',
 'test-user-defined-conversion-resolution-completion',
 'test-lifted-nullable-conversion-completion',
 'test-conversion-site-integration',
 'test-static-interface-conversion-contracts',
 'test-constrained-generic-implicit-conversion-dispatch',
 'test-constrained-generic-explicit-conversion-dispatch',
 'test-generic-conversion-provenance-inference-integration',
 'test-iterator-closure-gc-conversion-integration',
 'test-conversion-semantics-generic-conversion-integration',
 'test-native-thread-runtime-foundation',
 'test-per-thread-runtime-execution-context',
 'test-thread-safe-allocation-runtime-registry',
 'test-cooperative-stop-the-world-gc-safepoints',
 'test-managed-thread-surface',
 'test-monitor-mutual-exclusion-runtime-foundation',
 'test-lock-statement-completion',
 'test-thread-exception-native-boundary-cleanup-integration',
 'test-threaded-closure-delegate-generic-gc-integration',
 'test-native-concurrency-synchronization-integration-audit',
 'test-monitor-condition-wait-runtime-foundation',
 'test-managed-monitor-wait-completion',
 'test-monitor-pulse-completion',
 'test-portable-atomic-runtime-foundation',
 'test-interlocked-integer-operations',
 'test-interlocked-managed-reference-operations-gc-safety',
 'test-volatile-memory-ordering',
 'test-synchronization-exception-native-boundary-cleanup-integration',
 'test-threaded-generic-closure-delegate-synchronization-integration',
 'test-advanced-synchronization-atomic-integration-audit',
 'test-monotonic-time-timed-wait-runtime-foundation',
 'test-managed-thread-sleep-timeout-contract',
 'test-timed-monitor-wait-completion',
 'test-manual-reset-event-completion',
 'test-auto-reset-event-completion',
 'test-semaphore-completion',
 'test-cancellation-token-source-foundation',
 'test-cancellation-registration-wakeup-completion',
 'test-timed-cancellable-synchronization-integration',
 'test-timed-synchronization-cancellation-integration-audit',
 'test-managed-threadpool-work-queue-foundation',
 'test-threadpool-worker-lifecycle-gc-coordination',
 'test-threadpool-exception-shutdown-completion',
 'test-task-state-completion-foundation',
 'test-generic-task-result-gc-completion',
 'test-task-run-threadpool-scheduling-integration',
 'test-task-waiting-timeout-fault-propagation',
 'test-task-cancellation-integration',
 'test-task-continuation-completion-source-foundation',
 'test-threadpool-task-integration-audit',
 'test-async-await-syntax-semantic-foundation',
 'test-async-task-state-machine-lowering-foundation',
 'test-await-suspension-resumption-completion',
 'test-async-task-typed-await-results',
 'test-async-multiple-await-structured-control-flow',
 'test-async-exception-catch-finally-integration',
 'test-async-cancellation-integration',
 'test-async-generics-closures-gc-lifetime-integration',
 'test-async-object-model-library-boundary-integration',
 'test-async-await-task-runtime-integration-audit',
 'test-general-await-expression-lowering-foundation',
 'test-awaited-conditions-expression-evaluation-completion',
 'test-synchronous-foreach-suspension-cleanup-completion',
 'test-awaited-switch-expression-pattern-guard-completion',
 'test-async-enumeration-protocol-foundation',
 'test-await-foreach-syntax-lowering-completion',
 'test-async-iterator-method-foundation',
 'test-async-iterator-composition-completion',
 'test-async-iterator-cancellation-integration',
 'test-async-language-asynchronous-iteration-integration-audit',
 'test-by-reference-value-ref-local-foundation',
 'test-ref-assignment-aliasing-completion',
 'test-ref-return-method-completion',
 'test-ref-returning-properties-indexers-completion',
 'test-ref-readonly-scoped-escape-completion',
 'test-ref-struct-foundation',
 'test-ref-struct-escape-completion',
 'test-span-foundation',
 'test-span-conversion-slicing-stackalloc-completion',
 'test-by-reference-span-integration-audit',
 'test-index-from-end-foundation',
 'test-range-syntax-value-foundation',
 'test-general-index-range-consumer-semantics',
 'test-arrays-span-index-range-completion',
 'test-managed-array-backed-memory-foundation',
 'test-memory-slicing-conversion-span-bridge',
 'test-memory-suspension-gc-integration',
 'test-span-enumeration-ref-foreach',
 'test-contiguous-memory-operations-completion',
 'test-index-range-contiguous-memory-integration-audit',
 'test-unmanaged-generic-constraint-foundation',
 'test-generic-pointer-unmanaged-stack-storage-completion',
 'test-managed-pinning-runtime-foundation',
 'test-fixed-statement-scoped-pointer-semantics',
 'test-structural-pinnable-reference-protocol',
 'test-array-span-memory-pinning-completion',
 'test-unmanaged-span-construction-completion',
 'test-native-buffer-call-integration',
 'test-pinning-gc-threads-exceptions-native-boundary-integration',
 'test-unmanaged-memory-native-buffer-integration-audit',
 'test-native-heap-runtime-foundation',
 'test-native-memory-allocation-surface',
 'test-generic-typed-native-allocation-completion',
 'test-native-reallocation-alignment-completion',
 'test-native-function-pointer-type-foundation',
 'test-native-function-address-indirect-call-completion',
 'test-native-function-pointer-abi-integration',
 'test-generic-function-pointer-unmanaged-storage-integration',
 'test-native-heap-function-pointers-gc-threads-exception-boundary-integration',
 'test-native-heap-function-pointer-integration-audit',
 'test-awaiter-protocol-semantic-foundation',
 'test-general-awaiter-state-machine-lowering',
 'test-task-awaiter-task-de-specialization',
 'test-generic-struct-extension-awaiter-completion',
 'test-task-yield-allocation-free-yield-awaitable',
 'test-value-task-foundation',
 'test-generic-value-task-result-gc-completion',
 'test-async-value-task-return-integration',
 'test-awaiter-valuetask-gc-threads-exceptions-async-iteration-integration',
 'test-generalized-awaitables-valuetask-integration-audit',
 'test-async-lambda-syntax-semantic-foundation',
 'test-async-lambda-state-machine-lowering',
 'test-async-lambda-capture-generic-gc-lifetime-completion',
 'test-async-lambda-delegate-event-valuetask-integration',
 'test-await-using-syntax-structural-async-dispose-semantics',
 'test-await-using-state-machine-structured-cleanup-completion',
 'test-task-completion-factories-async-task-run-unwrapping',
 'test-task-when-all-completion',
 'test-task-when-any-completion',
 'test-async-lambdas-async-disposal-task-composition-integration-audit',
 'test-shared-monotonic-timer-queue-foundation',
 'test-task-delay-foundation',
 'test-task-delay-cancellation-race-completion',
 'test-cancellation-token-source-disposal-registration-lifetime-completion',
 'test-cancellation-token-source-cancel-after-completion',
 'test-linked-cancellation-token-source-completion',
 'test-task-wait-async-cancellation-completion',
 'test-task-wait-async-timeout-combined-cancellation-completion',
 'test-async-timing-cancellation-gc-thread-integration',
 'test-async-timing-cancellation-integration-audit',
 'test-managed-string-representation-foundation',
 'test-length-aware-string-semantics',
 'test-native-string-abi-contract-metadata-foundation',
 'test-borrowed-nul-native-string-parameters',
 'test-pointer-length-native-string-parameters',
 'test-borrowed-static-native-string-returns',
 'test-owned-native-string-returns',
 'test-void-string-export-abi',
 'test-native-string-lifetime-callback-boundary-integration',
 'test-managed-strings-native-string-abi-integration-audit',
 'test-utf8-validity-unicode-scalar-foundation',
 'test-string-core-properties-empty-semantics',
 'test-string-character-indexing-enumeration',
 'test-string-range-slicing-substring',
 'test-string-concatenation-composition',
 'test-ordinal-string-search-comparison',
 'test-immutable-string-editing',
 'test-string-builder-foundation',
 'test-interpolated-strings-primitive-formatting',
 'test-core-strings-utf8-text-construction-integration-audit']

LEGACY_COMMANDS = [['version'],
 ['lex', 'Examples/HelloProject/Program.void'],
 ['parse', 'Examples/HelloProject/Program.void'],
 ['parse', 'Examples/HelloProject'],
 ['parse', 'Tests/ParserFeatures.void'],
 ['parse', 'Tests/ComparisonOperators.void'],
 ['parse', 'Tests/RefOutIn.void'],
 ['parse', 'Tests/Arrays.void'],
 ['parse', 'Tests/Classes.void'],
 ['parse', 'Tests/Properties/Program.void'],
 ['parse', 'Tests/Inheritance/Program.void'],
 ['parse', 'Tests/InheritanceGC/Program.void'],
 ['parse', 'Tests/Interfaces/Program.void'],
 ['parse', 'Tests/Generics/Program.void'],
 ['parse', 'StandardLibrary/Void/Exception.void'],
 ['parse', 'StandardLibrary/Void/Collections/List.void'],
 ['parse', 'StandardLibrary/Void/Collections/Enumerable.void'],
 ['parse', 'Tests/Collections/Program.void'],
 ['parse', 'Tests/Foreach/Program.void'],
 ['parse', 'Tests/Yield/Program.void'],
 ['parse', 'StandardLibrary/Void/Delegates.void'],
 ['parse', 'Tests/Delegates/Program.void'],
 ['parse', 'Tests/Lambdas/Program.void'],
 ['parse', 'Tests/Events/Program.void'],
 ['parse', 'StandardLibrary/Void/Collections/Queue.void'],
 ['parse', 'StandardLibrary/Void/Collections/Stack.void'],
 ['parse', 'Tests/QueueStack/Program.void'],
 ['parse', 'Tests/DictionaryHashSet/Program.void'],
 ['parse', 'Tests/Attributes/Program.void'],
 ['parse', 'Tests/NativeInterop/Program.void'],
 ['parse', 'Tests/Pointers/Program.void'],
 ['parse', 'Tests/NativeStructs/Program.void'],
 ['parse', 'Tests/NativeByRef/Program.void'],
 ['parse', 'Tests/StaticCalls/Program.void'],
 ['parse', 'Tests/NativeAliases/Program.void'],
 ['parse', 'Tests/NativeLibraries/Program.void'],
 ['parse', 'Tests/NativeCallbacks/Program.void'],
 ['parse', 'Tests/Memory/Program.void'],
 ['parse', 'Tests/StackAlloc/Program.void'],
 ['parse', 'Tests/NativeIntegration/Program.void'],
 ['parse', 'Tests/Enums/Program.void'],
 ['parse', 'Tests/Switch/Program.void'],
 ['parse', 'Tests/ConstReadonly/Program.void'],
 ['parse', 'Tests/Casts/Program.void'],
 ['parse', 'Tests/NullOperators/Program.void'],
 ['parse', 'Tests/OptionalParams/Program.void'],
 ['parse', 'Tests/DefaultValues/Program.void'],
 ['parse', 'Tests/TypeMetadata/Program.void'],
 ['parse', 'Tests/Conversions/Program.void'],
 ['parse', 'Tests/ErgonomicsIntegration/Program.void'],
 ['parse', 'Tests/NullableValues/Program.void'],
 ['parse', 'Tests/NullableOperators/Program.void'],
 ['parse', 'Tests/NamedArguments/Program.void'],
 ['parse', 'Tests/TargetTypedDefault/Program.void'],
 ['parse', 'Tests/InstanceInitializers/Program.void'],
 ['parse', 'Tests/ObjectInitializers/Program.void'],
 ['parse', 'Tests/CollectionInitializers/Program.void'],
 ['parse', 'Tests/StaticMembers/Program.void'],
 ['parse', 'Tests/StaticInitialization/Program.void'],
 ['parse', 'Tests/LanguageCompletionIntegration/Program.void'],
 ['parse', 'Tests/BaseConstructors/Program.void'],
 ['parse', 'Tests/ConstructorDelegation/Program.void'],
 ['parse', 'Tests/InterfaceInheritance/Program.void'],
 ['parse', 'Tests/StructInterfaces/Program.void'],
 ['parse', 'Tests/VirtualProperties/Program.void'],
 ['parse', 'Tests/StaticEvents/Program.void'],
 ['parse', 'Tests/InterfaceEvents/Program.void'],
 ['parse', 'Tests/DelegateCompletion/Program.void'],
 ['parse', 'Tests/ConditionalExpressions/Program.void'],
 ['parse', 'Tests/ObjectModelIntegration'],
 ['parse', 'Tests/NestedValueStructs'],
 ['parse', 'Tests/ReadonlyStructs'],
 ['parse', 'Tests/JaggedArrays'],
 ['parse', 'Tests/MultidimensionalArrays'],
 ['parse', 'Tests/IteratorCompletion'],
 ['parse', 'Tests/ValueTypesClosuresControlFlowIntegration'],
 ['parse', 'Tests/StaticInterfaceProperties'],
 ['parse', 'Tests/StaticInterfaceEvents'],
 ['parse', 'Tests/ConstructorReturns'],
 ['parse', 'Tests/RectangularArrayInitializers'],
 ['parse', 'Tests/IteratorLocals'],
 ['parse', 'Tests/ReceiverExpressions'],
 ['parse', 'Tests/GenericMethodCalls'],
 ['parse', 'Tests/NativeCallbackByRef'],
 ['parse', 'Tests/UnsafeDelegates'],
 ['parse', 'Tests/UnsafePointerProperties'],
 ['parse', 'Tests/DoWhileCompletion'],
 ['parse', 'Tests/ExpressionBodiedMembers'],
 ['parse', 'Tests/CustomEventAccessors'],
 ['parse', 'Tests/NullConditionalArrays'],
 ['parse', 'Tests/SizedRectangularArrayInitializers'],
 ['parse', 'Tests/ImplicitRectangularArrayInitializers'],
 ['parse', 'Tests/GenericConstraints'],
 ['parse', 'Tests/LanguageSurfaceCompletionIIIntegration'],
 ['parse', 'Tests/ExceptionThrowFoundation'],
 ['parse', 'Tests/TryCatchCompletion'],
 ['parse', 'Tests/ExceptionPropagationGcUnwinding'],
 ['parse', 'Tests/ExceptionThrowUnhandled'],
 ['run', 'Examples/HelloProject'],
 ['run', 'Examples/HelloProjectless'],
 ['run', 'Tests/GC'],
 ['run', 'Tests/Properties'],
 ['run', 'Tests/Inheritance'],
 ['run', 'Tests/InheritanceGC'],
 ['run', 'Tests/Interfaces'],
 ['run', 'Tests/Generics'],
 ['run', 'Tests/Collections'],
 ['run', 'Tests/Foreach'],
 ['run', 'Tests/Yield'],
 ['run', 'Tests/Delegates'],
 ['run', 'Tests/Lambdas'],
 ['run', 'Tests/Events'],
 ['run', 'Tests/QueueStack'],
 ['run', 'Tests/DictionaryHashSet'],
 ['run', 'Tests/Attributes'],
 ['run', 'Tests/NativeInterop'],
 ['run', 'Tests/Pointers'],
 ['run', 'Tests/NativeStructs'],
 ['run', 'Tests/NativeByRef'],
 ['run', 'Tests/StaticCalls'],
 ['run', 'Tests/NativeAliases'],
 ['run', 'Tests/NativeLibraries'],
 ['run', 'Tests/NativeCallbacks'],
 ['run', 'Tests/Memory'],
 ['run', 'Tests/StackAlloc'],
 ['run', 'Tests/NativeIntegration'],
 ['run', 'Tests/Enums'],
 ['run', 'Tests/Switch'],
 ['run', 'Tests/ConstReadonly'],
 ['run', 'Tests/Casts'],
 ['run', 'Tests/NullOperators'],
 ['run', 'Tests/OptionalParams'],
 ['run', 'Tests/DefaultValues'],
 ['run', 'Tests/TypeMetadata'],
 ['run', 'Tests/Conversions'],
 ['run', 'Tests/ErgonomicsIntegration'],
 ['run', 'Tests/NullableValues'],
 ['run', 'Tests/NullableOperators'],
 ['run', 'Tests/NamedArguments'],
 ['run', 'Tests/TargetTypedDefault'],
 ['run', 'Tests/InstanceInitializers'],
 ['run', 'Tests/ObjectInitializers'],
 ['run', 'Tests/CollectionInitializers'],
 ['run', 'Tests/StaticMembers'],
 ['run', 'Tests/StaticInitialization'],
 ['run', 'Tests/LanguageCompletionIntegration'],
 ['run', 'Tests/BaseConstructors'],
 ['run', 'Tests/ConstructorDelegation'],
 ['run', 'Tests/InterfaceInheritance'],
 ['run', 'Tests/StructInterfaces'],
 ['run', 'Tests/VirtualProperties'],
 ['run', 'Tests/StaticEvents'],
 ['run', 'Tests/InterfaceEvents'],
 ['run', 'Tests/DelegateCompletion'],
 ['run', 'Tests/ConditionalExpressions'],
 ['run', 'Tests/ObjectModelIntegration'],
 ['run', 'Tests/NestedValueStructs'],
 ['run', 'Tests/ReadonlyStructs'],
 ['run', 'Tests/JaggedArrays'],
 ['run', 'Tests/MultidimensionalArrays'],
 ['run', 'Tests/ClosureCompletionI'],
 ['run', 'Tests/ClosureCompletionII'],
 ['run', 'Tests/IteratorCompletion'],
 ['run', 'Tests/UnsafeMemberCompletion'],
 ['run', 'Tests/CompoundTargets'],
 ['run', 'Tests/ValueTypesClosuresControlFlowIntegration'],
 ['run', 'Tests/StaticInterfaceProperties'],
 ['run', 'Tests/StaticInterfaceEvents'],
 ['run', 'Tests/ConstructorReturns'],
 ['run', 'Tests/RectangularArrayInitializers'],
 ['run', 'Tests/IteratorLocals'],
 ['run', 'Tests/ReceiverExpressions'],
 ['run', 'Tests/GenericMethodCalls'],
 ['run', 'Tests/NativeCallbackByRef'],
 ['run', 'Tests/UnsafeDelegates'],
 ['run', 'Tests/UnsafePointerProperties'],
 ['run', 'Tests/DoWhileCompletion'],
 ['run', 'Tests/ExpressionBodiedMembers'],
 ['run', 'Tests/CustomEventAccessors'],
 ['run', 'Tests/NullConditionalArrays'],
 ['run', 'Tests/SizedRectangularArrayInitializers'],
 ['run', 'Tests/ImplicitRectangularArrayInitializers'],
 ['run', 'Tests/GenericConstraints'],
 ['run', 'Tests/LanguageSurfaceCompletionIIIntegration'],
 ['run', 'Tests/ExceptionThrowFoundation'],
 ['run', 'Tests/TryCatchCompletion'],
 ['run', 'Tests/ExceptionPropagationGcUnwinding'],
 ['publish', 'Examples/HelloProject'],
 ['publish', 'Tests/NativeInterop'],
 ['publish', 'Tests/Pointers'],
 ['publish', 'Tests/NativeStructs'],
 ['publish', 'Tests/NativeByRef'],
 ['publish', 'Tests/StaticCalls'],
 ['publish', 'Tests/NativeAliases'],
 ['publish', 'Tests/NativeLibraries'],
 ['publish', 'Tests/NativeCallbacks'],
 ['publish', 'Tests/Memory'],
 ['publish', 'Tests/StackAlloc'],
 ['publish', 'Tests/NativeIntegration'],
 ['publish', 'Tests/Enums'],
 ['publish', 'Tests/Switch'],
 ['publish', 'Tests/ConstReadonly'],
 ['publish', 'Tests/Casts'],
 ['publish', 'Tests/NullOperators'],
 ['publish', 'Tests/OptionalParams'],
 ['publish', 'Tests/DefaultValues'],
 ['publish', 'Tests/TypeMetadata'],
 ['publish', 'Tests/Conversions'],
 ['publish', 'Tests/ErgonomicsIntegration'],
 ['publish', 'Tests/NullableValues'],
 ['publish', 'Tests/NullableOperators'],
 ['publish', 'Tests/NamedArguments'],
 ['publish', 'Tests/TargetTypedDefault'],
 ['publish', 'Tests/InstanceInitializers'],
 ['publish', 'Tests/ObjectInitializers'],
 ['publish', 'Tests/CollectionInitializers'],
 ['publish', 'Tests/StaticMembers'],
 ['publish', 'Tests/StaticInitialization'],
 ['publish', 'Tests/LanguageCompletionIntegration'],
 ['publish', 'Tests/BaseConstructors'],
 ['publish', 'Tests/ConstructorDelegation'],
 ['publish', 'Tests/InterfaceInheritance'],
 ['publish', 'Tests/StructInterfaces'],
 ['publish', 'Tests/VirtualProperties'],
 ['publish', 'Tests/StaticEvents'],
 ['publish', 'Tests/InterfaceEvents'],
 ['publish', 'Tests/DelegateCompletion'],
 ['publish', 'Tests/ConditionalExpressions'],
 ['publish', 'Tests/ObjectModelIntegration'],
 ['publish', 'Tests/NestedValueStructs'],
 ['publish', 'Tests/ReadonlyStructs'],
 ['publish', 'Tests/JaggedArrays'],
 ['publish', 'Tests/MultidimensionalArrays'],
 ['publish', 'Tests/ClosureCompletionI'],
 ['publish', 'Tests/ClosureCompletionII'],
 ['publish', 'Tests/IteratorCompletion'],
 ['publish', 'Tests/UnsafeMemberCompletion'],
 ['publish', 'Tests/CompoundTargets'],
 ['publish', 'Tests/ValueTypesClosuresControlFlowIntegration'],
 ['publish', 'Tests/StaticInterfaceProperties'],
 ['publish', 'Tests/StaticInterfaceEvents'],
 ['publish', 'Tests/ConstructorReturns'],
 ['publish', 'Tests/RectangularArrayInitializers'],
 ['publish', 'Tests/IteratorLocals'],
 ['publish', 'Tests/ReceiverExpressions'],
 ['publish', 'Tests/GenericMethodCalls'],
 ['publish', 'Tests/NativeCallbackByRef'],
 ['publish', 'Tests/UnsafeDelegates'],
 ['publish', 'Tests/UnsafePointerProperties'],
 ['publish', 'Tests/DoWhileCompletion'],
 ['publish', 'Tests/ExpressionBodiedMembers'],
 ['publish', 'Tests/CustomEventAccessors'],
 ['publish', 'Tests/NullConditionalArrays'],
 ['publish', 'Tests/SizedRectangularArrayInitializers'],
 ['publish', 'Tests/ImplicitRectangularArrayInitializers'],
 ['publish', 'Tests/GenericConstraints'],
 ['publish', 'Tests/LanguageSurfaceCompletionIIIntegration'],
 ['publish', 'Tests/ExceptionThrowFoundation'],
 ['publish', 'Tests/TryCatchCompletion'],
 ['publish', 'Tests/ExceptionPropagationGcUnwinding']]


DEFAULT_WORKER_COUNT = 3


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the complete VOID regression suite")
    default_jobs = DEFAULT_WORKER_COUNT
    parser.add_argument("--jobs", type=int, default=default_jobs,
                        help=f"isolated worker count (default: {default_jobs})")
    parser.add_argument("--keep-workers", action="store_true",
                        help="keep temporary worker trees for debugging")
    return parser.parse_args()


def run_capture(cmd: list[str], cwd: Path, env: dict[str, str] | None = None) -> tuple[int, str, float]:
    started = time.monotonic()
    proc = subprocess.run(
        cmd,
        cwd=cwd,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        errors="replace",
    )
    return proc.returncode, proc.stdout, time.monotonic() - started


def true_count(output: str) -> int:
    return sum(1 for line in output.splitlines() if line == "True")


def copy_source_tree(source: Path, destination: Path) -> None:
    def ignore(directory: str, names: list[str]) -> set[str]:
        ignored: set[str] = set()
        for name in names:
            if name in {".git", ".test-logs", "__pycache__", "bin", ".void", "publish"}:
                ignored.add(name)
        return ignored
    shutil.copytree(source, destination, ignore=ignore, symlinks=True)


def legacy_groups() -> list[tuple[str, list[list[str]]]]:
    groups: dict[str, list[list[str]]] = {
        "legacy-meta": [],
        "legacy-parse": [],
        "legacy-run": [],
        "legacy-publish": [],
    }
    for args in LEGACY_COMMANDS:
        op = args[0]
        if op in {"version", "lex"}:
            groups["legacy-meta"].append(args)
        elif op == "parse":
            groups["legacy-parse"].append(args)
        elif op == "run":
            groups["legacy-run"].append(args)
        elif op == "publish":
            groups["legacy-publish"].append(args)
        else:
            raise RuntimeError(f"unsupported legacy operation: {op}")
    return list(groups.items())


def execute_legacy(name: str, commands: list[list[str]], cwd: Path) -> tuple[int, str, float]:
    started = time.monotonic()
    chunks: list[str] = []
    for args in commands:
        code, output, _ = run_capture(["./bin/voidc", *args], cwd)
        chunks.append(f"$ ./bin/voidc {' '.join(args)}\n{output}")
        if code != 0:
            return code, "".join(chunks), time.monotonic() - started
    return 0, "".join(chunks), time.monotonic() - started


def main() -> int:
    args = parse_args()
    if args.jobs < 1:
        print("error: --jobs must be at least 1", file=sys.stderr)
        return 2
    if len(FOCUSED_TARGETS) != EXPECTED_FOCUSED_SUITES:
        print("error: focused suite manifest count drifted", file=sys.stderr)
        return 2

    root = Path(__file__).resolve().parents[1]
    makefile_text = (root / "Makefile").read_text()
    missing_targets = [target for target in FOCUSED_TARGETS
                       if not any(line.startswith(target + ":") for line in makefile_text.splitlines())]
    if missing_targets:
        print("error: full-suite manifest references missing Make targets: " +
              ", ".join(missing_targets), file=sys.stderr)
        return 2
    stress_targets = [target for target in FOCUSED_TARGETS if "stress" in target.lower()]
    if stress_targets:
        print("error: stress-only targets must not be part of make test: " +
              ", ".join(stress_targets), file=sys.stderr)
        return 2

    log_dir = root / "Tests" / ".test-logs"
    if log_dir.exists():
        shutil.rmtree(log_dir)
    log_dir.mkdir(parents=True)

    tasks: queue.Queue[tuple[str, str, object]] = queue.Queue()
    for target in FOCUSED_TARGETS:
        tasks.put((target, "make", target))
    for name, commands in legacy_groups():
        tasks.put((name, "legacy", commands))

    worker_count = min(args.jobs, tasks.qsize())
    temp_parent = Path(tempfile.mkdtemp(prefix="voidc-full-suite-"))
    results: list[tuple[str, int, int, float]] = []
    failures: list[tuple[str, int, str, float]] = []
    result_lock = threading.Lock()
    abort = threading.Event()
    started = time.monotonic()

    print(f"VOID cumulative test suite: {len(FOCUSED_TARGETS)} focused + 4 legacy groups")
    print(f"Workers: {worker_count} isolated trees")

    def worker(index: int) -> None:
        worker_root = temp_parent / f"worker-{index}"
        copy_source_tree(root, worker_root)
        code, output, _ = run_capture(["make", "-s", "clean"], worker_root)
        if code == 0:
            code, more, _ = run_capture(["make", "-s", "all"], worker_root)
            output += more
        if code != 0:
            with result_lock:
                failures.append((f"worker-{index}-prepare", code, output, 0.0))
            abort.set()
            return

        while not abort.is_set():
            try:
                name, kind, payload = tasks.get_nowait()
            except queue.Empty:
                return
            if kind == "make":
                suite_env = dict(os.environ)
                suite_env["VOID_FULL_SUITE"] = "1"
                code, output, elapsed = run_capture(["make", "-s", str(payload)], worker_root, suite_env)
            else:
                code, output, elapsed = execute_legacy(name, payload, worker_root)  # type: ignore[arg-type]

            (log_dir / f"{name}.log").write_text(output)
            count = true_count(output)
            with result_lock:
                results.append((name, code, count, elapsed))
                if code != 0:
                    failures.append((name, code, output, elapsed))
                    abort.set()
            tasks.task_done()

    threads = [threading.Thread(target=worker, args=(i,), name=f"voidc-test-{i}")
               for i in range(worker_count)]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()

    elapsed = time.monotonic() - started
    total_true = sum(item[2] for item in results)
    completed = len(results)
    total_suites = len(FOCUSED_TARGETS) + 4

    if not args.keep_workers and not failures:
        shutil.rmtree(temp_parent, ignore_errors=True)
    elif args.keep_workers or failures:
        print(f"Worker trees: {temp_parent}")

    if failures:
        name, code, output, failure_elapsed = failures[0]
        print(f"FAIL: {name} (exit {code}, {failure_elapsed:.1f}s)")
        tail = output.splitlines()[-40:]
        if tail:
            print("--- failure output (last 40 lines) ---")
            print("\n".join(tail))
        print(f"Completed suites: {completed}/{total_suites}")
        print(f"Logs: {log_dir}")
        return 1

    if completed != total_suites:
        print(f"FAIL: only {completed}/{total_suites} suites completed")
        print(f"Logs: {log_dir}")
        return 1
    if total_true != EXPECTED_TRUE_COUNT:
        print(f"FAIL: cumulative True count {total_true}, expected {EXPECTED_TRUE_COUNT}")
        print(f"Logs: {log_dir}")
        return 1

    slowest = sorted(results, key=lambda item: item[3], reverse=True)[:5]
    print(f"PASS: {completed}/{total_suites} suites | {total_true} checks | {elapsed:.1f}s")
    print("Slowest suites: " + ", ".join(f"{name} {seconds:.1f}s" for name, _, _, seconds in slowest))
    print(f"Logs: {log_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
