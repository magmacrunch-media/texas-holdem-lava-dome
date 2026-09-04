"""What the package says about itself.

Its own file, not a section of ``test_app.py``, because that suite opens with
``importorskip("textual")`` and this check needs nothing installed — the whole
point of a hand-kept literal is that it is readable from a bare checkout. CI's
full-suite job runs it on 3.12, so it gates every push.
"""

import pathlib
import sys

import pytest


def _pyproject() -> dict:
    """The packaging metadata, read from source.

    ``tomllib`` is 3.11; this package supports 3.10. Skipping there rather
    than taking a ``tomli`` dependency for one test is the cheaper trade —
    CI runs 3.12, so the check still gates every push.
    """
    if sys.version_info < (3, 11):
        pytest.skip("tomllib is 3.11+")
    import tomllib

    root = pathlib.Path(__file__).resolve().parent.parent
    return tomllib.loads((root / "pyproject.toml").read_text(encoding="utf-8"))


def test_the_version_is_the_one_the_package_declares():
    """``__version__`` and pyproject must agree.

    They did not, for four releases: the literal sat at 0.1.0 while 0.5.0 was
    being tagged. The release workflow compares the git tag against pyproject
    and never looks at the module, so nothing caught it — and nothing reads
    ``lavadome.__version__``, which is why it could rot unnoticed. This is the
    check; ``magmacrunch`` grew the same one for the same reason.
    """
    import lavadome

    assert lavadome.__version__ == _pyproject()["project"]["version"]
