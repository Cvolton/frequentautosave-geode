# Frequent Autosave
This mod makes your game save more often. For what this mod does and why it exists, see [about.md](about.md); this readme covers the technical functionality more in-depth.

To avoid lag spikes this mod has two contexts of saving.

The mod always creates a full save upon level entry. After that it does what we call a "partial save" and only updates data related to the level. A partial save is always saved upon receiving a new best.

Partial saves update this data specifically:
- GS_Value (player stats)
- GS_20 (collected keys)
- GS_12, GS_15 (active and queued quests)
And depending on the type of level they also update:
- The entirety of CCLocalLevels.dat for Editor levels
- For Main levels
- For Online levels
  - Saved levels
    - Relevant level entry in GLM_03
    - GS_7 (percentage used for calculating level orbs)
  - Daily levels
    - Relevant level entry in GLM_10
    - GS_14 (collected diamonds)
    - GS_16 (collected orbs)
    - GS_24 (collected diamonds)
  - Gauntlet levels
    - Relevant level entry in GLM_16
    - GS_18 (collected diamonds)
    - GS_23 (collected orbs)
This covers all aspects of the save file that change between level attempts without causing unnecessary lag and delay.

Full saves are triggered upon level completion (with a slight delay so there isn't a lag spike during the animation or upon level exit, depending on which comes first). These cause a minor slightly noticeable lag spike (about 200 ms on my save file with about 20,000 saved levels), so they are moved to moments where the player is unlikely to notice.

The following events can also trigger full game saves mid-level (upon death):
- Unlocking an achievement
- Claiming a quest
- Opening a chest
While most of these events cannot naturally happen in the vanilla game, they have been accounted for to maintain full compatibility with PauseLayer modifying mods.