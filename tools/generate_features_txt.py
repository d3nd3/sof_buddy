#!/usr/bin/env python3
"""
Auto-generate FEATURES.txt from feature directories.

This script scans src/features/ for directories and generates a properly
formatted FEATURES.txt file with appropriate categorization.
"""

import os
import sys
from pathlib import Path

# Feature categories and their order
# Note: Features ending with _base are automatically excluded (infrastructure features)
CATEGORIES = {
    "Core Features (always enabled)": ["media_timers"],
    "Graphics Features": [
        "texture_mapping_min_mag",
        "scaled_con",
        "scaled_hud",
        "scaled_text",
        "scaled_menu",  # Will be commented by default
        "hd_textures",
        "vsync_toggle",
        "lighting_blend",
    ],
    "Game Features": [
        "teamicons_offset",
    ],
    "Bug fixes": [
        "new_system_bug",
        "console_protection",
        "cl_maxfps_singleplayer",
    ],
    "Input Features": [
        "raw_mouse",
    ],
}

# Features that should be commented out by default
DEFAULT_DISABLED = {
    "scaled_menu": "Experimental menu scaling (requires UI_MENU)",
}

def find_feature_directories(features_dir):
    """Find all feature directories in src/features/"""
    features = []
    if not os.path.exists(features_dir):
        return features
    
    for item in os.listdir(features_dir):
        item_path = os.path.join(features_dir, item)
        if os.path.isdir(item_path) and not item.startswith('.'):
            # Skip base/infrastructure features (ending with _base)
            if item.endswith('_base'):
                continue
            features.append(item)
    
    return sorted(features)

def read_existing_features_txt(features_txt_path):
    """Read existing FEATURES.txt to preserve comments and disabled features"""
    disabled_features = {}
    if os.path.exists(features_txt_path):
        with open(features_txt_path, 'r', encoding='utf-8') as f:
            for line in f:
                original_line = line
                line = line.strip()
                # Skip header comments
                if line.startswith('# SoF') or line.startswith('# Lines') or line.startswith('# Uncommented') or line.startswith('# Commented'):
                    continue
                # Check for commented features (// feature_name or # feature_name)
                if line.startswith('//'):
                    feature = line[2:].strip()
                    # Extract comment if present
                    if '#' in feature:
                        parts = feature.split('#', 1)
                        feature = parts[0].strip()
                        comment = parts[1].strip()
                    else:
                        comment = None
                    if feature:
                        disabled_features[feature] = comment
                elif line.startswith('#'):
                    # Handle # feature_name format (but not category headers like "# Graphics Features")
                    # Category headers have a space immediately after #, feature names may or may not
                    # Also skip lines that look like headers (contain "Features" or start with capital)
                    stripped = line[1:].strip()
                    if not stripped:
                        continue
                    # Skip category headers: they have capital letters or contain "Features"
                    if stripped[0].isupper() or 'Features' in line or 'Features' in stripped:
                        continue
                    feature = stripped
                    if '#' in feature:
                        parts = feature.split('#', 1)
                        feature = parts[0].strip()
                        comment = parts[1].strip()
                    else:
                        comment = None
                    # Only add if it looks like a feature name (lowercase, underscores, hyphens)
                    # Feature names are typically lowercase with underscores/hyphens
                    if feature and feature.replace('_', '').replace('-', '').islower() and len(feature) > 2:
                        disabled_features[feature] = comment
    
    return disabled_features

def generate_features_txt(features_dir, output_path, preserve_disabled=True):
    """Generate FEATURES.txt from feature directories"""
    # Find all feature directories
    found_features = find_feature_directories(features_dir)
    
    # Read existing FEATURES.txt to preserve disabled features
    disabled_features = {}
    if preserve_disabled and os.path.exists(output_path):
        disabled_features = read_existing_features_txt(output_path)
    # Merge with default disabled (but don't override manually disabled)
    for feature, comment in DEFAULT_DISABLED.items():
        if feature not in disabled_features:
            disabled_features[feature] = comment
    
    # Build feature list organized by categories
    categorized = {}
    uncategorized = []
    
    # First, organize found features by category
    for feature in found_features:
        placed = False
        for category, features in CATEGORIES.items():
            if feature in features:
                if category not in categorized:
                    categorized[category] = []
                categorized[category].append(feature)
                placed = True
                break
        if not placed:
            uncategorized.append(feature)
    
    # Add any features from categories that weren't found (for structure)
    for category, features in CATEGORIES.items():
        if category not in categorized:
            categorized[category] = []
        for feature in features:
            if feature in found_features and feature not in categorized[category]:
                categorized[category].append(feature)
    
    # Generate output
    lines = [
        "# SoF Buddy Features Configuration",
        "# Lines starting with # are comments",
        "# Uncommented lines = enabled features  ",
        "# Commented lines (with //) = disabled features",
        "",
    ]
    
    # Write categorized features
    for category, features in CATEGORIES.items():
        if not features:
            continue
        
        lines.append(f"# {category}")
        for feature in features:
            if feature in found_features:
                if feature in disabled_features:
                    comment = disabled_features[feature]
                    if comment:
                        lines.append(f"# {feature}  # {comment}")
                    else:
                        lines.append(f"# {feature}")
                else:
                    lines.append(feature)
        lines.append("")
    
    # Add uncategorized features at the end
    if uncategorized:
        lines.append("# Uncategorized Features")
        for feature in sorted(uncategorized):
            if feature in disabled_features:
                comment = disabled_features[feature]
                if comment:
                    lines.append(f"# {feature}  # {comment}")
                else:
                    lines.append(f"# {feature}")
            else:
                lines.append(feature)
        lines.append("")
    
    # Write to file
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    
    print(f"Generated {output_path} with {len(found_features)} features")
    enabled = sum(1 for line in lines if line and not line.startswith('#') and not line.startswith('//'))
    print(f"  - {enabled} enabled")
    print(f"  - {len(disabled_features)} disabled/commented")

def main():
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    features_dir = project_root / "src" / "features"
    output_path = project_root / "features" / "FEATURES.txt"
    
    if not features_dir.exists():
        print(f"Error: Features directory not found: {features_dir}", file=sys.stderr)
        sys.exit(1)
    
    # Create features directory if it doesn't exist
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    generate_features_txt(str(features_dir), str(output_path))
    return 0

if __name__ == "__main__":
    sys.exit(main())

