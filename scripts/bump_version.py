import sys
import os

def bump_version(current, part):
    parts = current.split('.')
    if len(parts) != 3:
        # Handle case where current might already have a suffix or is malformed
        parts = parts[0].split('-')[0].split('.')[:3]
        while len(parts) < 3:
            parts.append('0')

    major, minor, patch = map(int, parts)

    if part == 'major':
        major += 1
        minor = 0
        patch = 0
    elif part == 'minor':
        minor += 1
        patch = 0
    elif part == 'patch':
        patch += 1
    
    return f"{major}.{minor}.{patch}"

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python bump_version.py <current_version> <major|minor|patch>")
        sys.exit(1)

    current_ver = sys.argv[1].strip()
    bump_type = sys.argv[2].strip().lower()
    
    new_ver = bump_version(current_ver, bump_type)
    print(new_ver)
