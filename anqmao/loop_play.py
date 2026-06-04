"""Loop play all audio files in current directory."""

import glob
import os
import sys
import time

import pygame


def find_audio_files(extensions=(".wav", ".mp3", ".ogg", ".flac", ".aiff")):
    files = []
    for ext in extensions:
        files.extend(glob.glob(f"*{ext}"))
        files.extend(glob.glob(f"*{ext.upper()}"))
    # Deduplicate while preserving order
    seen = set()
    unique = []
    for f in files:
        if f not in seen:
            seen.add(f)
            unique.append(f)
    return sorted(unique)


def main():
    files = find_audio_files()
    if not files:
        print("No audio files found in current directory.")
        sys.exit(1)

    print(f"Found {len(files)} audio file(s):")
    for f in files:
        print(f"  {f}")
    print("Press Ctrl+C to stop.\n")

    pygame.mixer.init()
    try:
        while True:
            for filepath in files:
                print(f"Playing: {filepath}")
                sound = pygame.mixer.Sound(filepath)
                channel = sound.play()
                if channel is None:
                    print(f"  Failed to play {filepath}, skipping...")
                    continue

                # Wait for playback to finish
                while channel.get_busy():
                    time.sleep(0.1)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        pygame.mixer.quit()


if __name__ == "__main__":
    main()
