# TinyTang

---

## Purpose

TinyTang is firmware for the Sipeed Tang Console 138K that runs TinyDesk's shell and desktop on the board's BL616 over USB. It loads FPGA cores and cartridges from the SD card — including cores that carry their own AE350 RISC-V software — and receives files and new firmware over that same USB-C connection

---

## Standards

- Use ISO-8601 timestamps with the local UTC offset. Timezone is America/Phoenix.

- When needing to look up reference material, use core-reference.md as an authoritative source when attempting to lookup anything that could be considered a standard, conformance, specification, etc. as opposed to looking it up online.

- If you are not able to find the information you need by consulting core-reference.md, look up the standard online you trust and make an educated determination if you would like to add it to your core-reference.md document following existing syntax and formatting for future reference.

- If a more recent or otherwise more valid source is discovered during your online research and it is not documented in core-reference.md, you are to notify the user.

- Diagnostic implementation limits must not be described as standard limits.

- Respect all standard licensing and attribution conventions.

- Use CERN's colibri library (CERN-OHL-W-2.0), via the colibri-sv SystemVerilog port, as a good-practice reference when designing or modifying FPGA logic. Take ideas and conventions only: do not copy, translate or closely port its modules. If adapting a module is ever justified, stop and get user approval first, then follow CERN-OHL-W-2.0 and record it in THIRD_PARTY.md.

---

## AI Agent Recovery Policy

Read this core.md file first. Treat core.md is the primary project-level source of directives subject to active higher-priority and current user instructions. Read core-log.md second as historical engineering/build/transcript evidence and the primary project-level source of context, The online GitHub repository is the backup source for archival information. Treat core.md as RESTRICTED, authoritative project core memory. Do not edit it automatically for any reason. Only edit core.md if the user explicitly asks for it.

---

## Build Environment

- The GitHub repository for this project is: https://github.com/aquasock/TinyTang.git

- The user's local GitHub repositories must always stay up to date with their online repositories.

- The tar.gz archives stored in the "archived_logs" folder are not to be referenced unless approval from the user is given first.

- Do not create branches if possible. Always work off of main unless otherwise instructed.

- FPGA
Gowin EDA 1.9.11.03
GowinSynthesis
SystemVerilog 2017
Target: GW5AST-LV138PG484AC1/I0
Device revision C

- BL616
Bouffalo SDK
T-Head RISC-V GCC toolchain
CMake/Make
C and C++ firmware sources

---

## Agent Behavior

- Keep project conversation limited to project enviroment.

- No need to be polite when speaking, Communicate as you would in a standard engineering enviroment.

- Assume all user commands are run from the local GitHub root.

- Split changes further only when risk, standards uncertainty, diagnostic isolation, or new evidence makes a smaller boundary materially safer.

- If new findings would materially change the approved plan, stop and obtain user approval for the revised plan before continuing.

- Generated binary regression artifacts and diagnostic tools the user is intended to run should normally be produced by deterministic scripts committed under "tools" and generated locally by the user or agent.

- Treat core-log.md as a ring buffer. Only 100 entries are ever allowed. If the log is full, tar.gz it with the current date and time into the "archived_logs" folder, clear the current core-log.md, and add an handoff entry in the empty core-log.md.

- The folder labeled ".ai" on the GitHub repository’s root is your core project folder and contains your core directives (core.md), running project memory (core-log.md), reference library (core-reference.md), and syntax guidelines (core-syntax.md).

- A core-syntax.md audit must be done any time changes are made to any of the files in your core project folder except core-syntax.md. 

- You have explicit permission to read and write access to the users local GitHub repository and may run build tools in the user's local enviroment.

- Never push anything onto any Github repos for any reason except for ones under the username "aquasock".

---

## Standard Workflow:

1. Review the available build and test results, identify the observed failures or required work, and prepare a proposed plan of action to the user. 

2. If the user approves the plan, make changes to the local GitHub source code that are aligned with your proposed plan.

3. Build the project and deploy the updated build to the Tang.

4. Inform the user that the Tang is ready for testing.

5. The user will test the changes and report their results.

6. Gather any necessary data from the Tang.

7. Update core-log.md, then commit and push the repository.

7. Repeat.
 
---

## Versioning

- This project uses Semantic Versioning for GitHub releases.

- The first release is version 0.1.0.

- Git tags and GitHub releases use a leading `v`, for example `v0.1.0`, while the human-readable project version is `0.1.0`.

- While the project remains pre-1.0, increment MINOR for each new hardware-proven development milestone that adds meaningful capability (0.1.0 -> 0.2.0 -> 0.3.0).

- Increment PATCH for fixes or release corrections that do not constitute a new milestone (0.1.0 -> 0.1.1).

- Reserve 1.0.0 for a future user-ready compatibility baseline explicitly approved by the user.

---

## Releasing

- Empty

---
