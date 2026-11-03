# admin paneli — giris denemesi sinirlayici (bellekte, tek worker icin)
import time
from collections import defaultdict

MAX_FAILS = 5          # 15 dakikada en fazla
WINDOW = 15 * 60
LOCK = 15 * 60

_fails: dict[str, list[float]] = defaultdict(list)
_locked: dict[str, float] = {}


def _now() -> float:
    return time.time()


def is_locked(ip: str) -> tuple[bool, int]:
    """(kilitli_mi, kalan_saniye)"""
    until = _locked.get(ip)
    if until and until > _now():
        return True, int(until - _now())
    if until:
        _locked.pop(ip, None)
        _fails.pop(ip, None)
    return False, 0


def record_fail(ip: str) -> None:
    t = _now()
    arr = [x for x in _fails[ip] if t - x < WINDOW]
    arr.append(t)
    _fails[ip] = arr
    if len(arr) >= MAX_FAILS:
        _locked[ip] = t + LOCK
        _fails[ip] = []


def clear(ip: str) -> None:
    _fails.pop(ip, None)
    _locked.pop(ip, None)


def prune() -> None:
    t = _now()
    for ip in list(_fails):
        _fails[ip] = [x for x in _fails[ip] if t - x < WINDOW]
        if not _fails[ip]:
            _fails.pop(ip, None)
    for ip, until in list(_locked.items()):
        if until <= t:
            _locked.pop(ip, None)
