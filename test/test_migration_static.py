from pathlib import Path
ROOT = Path(__file__).parents[1]

def test_platformio_shape():
    assert (ROOT / "platformio.ini").exists()
    assert (ROOT / "partitions/huge_app.csv").exists()

def test_no_untracked_build_directory():
    assert not (ROOT / ".pio").exists()
