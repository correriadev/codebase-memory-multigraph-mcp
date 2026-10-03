# HarnessKit UI Design Practices

This small thematic knowledge base contains reusable interface-design principles and a test mapping for the HarnessKit assistant conversation workspace. It exists to validate CBM theme discovery, graph navigation, exact citations, and horizon references.

## Catalog record

- Theme ID: `@harnesskit/ui-design-practices`
- Version: `1.0.0`
- Namespace: `product-design/interface`
- Maturity: `CURATED_FOR_E2E_TEST`
- Scope: responsive web interfaces, conversation workspaces, and component-level interaction design.
- Evidence boundary: general design principles below are guidance; dimensions and palette details in the HarnessKit mapping are synthetic fixture values, not approved product decisions.
- Graph node convention: each `T-xx` heading is a stable thematic topic. CBM indexes the file and sections as `File`/`Section` nodes with `DEFINES` edges. A section's `CBM_RELATIONS_V1[...]` declaration creates typed edges to `T-xx` sections in the same file. Put the declaration immediately after the heading: indexed section prose is capped at 500 bytes. Allowed types are `APPLIES_TO`, `CONSTRAINS`, `MOTION_SPECIFIED_BY`, `SPECIALIZES`, `SUPPORTED_BY`, `SUPPORTS`, `USES`, and `VERIFIED_WITH`. References to project files or horizon overlays remain explicit citations in their respective graphs.

## T-01 Context, scope, and applicability

Graph links: `CBM_RELATIONS_V1[APPLIES_TO=T-02; APPLIES_TO=T-03; APPLIES_TO=T-04; APPLIES_TO=T-05; APPLIES_TO=T-06; APPLIES_TO=T-07; APPLIES_TO=T-08; APPLIES_TO=T-09]`. `CONSTRAINED_BY` project design system, product decisions, and accessibility requirements remain prose because they are not topic nodes in this catalog.

Purpose: choose design guidance according to user task, information density, device size, input method, and accessibility needs.

Applicability: use these practices when shaping a new interface or revising an existing one. First identify the user task, primary action, secondary actions, content that must remain visible, and the narrowest supported viewport. Confirm project-specific standards in the project's own graph before treating a general recommendation as binding.

Exceptions: product policy, established design-system rules, user research, or technical constraints may supersede a general recommendation. Record the reason and the source of that constraint.

Verify: every proposed component or token serves an identified task; uncertain product choices remain marked as proposals until the user accepts them.

## T-02 Spacing, alignment, and layout rhythm

Graph links: `CBM_RELATIONS_V1[SUPPORTS=T-03; APPLIES_TO=T-09]`. Viewport, density, and existing layout tokens remain prose constraints.

Principle: use a small, named spacing scale and consistent alignment to make grouping and priority legible. A 4px base with common 8px increments is a useful starting proposal, not a universal mandate; choose the scale that fits the existing system.

Guidance: reserve larger gaps for distinct regions, smaller gaps for items within a group, align related controls, and keep content width readable. Define layout dimensions as tokens rather than unrelated per-component numbers. At narrow widths, reflow regions or move secondary content into an explicit accessible surface instead of compressing every column.

Exceptions: dense expert tables and data-heavy tools may require tighter spacing when scanning remains clear and targets remain usable. Do not use whitespace alone to imply a relationship that the content structure does not support.

Verify: inspect a spacing-token map, alignment of repeated controls, text measure, overflow at narrow widths, and keyboard access to any relocated region.

## T-03 Typography and visual hierarchy

Graph links: `CBM_RELATIONS_V1[SUPPORTED_BY=T-02]`. Headings, messages, navigation, metadata, localization, zoom, and content length remain prose scope and constraints.

Principle: hierarchy should make the primary task, current location, content groups, and action priority understandable before decoration is considered.

Guidance: use a limited type scale with deliberate size, weight, line height, and measure. Keep one clear primary heading per view; make labels concise; distinguish metadata from task content without making it faint; use sentence case consistently. Establish hierarchy through more than color alone.

Exceptions: a compact control may need a smaller label, but important content must remain readable at supported zoom and text-size settings. Long translated strings may require wrapping and flexible component height.

Verify: scan the page in grayscale, at 200% zoom, and with long representative text; confirm heading order reflects information structure.

## T-04 Components, anatomy, and states

Graph links: `CBM_RELATIONS_V1[SPECIALIZES=T-01; USES=T-05; MOTION_SPECIFIED_BY=T-07; VERIFIED_WITH=T-08]`.

Principle: reuse components when they share behavior and meaning, not merely because they look alike.

Guidance: document each component's purpose, anatomy, content rules, keyboard behavior, focus behavior, and states. Cover at least default, hover where applicable, focus-visible, pressed, disabled, loading, empty, success, and error states. Keep destructive or high-impact actions distinct and explain consequences before commitment.

Exceptions: transient states that cannot occur for a component may be omitted with a reason. Avoid adding decorative states that do not correspond to real behavior.

Verify: review a component/state matrix; test pointer and keyboard interaction; confirm focus does not disappear or become obscured when panels scroll.

## T-05 Color, semantic tokens, and contrast

Graph links: `CBM_RELATIONS_V1[APPLIES_TO=T-04]`. Brand palette, contrast requirements, and forced-colors mode remain prose constraints.

Principle: assign color by meaning (text, surface, border, focus, success, warning, danger) and use tokens so themes and states stay coherent.

Guidance: color must not be the only carrier of meaning. Check text/background combinations against the applicable accessibility target: WCAG 2.2 SC 1.4.3 specifies at least 4.5:1 for normal text and 3:1 for large text, subject to its stated exceptions. Check non-text controls and focus indicators against their applicable success criteria as well. Do not infer the contrast of a palette from the hue names alone; measure the actual rendered pairs.

Exceptions: logos and other cases listed by WCAG have different requirements; any exception must be checked against the criterion itself rather than generalized to nearby interface content.

Verify: test text, borders, icons, selected states, errors, and focus on every intended surface and theme.

## T-06 Responsive behavior and adaptive composition

Graph links: `CBM_RELATIONS_V1[SPECIALIZES=T-02; APPLIES_TO=T-09]`. Viewport, touch input, keyboard, and content length remain prose constraints.

Principle: preserve task priority and content relationships as the viewport changes; do not treat desktop columns as a fixed miniature layout on mobile.

Guidance: define which regions are primary, secondary, collapsible, or move to navigation. Prefer content-driven breakpoints. Preserve a visible route back to hidden content, maintain focus when panels open or close, and prevent horizontal scrolling unless the content itself requires it.

Exceptions: a specialized canvas may intentionally pan horizontally, provided the interaction is discoverable and does not hide essential controls.

Verify: test representative wide, medium, and narrow viewports, including keyboard-only navigation and browser zoom.

## T-07 Motion, transitions, and feedback

Graph links: `CBM_RELATIONS_V1[APPLIES_TO=T-04]`. Reduced-motion preference, performance, and input latency remain prose constraints.

Principle: motion should explain a state change, preserve spatial context, or provide timely feedback. It should not delay an essential action or add continuous movement without purpose.

Guidance: keep transitions brief and consistent; animate only the property needed to explain the change; avoid flashing; ensure feedback is also conveyed through text or state, not motion alone. Honor the user's reduced-motion preference with `prefers-reduced-motion`, removing or replacing non-essential motion.

Exceptions: essential motion may remain when it communicates information that cannot be conveyed another way; provide an equivalent cue and evaluate it against the applicable accessibility criterion.

Verify: test normal and reduced-motion settings, interruption, loading, panel changes, and reduced-frame/performance conditions.

## T-08 Accessibility and interaction resilience

Graph links: `CBM_RELATIONS_V1[CONSTRAINS=T-02; CONSTRAINS=T-03; CONSTRAINS=T-04; CONSTRAINS=T-05; CONSTRAINS=T-06; CONSTRAINS=T-07]`. Keyboard, screen reader, zoom, contrast, and reduced-motion checks remain prose verification criteria.

Principle: accessibility is part of the component and layout contract, not a final visual audit.

Guidance: use semantic controls and headings, visible keyboard focus, meaningful names, announced status changes where needed, sufficient target size, and logical reading/focus order. Support zoom, text resizing, high contrast or forced colors, and reduced motion. Preserve entered content when a transient error occurs where feasible.

Exceptions: platform-specific patterns may differ, but should retain equivalent functionality and a clear accessible name and state.

Verify: keyboard-only pass, semantic/assistive-technology spot check, zoom/reflow pass, rendered contrast checks, and state announcements for asynchronous work.

## T-09 HarnessKit assistant-workspace test mapping

Graph links: `CBM_RELATIONS_V1[APPLIES_TO=T-02; APPLIES_TO=T-03; APPLIES_TO=T-04; APPLIES_TO=T-05; APPLIES_TO=T-06; APPLIES_TO=T-07; APPLIES_TO=T-08]`. The link to `h_ui_visual_ideation_e2e_20261002_a1` is stored in that horizon as a typed citation, not as an intra-catalog edge.

Scenario: the existing CBM horizon `h_ui_visual_ideation_e2e_20261002_a1` contains a synthetic conversation-workspace fixture. Its six parts describe the experience, components, visual tokens, states, responsive/accessibility behavior, and explicit unknowns. This mapping connects that scenario to the practices above for graph discovery and citation tests.

Fixture values currently present in that horizon: left rail 256px, central conversation 720px, context drawer 320px, 24px gaps, 56px header, and 96px composer; example viewports are 1440x900 and 390x844. These are test inputs only. They are neither validated against HarnessKit source nor approved product requirements.

Open decisions: exact colors, typography, panel collapse behavior, animation choices, and product approval remain open until explicitly decided in the UI horizon.

## T-10 Source references and evidence boundary

Sources used for normative accessibility details:

- [W3C, WCAG 2.2, Success Criterion 1.4.3, Contrast (Minimum)](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum)
- [W3C, Media Queries Level 5, `prefers-reduced-motion`](https://www.w3.org/TR/mediaqueries-5/#prefers-reduced-motion)
- [W3C, WCAG 2.2 Technique C39, reduced motion](https://www.w3.org/WAI/WCAG22/Techniques/css/C39)

The remaining guidance is a curated starting model for this test, not a claim that one visual style is universally correct. Bind it to HarnessKit only after the user accepts the relevant decisions.
