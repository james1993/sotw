#include "collectors.h"
#include <stddef.h>
#include <string.h>

// One offer per collector, keyed by NPC name so the zone tables stay
// pure placement and the trade lives with the other trade data.
//
// Rewards are deliberately things the merchant does not stock: a
// collector should be a REASON to hunt something, not a slower way to
// buy what gold already buys. The counts follow pre-Searing's own
// shape - a handful of one trophy, not a grind.
static const CollectorOffer g_offers[] = {
    // Lakeside County: the first collector most characters ever meet,
    // and the one that pays for killing the thing you were killing
    // anyway.
    {
        .npcName = "Farmer Hamnet",
        .material = "Skale Fin",
        .count = 5,
        .reward = { .kind = ITEM_OFFHAND, .name = "Lakeside Buckler", .armor = 6, .count = 1 },
        .flavour = "Skale have been at my irrigation ditches again. Bring me "
                   "five fins and the shield off my wall is yours.",
    },
    // Green Hills: a collector for the grawl trophy, giving a reason to
    // fight the warbands rather than walk around them.
    {
        .npcName = "Sentry Wallin",
        .material = "Grawl Necklace",
        .count = 4,
        .reward = { .kind = ITEM_ARMOR, .name = "Studded Hauberk (AL 45)",
                    .armor = 45, .slot = EQUIP_CHEST, .count = 1 },
        .flavour = "Grawl come down off the hills every night. Four of their "
                   "necklaces and I'll fit you with a hauberk that stops one.",
    },
    // The Northlands: the Charr trophy, and the best thing a
    // pre-Searing character can be carrying when the Searing comes.
    {
        .npcName = "Quartermaster Ferrick",
        .material = "Charr Carving",
        .count = 5,
        .reward = { .kind = ITEM_WEAPON, .name = "Ascalon Warblade", .dmgMin = 17,
                    .dmgMax = 26, .range = 28.0f, .attackInterval = 1.33f, .count = 1 },
        .flavour = "Every carving is one of them that won't cross the wall "
                   "again. Bring me five and you'll carry Ascalon steel.",
    },
};
#define OFFER_COUNT (int)(sizeof(g_offers) / sizeof(g_offers[0]))

const CollectorOffer *Collectors_OfferFor(const char *npcName) {
    if (!npcName) return NULL;
    for (int i = 0; i < OFFER_COUNT; i++) {
        if (strcmp(g_offers[i].npcName, npcName) == 0) return &g_offers[i];
    }
    return NULL;
}
