"""
Copyright (c) Contributors to the Open 3D Engine Project.
For complete copyright and license terms please see the LICENSE at the root of this distribution.

SPDX-License-Identifier: Apache-2.0 OR MIT
"""

# Removes duplicate lines from a text file while preserving the order of first occurrences.

import sys

def remove_duplicates(input_path, output_path):
    seen = set()
    unique_lines = []

    with open(input_path, "r", encoding="utf-8") as infile:
        for line in infile:
            stripped = line.rstrip("\n\r")
            if stripped not in seen:
                seen.add(stripped)
                unique_lines.append(line)

    with open(output_path, "w", encoding="utf-8") as outfile:
        outfile.writelines(unique_lines)

    print(f"Removed duplicates from '{input_path}' → saved to '{output_path}'")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python remove_duplicates.py <input_file> <output_file>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    remove_duplicates(input_file, output_file)
