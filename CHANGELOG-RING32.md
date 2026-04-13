# CHANGELOG - Hidering Ring Size 32

## Version 0.18.1.0-ring32 (19 Janvier 2026)

### 🎯 Objectif
Augmentation du ring size minimum de 15 à 32 pour renforcer la confidentialité des transactions.

### 📝 Modifications du code source

#### 1. `src/cryptonote_config.h`
```cpp
// AVANT
#define HF_VERSION_MIN_MIXIN_15       16

// APRÈS
#define HF_VERSION_MIN_MIXIN_31       16

// AVANT
const size_t min_mixin = hf_version >= HF_VERSION_MIN_MIXIN_15 ? 15 : ...

// APRÈS
const size_t min_mixin = hf_version >= HF_VERSION_MIN_MIXIN_31 ? 31 : ...

// AVANT
// Caveat: at HF_VERSION_MIN_MIXIN_15, temporarily allow ring sizes

// APRÈS
// Caveat: at HF_VERSION_MIN_MIXIN_31, temporarily allow ring sizes

// AVANT
if (min_actual_mixin < min_mixin && !(hf_version == HF_VERSION_MIN_MIXIN_15 && min_actual_mixin == 10))

// APRÈS
if (min_actual_mixin < min_mixin && !(hf_version == HF_VERSION_MIN_MIXIN_31 && min_actual_mixin == 31))

// AVANT
} else if ((hf_version > HF_VERSION_MIN_MIXIN_15 && min_actual_mixin > 15)

// APRÈS
} else if ((hf_version > HF_VERSION_MIN_MIXIN_31 && min_actual_mixin > 31)

// AVANT
|| (hf_version == HF_VERSION_MIN_MIXIN_15 && min_actual_mixin != 15 && min_actual_mixin != 10)

// APRÈS
|| (hf_version == HF_VERSION_MIN_MIXIN_31 && min_actual_mixin != 31) // grace period removed for Hidering

// AVANT
if (use_fork_rules(HF_VERSION_MIN_MIXIN_15, 0))

// APRÈS
if (use_fork_rules(HF_VERSION_MIN_MIXIN_31, 0))

