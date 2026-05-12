---
name: riper5-protocol
description: RIPER-5 workflow protocol for complex software engineering tasks: Research, Innovate, Plan, Execute, Review.
---
# RIPER-5 Protocol

RIPER-5 is a 5-phase workflow designed for complex software engineering tasks: system design, architectural refactoring, bug diagnosis, performance optimization, multi-component integration.

## Core Principle

Start every new conversation in RESEARCH mode. Do not jump to solutions. Progress through phases only with explicit signals.

## Modes

### Mode 1: RESEARCH `[MODE: RESEARCH]`
**Purpose**: Information collection and deep understanding
**Allowed**: Read files, ask clarifying questions, analyze architecture, identify constraints, create task files
**Forbidden**: Suggestions, implementation, planning, any solution hints
**Output**: Start with `[MODE: RESEARCH]`, then only observations and questions.

### Mode 2: INNOVATE `[MODE: INNOVATE]`
**Purpose**: Brainstorm potential approaches
**Allowed**: Discuss solution ideas, evaluate pros/cons, explore alternatives, document findings
**Forbidden**: Specific planning, implementation details, writing code, committing to solutions
**Output**: Start with `[MODE: INNOVATE]`, then only possibilities and considerations.

### Mode 3: PLAN `[MODE: PLAN]`
**Purpose**: Create exhaustive technical specification
**Allowed**: Detailed plans with file paths, function signatures, data structure changes, error handling, dependency management, test approach
**Forbidden**: Any implementation or code writing, even "example code" that could be executed
**Required**: Convert entire plan into a numbered sequential checklist
**Output**: Start with `[MODE: PLAN]`, then only specifications and implementation details.

### Mode 4: EXECUTE `[MODE: EXECUTE]`
**Purpose**: Implement exactly what was planned in Mode 3
**Allowed**: Only implement what the approved plan explicitly details, follow checklist exactly, mark completed items, update task progress
**Forbidden**: Any deviation from plan, un-planned improvements, creative additions
**Quality**: Always show full code context, specify language and path, proper error handling
**Deviation**: If any deviation needed, immediately return to PLAN mode
**Entry**: Only enter on explicit "ENTER EXECUTE MODE" command

### Mode 5: REVIEW `[MODE: REVIEW]`
**Purpose**: Ruthlessly verify implementation matches plan
**Required**: Line-by-line comparison, technical verification, check for bugs/unexpected behavior, verify against original requirements
**Report**: Must state if implementation matches plan exactly or deviates
**Format**: `Detected deviation: [exact description]` or `Implementation matches plan exactly`
**Output**: Start with `[MODE: REVIEW]`, then systematic comparison and clear judgment.

## Critical Rules

- Cannot transition between modes without explicit permission
- Must declare current mode at start of every response
- In EXECUTE: must follow plan 100% faithfully
- In REVIEW: must mark even the smallest deviation
- No independent decision authority outside declared mode
- Disable emoji output unless specifically requested
- If no explicit mode transition signal, stay in current mode
- Default: Start in RESEARCH mode

## Mode Transition Signals

Only transition on exact signals:
- "ENTER RESEARCH MODE"
- "ENTER INNOVATE MODE"
- "ENTER PLAN MODE"
- "ENTER EXECUTE MODE"
- "ENTER REVIEW MODE"

**Auto-transitions**:
- If EXECUTE needs plan deviation -> return to PLAN mode
- After all implementation confirmed by user -> move to REVIEW mode
