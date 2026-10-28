---
name: mermaid
description: Whenever diagrams or mermaid is mentioned
---
Here are the essential rules for creating Mermaid diagrams that render well in GitHub Markdown:


1.  **Use the Correct Fenced Code Block:**

    *   Always start the block with ```mermaid and end it with ```.

    *   ```markdown

        ```mermaid

          graph TD

            A[Start] --> B{Decision};

            B -- Yes --> C[Action 1];

            B -- No --> D[Action 2];

        ```

        ```


2.  **Stick to Well-Supported Diagram Types:**

    *   GitHub generally has good support for the most common types:

        *   `graph` (Flowcharts - `TD` is often best for readability)

        *   `sequenceDiagram`

        *   `classDiagram`

        *   `stateDiagram-v2` (Prefer v2 for better features/rendering)

        *   `erDiagram` (Entity Relationship)

        *   `pie` (Pie charts)

        *   `gantt` (Gantt charts)

        *   `mindmap` (Mind Maps - Basic structure only. *See syntax notes below*)

    *   Avoid extremely new or less common diagram types, as GitHub's Mermaid version might lag slightly behind the latest release.


3.  **Keep Syntax Simple and Standard:**

    *   **Node IDs:** Use simple alphanumeric IDs (e.g., `node1`, `processA`, `userAuth`). Avoid spaces or special characters in IDs.

    *   **Node/Actor/State Labels:** **Use quotes (`"..."`)** for labels, especially if they contain spaces, punctuation, special characters, or Mermaid keywords. This is the most common source of rendering errors.

        *   *Good:* `A["User Input"] --> B["Validate Data"];`

        *   *Bad (potential error):* `A[User Input] --> B[Validate Data];`

    *   **Arrows:** Use standard arrow types (`-->`, `---`, `==>`, `->>`, etc.).

    *   **Comments:** Use `%%` for comments if needed.


4.  **Mindmap Specifics for GitHub:**

    *   GitHub **supports the basic `mindmap` structure**, using indentation to define hierarchy (parent/child relationships).

    *   Ensure each node/item is on its **own line** with correct indentation relative to its parent.

    *   GitHub **DOES NOT support** experimental or advanced `mindmap` features like `::icon()` syntax. Using icons **will cause rendering errors**.

    *   Stick to plain text nodes for mind maps intended for GitHub Markdown.

    *   *Correct Example (GitHub compatible):*

        ```mermaid

        mindmap

          Root

            Parent

              Child 1

              Child 2

        ```

    *   *Incorrect Example (GitHub incompatible due to icons):*

        ```mermaid

        mindmap

          Root

            Parent

              ::icon(fa fa-one) Child 1

              ::icon(fa fa-two) Child 2

        ```


5.  **Prefer Vertical Layouts (`graph TD`):**

    *   For flowcharts (`graph`), `TD` (Top Down) or `TB` (Top Bottom) usually renders more readably within the flow of a Markdown document than `LR` (Left Right). GitHub often gives diagrams ample width, but vertical flows are easier to follow on typical screens.


6.  **Let GitHub Handle the Styling:**

    *   **DO NOT** try to set themes (e.g., `%%{init: {'theme': 'dark'}}%%`) or apply custom styling using `classDef` or `style` *within the Mermaid code*.

    *   GitHub **ignores** these theme directives and applies its own styling based on whether the user is viewing GitHub in light, dark, or dimmed mode. Your diagram will automatically adapt. Trying to force colors or themes will likely just be ignored or look out of place.


7.  **Keep Diagrams Focused:**

    *   Avoid overly complex diagrams with excessive nodes, edges, or nesting in a single block. While GitHub *can* render complex ones, they might become hard to read or hit rendering limits. Break down very complex ideas into multiple simpler diagrams if possible.


8.  **Test in GitHub Preview:**

    *   Always use GitHub's preview tab when editing your Markdown file to see exactly how the diagram will render. This is the best way to catch syntax errors or unexpected layout issues.


9.  **Tooling & Edit Verification Note:**

    *   Automated code editing tools may sometimes struggle to apply changes correctly within Mermaid blocks, especially with complex syntax involving indentation or specific formatting (like the mindmap example above).

    *   **Always carefully review edits** made to Mermaid blocks by automated tools. If errors persist or the diagram doesn't render as expected, manual correction might be necessary.


**In short:** Write standard, clearly quoted Mermaid syntax using common diagram types. For mindmaps in GitHub, use only the basic indentation structure (no icons). Prefer `graph TD`. Avoid trying to control the theme/colors yourself. Use the GitHub preview, and double-check automated edits to Mermaid blocks.
