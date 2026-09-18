# ESP32 Wireless Dongle

# Project Governing Rules and Verification Standard

This document defines the mandatory rules that govern the architecture, source code, development process, testing, verification, documentation, configuration, and maintenance of the ESP32 Wireless Dongle project.

These rules apply to all existing and future work.

The project must evolve as a controlled engineering system.

---

# 1. PRIMARY PROJECT PRINCIPLE

The project must follow this lifecycle:

```text
Understand
    ->
Preserve
    ->
Implement
    ->
Verify
    ->
Document
```

Do not use:

```text
Rewrite
    ->
Assume
    ->
Patch
    ->
Declare Done
```

---

# 2. SOURCE OF TRUTH

The actual implementation is the primary source of truth.

The following priority applies when information conflicts:

```text
Actual source code
    >
Build configuration
    >
Verified runtime behavior
    >
Verified tests
    >
Protocol evidence
    >
Documentation
    >
Comments
    >
Names
    >
Assumptions
```

Do not invent behavior that cannot be established from the project.

Use:

```text
UNKNOWN
NOT VERIFIED
NOT IMPLEMENTED
NOT DETERMINED FROM SOURCE
```

when necessary.

---

# 3. PROJECT CONFLICT PRIORITY

When project rules or objectives conflict, apply this order:

```text
1. Safety and data integrity
2. Explicit user requirement
3. Existing externally visible behavior
4. Protocol compatibility
5. Existing architecture
6. Functional correctness
7. Build and framework compatibility
8. Memory and resource constraints
9. Testability
10. Maintainability
11. Code style
12. Refactoring elegance
13. Cosmetic improvements
```

Higher priority rules override lower priority rules.

Examples:

```text
Protocol compatibility overrides cleanup.

Functional correctness overrides cosmetic refactoring.

Architecture preservation overrides a nicer folder layout.

Memory safety overrides abstraction elegance.

Explicit user requirements override normal project conventions.
```

Any unresolved conflict must be documented.

---

# 4. PROJECT ARCHITECTURE IS PROTECTED

The established architecture is a protected baseline.

Preserve:

```text
Layer boundaries
Module boundaries
Service ownership
Interface boundaries
Protocol boundaries
Transport boundaries
Dependency direction
State ownership
Resource ownership
Runtime lifecycle
File organization
```

Do not change these without a documented technical reason.

---

# 5. LAYERED ARCHITECTURE

The project must maintain clear architectural layers.

The exact current names must be derived from the source tree.

The conceptual structure is:

```text
Application
    ->
Protocol
    ->
Services
    ->
Abstraction or Hardware
    ->
ESP32 / Arduino / Third Party APIs
```

Transport must remain a distinct concern.

---

# 6. RESPONSIBILITY BOUNDARIES

Each layer must have a clear responsibility.

## Application

Responsible for:

```text
Lifecycle
Orchestration
System coordination
High-level execution flow
```

## Protocol

Responsible for:

```text
Parsing
Validation
Command interpretation
Response construction
Protocol errors
Protocol events
```

## Transport

Responsible for:

```text
Read
Write
Availability
Buffering
Transport lifecycle
```

## Services

Responsible for:

```text
Domain-specific operations
State
Hardware interaction through defined boundaries
Service-level errors
Service events
```

## Hardware Layer

Responsible for:

```text
Hardware-specific integration
Framework-specific API usage
Low-level resource access
```

---

# 7. NO RESPONSIBILITY LEAK

Do not allow one layer to perform another layer's responsibilities without a documented reason.

Examples of unwanted coupling:

```text
Service -> Protocol formatting
Service -> Raw terminal output
Protocol -> Direct ESP32 API calls
Hardware -> Application logic
Transport -> Business logic
Callback -> Unrelated subsystem logic
```

---

# 8. DEPENDENCY DIRECTION

Dependencies should flow downward or toward abstractions.

Avoid:

```text
Hardware -> Application
Service -> CLI
Transport -> Service
Protocol -> Concrete hardware implementation
```

New dependencies that violate the established direction require explicit architectural justification.

---

# 9. TRANSPORT AND PROTOCOL SEPARATION

Transport describes:

```text
How data moves
```

Protocol describes:

```text
What data means
```

Protocol logic must not depend unnecessarily on a specific transport.

Services must not depend on transport details.

---

# 10. SERVICE ISOLATION

Each service must have clear ownership.

Examples may include:

```text
WiFi
Bluetooth Classic
BLE
A2DP
TCP
UDP
System
```

Services must not become interchangeable containers for unrelated functionality.

---

# 11. OOP RULES

Use object-oriented design where it improves:

```text
Encapsulation
Ownership
Cohesion
Testability
Extensibility
Dependency control
```

Do not use OOP merely for additional classes.

---

# 12. SINGLE RESPONSIBILITY

Every important:

```text
Class
Module
Function
Interface
```

must have a clear responsibility.

Avoid combining unrelated functionality.

---

# 13. NO GOD CLASS

Do not allow a class to become responsible for unrelated systems such as:

```text
Serial
Protocol
WiFi
Bluetooth
BLE
A2DP
TCP
UDP
Logging
Configuration
```

---

# 14. NO GOD FUNCTION

Large functions must not become containers for unrelated behavior.

Functions such as:

```text
setup()
loop()
handleCommand()
processCommand()
setupBluetooth()
processMenu()
```

must remain focused.

---

# 15. SOLID PRINCIPLES

Apply SOLID pragmatically.

## Single Responsibility

One clear responsibility per component.

## Open/Closed

Extend existing behavior where practical instead of rewriting stable code.

## Liskov Substitution

Implementations must obey their interface contracts.

## Interface Segregation

Prefer focused interfaces over huge interfaces.

## Dependency Inversion

High-level logic should depend on abstractions where abstraction provides real value.

---

# 16. COMPOSITION OVER UNNECESSARY INHERITANCE

Use inheritance only when there is a true substitutable relationship.

Prefer composition for:

```text
Service assembly
Dependency ownership
Subsystem coordination
Adapters
Helpers
```

when appropriate.

---

# 17. INTERFACE RULE

Create an interface only when it provides meaningful abstraction.

An interface should have:

```text
Clear contract
Real consumer
Real implementation
Substitution value
Isolation value
```

Do not create empty or artificial interfaces.

---

# 18. DEPENDENCY INJECTION

Use dependency injection where it provides practical benefits.

Do not introduce global access merely because dependency injection is inconvenient.

Do not introduce dependency injection everywhere without architectural value.

---

# 19. GLOBAL STATE

Mutable global state must be minimized.

Every global should be classified:

```text
KEEP
MOVE INTO CLASS
MOVE INTO CONTEXT
MOVE INTO CONFIGURATION
CONVERT TO CONSTANT
REMOVE
```

New mutable global state requires justification.

---

# 20. SINGLETON

Singletons are allowed only when system-wide uniqueness and lifecycle make them appropriate.

Do not use Singleton as a shortcut around dependency design.

---

# 21. STATE MANAGEMENT

Stateful subsystems must have explicit state ownership.

Relevant examples:

```text
WiFi
Bluetooth
BLE
A2DP
TCP
UDP
Application
```

State must not be duplicated across unrelated components without a synchronization strategy.

---

# 22. STATE MACHINE RULE

When a subsystem has meaningful states, use an explicit state model where practical.

Document:

```text
Initial state
States
Transitions
Triggers
Actions
Error states
Invalid transitions
```

---

# 23. STATE SINGLE SOURCE OF TRUTH

A conceptual state should have one authoritative owner.

Do not allow multiple unrelated booleans or duplicated state models to become competing sources of truth.

---

# 24. CALLBACK RULE

Callbacks must respect architecture boundaries.

Callbacks must not become hidden application entry points.

Avoid direct callback logic such as:

```text
Raw serial output
Protocol serialization
Unrelated service control
Unbounded allocation
Long blocking work
```

unless explicitly justified.

---

# 25. EVENT RULE

Commands and events are different.

Command:

```text
Client -> Device
```

Event:

```text
Device -> Consumer
```

Do not mix their semantics.

---

# 26. ERROR HANDLING

Use a consistent project-wide error model.

Avoid uncontrolled combinations of:

```text
false
-1
nullptr
magic values
raw strings
direct serial output
```

Use the established project result and error abstractions where available.

---

# 27. ERROR PROPAGATION

Errors must remain traceable.

Preferred flow:

```text
Hardware
    ->
Service
    ->
Application
    ->
Protocol
    ->
Client
```

Do not hide a functional error inside a log message.

---

# 28. LOGGING ARCHITECTURE

Logging must use a central and consistent mechanism.

Typical levels:

```text
TRACE
DEBUG
INFO
WARN
ERROR
```

The actual project levels are authoritative.

---

# 29. RAW SERIAL OUTPUT

Do not add uncontrolled:

```cpp
Serial.print(...)
Serial.println(...)
```

throughout the codebase.

All output must belong to one of the defined categories:

```text
Protocol response
Structured log
Controlled diagnostic output
```

---

# 30. TERMINAL OUTPUT IS AN INTERFACE

If external tools or scripts consume terminal output, treat its format as an interface.

Before changing it, inspect:

```text
Protocol clients
Python tools
Tests
Automation
Documentation
```

---

# 31. MAIN ENTRY POINT

The main entry point must remain small.

The preferred principle is:

```text
setup()
    ->
application.begin()

loop()
    ->
application.update()
```

The exact implementation may differ.

Business logic should not accumulate in main.cpp.

---

# 32. NON-BLOCKING DESIGN

Avoid unnecessary:

```text
delay()
Busy waiting
Unbounded loops
Long blocking calls
```

Any blocking operation must be:

```text
Necessary
Bounded
Documented
```

---

# 33. CONCURRENCY

Do not change execution models without explicit analysis.

Changes such as:

```text
Synchronous -> Asynchronous
Callback -> Task
Task -> Callback
Single context -> Multi-context
```

are architectural changes.

---

# 34. THREAD AND TASK SAFETY

For shared state identify:

```text
Readers
Writers
Execution contexts
Synchronization requirements
Lifetime
Ownership
```

Never assume thread safety without evidence.

---

# 35. MEMORY RULES

ESP32 resources are limited.

Every significant change must consider:

```text
Flash
RAM
Heap
Stack
Dynamic allocation
String fragmentation
Buffers
BLE memory
A2DP memory
Network buffers
Temporary objects
```

---

# 36. DYNAMIC ALLOCATION

Avoid unnecessary runtime allocation, especially in:

```text
Main loop
Callbacks
BLE callbacks
Bluetooth callbacks
A2DP callbacks
Network receive paths
```

Any significant allocation pattern must be understood.

---

# 37. RESOURCE OWNERSHIP

For important resources document:

```text
Creator
Owner
Users
Release owner
Lifetime
```

This applies to:

```text
Buffers
Sockets
BLE objects
Bluetooth objects
A2DP resources
Tasks
Queues
Timers
Callbacks
```

---

# 38. LIFETIME CONTROL

Do not change object lifetime or resource lifetime without analysis.

Be especially careful with:

```text
Bluetooth stack
BLE scans
A2DP objects
WiFi clients
TCP sockets
UDP sockets
Callbacks
Buffers
```

---

# 39. CONFIGURATION

Important configuration values must have one clear source of truth.

Examples:

```text
Firmware version
Device name
Baud rate
Timeouts
Pins
Feature flags
Partition
Build flags
```

---

# 40. VERSION MANAGEMENT

Firmware version must have one authoritative definition.

Do not maintain unrelated hard-coded versions in multiple places.

---

# 41. PROTOCOL GOVERNANCE

The protocol is a protected public interface.

Protect:

```text
Command IDs
Command syntax
Parameters
Responses
Errors
Events
Protocol version
```

Do not change protocol semantics casually.

---

# 42. NEW COMMAND RULE

Every new command must define:

```text
Command ID
Name
Parameters
Validation
Handler
Target service
Success response
Error response
State requirements
Timeout behavior
Tests
Documentation
```

---

# 43. COMMAND REMOVAL

Do not remove commands without analyzing:

```text
Source references
Clients
Tests
Automation
Documentation
External consumers
```

---

# 44. A2DP GOVERNANCE

A2DP is a protected subsystem.

Preserve unless explicitly changing requirements:

```text
Initialization
Status
Connection by MAC
Connection by name
Disconnect
Streaming
Audio callback
Test tone
State management
Callbacks
Resource lifecycle
```

---

# 45. BLUETOOTH GOVERNANCE

Bluetooth Classic and BLE remain distinct logical domains.

Do not merge them into a generic abstraction without a real architectural reason.

---

# 46. WIFI GOVERNANCE

Preserve:

```text
Initialization
Scan
Scan results
Connect
Disconnect
Status
Timeouts
Callbacks
Events
State
```

---

# 47. NETWORK GOVERNANCE

TCP Client, TCP Server, and UDP remain distinct responsibilities.

Do not move protocol logic into network implementations.

---

# 48. HEADER HYGIENE

Headers must avoid:

```text
Unnecessary includes
Circular includes
Implementation leakage
Hidden dependencies
```

Use forward declarations where appropriate.

---

# 49. NAMING

Use the established project naming convention.

Names must be:

```text
Clear
Consistent
Semantic
Stable
```

Do not introduce a second naming convention into the project.

---

# 50. MAGIC VALUES

Do not introduce new magic numbers or magic strings for:

```text
Commands
Timeouts
Buffer sizes
Pins
States
Versions
Protocol values
```

Use appropriate constants, enums, or configuration.

---

# 51. CODE DUPLICATION

Do not add duplicate:

```text
Implementation
Logic
Constants
State
Protocol handling
Error handling
```

Before adding new logic, search for existing behavior.

---

# 52. DEPENDENCY GOVERNANCE

Every dependency must be:

```text
Known
Required
Compatible
Reproducible
Documented
```

Do not rely on hidden local dependencies.

---

# 53. DEPENDENCY ADDITION

Before adding a dependency:

```text
Check whether the project already provides the functionality.
Check whether the dependency is necessary.
Check framework compatibility.
Check memory impact.
Check flash impact.
Check transitive dependencies.
Check maintenance status.
```

---

# 54. DEPENDENCY UPGRADES

Do not upgrade libraries or frameworks as part of unrelated work.

Dependency upgrades are controlled changes.

---

# 55. PLATFORMIO GOVERNANCE

PlatformIO configuration is part of the project source.

Important configuration includes:

```text
Platform
Framework
Board
Partition
Build flags
Library dependencies
Monitor settings
Upload settings
Compiler settings
```

---

# 56. BUILD REPRODUCIBILITY

The project must build using declared project configuration.

Do not rely on:

```text
IDE-only settings
Global libraries
Developer machine state
Hidden files
Undocumented environment variables
```

---

# 57. PARTITION GOVERNANCE

Partition changes require explicit analysis of:

```text
Firmware size
Flash layout
Filesystem
OTA
Boot behavior
Available application space
```

---

# 58. FRAMEWORK GOVERNANCE

Do not change framework versions or framework types during unrelated work.

Framework migration is a controlled project change.

---

# 59. DOCUMENTATION GOVERNANCE

Documentation must describe the implementation that actually exists.

Documentation must not present planned behavior as implemented behavior.

Use explicit status when needed:

```text
IMPLEMENTED
PARTIALLY IMPLEMENTED
PLANNED
UNKNOWN
NOT VERIFIED
```

---

# 60. SOURCE AND DOCUMENTATION CONSISTENCY

Check that:

```text
Class names
Method names
File paths
Commands
States
Dependencies
Behavior
```

match the actual source.

When documentation conflicts with source, the source must be treated as authoritative.

The discrepancy should then be corrected.

---

# 61. TEST GOVERNANCE

Existing tests must not be deleted merely because they fail after a change.

If a test becomes invalid:

```text
Explain why
Update it
Add replacement coverage when required
```

---

# 62. REGRESSION TESTING

Any change that can affect behavior must trigger appropriate regression testing.

At minimum, relevant core functionality includes:

```text
PING
GET_VERSION
GET_INFO

WiFi Scan
WiFi Connect
WiFi Disconnect
WiFi Status

TCP Client
TCP Server
UDP

Bluetooth Info
Bluetooth Classic Scan
BLE Scan
Bluetooth Classic Connect
Bluetooth Disconnect

A2DP Status
A2DP Connect by MAC
A2DP Connect by Name
A2DP Test Tone ON
A2DP Test Tone OFF
A2DP Disconnect
```

All additional project features must also be tested within their relevant scope.

---

# 63. TEST LEVELS

Distinguish:

```text
Static inspection
Compilation
Linking
Unit test
Integration test
Protocol test
Runtime test
Hardware test
Regression test
```

Passing one level does not prove another.

---

# 64. VERIFICATION EVIDENCE

Important verification claims must have evidence.

Evidence can include:

```text
Source file
Class
Method
Build output
Test result
Runtime log
Hardware result
```

Never claim verification without evidence.

---

# 65. NO FALSE VERIFICATION

Do not claim:

```text
Verified
Passed
Working
Tested
Compile successful
Hardware verified
```

unless the corresponding verification actually occurred.

---

# 66. BASELINE RULE

Before major changes, record relevant baseline information:

```text
Version
Git commit
Build result
Firmware size
RAM usage
Runtime behavior
Relevant logs
Feature behavior
```

If the baseline does not build, record the failure separately.

---

# 67. PRE-EXISTING FAILURE RULE

A failure that existed before the current change must not be attributed to that change.

Track it as:

```text
PRE-EXISTING ISSUE
```

unless evidence proves otherwise.

---

# 68. REGRESSION RULE

If a previously working feature stops working after a change:

```text
REGRESSION
```

must be recorded.

Do not silently redefine the changed behavior as correct.

---

# 69. EDGE CASE GOVERNANCE

Relevant edge cases must be considered:

```text
Invalid command
Missing parameter
Invalid parameter
Device not found
Timeout
Duplicate connection
Disconnect while disconnected
Connect while busy
Scan while scanning
Unexpected callback
Unexpected state
Memory allocation failure
Network failure
Bluetooth failure
A2DP failure
WiFi failure
```

---

# 70. CHANGE ISOLATION

A change should be:

```text
Focused
Small
Atomic
Reviewable
Reversible
```

Do not combine unrelated work into one change.

---

# 71. NO OPPORTUNISTIC REFACTORING

Do not use a feature or bug fix as an excuse to perform unrelated:

```text
Folder restructuring
Naming cleanup
Architecture rewrite
Dependency migration
Framework upgrade
Protocol changes
```

---

# 72. NO REWRITE BY DEFAULT

A working subsystem should not be rewritten from scratch unless:

```text
The requirement explicitly requests a rewrite
The current implementation is fundamentally incompatible
The existing architecture cannot support the required behavior
```

---

# 73. CHANGE TRACEABILITY

Significant changes must be traceable:

```text
Requirement
    ->
Design decision
    ->
Files
    ->
Classes
    ->
Methods
    ->
Tests
    ->
Result
```

---

# 74. ARCHITECTURE COMPLIANCE GATE

Before accepting new functionality ask:

```text
Does it belong in this layer?
Does it preserve module ownership?
Does it introduce coupling?
Does it bypass an abstraction?
Does it add global state?
Does it duplicate logic?
Does it duplicate state?
Does it create a God class?
Does it create a God function?
Does it change protocol behavior?
Does it increase memory risk?
Does it introduce blocking behavior?
Does it change lifecycle?
Does it change concurrency?
```

Any problematic answer requires review.

---

# 75. FILE STRUCTURE RULES

New files must follow the existing project organization.

Typical ownership:

```text
protocol/
transport/
services/wifi/
services/bluetooth/
services/a2dp/
services/network/
services/system/
core/
config/
application/
utils/
```

Use the actual project structure as the final authority.

Do not invent a new folder system without need.

---

# 76. ROOT DIRECTORY RULE

Keep the project root limited to true project-level artifacts.

Do not place arbitrary source files in the root.

---

# 77. TEMPORARY ARTIFACT RULE

Do not leave temporary files such as:

```text
temp.cpp
test2.cpp
backup.cpp
old_main.cpp
debug_old.cpp
```

in the final project.

---

# 78. CLEAN CODE RULE

Code should be:

```text
Readable
Predictable
Cohesive
Consistent
Testable
```

Do not sacrifice correctness for stylistic purity.

---

# 79. COMMENT RULE

Comments should explain:

```text
Why
Constraint
Hardware limitation
Protocol limitation
Non-obvious behavior
```

Do not use comments to justify architecture violations.

---

# 80. SECURITY AND ROBUSTNESS

Changes must avoid introducing:

```text
Buffer overflows
Out of bounds access
Use after free
Dangling references
Unchecked allocation failures
Unchecked network input
Unsafe parsing
Unbounded input
```

Any external input must be validated.

---

# 81. RESOURCE FAILURE HANDLING

Important resource failures must have defined behavior.

Examples:

```text
Memory allocation failure
Connection failure
Scan failure
Timeout
Unavailable hardware
Invalid state
```

Do not assume resources always succeed.

---

# 82. API STABILITY

Public APIs should remain stable where practical.

Before breaking an API, consider:

```text
Extension
Overload
Adapter
Wrapper
Compatibility method
```

---

# 83. BACKWARD COMPATIBILITY

When possible preserve:

```text
Protocol compatibility
Client compatibility
Public API compatibility
Configuration compatibility
Runtime behavior
```

Breaking changes require explicit justification.

---

# 84. BUILD WARNINGS

Important compiler warnings must be reviewed.

Do not hide warnings simply by disabling diagnostics unless there is a documented reason.

---

# 85. STATIC QUALITY CHECK

Before finalizing significant changes inspect:

```text
Unused includes
Unused variables
Dead code
Duplicate code
Magic values
Unchecked return values
Unsafe casts
Null handling
Lifetime issues
Buffer limits
Const correctness
Signed/unsigned mismatches
Ownership ambiguity
```

---

# 86. MEMORY VERIFICATION

For relevant changes verify:

```text
Flash usage
RAM usage
Heap usage
Stack usage
Dynamic allocation
Buffer sizes
Fragmentation risk
```

Measure before and after for high-risk memory changes when possible.

---

# 87. PERFORMANCE GOVERNANCE

Do not optimize based on assumptions.

Performance changes should identify:

```text
Current behavior
Target behavior
Measurement method
Expected improvement
Actual improvement
Resource cost
```

---

# 88. NO PREMATURE OPTIMIZATION

Do not add complexity for hypothetical performance problems.

Optimize measured bottlenecks or explicit requirements.

---

# 89. HARDWARE DEPENDENCY GOVERNANCE

Hardware-specific behavior must be documented.

Examples:

```text
Pins
UART
CP2102
Flash
Boot mode
Bluetooth hardware
WiFi hardware
A2DP hardware interaction
```

Do not abstract hardware merely for theoretical purity if it makes the firmware harder to maintain.

---

# 90. PRACTICAL OOP RULE

Architecture must remain practical for embedded systems.

Avoid unnecessary:

```text
Deep inheritance trees
Heavy generic abstractions
Large STL structures
Excessive dynamic allocation
Artificial interfaces
Excessive indirection
```

Use abstraction where it provides measurable architectural value.

---

# 91. PROJECT CHANGE RISK

Classify significant changes:

```text
LOW
MEDIUM
HIGH
CRITICAL
```

Examples:

```text
LOW:
Documentation
Comments
Non-functional local cleanup

MEDIUM:
Local service logic
Internal state changes

HIGH:
Protocol
Transport
Bluetooth
BLE
A2DP
Network
Memory
Dependencies
Build configuration

CRITICAL:
Framework migration
Partition change
Protocol breaking change
Architecture rewrite
Major subsystem rewrite
```

HIGH and CRITICAL changes require expanded verification.

---

# 92. HIGH AND CRITICAL CHANGE REQUIREMENTS

Before implementation, provide:

```text
Architecture impact
Dependency impact
Protocol impact
Memory impact
Runtime impact
Regression plan
Rollback plan
```

---

# 93. ROLLBACK RULE

Important changes must be reversible.

Record:

```text
What changed
Which files changed
Which APIs changed
Which configuration changed
Which commit introduced the change
How to revert it
```

---

# 94. FAILED CHANGE PROCEDURE

If a change causes a regression:

```text
Stop
    ->
Identify the regression
    ->
Compare with baseline
    ->
Determine root cause
    ->
Fix or revert
    ->
Rebuild
    ->
Retest
```

Do not stack unrelated fixes on top of a known broken change.

---

# 95. RELEASE GATE

A build or revision must not be considered final until the relevant checks pass.

Required categories:

```text
Architecture
Build
Protocol
Runtime
Memory
Regression
Documentation
```

The required depth depends on change scope.

---

# 96. FINAL VERIFICATION STATUS

Use only:

```text
PASS
FAIL
PARTIAL
BLOCKED
NOT TESTED
NOT VERIFIED
UNKNOWN
NOT APPLICABLE
```

Do not use a vague "OK".

---

# 97. FINAL VERIFICATION REPORT

For significant releases or changes, provide:

```text
==================================================
ESP32 WIRELESS DONGLE
FINAL VERIFICATION REPORT
==================================================

PROJECT VERSION:

ARCHITECTURE:
PASS / FAIL / PARTIAL

BUILD:
PASS / FAIL / PARTIAL

PROTOCOL:
PASS / FAIL / PARTIAL

TRANSPORT:
PASS / FAIL / PARTIAL

WIFI:
PASS / FAIL / PARTIAL

BLUETOOTH CLASSIC:
PASS / FAIL / PARTIAL

BLE:
PASS / FAIL / PARTIAL

A2DP:
PASS / FAIL / PARTIAL

TCP:
PASS / FAIL / PARTIAL

UDP:
PASS / FAIL / PARTIAL

MEMORY:
PASS / FAIL / PARTIAL

LOGGING:
PASS / FAIL / PARTIAL

ERROR HANDLING:
PASS / FAIL / PARTIAL

DOCUMENTATION:
PASS / FAIL / PARTIAL

REGRESSION:
PASS / FAIL / PARTIAL

==================================================
KNOWN ISSUES
==================================================

1.
2.
3.

==================================================
PRE-EXISTING ISSUES
==================================================

1.
2.
3.

==================================================
REGRESSIONS
==================================================

1.
2.
3.

==================================================
NOT VERIFIED
==================================================

1.
2.
3.

==================================================
ARCHITECTURAL RISKS
==================================================

1.
2.
3.

==================================================
FINAL STATUS
==================================================

PASS
PARTIAL
FAIL
BLOCKED
NOT VERIFIED
```

---

# 98. PROJECT GOVERNING CHECKLIST

Before accepting a significant project revision:

## Architecture

```text
[ ] Layer structure preserved
[ ] Module boundaries preserved
[ ] Dependency direction preserved
[ ] No unnecessary layer bypass
[ ] No circular dependency
[ ] No God class
[ ] No God function
[ ] Ownership is clear
[ ] State ownership is clear
```

## OOP

```text
[ ] Responsibilities are clear
[ ] Interfaces are meaningful
[ ] Coupling is controlled
[ ] Cohesion is acceptable
[ ] Composition is used where appropriate
[ ] Inheritance is justified
[ ] Global mutable state is controlled
```

## Protocol

```text
[ ] Commands are preserved
[ ] IDs are preserved
[ ] Responses are preserved
[ ] Errors are preserved
[ ] Events are defined
[ ] Versioning is clear
```

## Transport

```text
[ ] Transport is isolated
[ ] Read path verified
[ ] Write path verified
[ ] Buffering verified
[ ] Lifecycle verified
```

## Wireless

```text
[ ] WiFi verified
[ ] Bluetooth Classic verified
[ ] BLE verified
[ ] A2DP verified
```

## Network

```text
[ ] TCP Client verified
[ ] TCP Server verified
[ ] UDP verified
```

## Memory

```text
[ ] Flash checked
[ ] RAM checked
[ ] Heap checked
[ ] Stack checked
[ ] Dynamic allocation checked
[ ] Buffer sizes checked
```

## Build

```text
[ ] PlatformIO configuration verified
[ ] Dependencies verified
[ ] Framework version verified
[ ] Board verified
[ ] Partition verified
[ ] Build passes
[ ] Link passes
```

## Runtime

```text
[ ] Boot verified
[ ] Setup verified
[ ] Loop verified
[ ] Protocol verified
[ ] Error paths verified
[ ] Edge cases verified
```

## Documentation

```text
[ ] README current
[ ] Architecture docs current
[ ] Protocol docs current
[ ] Dependency docs current
[ ] State docs current
[ ] Known issues documented
```

---

# 99. CHANGE APPROVAL RULE

A change is approved only when:

```text
Requirement satisfied
AND
Architecture preserved
AND
Behavior preserved unless explicitly changed
AND
Build passes
AND
Required tests pass
AND
No unexplained regression exists
AND
Documentation is consistent
AND
High-risk changes have rollback information
```

Otherwise the status must not be PASS.

---

# 100. FINAL PROJECT PRINCIPLE

The project must evolve through controlled engineering changes.

The preferred model is:

```text
Existing Stable System
        +
Controlled Requirement
        ->
Minimal Implementation
        ->
Verification
        ->
Documented New Baseline
```

Not:

```text
New Requirement
        ->
Uncontrolled Refactoring
        ->
Architecture Drift
        ->
Hidden Dependency
        ->
Regression
        ->
Repeated Patching
```

---

# 101. FINAL RULE

Before making any important decision, ask:

```text
Does this preserve the existing architecture?

Does this preserve existing behavior?

Does this preserve protocol compatibility?

Does this preserve module ownership?

Does this preserve dependency direction?

Does this preserve state ownership?

Does this preserve memory constraints?

Can the change be implemented more simply?

Can the change be verified?

Can the change be reversed?
```

If the answer is not clear, do not assume.

Investigate the source, record the evidence, and make the smallest justified change.

The permanent project strategy is:

```text
UNDERSTAND
    ->
PRESERVE
    ->
EXTEND
    ->
VERIFY
    ->
DOCUMENT
    ->
MAINTAIN
```
