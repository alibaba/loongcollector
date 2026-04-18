---
name: mermaid
description: Mermaid diagram conventions. Use whenever diagrams are needed in documentation or code review.
---
# Mermaid Diagram Conventions

## Rules for Creating Mermaid Diagrams

1. **Use Correct Fenced Code Block**: Always use ````mermaid ... ````

2. **Stick to Well-Supported Diagram Types**:
   - `graph` (flowcharts, `TD` preferred for readability)
   - `sequenceDiagram`
   - `classDiagram`
   - `stateDiagram-v2` (prefer v2)
   - `erDiagram`
   - `pie`, `gantt`, `mindmap` (basic only)
   - Avoid very new or uncommon types

3. **Simple Standard Syntax**:
   - **Node IDs**: Use simple alphanumeric IDs (`node1`, `processA`). No spaces or special chars.
   - **Labels**: **Use quotes** for labels with spaces/punctuation/keywords.
     - Good: `A["User Input"] --> B["Validate Data"];`
     - Bad: `A[User Input] --> B[Validate Data];`
   - Use standard arrows (`-->`, `---`, `==>`)
   - Comments: `%%`

4. **Mindmap (GitHub compatible)**:
   - Use basic indentation structure only
   - NO `::icon()` syntax (causes rendering errors)
   - Each node on its own line with correct indentation

5. **Prefer Vertical Layouts**: `graph TD` or `graph TB` for flowcharts (easier to read in Markdown)

6. **Let GitHub Handle Styling**:
   - DO NOT set themes (`%%{init: ...}`)
   - DO NOT use `classDef` or `style`
   - GitHub auto-adapts to light/dark mode

7. **Keep Diagrams Focused**: Break complex diagrams into multiple simpler ones

8. **Always Review Automated Edits**: Tools may break Mermaid syntax, especially with indentation-heavy formats like mindmap
