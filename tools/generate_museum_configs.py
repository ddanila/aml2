#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import re
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path


GAMES_PREFIX = "C:\\GAMES\\"

IGNORE_ROOTS = {
    "AML.EXE",
    "AML.OLD",
    "AML.OL1",
    "AML.OL2",
    "AML.OL3",
    "AUTOEXEC.BAT",
    "GAMES.BAT",
    "GAMES.OLD",
    "GAMES2.BAT",
    "GAMES2.OLD",
    "GAMES2_B.BAT",
    "GAMES3.BAT",
    "GAMES_14.BAT",
    "GAMES_BA.BAT",
    "GO.BAT",
    "LAUNCHER.CFG",
    "LAUNCHER.OLD",
    "OLDGAMES.BAT",
    "SOUND.BAT",
    "T.BAT",
    "TETRIS.RES",
    "VBREAK.COM",
}

IGNORE_FILE_BASES = {
    "AUTOEXEC",
    "CRACK",
    "EGRAPHIC",
    "INSTALL",
    "MGRAPHIC",
    "MISC",
    "SETUP",
    "SOUND",
    "TGRAPHIC",
}

TITLE_ALIASES = {
    "ALLEYCAT": "Alley Cat",
    "ALONE": "Alone in the Dark",
    "ALONE1": "Alone in the Dark",
    "ANIMAL": "Animal Quest",
    "AV": "Arcade Volleyball",
    "BABYOLD": "BabyType",
    "BABYTYPE": "BabyType",
    "BLOCK": "Blockout",
    "CARS": "Crazy Cars 3",
    "CD-MAN": "CD-Man",
    "CDMAN": "CD-Man",
    "CIV": "Sid Meier's Civilization",
    "CKEEN4": "Commander Keen 4: Secret of the Oracle",
    "DAVE": "Dangerous Dave",
    "DAVE2": "Dangerous Dave in the Haunted Mansion",
    "DD": "Dangerous Dave",
    "DUNE": "Dune",
    "DUNE2": "Dune 2 - The Building of a Dynasty",
    "EAGLESNE": "Into the Eagle's Nest",
    "FLASHBCK": "Flashback",
    "KYRAND2": "The Legend of Kyrandia - Hand of Fate",
    "LEMMI": "Lemmings",
    "LEMMINGS": "Lemmings",
    "MALCOLM": "The Legend of Kyrandia - Malcolm's Revenge",
    "MARI": "Mario Brothers VGA",
    "MARIO": "Mario Brothers VGA",
    "MARIO2": "Mario Brothers VGA",
    "MONKEY": "The Secret of Monkey Island",
    "MSPACPC": "Ms. Pac-PC",
    "ORION": "Master of Orion",
    "PARROT": "Parrot",
    "POLE": "Pole Position",
    "PRINCE": "Prince of Persia",
    "SKYROADS": "SkyRoads",
    "SPACEINV": "Space Invaders",
    "SR2": "Stunts",
    "SUMMER": "The Games - Summer Challenge",
    "SUPAPLEX": "Supaplex",
    "TANKWARS": "Tank Wars",
    "TSTDRIVE": "The Duel: Test Drive II",
    "TYCOON": "Railroad Tycoon Deluxe",
    "VIKNGS": "The Lost Vikings",
    "VOLLEY": "Arcade Volleyball",
    "WINTER": "The Games - Winter Challenge",
    "XONIX": "Xonix",
}

COMMAND_OVERRIDES = {
    "ALONE": "ALONE.COM",
    "ALONE1": "ALONE.COM",
    "CARS": "CARS.COM",
    "CIV": "CIV.EXE",
    "DD": "DAV1\\DAVE.EXE",
    "DINO": "DP.EXE",
    "DUNE": "DUNEPRG.EXE",
    "KYRAND2": "HOF.EXE",
    "LEMMI": "LEMVGA.COM",
    "LEMMINGS": "VGALEMMI.EXE",
    "MALCOLM": "MAIN.EXE",
    "MARIO2": "START.BAT",
    "MM2": "MM2.EXE",
    "ORION": "ORION.EXE",
    "PARROT": "PARROT\\PARROT.EXE",
    "SPACEINV": "INVADERS.COM",
    "SR2": "SR2.EXE",
    "SUPAPLEX": "SUPAPLEX.EXE",
    "TSTDRIVE": "TD2\\TD2EGA.EXE",
    "TYCOON": "TYCOON.EXE",
    "UFO": "UFOINV\\UFO.EXE",
    "VIKNGS": "VIKINGS.EXE",
    "WINTER": "WINTER.EXE",
}


@dataclass
class Metadata:
    title: str
    year: str = ""


MANUAL_METADATA = {
    "ALONE": Metadata(title="Alone in the Dark", year="1992"),
    "ALONE1": Metadata(title="Alone in the Dark", year="1992"),
    "ANIMAL": Metadata(title="Animal Quest", year="1996"),
    "AV": Metadata(title="Arcade Volleyball", year="1987"),
    "BABYOLD": Metadata(title="BabyType", year="1993"),
    "BABYTYPE": Metadata(title="BabyType", year="1993"),
    "BLOCK": Metadata(title="Blockout", year="1989"),
    "CARS": Metadata(title="Crazy Cars 3", year="1992"),
    "CD-MAN": Metadata(title="CD-Man", year="1989"),
    "CDMAN": Metadata(title="CD-Man", year="1989"),
    "CIV": Metadata(title="Sid Meier's Civilization", year="1991"),
    "CKEEN4": Metadata(title="Commander Keen 4: Secret of the Oracle", year="1991"),
    "DAVE": Metadata(title="Dangerous Dave", year="1988"),
    "DAVE2": Metadata(title="Dangerous Dave in the Haunted Mansion", year="1991"),
    "DD": Metadata(title="Dangerous Dave", year="1988"),
    "EAGLESNE": Metadata(title="Into the Eagle's Nest", year="1987"),
    "FLASHBCK": Metadata(title="Flashback", year="1993"),
    "KYRAND2": Metadata(title="The Legend of Kyrandia - Hand of Fate", year="1993"),
    "LEMMI": Metadata(title="Lemmings", year="1991"),
    "LEMMINGS": Metadata(title="Lemmings", year="1991"),
    "MALCOLM": Metadata(title="The Legend of Kyrandia - Malcolm's Revenge", year="1994"),
    "MONKEY": Metadata(title="The Secret of Monkey Island", year="1990"),
    "PARROT": Metadata(title="Parrot"),
    "SPACEINV": Metadata(title="Space Invaders"),
    "SUMMER": Metadata(title="The Games - Summer Challenge", year="1992"),
    "TANKWARS": Metadata(title="Tank Wars", year="1986"),
    "TSTDRIVE": Metadata(title="The Duel: Test Drive II", year="1989"),
    "TYCOON": Metadata(title="Railroad Tycoon Deluxe", year="1993"),
}


def norm(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "", text.lower())


def title_case_stem(stem: str) -> str:
    text = stem.replace("-", " ").replace("_", " ")
    text = re.sub(r"(?<=[a-z])(?=[A-Z0-9])", " ", text)
    text = re.sub(r"(?<=[0-9])(?=[A-Za-z])", " ", text)
    text = re.sub(r"(?<=[A-Za-z])(?=[0-9])", " ", text)
    return " ".join(part.capitalize() if not part.isupper() else part for part in text.split())


def load_metadata(csv_path: Path) -> dict[str, Metadata]:
    index: dict[str, Metadata] = {}
    with csv_path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            title = row["name"].strip()
            year = row["year"].strip()
            index[norm(title)] = Metadata(title=title, year=year)
    return index


def enrich_title(root: str, metadata: dict[str, Metadata]) -> tuple[str, bool]:
    manual = MANUAL_METADATA.get(root.upper())
    if manual is not None:
        if manual.year:
            return f"{manual.title} ({manual.year})", True
        return manual.title, False

    alias = TITLE_ALIASES.get(root.upper(), title_case_stem(root))
    entry = metadata.get(norm(alias))
    if entry is None:
        return alias, False
    return f"{entry.title} ({entry.year})", True


def parse_listing(path: Path) -> dict[str, list[str]]:
    text = path.read_text(encoding="cp437", errors="replace")
    groups: dict[str, list[str]] = defaultdict(list)

    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        upper = line.upper()
        if not upper.startswith(GAMES_PREFIX):
            continue

        rest = line[len(GAMES_PREFIX):]
        root = rest.split("\\", 1)[0]
        groups[root.upper()].append(line)

    return dict(sorted(groups.items()))


def candidate_paths(root: str, lines: list[str]) -> list[str]:
    candidates: list[str] = []
    root_prefix = f"{GAMES_PREFIX}{root}\\"

    for line in lines:
        upper = line.upper()
        if upper == f"{GAMES_PREFIX}{root}":
            continue
        if not upper.startswith(root_prefix):
            continue

        rel = line[len(root_prefix):]
        suffix = Path(rel).suffix.lower()
        if suffix not in {".exe", ".com", ".bat"}:
            continue

        base = Path(rel).stem.upper()
        if base in IGNORE_FILE_BASES:
            continue

        candidates.append(rel)

    if root not in IGNORE_ROOTS and Path(root).suffix.lower() in {".exe", ".com", ".bat"}:
        base = Path(root).stem.upper()
        if base not in IGNORE_FILE_BASES:
            candidates.append(root)

    return sorted(set(candidates))


def score_command(root: str, candidate: str) -> int:
    override = COMMAND_OVERRIDES.get(root.upper())
    if override and candidate.upper() == override.upper():
        return 1000

    root_norm = norm(root)
    stem = Path(candidate).stem
    stem_norm = norm(stem)
    ext = Path(candidate).suffix.lower()
    depth = candidate.count("\\")
    score = 0

    if stem_norm == root_norm:
        score += 100
    elif stem_norm.startswith(root_norm) or root_norm.startswith(stem_norm):
        score += 70

    if ext == ".exe":
        score += 20
    elif ext == ".com":
        score += 10

    if depth > 0:
        score -= depth * 2

    upper_stem = stem.upper()
    upper_candidate = candidate.upper()
    if "SETUP" in upper_stem or "INSTALL" in upper_stem:
        score -= 80
    if "DOS4GW" in upper_stem or "CFG_" in upper_stem:
        score -= 80
    if upper_stem in {"MAIN", "RUNMAIN", "RUNGAME"}:
        score += 25
    if upper_stem in {"LEMVGA", "VGALEMMI", "PARROT", "CC3"}:
        score += 25
    if upper_candidate.endswith("\\UFO.EXE"):
        score += 25
    if upper_candidate.endswith("\\DAVE.EXE"):
        score += 20
    if upper_candidate.endswith("\\TD2EGA.EXE"):
        score += 20
    if upper_candidate.endswith("\\PARROT.EXE"):
        score += 20

    return score


def choose_command(root: str, candidates: list[str], fallback: str | None = None) -> str | None:
    if candidates:
        scored = sorted(
            ((score_command(root, candidate), candidate) for candidate in candidates),
            key=lambda item: (-item[0], item[1]),
        )
        return scored[0][1]

    return fallback


def write_config(out_path: Path, listing_name: str, entries: list[tuple[str, str, str]]) -> None:
    with out_path.open("w", encoding="ascii", newline="\n") as handle:
        handle.write(f"# Generated from {listing_name}\n")
        handle.write("# Format: Name|Command|Working Directory\n\n")
        for title, command, workdir in entries:
            safe_title = title.replace("|", "-")
            handle.write(f"{safe_title}|{command}|{workdir}\n")


def split_command_path(root: str, command: str) -> tuple[str, str]:
    if "\\" not in command:
        if Path(root).suffix.lower() in {".exe", ".com", ".bat"}:
            return Path(command).name, "C:\\GAMES"
        return Path(command).name, f"C:\\GAMES\\{root}"

    parent = Path(command).parent.as_posix().replace("/", "\\")
    if parent in {"", "."}:
        return Path(command).name, f"C:\\GAMES\\{root}"
    return Path(command).name, f"C:\\GAMES\\{root}\\{parent}"


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate aml2 configs from DOS file listings.")
    parser.add_argument(
        "--listings",
        default="temp",
        help="Directory with listing TXT files. Default: temp",
    )
    parser.add_argument(
        "--games-csv",
        default="/home/ddanila/fun/games_list/games.csv",
        help="CSV with game metadata for title/year enrichment.",
    )
    parser.add_argument(
        "--out",
        default="generated/museum-configs",
        help="Output directory for generated cfg files.",
    )
    args = parser.parse_args()

    listings_dir = Path(args.listings)
    out_dir = Path(args.out)
    metadata = load_metadata(Path(args.games_csv))

    out_dir.mkdir(parents=True, exist_ok=True)
    all_groups = {listing: parse_listing(listing) for listing in sorted(listings_dir.glob("*.TXT"))}
    learned_commands: dict[str, str] = {}

    for groups in all_groups.values():
        for root, lines in groups.items():
            if root in IGNORE_ROOTS:
                continue
            candidates = candidate_paths(root, lines)
            command = choose_command(root, candidates)
            if command is not None:
                known = learned_commands.get(root)
                if known is None or score_command(root, command) > score_command(root, known):
                    learned_commands[root] = command

    for listing in sorted(listings_dir.glob("*.TXT")):
        groups = all_groups[listing]
        entries: list[tuple[str, str, str]] = []
        enriched = 0
        skipped: list[str] = []

        for root, lines in groups.items():
            if root in IGNORE_ROOTS:
                continue

            candidates = candidate_paths(root, lines)
            command = choose_command(root, candidates, learned_commands.get(root))
            if command is None:
                skipped.append(root)
                continue

            title, did_enrich = enrich_title(root, metadata)
            if did_enrich:
                enriched += 1

            command_name, workdir = split_command_path(root, command)
            entries.append((title, command_name, workdir))

        entries.sort(key=lambda item: item[0].lower())
        out_path = out_dir / f"{listing.stem}.cfg"
        write_config(out_path, listing.name, entries)

        print(f"{listing.name}: wrote {len(entries)} entries to {out_path}")
        print(f"  title/year enrichment: {enriched}")
        if skipped:
            print(f"  skipped without clear launcher ({len(skipped)}): {', '.join(skipped[:12])}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
