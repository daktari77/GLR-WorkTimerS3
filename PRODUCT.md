# Product

## Register

product

## Users

The device owner (single user), reaching the page from a phone or laptop on the same WiFi, via `worktimer.local` or the board IP shown on the LCD. Usage is occasional and brief: glance at what the timer is doing right now, or tune pomodoro durations before a focus block. They already see the physical T-Display S3 on the desk; the web page is a second screen for it, not a replacement.

## Product Purpose

A self-contained web dashboard served directly off the ESP32-S3 firmware. It does three jobs:
1. **Live status** — current mode (clock/stopwatch/pomodoro), phase, running state, elapsed time, board IP.
2. **Config** — edit pomodoro durations (work, break, long break, cycles-to-long), clamped and persisted to NVS.
3. **Session log** — recent entries from `/sessions.csv` (timestamp, kind, seconds) with simple totals.

Success: the owner opens the page, instantly reads what the timer is doing, optionally adjusts settings, and closes it. No login, no accounts, no cloud.

## Brand Personality

Echoes the device itself: a minimal black-background panel with big 7-segment-style numerals, the same restrained instrument look as the LCD. Three words: **precise, instrument, quiet**. The page should feel like the timer's own screen extended to the browser, not a separate app.

## Anti-references

- **Generic Bootstrap / default-framework SaaS look.** No card-grid templates, no stock component library aesthetic. Hand-built and intentional.
- **Grim router-admin config page.** No cramped grey table-of-fields embedded-device look.
- No gradients-for-decoration, glassmorphism, or motion for its own sake.

## Design Principles

- **Second screen for the device.** The page mirrors the LCD's visual language (black field, luminous 7-seg digits, dim labels); seeing both should feel like one product.
- **Status first, config second.** What the timer is doing now is the headline; settings are a deliberate secondary action.
- **Self-contained and cheap.** Every byte is served from flash and built as a C++ `String`. No external fonts, CDNs, or JS frameworks. Inline assets only; keep the payload small.
- **Honest instrument.** Show real state and real numbers, no faked metrics or decorative filler. Restraint over flourish.

## Accessibility & Inclusion

- Bilingual IT/EN with a user-toggle (default Italian); the toggle choice persists.
- Target WCAG AA contrast on the dark field (luminous digits and labels must clear 4.5:1 for text).
- Honor `prefers-reduced-motion`; any status pulse or transition degrades to none.
- Real form labels associated with inputs; numeric inputs keyboard-friendly with min/max; touch targets ≥44px for phone use.
