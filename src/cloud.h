#pragma once

inline const char *nm_cloud_profile_error(bool demo, bool child, bool parental, bool sideloaded) {
    if (demo) return "cloud access is disabled in demo mode";
    if (child) return "cloud access is disabled for child accounts";
    if (parental) return "cloud access is disabled by parental controls";
    if (sideloaded) return "cloud access requires a normal Kobo account profile";
    return nullptr;
}
