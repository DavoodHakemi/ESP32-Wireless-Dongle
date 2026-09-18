# ESP32 Wireless Dongle

# Change Management and Architecture Preservation Rules

This document defines the mandatory rules for all future changes to the ESP32 Wireless Dongle project.

These rules apply to:

* Bug fixes
* New features
* Refactoring
* Performance changes
* Memory optimization
* Protocol changes
* API changes
* Dependency changes
* Build changes
* Configuration changes
* Logging changes
* State machine changes
* File changes
* Class changes
* Interface changes
* Documentation changes
* Hardware related changes

The main objective is:

```text
Understand
    ->
Preserve
    ->
Change
    ->
Verify
    ->
Document
```

The default rule is:

> Make the smallest safe change that satisfies the requirement while preserving the existing architecture and behavior.

---

# 1. SOURCE OF TRUTH

The actual source code is the primary source of truth.

When these sources conflict:

```text
Actual implementation
Build configuration
Runtime evidence
Tests
Documentation
Comments
Names
Assumptions
```

the higher item has priority over the lower item.

Do not invent behavior based on file names, class names, comments, or assumptions.

If something cannot be established from the project, mark it as:

```text
UNKNOWN
NOT VERIFIED
NOT IMPLEMENTED
NOT DETERMINED FROM SOURCE
```

---

# 2. MANDATORY AUDIT BEFORE CHANGE

No change may start before the affected area is understood.

At minimum inspect:

```text
Project tree
platformio.ini
src/
include/
lib/
test/
docs/
Build flags
Framework version
Board
Partition scheme
Library dependencies
Relevant source files
Relevant tests
Relevant runtime behavior
```

Identify:

```text
Layers
Modules
Classes
Interfaces
Functions
Globals
Dependencies
Commands
Responses
Events
Callbacks
States
State transitions
Resource ownership
Memory sensitive areas
```

---

# 3. CHANGE CLASSIFICATION

Every change must be classified as one of:

```text
TYPE A - Bug Fix
TYPE B - Small Enhancement
TYPE C - New Feature
TYPE D - Refactoring
TYPE E - Architecture Change
TYPE F - Protocol Change
TYPE G - Dependency Change
TYPE H - Build or Configuration Change
TYPE I - Performance or Memory Change
TYPE J - Documentation Only
```

The change type determines the required verification scope.

---

# 4. CHANGE SCOPE

Every change must have a scope:

```text
LOCAL
MODULE
LAYER
CROSS-LAYER
SYSTEM-WIDE
```

The scope must be known before implementation.

---

# 5. CONFLICT RESOLUTION PRIORITY

When two rules or goals conflict, use the following priority order:

```text
1. Safety and data integrity
2. Explicit user requirement
3. Existing externally visible behavior and protocol compatibility
4. Existing architecture and layer boundaries
5. Build correctness and framework compatibility
6. Functional correctness
7. Resource and memory constraints
8. Testability
9. Maintainability
10. Code style and naming preferences
11. Refactoring elegance
12. Cosmetic improvements
```

Higher priority rules override lower priority rules.

Examples:

```text
Protocol compatibility overrides cleanup.
Functional correctness overrides cosmetic refactoring.
Architecture preservation overrides a cleaner folder layout.
Memory safety overrides abstraction elegance.
Explicit user requirements override default project conventions.
```

If a lower priority goal requires violating a higher priority rule, do not make the change silently.

Record the conflict and explain the chosen resolution.

---

# 6. MINIMAL CHANGE RULE

Use the smallest change that can safely satisfy the requirement.

Preferred order:

```text
1. Modify existing implementation
2. Extend existing class
3. Extend existing module
4. Add a method
5. Add a class inside the existing module
6. Add a file inside the existing module
7. Add a new module only when required
8. Add a new layer only as a last resort
```

Do not redesign the project simply because a new feature could be implemented differently.

---

# 7. STRUCTURE PRESERVATION

The existing project structure is a protected baseline.

Preserve, unless technically required:

```text
Directory structure
Layer structure
Module ownership
Class ownership
Interface boundaries
Protocol boundaries
Transport boundaries
Dependency direction
File naming convention
Naming convention
Public APIs
State ownership
Runtime lifecycle
```

Do not move code merely because another location looks cleaner.

---

# 8. NO UNNECESSARY FILE MOVEMENT

Do not move a file unless the current location is objectively wrong for the responsibility.

If a file must be moved:

```text
Record old path
Record new path
Search all references
Update includes
Update build references
Build the project
Verify no orphan file remains
Document the move
```

---

# 9. NO UNNECESSARY RENAMING

Do not rename:

```text
Directories
Files
Classes
Interfaces
Methods
Commands
Enums
Constants
```

unless at least one of the following applies:

```text
The name is technically misleading
The name causes an actual ambiguity
The name violates a mandatory project convention
The user explicitly requests the rename
```

Any rename must include impact analysis.

---

# 10. NO UNNECESSARY NEW FILES

Before creating a file, answer:

```text
Why can the existing file not own this responsibility?
Why can the existing class not implement this safely?
Why does a separate file improve ownership or isolation?
```

If there is no strong technical reason, do not create the file.

---

# 11. NO UNNECESSARY NEW CLASSES

Before creating a class, check:

```text
Can the existing class own the behavior?
Can an existing method be extended?
Can composition solve the problem?
Would a new class reduce coupling or improve testability?
```

A new class must have a clear responsibility.

---

# 12. NO ARTIFICIAL INTERFACES

Do not create interfaces only to make the project look more object oriented.

An interface should provide real abstraction value and have:

```text
A clear contract
A meaningful consumer
A meaningful implementation
A reason for substitution or isolation
```

---

# 13. LAYER PRESERVATION

The existing layer architecture must remain recognizable.

Typical direction:

```text
Application
    ->
Protocol
    ->
Services
    ->
Abstraction or Hardware
    ->
ESP32 or Arduino or Third Party APIs
```

Transport remains an independent concern.

Do not introduce dependency inversion violations.

---

# 14. NO LAYER BYPASS

A new implementation must not bypass established layers just because direct access is easier.

Examples of forbidden shortcuts:

```text
Service -> Serial output
Service -> Protocol encoding
Protocol -> ESP32 hardware API
Hardware -> Application logic
Callback -> arbitrary business logic
```

Use the established architectural path.

---

# 15. TRANSPORT AND PROTOCOL SEPARATION

Transport defines how data moves.

Protocol defines what data means.

Do not couple protocol logic to a specific transport unless explicitly required.

Adding a transport should not require rewriting unrelated services.

---

# 16. SERVICE BOUNDARIES

Each service must keep its own domain responsibility.

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

Do not move logic between services without a concrete architectural reason.

---

# 17. APPLICATION RESPONSIBILITY

Application code is responsible for system orchestration and lifecycle.

Do not move business logic into:

```text
main.cpp
setup()
loop()
```

unless it is genuinely startup or application orchestration logic.

---

# 18. MAIN LOOP RULE

The main loop must remain small and predictable.

The preferred architecture is similar to:

```cpp
void setup()
{
    application.begin();
}

void loop()
{
    application.update();
}
```

The exact implementation may differ, but the principle remains:

```text
main.cpp should not become a business logic container.
```

---

# 19. NO GOD CLASS

Do not create or expand a class until it becomes responsible for unrelated domains such as:

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

Split responsibilities when necessary, but preserve existing module boundaries when possible.

---

# 20. NO GOD FUNCTION

Do not turn functions such as:

```text
loop()
handleCommand()
processCommand()
setupBluetooth()
processMenu()
```

into containers for unrelated logic.

Functions should remain:

```text
Focused
Cohesive
Predictable
Testable
```

---

# 21. SINGLE RESPONSIBILITY

Each class, module, and function should have a clear responsibility.

A change must not combine unrelated functionality only because the code is nearby.

---

# 22. DEPENDENCY DIRECTION

Avoid dependencies from lower layers to higher layers.

Do not introduce:

```text
Hardware -> Application
Service -> CLI
Protocol -> Concrete hardware implementation
Transport -> Business logic
```

without explicit architectural justification.

---

# 23. GLOBAL STATE RULE

Every mutable global must be reviewed.

Possible actions:

```text
KEEP
MOVE INTO CLASS
MOVE INTO CONTEXT
MOVE INTO CONFIGURATION
CONVERT TO CONSTANT
REMOVE
```

New mutable global state requires strong justification.

---

# 24. SINGLETON RULE

Do not introduce a Singleton only to avoid dependency injection.

A Singleton requires a clear reason related to:

```text
Global ownership
Global lifecycle
Hardware limitation
System-wide uniqueness
```

---

# 25. PUBLIC API PRESERVATION

Existing public APIs should remain stable unless the requirement requires a change.

Prefer:

```text
Extend
Adapt
Wrap
Compose
```

before:

```text
Break
Rename
Replace
```

---

# 26. INTERFACE CHANGE RULE

Changing an interface is a high impact operation.

Before changing an interface, check:

```text
Can implementation be changed instead?
Can an adapter solve it?
Can a wrapper solve it?
Can a new method be added?
Can composition solve it?
```

Only change the interface when necessary.

---

# 27. ADAPTER BEFORE BREAKING CHANGE

When a new library, subsystem, or API conflicts with the current architecture, prefer an adapter or compatibility layer before changing existing boundaries.

---

# 28. STATE OWNERSHIP

Every important state must have one clear owner.

Do not move state from a service to another layer just because the new location appears convenient.

---

# 29. STATE MACHINE CHANGES

Any change to a state machine must document:

```text
Old states
New states
Added transitions
Removed transitions
Changed transitions
New error paths
Behavior changes
```

Do not introduce hidden state changes.

---

# 30. NO DUPLICATE STATE

Do not maintain the same conceptual state in multiple unrelated variables or classes unless synchronization is explicitly defined.

---

# 31. CALLBACK RULE

Callback code must respect architecture boundaries.

Do not use callbacks as a shortcut to:

```text
Write protocol output
Write raw terminal output
Execute unrelated business logic
Modify unrelated services
```

Callbacks should update local state or emit defined events.

---

# 32. EVENT RULE

Commands and events are different concepts.

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

# 33. ERROR HANDLING

Do not introduce inconsistent error handling such as random use of:

```text
false
-1
nullptr
magic numbers
raw strings
Serial messages
```

Use the project's existing error model.

If the project has a Result or ErrorCode abstraction, extend it instead of creating another unrelated error mechanism.

---

# 34. ERROR PROPAGATION

Errors should remain traceable from low level to high level:

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

Do not hide errors inside log messages.

---

# 35. LOGGING RULE

Use the existing logger or logging abstraction.

Do not solve debugging tasks by scattering:

```cpp
Serial.println("DEBUG");
```

through the project.

Any new logs must use the established format and log levels.

---

# 36. TERMINAL OUTPUT RULE

Do not change terminal output format unless the requirement explicitly needs it.

Treat externally consumed output as an interface.

Before changing output, check:

```text
Python client
Tests
Protocol parser
Automation
Documentation
User tools
```

---

# 37. NO MAGIC STRINGS

Do not introduce duplicated command strings, state strings, error strings, or other protocol strings.

Use the existing centralized representation where appropriate.

---

# 38. NO MAGIC NUMBERS

Avoid new hard-coded values for:

```text
Command IDs
Timeouts
Buffer sizes
Pins
Baud rates
States
Feature flags
Protocol versions
```

Use the project's configuration, constants, or enums.

---

# 39. CONFIGURATION OWNERSHIP

Each important configuration value should have one authoritative definition.

Examples:

```text
Firmware version
Device name
Baud rate
Timeout
Pin
Partition
Feature flag
```

Do not duplicate definitions across files.

---

# 40. VERSION OWNERSHIP

Firmware version must have one source of truth.

Do not independently hard-code different versions in:

```text
Source files
Protocol code
CLI
Build flags
Documentation
```

unless the difference is intentional and documented.

---

# 41. DEPENDENCY ADDITION

Before adding a library, verify:

```text
Is it really needed?
Does the project already provide the functionality?
Can the feature be implemented without it?
Is it compatible with the current ESP32 framework?
What is the flash impact?
What is the RAM impact?
What transitive dependencies does it add?
Is it maintained?
Does it introduce API or licensing concerns?
```

---

# 42. NO HIDDEN DEPENDENCIES

All dependencies required to build and run the project must be declared in the appropriate project configuration.

The project must not depend on undocumented local libraries.

---

# 43. DEPENDENCY VERSION CONTROL

Do not upgrade a library or framework as part of an unrelated bug fix unless required.

Dependency upgrades are their own change category.

---

# 44. FRAMEWORK PRESERVATION

Do not replace:

```text
Arduino
PlatformIO
ESP32 framework
Board definition
```

during a normal feature or bug fix.

Framework migration requires explicit approval or a dedicated architecture change.

---

# 45. PARTITION PRESERVATION

Do not change the partition scheme during unrelated work.

A partition change requires explicit impact analysis for:

```text
Firmware size
Flash layout
OTA
Filesystem
Boot behavior
```

---

# 46. FILE PLACEMENT

New code must be placed in the existing module that owns the responsibility.

Examples:

```text
Protocol -> protocol/
Transport -> transport/
WiFi -> services/wifi/
Bluetooth -> services/bluetooth/
A2DP -> services/a2dp/
Configuration -> config/
Core types -> core/
```

Use the actual project structure if it differs.

Do not create a new folder convention without architectural need.

---

# 47. ROOT DIRECTORY CLEANLINESS

Do not add arbitrary source files to the project root.

Root level should contain only true project-level artifacts.

---

# 48. NO DUPLICATE IMPLEMENTATION

Before adding functionality, search for an existing implementation.

Prefer:

```text
Reuse
Extend
Extract
Adapt
```

instead of duplicating code.

---

# 49. NO DUPLICATE CONSTANTS

One conceptual value should have one authoritative definition.

---

# 50. NO DUPLICATE LOGIC

Equivalent behavior should not be implemented independently in multiple services or handlers.

If duplication already exists and the change touches it, consider whether extraction is required, but avoid unrelated refactoring.

---

# 51. MEMORY SAFETY

Every change must consider:

```text
Heap
Stack
Flash
RAM
Dynamic allocation
String fragmentation
Large buffers
BLE memory
A2DP buffers
TCP buffers
Temporary objects
```

Avoid unnecessary allocations, especially inside loops and callbacks.

---

# 52. RESOURCE OWNERSHIP

For each important resource, determine:

```text
Who creates it?
Who owns it?
Who uses it?
Who releases it?
What is its lifetime?
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
Callbacks
Timers
```

Do not change ownership implicitly.

---

# 53. LIFETIME PRESERVATION

Do not change the lifetime of important objects without explicit analysis.

Be especially careful with:

```text
Bluetooth stack
BLE scanning
A2DP objects
WiFi clients
TCP sockets
UDP sockets
Callbacks
Buffers
```

---

# 54. NON-BLOCKING RULE

Do not introduce new unbounded blocking behavior.

Review:

```text
delay()
busy waits
blocking loops
long synchronous calls
```

Any blocking operation must be:

```text
Necessary
Bounded
Documented
```

---

# 55. CONCURRENCY PRESERVATION

Do not silently change:

```text
Synchronous -> asynchronous
Callback -> task
Task -> callback
Single context -> multi-context
```

Such changes are architecture changes.

---

# 56. THREAD AND TASK SAFETY

If shared state is changed, identify:

```text
Who reads it?
Who writes it?
From which context?
Is synchronization required?
```

Do not assume thread safety without evidence.

---

# 57. PROTOCOL PRESERVATION

Existing protocol behavior is protected.

Do not casually change:

```text
Command IDs
Command syntax
Parameters
Responses
Errors
Events
Protocol version
```

---

# 58. NEW COMMAND CHECKLIST

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
Documentation
Tests
```

---

# 59. COMMAND REMOVAL

Do not remove a command without checking:

```text
All source references
Python clients
Tests
Documentation
Automation
External consumers
```

A command removal is a protocol change.

---

# 60. A2DP PRESERVATION

A2DP behavior is protected.

Changes must preserve, unless explicitly required:

```text
Status
Connect by MAC
Connect by name
Disconnect
Test tone ON
Test tone OFF
Streaming behavior
Callbacks
State management
```

---

# 61. BLUETOOTH PRESERVATION

Bluetooth Classic and BLE must remain logically distinguishable.

Do not merge them into a generic implementation unless architecture explicitly requires it.

---

# 62. NETWORK PRESERVATION

TCP Client, TCP Server, and UDP must remain separate responsibilities.

Do not move protocol logic into network implementations.

---

# 63. BUILD PRESERVATION

For source changes:

```text
Compile
Link
Build
```

must be checked.

For changes affecting dependencies or build configuration, perform a clean build.

---

# 64. BUILD REPRODUCIBILITY

The project must build from the declared PlatformIO configuration without hidden IDE settings or undocumented local files.

---

# 65. NO TEMPORARY ARTIFACTS

Do not leave temporary files such as:

```text
test2.cpp
temp.cpp
backup.cpp
old_main.cpp
debug_old.cpp
```

in the final source tree.

---

# 66. NO OPPORTUNISTIC REFACTORING

During a focused change, do not also:

```text
Rename unrelated classes
Reorganize unrelated folders
Rewrite working modules
Change unrelated dependencies
Change protocol format
Replace framework
Rewrite logging
```

unless required by the same technical problem.

---

# 67. NO REWRITE BY DEFAULT

Do not rewrite a subsystem from scratch unless:

```text
Current implementation is fundamentally incompatible
The requirement explicitly requests a rewrite
Existing architecture cannot support the requirement
```

A rewrite must have explicit justification.

---

# 68. CHANGE ISOLATION

Each change should be:

```text
Small
Focused
Atomic
Reviewable
Reversible
```

One logical purpose per change is preferred.

---

# 69. FORMATTING ISOLATION

Do not perform large formatting changes during a functional change.

Avoid turning a small fix into a massive diff caused by formatting.

---

# 70. DOCUMENTATION CONSISTENCY

If a change affects:

```text
Architecture
Protocol
State
Class behavior
Dependencies
Configuration
Lifecycle
```

update the related documentation.

Documentation must describe the actual implementation, not an intended future design.

---

# 71. TEST PRESERVATION

Do not delete or weaken an existing test just to make the project pass.

If a test becomes invalid:

```text
Explain why
Update it
Add a replacement when needed
```

---

# 72. REGRESSION TEST RULE

Important bug fixes should result in a regression test when practical.

Preferred flow:

```text
Bug
    ->
Failing test
    ->
Fix
    ->
Passing test
```

---

# 73. BASELINE BEFORE CHANGE

Record, when relevant:

```text
Project version
Git commit
Build result
Firmware size
RAM usage
Relevant logs
Relevant runtime behavior
Affected feature behavior
```

This allows:

```text
BEFORE
vs
AFTER
```

comparison.

---

# 74. EXISTING BUILD FAILURE

If the project does not build before the change:

```text
Record the baseline failure
Do not attribute it to the new change
Separate pre-existing failures from introduced failures
```

---

# 75. IMPACT ANALYSIS

Before implementation, identify:

```text
Affected files
Affected classes
Affected methods
Affected interfaces
Affected layers
Affected services
Affected commands
Affected events
Affected states
Affected dependencies
Memory impact
Build impact
Test impact
Documentation impact
```

Use:

```text
NONE
```

when a category is not affected.

---

# 76. CHANGE RISK

Classify each change:

```text
LOW
MEDIUM
HIGH
CRITICAL
```

HIGH and CRITICAL changes require deeper verification.

Examples:

```text
LOW:
Documentation
Comments
Internal non-functional cleanup

MEDIUM:
Local service behavior
Internal state logic

HIGH:
Protocol
Transport
Bluetooth
BLE
A2DP
Memory
Dependencies
Build configuration

CRITICAL:
Framework migration
Protocol breaking change
Partition change
Architecture rewrite
Major subsystem rewrite
```

---

# 77. HIGH AND CRITICAL CHANGE REQUIREMENTS

Before implementation, provide:

```text
Architecture impact
Dependency impact
Memory impact
Protocol impact
Regression plan
Rollback plan
```

---

# 78. ROLLBACK

Every important change must be reversible.

Record:

```text
What changed
Which files changed
Which interfaces changed
Which configuration changed
Which commit introduced it
How to revert it
```

---

# 79. FAILED CHANGE RULE

If a change introduces a regression:

```text
Stop
    ->
Identify regression
    ->
Compare against baseline
    ->
Fix or revert
    ->
Rebuild
    ->
Retest
```

Do not stack additional unrelated changes on top of a broken change.

---

# 80. TRACEABILITY

Every meaningful change must be traceable:

```text
Requirement
    ->
Design decision
    ->
Changed files
    ->
Changed classes
    ->
Changed methods
    ->
Tests
    ->
Result
```

---

# 81. CHANGE RECORD

For significant changes, use:

```text
CHANGE-ID:

DATE:

TYPE:

RISK:

REQUIREMENT:

CURRENT BEHAVIOR:

DESIRED BEHAVIOR:

AFFECTED LAYERS:

AFFECTED MODULES:

AFFECTED FILES:

AFFECTED CLASSES:

AFFECTED INTERFACES:

AFFECTED PROTOCOL:

AFFECTED STATES:

AFFECTED EVENTS:

DEPENDENCY IMPACT:

MEMORY IMPACT:

BUILD IMPACT:

TEST PLAN:

ROLLBACK PLAN:
```

---

# 82. IMPLEMENTATION SEQUENCE

Use this sequence for significant changes:

```text
Requirement
    ->
Audit
    ->
Impact analysis
    ->
Design
    ->
Implementation
    ->
Compile
    ->
Static review
    ->
Unit or integration test
    ->
Runtime test
    ->
Regression
    ->
Documentation update
    ->
Final verification
```

Do not skip a step without recording why.

---

# 83. NEW FEATURE CHECKLIST

Before finalizing a new feature:

```text
[ ] Correct layer selected
[ ] Correct module selected
[ ] Existing class reuse evaluated
[ ] New class justified if created
[ ] Interface need evaluated
[ ] State owner identified
[ ] Error handling defined
[ ] Logging defined
[ ] Protocol integration defined if required
[ ] Event behavior defined if required
[ ] Memory impact reviewed
[ ] Build impact reviewed
[ ] Tests added or updated
[ ] Documentation updated
```

---

# 84. NEW SERVICE CHECKLIST

```text
[ ] Responsibility defined
[ ] Public API defined
[ ] State defined
[ ] Interface defined if needed
[ ] Dependencies defined
[ ] Lifecycle defined
[ ] Error model defined
[ ] Callback model defined
[ ] Event model defined
[ ] Memory behavior checked
[ ] Tests defined
[ ] Documentation updated
```

---

# 85. NEW PROTOCOL COMMAND CHECKLIST

```text
[ ] Command ID
[ ] Command name
[ ] Parameters
[ ] Parameter validation
[ ] Handler
[ ] Target service
[ ] Success response
[ ] Error response
[ ] Required state
[ ] Timeout behavior
[ ] Regression test
[ ] Documentation
```

---

# 86. FINAL CHANGE VERIFICATION

Before marking a change complete:

## Structure

```text
[ ] Existing directories preserved
[ ] Existing module boundaries preserved
[ ] No unnecessary file moves
[ ] No unnecessary renames
[ ] No unnecessary new layers
[ ] No unnecessary new files
```

## Architecture

```text
[ ] Layer boundaries preserved
[ ] Dependency direction preserved
[ ] No layer bypass
[ ] No circular dependency introduced
[ ] No unnecessary coupling
[ ] Existing ownership preserved
```

## Code

```text
[ ] No duplicate implementation
[ ] No duplicate constants
[ ] No duplicate state
[ ] No unnecessary global state
[ ] No new God class
[ ] No new God function
[ ] Naming conventions preserved
```

## Protocol

```text
[ ] Existing commands preserved
[ ] Existing IDs preserved
[ ] Existing response behavior preserved
[ ] New commands documented
[ ] Error behavior preserved
```

## Runtime

```text
[ ] Lifecycle preserved
[ ] State transitions checked
[ ] Callbacks checked
[ ] Events checked
[ ] Blocking behavior checked
[ ] Resource lifetime checked
```

## Memory

```text
[ ] Heap impact checked
[ ] Stack impact checked
[ ] Flash impact checked
[ ] RAM impact checked
[ ] Dynamic allocation checked
[ ] Buffer growth checked
```

## Build

```text
[ ] Compile passed
[ ] Link passed
[ ] PlatformIO build passed
[ ] Dependencies resolved
[ ] Partition preserved
[ ] Build flags preserved
[ ] Clean build completed when required
```

## Tests

```text
[ ] New feature tested
[ ] Affected feature regression tested
[ ] Error paths checked
[ ] Edge cases checked
[ ] Runtime checked when required
[ ] Hardware checked when required
```

## Documentation

```text
[ ] README checked
[ ] Architecture docs checked
[ ] Protocol docs checked
[ ] State docs checked
[ ] Dependency docs checked
[ ] Change record created when required
```

---

# 87. REGRESSION RULE

At minimum, preserve and retest all functionality affected by the change.

The core regression set includes:

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

If the project contains additional features, they must also be included in the appropriate regression scope.

---

# 88. EDGE CASE CHECKLIST

Check relevant cases such as:

```text
Invalid command
Missing parameter
Invalid parameter
Duplicate connect
Disconnect while disconnected
Connect while busy
Scan while scanning
Timeout
Device not found
Device disappears
Bluetooth not initialized
A2DP not connected
A2DP disconnect during streaming
Invalid WiFi credentials
TCP connection failure
UDP failure
Allocation failure
Unexpected callback
Unexpected state transition
```

For each tested case record:

```text
Expected
Actual
Status
Evidence
```

---

# 89. VERIFICATION STATUS

Use only:

```text
PASS
FAIL
PARTIAL
BLOCKED
NOT TESTED
NOT VERIFIED
NOT APPLICABLE
UNKNOWN
```

Do not use "Verified" without evidence.

---

# 90. EVIDENCE RULE

For each important verification result, record evidence such as:

```text
Source file
Class
Method
Build output
Test output
Runtime log
Hardware result
```

A claim without evidence is not considered verified.

---

# 91. STATIC VS BUILD VS RUNTIME

These are different verification levels:

```text
Code inspection
    !=
Compilation
    !=
Linking
    !=
Flashing
    !=
Boot verification
    !=
Runtime test
    !=
Hardware verification
```

Passing one level does not prove the next level.

---

# 92. NO SILENT REGRESSION

If a previously working feature stops working after a change, classify it as:

```text
REGRESSION
```

Do not redefine the failure as "expected" unless the requirement explicitly changed the behavior.

---

# 93. ARCHITECTURE COMPLIANCE GATE

Before accepting any new feature or change, ask:

```text
Does it belong in this layer?
Does it preserve module ownership?
Does it introduce coupling?
Does it bypass an interface?
Does it add global state?
Does it duplicate logic?
Does it create duplicate state?
Does it create a God class?
Does it create a God function?
Does it change protocol behavior?
Does it increase memory risk?
Does it add blocking behavior?
Does it change lifecycle?
Does it change concurrency?
```

If the answer indicates a violation, review the design before implementation.

---

# 94. CHANGE REJECTION CONDITIONS

A change must be rejected or redesigned if it:

```text
Breaks architecture without justification
Creates unexplained coupling
Introduces hidden dependencies
Breaks protocol compatibility without approval
Creates uncontrolled memory growth
Creates duplicate state
Creates a God class
Creates a God function
Breaks the build
Introduces unexplained regression
Moves responsibilities across layers without reason
Changes framework or partition without requirement
Cannot be adequately verified
```

---

# 95. FINAL DEFINITION OF STRUCTURE PRESERVATION

"Structure preserved" means:

```text
Existing layers remain recognizable.
Existing modules keep their ownership.
Existing service boundaries remain intact.
Existing protocol boundaries remain intact.
Existing transport boundaries remain intact.
Existing dependency direction remains intact.
Existing file organization remains stable unless needed.
Existing public APIs remain stable where possible.
Existing state ownership remains stable.
Existing lifecycle remains stable.
Existing command behavior remains stable.
Existing runtime behavior remains stable unless explicitly changed.
```

---

# 96. FINAL CHANGE APPROVAL GATE

A change may be marked:

```text
APPROVED
```

only when:

```text
Requirement satisfied
AND
Architecture preserved
AND
Impact understood
AND
Build passes
AND
Required tests pass
AND
No unexplained regression exists
AND
Documentation is consistent
AND
Rollback is understood for high-risk changes
```

Otherwise use:

```text
PARTIAL
FAIL
BLOCKED
NOT VERIFIED
```

---

# 97. CORE PRINCIPLE

The project must evolve by extension and controlled modification, not by uncontrolled restructuring.

Preferred:

```text
Existing Architecture
        +
Required Change
        ->
Small Controlled Extension
        ->
Verified Architecture
```

Avoid:

```text
Feature Request
        ->
Rewrite Project
        ->
Move Files
        ->
Change Interfaces
        ->
Change Dependencies
        ->
Create Regressions
```

unless the requirement explicitly calls for a migration or rewrite.

---

# 98. FINAL RULE

Before every change, ask:

```text
What is the smallest safe change that satisfies the requirement
without breaking:

- the architecture
- the layer boundaries
- the module boundaries
- the interfaces
- the state ownership
- the protocol
- the dependencies
- the memory constraints
- the existing behavior
- the project structure
```

The default engineering strategy is:

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
```

Not:

```text
REWRITE
    ->
MOVE
    ->
BREAK
    ->
REPAIR
```
