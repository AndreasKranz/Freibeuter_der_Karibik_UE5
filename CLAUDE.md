# CLAUDE.md — Freibeuter der Karibik (UE5 Remake)

> This file is the primary context Claude Code loads at the start of **every** session.
> It is written to be self-sufficient: assume zero prior knowledge beyond what is here.
> The full design dossier lives at **`FREIBE_1.MD`** in the project root — read it for
> mechanical detail, economy numbers, asset sources, and reverse-engineering notes.

---

## 0. READ FIRST — the two rules that override everything

1. **We build in phases. Do not jump ahead.** Phase 0 is complete (see §3.1 for what shipped).
   Phase 1 is next but **not yet authorized to start** — check in with the developer before
   beginning any Phase 1 work.
   "Prove the current phase, then move on" is the whole philosophy of this project.
2. **C++ is the default for essentially all logic. Blueprints are for dressing and layout only.**
   See §2.1. This is non-negotiable and keeps the codebase reviewable across an MCP-driven workflow.
   
   
### 0.1 Autonomous session rules (apply when developer is not present)

These activate during long sessions without the developer present. They supplement the two rules
above — they do not replace them.

- **Branch:** work exclusively on `agentic`. Never commit directly to `main`.
- **Commit granularly.** Commit after every logical unit of work (struct definition, system complete,
  etc.) with a descriptive message. These commits are the audit trail for later review.
- **Push is permitted and encouraged.** `git push origin agentic` after each commit keeps
  work backed up to GitHub during long sessions. Never push to `main`.
- **File scope:** only modify files inside `Source/` and `Config/DataTables/`. Do not touch anything
  else without explicit prior approval.
- **No new dependencies.** Do not add plugins, third-party libraries, or new UE modules. Work within
  the existing project structure.
- **Open unknowns = stop and mark, don't guess.** If a decision point requires a value or design
  choice listed in §6, leave a `// TODO(dev): [clear question]` comment and continue around it.
  Do not invent values that belong in DataTables — they live there precisely so they can be tuned
  without recompiling.
- **Phase discipline.** Phase 1 scope only. Do not begin Phase 2 systems (siege escalation,
  month-boundary events, co-op layer) even if they appear to be a natural next step from current work.

---

## 1. Project overview

A modern PC remake, in **Unreal Engine 5.8**, of *Freibeuter der Karibik* ("Handel, Häfen und Halunken") —
a discontinued 2008 German electronic board game for the Yvio console, designed by Alexander Pfister
(later of *Great Western Trail* / *Mombasa*). Players are trading captains in the Caribbean: load a good,
sail it to a port that doesn't produce it, get paid net profit on arrival, and spend the money on ship
upgrades — **the upgrades themselves are the victory points**. Wrapped around that competitive trade race
is a **co-operative survival timer**: pirates progressively besiege the ports, and if too many fall at once,
*everyone loses*. The signature mechanic is a **continuous day-based scheduler** (no rounds/turns — see §2.3).

Presentation is **3D ships sailing a stylized 3D-map**, there are islands a sea and fog, the edges of just end, water falls into the fog/sky, not an abstracted board-game UI. **PC only** — no
console/mobile considerations.

For anything about mechanics, economy, combat, the siege system, or original-game numbers not covered here:
**`FREIBE_1.MD` is the source of truth.**

---

## 2. Architecture principles

### 2.1 C++ vs Blueprint split (firm rule)

**C++ (written via Claude Code) is the default for essentially everything:**
the scheduler, all core data structures (Port / Trader / Ship / Encounter etc.), economy logic, combat
resolution, siege & reputation logic, save/load, DataTable row-struct definitions, game state, and all
future networking/replication code.

**Blueprints (via MCP) are reserved narrowly for:**
- Level dressing and actor placement in the 3D world.
- UMG widget **layout** — visual composition of trade/port screens. Widget logic stays *thin*: wiring to
  C++ calls only, never real logic.
- Simple one-off event wiring where a visual graph is genuinely clearer than code (e.g. "on trigger, play sound").

**Blueprints must never hold game logic, economic rules, or anything requiring code-review scrutiny.**
If you find yourself putting a rule in a Blueprint, it belongs in C++ instead.

### 2.2 Data-driven design (hard requirement)

All tunable game data — ports, goods, traders, prices, pirate stats, upgrade costs, distances, VP values,
month events — **must live in UE5 DataTables from day one**, never hardcoded in C++. The dossier flags most
exact original numbers as unknown/reconstructed (see §6), so the system must support retuning **without
recompiling**. C++ defines the row structs (`F...Row : public FTableRowBase`) and reads from DataTables;
C++ never bakes in the values.

### 2.3 The scheduler — the most important concept in the project

There are **no rounds or turns** in the traditional sense. Every pending event has a trigger day. The core
is an **event-driven min-heap / priority queue keyed on that day value**:

> Always pop and resolve whichever event has the **soonest** day, resolve it, then push any new events it
> generates back onto the heap.

This means the player whose clock has advanced least acts next — short-hop traders act more often, a captain
on a long lucrative run waits. Route choice is the interesting decision *because* of this.

**Implementation constraints:**
- **Event-driven, explicitly NOT tick/frame-based.** The world advances by *day-values*, not by frame time.
  Do not drive the sim from `Tick()`.
- **Encounter handling = "Option A" (locked-in):** when a player commits to a voyage (loads cargo, picks
  destination), immediately roll whether a pirate encounter occurs. If yes, push an **additional "encounter"
  event** onto the heap at a day *partway through* the voyage — separate from and prior to the "arrival" event.
  When the encounter resolves (player picks Flee / Board / Cannons), its outcome **modifies the already-scheduled
  arrival day** (loss = delay) before the arrival event later fires.
  **Do not resolve encounters lazily at arrival time** — they must be their own distinct scheduled moment, so
  we can show the player a legible win-probability *before* they commit to the fight (see §5.1).
- **Month-boundary global events** (siege spread, silver fleet, famine, storms — dossier §6–7) are pushed onto
  the **same heap** as world-level events, not player-specific ones.

---

## 3. Current phase & roadmap

**► Phase 0 is complete** (see §3.1). **Phase 1 has not started** — do not build Phase 1 (or
later) systems without checking in with the developer first.

| Phase | Scope | Goal |
|---|---|---|
| **0 — Core loop prototype** ✅ *complete* | Single-player. 2–3 ports, one trader, no siege. Placeholder visuals only. | Prove the **day-based scheduler** and the trade loop *feel right* before investing further. |
| **1 — Full economic sim** ◄ *next — check in before starting* | Single-player. All 8 ports, all 5 traders, tier-1/tier-2 upgrade unlocks, combat resolution, reputation/titles, haggling with price decay. | Complete, tunable single-player economy. |
| **2 — Siege / co-op layer** | Pirate siege escalation, month-boundary global events, the "5 besieged ports = all players lose" fail state. | The co-op survival timer around the competitive race. |
| **3 — Local multiplayer** | Same-machine / LAN multiplayer; network the scheduler and shared world state. Sequenced *after* single-player is solid — highest-risk phase to build on unproven foundations. | Multiplayer on a proven core. |
| **4 — Presentation polish** | 3D ship movement/animation, VFX, audio direction, UI polish. | The atmosphere that reviewers said carried the original. |

**Rule of thumb:** the scheduler and data-driven architecture must be right early, because everything —
especially multiplayer replication in Phase 3 — is built on them.

### 3.1 Phase 0 — what shipped

Delivered across two commits (`07ada85`, `3d09ca9`):

- **Scheduler core proven.** `UFreibeuterSchedulerSubsystem` (a `UWorldSubsystem`, never
  ticked) drives voyages through a `TArray`-backed min-heap keyed on `TriggerDay`. Option-A
  encounters (§2.3) are a separate scheduled event that mutates the already-queued arrival
  day on a loss, rather than resolving lazily at arrival. Source: `Source/FdK_UE5/Scheduler/`.
- **Data-driven from the start.** Ports, distances, traders, and goods are read from
  DataTable assets in `Content/Freibeuter/Data`, not hardcoded — see the row structs in
  `FreibeuterDataTableRows.h` (`FFreibeuterPortRow`, `FFreibeuterPortDistanceRow`,
  `FFreibeuterTraderRow`, `FFreibeuterGoodRow`).
- **Scope kept to Phase 0's slice.** All 8 dossier ports are defined in the port table, but
  the playable slice is narrowed to **San Juan / Puerto Plata / Curacao** plus **one trader**
  (Puerto Plata, Cannons only) — matching "2-3 ports, one trader" above. The other 4
  traders/upgrade types are explicitly deferred to Phase 1.
- **A real trade loop, not just the scheduler demo.** Cargo load is validated against what
  the port produces; arrival pays net profit only (distance × scarcity multipliers, §4);
  tier-1/tier-2 Cannon purchases enforce the tier-2-locked-until-tier-1-sold rule.
- **Playable via console commands** (no UI yet — placeholder-visuals scope):
  `Freibeuter.NewGame`, `Freibeuter.SetSail`, `Freibeuter.Advance`, `Freibeuter.BuyCannon`,
  `Freibeuter.Status` (plus the earlier scripted `Freibeuter.RunSchedulerDemo` proof).
  Verified live end-to-end across two routes/goods in PIE.
- **Not yet built (deliberately out of Phase 0 scope):** all 8 ports playable, the other 4
  traders, tier-2 unlocks generally, combat resolution, reputation/titles, haggling/price
  decay, siege, any real UI/3D presentation.

---

## 4. Data model reference

Adapted from the dossier's "Minimum viable data model." Treat as the persistent shape of the core structs;
concrete numbers live in DataTables, not here.

```
Good        = Wheat | Wood | Tobacco | Rum        // ascending value & pirate risk
UpgradeType = Cannon | Chest | Sail | Sailor | Residence

Port {
  id, name, position,
  produces[2]: Good, accepts[2]: Good,
  trader?: Trader,
  besieged: bool, occupierStrength: int,
  lastDeliveryDay[Good]                            // drives the scarcity/starvation bonus
}

Trader {
  type: UpgradeType,
  tier1Stock: int, tier2Stock: int,                // tier2 LOCKED until tier1Stock == 0
  currentOffer: { tier, price },
  daysUnsold: int                                  // price decays with this (the "hidden auction")
}

Ship {
  player, gold, cargo: { good, qty },
  cannons, chests, sails, sailors, residences,     // each as { tier1Count, tier2Count }
  reputation, title,
  currentPort, destinationPort, arrivalDay
}

VP(ship) = sum over upgrades:
             Residence ? (t1*2 + t2*4) : (t1*1 + t2*2)

Encounter { pirateCannons, pirateSailors, pirateSails, cargoTier }

Scheduler: min-heap on arrivalDay (and all pending event days); pop lowest, resolve, push back.
Month boundary = global event tick (siege spread, silver fleet, famine, storm).
```

**Economy notes to preserve (dossier §2–4):** no purchase price is paid for cargo — the player receives
**net profit only** on arrival; payout scales with **distance** and with **scarcity** (a long-neglected port
pays more and grants reputation). Starting capital **1500 Dukaten**. Wheat/Wood are never attacked; Tobacco/Rum
attract pirates (Rum pirates are stronger). Titles (Major → Colonel → Admiral → Baron) give **cash bonuses, no VP**.

---

## 5. Design requirements carried from the original's flaws

These are **requirements**, not nice-to-haves. The original's specific failures are named causes of lost
player goodwill; we do not repeat them.

1. **Legible combat odds.** Combat must show the **actual win-probability before** the player commits to
   Flee / Board / Cannons. The original's "you can lose while stronger" opacity is precisely what the
   Option-A scheduled-encounter design (§2.3) exists to enable — surface the percentage.
2. **Liberation must pay.** Defeating a port's occupier / capturing pirates must give a reward **beyond just
   avoiding the loss condition**. In the original, liberation was a pure tax on competitive players. Fix it.
3. **Visible siege schedule.** The siege-escalation timeline must be **legible to players**. (The original
   shipped an escalation bug that went unnoticed *because the schedule was hidden.*)
4. **Visible trader tier-1 stock.** Each trader's remaining tier-1 stock must be shown on the map/UI. This
   turns the tier-1→tier-2 unlock into a **negotiation surface** between players instead of a hidden number.
5. **Rebalance residences.** Residences (2/4 VP, no gameplay benefit, cheaper the longer they sit) are a
   dominant endgame play in the original — a single tier-2 residence is ~40% of a standard game. **Do not port
   the original numbers uncritically**; add a carrying cost or rebalance.

---

## 6. Open / unknown items

Phase 0 seeded placeholder/invented values into DataTables for the 3-port slice (see §3.1) —
that satisfies §2.2 ("never hardcoded"), but these numbers are not yet the reverse-engineered
dossier values. Everything below remains open before Phase 1 locks the full economy.

Numbers to **decide or invent** before/during Phase 1. All are recoverable from a physical copy (manual +
board scan + logged playthroughs) — see dossier §8–10 — but until then they are tuning targets, and because
of §2.2 they live in DataTables and can be changed freely.

1. **Goods-production matrix** — which 2 of 4 goods each of the 8 ports produces vs. accepts.
2. **Distance table** between ports (in days) + the sail-speed modifier formula.
3. **Price tables** per upgrade tier + the decay rate of unsold equipment.
4. **Trader stock counts** — tier-1 and tier-2 item counts per trader. *The single most important unknown;
   it sets the pace of the whole game.*
5. **Combat resolution formula** (weighted-probability in the original, widely felt unfair — see §5.1).
6. **Full side-quest list** (confirmed so far: kidnapped merchant's daughter, treasure-map/island hunt,
   deliver a letter, escort a prisoner, hunt the Spanish silver fleet).
7. **Month-event table** (famine, storm, silver-fleet appearance, siege announcements).

**Also undecided (flagged separately from the dossier):**
- **Presentation art direction** — the dossier does not cover it. Style, palette, ship/port fidelity, UI
  aesthetic all open. Decide before Phase 4; a rough placeholder direction may help Phase 0 framing.
- **Siege thresholds** — sources disagree on ports-besieged-per-month (2 vs 3) and the loss threshold
  (4 vs 5 simultaneous). Treat as DataTable-driven tuning values.

---

## 7. Dev environment reference

The toolchain is already set up and working — **don't waste time re-diagnosing it.**

- **Engine/project:** UE **5.8**, C++ project with a `Source/` folder, started from the **Top-Down template**.
- **Editors/build:** JetBrains **Rider** is the preferred code editor (RiderLink installed, MSBuild/dotnet
  paths configured). **Visual Studio 2022** installed for compilation (Desktop C++, Game Dev C++, .NET Desktop
  workloads). **.NET 10** runtime required and installed (UE 5.8 UnrealBuildTool requirement). First build
  succeeded in Rider.
- **Source control:** Git + Git LFS configured; `.gitignore`/`.gitattributes` set for UE binary assets;
  GitHub remote connected via `gh` CLI auth.
- **MCP:** Unreal MCP server plugin + an "alltoolset" plugin installed, auto-start, connected to both Claude
  Code (terminal) and Claude Desktop, pointed at this project directory. A local
  `unreal-engine-skills-for-claude-plugin` is installed via Claude Code and verified.
- **Machine:** AMD Ryzen 5 7600X3D · 32 GB DDR5-6000 · RX 7900 XT 20 GB (slight OC) · Win 11 · QWERTZ.
  **Note:** on full rebuilds, **UBA (Unreal Build Accelerator) may throttle/retry compile workers under
  memory pressure** with 32 GB — this is expected, not a bug.

---

## 8. Coding conventions

These are **idiomatic UE5 / Epic conventions** and are the locked style for all C++ Claude Code writes.

> **Developer context — the developer is new to C++** (background is Java and Python, this is their first C++
> project). Write clear, conventional, un-clever C++ and **briefly explain C++-specific concepts when they
> first come up** — pointers vs references, ownership/lifetimes, header/cpp separation, why `UPROPERTY` exists
> (UE's garbage collector needs it to see object pointers), `TObjectPtr` vs raw pointers, `.h`/`.cpp` build
> flow, and forward declarations. A one- or two-line inline explanation is enough; don't lecture, but don't
> assume prior C++ knowledge either. Prefer readable, standard patterns over micro-optimizations.

**Naming (Epic prefixes):**
- `U` UObject-derived, `A` Actors, `F` structs / plain classes, `E` enums (prefer `enum class`), `I` interfaces,
  `T` templates. Booleans prefixed `b` (`bIsBesieged`). Types/functions/members in **PascalCase**; locals in
  PascalCase too (Epic style).
- DataTable row structs: `FPortRow`, `FTraderRow`, … each `: public FTableRowBase`.

**Structure:**
- One primary game module, **`Freibeuter`**; split additional runtime modules only when a real boundary
  appears. Editor-only code in a separate editor module.
- Generally one class per header/cpp pair. Thin headers, fat cpps: **forward-declare in headers, include in cpps**
  (IWYU). Keep `#include` lists minimal and explicit.

**UE idioms:**
- `TObjectPtr<>` for `UPROPERTY` object pointers; raw pointers only for non-owning transient locals.
- Expose data-driven fields with `UPROPERTY(EditAnywhere, BlueprintReadOnly)` so DataTables and thin BP wiring
  work; expose C++ entry points to thin BP widgets via `UFUNCTION(BlueprintCallable)`.
- Const-correctness throughout. Use `check()`/`ensure()` for invariants; `checkf`/`ensureMsgf` with context.
- Prefer `TArray`/`TMap`/`TSet` and Unreal containers over STL in gameplay code.

**Scheduler specifics:**
- The min-heap should be a first-class C++ system (e.g. a `UWorldSubsystem` or a dedicated game-state-owned
  object), event-driven, **never** advanced from `Tick`. Events are plain data pushed with a `TriggerDay`;
  keep event types extensible (player arrival, encounter, month-boundary global tick).

**Comments/docs:** document *why*, not *what*. Every non-obvious economic or scheduler rule gets a one-line
rationale pointing back to the relevant `FREIBE_1.MD` section where useful.

---

*End of CLAUDE.md. When in doubt about game mechanics, read `FREIBE_1.MD`. When in doubt about phase scope,
check in before building.*
