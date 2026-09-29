/* Public grouping and layout model. It contains no host-widget ABI. */
#include "global-midi-learn-groups.h"

namespace {

constexpr MpclearnGlobalLearnGroup kGroups[MPCLEARN_GLOBAL_LEARN_GROUP_COUNT] = {
    {"PADS", 0u, 16u},
    {"PAD BANKS", 16u, 8u},
    {"Q-LINKS", 24u, 16u},
    {"TRANSPORT & NAVIGATION", 40u, 13u},
    {"GLOBAL CONTROLS", 53u, 8u},
    {"SEQUENCE & TRACK", 61u, 8u},
    {"PAD PERFORMANCE", 69u, 5u},
    {"LOOPER", 74u, 9u},
    {"STEP SEQUENCER", 83u, 2u},
    {"EDITOR TABS", 85u, 5u},
    {"MODES", 90u, 14u},
    {"PANELS", 104u, 6u},
    {"BROWSER", 110u, 7u},
    {"ADDITIONAL TARGETS", 117u, 3u},
};

} // namespace

extern "C" const MpclearnGlobalLearnGroup *mpclearn_global_learn_group(
    uint32_t group) {
    return group < MPCLEARN_GLOBAL_LEARN_GROUP_COUNT ? &kGroups[group]
                                                     : nullptr;
}

extern "C" int mpclearn_global_learn_target_location(
    uint32_t target, uint32_t *group, uint32_t *index_in_group) {
    if (target >= MPCLEARN_GLOBAL_LEARN_TARGET_COUNT || !group ||
        !index_in_group) return 0;
    for (uint32_t candidate = 0;
         candidate < MPCLEARN_GLOBAL_LEARN_GROUP_COUNT; ++candidate) {
        const MpclearnGlobalLearnGroup &entry = kGroups[candidate];
        if (target >= entry.first_target &&
            target - entry.first_target < entry.target_count) {
            *group = candidate;
            *index_in_group = target - entry.first_target;
            return 1;
        }
    }
    return 0;
}

extern "C" int mpclearn_global_learn_layout(
    MpcUiRect bounds, MpclearnGlobalLearnLayout *layout) {
    if (!layout || bounds.x < 0 || bounds.y < 0 || bounds.width < 1280 ||
        bounds.height < 620) return 0;
    const int body_width = bounds.width - 64;
    const int detail_width = body_width - 700;
    *layout = {
        {32, 20, 248, 48},
        {296, 20, 72, 48},
        {380, 20, 252, 48},
        {644, 20, 72, 48},
        {732, 20, 120, 48},
        {864, 20, 120, 48},
        {996, 20, 120, 48},
        {1128, 20, 120, 48},
        {32, 84, 248, bounds.height - 212},
        {296, 84, 420, bounds.height - 212},
        {732, 84, detail_width, bounds.height - 212},
        {732, 200, 248, 48},
        {992, 200, 256, 48},
        {732, 260, 56, 48},
        {796, 260, 180, 48},
        {984, 260, 56, 48},
        {1048, 260, 200, 48},
        {732, 320, 56, 48},
        {796, 320, 180, 48},
        {984, 320, 56, 48},
        {1048, 320, 200, 48},
        {732, 380, 516, 80},
        {1008, 474, 240, 56},
        {732, bounds.height - 112, 240, 64},
        {bounds.width - 252, bounds.height - 112, 220, 64},
    };
    return 1;
}
