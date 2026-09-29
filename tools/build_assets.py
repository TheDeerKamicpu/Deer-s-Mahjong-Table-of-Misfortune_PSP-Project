from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import struct
import subprocess
import shutil
import sys


ROOT = Path(__file__).resolve().parents[1]

SOURCE_STATIC = ROOT / "static"

SOURCE_TILES = (
    SOURCE_STATIC
    / "tiles"
    / "doman"
)

SOURCE_AUDIO = (
    SOURCE_STATIC
    / "audio"
    / "background.mp3"
)


OUT = ROOT / "assets"

OUT_TILES = (
    OUT
    / "tiles"
    / "doman"
)

OUT_UI = (
    OUT
    / "ui"
)

OUT_AUDIO = (
    OUT
    / "audio"
)


REQUIRED_TILES = (
    [f"M{i}" for i in range(1, 10)]
    + [f"D{i}" for i in range(1, 10)]
    + [f"B{i}" for i in range(1, 10)]
    + [
        "E",
        "S",
        "W",
        "N",
        "RD",
        "GD",
        "WD",
        "J",
    ]
)


def save_rgba(
    image: Image.Image,
    destination: Path
):
    image = image.convert("RGBA")

    w, h = image.size

    raw = image.tobytes("raw", "RGBA")

    destination.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    with destination.open("wb") as f:
        f.write(b"RGBA")

        f.write(
            struct.pack(
                "<HH",
                w,
                h,
            )
        )

        f.write(raw)


def fit_tile(
    image: Image.Image,
    width=48,
    height=64,
):
    image = image.convert("RGBA")

    image.thumbnail(
        (width, height),
        Image.Resampling.LANCZOS,
    )

    canvas = Image.new(
        "RGBA",
        (width, height),
        (0, 0, 0, 0),
    )

    x = (
        width
        - image.width
    ) // 2

    y = (
        height
        - image.height
    ) // 2

    canvas.alpha_composite(
        image,
        (x, y),
    )

    return canvas


def find_tile_source(code):
    exact = SOURCE_TILES / f"{code}.png"

    if exact.exists():
        return exact

    for candidate in SOURCE_TILES.rglob("*.png"):
        if candidate.stem == code:
            return candidate

    return None


def convert_tiles():
    missing = []

    for code in REQUIRED_TILES:
        source = find_tile_source(code)

        if source is None:
            missing.append(code)
            continue

        image = Image.open(source)

        image = fit_tile(
            image,
            48,
            64,
        )

        save_rgba(
            image,
            OUT_TILES
            / f"{code}.rgba",
        )

        print(
            f"TILE {code}"
        )

    if missing:
        print()
        print(
            "MISSING DOMAN FILES:"
        )

        print(
            " ".join(missing)
        )

        sys.exit(1)


def find_font():
    candidates = [
        Path(
            "/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf"
        ),
        Path(
            "/usr/share/fonts/truetype/liberation2/LiberationSerif-Bold.ttf"
        ),
    ]

    for path in candidates:
        if path.exists():
            return path

    return None


def make_title():
    width = 450
    height = 46

    image = Image.new(
        "RGBA",
        (width, height),
        (0, 0, 0, 0),
    )

    draw = ImageDraw.Draw(image)

    text = (
        "The Deer's Mahjong "
        "Table of Misfortune"
    )

    font_path = find_font()

    if font_path:
        font = ImageFont.truetype(
            str(font_path),
            24,
        )
    else:
        font = ImageFont.load_default()

    bbox = draw.textbbox(
        (0, 0),
        text,
        font=font,
        stroke_width=1,
    )

    text_width = (
        bbox[2]
        - bbox[0]
    )

    text_height = (
        bbox[3]
        - bbox[1]
    )

    x = (
        width
        - text_width
    ) // 2

    y = (
        height
        - text_height
    ) // 2 - 2

    draw.text(
        (x + 2, y + 2),
        text,
        font=font,
        fill=(0, 0, 0, 170),
        stroke_width=1,
        stroke_fill=(0, 0, 0, 180),
    )

    draw.text(
        (x, y),
        text,
        font=font,
        fill=(237, 222, 183, 255),
        stroke_width=1,
        stroke_fill=(112, 84, 36, 255),
    )

    OUT_UI.mkdir(
        parents=True,
        exist_ok=True,
    )

    image.save(
        OUT_UI / "title.png"
    )

    save_rgba(
        image,
        OUT_UI / "title.rgba",
    )

    print(
        "TITLE GENERATED"
    )


def convert_audio():
    OUT_AUDIO.mkdir(
        parents=True,
        exist_ok=True,
    )

    wav = (
        OUT_AUDIO
        / "background.wav"
    )

    if not SOURCE_AUDIO.exists():
        print(
            f"Missing music: "
            f"{SOURCE_AUDIO}"
        )

        sys.exit(1)

    ffmpeg = shutil.which(
        "ffmpeg"
    )

    if not ffmpeg:
        print(
            "ffmpeg is not installed."
        )

        sys.exit(1)

    subprocess.run(
        [
            ffmpeg,
            "-y",
            "-i",
            str(SOURCE_AUDIO),
            "-vn",
            "-ac",
            "2",
            "-ar",
            "44100",
            "-c:a",
            "pcm_s16le",
            str(wav),
        ],
        check=True,
    )

    print(
        "AUDIO GENERATED"
    )


def main():
    if not SOURCE_TILES.exists():
        print(
            f"Missing folder: "
            f"{SOURCE_TILES}"
        )

        sys.exit(1)

    OUT_TILES.mkdir(
        parents=True,
        exist_ok=True,
    )

    convert_tiles()
    make_title()
    convert_audio()

    print()
    print(
        "ASSETS COMPLETE"
    )


if __name__ == "__main__":
    main()