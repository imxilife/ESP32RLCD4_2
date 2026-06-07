from configparser import ConfigParser
from pathlib import Path
from urllib.parse import urlsplit

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
    def normalize_value(key, value):
        compact = "".join(value.split())
        if key != "api_host":
            return compact

        if "://" in compact:
            parsed = urlsplit(compact)
            compact = parsed.netloc or parsed.path
        return compact.split("/", 1)[0]

    defines = []
    loaded = {}
    for macro, key in mapping.items():
        value = normalize_value(key, parser.get("qweather", key, fallback=""))
        if value:
            defines.append((macro, env.StringifyMacro(value)))
            loaded[key] = value

    if defines:
        env.Append(CPPDEFINES=defines)
        print(
            "[secrets] loaded qweather settings "
            f"host={loaded.get('api_host', '<empty>')} "
            f"location={loaded.get('location', '<empty>')} "
            f"api_key_len={len(loaded.get('api_key', ''))}"
        )


_append_qweather_defines()
