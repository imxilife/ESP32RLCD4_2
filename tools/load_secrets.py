from configparser import ConfigParser
from pathlib import Path

Import("env")


def _append_qweather_defines():
    """Load optional local secrets.ini and expose QWeather settings as C macros."""
    project_dir = Path(env.subst("$PROJECT_DIR"))
    secrets_path = project_dir / "secrets.ini"
    if not secrets_path.exists():
        return

    parser = ConfigParser()
    parser.read(secrets_path, encoding="utf-8")
    if not parser.has_section("qweather"):
        print("[secrets] skip: missing [qweather] section")
        return

    mapping = {
        "QWEATHER_API_HOST": "api_host",
        "QWEATHER_API_KEY": "api_key",
        "QWEATHER_LOCATION": "location",
    }
    defines = []
    for macro, key in mapping.items():
        value = parser.get("qweather", key, fallback="").strip()
        if value:
            defines.append((macro, env.StringifyMacro(value)))

    if defines:
        env.Append(CPPDEFINES=defines)
        print("[secrets] loaded qweather settings")


_append_qweather_defines()
