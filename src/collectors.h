#ifndef COLLECTORS_H
#define COLLECTORS_H

#include "items.h"

// GW1's collectors: NPCs standing out in the field who take a fixed
// number of one monster trophy and hand back a specific item.
//
// This is a whole economy the demake was missing, and it is the reason
// trophies are worth carrying rather than vendoring on sight. A
// merchant pays you a flat few gold for a Charr Carving; a collector
// turns five of them into a weapon or an armour piece you cannot buy.
// That difference is what makes "where do I hunt, and for what" a
// question with an answer.
//
// Collectors are also how pre-Searing arms a character who has no gold:
// the trade costs nothing but the hunting.

typedef struct {
    const char *npcName;
    const char *material;   // trophy taken, by name (matches Item.name)
    int count;              // how many
    Item reward;            // what comes back
    const char *flavour;    // one line, shown in the trade window
} CollectorOffer;

// The offer this NPC makes, or NULL if they aren't a collector.
const CollectorOffer *Collectors_OfferFor(const char *npcName);

#endif
