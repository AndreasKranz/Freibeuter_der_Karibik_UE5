# Freibeuter der Karibik (Yvio, 2008) — Reconstruction Dossier

Research compiled 21 July 2026 for a UE5 digital remake.

---

## 1. Identity & credits

| Field | Value |
|---|---|
| Title | Freibeuter der Karibik — "Handel, Häfen und Halunken" |
| English alt. name (BGG) | Swashbuckling Pirates |
| Designer | **Alexander Pfister** (his early work — later of Great Western Trail / Mombasa) |
| Illustrators | Conrad Krause, Tina Blüher, Thomas Gehlhoff, Jan-Hendrik Röhrs |
| Publisher | PublicSolution GmbH (Dresden), 2008. Licence: White Castle (AT) |
| Article no. / EAN | 98050 / 4260165980500 |
| Players / age / time | 1–4, 10+, 60–75 min |
| BGG | id 40579, rating 6.8, weight 2.13 |
| Status | Publisher insolvent March 2010; official site offline since 14 May 2010 |

**Copyright note:** the game is discontinued but not public domain. Rights presumably sit with the insolvency estate / Alexander Pfister / White Castle. A faithful clone with original art is legally exposed; a re-implementation with your own art and a changed title is the safer commercial path. Worth a direct mail to Pfister — designers of dead titles are often receptive, and a licence or blessing would be a real asset for a commercial release.

---

## 2. Core game loop (confirmed across four independent reviews)

Players are **trading captains in the Caribbean**. Buy nothing — load a good, sail it to a port that doesn't produce it, get paid net profit on arrival, spend the money on ship upgrades. **Upgrades ARE the victory points.**

### Turn structure — this is the distinctive part

**There are no rounds.** The console tracks a continuous timeline in *days*. Each voyage costs a number of days based on:

- distance between origin and destination port
- number of sails the player owns (more sails = fewer days)
- random events (lost fights slow you down)

The console always activates whichever player's clock has advanced least — so a player who takes short hops acts more often; a player on a long, lucrative run waits. This is a **priority-queue / event-scheduler**, not a round-robin. In UE5 terms: a min-heap keyed on `arrivalDay`, pop the lowest, resolve, push back.

The world clock is also divided into **months**. At each month boundary the console announces global events (see §6).

### Player decision sequence

1. **In port:** choose one of the two goods the port produces to load (cargo hold size = how many units).
2. **Choose destination port.**
3. **At sea:** possible random encounter → choose **Flee** (sails), **Board/melee** (sailors), or **Cannons** (ranged).
4. **On arrival:** cargo auto-sold, net profit paid immediately. Optional: buy upgrade if this port has a trader; haggle; accept a side quest from the harbour tavern.
5. Repeat.

---

## 3. Map & ports

**8 ports arranged around the console** in the centre of the board. Starting port for all ships: **San Juan**.

**5 ports have traders** (upgrade type is fixed per port):

| Port | Trader sells | Notes |
|---|---|---|
| Puerto Plata | **Cannons** | Trader named "Mabu" — catchphrase "Mabu mag dich" when he lowers the price |
| Kingstown | **Sails** | |
| Cartagena | **Sailors / crew** | |
| Prinzapolca | **Cargo hold (Lagerraum)** | |
| Camarco | **Residence (Residenz)** | Pure VP, no in-game benefit; worth **double** VP |

**The remaining 3 ports:** San Juan (start), Curaçao, **Spring Point**.

Full port list (8): San Juan, Cartagena, Puerto Plata, Kingstown, Prinzapolca, Camarco, Curaçao, Spring Point.

Each port **produces 2 of the 4 goods and buys the other 2.**

---

## 4. Goods & economy

Four goods, in ascending value order:

| Good | Value | Pirate risk |
|---|---|---|
| Weizen (wheat) | lowest | **Never attacked** |
| Holz (wood) | low | **Never attacked** |
| Tabak (tobacco) | high | Pirates always present |
| Rum | highest | Pirates always present **and noticeably stronger** |

Profit modifiers:

- **Distance:** the further the destination, the higher the payout.
- **Scarcity/starvation:** a port that hasn't received a given good for a long time pays substantially more, and grants **Ansehen** (reputation).
- No purchase price is paid for cargo — you receive net profit only, so individual payouts are modest.

**Starting capital: 1500 Dukaten.**

### Upgrade tiers & victory points

All five upgrade types exist in **two tiers**. Each trader holds a fixed stock of tier-1 items and a fixed stock of tier-2 items. **Tier 2 is locked until every tier-1 item at that trader has been bought.**

| Upgrade | Tier 1 VP | Tier 2 VP |
|---|---|---|
| Cannons | 1 | 2 |
| Chest / cargo hold | 1 | 2 |
| Sails | 1 | 2 |
| Sailors | 1 | 2 |
| **Residence** | **2** | **4** |

This is the game's second, quieter cooperation lever: no single player can unlock tier 2 alone within a reasonable time, so the table has to collectively drain a trader's tier-1 stock before anyone gets access to the strong equipment. It cuts both ways — buying a tier-1 item you don't need still advances everyone toward tier 2, and the player positioned nearest that trader when the last tier-1 sells gets first pick.

Note how this interacts with residences: at 2 and 4 VP they're worth double, provide no in-game benefit, and get cheaper the longer they sit unsold. A tier-2 residence alone is 40% of a standard 10-VP game.

### Reputation / titles
Successful pirate kills and deliveries to long-neglected ports raise reputation, producing ranks like **Major → Colonel → Admiral → Baron**. Titles give **no VP** — each promotion pays a **cash bonus** only.

### Upgrade pricing — a hidden auction
Equipment prices **decay over time while unsold** (described by a player as "indirectly auctioned"). You may **haggle**: reject the offer via a console button to get a lower one, but push too far and the trader withdraws the offer entirely. Example from a play report: a 2nd extra sail talked down from ~1200 to 700 Dukaten. Cannons quoted around 1400 Dukaten.

(See "Upgrade tiers & victory points" below — the "cannon with value 2" in one play report is a tier-2 cannon.)

---

## 5. Combat

Encounters occur at sea (against NPC pirates or, in multiplayer, against another player's ship — deliberate raiding is allowed and encouraged at 4 players).

Three options: **Flee** (sails), **Melee/board** (sailors), **Ranged** (cannons).

The console displays the pirate's **cannons / sailors / sails** stats. Early pirates are lopsided — strong in either sailors *or* cannons.

**Critically: outnumbering the opponent in the relevant stat does not guarantee success.** Multiple reviewers complain about losing three fights in a row while stronger. The original resolution is a weighted probability, not a deterministic comparison, and it was widely considered unfair from ~month 8 onward.

**Loss consequences:** you lose cargo (worse payout) and speed (longer voyage). Your ship is never sunk. Players *can* sink enemy ships. Captured pirate ships yield **no** extra profit — so hunting pirates is a public good, not a income source. This is a design flaw worth fixing in a remake.

Against NPC pirates: cannons are generally the better investment (many pirate ships are sailor-heavy). Against other players, sailors/boarding is the play.

---

## 6. The pirate siege system (the co-op layer)

- Pirates progressively **besiege ports**. One port is besieged from the start.
- Each month, **up to 2 more ports** become besieged (per the designer; one reviewer observed 3).
- The occupying pirate in a besieged port **grows stronger over time**.
- Besieged ports must be liberated by players sailing there and defeating the occupier.
- **If 5 ports are besieged simultaneously, ALL players lose.** (One forum report says 4 ports held for a full month in the pro variant — verify; likely a variant difference.)

This forces cooperation despite the competitive VP race. Standard practice among experienced groups: explicitly divide up which player liberates which port, and don't all buy the same upgrade type early.

**Known original bug:** in some copies the siege escalation never fired — testers played the hardest difficulty and never saw more than one besieged port, making the game trivial. A patch was promised and apparently never shipped before insolvency.

---

## 7. Difficulty modes & length

| Mode | Target VP | Notes |
|---|---|---|
| Einsteigerspiel (intro) | 5 | Console teaches the rules as you play |
| Standardspiel | 10 | |
| Profispiel (expert) | 10 | Much stronger enemies; abbreviated voice output (faster) |
| Extended | 15 or 20 | 20 is the hard cap — game ends there regardless |

Timeline: the **first ~6 months are easy** — build your economic base. **Months 7–18** are decisive. A full standard game runs roughly 18 months of in-game time.

---

## 8. Yvio hardware & the SD card — realistic assessment

**Verdict: there is no publicly available dump, decompilation, or file-format documentation for the Freibeuter SD card. Do not plan around recovering the original code.**

What is known:

- **CPU:** NXP **LPC2368** (ARM7TDMI-S core). Support chips: NXP 74LVC4245A transceiver, 74LVC10A NAND gate; a Cypress **CY8C21634** PSoC drives the LED matrix.
- Game software ships on a standard **SD card**; a plastic "Spielskin" overlay relabels the console's buttons per game.
- Piece detection uses **patented transponder technology** in up to 8 "Yvies" discs, plus **RFID** chips in cards (used by *Partytime*). Board traces are almost certainly multiplexed excitation coils; the discs likely contain resonant circuits at distinguishing frequencies.
- **Freibeuter uses a unique board type ("Typ 4")** — unlike most Yvio titles which share identical board electronics.
- Only known confirmed modification: **audio files on the SD card are replaceable** (per the neuby.de maintainer, in the context of translating games). This implies audio is stored in a recognisable container, but the game logic itself is not documented.

Practical routes if you want the original data anyway:

1. **Buy a used copy** (second-hand German marketplaces still list it, ~€20–60) and image the SD card yourself with `dd`. Then carbe the image: look for RIFF/WAV headers, ADPCM streams, and a lookup table of port/good/price constants. This is the only realistic path to authentic voice assets and exact numbers.
2. **wiki.neuby.de** (linked from neuby.de) is the only hobbyist reverse-engineering effort found — hardware-focused, largely stub pages. Its author is on GitHub as **ranseyer**; worth contacting.
3. **naviara-music.com/forum** — a registration-required Yvio community forum. Possible archive of files/knowledge; couldn't be inspected without an account.

Realistically: reconstruct the logic from the behavioural descriptions in this document plus your own playtesting, and treat the original numbers as a tuning target rather than a spec.

---

## 9. Asset sources (visual reference)

**BoardGameGeek gallery** — 24 images, the best reference set available. Open and download from these pages:

| Subject | URL |
|---|---|
| **Board scan ("Yivo Board")** ← most important | https://boardgamegeek.com/image/821389/yvio-freibeuter-der-karibik |
| **Player board / ship mat (scan)** | https://boardgamegeek.com/image/821381/yvio-freibeuter-der-karibik |
| Rule book front | https://boardgamegeek.com/image/821380/yvio-freibeuter-der-karibik |
| Equipment tiles | https://boardgamegeek.com/image/821386/yvio-freibeuter-der-karibik |
| Currency (dubloons) | https://boardgamegeek.com/image/821388/yvio-freibeuter-der-karibik |
| Set-up shot | https://boardgamegeek.com/image/882505/yvio-freibeuter-der-karibik |
| Gameplay | https://boardgamegeek.com/image/882507/yvio-freibeuter-der-karibik |
| Front cover | https://boardgamegeek.com/image/882504/yvio-freibeuter-der-karibik |
| Memory card | https://boardgamegeek.com/image/821385/yvio-freibeuter-der-karibik |
| Full gallery | https://boardgamegeek.com/boardgame/40579/yvio-freibeuter-der-karibik/images |

**Other images:**

- Board photo: https://www.angespielt.de/gallery200/12255/brett.jpg (plus detail1/2/3.jpg)
- Component photo: https://das-spielen.de/wordpress/wp-content/uploads/freibeuter.jpg
- Archive photo: https://www.brettspiele.digital/sites/default/files/styles/large/public/images/freibeuter_der_karibik_2.jpg
- Board-layout diagram (Typ 4): https://wiki.neuby.de/index.php/Datei:Aufteilung_FdK.jpg
- Video: gamecaptain.de Yvio special includes Freibeuter footage — https://www.gamecaptain.de/Alle/Artikel/4010/Yvio_Special.html

**Note:** BGG has **no rulebook file** uploaded. The German manual exists only in the physical box. The most reliable way to get exact numbers (voyage days, prices, VP values, pirate stat tables) is to buy a used copy.

**No files are downloaded here** — I can't fetch image binaries in this environment. Open the links above and save them yourself.

---

## 10. What's still missing

1. **The goods-production matrix per port** (which 2 of 4 each of the 8 ports produces / accepts).
2. **Distance table** between ports in days, and the sail-speed modifier formula.
3. **Price tables** for each upgrade tier and the decay rate of unsold equipment.
4. **Trader stock counts** — how many tier-1 and tier-2 items each of the 5 traders holds. This sets the pace of the whole game and is the single most important unknown number.
5. **Combat resolution formula.**
6. **Full side-quest list** (confirmed so far: kidnapped merchant's daughter, treasure map/island hunt, deliver a letter to a neighbouring island, escort a dangerous prisoner, hunt the Spanish silver fleet).
7. **Month-event table** (famine, storm devastating a settlement, silver fleet appearance, siege announcements).

All seven are recoverable from a physical copy: manual + board scan + a few logged playthroughs.

---

## 11. Design notes for the UE5 remake

Things the original got right — keep them:

- The **continuous time / travel-day scheduler** is the game's signature. It's unusual, it's readable, and it's what makes route choice interesting. Don't flatten it into turns.
- **Atmosphere carried the game.** Every reviewer says the mechanics are thin but the voice acting, trader personalities, ambient sound and title music made it memorable. Budget accordingly — this is where the value is.
- **Competitive race wrapped in a co-op survival timer.** Excellent tension: liberate a port for the common good, or take one more profitable run?
- **Haggling with price decay** is a cheap mechanic that generates real tension.

Things to fix:

- **Combat felt unfair.** Make odds legible — show the actual win percentage before committing. Losing three fights while stronger is what killed the original's goodwill.
- **Capturing pirates yields nothing.** Give liberation a reward beyond survival, or the co-op layer is pure tax.
- **Siege escalation was buggy and opaque.** Make the schedule visible.
- **Decisions are thin** ("which of two goods, which port"). Reviewers split exactly on this: family players loved it, strategy players found it one-dimensional. Adding depth is your main opportunity to improve on the original — but the co-op siege pressure is the right place to add it, not the trading.
- **Residences are a dominant endgame play** (double VP, cheap early, no downside). At 2/4 VP a single tier-2 residence is 40% of a standard game. Rebalance or give them a carrying cost.
- **The tier-unlock is invisible.** Players can't cooperate on something they can't see. Show each trader's remaining tier-1 stock on the map — this turns a hidden mechanic into the game's best negotiation surface.

Minimum viable data model:

```
Good      = Wheat | Wood | Tobacco | Rum        // ascending value & risk
UpgradeType = Cannon | Chest | Sail | Sailor | Residence

Port {
  id, name, position,
  produces[2]: Good, accepts[2]: Good,
  trader?: Trader,
  besieged: bool, occupierStrength: int,
  lastDeliveryDay[Good]                          // drives scarcity bonus
}

Trader {
  type: UpgradeType,
  tier1Stock: int, tier2Stock: int,              // tier2 locked until tier1Stock == 0
  currentOffer: { tier, price },
  daysUnsold: int                                 // price decays with this
}

Ship {
  player, gold, cargo: {good, qty},
  cannons, chests, sails, sailors, residences,    // each as {tier1Count, tier2Count}
  reputation, title,
  currentPort, destinationPort, arrivalDay
}

VP(ship) = sum over upgrades:
             Residence ? (t1*2 + t2*4) : (t1*1 + t2*2)

Encounter { pirateCannons, pirateSailors, pirateSails, cargoTier }

Scheduler: min-heap on Ship.arrivalDay; pop lowest, resolve, push back.
Month boundary = global event tick (siege spread, silver fleet, famine, storm).
```

---

## Sources

- [BoardGameGeek — Yvio: Freibeuter der Karibik](https://boardgamegeek.com/boardgame/40579/yvio-freibeuter-der-karibik)
- [Ludorium — detailed review by Kurt Schellenbauer (~50 plays; best single source on ports & economy)](http://ludorium.at/SPIELE/publicsolution/freibeuter_der_karibik_2.htm)
- [angespielt.de — review by Carsten Pinnow](http://www.angespielt.de/kritiken/yvio-freibeuter-der-karibik)
- [Das-SpielEn.de — review by Kathrin, incl. the siege bug and support saga](https://das-spielen.de/index.php/yvio-freibeuter-der-karibik/)
- [Blogspiele.de — review](https://www.blogspiele.de/2009/02/17/freibeuter-der-karibik-piraten-auf-der-yvio/)
- [brettspiele.digital — ludographic record (Institut für Ludologie)](https://www.brettspiele.digital/spiele/freibeuter-der-karibik-handel-haefen-und-halunken)
- [Neuby's Home / wiki.neuby.de — Yvio console teardown & hacking notes](https://www.neuby.de/yvio-console-und-hackmoglichkeiten/)
- [Mikrocontroller.net — thread on Yvio piece detection](https://www.mikrocontroller.net/topic/417964)
- [unknowns.de — strategy discussion thread](https://unknowns.de/forum/thread/2008-freibeuter-der-karibik/)
- [spielen.de — solo play results thread](https://www.spielen.de/forum/viewtopic.php?t=249970)
- [spielen.de — Kingstown board defect thread](https://www.spielen.de/forum/viewtopic.php?t=249312)
- [gamesweplay.de — notes that spielbox magazine Apr/May 2009 carries a long review + Yvio console feature](https://gamesweplay.de/freibeuterderkaribik.html)
- [TOMMI children's software prize — 2nd place, console category](https://tommi.kids/magazin/spiele/freibeuter-der-karibik-yvio/)
