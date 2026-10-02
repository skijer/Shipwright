# Randomizer

A mod can put its items in the randomizer pool, add settings to the seed, and change what the logic considers
reachable, without touching a single file of the randomizer.

## Your item in the pool

Fill the `randomizer` block of the item definition and it gets a `RandomizerGet` of its own, a row in the item
table and `poolCount` copies in the pool:

```c
static const SOHCustomItemRandomizer sLanternRando = {
    sizeof(SOHCustomItemRandomizer),
    SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT,
    SOH_CUSTOM_ITEM_TYPE_ITEM,
    0,
    1,
    "yourname.lantern",
    "yourname.lantern",
    "",
};

Z64Items_SetLogic(&lantern, &sLanternRando);
```

| Field | Meaning |
|---|---|
| `flags` | `SOH_CUSTOM_ITEM_RANDO_ADVANCEMENT` (the logic may need it), `SOH_CUSTOM_ITEM_RANDO_PROGRESSIVE`. |
| `type` | `SOH_CUSTOM_ITEM_TYPE_ITEM`, `_EQUIP`, `_SONG`, `_SHOP`. |
| `price` | Shop price, if it can appear in a shop. |
| `poolCount` | Copies in the pool. More than one puts the same item (and slot) in the pool several times. |
| `logicKey`, `hintKey` | Keys the logic and hints know it by; usually the item key. |
| `progressionGroup` | Items that upgrade together. |
| `poolCountOption` | A mod rando option (below) whose selected index adds to `poolCount`, so a setting decides how many copies are shuffled. |

A progressive item sets `SOH_CUSTOM_ITEM_RANDO_PROGRESSIVE` and an `onReceive`: it runs for every copy found, so
each copy can unlock a little more.

The item's id is `sApi->GetRandoItem(key)`. It depends on the installed mods: ask for it, never store it.

## Settings of your own

```c
static const char* const sTreatments[] = { "Medallions", "One pickup", "Progressive" };
static const SOHModRandoOption sWandOption = {
    sizeof(SOHModRandoOption), "yourname.wand_treatment", "Elemental Wand", "How the wand is shuffled.",
    sTreatments, 3, 0, SOH_MOD_RANDO_WIDGET_COMBOBOX,
};

sApi->RegisterRandoOption(&sWandOption);
uint8_t treatment = sApi->GetRandoOption("yourname.wand_treatment");
```

The option appears under *Randomizer → Mods*, joins the seed like any vanilla setting (the spoiler log records
it by name), and is saved by key. `GetRandoOption` answers with the seed's value in a randomizer file and the menu's
value otherwise. The `label` must be unique across every mod and vanilla setting.

## Changing the logic

### Capabilities: "my item can also do this"

A capability is one of the logic's reusable helpers (`CanCutShrubs`, `BlastOrSmash`, `CanBonkTrees`…). Granting one
reaches every check, entrance and event that goes through the helper, **nested helpers included**: granting
`BlastOrSmash` also opens bomb grottos, because `CanOpenBombGrotto` asks `BlastOrSmash`.

```c
static bool HasShovel(void* userData) {
    return sApi->LogicHasItem(sApi->GetRandoItem("yourname.shovel"));
}

sApi->GrantLogicCapability("yourname.shovel", "CanCutShrubs", HasShovel, NULL);
```

The full list is `RANDO_LOGIC_CAPABILITIES` in `soh/soh/ModApi/RandoLogic/RandoLogic.h`. Grants only ever add
access; to take access away, use a rule.

### Rules: a set of checks, and what to do with them

A rule selects checks, entrances and events from the world graph and changes their answer. The fields of one
selector narrow each other, selectors add up, and the exclude list subtracts.

| Selector field | Picks |
|---|---|
| `capability` | Conditions that spell that helper out, e.g. `"CanCutShrubs"` (does not follow nesting). |
| `conditionText` | Any substring of the condition, e.g. `"IsAdult"`, `"RG_SILVER_GAUNTLETS"`. |
| `checkType` | `RCTYPE_GRASS`, `RCTYPE_POT`, `RCTYPE_BEEHIVE`… |
| `region` | `RR_*` (the parent region, for an entrance). |
| `scene` | `SCENE_*`. |
| `entranceTo` | `RR_*` the entrance leads to, before entrance shuffle. |
| `logicEvent` | `LOGIC_*`. |
| `checks` / `checkCount` | An explicit `RC_*` list. |

`SOH_LOGIC_ANY` (-1) and `NULL` mean "any". `targets` restricts to `SOH_LOGIC_TARGET_LOCATIONS`, `_ENTRANCES`,
`_EVENTS`; `SOH_LOGIC_TARGET_ALL` is all three.

| Effect | Against the vanilla answer |
|---|---|
| `SOH_LOGIC_ALLOW` | vanilla OR yours |
| `SOH_LOGIC_REQUIRE` | vanilla AND yours |
| `SOH_LOGIC_DENY` | always false |
| `SOH_LOGIC_REPLACE` | yours only |

Every boulder that needs the Silver Gauntlets, except those in the Spirit Temple:

```c
static const SOHLogicSelector sBoulders[] = {
    { sizeof(SOHLogicSelector), SOH_LOGIC_TARGET_ALL, NULL, "RG_SILVER_GAUNTLETS",
      SOH_LOGIC_ANY, SOH_LOGIC_ANY, SOH_LOGIC_ANY, SOH_LOGIC_ANY, SOH_LOGIC_ANY, NULL, 0 },
};

static const SOHLogicSelector sNotSpirit[] = {
    { sizeof(SOHLogicSelector), SOH_LOGIC_TARGET_ALL, NULL, NULL,
      SOH_LOGIC_ANY, SOH_LOGIC_ANY, SCENE_SPIRIT_TEMPLE, SOH_LOGIC_ANY, SOH_LOGIC_ANY, NULL, 0 },
};

static const SOHLogicRule sRule = {
    sizeof(SOHLogicRule), "yourname.mitts.boulders", SOH_LOGIC_ALLOW, HasMitts, NULL,
    sBoulders, 1, sNotSpirit, 1,
};

sApi->RegisterLogicRule(&sRule);
```

Several rules on one check apply in registration order. Registering the same name again replaces the rule;
`RemoveLogicOwner(name)` drops it.

### Reading the logic inside a condition

A condition runs inside the fill, against a **simulated** inventory, not the live save. Read it only through
`LogicCanUseItem`, `LogicHasItem`, `LogicIsChild`, `LogicIsAdult`, `LogicIsAtDay`, `LogicIsAtNight` and
`LogicHasCapability`. Reading `gSaveContext` there gives the wrong answer and a different seed every time.

A custom item that replaces a vanilla one (`randoItem`, see [Items](ITEMS.md#vanilla-items)) answers as that item,
so `LogicCanUseItem(RG_MEGATON_HAMMER)` covers it.

## Flags of your own

`RegisterRandoFlag(key)`, `GetRandoFlag(key)` and `SetRandoFlag(key, state)` are named flags with no fixed limit,
saved in the file by key. The flags of a mod that is not installed right now are kept as they were, so removing a
mod for one session does not erase its progress.
