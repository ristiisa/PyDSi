import pydsi.__main__ as cli
from conftest import RESULTS


def test_cli_runs_rom(test_rom, tmp_path, capsys):
  png = tmp_path / "out.png"
  assert cli.main([test_rom, "--dsi", "--frames", "60", "--press", "a@10:5", "--png", str(png), "--shot-every", "30", "--read", hex(RESULTS + 4)]) == 0
  out = capsys.readouterr().out
  assert "console: DSi, DSi mode active: True" in out
  assert "0x8307f100" in out
  assert png.read_bytes().startswith(b"\x89PNG")
  assert (tmp_path / "out_00030.png").exists() and (tmp_path / "out_00060.png").exists()


def test_cli_ds_mode(test_rom, capsys):
  assert cli.main([test_rom, "--ds", "--frames", "10", "--read", hex(RESULTS + 4)]) == 0
  out = capsys.readouterr().out
  assert "console: DS," in out
  assert "= 0 (0x0)" in out
